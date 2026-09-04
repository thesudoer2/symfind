#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <cstdint>

#include <symfind/utils/Global.h>

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
    NO_TYPE = 0x01,
    DATA_OBJ = 0x02,
    FUNC = 0x03,
    RELOC_SYM = 0x04,
    FILE_SYM = 0x5,
};

enum class SymbolBind : std::uint8_t
{
    UNKNOWN = 0x00,
    LOCAL = 0x01,
    GLOBAL = 0x02,
    WEAK = 0x03,
};

enum class SymbolVisibility : std::uint8_t
{
    UNKNOWN = 0x00,
    DEFAULT = 0x01,
    INTERNAL = 0x02,
    HIDDEN = 0x03,
    PROTECTED = 0x04,
};

SYMFIND_PACK(struct SymbolMetaData
{
    bool is_defined{false};
    SymbolSourceSection source_section{SymbolSourceSection::UNKNOWN};
    SymbolType type{SymbolType::UNKNOWN};
    SymbolBind bind{SymbolBind::UNKNOWN};
    SymbolVisibility visibility{SymbolVisibility::UNKNOWN};
    std::uint32_t offset{0};
});

using SymbolName = std::string;
using SymbolNameView = std::string_view;

struct SymbolEntry
{
    SymbolName name;
    SymbolMetaData metadata;
};

struct SymbolEntryView
{
    SymbolNameView name;
    const SymbolMetaData* metadata;
};

using SymbolEntries = std::vector<SymbolEntry>;

std::string symbol_source_section_to_str(SymbolSourceSection sym_sec) noexcept;
std::string symbol_type_to_str(SymbolType sym_type) noexcept;
std::string symbol_bind_to_str(SymbolBind sym_bind) noexcept;
std::string symbol_visibility_to_str(SymbolVisibility sym_visibility) noexcept;


} // namespace SymFind
