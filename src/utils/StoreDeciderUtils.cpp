#include <symfind/utils/StoreDeciderUtils.h>

#include <symfind/core/StringComparator.h>
#include <symfind/core/Symbol.h>

namespace SymFind
{

bool symfinder_symbol_should_be_stored(const SymbolEntry &sym_ent,
                                       SymbolDefinitionToPrint def_to_print,
                                       const StringComparator &compare_sym_names) noexcept
{
    if (compare_sym_names(sym_ent.name))
    {
        const auto &sym_metadata = sym_ent.metadata;
        if (sym_metadata.is_defined && (def_to_print == SymbolDefinitionToPrint_ONLYDEFINED ||
                                         def_to_print == SymbolDefinitionToPrint_SHOWFULL))
        {
            return true;
        }

        if (sym_metadata.is_defined && (def_to_print == SymbolDefinitionToPrint_ONLYDEFINED ||
                                         def_to_print == SymbolDefinitionToPrint_SHOWFULL))
        {
            return true;
        }
    }

    return false;
}

bool database_reader_entry_should_be_stored(const SymFind::SymbolEntryView &sym_ent_v,
                                            SymbolDefinitionToPrint def_to_print,
                                            const StringComparator &compare_sym_names) noexcept
{
    // TODO: Avoid copying symbol name.
    if (compare_sym_names(sym_ent_v.name))
    {
        const auto &sym_metadata = sym_ent_v.metadata;
        if (sym_metadata->is_defined && (def_to_print == SymbolDefinitionToPrint_ONLYDEFINED ||
                                         def_to_print == SymbolDefinitionToPrint_SHOWFULL))
        {
            return true;
        }

        if (sym_metadata->is_defined && (def_to_print == SymbolDefinitionToPrint_ONLYDEFINED ||
                                         def_to_print == SymbolDefinitionToPrint_SHOWFULL))
        {
            return true;
        }
    }

    return false;
}

} // namespace SymFind
