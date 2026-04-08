#include "JsonParser.h"

#include <cstdlib>
#include <cstring>

#include <iostream>
#include <string>

#include "FileWrapper.h"

namespace SymFind
{

// -----------------------------------------------------------------------------
// JsonItems implementation
// -----------------------------------------------------------------------------

JsonItem::JsonItem(std::string key, const JsonParser::Json_t &value) noexcept : _key(std::move(key)), _value(value)
{
}

const std::string &JsonItem::key() const noexcept
{
    return _key;
}

JsonParser JsonItem::value() const noexcept
{
    return JsonParser(_value);
}

JsonItems::JsonItems(const JsonParser::Json_t &json) : _json(json)
{
}

JsonItems::Iterator::Iterator(base_iter it) : _it(std::move(it)) // NOLINT(readability-identifier-length)
{
}

JsonItems::Iterator JsonItems::begin() const noexcept
{
    return Iterator(_json.begin());
}

JsonItems::Iterator JsonItems::end() const noexcept
{
    return Iterator(_json.end());
}


// -----------------------------------------------------------------------------
// JsonItems::Iterator implementation
// -----------------------------------------------------------------------------

JsonItems::Iterator &JsonItems::Iterator::operator++() noexcept
{
    ++_it;
    return *this;
}

bool JsonItems::Iterator::operator!=(const Iterator &other) const noexcept
{
    return _it != other._it;
}

JsonItem JsonItems::Iterator::operator*() const noexcept
{
    return {_it.key(), _it.value()};
}


// -----------------------------------------------------------------------------
// JsonParser implementation
// -----------------------------------------------------------------------------

JsonParser::JsonParser(Json_t json_data) noexcept : _json_data(std::move(json_data))
{
}

JsonParser::JsonParser(std::string json_path) noexcept : _json_path(std::move(json_path))
{
}

bool JsonParser::parse(std::string *err_msg) noexcept
{
    FileWrapper file;
    if (bool open_res = file.open(_json_path, "r"); !open_res)
    {
        std::cerr << "SymFind::JsonParser -- Opening file failed: " << std::strerror(file.get_errno()) << '\n';
        exit(EXIT_FAILURE);
    }

    try
    {
        _json_data = Json_t::parse(file.get_fp(), nullptr, true, true);
    }
    catch (const std::exception &ex)
    {
        *err_msg = ex.what();
        return false;
    }

    return true;
}

bool JsonParser::is_primitive() const noexcept
{
    return _json_data.is_primitive();
}

bool JsonParser::is_structured() const noexcept
{
    return _json_data.is_structured();
}

bool JsonParser::is_null() const noexcept
{
    return _json_data.is_null();
}

bool JsonParser::is_boolean() const noexcept
{
    return _json_data.is_boolean();
}

bool JsonParser::is_number() const noexcept
{
    return _json_data.is_number();
}

bool JsonParser::is_number_integer() const noexcept
{
    return _json_data.is_number_integer();
}

bool JsonParser::is_number_unsigned() const noexcept
{
    return _json_data.is_number_unsigned();
}

bool JsonParser::is_number_float() const noexcept
{
    return _json_data.is_number_float();
}

bool JsonParser::is_object() const noexcept
{
    return _json_data.is_object();
}

bool JsonParser::is_array() const noexcept
{
    return _json_data.is_array();
}

bool JsonParser::is_string() const noexcept
{
    return _json_data.is_string();
}

bool JsonParser::is_binary() const noexcept
{
    return _json_data.is_binary();
}

bool JsonParser::is_discarded() const noexcept
{
    return _json_data.is_discarded();
}

JsonItems JsonParser::items() const noexcept
{
    return JsonItems(_json_data);
}

} // namespace SymFind
