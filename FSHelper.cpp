#include "FSHelper.h"

#include <algorithm>
#include <string>
#include <vector>

#include <arpa/inet.h>

#include <unistd.h>
#include <mntent.h>

namespace SymFind::FSHelper
{

// NOLINTBEGIN(readability-identifier-length,cppcoreguidelines-pro-type-vararg)
int dir_path_cmp(const std::string &a, const std::string &b)
{
    auto [ai, bi] = mismatch(a.begin(), a.end(), b.begin(), b.end());
    if (ai == a.end() && bi == b.end())
    {
        return 0;
    }
    if (ai == a.end())
    {
        return -1;
    }
    if (bi == b.end())
    {
        return 1;
    }
    if (*ai == *bi)
    {
        return 0;
    }
    if (*ai == '/')
    {
        return -1;
    }
    if (*bi == '/')
    {
        return 1;
    }
    return int((unsigned char)*ai) - int((unsigned char)*bi);
}

void string_list_dir_path_sort(std::vector<std::string> &list)
{
    std::sort(list.begin(), list.end(), [](const std::string &a, const std::string &b) {
        return dir_path_cmp(a, b) < 0;
    });
}

bool string_list_contains_dir_path(const std::vector<std::string> &list, size_t &idx, const std::string &path)
{
    int cmp = 0;
    while (idx < list.size() && (cmp = dir_path_cmp(list[idx], path)) < 0)
    {
        ++idx;
    }
    if (idx < list.size() && cmp == 0)
    {
        ++idx;
        return true;
    }
    return false;
}

bool filesystem_is_excluded(const std::vector<std::string> &list, const std::string &path) noexcept
{
    FILE *f = setmntent("/proc/mounts", "r");
    if (f == nullptr)
    {
        return false;
    }

    struct mntent *mount_ent{};
    while ((mount_ent = getmntent(f)) != nullptr)
    {
        if (path != mount_ent->mnt_dir)
        {
            continue;
        }
        std::string type(mount_ent->mnt_type);
        for (char &p : type)
        {
            p = (char)toupper(p);
        }
        bool exclude = (std::find(list.begin(), list.end(), type) != list.end());
        endmntent(f);
        return exclude;
    }
    endmntent(f);
    return false;
}
// NOLINTEND(readability-identifier-length,cppcoreguidelines-pro-type-vararg)

} // namespace SymFind::FSHelper
