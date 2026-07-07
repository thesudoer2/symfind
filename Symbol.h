#pragma once

#include <memory>
#include <string>
#include <vector>

#include <cinttypes>

#include "Global.h"

namespace SymFind
{

enum class SymbolSourceSection : std::uint8_t
{
    UNKNOWN = 0x00,
    SYMTAB = 0x01, // SHT_SYMTAB (.symtab)
    DYNSYM = 0x02, // SHT_DYNSYM (.dynsym)
};

enum class SymbolType : std::uint8_t
{
    UNKNOWN = 0x00,
    NODEF = 0x01,
    OBJSYM = 0x02,
    FUNC = 0x04,
    SECSYM = 0x08,
};

enum class SymbolBind : std::uint8_t
{
    UNKNOWN = 0x00,
    LOCAL = 0x01,
    GLOBAL = 0x02,
    WEAK = 0x04,
};

enum class SymbolVisibility : std::uint8_t
{
    UNKNOWN = 0x00,
    DEFAULT = 0x01,
    INTERNAL = 0x02,
    HIDDEN = 0x04,
    PROTECTED = 0x8,
};

SYMFIND_PACK(struct SymbolMetaData
{
    SymbolSourceSection source_section{SymbolSourceSection::UNKNOWN};
    SymbolType type{SymbolType::UNKNOWN};
    SymbolBind bind{SymbolBind::UNKNOWN};
    SymbolVisibility visibility{SymbolVisibility::UNKNOWN};
    bool is_defined{false};
    std::uint32_t offset{0};
});

using SymbolName = std::string;

struct SymbolEntry
{
    SymbolName name;
    SymbolMetaData metadata;
};

using SymbolEntries = std::vector<SymbolEntry>;

std::string symbol_source_section_to_str(SymbolSourceSection sym_sec) noexcept;
std::string symbol_type_to_str(SymbolType sym_type) noexcept;
std::string symbol_bind_to_str(SymbolBind sym_bind) noexcept;
std::string symbol_visibility_to_str(SymbolVisibility sym_visibility) noexcept;


} // namespace SymFind
