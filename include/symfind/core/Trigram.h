#pragma once

//
// Trigram.h
//
// Build-time only. Consumes (word, symbol_id) pairs and produces the
// uncompressed on-disk trigram block described in trigram_format.hpp.
//
// NOTE ON THREADING: this class is intentionally NOT thread-safe. It is
// meant to be driven by a single aggregation pass *after* your concurrent
// parser has finished, e.g.:
//
//   TrigramBuilder builder;
//   for (const auto& [symbol_name, refs] : concurrent_map) {
//       std::uint32_t symbol_id = symbol_index.id_for(symbol_name); // however
//                                                               // you assign
//                                                               // SymbolIndex
//                                                               // IDs
//       builder.add_word(symbol_name, symbol_id);
//   }
//   ByteArray block;
//   bool res = builder.build(block);
//

#include <cinttypes>

#include <string_view>
#include <unordered_map>
#include <vector>

#include <symfind/utils/Global.h>

namespace SymFind
{

class TrigramBuilder
{
public:
    using TrigramToSymbolsMap = std::unordered_map<std::uint32_t, std::vector<std::uint32_t>>;

    // Registers `word` (a symbol name) under `symbol_id` (its SymbolIndex
    // ID). Safe to call multiple times with the same symbol_id and
    // different words, or the same word from different symbol_ids (e.g.
    // the same symbol name provided by multiple libraries) -- postings are
    // deduplicated automatically in build().
    void add_word(std::string_view word, std::uint32_t symbol_id) noexcept;

    // Serializes everything gathered so far into a single contiguous,
    // 16-byte-aligned buffer matching the layout in trigram_format.hpp.
    // This buffer is what you hand to ZSTD_compressCCtx (no dictionary --
    // per the spec, the dictionary is reserved for file/symbol name
    // blocks).
    // __nodiscard bool build(std::vector<std::byte> &out) const noexcept;

    __nodiscard std::uint64_t symbol_count() const noexcept;

    __nodiscard std::uint64_t unique_trigram_count() const noexcept;

    __nodiscard const TrigramToSymbolsMap &get_trigram_to_symbols() const noexcept;

private:
    TrigramToSymbolsMap _trigram_to_symbols;
    std::vector<std::uint32_t> _scratch_trigrams; // reused per add_word() call
    std::uint64_t _symbol_count{0};
};

} // namespace SymFind
