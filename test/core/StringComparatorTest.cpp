#include <format>
#include <memory>
#include <string_view>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <symfind/core/StringComparator.h>

namespace SymFind
{

namespace
{

bool EQUAL = true;
bool NOT_EQUAL = false;

} // namespace (anonymous)

// =============================================================================
// 1. Factory function tests
// =============================================================================
//
// Verifies that make_string_comparator() returns a non-null pointer of the
// concrete type that corresponds to the requested StringComparatorType, and
// that it does NOT satisfy a dynamic_cast to any of the other concrete types.

namespace
{

// Helper: assert that `ptr` is exactly of type T (and not any of the other
// listed comparator types), keeping each factory test to one clear assertion
// block instead of duplicating the same cast logic everywhere.
template <typename T>
void ExpectExactType(const StringComparatorPtr &ptr)
{
    ASSERT_NE(ptr, nullptr);
    EXPECT_NE(dynamic_cast<const T *>(ptr.get()), nullptr) << "Pointer is not of the expected concrete type.";
}

} // namespace (anonymous)

TEST(StringComparatorFactoryTest, CreatesDefaultStringComparator)
{
    StringComparatorPtr comp = make_string_comparator(StringComparatorType_DEFAULT, std::string("hello"));

    ExpectExactType<DefaultStringComparator>(comp);
    EXPECT_EQ(dynamic_cast<const Re2RegexStringComparator *>(comp.get()), nullptr);
    EXPECT_EQ(dynamic_cast<const FuzzyStringComparator *>(comp.get()), nullptr);
}

TEST(StringComparatorFactoryTest, CreatesRegexStringComparator)
{
    // Note: StringComparatorType_REGEX maps to the RE2-based implementation,
    // not StdRegexStringComparator (which the factory never constructs).
    StringComparatorPtr comp = make_string_comparator(StringComparatorType_REGEX, std::string("hel+o"));

    ExpectExactType<Re2RegexStringComparator>(comp);
    EXPECT_EQ(dynamic_cast<const DefaultStringComparator *>(comp.get()), nullptr);
    EXPECT_EQ(dynamic_cast<const FuzzyStringComparator *>(comp.get()), nullptr);
}

TEST(StringComparatorFactoryTest, CreatesFuzzyStringComparatorWithDefaultThreshold)
{
    StringComparatorPtr comp = make_string_comparator(StringComparatorType_FUZZY, std::string("hello"));

    ExpectExactType<FuzzyStringComparator>(comp);
    EXPECT_EQ(dynamic_cast<const DefaultStringComparator *>(comp.get()), nullptr);
    EXPECT_EQ(dynamic_cast<const Re2RegexStringComparator *>(comp.get()), nullptr);
}

TEST(StringComparatorFactoryTest, CreatesFuzzyStringComparatorWithExplicitThreshold)
{
    StringComparatorPtr comp = make_string_comparator(StringComparatorType_FUZZY, std::string("hello"), 60.0);

    ExpectExactType<FuzzyStringComparator>(comp);
}

TEST(StringComparatorFactoryTest, NotSetTypeReturnsNullptr)
{
    StringComparatorPtr comp = make_string_comparator(StringComparatorType_NOT_SET, std::string("hello"));
    EXPECT_EQ(comp, nullptr);
}

TEST(StringComparatorFactoryTest, UnknownTypeReturnsNullptr)
{
    StringComparatorPtr comp = make_string_comparator(StringComparatorType_UNKNOWN, std::string("hello"));
    EXPECT_EQ(comp, nullptr);
}

// =============================================================================
// 2. DefaultStringComparator behavior
// =============================================================================
//
// DefaultStringComparator does a plain equality check between the fixed
// string it was constructed with and the string it is compared against.

struct DefaultComparatorCase
{
    std::string fixed_str;
    std::string input;
    bool expected;
};

class DefaultStringComparatorTest : public ::testing::TestWithParam<DefaultComparatorCase>
{
};

TEST_P(DefaultStringComparatorTest, MatchesExactEquality)
{
    const auto &tc = GetParam();
    DefaultStringComparator comparator(tc.fixed_str);

    EXPECT_EQ(comparator(std::string_view(tc.input)), tc.expected) << std::format("fixed=\"{}\" input=\"{}\"", tc.fixed_str, tc.input);
}

INSTANTIATE_TEST_SUITE_P(
    DataSet,
    DefaultStringComparatorTest,
    ::testing::Values(
        DefaultComparatorCase{"hello", "hello", EQUAL},
        DefaultComparatorCase{"hello", "Hello", NOT_EQUAL},   // case sensitive
        DefaultComparatorCase{"hello", "hello ", NOT_EQUAL},  // trailing whitespace
        DefaultComparatorCase{"hello", "hell", NOT_EQUAL},    // substring, not equal
        DefaultComparatorCase{"hello", "hello world", NOT_EQUAL},
        DefaultComparatorCase{"", "", EQUAL},               // empty fixed & input
        DefaultComparatorCase{"", "x", NOT_EQUAL},
        DefaultComparatorCase{"a.b*c", "a.b*c", EQUAL},      // regex-special chars, literal here
        DefaultComparatorCase{"symfind-core", "symfind-core", EQUAL}
    )
);

// =============================================================================
// 3. Re2RegexStringComparator behavior
// =============================================================================
//
// Re2RegexStringComparator escapes the fixed string via regex_escape() before
// handing it to RE2, so the "pattern" behaves as a literal substring search
// (RE2::PartialMatch) rather than an actual regular expression. Tests reflect
// that real behavior, including on inputs containing regex metacharacters.

struct RegexComparatorCase
{
    std::string fixed_str;
    std::string input;
    bool expected;
};

class Re2RegexStringComparatorTest : public ::testing::TestWithParam<RegexComparatorCase>
{
};

TEST_P(Re2RegexStringComparatorTest, MatchesAsEscapedLiteralSubstring)
{
    const auto &tc = GetParam();
    Re2RegexStringComparator comparator(tc.fixed_str);

    EXPECT_EQ(comparator(std::string_view(tc.input)), tc.expected)
        << "fixed=\"" << tc.fixed_str << "\" input=\"" << tc.input << "\"";
}

INSTANTIATE_TEST_SUITE_P(
    DataSet,
    Re2RegexStringComparatorTest,
    ::testing::Values(
        // Exact match
        RegexComparatorCase{"hello", "hello", EQUAL},
        // Partial match: fixed_str is a substring of the input
        RegexComparatorCase{"hello", "say hello world", EQUAL},
        RegexComparatorCase{"world", "hello world", EQUAL},
        // No match
        RegexComparatorCase{"hello", "hell", NOT_EQUAL},
        RegexComparatorCase{"hello", "goodbye", NOT_EQUAL},
        // Case sensitivity
        RegexComparatorCase{"hello", "HELLO", NOT_EQUAL},
        // Fixed string contains regex metacharacters, which must be treated
        // as literal characters (thanks to regex_escape) rather than as
        // regex syntax.
        RegexComparatorCase{"a.b", "a.b", EQUAL},
        RegexComparatorCase{"a.b", "aXb", NOT_EQUAL},          // '.' must NOT act as wildcard
        RegexComparatorCase{"1+1=2", "result: 1+1=2 ok", EQUAL},
        RegexComparatorCase{"(test)", "this is a (test) case", EQUAL},
        RegexComparatorCase{"(test)", "this is a test case", NOT_EQUAL}, // literal parens required
        RegexComparatorCase{"a*b", "a*b", EQUAL},
        RegexComparatorCase{"a*b", "aaab", NOT_EQUAL},          // '*' must NOT act as quantifier
        RegexComparatorCase{"[abc]", "x[abc]y", EQUAL},
        RegexComparatorCase{"[abc]", "a", NOT_EQUAL},
        // Empty fixed string matches any input (empty pattern always matches)
        RegexComparatorCase{"", "anything",  EQUAL},
        RegexComparatorCase{"", "", EQUAL}
    )
);

// =============================================================================
// 4. FuzzyStringComparator behavior
// =============================================================================
//
// FuzzyStringComparator uses rapidfuzz's ratio-based similarity score and
// compares it against a threshold (default 80.0). To keep the tests robust
// against small differences in rapidfuzz's exact scoring algorithm, cases are
// chosen to be clearly above or clearly below the threshold rather than
// relying on borderline values.

struct FuzzyComparatorCase
{
    std::string fixed_str;
    std::string input;
    double threshold;
    bool expected;
};

class FuzzyStringComparatorTest : public ::testing::TestWithParam<FuzzyComparatorCase>
{
};

TEST_P(FuzzyStringComparatorTest, MatchesAboveOrBelowThreshold)
{
    const auto &tc = GetParam();
    FuzzyStringComparator comparator(tc.fixed_str, tc.threshold);

    EXPECT_EQ(comparator(std::string_view(tc.input)), tc.expected)
        << "fixed=\"" << tc.fixed_str << "\" input=\"" << tc.input << "\" threshold=" << tc.threshold;
}

INSTANTIATE_TEST_SUITE_P(
    DataSet,
    FuzzyStringComparatorTest,
    ::testing::Values(
        // Identical strings: 100% similarity, always above default threshold
        FuzzyComparatorCase{"hello", "hello", 80.0, EQUAL},
        // Single-character typo: high similarity, still above default threshold
        FuzzyComparatorCase{"hello", "hallo", 80.0, EQUAL},
        FuzzyComparatorCase{"hello world", "hello wrold", 80.0, EQUAL}, // transposition
        // Completely different strings: low similarity, below default threshold
        FuzzyComparatorCase{"hello", "goodbye", 80.0, NOT_EQUAL},
        FuzzyComparatorCase{"symfind", "xyz123", 80.0, NOT_EQUAL},
        // Empty input vs. non-empty fixed string: minimal similarity
        FuzzyComparatorCase{"hello", "", 80.0, NOT_EQUAL},
        // Threshold configuration changes the outcome for the same pair
        FuzzyComparatorCase{"hello", "help", 90.0, NOT_EQUAL}, // strict threshold rejects it
        FuzzyComparatorCase{"hello", "help", 50.0, EQUAL},  // lenient threshold accepts it
        // Empty fixed string vs. empty input: defined as a perfect match
        FuzzyComparatorCase{"", "", 80.0, EQUAL}
    )
);

// =============================================================================
// 5. StdRegexStringComparator behavior (documents current stub behavior)
// =============================================================================
//
// NOTE: The implementation currently has string_view -> std::regex_search
// support commented out (see the TODO in StringComparator.cpp), so
// operator() unconditionally returns true regardless of input. This test
// documents that current behavior; it should be revisited once the TODO is
// resolved and std::regex_search is actually invoked.

TEST(StdRegexStringComparatorTest, CurrentlyAlwaysReturnsTrueRegardlessOfInput)
{
    StdRegexStringComparator comparator("hello");

    EXPECT_TRUE(comparator(std::string_view("hello")));
    EXPECT_TRUE(comparator(std::string_view("completely unrelated string")));
    EXPECT_TRUE(comparator(std::string_view("")));
}

} // namespace SymFind
