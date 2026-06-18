#include "Config.h"

#include <cctype>
#include <cstdlib>

#include <string>

#include "FileWrapper.h"
#include "Global.h"
#include "JsonParser.h"

namespace SymFind
{

std::unordered_set<std::string> ConfigParser::_valid_configs{PRUNE_BIND_MOUNTS_CONFIG,
                                                             DEBUG_PRUNING_CONFIG,
                                                             PRUNE_FS_CONFIG,
                                                             PRUNE_PATHS_CONFIG,
                                                             PRUNE_NAMES_CONFIG,
                                                             DATABASE_PATH_CONFIG,
                                                             DATABASE_SCAN_PATH_CONFIG};

ConfigParser::ConfigParser() noexcept : _database_path(DATABASE_DEFAULT_PATH), _database_scan_path(DATABASE_DEFAULT_SCAN_ROOT_PATH)
{
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

    generate_conf_block();

    return {true, ""};
}

/* Store a string list to OBSTACK */
void generate_conf_block_string_list(std::string &obstack, const std::vector<std::string> &strings)
{
    for (const auto &str : strings)
    {
        obstack += str;
        obstack += '\0';
    }
    obstack += '\0';
}

/* Store a string to OBSTACK */
void generate_conf_block_string(std::string &obstack, const std::string &string)
{
    obstack += string;
    obstack += "\0\0";
}

void ConfigParser::generate_conf_block() noexcept
{
    // NOLINTNEXTLINE
    auto CONST = [this]<typename T, std::size_t N>(const T(&s)[N]) { _conf_block.append(s, N); };

    _conf_block.clear();

	CONST(PRUNE_BIND_MOUNTS_CONFIG);
    /* Add two NUL bytes after the value */
	_conf_block.append(_prune_bind_mounts ? "1\0" : "0\0", 3);

    CONST(DEBUG_PRUNING_CONFIG);
    /* Add two NUL bytes after the value */
	_conf_block.append(_debug_pruning ? "1\0" : "0\0", 3);

	CONST(PRUNE_FS_CONFIG);
	generate_conf_block_string_list(_conf_block, _prunefs);

	CONST(PRUNE_NAMES_CONFIG);
	generate_conf_block_string_list(_conf_block, _prunenames);

	CONST(PRUNE_PATHS_CONFIG);
	generate_conf_block_string_list(_conf_block, _prunepaths);

    CONST(DATABASE_PATH_CONFIG);
    generate_conf_block_string(_conf_block, _database_path);

    CONST(DATABASE_SCAN_PATH_CONFIG);
    generate_conf_block_string(_conf_block, _database_scan_path);

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

void list_to_upper(std::vector<std::string> &list) noexcept
{
    for (auto &str : list)
    {
        for (char &ch : str)
        {
            ch = (char)toupper(ch);
        }
    }
}

std::pair<bool, std::string> ConfigParser::store_config(const std::string &key, const std::string &value) noexcept
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
    else if (key == DATABASE_PATH_CONFIG)
    {
        if (!value.empty())
        {
            return {true, ""};
        }
        _database_path = value;
    }
    else if (key == DATABASE_SCAN_PATH_CONFIG)
    {
        if (!value.empty())
        {
            return {true, ""};
        }
        _database_scan_path = value;
    }
    else if (key == PRUNE_FS_CONFIG)
    {
        break_string(_prunefs, value);
        list_to_upper(_prunefs);
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

const std::string &ConfigParser::get_database_path() const noexcept
{
    return _database_path;
}

const std::string &ConfigParser::get_database_scan_path() const noexcept
{
    return _database_scan_path;
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

const std::string &ConfigParser::get_conf_block() const noexcept
{
    return _conf_block;
}

} // namespace SymFind
