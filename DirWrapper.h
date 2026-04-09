#pragma once

#include <cinttypes>
#include <dirent.h>
#include <sys/types.h>

#include <iterator>
#include <memory>
#include <string>

#include "Expected.h"
#include "Global.h"

namespace SymFind
{

class DirWrapper final
{
private:
    struct DirCloser
    {
        // NOLINTNEXTLINE(readability-identifier-length)
        void operator()(DIR *dp) const noexcept
        {
            if (dp != nullptr)
            {
                ::closedir(dp); // NOLINT(cppcoreguidelines-owning-memory)
            }
        }
    };

    using DirPtr = std::unique_ptr<DIR, DirCloser>;

public:
    using Errno_t = std::int32_t;

    class iterator
    {
    public:
        using iterator_category = std::input_iterator_tag;
        using value_type = struct dirent;
        using difference_type = std::ptrdiff_t;
        using pointer = struct dirent *;
        using reference = struct dirent &;

        iterator() noexcept = default;
        explicit iterator(DirWrapper *dir) noexcept;

        reference operator*() const noexcept;
        pointer operator->() const noexcept;

        iterator &operator++() noexcept;
        iterator operator++(int) noexcept;

        bool operator==(const iterator &other) const noexcept;
        bool operator!=(const iterator &other) const noexcept;

    private:
        DirWrapper *_dir = nullptr;
        struct dirent *_entry = nullptr;
    };

public: // NOLINT(readability-redundant-access-specifiers)
    DirWrapper() noexcept;
    ~DirWrapper() noexcept;

    DirWrapper(const DirWrapper &) noexcept = delete;
    DirWrapper(DirWrapper &&) noexcept = delete;

    DirWrapper &operator=(const DirWrapper &) noexcept = delete;
    DirWrapper &operator=(DirWrapper &&) noexcept = delete;

    explicit DirWrapper(const std::string &dir_path) noexcept;
    DirWrapper(const std::string &dir_path, bool noatime) noexcept;

    bool operator!() const noexcept;
    bool operator!=(bool com_val) const noexcept;

    explicit operator bool() const noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    const DIR *get_cdp() const noexcept;
    DIR *get_dp() noexcept;

    bool open(const std::string &dir_path) noexcept;
    bool open_noatime(const std::string &dir_path) noexcept;
    void close() noexcept;

    __nodiscard bool is_open() const noexcept;

    __nodiscard Errno_t get_errno() const noexcept;

    __nodiscard std::string get_dir_path() const noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    struct dirent *read() noexcept;
    void rewind() noexcept;

    expected<std::int32_t, Errno_t> get_fd() const noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;

private:
    static DirPtr open_impl(const std::string &dir_path, bool noatime) noexcept;

    DirPtr _dp{nullptr};
    mutable Errno_t _last_errno = -1;
    bool _is_open = false;
    std::string _dir_path;
};

} // namespace SymFind
