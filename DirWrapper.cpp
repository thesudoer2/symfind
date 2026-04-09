#include "DirWrapper.h"

#include <cerrno>

#include <fcntl.h>
#include <unistd.h>

namespace SymFind
{

DirWrapper::DirWrapper() noexcept = default;
DirWrapper::~DirWrapper() noexcept = default;

DirWrapper::DirWrapper(const std::string &dir_path) noexcept
    : DirWrapper(dir_path, false)
{
}

DirWrapper::DirWrapper(const std::string &dir_path, bool noatime) noexcept
{
    _dp = open_impl(dir_path, noatime);
    _is_open = _dp != nullptr; // NOLINT(cppcoreguidelines-prefer-member-initializer)
    _last_errno = _is_open ? 0 : errno;
    if (_is_open)
    {
        _dir_path = dir_path;
    }
}

DirWrapper::DirPtr DirWrapper::open_impl(const std::string &dir_path, bool noatime) noexcept
{
    if (!noatime)
    {
        return DirPtr(::opendir(dir_path.c_str()));
    }

#if defined(__linux__) && defined(O_NOATIME) && defined(HAVE_FDOPENDIR) // or just always try on Linux
    int fd = ::open(dir_path.c_str(), O_RDONLY | O_DIRECTORY | O_NOATIME | O_CLOEXEC);
    if (fd == -1)
    {
        // Fallback: O_NOATIME may fail for non-owners without CAP_FOWNER
        fd = ::open(dir_path.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    }

    if (fd != -1)
    {
        DIR* dp = ::fdopendir(fd);
        if (dp == nullptr)
            ::close(fd);   // fdopendir failed → close the fd ourselves
        return DirPtr(dp);
    }
#endif

    // Fallback to normal opendir on non-Linux or if everything failed
    return DirPtr(::opendir(dir_path.c_str()));
}

bool DirWrapper::open(const std::string &dir_path) noexcept
{
    return open_noatime(dir_path);   // default to normal open
}

bool DirWrapper::open_noatime(const std::string &dir_path) noexcept
{
    _dp = open_impl(dir_path, true);
    _is_open = _dp != nullptr;
    _last_errno = _is_open ? 0 : errno;

    if (_is_open)
    {
        _dir_path = dir_path;
    }
    else
    {
        _dir_path.clear();
    }

    return _is_open;
}

void DirWrapper::close() noexcept
{
    _dp.reset();
    _is_open = false;
    _dir_path.clear();
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
    return _dir_path;
}


struct dirent* DirWrapper::read() noexcept
{
    if (!_is_open)
    {
        return nullptr;
    }

    errno = 0;
    struct dirent* entry = ::readdir(_dp.get());
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

    int fd = ::dirfd(_dp.get()); // NOLINT(readability-identifier-length)
    return (fd == -1) ? unexpected<Errno_t>(errno) : expected<std::int32_t, Errno_t>(fd);
}

// -----------------------------------------------------------------------------
// Iterator implementation
// -----------------------------------------------------------------------------
DirWrapper::iterator::iterator(DirWrapper* dir) noexcept
    : _dir(dir)
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

DirWrapper::iterator& DirWrapper::iterator::operator++() noexcept
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

bool DirWrapper::iterator::operator==(const iterator& other) const noexcept
{
    return _entry == other._entry;
}

bool DirWrapper::iterator::operator!=(const iterator& other) const noexcept
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
