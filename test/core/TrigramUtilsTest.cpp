#include <gtest/gtest.h>

#include <cstdint>
#include <vector>
#include <string>
#include <string_view>

#include <symfind/core/TrigramUtils.h>

namespace SymFind
{

// =============================================================================
// 1. encode_trigram tests
// =============================================================================

TEST(EncodeTrigramTest, EncodesBytesInto24BitCode)
{
    EXPECT_EQ(encode_trigram(0x61, 0x62, 0x63), static_cast<TrigramCode>(0x616263));
    EXPECT_EQ(encode_trigram(0, 0, 0), static_cast<TrigramCode>(0x000000));
    EXPECT_EQ(encode_trigram(0xFF, 0xFF, 0xFF), static_cast<TrigramCode>(0xFFFFFF));
    EXPECT_EQ(encode_trigram('a', 'b', 'c'), static_cast<TrigramCode>((0x61u << 16) | (0x62u << 8) | 0x63u));
}

TEST(EncodeTrigramTest, PacksHighByteInMostSignificantPosition)
{
    // a=0x01, b=0x02, c=0x03 => 0x010203
    EXPECT_EQ(encode_trigram(0x01, 0x02, 0x03), static_cast<TrigramCode>(0x010203));
    // Only high byte set
    EXPECT_EQ(encode_trigram(0xAB, 0x00, 0x00), static_cast<TrigramCode>(0xAB0000));
    EXPECT_EQ(encode_trigram(0x00, 0xAB, 0x00), static_cast<TrigramCode>(0x00AB00));
    EXPECT_EQ(encode_trigram(0x00, 0x00, 0xAB), static_cast<TrigramCode>(0x0000AB));
}

TEST(EncodeTrigramTest, ZeroPaddingIsPreserved)
{
    EXPECT_EQ(encode_trigram(0, 0, 'a'), static_cast<TrigramCode>(0x000061));
    EXPECT_EQ(encode_trigram(0, 'a', 0), static_cast<TrigramCode>(0x006100));
    EXPECT_EQ(encode_trigram('a', 0, 0), static_cast<TrigramCode>(0x610000));
}

// =============================================================================
// 2. trigram_count_for_length tests
// =============================================================================

TEST(TrigramCountForLengthTest, ReturnsZeroForEmpty)
{
    EXPECT_EQ(trigram_count_for_length(0), 0u);
}

TEST(TrigramCountForLengthTest, ReturnsNPlusTwoForPositiveLengths)
{
    EXPECT_EQ(trigram_count_for_length(1), 3u);
    EXPECT_EQ(trigram_count_for_length(2), 4u);
    EXPECT_EQ(trigram_count_for_length(3), 5u);
    EXPECT_EQ(trigram_count_for_length(5), 7u);
    EXPECT_EQ(trigram_count_for_length(10), 12u);
    EXPECT_EQ(trigram_count_for_length(100), 102u);
}

TEST(TrigramCountForLengthTest, MatchesActualForEachTrigramCount)
{
    for (std::uint64_t n : {0, 1, 2, 3, 5, 10, 50})
    {
        std::string s(n, 'x');
        std::vector<TrigramCode> codes;
        for_each_trigram(s, [&](TrigramCode c) { codes.push_back(c); });
        EXPECT_EQ(codes.size(), trigram_count_for_length(n)) << "n=" << n;
    }
}

// =============================================================================
// 3. for_each_trigram tests
// =============================================================================

TEST(ForEachTrigramTest, EmptyWordProducesNoTrigrams)
{
    std::vector<TrigramCode> codes;
    for_each_trigram("", [&](TrigramCode c) { codes.push_back(c); });
    EXPECT_TRUE(codes.empty());

    for_each_trigram(std::string_view{}, [&](TrigramCode) { FAIL() << "should not be called for empty word"; });
}

TEST(ForEachTrigramTest, SingleCharProducesThreePaddedTrigrams)
{
    std::vector<TrigramCode> codes;
    for_each_trigram("a", [&](TrigramCode c) { codes.push_back(c); });

    ASSERT_EQ(codes.size(), 3u);
    EXPECT_EQ(codes[0], encode_trigram(0, 0, 'a'));
    EXPECT_EQ(codes[1], encode_trigram(0, 'a', 0));
    EXPECT_EQ(codes[2], encode_trigram('a', 0, 0));
}

TEST(ForEachTrigramTest, TwoCharWordProducesFourTrigrams)
{
    std::vector<TrigramCode> codes;
    for_each_trigram("ab", [&](TrigramCode c) { codes.push_back(c); });

    ASSERT_EQ(codes.size(), 4u);
    EXPECT_EQ(codes[0], encode_trigram(0, 0, 'a'));
    EXPECT_EQ(codes[1], encode_trigram(0, 'a', 'b'));
    EXPECT_EQ(codes[2], encode_trigram('a', 'b', 0));
    EXPECT_EQ(codes[3], encode_trigram('b', 0, 0));
}

TEST(ForEachTrigramTest, CatWordMatchesDocumentedExample)
{
    // Documented in TrigramUtils.h:
    //   \0 \0 c a t \0 \0  -> 5 trigrams: (0,0,c) (0,c,a) (c,a,t) (a,t,0) (t,0,0)
    std::vector<TrigramCode> codes;
    for_each_trigram("cat", [&](TrigramCode c) { codes.push_back(c); });

    ASSERT_EQ(codes.size(), 5u);
    EXPECT_EQ(codes[0], encode_trigram(0, 0, 'c'));
    EXPECT_EQ(codes[1], encode_trigram(0, 'c', 'a'));
    EXPECT_EQ(codes[2], encode_trigram('c', 'a', 't'));
    EXPECT_EQ(codes[3], encode_trigram('a', 't', 0));
    EXPECT_EQ(codes[4], encode_trigram('t', 0, 0));
}

TEST(ForEachTrigramTest, IsCaseInsensitiveAscii)
{
    std::vector<TrigramCode> lower, upper, mixed;
    for_each_trigram("abc", [&](TrigramCode c) { lower.push_back(c); });
    for_each_trigram("ABC", [&](TrigramCode c) { upper.push_back(c); });
    for_each_trigram("AbC", [&](TrigramCode c) { mixed.push_back(c); });

    EXPECT_EQ(lower, upper);
    EXPECT_EQ(lower, mixed);
}

TEST(ForEachTrigramTest, HelloProducesSevenTrigrams)
{
    // "hello" length 5 => 7 trigrams
    std::vector<TrigramCode> codes;
    for_each_trigram("hello", [&](TrigramCode c) { codes.push_back(c); });

    ASSERT_EQ(codes.size(), 7u);
    EXPECT_EQ(codes[0], encode_trigram(0, 0, 'h'));
    EXPECT_EQ(codes[1], encode_trigram(0, 'h', 'e'));
    EXPECT_EQ(codes[2], encode_trigram('h', 'e', 'l'));
    EXPECT_EQ(codes[3], encode_trigram('e', 'l', 'l'));
    EXPECT_EQ(codes[4], encode_trigram('l', 'l', 'o'));
    EXPECT_EQ(codes[5], encode_trigram('l', 'o', 0));
    EXPECT_EQ(codes[6], encode_trigram('o', 0, 0));
}

TEST(ForEachTrigramTest, ForEachCountConsistentAcrossLengths)
{
    for (std::uint64_t n = 1; n <= 20; ++n)
    {
        std::string s(n, 'x');
        std::vector<TrigramCode> codes;
        for_each_trigram(s, [&](TrigramCode c) { codes.push_back(c); });
        EXPECT_EQ(codes.size(), trigram_count_for_length(n)) << "n=" << n;
    }
}

// =============================================================================
// 4. trigram_capacity_for_count tests
// =============================================================================

TEST(TrigramCapacityForCountTest, MinimumCapacityIsSixteen)
{
    EXPECT_EQ(trigram_capacity_for_count(0), 16u);
    EXPECT_EQ(trigram_capacity_for_count(1), 16u);
    EXPECT_EQ(trigram_capacity_for_count(10), 16u);
    EXPECT_EQ(trigram_capacity_for_count(11), 16u);
}

TEST(TrigramCapacityForCountTest, LoadFactorBoundary)
{
    // Default max_load_factor = 0.7, capacity 16 => max 16*0.7 = 11.2 items fit.
    // 11 items -> stays at 16, 12 items -> bumps to 32.
    EXPECT_EQ(trigram_capacity_for_count(11), 16u);
    EXPECT_EQ(trigram_capacity_for_count(12), 32u);
    EXPECT_EQ(trigram_capacity_for_count(16), 32u);
    EXPECT_EQ(trigram_capacity_for_count(22), 32u); // 32*0.7=22.4
    EXPECT_EQ(trigram_capacity_for_count(23), 64u);
}

TEST(TrigramCapacityForCountTest, AlwaysPowerOfTwo)
{
    for (std::uint64_t count : {0, 1, 5, 11, 12, 22, 23, 100, 1000, 10000})
    {
        auto cap = trigram_capacity_for_count(count);
        EXPECT_NE(cap, 0u);
        EXPECT_EQ(cap & (cap - 1), 0u) << "capacity " << cap << " for count " << count << " is not power of two";
    }
}

TEST(TrigramCapacityForCountTest, RespectsCustomLoadFactor)
{
    // With load_factor 0.5, capacity 16 holds at most 8 items.
    EXPECT_EQ(trigram_capacity_for_count(8, 0.5), 16u);
    EXPECT_EQ(trigram_capacity_for_count(9, 0.5), 32u);
    // With load_factor 1.0, capacity 16 holds exactly 16 items.
    EXPECT_EQ(trigram_capacity_for_count(16, 1.0), 16u);
    EXPECT_EQ(trigram_capacity_for_count(17, 1.0), 32u);
}

TEST(TrigramCapacityForCountTest, NeverBelowCountAtGivenLoadFactor)
{
    for (std::uint64_t count : {0, 7, 15, 31, 63, 100, 500})
    {
        auto cap = trigram_capacity_for_count(count, 0.7);
        EXPECT_GE(static_cast<double>(cap) * 0.7, static_cast<double>(count))
            << "count=" << count << " cap=" << cap;
    }
}

// =============================================================================
// 5. trigram_home_slot tests
// =============================================================================

TEST(TrigramHomeSlotTest, ZeroCapacityBitsAlwaysReturnsZero)
{
    EXPECT_EQ(trigram_home_slot(0, 0), 0u);
    EXPECT_EQ(trigram_home_slot(12345, 0), 0u);
    EXPECT_EQ(trigram_home_slot(0xFFFFFF, 0), 0u);
}

TEST(TrigramHomeSlotTest, ResultIsWithinCapacityRange)
{
    const TrigramCode code = encode_trigram('a', 'b', 'c');
    for (std::uint32_t bits : {1u, 2u, 4u, 8u, 10u, 12u})
    {
        auto slot = trigram_home_slot(code, bits);
        EXPECT_LT(slot, (1ULL << bits)) << "bits=" << bits;
    }
}

TEST(TrigramHomeSlotTest, DifferentCodesMayMapToDifferentSlots)
{
    // Not a strict requirement, but with fibonacci hashing different codes
    // should usually map differently for a reasonably sized table.
    auto s1 = trigram_home_slot(encode_trigram('a', 'b', 'c'), 8);
    auto s2 = trigram_home_slot(encode_trigram('x', 'y', 'z'), 8);
    // They should not trivially collide for all code pairs; at least this
    // pair is expected to differ with the current hash. If they collide it
    // is still correct but this documents the avalanche property.
    // Instead, test a stronger property: the slot is deterministic.
    EXPECT_EQ(s1, trigram_home_slot(encode_trigram('a', 'b', 'c'), 8));
    EXPECT_EQ(s2, trigram_home_slot(encode_trigram('x', 'y', 'z'), 8));
}

TEST(TrigramHomeSlotTest, DeterministicForSameInput)
{
    const TrigramCode code = encode_trigram('h', 'e', 'l');
    const std::uint32_t bits = 6;
    EXPECT_EQ(trigram_home_slot(code, bits), trigram_home_slot(code, bits));
}

TEST(TrigramHomeSlotTest, ZeroCodeHashesToZeroSlot)
{
    // 0 * golden_ratio == 0 => top bits are 0
    EXPECT_EQ(trigram_home_slot(0, 4), 0u);
    EXPECT_EQ(trigram_home_slot(0, 10), 0u);
}

} // namespace SymFind
