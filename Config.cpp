#include "Config.h"

#include <cstdlib>

#include <iostream>
#include <string>
#include <tuple>

#include "Global.h"
#include "JsonParser.h"

#define PRUNE_BIND_MOUNTS_CONFIG "prune_bind_mounts"
#define PRUNE_FS_CONFIG "prunefs"
#define PRUNE_PATHS_CONFIG "prunepaths"
#define PRUNE_NAMES_CONFIG "prunenames"

namespace FindSymbol
{

std::unordered_set<std::string> ConfigParser::_valid_configs{PRUNE_BIND_MOUNTS_CONFIG,
                                                             PRUNE_FS_CONFIG,
                                                             PRUNE_PATHS_CONFIG,
                                                             PRUNE_NAMES_CONFIG};

ConfigParser::ConfigParser(const std::string &config_file) noexcept : _prune_bind_mounts(false)
{
    bool parser_res = parse(config_file);
    if (!parser_res)
    {
        exit(EXIT_FAILURE);
    }
}

bool ConfigParser::parse(const std::string &config_file) noexcept
{
    JsonParser json_parser(config_file);

    std::string err_msg;
    if (bool parse_res = json_parser.parse(&err_msg); !parse_res)
    {
        std::cerr << "FindSymbol::ConfigParser -- Parsing Json file failed:\n";
        std::cerr << err_msg << '\n';
        exit(EXIT_FAILURE);
    }

    bool validation_status = false;
    std::tie(validation_status, err_msg) = validate_config(json_parser);
    if (!validation_status)
    {
        std::cerr << "FindSymbol::ConfigParser -- Validating Json file failed:\n";
        std::cerr << err_msg << '\n';
        exit(EXIT_FAILURE);
    }

    extract_config(json_parser);

    return true;
}

std::pair<bool, std::string> ConfigParser::validate_config(const JsonParser &json_data) const noexcept // NOLINT
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

void ConfigParser::extract_config(const JsonParser &json_data) noexcept
{
    for (const auto &item : json_data.items())
    {
        const auto &key = item.key();
        const auto &value = item.value();

        store_config(key, value.get_cref_value<std::string>());
    }
}

bool to_bool(const std::string &str)
{
    return strcasecmp("true", str.c_str()) == 0;
}

void ConfigParser::store_config(const std::string &key, std::string value) noexcept
{
    if (key == PRUNE_BIND_MOUNTS_CONFIG)
    {
        _prune_bind_mounts = to_bool(value);
    }
    else if (key == PRUNE_FS_CONFIG)
    {
        _prunefs = std::move(value);
    }
    else if (key == PRUNE_PATHS_CONFIG)
    {
        _prunepaths = std::move(value);
    }
    else if (key == PRUNE_NAMES_CONFIG)
    {
        _prunenames = std::move(value);
    }
}

bool ConfigParser::get_prune_bind_mounts() const noexcept
{
    return _prune_bind_mounts;
}

const std::string &ConfigParser::get_prune_fs() const noexcept
{
    return _prunefs;
}

const std::string &ConfigParser::get_prune_paths() const noexcept
{
    return _prunepaths;
}

const std::string &ConfigParser::get_prune_names() const noexcept
{
    return _prunenames;
}

} // namespace FindSymbol
