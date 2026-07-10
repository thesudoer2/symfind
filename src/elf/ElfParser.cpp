#include <symfind/elf/ElfParser.h>

#include <cstdlib>
#include <cstring>

#include <cxxabi.h>
#include <gelf.h>
#include <libelf.h>
#include <unistd.h>

#include <iostream>
#include <memory>
#include <string>

#include <symfind/utils/Expected.h>
#include <symfind/filesystem/FileWrapper.h>
#include <symfind/core/Symbol.h>


// NOLINTBEGIN(readability-identifier-length,performance-unnecessary-value-param)

namespace SymFind
{

namespace
{

const std::uint32_t elf_current_version = elf_version(EV_CURRENT);

} // namespace

struct ElfCloser
{
public:
    void operator()(Elf *elf) const noexcept
    {
        elf_end(elf);
    }
};

using ElfPtr = std::shared_ptr<Elf>;

class Demangler final
{
public:
    Demangler() noexcept : _buffer_size(256), _buffer(static_cast<char *>(std::malloc(_buffer_size)))
    {
    }

    std::string demangle(const std::string& sym_name)
    {
        int status = 0;

        char* result = abi::__cxa_demangle(
            sym_name.c_str(),
            _buffer,
            &_buffer_size,
            &status);

        if (status != 0 || result == nullptr)
        {
            return std::string(sym_name);
        }

        _buffer = result;  // may have changed due to realloc
        return result;
    }

    ~Demangler() noexcept
    {
        std::free(_buffer);
    }

    Demangler(const Demangler&) noexcept = delete;
    Demangler(Demangler&&) noexcept = delete;

    Demangler& operator=(const Demangler&) noexcept = delete;
    Demangler& operator=(Demangler&&) noexcept = delete;

private:
    std::size_t _buffer_size = 0;
    char* _buffer = nullptr;
};

inline ElfPtr make_elf(Elf *raw)
{
    return ElfPtr(raw, ElfCloser{});
}

bool is_symbol_defined(const GElf_Sym &sym) noexcept
{
    return sym.st_shndx != SHN_UNDEF;
}

SymbolType get_symbol_type(std::uint8_t sym_info) noexcept
{
    std::uint8_t sym_type = ELF64_ST_TYPE(sym_info);
    switch (sym_type)
    {
    case STT_NOTYPE:
        return SymbolType::NODEF;
    case STT_FUNC:
        return SymbolType::FUNC;
    case STT_SECTION:
        return SymbolType::SECSYM;
    case STT_OBJECT:
        return SymbolType::OBJSYM;
    default:
        return SymbolType::UNKNOWN;
    }
}

SymbolBind get_symbol_bind(std::uint8_t sym_info) noexcept
{
    unsigned char sym_bind = ELF64_ST_BIND(sym_info);
    switch (sym_bind)
    {
    case STB_LOCAL:
        return SymbolBind::LOCAL;
    case STB_WEAK:
        return SymbolBind::WEAK;
    case STB_GLOBAL:
        return SymbolBind::GLOBAL;
    default:
        return SymbolBind::UNKNOWN;
    }
}

void parse_symtable(ElfPtr elf,
                    Elf_Scn *scn,
                    GElf_Shdr &shdr,
                    const TryStoreSymbolCallback &try_store_symbol_callback,
                    SymbolSourceSection sec,
                    const SymbolShouldBeIgnoredCallback &ignore_symbol_callback) noexcept
{
    static thread_local Demangler demangler;

    Elf_Data *data = elf_getdata(scn, nullptr);

    std::size_t count = shdr.sh_size / shdr.sh_entsize;

    Elf_Scn *str_scn = elf_getscn(elf.get(), shdr.sh_link);
    Elf_Data *str_data = elf_getdata(str_scn, nullptr);

    for (std::size_t i = 0; i < count; ++i)
    {
        GElf_Sym sym;

        // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions,bugprone-narrowing-conversions)
        gelf_getsym(data, i, &sym);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::string sym_name = reinterpret_cast<char *>(str_data->d_buf) + sym.st_name;

        if (sym_name.size() == 0)
        {
            continue;
        }

        // TODO: Make demangle decision based on conditions like arguments.
        sym_name = demangler.demangle(sym_name);

        // NOLINTNEXTLINE(bugprone-unhandled-exception-at-new)
        SymbolMetaData sym_metadata{
            .source_section = sec,
            .type = get_symbol_type(sym.st_info),
            .bind = get_symbol_bind(sym.st_info),
            .visibility = SymbolVisibility::UNKNOWN, // TODO: Set symbol visitility.
            .is_defined = is_symbol_defined(sym),
            .offset = 0, // TODO: Get symbol offset.
        };

        SymbolEntry sym_ent{.name = std::move(sym_name), .metadata = sym_metadata};
        if (!ignore_symbol_callback(sym_ent))
        {
            try_store_symbol_callback(std::move(sym_ent));
        }
    }
}

void parse_elf(ElfPtr elf,
               const TryStoreSymbolCallback &try_store_symbol_callback,
               const SymbolShouldBeIgnoredCallback &ignore_symbol_callback) noexcept
{
    size_t shstrndx{0};

    elf_getshdrstrndx(elf.get(), &shstrndx);

    Elf_Scn *scn = nullptr;

    while ((scn = elf_nextscn(elf.get(), scn)) != nullptr)
    {
        GElf_Shdr shdr;
        gelf_getshdr(scn, &shdr);

        if (shdr.sh_type == SHT_SYMTAB)
        {
            parse_symtable(elf, scn, shdr, try_store_symbol_callback, SymbolSourceSection::SYMTAB, ignore_symbol_callback);
        }
        else if (shdr.sh_type == SHT_DYNSYM)
        {
            parse_symtable(elf, scn, shdr, try_store_symbol_callback, SymbolSourceSection::DYNSYM, ignore_symbol_callback);
        }
    }
}

void parse_archive(int fd,
                   ElfPtr archive,
                   const TryStoreSymbolCallback &try_store_symbol_callback,
                   const SymbolShouldBeIgnoredCallback &ignore_symbol_callback) noexcept
{
    Elf_Arhdr *arh = nullptr;
    for (ElfPtr member = make_elf(elf_begin(fd, ELF_C_READ, archive.get())); member != nullptr;
         member = make_elf(elf_begin(fd, ELF_C_READ, archive.get())))
    {
        if (arh = elf_getarhdr(member.get()); arh == nullptr)
        {
            std::cerr << "elf_getarhdr() failed!\n";
            exit(EXIT_FAILURE);
        }

        if (elf_kind(member.get()) == ELF_K_ELF)
        {
            parse_elf(member, try_store_symbol_callback, ignore_symbol_callback);
        }

        (void)elf_next(member.get());
    }
}

bool parse_symtables(const std::string &file,
                     const TryStoreSymbolCallback &try_store_symbol_callback,
                     const SymbolShouldBeIgnoredCallback &ignore_symbol_callback,
                     std::string *err_msg) noexcept
{
    if (elf_current_version == EV_NONE) [[unlikely]]
    {
        std::cerr << "libelf init failed\n";
        exit(EXIT_FAILURE);
    }

    FileWrapper file_to_parse(file, "r");
    if (!file_to_parse) [[unlikely]]
    {
        if (err_msg != nullptr)
        {
            *err_msg = std::strerror(file_to_parse.get_errno());
        }
        return false;
    }

    expected<std::int32_t, FileWrapper::Errno_t> fd_res = file_to_parse.get_fd();
    if (!fd_res.has_value()) [[unlikely]]
    {
        if (err_msg != nullptr)
        {
            *err_msg = std::strerror(fd_res.error());
        }
        return false;
    }

    int fd = fd_res.value();

    ElfPtr elf = make_elf(elf_begin(fd, ELF_C_READ, nullptr));
    if (!elf) [[unlikely]]
    {
        if (err_msg != nullptr)
        {
            *err_msg = "elf_begin failed";
        }
        return false;
    }

    Elf_Kind kind = elf_kind(elf.get());

    if (kind == ELF_K_ELF)
    {
        parse_elf(elf, try_store_symbol_callback, ignore_symbol_callback);
    }
    else if (kind == ELF_K_AR)
    {
        parse_archive(fd, elf, try_store_symbol_callback, ignore_symbol_callback);
    }
    else
    {
        if (err_msg != nullptr)
        {
            *err_msg = "Unknown file type\n";
        }
        return false;
    }
    return true;
}

} // namespace SymFind


// NOLINTEND(readability-identifier-length,performance-unnecessary-value-param)
