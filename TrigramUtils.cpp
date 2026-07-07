#include "TrigramUtils.h"

// NOLINTBEGIN(readability-identifier-length,readability-magic-numbers)

namespace SymFind
{

TrigramCode encode_trigram(unsigned char a, unsigned char b, unsigned char c) noexcept
{
    return (static_cast<uint32_t>(a) << 16) | (static_cast<uint32_t>(b) << 8) | static_cast<uint32_t>(c);
}

std::uint64_t trigram_count_for_length(std::uint64_t n) noexcept
{
    return n == 0 ? 0 : n + 2;
}

constexpr std::uint64_t trigram_fibonacci_hash(TrigramCode trigram_code) noexcept
{
    constexpr std::uint64_t golden_ratio64 = 0x9E3779B97F4A7C15ULL;
    return static_cast<std::uint64_t>(trigram_code) * golden_ratio64;
}

std::uint64_t trigram_home_slot(TrigramCode trigram_code, std::uint32_t capacity_bits) noexcept
{
    if (capacity_bits == 0)
    {
        return 0;
    }

    return trigram_fibonacci_hash(trigram_code) >> (64 - capacity_bits);
}

std::uint64_t trigram_capacity_for_count(std::uint64_t count, double max_load_factor) noexcept
{
    std::uint64_t capacity = 16;
    while (capacity == 0 || static_cast<double>(count) > static_cast<double>(capacity) * max_load_factor)
    {
        capacity <<= 1;
    }
    return capacity;
}

} // namespace SymFind

// NOLINTEND(readability-identifier-length,readability-magic-numbers)
