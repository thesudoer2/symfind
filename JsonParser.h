#pragma once

#include "FileWrapper.h"

#include <nlohmann/json.hpp>

#include <nlohmann/json_fwd.hpp>
#include <tl/expected.hpp>

namespace FindSymbol
{

class JsonItems;

class JsonParser final
{
    friend class JsonItem;

public:
    using Json_t = nlohmann::json;

private:
    explicit JsonParser(Json_t json_data) noexcept;

public:
    explicit JsonParser(std::string json_path) noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    bool parse(std::string *err_msg) noexcept;

    __nodiscard bool is_primitive() const noexcept;

    __nodiscard bool is_structured() const noexcept;

    __nodiscard bool is_null() const noexcept;

    __nodiscard bool is_boolean() const noexcept;

    __nodiscard bool is_number() const noexcept;

    __nodiscard bool is_number_integer() const noexcept;

    __nodiscard bool is_number_unsigned() const noexcept;

    __nodiscard bool is_number_float() const noexcept;

    __nodiscard bool is_object() const noexcept;

    __nodiscard bool is_array() const noexcept;

    __nodiscard bool is_string() const noexcept;

    __nodiscard bool is_binary() const noexcept;

    __nodiscard bool is_discarded() const noexcept;

    __nodiscard JsonItems items() const noexcept;

    template<typename ValueType>
    ValueType get_value() const noexcept
    {
        return _json_data.get<ValueType>();
    }

    template<typename ValueType>
    const ValueType& get_cref_value() const
    {
        return _json_data.get_ref<const ValueType&>();
    }

private:
    std::string _json_path;
    Json_t _json_data;
};

class JsonItem
{
public:
    JsonItem(std::string key, const JsonParser::Json_t &value) noexcept;

    __nodiscard const std::string &key() const noexcept;

    __nodiscard JsonParser value() const noexcept;

private:
    std::string _key;
    const JsonParser::Json_t &_value; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

class JsonItems
{
public:
    using base_iter = nlohmann::json::const_iterator;

public: // NOLINT(readability-redundant-access-specifiers)
    explicit JsonItems(const JsonParser::Json_t &json);

    class Iterator
    {
    public:
        explicit Iterator(base_iter it); // NOLINT(readability-identifier-length)

        Iterator &operator++() noexcept;

        bool operator!=(const Iterator &other) const noexcept;

        JsonItem operator*() const noexcept;

    private:
        base_iter _it;
    };

    __nodiscard Iterator begin() const noexcept;

    __nodiscard Iterator end() const noexcept;

private:
    const JsonParser::Json_t &_json; // NOLINT(cppcoreguidelines-avoid-const-or-ref-data-members)
};

} // namespace FindSymbol
