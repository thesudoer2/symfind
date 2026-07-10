#include <symfind/filesystem/BindMount.h>

#include <cctype>

#include <optional>
#include <string>
#include <thread>

#include <limits.h>
#include <fcntl.h>
#include <poll.h>

#include <symfind/config/Config.h>
#include <symfind/filesystem/FSUtils.h>
#include <symfind/filesystem/FileWrapper.h>

namespace SymFind
{

BindMount::BindMount(ConfigParserPtr conf) noexcept : _conf(std::move(conf))
{
    init_bind_mount();
}

void BindMount::init_bind_mount() noexcept
{
    _mountinfo_fd = open(MOUNTINFO_PATH, O_RDONLY); // NOLINT(cppcoreguidelines-pro-type-vararg)
    if (_mountinfo_fd == -1)
    {
        return;
    }

    rebuild_bind_mount_paths();

    std::thread poll_thread([&] {
        while (true)
        {
            struct pollfd pfd{};

            /* Unfortunately (mount --bind $path $path/subdir) would leave st_dev
			   unchanged between $path and $path/subdir, so we must keep reparsing
			   mountinfo_path each time it changes. */
            pfd.fd = _mountinfo_fd;
            pfd.events = POLLPRI;
            if (poll(&pfd, 1, /*timeout=*/-1) == -1)
            {
                perror("poll()");
                exit(1);
            }
            if ((pfd.revents & POLLPRI) != 0)
            {
                _mountinfo_updated = true;
            }
        }
    });

    poll_thread.detach();
}

bool BindMount::is_bind_mount(const std::string &path) noexcept
{
    if (_mountinfo_updated.exchange(false))
    {
        rebuild_bind_mount_paths();
        _bind_mount_paths_index = 0;
    }

    return FSHelper::string_list_contains_dir_path(_bind_mount_paths, _bind_mount_paths_index, path);
}

// NOLINTBEGIN
void BindMount::rebuild_bind_mount_paths() noexcept
{
    if (_conf->get_debug_pruning())
    {
        fprintf(stderr, "Rebuilding bind_mount_paths:\n");
    }

    std::optional<MountEntries> mount_entries = read_mount_entries();
    if (!mount_entries.has_value())
    {
        return;
    }

    if (_conf->get_debug_pruning())
    {
        std::fprintf(stderr, "Matching bind_mount_paths:\n");
    }

    _bind_mount_paths.clear();

    for (const auto &[dev_id, mount_entry] : *mount_entries)
    {
        const auto &[first, second] =
            mount_entries->equal_range(std::make_pair(mount_entry.dev_major, mount_entry.dev_minor));
        for (auto it = first; it != second; ++it)
        {
            const Mount &other = it->second;
            if (other.id == mount_entry.id)
            {
                // Don't compare an element to itself.
                continue;
            }
            // We have two mounts from the same device. Is one a prefix of the other?
            // If there are two that are equal, prefer the one with lowest ID.
            if (mount_entry.root.size() > other.root.size() && mount_entry.root.find(other.root) == 0)
            {
                if (_conf->get_debug_pruning())
                {
                    std::fprintf(stderr,
                                 " => adding `%s' (root `%s' is a child of `%s', mounted on `%s')\n",
                                 mount_entry.mount_point.c_str(),
                                 mount_entry.root.c_str(),
                                 other.root.c_str(),
                                 other.mount_point.c_str());
                }
                _bind_mount_paths.push_back(mount_entry.mount_point);
                break;
            }

            if (mount_entry.root == other.root && mount_entry.id > other.id)
            {
                if (_conf->get_debug_pruning())
                {
                    std::fprintf(stderr,
                                 " => adding `%s' (duplicate of mount point `%s')\n",
                                 mount_entry.mount_point.c_str(),
                                 other.mount_point.c_str());
                }
                _bind_mount_paths.push_back(mount_entry.mount_point);
                break;
            }
        }
    }

    if (_conf->get_debug_pruning())
    {
        std::fprintf(stderr, "...done\n");
    }

    FSHelper::string_list_dir_path_sort(_bind_mount_paths);
}

bool find_whether_under_pruned(int id,
                               const std::unordered_map<int, Mount *> &id_to_mount,
                               std::unordered_map<int, bool> *id_to_pruned_cache)
{
    auto cache_it = id_to_pruned_cache->find(id);
    if (cache_it != id_to_pruned_cache->end())
    {
        return cache_it->second;
    }

    auto mount_it = id_to_mount.find(id);
    if (mount_it == id_to_mount.end())
    {
        // Should not happen.
        return false;
    }

    bool result = mount_it->second->pruned_due_to_fs_type || mount_it->second->pruned_due_to_path ||
                  (mount_it->second->parent_id != 0 &&
                   find_whether_under_pruned(mount_it->second->parent_id, id_to_mount, id_to_pruned_cache));
    id_to_pruned_cache->emplace(id, result);
    return result;
}

std::optional<BindMount::MountEntries> BindMount::read_mount_entries() noexcept
{
    FileWrapper file(MOUNTINFO_PATH, "r");
    if (file)
    {
        return {};
    }

    MountEntries mount_entries;

    {
        Mount mount_entry;
        while (read_mount_entry(file, mount_entry))
        {
            std::string fs_type_upper = mount_entry.fs_type;
            for (char &c : fs_type_upper)
            {
                c = std::toupper(c);
            }
            mount_entry.pruned_due_to_fs_type =
                (std::find(_conf->get_prune_fs().begin(), _conf->get_prune_fs().end(), fs_type_upper) !=
                 _conf->get_prune_fs().end());
            size_t prunepath_index = 0; // Search the entire list every time.
            mount_entry.pruned_due_to_path = FSHelper::string_list_contains_dir_path(_conf->get_prune_paths(),
                                                                                     prunepath_index,
                                                                                     mount_entry.mount_point.c_str());
            mount_entries.emplace(std::make_pair(mount_entry.dev_major, mount_entry.dev_minor), mount_entry);
            if (_conf->get_debug_pruning())
            {
                std::fprintf(stderr,
                             " `%s' (%d on %d) is `%s' of `%s' (%u:%u), type `%s' (pruned_fs=%d, pruned_path=%d)\n",
                             mount_entry.mount_point.c_str(),
                             mount_entry.id,
                             mount_entry.parent_id,
                             mount_entry.root.c_str(),
                             mount_entry.source.c_str(),
                             mount_entry.dev_major,
                             mount_entry.dev_minor,
                             mount_entry.fs_type.c_str(),
                             mount_entry.pruned_due_to_fs_type,
                             mount_entry.pruned_due_to_path);
            }
        }
    }

    // Now propagate pruned status recursively through parent links
    // (e.g. if /run is tmpfs, then /run/foo should also be pruned).
    std::unordered_map<int, Mount *> id_to_mount;
    for (auto &[key, me] : mount_entries)
    {
        id_to_mount[me.id] = &me;
    }
    std::unordered_map<int, bool> id_to_pruned_cache;
    for (auto &[key, me] : mount_entries)
    {
        me.to_remove = find_whether_under_pruned(me.id, id_to_mount, &id_to_pruned_cache);
        if (_conf->get_debug_pruning() && me.to_remove)
        {
            std::fprintf(stderr, " `%s' is, or is under, a pruned file system; removing\n", me.mount_point.c_str());
        }
    }

    // Now take out those that we won't see due to file system type anyway,
    // so that we don't inadvertently prefer them to others during bind mount
    // duplicate detection.
    for (auto it = mount_entries.begin(); it != mount_entries.end();)
    {
        if (it->second.to_remove)
        {
            it = mount_entries.erase(it);
        }
        else
        {
            ++it;
        }
    }

    return mount_entries;
}

int parse_mount_string(std::string *dest, const char **str)
{
    const char *src = *str;
    while (*src == ' ' || *src == '\t')
    {
        src++;
    }
    if (*src == 0)
    {
        return -1;
    }

    std::string mount_string;
    while (true)
    {
        char c = *src;

        switch (c)
        {
        case 0:
        {
            [[fallthrough]];
        }
        case ' ':
        {
            [[fallthrough]];
        }
        case '\t':
        {
            goto done;
        }
        case '\\':
        {
            if (src[1] >= '0' && src[1] <= '7' && src[2] >= '0' && src[2] <= '7' && src[3] >= '0' && src[3] <= '7')
            {
                unsigned v;

                v = ((src[1] - '0') << 6) | ((src[2] - '0') << 3) | (src[3] - '0');
                if (v <= UCHAR_MAX)
                {
                    mount_string.push_back(v);
                    src += 4;
                    break;
                }
            }

            // else
            [[fallthrough]];
        }
        default:
        {
            mount_string.push_back(c);
            src++;
        }
        }
    }

done:
    *str = src;
    if (dest != nullptr)
    {
        *dest = std::move(mount_string);
    }
    return 0;
}

bool BindMount::read_mount_entry(FileWrapper &file, Mount &mount_entry) noexcept
{
    std::string line = read_mount_line(file);
    if (line.empty())
    {
        return false;
    }
    size_t offset{};

    if (std::sscanf(line.c_str(),
                    "%d %d %u:%u%zn",
                    &mount_entry.id,
                    &mount_entry.parent_id,
                    &mount_entry.dev_major,
                    &mount_entry.dev_minor,
                    &offset) != 4)
    {
        return false;
    }

    const char *ptr = line.c_str() + offset;
    if (parse_mount_string(&mount_entry.root, &ptr) != 0 || parse_mount_string(&mount_entry.mount_point, &ptr) != 0 ||
        parse_mount_string(nullptr, &ptr) != 0)
    {
        return false;
    }
    bool separator_found = false;
    do
    {
        std::string option;
        if (parse_mount_string(&option, &ptr) != 0)
        {
            return false;
        }
        separator_found = strcmp(option.c_str(), "-") == 0;
    } while (!separator_found);

    if (parse_mount_string(&mount_entry.fs_type, &ptr) != 0 || parse_mount_string(&mount_entry.source, &ptr) != 0 ||
        parse_mount_string(nullptr, &ptr) != 0)
    {
        return false;
    }

    return true;
}

std::string BindMount::read_mount_line(FileWrapper &file) noexcept
{
    std::string line;

    while (true)
    {
        char buf[LINE_MAX]{'\0'};

        if (std::fgets(buf, sizeof(buf), file.get_fp()) == nullptr)
        {
            if (std::feof(file.get_fp()))
            {
                break;
            }

            return "";
        }

        std::size_t chunk_length = std::strlen(buf);
        if (chunk_length > 0 && buf[chunk_length - 1] == '\n')
        {
            line.append(buf, chunk_length - 1);
            break;
        }

        line.append(buf, chunk_length);
    }

    return line;
}
// NOLINTEND

} // namespace SymFind
