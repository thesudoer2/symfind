#pragma once

//
// TrigramUtils.h
//
// Trigram extraction and hashing logic.
//

#include <string_view>
#include <vector>

#include <cstdint>

// NOLINTBEGIN(readability-identifier-length)

namespace SymFind
{

using TrigramCode = std::uint32_t;

struct TrigramEntry
{
    TrigramCode code;
    std::vector<std::uint32_t> symbols;
};

using TrigramCodeList = std::vector<TrigramCode>;
using TrigramEntries = std::vector<TrigramEntry>;

// Encodes 3 bytes into a 24-bit-in-a-32-bit trigram code.
TrigramCode encode_trigram(unsigned char a, unsigned char b, unsigned char c) noexcept;

// Extracts every trigram of `word` and invokes fn(TrigramCode code) for each.
//
// The word is conceptually padded with two NUL bytes on each side before
// sliding a window of 3 across it, e.g. for "cat":
//
//   \0 \0 c a t \0 \0
//   ^-----^                  -> (\0,\0,c)
//      ^-----^                -> (\0,c,a)
//         ^-----^              -> (c,a,t)
//            ^-----^            -> (a,t,\0)
//               ^-----^         -> (t,\0,\0)
//
// This is the same trick plocate/agrep-style fuzzy matchers use: it lets
// short words (even single characters) still produce trigrams, and it
// encodes "starts with" / "ends with" information into the trigram set,
// which noticeably improves match quality for identifier-style tokens
// like symbol names (e.g. it helps distinguish "vector" as a whole word
// from "vector" as a substring of "std::vector::push_back").
//
// Characters are lowercased (ASCII only, which is all ELF symbol names
// use) so search is case-insensitive.
//
// Trigram count for a word of length n (n >= 1) is always n + 2.
template <class Fn>
inline void for_each_trigram(std::string_view word, Fn &&fn) noexcept
{
    const auto n = static_cast<std::int64_t>(word.size());
    if (n <= 0)
    {
        return;
    }

    auto at = [&](std::int64_t i) -> unsigned char {
        if (i < 0 || i >= n)
        {
            return 0;
        }

        auto c = static_cast<unsigned char>(word[static_cast<std::size_t>(i)]);
        if (c >= 'A' && c <= 'Z')
        {
            c = static_cast<unsigned char>(c - 'A' + 'a');
        }

        return c;
    };

    {
        for (std::int64_t i = -2; i <= n - 1; ++i)
        {
            fn(encode_trigram(at(i), at(i + 1), at(i + 2)));
        }
    }
}

// Number of trigrams for a word of length n, matching for_each_trigram
// above. Exposed separately so callers who only need the count (not the
// codes) don't have to run the loop.
std::uint64_t trigram_count_for_length(std::uint64_t n) noexcept;

// Fibonacci (multiplicative) hashing of the trigram code. We deliberately
// avoid std::hash<uint32_t> because libstdc++/libc++ implementations are
// often the identity function for integral types, which would leave the
// high bits of the resulting table index correlated with the (fairly
// low-entropy, since trigram bytes are mostly printable-ASCII) input
// bytes. Multiplying by a fixed odd 64-bit constant and taking the high
// bits of the product gives good avalanche behavior cheaply.
constexpr std::uint64_t trigram_fibonacci_hash(TrigramCode trigram_code) noexcept;

// Maps a trigram code to its ideal (pre-probing) slot index in a table of
// the given capacity (must be a power of two). Takes the top `capacity_bits`
// bits of the hash, which are the highest-quality (most mixed) bits of a
// multiplicative hash.
std::uint64_t trigram_home_slot(TrigramCode trigram_code, std::uint32_t capacity_bits) noexcept;

// Smallest power of two capacity such that `count` items fit at or below
// the given load factor (default 0.7, a reasonable tradeoff between probe
// length and wasted space for linear-probed open addressing).
std::uint64_t trigram_capacity_for_count(std::uint64_t count, double max_load_factor = 0.7) noexcept;

} // namespace SymFind

// NOLINTEND(readability-identifier-length)
