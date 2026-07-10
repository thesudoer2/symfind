#pragma once

#include <cinttypes>
#include <dirent.h>
#include <fcntl.h>
#include <sys/types.h>

#include <iterator>
#include <memory>
#include <string>

#include <symfind/utils/Expected.h>
#include <symfind/utils/Global.h>

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

    using DirStat = struct stat;
    using DirStatPtr = std::shared_ptr<DirStat>;

    using Dev_t = dev_t;

    struct DirTime
    {
        std::int64_t sec;
        std::int32_t nsec;

        bool operator<(const DirTime &other) const
        {
            if (sec != other.sec)
            {
                return sec < other.sec;
            }

            return nsec < other.nsec;
        }

        bool operator>=(const DirTime &other) const
        {
            return !(other < *this);
        }
    };

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
    DirWrapper(DirWrapper &&) noexcept = default;

    DirWrapper &operator=(const DirWrapper &) noexcept = delete;
    DirWrapper &operator=(DirWrapper &&) noexcept = default;

    explicit DirWrapper(const std::string &dir_path, int hint_fd = AT_FDCWD) noexcept;

    bool operator!() const noexcept;
    bool operator!=(bool com_val) const noexcept;

    explicit operator bool() const noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    __nodiscard static DirTime get_dirtime_from_stat(const DirStat& stat) noexcept;
    __nodiscard static Dev_t get_dev_from_stat(const DirStat& stat) noexcept;

    const DIR *get_cdp() const noexcept;
    DIR *get_dp() noexcept;

    bool open(const std::string &dir_path, int hint_fd = AT_FDCWD) noexcept;
    bool open_noatime(const std::string &dir_path, int hint_fd = AT_FDCWD) noexcept;
    void close() noexcept;

    __nodiscard bool is_open() const noexcept;

    __nodiscard Errno_t get_errno() const noexcept;

    __nodiscard std::string get_dir_path() const noexcept;

    __nodiscard expected<DirStatPtr, Errno_t> get_stat() noexcept;

    __nodiscard expected<DirStat, Errno_t> get_parent_stat() noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    struct dirent *read() noexcept;
    void rewind() noexcept;

    expected<std::int32_t, Errno_t> get_fd() const noexcept;

    iterator begin() noexcept;
    iterator end() noexcept;

private:
    static int open_impl(const std::string &dir_path, int hint_fd) noexcept;

    void clear() noexcept;

private: // NOLINT(readability-redundant-access-specifiers)
    DirPtr _dp{nullptr};
    DirStatPtr _stat{nullptr};
    mutable Errno_t _last_errno = -1;
    bool _is_open = false;
};

static constexpr DirWrapper::DirTime not_a_dir{-1, 0};
static constexpr DirWrapper::DirTime unknown_dir_time{0, 0};

} // namespace SymFind
