#include <symfind/core/Symbol.h>

namespace SymFind
{

std::string symbol_source_section_to_str(SymbolSourceSection sym_sec) noexcept
{
    using namespace SymFind;

    switch (sym_sec)
    {
    case SymbolSourceSection::UNKNOWN:
        return "UNKNOWN";
    case SymbolSourceSection::SYMTAB:
        return "SYMTABLE";
    case SymbolSourceSection::DYNSYM:
        return "DYNAMIC_SYMTABLE";
    default:
        return "INVALID";
    }
}

std::string symbol_type_to_str(SymbolType sym_type) noexcept
{
    using namespace SymFind;

    switch (sym_type)
    {
    case SymbolType::UNKNOWN:
        return "UNKNOWN";
    case SymbolType::NO_TYPE:
        return "NO_TYPE";
    case SymbolType::DATA_OBJ:
        return "DATA";
    case SymbolType::FUNC:
        return "FUNCTION";
    case SymbolType::RELOC_SYM:
        return "RELOCATION_SYMBOL";
    default:
        return "INVALID";
    }
}

std::string symbol_bind_to_str(SymbolBind sym_bind) noexcept
{
    using namespace SymFind;

    switch (sym_bind)
    {
    case SymbolBind::UNKNOWN:
        return "UNKNOWN";
    case SymbolBind::LOCAL:
        return "LOCAL";
    case SymbolBind::GLOBAL:
        return "GLOBAL";
    case SymbolBind::WEAK:
        return "WEAK";
    default:
        return "INVALID";
    }
}

std::string symbol_visibility_to_str(SymbolVisibility sym_visibility) noexcept
{
    using namespace SymFind;

    switch (sym_visibility)
    {
    case SymbolVisibility::UNKNOWN:
        return "UNKNOWN";
    case SymbolVisibility::DEFAULT:
        return "DEFAULT";
    case SymbolVisibility::INTERNAL:
        return "INTERNAL";
    case SymbolVisibility::HIDDEN:
        return "HIDDEN";
    case SymbolVisibility::PROTECTED:
        return "PROTECTED";
    default:
        return "INVALID";
    }
}

}
