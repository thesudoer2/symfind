#include <symfind/filesystem/FSScanner.h>

#include <cstddef>
#include <cstring>

#include <format>
#include <memory>

#include <dirent.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/resource.h>

#include <symfind/filesystem/BindMount.h>
#include <symfind/config/Config.h>
#include <symfind/core/DictionaryBuilder.h>
#include <symfind/utils/Expected.h>
#include <symfind/filesystem/FSUtils.h>
#include <symfind/filesystem/DirWrapper.h>

#define FOUND_FILES_RESERVED_SIZE 40'000

#define MAX_FILE_NAME_SAMPLES 100'000
#define MAX_PATH_NAME_SAMPLES 100'000

#define SHARED_LIBRARY_EXTENSION ".so"
#define STATIC_LIBRARY_EXTENSION ".a"
#define OBJECT_FILE_EXTENSION ".o"
namespace
{

const std::vector<std::string> tracking_extensions{
    SHARED_LIBRARY_EXTENSION,
    OBJECT_FILE_EXTENSION,
    STATIC_LIBRARY_EXTENSION,
};

} // namespace


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
    enum EntryType : std::uint16_t
    {
        UNKNOWN = 0x001,
        DIRECTORY = 0x002,
        REG_FILE = 0x004,
        FIFO = 0x008,
        CHARACTER_DEV = 0x010,
        BLOCK_DEV = 0x020,
        LINK = 0x040,
        SOCKET = 0x080,
        WHT = 0x100,
    };

    std::string name;
    EntryType entry_type = EntryType::UNKNOWN;

    // For directories only:
    std::shared_ptr<DirWrapper> dir{nullptr};
};

FSScanner::StringCache FSScanner::_found_files_paths_cache{};

FSScanner::FSScanner(ConfigParserPtr conf,
                     BindMount::InstancePtr bind_mount,
                     DictionaryBuilderPtr dict_builder_ptr) noexcept
    : _conf(std::move(conf)), _bind_mount(std::move(bind_mount)), _dict_builder_ptr(std::move(dict_builder_ptr))
{
    _found_files.reserve(FOUND_FILES_RESERVED_SIZE);
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
        case DT_FIFO:
            entry.entry_type = FoundEntry::FIFO;
            break;
        case DT_CHR:
            entry.entry_type = FoundEntry::CHARACTER_DEV;
            break;
        case DT_BLK:
            entry.entry_type = FoundEntry::BLOCK_DEV;
            break;
        case DT_LNK:
            entry.entry_type = FoundEntry::LINK;
            break;
        case DT_SOCK:
            entry.entry_type = FoundEntry::SOCKET;
            break;
        case DT_WHT:
            entry.entry_type = FoundEntry::WHT;
            break;
        default:
            entry.entry_type = FoundEntry::UNKNOWN;
            break;
        }

        if ((bool)(entry.entry_type & FoundEntry::REG_FILE))
        {
            if (FSHelper::filename_has_any_of_extensions(entry.name, tracking_extensions))
            {
                // Add file name sample to dictionary if exists.
                if (this_p._dict_builder_ptr && ++this_p.file_name_samples <= MAX_FILE_NAME_SAMPLES)
                {
                    this_p._dict_builder_ptr->add_sample(entry.name);
                }

                StringCache::StringID id = _found_files_paths_cache.store_string(path_plus_slash);
                this_p._found_files.emplace_back(entry.name, id);
            }
            continue;
        }
        else if (!(bool)(entry.entry_type & FoundEntry::UNKNOWN) && !(bool)(entry.entry_type & FoundEntry::DIRECTORY))
        {
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

        expected<int, DirWrapper::Errno_t> fd_res = dir->get_fd();
        if (!fd_res.has_value()) [[unlikely]]
        {
            return {false, ""};
        }
        int fd = fd_res.value();

        entry.dir = std::make_shared<DirWrapper>();
        entry.dir->open(entry.name, fd);
        if (!entry.dir->is_open())
        {
            if (this_p._conf->get_debug_pruning())
            {
                fprintf(stderr,
                        "Failed opening \"%s%s\": %s (entry type: %d)\n",
                        path_plus_slash.c_str(),
                        entry.name.c_str(),
                        std::strerror(entry.dir->get_errno()),
                        entry.entry_type);
            }

            continue;
        }

        const std::string file_full_name = path_plus_slash + entry.name;

        expected<DirWrapper::DirStatPtr, DirWrapper::Errno_t> stat_res = entry.dir->get_stat();
        if (!stat_res.has_value())
        {
            if (FSHelper::filesystem_is_excluded(this_p._conf->get_prune_fs(), file_full_name))
            {
                if (this_p._conf->get_debug_pruning())
                {
                    fprintf(stderr,
                            "Skipping `%s' excluded due to filesystem type\n",
                            file_full_name.c_str());
                }
                continue;
            }

            fprintf(stderr,
                    "Could not get stat for \"%s\": %s\n",
                    path_plus_slash.c_str(),
                    std::strerror(stat_res.error()));
            exit(EXIT_FAILURE);
        }
        DirWrapper::DirStatPtr stat = stat_res.value();

        expected<DirWrapper::DirStatPtr, DirWrapper::Errno_t> parent_stat_res = dir->get_stat();
        if (!parent_stat_res.has_value())
        {
            fprintf(stderr,
                    "Could not parent stat for \"%s\": %s\n",
                    path_plus_slash.c_str(),
                    std::strerror(stat_res.error()));
            exit(EXIT_FAILURE);
        }
        DirWrapper::DirStatPtr parent_stat = parent_stat_res.value();

        // Run if mount point of parent and child directories are not the same.
        if ((*stat).st_dev != (*parent_stat).st_dev)
        {
            if (FSHelper::filesystem_is_excluded(this_p._conf->get_prune_fs(), file_full_name))
            {
                if (this_p._conf->get_debug_pruning())
                {
                    fprintf(stderr,
                            "Skipping `%s' excluded due to filesystem type\n",
                            file_full_name.c_str());
                }
                continue;
            }
        }

        if ((bool)(entry.entry_type & FoundEntry::DIRECTORY) && fd != -1)
        {
            // Add path name sample to dictionary if exists.
            if (this_p._dict_builder_ptr && ++this_p.path_name_samples <= MAX_PATH_NAME_SAMPLES)
            {
                this_p._dict_builder_ptr->add_sample(entry.name);
            }

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
