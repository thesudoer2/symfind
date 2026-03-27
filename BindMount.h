#pragma once

#include <atomic>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "Config.h"
#include "FileWrapper.h"
#include "Singleton.h"

namespace FindSymbol
{

using DeviceMajor = std::uint32_t;
using DeviceMinor = std::uint32_t;

struct Mount
{
    int id = -1;
    int parent_id = -1;
    DeviceMajor dev_major{0};
    DeviceMinor dev_minor{0};
    std::string root;
    std::string mount_point;
    std::string fs_type;
    std::string source;

    // Derived properties.
    bool pruned_due_to_fs_type{false};
    bool pruned_due_to_path{false};
    bool to_remove{false};
};

class BindMount : public Singleton<BindMount>
{
    friend Singleton<BindMount>;

public:
    constexpr static const char *MOUNTINFO_PATH = "/proc/self/mountinfo";

private:
    using MountEntries = std::multimap<std::pair<DeviceMajor, DeviceMinor>, Mount>;

private: // NOLINT(readability-redundant-access-specifiers)
    explicit BindMount(ConfigParser conf) noexcept;

private: // NOLINT(readability-redundant-access-specifiers)
    void init_bind_mount() noexcept;

    void rebuild_bind_mount_paths() noexcept;

    bool read_mount_entry(FileWrapper& file, Mount& mount_entry) noexcept;

    std::string read_mount_line(FileWrapper& file) noexcept;

    std::optional<BindMount::MountEntries> read_mount_entries() noexcept;

private: // NOLINT(readability-redundant-access-specifiers)
    /* mountinfo_path file descriptor, or -1 */
    int _mountinfo_fd = -1;

    /* mountinfo update state */
    std::atomic_bool mountinfo_updated = false;

    /* Next bind_mount_paths entry */
    std::size_t bind_mount_paths_index = 0;

    /* Known bind mount paths */
    std::vector<std::string> _bind_mount_paths;

    /* Configuration */
    ConfigParser _conf;
};

} // namespace FindSymbol
