#pragma once

#include <string>
#include <unordered_set>

#include "Global.h"
#include "JsonParser.h"

namespace FindSymbol
{

class ConfigParser final
{
public:
    ConfigParser() noexcept = delete;
    ~ConfigParser() noexcept = default;

    ConfigParser(const ConfigParser &) noexcept = delete;
    ConfigParser(ConfigParser &&) noexcept = delete;

    ConfigParser &operator=(const ConfigParser &) noexcept = delete;
    ConfigParser &operator=(ConfigParser &&) noexcept = delete;

    explicit ConfigParser(const std::string &config_file) noexcept;

private:
    bool parse(const std::string &config_file) noexcept;

    std::pair<bool, std::string> validate_config(const JsonParser &json_data) const noexcept;

    void extract_config(const JsonParser &json_data) noexcept;

    void store_config(const std::string& key, std::string value) noexcept;

public:
    __nodiscard bool get_prune_bind_mounts() const noexcept;
    __nodiscard const std::string &get_prune_fs() const noexcept;
    __nodiscard const std::string &get_prune_paths() const noexcept;
    __nodiscard const std::string &get_prune_names() const noexcept;

private:
    bool _prune_bind_mounts;
    std::string _prunefs;
    std::string _prunepaths;
    std::string _prunenames;

    static std::unordered_set<std::string> _valid_configs;
};

} // namespace FindSymbol
