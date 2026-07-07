#include "FileWrapper.h"

#include <bits/types/error_t.h>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <string>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Expected.h"

#define MAX_PATH_LEN 1024
#define PROC_FD_PATH "/proc/self/fd/"

namespace SymFind
{

FileWrapper::FileWrapper() noexcept = default;
FileWrapper::~FileWrapper() noexcept = default;

FileWrapper::FileWrapper(const std::string &file_path, const std::string &mode) noexcept
{
    open(file_path, mode);
}

bool FileWrapper::operator!() const noexcept
{
    return !_is_open;
}

bool FileWrapper::operator!=(bool com_val) const noexcept
{
    return _is_open != com_val;
}

FileWrapper::operator bool() const noexcept
{
    return _is_open;
}

expected<std::int32_t, FileWrapper::Errno_t> FileWrapper::get_fd() const noexcept
{
    if (!_is_open)
    {
        return unexpected<Errno_t>(EIO);
    }
    int fd = fileno(_fp.get()); // NOLINT(readability-identifier-length)
    return (fd == -1) ? unexpected<Errno_t>(errno) : expected<std::int32_t, Errno_t>(fd);
}

const FILE *FileWrapper::get_cfp() const noexcept
{
    return _fp.get();
}

FILE *FileWrapper::get_fp() noexcept
{
    return _fp.get();
}

bool FileWrapper::open(const std::string &file_path, const std::string &mode) noexcept
{
    _fp = FilePtr(std::fopen(file_path.c_str(), mode.c_str()));
    _is_open = _fp != nullptr;
    _last_errno = errno;
    return _is_open;
}

void FileWrapper::close() noexcept
{
    _fp.reset();
}

bool FileWrapper::is_open() const noexcept
{
    return _is_open;
}

FileWrapper::Errno_t FileWrapper::get_errno() const noexcept
{
    return _last_errno;
}

std::string FileWrapper::get_file_path() const noexcept
{
    expected<std::int32_t, FileWrapper::Errno_t> fdex = get_fd();
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

expected<FileWrapper::Offset_t, FileWrapper::Errno_t> FileWrapper::seek(FileWrapper &file,
                                                                        Offset_t offset,
                                                                        int destination) noexcept
{
    // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions,bugprone-narrowing-conversions)
    std::int64_t ret = std::fseek(file._fp.get(), offset, destination);
    return (ret == -1) ? unexpected<Errno_t>(errno) : expected<Offset_t, Errno_t>(static_cast<Offset_t>(ret));
}

expected<FileWrapper::Offset_t, FileWrapper::Errno_t> FileWrapper::tellp(const FileWrapper &file) noexcept
{
    std::int64_t ret = std::ftell(file._fp.get());
    return (ret == -1) ? unexpected<Errno_t>(errno) : expected<Offset_t, Errno_t>(static_cast<Offset_t>(ret));
}

bool FileWrapper::read(const FileWrapper &file, void *ptr, size_t len, Offset_t offset) noexcept
{
    if (!file.is_open())
    {
        return false;
    }

    int fd = fileno(file._fp.get()); // NOLINT(readability-identifier-length)

    while (len > 0)
    {
        // NOLINTNEXTLINE(cppcoreguidelines-narrowing-conversions,bugprone-narrowing-conversions)
        ssize_t read_bytes = pread(fd, ptr, len, offset);
        if (read_bytes == -1 && errno == EINTR)
        {
            continue;
        }

        if (read_bytes <= 0)
        {
            file._last_errno = errno;
            return false;
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        ptr = reinterpret_cast<char *>(ptr) + read_bytes;
        len -= read_bytes;
        offset += read_bytes;
    }

    return true;
}

bool FileWrapper::write(const FileWrapper &file, const void *buf, size_t len, Offset_t offset) noexcept
{
    if (!file.is_open())
    {
        return false;
    }

    int fd = fileno(file._fp.get()); // NOLINT(readability-identifier-length)

    const char *ptr = static_cast<const char *>(buf);
    size_t remaining = len;

    while (remaining > 0)
    {
        ssize_t nwrite = pwrite(fd, ptr, remaining, offset);

        if (nwrite <= 0)
        {
            if (errno == EINTR)
            {
                file._last_errno = EINTR;
                continue;
            }
            file._last_errno = errno;
            return false;
        }

        ptr += nwrite; // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
        offset += nwrite;
        remaining -= nwrite;
    }

    return true;
}

expected<std::uint64_t, FileWrapper::Errno_t> FileWrapper::get_file_size(FileWrapper &file) noexcept
{
    auto fd_ex = file.get_fd();
    if (!fd_ex.has_value())
    {
        return unexpected<Errno_t>(fd_ex.error());
    }

    int fd = fd_ex.value(); // NOLINT

    struct stat st{}; // NOLINT
    if (::fstat(fd, &st) != 0)
    {
        return unexpected<Errno_t>(errno);
    }

    return static_cast<std::uint64_t>(st.st_size);
}

} // namespace SymFind
