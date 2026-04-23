#include "ElfParser.h"

#include <cstdlib>
#include <cstring>

#include <gelf.h>
#include <libelf.h>
#include <unistd.h>
#include <cxxabi.h>

#include <iostream>
#include <memory>
#include <string>

#include "Expected.h"
#include "FileWrapper.h"
#include "Symbol.h"


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

std::string demangle(const std::string &name)
{
    int status = 0;

    std::unique_ptr<char, void (*)(void *)> demangled(abi::__cxa_demangle(name.c_str(), nullptr, nullptr, &status),
                                                      std::free);

    if (status == 0 && demangled)
    {
        return demangled.get();
    }

    return name; // fallback if not mangled
}


inline ElfPtr make_elf(Elf* raw)
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
                    SymbolEntries &parsed_symbol_entries,
                    SymbolSourceSection sec) noexcept
{
    Elf_Data *data = elf_getdata(scn, nullptr);

    std::size_t count = shdr.sh_size / shdr.sh_entsize;

    Elf_Scn *str_scn = elf_getscn(elf.get(), shdr.sh_link);
    Elf_Data *str_data = elf_getdata(str_scn, nullptr);

    for (std::size_t i = 0; i < count; i++)
    {
        GElf_Sym sym;

        // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions,bugprone-narrowing-conversions)
        gelf_getsym(data, i, &sym);

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        std::string sym_name = reinterpret_cast<char*>(str_data->d_buf) + sym.st_name;
        sym_name = demangle(sym_name);

        // NOLINTNEXTLINE(bugprone-unhandled-exception-at-new)
        SymbolMetaDataPtr sym_metadata{new SymbolMetaData{
            .source_section = sec,
            .type = get_symbol_type(sym.st_info),
            .bind = get_symbol_bind(sym.st_info),
            .visibility = SymbolVisibility::UNKNOWN, // TODO: Set symbol visitility.
            .is_defined = is_symbol_defined(sym),
            .offset = 0, // TODO: Get symbol offset.
        }};

        parsed_symbol_entries.emplace_back(
            SymbolEntry{.name = std::move(sym_name), .metadata = std::move(sym_metadata)});
    }
}

void parse_elf(ElfPtr elf, SymbolEntries &parsed_symbol_entries) noexcept
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
            parse_symtable(elf, scn, shdr, parsed_symbol_entries, SymbolSourceSection::SYMTAB);
        }
        else if (shdr.sh_type == SHT_DYNSYM)
        {
            parse_symtable(elf, scn, shdr, parsed_symbol_entries, SymbolSourceSection::SYMTAB);
        }
    }
}

void parse_archive(int fd, ElfPtr archive, SymbolEntries &parsed_symbol_entries) noexcept
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
            parse_elf(member, parsed_symbol_entries);
        }

        (void)elf_next(member.get());
    }
}

bool parse_symtables(const std::string &file, SymbolEntries &parsed_symbol_entries, std::string *err_msg) noexcept
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
        parse_elf(elf, parsed_symbol_entries);
    }
    else if (kind == ELF_K_AR)
    {
        parse_archive(fd, elf, parsed_symbol_entries);
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
