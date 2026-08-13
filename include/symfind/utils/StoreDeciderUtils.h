#pragma once

#include <symfind/core/StringComparator.h>
#include <symfind/core/Symbol.h>

namespace SymFind
{

enum SymbolDefinitionToPrint : std::uint8_t
{
    SymbolDefinitionToPrint_NOT_SET = 0x00,
    SymbolDefinitionToPrint_ONLYDEFINED = 0x01,
    SymbolDefinitionToPrint_ONLYRUNTIME = 0x02,
    SymbolDefinitionToPrint_SHOWFULL = 0x04,
    SymbolDefinitionToPrint_UNKNOWN = 0x08,
};

bool symfinder_symbol_should_be_stored(const SymbolEntry &sym_ent,
                                       SymbolDefinitionToPrint def_to_print,
                                       const StringComparator &compare_sym_names) noexcept;

bool database_reader_entry_should_be_stored(const SymFind::SymbolEntryView &sym_ent_v,
                                            SymbolDefinitionToPrint def_to_print,
                                            const StringComparator &compare_sym_names) noexcept;

} // namespace SymFind
