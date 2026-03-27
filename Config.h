#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "Global.h"
#include "JsonParser.h"

namespace FindSymbol
{

class ConfigParser final
{
public:
    ConfigParser() noexcept = delete;
    ~ConfigParser() noexcept = default;

    ConfigParser(const ConfigParser &) noexcept = default;
    ConfigParser(ConfigParser &&) noexcept = default;

    ConfigParser &operator=(const ConfigParser &) noexcept = default;
    ConfigParser &operator=(ConfigParser &&) noexcept = default;

    explicit ConfigParser(const std::string &config_file, std::string *err_msg) noexcept;

private:
    bool parse(const std::string &config_file, std::string *err_msg) noexcept;

    std::pair<bool, std::string> validate_config(const JsonParser &json_data) const noexcept;

    std::pair<bool, std::string> extract_config(const JsonParser &json_data) noexcept;

    std::pair<bool, std::string> store_config(const std::string &key, std::string value) noexcept;

public:
    __nodiscard bool get_prune_bind_mounts() const noexcept;
    __nodiscard bool get_debug_pruning() const noexcept;
    __nodiscard const std::vector<std::string> &get_prune_fs() const noexcept;
    __nodiscard const std::vector<std::string> &get_prune_paths() const noexcept;
    __nodiscard const std::vector<std::string> &get_prune_names() const noexcept;

private:
    bool _prune_bind_mounts;
    bool _debug_pruning;
    std::vector<std::string> _prunefs;
    std::vector<std::string> _prunepaths;
    std::vector<std::string> _prunenames;

    static std::unordered_set<std::string> _valid_configs;
};

} // namespace FindSymbol
