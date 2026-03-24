#include "FileWrapper.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <tuple>

#include <unistd.h>

#include <tl/expected.hpp>

namespace FindSymbol
{

FileWrapper::FileWrapper() noexcept = default;
FileWrapper::~FileWrapper() noexcept = default;

FileWrapper::FileWrapper(const std::string &file_path, const std::string &mode) noexcept
    : _fp(std::fopen(file_path.c_str(), mode.c_str()))
{
}

bool FileWrapper::operator!() const noexcept
{
    return _is_open;
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
    int fd = fileno(_fp.get()); // NOLINT(readability-identifier-length)
    return (fd == -1) ? unexpected<Errno_t>(errno) : expected<std::int32_t, Errno_t>(fd);
}

const FILE* FileWrapper::get_cfp() const noexcept
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

expected<FileWrapper::Offset_t, FileWrapper::Errno_t> FileWrapper::seek(FileWrapper &file,
                                                                        Offset_t offset,
                                                                        int destination) noexcept
{
    Offset_t ret = std::fseek(file._fp.get(), offset, destination);
    return (ret == -1) ? unexpected<Errno_t>(errno) : expected<Offset_t, Errno_t>(ret);
}

expected<FileWrapper::Offset_t, FileWrapper::Errno_t> FileWrapper::tellp(const FileWrapper &file) noexcept
{
    Offset_t ret = std::ftell(file._fp.get());
    return (ret == -1) ? unexpected<Errno_t>(errno) : expected<Offset_t, Errno_t>(ret);
}

bool FileWrapper::read(const FileWrapper &file, void *ptr, size_t len, off_t offset) noexcept
{
    if (!file.is_open())
    {
        return false;
    }

    int fd = fileno(file._fp.get()); // NOLINT(readability-identifier-length)

    while (len > 0)
    {
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

        ptr = reinterpret_cast<char *>(ptr) + read_bytes; // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast,cppcoreguidelines-pro-bounds-pointer-arithmetic)
        len -= read_bytes;
        offset += read_bytes;
    }

    return true;
}

expected<std::int64_t, FileWrapper::Errno_t> FileWrapper::get_file_size(FileWrapper &file) noexcept
{
    if (!file.is_open())
    {
        return unexpected<Errno_t>(-1);
    }

    expected<Offset_t, Errno_t> seek_res = FileWrapper::seek(file, Offset_t(0), SEEK_END);

    std::ignore = FileWrapper::seek(file, Offset_t(0), SEEK_SET);

    return seek_res ? expected<std::int64_t, Errno_t>{seek_res.value()} : unexpected<Errno_t>(seek_res.error());
}

} // namespace FindSymbol
