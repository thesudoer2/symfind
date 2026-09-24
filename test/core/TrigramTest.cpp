#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
#include <algorithm>

#include <symfind/core/Trigram.h>
#include <symfind/core/TrigramUtils.h>

namespace SymFind
{

// =============================================================================
// 1. Initial state and empty-word handling
// =============================================================================

TEST(TrigramBuilderTest, InitialStateIsEmpty)
{
    TrigramBuilder builder;
    EXPECT_EQ(builder.symbol_count(), 0u);
    EXPECT_EQ(builder.unique_trigram_count(), 0u);
    EXPECT_TRUE(builder.get_trigram_to_symbols().empty());
}

TEST(TrigramBuilderTest, EmptyWordIsNoOp)
{
    TrigramBuilder builder;
    builder.add_word("", 0);
    EXPECT_EQ(builder.symbol_count(), 0u);
    EXPECT_EQ(builder.unique_trigram_count(), 0u);
    EXPECT_TRUE(builder.get_trigram_to_symbols().empty());

    builder.add_word(std::string_view{}, 42);
    EXPECT_EQ(builder.symbol_count(), 0u);
    EXPECT_EQ(builder.unique_trigram_count(), 0u);
}

TEST(TrigramBuilderTest, EmptyWordAfterNonEmptyDoesNotChangeCount)
{
    TrigramBuilder builder;
    builder.add_word("hello", 1);
    const auto sc_before = builder.symbol_count();
    const auto uniq_before = builder.unique_trigram_count();

    builder.add_word("", 2);
    EXPECT_EQ(builder.symbol_count(), sc_before);
    EXPECT_EQ(builder.unique_trigram_count(), uniq_before);
}

// =============================================================================
// 2. Single word insertion
// =============================================================================

TEST(TrigramBuilderTest, SingleWordIncrementsSymbolCountAndUniqueTrigrams)
{
    TrigramBuilder builder;
    builder.add_word("hello", 0);

    EXPECT_EQ(builder.symbol_count(), 1u);
    // "hello" length 5 => 7 trigrams, all unique for this word
    EXPECT_EQ(builder.unique_trigram_count(), 7u);
    EXPECT_EQ(builder.get_trigram_to_symbols().size(), 7u);
}

TEST(TrigramBuilderTest, SingleCharWordProducesThreeTrigrams)
{
    TrigramBuilder builder;
    builder.add_word("a", 10);

    EXPECT_EQ(builder.symbol_count(), 1u);
    EXPECT_EQ(builder.unique_trigram_count(), 3u);

    const auto &map = builder.get_trigram_to_symbols();
    EXPECT_NE(map.find(encode_trigram(0, 0, 'a')), map.end());
    EXPECT_NE(map.find(encode_trigram(0, 'a', 0)), map.end());
    EXPECT_NE(map.find(encode_trigram('a', 0, 0)), map.end());
}

TEST(TrigramBuilderTest, PostingsContainCorrectSymbolId)
{
    TrigramBuilder builder;
    builder.add_word("cat", 42);

    const auto &map = builder.get_trigram_to_symbols();
    for (const auto &[code, symbols] : map)
    {
        ASSERT_EQ(symbols.size(), 1u);
        EXPECT_EQ(symbols[0], 42u) << "code=" << std::hex << code;
    }
}

TEST(TrigramBuilderTest, TrigramSetMatchesForEachTrigram)
{
    TrigramBuilder builder;
    builder.add_word("hello", 7);

    std::vector<TrigramCode> expected;
    for_each_trigram("hello", [&](TrigramCode c) { expected.push_back(c); });
    std::sort(expected.begin(), expected.end());
    expected.erase(std::unique(expected.begin(), expected.end()), expected.end());

    const auto &map = builder.get_trigram_to_symbols();
    std::vector<TrigramCode> actual;
    actual.reserve(map.size());
    for (const auto &[code, _] : map)
        actual.push_back(code);
    std::sort(actual.begin(), actual.end());

    EXPECT_EQ(actual, expected);
}

// =============================================================================
// 3. Deduplication within a single word
// =============================================================================

TEST(TrigramBuilderTest, RepeatedTrigramsWithinWordAreDeduplicated)
{
    // "aaaa" length 4 => raw 6 trigrams but "a,a,a" appears twice, so 5 unique.
    // TrigramBuilder dedups _scratch_trigrams per add_word() call.
    TrigramBuilder builder;
    builder.add_word("aaaa", 5);

    EXPECT_EQ(builder.symbol_count(), 1u);
    EXPECT_EQ(builder.unique_trigram_count(), 5u);
    EXPECT_EQ(builder.get_trigram_to_symbols().size(), 5u);

    // Each posting should contain the symbol exactly once, not twice.
    for (const auto &[code, symbols] : builder.get_trigram_to_symbols())
    {
        EXPECT_EQ(symbols.size(), 1u) << "code=" << std::hex << code;
        EXPECT_EQ(symbols[0], 5u);
    }
}

TEST(TrigramBuilderTest, WordWithAllSameTrigramsDedups)
{
    // "aaa" already tested as 5 unique; "aaaaa" length 5 => raw 7, but
    // the middle trigram (a,a,a) appears 3 times, so unique is 5 as well.
    TrigramBuilder builder;
    builder.add_word("aaaaa", 1);
    // Let's compute expected by deduplicating for_each output
    std::vector<TrigramCode> raw;
    for_each_trigram("aaaaa", [&](TrigramCode c) { raw.push_back(c); });
    std::sort(raw.begin(), raw.end());
    raw.erase(std::unique(raw.begin(), raw.end()), raw.end());

    EXPECT_EQ(builder.unique_trigram_count(), raw.size());
}

// =============================================================================
// 4. Case insensitivity
// =============================================================================

TEST(TrigramBuilderTest, CaseInsensitiveWordProducesSameTrigrams)
{
    TrigramBuilder lower, upper, mixed;
    lower.add_word("abc", 1);
    upper.add_word("ABC", 1);
    mixed.add_word("AbC", 1);

    EXPECT_EQ(lower.get_trigram_to_symbols(), upper.get_trigram_to_symbols());
    EXPECT_EQ(lower.get_trigram_to_symbols(), mixed.get_trigram_to_symbols());
    EXPECT_EQ(lower.unique_trigram_count(), upper.unique_trigram_count());
}

TEST(TrigramBuilderTest, CaseInsensitiveHello)
{
    TrigramBuilder b1, b2;
    b1.add_word("Hello", 10);
    b2.add_word("hello", 10);
    EXPECT_EQ(b1.get_trigram_to_symbols(), b2.get_trigram_to_symbols());
}

// =============================================================================
// 5. Multiple words / symbol sharing
// =============================================================================

TEST(TrigramBuilderTest, TwoDifferentWordsShareNoTrigramsIfUnrelated)
{
    TrigramBuilder builder;
    builder.add_word("abc", 1);
    builder.add_word("xyz", 2);

    // "abc" trigrams: 0,0,a / 0,a,b / a,b,c / b,c,0 / c,0,0
    // "xyz" trigrams: 0,0,x / 0,x,y / x,y,z / y,z,0 / z,0,0
    // No overlap expected except none of the codes coincide (since letters differ)
    EXPECT_EQ(builder.symbol_count(), 2u);
    EXPECT_EQ(builder.unique_trigram_count(), 10u);

    // Each trigram's posting list should have exactly one symbol.
    for (const auto &[code, symbols] : builder.get_trigram_to_symbols())
    {
        EXPECT_EQ(symbols.size(), 1u);
    }
}

TEST(TrigramBuilderTest, SameWordWithDifferentSymbolIdsSharesPostings)
{
    TrigramBuilder builder;
    builder.add_word("hello", 1);
    builder.add_word("hello", 2);

    EXPECT_EQ(builder.symbol_count(), 2u);
    EXPECT_EQ(builder.unique_trigram_count(), 7u);

    for (const auto &[code, symbols] : builder.get_trigram_to_symbols())
    {
        ASSERT_EQ(symbols.size(), 2u) << "code=" << std::hex << code;
        // Both symbols should appear (order is insertion order: 1 then 2)
        EXPECT_EQ(symbols[0], 1u);
        EXPECT_EQ(symbols[1], 2u);
    }
}

TEST(TrigramBuilderTest, OverlappingWordsShareSomeTrigrams)
{
    TrigramBuilder builder;
    builder.add_word("hello", 1);
    builder.add_word("helloworld", 2);

    EXPECT_EQ(builder.symbol_count(), 2u);
    // "helloworld" length 10 => 12 trigrams, "hello" 7 trigrams, some overlap
    // Unique count should be less than 19
    EXPECT_LT(builder.unique_trigram_count(), 19u);
    EXPECT_GT(builder.unique_trigram_count(), 12u);

    // Trigrams common to both should have two postings.
    // At least the hello-prefix trigrams must be shared.
    std::vector<TrigramCode> hello_codes;
    for_each_trigram("hello", [&](TrigramCode c) { hello_codes.push_back(c); });
    std::sort(hello_codes.begin(), hello_codes.end());
    hello_codes.erase(std::unique(hello_codes.begin(), hello_codes.end()), hello_codes.end());

    const auto &map = builder.get_trigram_to_symbols();
    for (TrigramCode c : hello_codes)
    {
        auto it = map.find(c);
        ASSERT_NE(it, map.end()) << "missing hello trigram " << std::hex << c;
        // The hello trigrams that are also in helloworld will have 2 entries,
        // but the suffix ones (l,o,0) (o,0,0) of hello differ from helloworld's
        // suffix, so they will have only 1. Check at least some have 2.
    }

    bool found_shared = false;
    for (TrigramCode c : hello_codes)
    {
        auto it = map.find(c);
        if (it != map.end() && it->second.size() == 2)
        {
            found_shared = true;
            break;
        }
    }
    EXPECT_TRUE(found_shared) << "Expected at least one shared trigram between hello and helloworld";
}

TEST(TrigramBuilderTest, DuplicateWordSameSymbolIdAppendsDuplicatePosting)
{
    // Current implementation does NOT deduplicate postings for the same
    // symbol_id added twice; it blindly pushes. This documents that
    // behavior rather than requiring it to be "correct".
    TrigramBuilder builder;
    builder.add_word("hello", 99);
    builder.add_word("hello", 99);

    EXPECT_EQ(builder.symbol_count(), 2u);
    for (const auto &[code, symbols] : builder.get_trigram_to_symbols())
    {
        ASSERT_EQ(symbols.size(), 2u);
        EXPECT_EQ(symbols[0], 99u);
        EXPECT_EQ(symbols[1], 99u);
    }
}

TEST(TrigramBuilderTest, MultipleDistinctWordsIncrementSymbolCount)
{
    TrigramBuilder builder;
    builder.add_word("alpha", 1);
    builder.add_word("beta", 2);
    builder.add_word("gamma", 3);

    EXPECT_EQ(builder.symbol_count(), 3u);
    EXPECT_GT(builder.unique_trigram_count(), 0u);
}

// =============================================================================
// 6. Symbol IDs are preserved verbatim
// =============================================================================

TEST(TrigramBuilderTest, PreservesLargeSymbolIds)
{
    TrigramBuilder builder;
    const std::uint32_t large_id = 0xFFFFFF00u;
    builder.add_word("test", large_id);

    for (const auto &[code, symbols] : builder.get_trigram_to_symbols())
    {
        ASSERT_EQ(symbols.size(), 1u);
        EXPECT_EQ(symbols[0], large_id);
    }
}

} // namespace SymFind
