#include "Database.h"

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
#include "ExistingDB.h"
#include "Expected.h"
#include "FSHelper.h"
#include "DirWrapper.h"

// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg,readability-identifier-length,cppcoreguidelines-pro-bounds-array-to-pointer-decay)

namespace SymFind
{

struct FoundEntry
{
    std::string name;
    bool is_directory = false;

    // For directories only:
    DirWrapper dir;
};

Database::Database(std::shared_ptr<ConfigParser> conf) noexcept
    : _conf(std::move(conf)), _existing_db(std::make_unique<ExistingDB>(_conf))
{
    SymFind::BindMount::init(_conf);
    _bind_mount = SymFind::BindMount::getInstancePtr();
}

std::pair<bool, std::string> Database::scan() noexcept
{
    const std::string &database_scan_path = _conf->get_database_scan_path();
    DirWrapper root_dir(database_scan_path);

    if (!root_dir)
    {
        return {false, std::strerror(root_dir.get_errno())};
    }

    return scan_impl(*this, root_dir);
}

std::pair<bool, std::string> Database::scan_impl(const Database& this_p, DirWrapper& dir) noexcept
{
    const std::string& current_dir_path = dir.get_dir_path();

    expected<int, DirWrapper::Errno_t> fd_res = dir.get_fd();
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
            fprintf(stderr, "Skipping `%s': in %s\n", current_dir_path.c_str(), PRUNE_PATHS_CONFIG);
        }
        return {false, std::format("Database scan path determined in {} list", PRUNE_PATHS_CONFIG)};
    }

    if (this_p._conf->get_prune_bind_mounts() && this_p._bind_mount->is_bind_mount(current_dir_path))
    {
        if (this_p._conf->get_debug_pruning())
        {
            fprintf(stderr, "Skipping `%s': %s\n", current_dir_path.c_str(), PRUNE_BIND_MOUNTS_CONFIG);
        }
        return {false, std::format("Database scan path determined in {} list", PRUNE_BIND_MOUNTS_CONFIG)};
    }

    const std::string path_plus_slash = current_dir_path.back() == '/' ? current_dir_path : current_dir_path + '/';

    std::vector<FoundEntry> entries;

    DIR *dp = dir.get_dp();
    if (dp == nullptr)
    {
        // fdopendir() wants to fstat() the fd to verify that it's indeed
        // a directory, which can seemingly fail on at least CIFS filesystems
        // if the server feels like it. We treat this as if we had an error
        // on opening it, ie., ignore the directory.
        return {true, ""};
    }

    for (const struct dirent& de : dir)
    {
        if (strcmp(de.d_name, ".") == 0 || strcmp(de.d_name, "..") == 0)
        {
            continue;
        }

        if (strlen(de.d_name) == 0)
        {
            /* Unfortunately, this does happen, and mere assert() does not give
                users enough information to complain to the right people. */
            fprintf(stderr, "file system error: zero-length file name in directory %s", current_dir_path.c_str());
            continue;
        }

        FoundEntry entry;
        entry.name = de.d_name;
        if (de.d_type == DT_UNKNOWN)
        {
            // Evidently some file systems, like older versions of XFS
            // (mkfs.xfs -m crc=0 -n ftype=0), can return this,
            // and we need a stat(). If we wanted to optimize for this,
            // we could probably defer it to later (we're stat-ing directories
            // when recursing), but this is rare, and not really worth it --
            // the second stat() will be cached anyway.
            struct stat buf{};
            entry.is_directory = ::fstatat(fd, de.d_name, &buf, AT_SYMLINK_NOFOLLOW) == 0 && S_ISDIR(buf.st_mode);
        }
        else
        {
            entry.is_directory = (de.d_type == DT_DIR);
        }

        if (!entry.is_directory)
        {
            entries.push_back(std::move(entry));
            continue;
        }

        if (const auto &conf_prunenames = this_p._conf->get_prune_names();
        std::find(conf_prunenames.begin(), conf_prunenames.end(), entry.name) != conf_prunenames.end())
        {
            if (this_p._conf->get_debug_pruning())
            {
                fprintf(stderr, "Skipping `%s': in prunenames\n", entry.name.c_str());
            }

            entries.push_back(std::move(entry));
            continue;
        }

        entry.dir.open(entry.name);
        if (!entry.dir)
        {
            entries.push_back(std::move(entry));
            continue;
        }

        expected<DirWrapper::DirStatPtr, DirWrapper::Errno_t> stat_res = dir.get_stat();
        if (!stat_res.has_value())
        {
            if (FSHelper::filesystem_is_excluded(this_p._conf->get_prune_fs(), path_plus_slash + entry.name))
            {
                entries.push_back(std::move(entry));
                continue;
            }

            fprintf(stderr,
                    "Could not get stat for \"%s\": %s\n",
                    dir.get_dir_path().c_str(),
                    std::strerror(stat_res.error()));
            exit(EXIT_FAILURE);
        }
        DirWrapper::DirStatPtr stat = stat_res.value();

        expected<DirWrapper::DirStat, DirWrapper::Errno_t> parent_stat_res = dir.get_parent_stat();
        if (!parent_stat_res.has_value())
        {
            fprintf(stderr,
                    "Could not parent stat for \"%s\": %s\n",
                    dir.get_dir_path().c_str(),
                    std::strerror(stat_res.error()));
            exit(EXIT_FAILURE);
        }
        DirWrapper::DirStat parent_stat = parent_stat_res.value();

        if ((*stat).st_dev != parent_stat.st_dev)
        {
            if (FSHelper::filesystem_is_excluded(this_p._conf->get_prune_fs(), path_plus_slash + entry.name))
            {
                entries.push_back(std::move(entry));
                continue;
            }
        }

        // _corpus->add_file(path_plus_slash + entry.name, entry.dt);
        // _dict_builder->add_file(path_plus_slash + entry.name, entry.dt);

        if (entry.is_directory && fd != -1)
        {
            auto [stat, message] = scan_impl(this_p, entry.dir);
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

} // namespace SymFind

// NOLINTEND(cppcoreguidelines-pro-type-vararg,readability-identifier-length,cppcoreguidelines-pro-bounds-array-to-pointer-decay)
