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
        return "SYMTAB";
    case SymbolSourceSection::DYNSYM:
        return "DYNSYM";
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
    case SymbolType::NODEF:
        return "NODEF";
    case SymbolType::OBJSYM:
        return "OBJSYM";
    case SymbolType::FUNC:
        return "FUNC";
    case SymbolType::SECSYM:
        return "SECSYM";
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
