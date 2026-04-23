#include "FSScanner.h"

#include <cstddef>
#include <cstring>

#include <format>
#include <memory>

#include <dirent.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/resource.h>

#include "BindMount.h"
#include "Config.h"
#include "Expected.h"
#include "FSHelper.h"
#include "DirWrapper.h"

#define SHARED_LIBRARY_EXTENSION ".so"
#define STATIC_LIBRARY_EXTENSION ".a"
#define OBJECT_FILE_EXTENSION ".o"

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg,readability-identifier-length,cppcoreguidelines-pro-bounds-array-to-pointer-decay)

namespace SymFind
{

// -----------------------------------------------------------------------------
// StringCache implementation
// -----------------------------------------------------------------------------

FSScanner::StringCache::StringID FSScanner::StringCache::store_string(const String& str) noexcept
{
    StringID id{};
    if (auto found_it = _str_to_id.find(str); found_it == _str_to_id.end())
    {
        // Store string in string list (id->string)
        _id_to_str.push_back(str);
        id = _id_to_str.size() - 1;

        // Store {str, id} pair in map (string->id)
        _str_to_id.try_emplace(str, id);
    }
    else
    {
        id = found_it->second;
    }
    return id;
}

FSScanner::StringCache::String FSScanner::StringCache::get_string(StringID id) const noexcept
{
    if (id >= _id_to_str.size())
    {
        return "";
    }
    return _id_to_str[id];
}

// -----------------------------------------------------------------------------
// FSScanner implementation
// -----------------------------------------------------------------------------

struct FoundEntry
{
    enum EntryType : std::uint8_t
    {
        UNKNOWN = 0x01,
        DIRECTORY = 0x02,
        REG_FILE = 0x04,
    };

    std::string name;
    EntryType entry_type = EntryType::UNKNOWN;

    // For directories only:
    std::shared_ptr<DirWrapper> dir{nullptr};
};

FSScanner::StringCache FSScanner::_found_files_paths_cache{};

FSScanner::FSScanner(ConfigParserPtr conf, BindMount::InstancePtr bind_mount) noexcept
    : _conf(std::move(conf)), _bind_mount(std::move(bind_mount))
{
}

std::pair<bool, std::string> FSScanner::scan() noexcept
{
    const std::string &database_scan_path = _conf->get_database_scan_path();
    std::shared_ptr<DirWrapper> root_dir(std::make_shared<DirWrapper>(database_scan_path));

    if (!root_dir->is_open())
    {
        return {false, std::strerror(root_dir->get_errno())};
    }

    auto [scan_stat, err_msg] = scan_fs(*this, root_dir);
    if (!scan_stat)
    {
        return {scan_stat, err_msg};
    }

    return {true, ""};
}

// NOLINTNEXTLINE(readability-function-cognitive-complexity,performance-unnecessary-value-param)
std::pair<bool, std::string> FSScanner::scan_fs(FSScanner &this_p, std::shared_ptr<DirWrapper> dir) noexcept
{
    const std::string& current_dir_path = dir->get_dir_path();
    const std::string path_plus_slash = current_dir_path.back() == '/' ? current_dir_path : current_dir_path + '/';

    expected<int, DirWrapper::Errno_t> fd_res = dir->get_fd();
    if (!fd_res.has_value()) [[unlikely]]
    {
        return {false, ""};
    }

    int fd = fd_res.value();

    if (std::size_t conf_prunepaths_index{0};
        FSHelper::string_list_contains_dir_path(this_p._conf->get_prune_paths(), conf_prunepaths_index, current_dir_path))
    {
        if (this_p._conf->get_debug_pruning())
        {
            fprintf(stderr, "Skipping `%s': in %s\n", path_plus_slash.c_str(), PRUNE_PATHS_CONFIG);
        }
        return {true, std::format("FSScanner scan path determined in {} list", PRUNE_PATHS_CONFIG)};
    }

    if (this_p._conf->get_prune_bind_mounts() && this_p._bind_mount->is_bind_mount(current_dir_path))
    {
        if (this_p._conf->get_debug_pruning())
        {
            fprintf(stderr, "Skipping `%s': %s\n", path_plus_slash.c_str(), PRUNE_BIND_MOUNTS_CONFIG);
        }
        return {false, std::format("FSScanner scan path determined in {} list", PRUNE_BIND_MOUNTS_CONFIG)};
    }

    DIR *dp = dir->get_dp();
    if (dp == nullptr)
    {
        // fdopendir() wants to fstat() the fd to verify that it's indeed
        // a directory, which can seemingly fail on at least CIFS filesystems
        // if the server feels like it. We treat this as if we had an error
        // on opening it, ie., ignore the directory.
        return {true, ""};
    }

    for (const struct dirent& de : *dir)
    {
        if (strcmp(de.d_name, ".") == 0 || strcmp(de.d_name, "..") == 0)
        {
            continue;
        }

        if (strlen(de.d_name) == 0)
        {
            /* Unfortunately, this does happen, and mere assert() does not give
                // users enough information to complain to the right people. */
            fprintf(stderr, "file system error: zero-length file name in directory \"%s\"", path_plus_slash.c_str());
            continue;
        }

        FoundEntry entry;
        entry.name = de.d_name;

        switch (de.d_type)
        {
        case DT_DIR:
            entry.entry_type = FoundEntry::DIRECTORY;
            break;
        case DT_REG:
            entry.entry_type = FoundEntry::REG_FILE;
            break;
        default:
            entry.entry_type = FoundEntry::UNKNOWN;
            break;
        }

        if (bool(entry.entry_type & FoundEntry::REG_FILE))
        {
            if (FSHelper::filename_has_extension(entry.name, SHARED_LIBRARY_EXTENSION) ||
                FSHelper::filename_has_extension(entry.name, STATIC_LIBRARY_EXTENSION) ||
                FSHelper::filename_has_extension(entry.name, OBJECT_FILE_EXTENSION))
            {
                StringCache::StringID id = _found_files_paths_cache.store_string(path_plus_slash);
                this_p._found_files.emplace_back(FileInfo{entry.name, id});
            }
            continue;
        }

        if (const auto &conf_prunenames = this_p._conf->get_prune_names();
        std::find(conf_prunenames.begin(), conf_prunenames.end(), entry.name) != conf_prunenames.end())
        {
            if (this_p._conf->get_debug_pruning())
            {
                fprintf(stderr, "Skipping `%s': in prunenames\n", entry.name.c_str());
            }

            continue;
        }

        entry.dir = std::make_shared<DirWrapper>();
        entry.dir->open(entry.name, fd);
        if (!entry.dir->is_open())
        {
            if (this_p._conf->get_debug_pruning())
            {
                fprintf(stderr,
                        "Failed opening \"%s%s\": %s\n",
                        path_plus_slash.c_str(),
                        entry.name.c_str(),
                        std::strerror(entry.dir->get_errno()));
            }

            continue;
        }

        expected<DirWrapper::DirStatPtr, DirWrapper::Errno_t> stat_res = dir->get_stat();
        if (!stat_res.has_value())
        {
            if (FSHelper::filesystem_is_excluded(this_p._conf->get_prune_fs(), path_plus_slash + entry.name))
            {
                continue;
            }

            fprintf(stderr,
                    "Could not get stat for \"%s\": %s\n",
                    path_plus_slash.c_str(),
                    std::strerror(stat_res.error()));
            exit(EXIT_FAILURE);
        }
        DirWrapper::DirStatPtr stat = stat_res.value();

        expected<DirWrapper::DirStat, DirWrapper::Errno_t> parent_stat_res = dir->get_parent_stat();
        if (!parent_stat_res.has_value())
        {
            fprintf(stderr,
                    "Could not parent stat for \"%s\": %s\n",
                    path_plus_slash.c_str(),
                    std::strerror(stat_res.error()));
            exit(EXIT_FAILURE);
        }
        DirWrapper::DirStat parent_stat = parent_stat_res.value();

        if ((*stat).st_dev != parent_stat.st_dev)
        {
            if (FSHelper::filesystem_is_excluded(this_p._conf->get_prune_fs(), path_plus_slash + entry.name))
            {
                continue;
            }
        }

        // _corpus->add_file(path_plus_slash + entry.name, entry.dt);
        // _dict_builder->add_file(path_plus_slash + entry.name, entry.dt);

        if (bool(entry.entry_type & FoundEntry::DIRECTORY) && fd != -1)
        {
            auto [stat, message] = scan_fs(this_p, entry.dir);
            if (!stat)
            {
                // TODO: The unscanned file descriptors will leak, but it doesn't really matter,
                // as we're about to exit.
                return {false, std::format("Failed to scan: {}", entry.name)};
            }
        }
    }

    return {true, ""};
}

const FSScanner::FileList &FSScanner::get_found_files() const noexcept
{
    return _found_files;
}

std::string FSScanner::get_file_info_full_path(const FileInfo &file_info) noexcept
{
    return std::string(FSScanner::_found_files_paths_cache.get_string(file_info.path_id) + file_info.name);
}

} // namespace SymFind

// NOLINTEND(cppcoreguidelines-pro-type-vararg,readability-identifier-length,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
