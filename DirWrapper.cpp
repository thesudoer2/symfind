#include "DirWrapper.h"

#include <cerrno>

#include <fcntl.h>
#include <unistd.h>

#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/time.h>

#include "Expected.h"

#define MAX_PATH_LEN 1024
#define PROC_FD_PATH "/proc/self/fd/"

#define TIME_USEC_TO_NSEC(t) (t * 1000)


// NOLINTBEGIN(cppcoreguidelines-pro-type-vararg,readability-identifier-length)

namespace SymFind
{

DIR *get_dp_from_fd(int fd) noexcept
{
    if (fd != -1)
    {
        DIR *dp = ::fdopendir(fd);
        if (dp == nullptr)
        {
            ::close(fd);
        }
        return dp;
    }
    return nullptr;
}

DirWrapper::DirWrapper() noexcept = default;
DirWrapper::~DirWrapper() noexcept = default;

DirWrapper::DirWrapper(const std::string &dir_path) noexcept
{
    int fd = open_impl(dir_path);

    _is_open = fd != -1;
    _last_errno = _is_open ? 0 : errno;
    _dp.reset(get_dp_from_fd(fd));

    _is_open = _dp != nullptr; // NOLINT(cppcoreguidelines-prefer-member-initializer)
    _last_errno = _is_open ? 0 : errno;
}

int DirWrapper::open_impl(const std::string &dir_path) noexcept
{
    static bool noatime_failed = false;

    if (noatime_failed)
    {
        return ::openat(AT_FDCWD, dir_path.c_str(), O_RDONLY | O_DIRECTORY);
    }

#if defined(__linux__) && defined(O_NOATIME) && defined(HAVE_FDOPENDIR)
    int fd = ::openat(AT_FDCWD, dir_path.c_str(), O_RDONLY | O_DIRECTORY | O_NOATIME | O_CLOEXEC);
    if (fd == -1)
    {
        noatime_failed = true;

        // Fallback: O_NOATIME may fail for non-owners without CAP_FOWNER
        fd = ::openat(AT_FDCWD, dir_path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    }

    if (fd == -1)
    {
        if (errno == EMFILE || errno == ENFILE)
        {
            // The admin probably wants to know about this.
            perror(dir_path.c_str());

            rlimit rlim{};
            if (getrlimit(RLIMIT_NOFILE, &rlim) == -1)
            {
                fprintf(stderr, "Hint: Try `ulimit -n 131072' or similar.\n");
            }
            else
            {
                fprintf(stderr,
                        "Hint: Try `ulimit -n %" PRIu64 " or similar (current limit is %" PRIu64 ").\n",
                        static_cast<uint64_t>(rlim.rlim_cur * 2),
                        static_cast<uint64_t>(rlim.rlim_cur));
            }
            exit(EXIT_FAILURE);
        }
    }

    return fd;
#endif

    // Fallback to normal opendir on non-Linux or if everything failed
    return ::openat(AT_FDCWD, dir_path.c_str(), O_RDONLY | O_DIRECTORY);
}

void DirWrapper::clear() noexcept
{
    _dp.reset();
    _stat.reset();
    _last_errno = -1;
    _is_open = false;
}

bool DirWrapper::open(const std::string &dir_path) noexcept
{
    clear();
    return open_noatime(dir_path); // default to normal open
}

bool DirWrapper::open_noatime(const std::string &dir_path) noexcept
{
    int fd = open_impl(dir_path);

    _is_open = fd != -1;
    _last_errno = _is_open ? 0 : errno;
    _dp.reset(get_dp_from_fd(fd));

    return _is_open;
}

void DirWrapper::close() noexcept
{
    clear();
}

bool DirWrapper::operator!() const noexcept
{
    return !_is_open;
}

bool DirWrapper::operator!=(bool com_val) const noexcept
{
    return _is_open != com_val;
}

DirWrapper::operator bool() const noexcept
{
    return _is_open;
}

const DIR *DirWrapper::get_cdp() const noexcept
{
    return _dp.get();
}

DIR *DirWrapper::get_dp() noexcept
{
    return _dp.get();
}

bool DirWrapper::is_open() const noexcept
{
    return _is_open;
}

DirWrapper::Errno_t DirWrapper::get_errno() const noexcept
{
    return _last_errno;
}

std::string DirWrapper::get_dir_path() const noexcept
{
    expected<std::int32_t, DirWrapper::Errno_t> fdex = get_fd();
    if (!fdex.has_value())
    {
        return {};
    }

    std::string fd_path;
    fd_path.resize(MAX_PATH_LEN);

    std::string file_path;
    file_path.resize(MAX_PATH_LEN);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
    std::snprintf(fd_path.data(), MAX_PATH_LEN, "%s/%d", PROC_FD_PATH, fdex.value());

    ssize_t len = readlink(fd_path.c_str(), file_path.data(), MAX_PATH_LEN - 1);
    if (len == -1)
    {
        return {};
    }

    file_path[len] = '\0';
    return file_path;
}

expected<DirWrapper::DirStatPtr, DirWrapper::Errno_t> DirWrapper::get_stat() noexcept
{
    // Return current stat if exists
    if (_stat)
    {
        return _stat;
    }

    // Get the directory file descriptor
    expected<std::int32_t, Errno_t> fd_res = get_fd();
    if (!fd_res.has_value())
    {
        return unexpected<Errno_t>(fd_res.error());
    }
    int fd = fd_res.value();

    // NOLINTNEXTLINE(cppcoreguidelines-owning-memory, bugprone-unhandled-exception-at-new)
    _stat.reset(new struct stat);

    if (::fstat(fd, _stat.get()) != 0)
    {
        _stat.reset(); // free on failure
        return unexpected<Errno_t>(errno);
    }
    return _stat;
}

expected<DirWrapper::DirStat, DirWrapper::Errno_t> DirWrapper::get_parent_stat() noexcept
{
    // Get the directory file descriptor
    expected<std::int32_t, Errno_t> fd_res = get_fd();
    if (!fd_res.has_value())
    {
        return unexpected<Errno_t>(fd_res.error());
    }
    int fd = fd_res.value();

    struct stat parent_stat{};

    if (::fstatat(fd, "..", &parent_stat, 0) == -1)
    {
        _stat.reset(); // free on failure
        return unexpected<Errno_t>(errno);
    }
    return parent_stat;
}

bool time_is_current(const DirWrapper::DirTime &dt)
{
	static DirWrapper::DirTime cache{ 0, 0 };

	/* This is more difficult than it should be because Linux uses a cheaper time
	   source for filesystem timestamps than for gettimeofday() and they can get
	   slightly out of sync, see
	   https://bugzilla.redhat.com/show_bug.cgi?id=244697 .  This affects even
	   nanosecond timestamps (and don't forget that tv_nsec existence doesn't
	   guarantee that the underlying filesystem has such resolution - it might be
	   microseconds or even coarser).

	   The worst case is probably FAT timestamps with 2-second resolution
	   (although using such a filesystem violates POSIX file times requirements).

	   So, to be on the safe side, require a >3.0 second difference (2 seconds to
	   make sure the FAT timestamp changed, 1 more to account for the Linux
	   timestamp races).  This large margin might make updatedb marginally more
	   expensive, but it only makes a difference if the directory was very
	   recently updated _and_ is will not be updated again until the next
	   updatedb run; this is not likely to happen for most directories. */

	/* Cache gettimeofday () results to rule out obviously old time stamps;
	   CACHE contains the earliest time we reject as too current. */
	if (dt < cache) {
		return false;
	}

	struct timeval tv{};
	gettimeofday(&tv, nullptr);
	cache.sec = tv.tv_sec - 3;
	cache.nsec = TIME_USEC_TO_NSEC(tv.tv_usec);

	return dt >= cache;
}

DirWrapper::DirTime DirWrapper::get_dirtime_from_stat(const DirStat& stat) noexcept
{
#ifndef __APPLE__
    DirTime ctime{stat.st_ctim.tv_sec, int32_t(stat.st_ctim.tv_nsec)};
    DirTime mtime{stat.st_mtim.tv_sec, int32_t(stat.st_mtim.tv_nsec)};
#else
    DirTime ctime{stat.st_ctimespec.tv_sec, int32_t(stat.st_ctimespec.tv_nsec)};
    DirTime mtime{stat.st_mtimespec.tv_sec, int32_t(stat.st_mtimespec.tv_nsec)};
#endif

    DirTime dt = std::max(ctime, mtime);

    if (time_is_current(dt))
    {
        /* The directory might be changing right now and we can't be sure the
		   timestamp will be changed again if more changes happen very soon, mark
		   the timestamp as invalid to force rescanning the directory next time
		   updatedb is run. */
        return unknown_dir_time;
    }
    return dt;
}

DirWrapper::Dev_t DirWrapper::get_dev_from_stat(const DirStat& stat) noexcept
{
    return stat.st_dev;
}

struct dirent *DirWrapper::read() noexcept
{
    if (!_is_open)
    {
        return nullptr;
    }

    errno = 0;
    struct dirent *entry = ::readdir(_dp.get());
    if (entry == nullptr)
    {
        _last_errno = errno;
    }

    return entry;
}

void DirWrapper::rewind() noexcept
{
    if (_is_open)
    {
        ::rewinddir(_dp.get());
    }
}

expected<std::int32_t, DirWrapper::Errno_t> DirWrapper::get_fd() const noexcept
{
    if (!_is_open)
    {
        return unexpected<Errno_t>(EBADF);
    }

    int fd = ::dirfd(_dp.get());
    return (fd == -1) ? unexpected<Errno_t>(errno) : expected<std::int32_t, Errno_t>(fd);
}

// -----------------------------------------------------------------------------
// Iterator implementation
// -----------------------------------------------------------------------------
DirWrapper::iterator::iterator(DirWrapper *dir) noexcept : _dir(dir)
{
    if (_dir != nullptr)
    {
        _entry = _dir->read();
    }
}

DirWrapper::iterator::reference DirWrapper::iterator::operator*() const noexcept
{
    return *_entry;
}

DirWrapper::iterator::pointer DirWrapper::iterator::operator->() const noexcept
{
    return _entry;
}

DirWrapper::iterator &DirWrapper::iterator::operator++() noexcept
{
    if (_dir != nullptr)
    {
        _entry = _dir->read();
    }
    return *this;
}

DirWrapper::iterator DirWrapper::iterator::operator++(int) noexcept
{
    iterator tmp = *this;
    ++(*this);
    return tmp;
}

bool DirWrapper::iterator::operator==(const iterator &other) const noexcept
{
    return _entry == other._entry;
}

bool DirWrapper::iterator::operator!=(const iterator &other) const noexcept
{
    return !(*this == other);
}

DirWrapper::iterator DirWrapper::begin() noexcept
{
    return iterator(this);
}

// NOLINTNEXTLINE(readability-convert-member-functions-to-static)
DirWrapper::iterator DirWrapper::end() noexcept
{
    return iterator(nullptr);
}

} // namespace SymFind

// NOLINTEND(cppcoreguidelines-pro-type-vararg,readability-identifier-length)
