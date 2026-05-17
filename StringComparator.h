#pragma once

#include <regex>
#include <string>

#include <cinttypes>

#include <re2/re2.h>

#include <rapidfuzz/fuzz.hpp>

#include "Global.h"

namespace SymFind
{

enum class StringComparatorType : std::uint8_t
{
    DEFAULT = 0,
    REGEX = 1,
    FUZZY = 2,
};

class StringComparator
{
public:
    virtual ~StringComparator() = default;

    virtual bool operator()(const std::string &str_to_compare) const noexcept = 0;
};

class DefaultStringComparator final : public StringComparator
{
public:
    explicit DefaultStringComparator(const std::string &fixed_str) noexcept;

    bool operator()(const std::string &str_to_compare) const noexcept override;

private:
    std::string _fixed_str;
};

class StdRegexStringComparator final : public StringComparator
{
public:
    explicit StdRegexStringComparator(const std::string &fixed_str) noexcept;

    bool operator()(const std::string &str_to_compare) const noexcept override;

private:
    std::regex _regexp;
};

class Re2RegexStringComparator final : public StringComparator
{
public:
    explicit Re2RegexStringComparator(const std::string &fixed_str) noexcept;

    bool operator()(const std::string &str_to_compare) const noexcept override;

private:
    RE2 _regexp;
};

class FuzzyStringComparator : public StringComparator
{
public:
    explicit FuzzyStringComparator(const std::string &fixed_str, double threshold = 80.0);

    bool operator()(const std::string &str_to_compare) const noexcept override;

private:
    rapidfuzz::fuzz::CachedRatio<std::string::value_type> _scorer;
    double _threshold;
};

using StringComparatorPtr = std::unique_ptr<StringComparator>;

template <typename... Args>
StringComparatorPtr make_string_comparator(StringComparatorType comp_type, Args &&...args) noexcept
{
    switch (comp_type)
    {
    case StringComparatorType::DEFAULT:
        return std::make_unique<DefaultStringComparator>(std::forward<Args>(args)...);
    case StringComparatorType::REGEX:
        // return std::make_unique<StdRegexStringComparator>(std::forward<Args>(args)...);
        return std::make_unique<Re2RegexStringComparator>(std::forward<Args>(args)...);
    case StringComparatorType::FUZZY:
        return std::make_unique<FuzzyStringComparator>(std::forward<Args>(args)...);
    default:
        return nullptr;
    }
}

} // namespace SymFind
