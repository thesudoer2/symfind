#pragma once

#include <string>
#include <unordered_set>
#include <vector>

#include "Global.h"
#include "JsonParser.h"


#define PRUNE_BIND_MOUNTS_CONFIG "prune_bind_mounts"
#define DEBUG_PRUNING_CONFIG "debug_pruning"
#define PRUNE_FS_CONFIG "prunefs"
#define PRUNE_PATHS_CONFIG "prunepaths"
#define PRUNE_NAMES_CONFIG "prunenames"
#define DATABASE_PATH_CONFIG "database_path"
#define DATABASE_SCAN_PATH_CONFIG "database_scan_path"

#ifndef DATABASE_DEFAULT_PATH
#define DATABASE_DEFAULT_PATH "/var/lib/symfind/symfind.db"
#endif

#ifndef DATABASE_DEFAULT_SCAN_ROOT_PATH
#define DATABASE_DEFAULT_SCAN_ROOT_PATH "/"
#endif

namespace SymFind
{

class ConfigParser final
{
public:
    ConfigParser() noexcept;
    ~ConfigParser() noexcept = default;

    ConfigParser(const ConfigParser &) noexcept = default;
    ConfigParser(ConfigParser &&) noexcept = default;

    ConfigParser &operator=(const ConfigParser &) noexcept = default;
    ConfigParser &operator=(ConfigParser &&) noexcept = default;

    bool parse(const std::string &config_file, std::string *err_msg) noexcept;

private:
    std::pair<bool, std::string> validate_config(const JsonParser &json_data) const noexcept;

    std::pair<bool, std::string> extract_config(const JsonParser &json_data) noexcept;

    std::pair<bool, std::string> store_config(const std::string &key, const std::string &value) noexcept;

    void generate_conf_block() noexcept;

public:
    __nodiscard bool get_prune_bind_mounts() const noexcept;
    __nodiscard bool get_debug_pruning() const noexcept;
    __nodiscard const std::string &get_database_path() const noexcept;
    __nodiscard const std::string &get_database_scan_path() const noexcept;
    __nodiscard const std::vector<std::string> &get_prune_fs() const noexcept;
    __nodiscard const std::vector<std::string> &get_prune_paths() const noexcept;
    __nodiscard const std::vector<std::string> &get_prune_names() const noexcept;
    __nodiscard const std::string &get_conf_block() const noexcept;

private:
    bool _prune_bind_mounts{false};
    bool _debug_pruning{false};

    std::string _database_path;
    std::string _database_scan_path;

    std::vector<std::string> _prunefs;
    std::vector<std::string> _prunepaths;
    std::vector<std::string> _prunenames;

    /// Aggregate all configurations
    std::string _conf_block;

    static std::unordered_set<std::string> _valid_configs;
};

using ConfigParserPtr = std::shared_ptr<ConfigParser>;

} // namespace SymFind
