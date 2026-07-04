#include "Trigram.h"

// #include <cinttypes>

#include <algorithm>
// #include <bit>
// #include <cstring>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "TrigramUtils.h"

namespace SymFind
{

void TrigramBuilder::add_word(std::string_view word, std::uint32_t symbol_id) noexcept
{
    if (word.empty())
    {
        return;
    }

    _scratch_trigrams.clear();
    for_each_trigram(word, [this](std::uint32_t code) { this->_scratch_trigrams.push_back(code); });
    std::ranges::sort(_scratch_trigrams);
    _scratch_trigrams.erase(std::ranges::unique(_scratch_trigrams).begin(), _scratch_trigrams.end());

    for (std::uint32_t code : _scratch_trigrams)
    {
        _trigram_to_symbols[code].push_back(symbol_id);
    }

    ++_symbol_count;
}

// bool TrigramBuilder::build(std::vector<std::byte> &out) const noexcept
// {
//     // 1. Finalize postings: sort + dedup each trigram's symbol list.
//     //    Sorting also makes the postings cache-friendlier and leaves
//     //    room for future galloping-intersection optimizations even
//     //    though plain Dice search doesn't require it.
//     TrigramEntries entries;

//     entries.reserve(_trigram_to_symbols.size());
//     for (const auto &[code, symbols] : _trigram_to_symbols)
//     {
//         std::vector<std::uint32_t> syms = symbols;
//         std::ranges::sort(syms);
//         syms.erase(std::ranges::unique(syms).begin(), syms.end());
//         entries.push_back(TrigramEntry{.code = code, .symbols = std::move(syms)});
//     }

//     const std::uint64_t unique_trigrams = entries.size();
//     const std::uint64_t capacity = capacity_for_count(unique_trigrams);
//     const auto capacity_bits = static_cast<std::uint32_t>(std::countr_zero(capacity));

//     // 2. Insert into the open-addressed table with linear probing.
//     //    Because capacity is chosen with headroom (load factor 0.7),
//     //    this always terminates without needing tombstones/deletion.
//     std::vector<TrigramSlot> slots(capacity, TrigramSlot{trigram_empty_slot, 0, 0});

//     std::uint64_t total_postings = 0;
//     for (const auto &ent : entries)
//     {
//         total_postings += ent.symbols.size();
//     }

//     std::vector<std::uint32_t> postings;
//     postings.reserve(total_postings);

//     for (const auto &ent : entries)
//     {
//         std::uint64_t idx = trigram_home_slot(ent.code, capacity_bits);
//         while (slots[idx].trigram_code != trigram_empty_slot)
//         {
//             idx = (idx + 1) & (capacity - 1);
//         }

//         slots[idx].trigram_code = ent.code;
//         slots[idx].posting_offset = static_cast<std::uint32_t>(postings.size());
//         slots[idx].posting_count = static_cast<std::uint32_t>(ent.symbols.size());
//         postings.insert(postings.end(), ent.symbols.begin(), ent.symbols.end());
//     }

//     // 3. Lay out header + slots + postings into one buffer.
//     Database::TrigramIndexHeader tg_hdr{};
//     tg_hdr.symbol_count = _symbol_count;
//     tg_hdr.table_capacity = capacity;
//     tg_hdr.posting_count = postings.size();

//     std::uint64_t offset = align_up(sizeof(TrigramBlockHeader), alignof(TrigramSlot));
//     tg_hdr.table_offset = offset;
//     offset += capacity * sizeof(TrigramSlot);

//     offset = align_up(offset, alignof(std::uint32_t));
//     tg_hdr.posting_offset = offset;
//     offset += postings.size() * sizeof(std::uint32_t);

//     out.resize(offset);
//     std::memcpy(out.data(), &tg_hdr, sizeof(tg_hdr));
//     std::memcpy(out.data() + tg_hdr.table_offset, slots.data(), slots.size() * sizeof(TrigramSlot));
//     std::memcpy(out.data() + tg_hdr.posting_offset, postings.data(), postings.size() * sizeof(std::uint32_t));

//     return true;
// }

std::uint64_t TrigramBuilder::symbol_count() const noexcept
{
    return _symbol_count;
}

std::uint64_t TrigramBuilder::unique_trigram_count() const noexcept
{
    return _trigram_to_symbols.size();
}

const TrigramBuilder::TrigramToSymbolsMap &TrigramBuilder::get_trigram_to_symbols() const noexcept
{
    return _trigram_to_symbols;
}

} // namespace SymFind
