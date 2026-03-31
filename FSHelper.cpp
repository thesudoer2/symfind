#include "FSHelper.h"

#include <algorithm>
#include <string>
#include <vector>

#include <arpa/inet.h>
#include <unistd.h>

namespace SymFind::FSHelper
{

// NOLINTBEGIN(readability-identifier-length)
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
    std::sort(list.begin(), list.end(), [](const std::string &a, const std::string &b) { return dir_path_cmp(a, b) < 0; });
}

bool string_list_contains_dir_path(const std::vector<std::string> *list, size_t *idx, const std::string &path)
{
    int cmp = 0;
    while (*idx < list->size() && (cmp = dir_path_cmp((*list)[*idx], path)) < 0)
    {
        (*idx)++;
    }
    if (*idx < list->size() && cmp == 0)
    {
        (*idx)++;
        return true;
    }
    return false;
}
// NOLINTEND(readability-identifier-length)

} // namespace SymFind
