#include "Config.h"

#include <cctype>
#include <cstdlib>

#include <string>
#include <tuple>

#include "FileWrapper.h"
#include "Global.h"
#include "JsonParser.h"

#define PRUNE_BIND_MOUNTS_CONFIG "prune_bind_mounts"
#define DEBUG_PRUNING_CONFIG "debug_pruning"
#define PRUNE_FS_CONFIG "prunefs"
#define PRUNE_PATHS_CONFIG "prunepaths"
#define PRUNE_NAMES_CONFIG "prunenames"

namespace FindSymbol
{

std::unordered_set<std::string> ConfigParser::_valid_configs{PRUNE_BIND_MOUNTS_CONFIG,
                                                             DEBUG_PRUNING_CONFIG,
                                                             PRUNE_FS_CONFIG,
                                                             PRUNE_PATHS_CONFIG,
                                                             PRUNE_NAMES_CONFIG};

ConfigParser::ConfigParser(const std::string &config_file, std::string *err_msg) noexcept
    : _prune_bind_mounts(false), _debug_pruning(false)
{
    bool parser_res = parse(config_file, err_msg);
    if (!parser_res)
    {
        exit(EXIT_FAILURE);
    }
}

bool ConfigParser::parse(const std::string &config_file, std::string *err_msg) noexcept
{
    JsonParser json_parser(config_file);

    std::string jp_err_msg;
    if (bool parse_res = json_parser.parse(&jp_err_msg); !parse_res)
    {
        *err_msg = jp_err_msg;
        return false;
    }

    auto [validation_status, validation_err_msg] = validate_config(json_parser);
    if (!validation_status)
    {
        *err_msg = validation_err_msg;
        return false;
    }

    auto [extraction_status, extraction_err_msg] = extract_config(json_parser);
    if (!extraction_status)
    {
        *err_msg = extraction_err_msg;
        return false;
    }

    return true;
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
std::pair<bool, std::string> ConfigParser::validate_config(const JsonParser &json_data) const noexcept
{
    if (!json_data.is_object())
    {
        return {false, "JSON must be an object"};
    }

    for (const auto &item : json_data.items())
    {
        const auto &key = item.key();
        const auto &value = item.value();

        if (_valid_configs.count(key) == 0)
        {
            return {false, "Invalid config '" + item.key() + "'"};
        }

        if (!value.is_string())
        {
            return {false, "Value for key '" + item.key() + "' must be a string"};
        }
    }

    return {true, {}};
}

std::pair<bool, std::string> ConfigParser::extract_config(const JsonParser &json_data) noexcept
{
    for (const auto &item : json_data.items())
    {
        const auto &key = item.key();
        const auto &value = item.value();

        auto [store_status, store_err_msg] = store_config(key, value.get_cref_value<std::string>());
        if (!store_status)
        {
            return {store_status, store_err_msg};
        }
    }

    return {true, ""};
}

expected<bool, std::string> to_bool(const std::string &str)
{
    if (strcasecmp("true", str.c_str()) == 0)
    {
        return true;
    }
    else if (strcasecmp("false", str.c_str()) == 0)
    {
        return false;
    }

    return unexpected<std::string>("Invalid value as boolean: " + str);
}

/* Add values from space-separated STR to LIST */
void break_string(std::vector<std::string> &list, const std::string &str)
{
    std::string extracted_str;
    extracted_str.resize(str.size());

    for (char ch : str) // NOLINT(readability-identifier-length)
    {
        if (std::isspace(ch) != 0)
        {
            if (!extracted_str.empty())
            {
                list.push_back(extracted_str);
                extracted_str.clear();
            }

            continue;
        }

        extracted_str += ch;
    }

    if (!extracted_str.empty())
    {
        list.push_back(extracted_str);
    }
}

std::pair<bool, std::string> ConfigParser::store_config(const std::string &key, std::string value) noexcept
{
    if (key == PRUNE_BIND_MOUNTS_CONFIG)
    {
        auto res = to_bool(value);
        if (!res.has_value())
        {
            return {false, res.error()};
        }
        _prune_bind_mounts = res.value();
    }
    else if (key == DEBUG_PRUNING_CONFIG)
    {
        auto res = to_bool(value);
        if (!res.has_value())
        {
            return {false, res.error()};
        }
        _debug_pruning = res.value();
    }
    else if (key == PRUNE_FS_CONFIG)
    {
        break_string(_prunefs, value);
    }
    else if (key == PRUNE_PATHS_CONFIG)
    {
        break_string(_prunepaths, value);
    }
    else if (key == PRUNE_NAMES_CONFIG)
    {
        break_string(_prunenames, value);
    }

    return {true, ""};
}

bool ConfigParser::get_prune_bind_mounts() const noexcept
{
    return _prune_bind_mounts;
}

bool ConfigParser::get_debug_pruning() const noexcept
{
    return _debug_pruning;
}

const std::vector<std::string> &ConfigParser::get_prune_fs() const noexcept
{
    return _prunefs;
}

const std::vector<std::string> &ConfigParser::get_prune_paths() const noexcept
{
    return _prunepaths;
}

const std::vector<std::string> &ConfigParser::get_prune_names() const noexcept
{
    return _prunenames;
}

} // namespace FindSymbol
