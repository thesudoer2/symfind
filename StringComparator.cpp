#include "StringComparator.h"

#include <memory>
#include <regex>
#include <string>

#include <iostream>

#include <re2/re2.h>

#include <rapidfuzz/fuzz.hpp>

namespace SymFind
{

// -----------------------------------------------------------------------------
// Free Functions
// -----------------------------------------------------------------------------

namespace
{

std::string regex_escape(const std::string& text)
{
    static const std::string special_chars = R"(.^$|()[]{}*+?\/)";
    std::string escaped;
    escaped.reserve(text.size() * 2);  // rough estimate

    for (char c : text)
    {
        if (special_chars.find(c) != std::string::npos)
        {
            escaped += '\\';
        }
        escaped += c;
    }

    return escaped;
}

} // namespace (anonymouse)

// -----------------------------------------------------------------------------
// Default String Comparator Implementation
// -----------------------------------------------------------------------------

DefaultStringComparator::DefaultStringComparator(const std::string &fixed_str) noexcept : _fixed_str(fixed_str)
{
}

bool DefaultStringComparator::operator()(const std::string &str_to_compare) const noexcept
{
    return _fixed_str == str_to_compare;
}

// -----------------------------------------------------------------------------
// STD Regex String Comparator Implementation
// -----------------------------------------------------------------------------

StdRegexStringComparator::StdRegexStringComparator(const std::string &fixed_str) noexcept
    : _regexp(fixed_str, std::regex::optimize)
{
}

bool StdRegexStringComparator::operator()(const std::string &str_to_compare) const noexcept
{
    return std::regex_search(str_to_compare, _regexp);
}

// -----------------------------------------------------------------------------
// Re2 Regex String Comparator Implementation
// -----------------------------------------------------------------------------

Re2RegexStringComparator::Re2RegexStringComparator(const std::string &fixed_str) noexcept
    : _regexp(regex_escape(fixed_str))
{
}

bool Re2RegexStringComparator::operator()(const std::string &str_to_compare) const noexcept
{
    return RE2::PartialMatch(str_to_compare, _regexp);
}

// -----------------------------------------------------------------------------
// Fuzzy String Comparator Implementation
// -----------------------------------------------------------------------------

FuzzyStringComparator::FuzzyStringComparator(const std::string &fixed_str, double threshold)
    : _scorer(fixed_str), _threshold(threshold)
{
}

bool FuzzyStringComparator::operator()(const std::string &str_to_compare) const noexcept
{
    return _scorer.similarity(str_to_compare) >= _threshold;
}

} // namespace SymFind
