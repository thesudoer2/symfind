#pragma once

#include <string>
#include <vector>

namespace SymFind::FSHelper
{

/**
 * @brief Compare two path names according to the database directory order.
 *
 * This comparison is **not** based on `strcmp()` order. In this order, "a" is less than "a.b",
 * and thus "a/z" is considered less than "a.b".
 *
 * @param path1 The first path name to compare.
 * @param path2 The second path name to compare.
 *
 * @return A negative value if `path1` is less than `path2`,
 *         zero if they are equal, and a positive value if `path1` is greater than `path2`.
 */
extern int dir_path_cmp(const std::string &a, const std::string &b); //NOLINT(readability-identifier-length)

/**
 * @brief Sort `LIST` using the `dir_path_cmp()` comparison function.
 *
 * This function sorts the elements of `LIST` based on the order defined by `dir_path_cmp()`.
 *
 * @param LIST The list of paths to be sorted.
 *
 * @return None (or void), as the list is sorted in place.
 */
extern void string_list_dir_path_sort(std::vector<std::string> &list);

/**
 * @brief Check if `PATH` is present in `LIST` and update `IDX` to position within `LIST`.
 *
 * This function assumes that `LIST` is sorted using `dir_path_cmp()`.
 * Subsequent calls should provide `PATH` values that are increasing according to `dir_path_cmp()`.
 *
 * @param PATH The path to check for inclusion in `LIST`.
 * @param LIST The list of paths to search through.
 * @param IDX The index to update, positioning it within `LIST`.
 *
 * @return Returns `true` if `PATH` is found in `LIST`, otherwise `false`.
 */
extern bool string_list_contains_dir_path(const std::vector<std::string> &list, size_t &idx, const std::string &path);

/**
 * @brief Checks if a filesystem at a given path is excluded based on its type.
 *
 * This function checks if the filesystem type of the specified path is in the provided
 * exclusion list. It returns `true` if excluded, otherwise `false`.
 *
 * @param list A vector of filesystem types to exclude.
 * @param path The path to check.
 *
 * @return `true` if the filesystem is excluded, `false` otherwise.
 *
 * @note Prints a message to `stderr` when a filesystem is excluded.
 */
extern bool filesystem_is_excluded(const std::vector<std::string> &list, const std::string &path) noexcept;

extern bool filename_has_extension(const std::string& filename, const std::string& extension);

} // namespace SymFind::FSHelper
