#include <symfind/filesystem/MemoryMapFile.h>

#include <cstddef>
#include <cstring>

#include <format>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <symfind/utils/Expected.h>
#include <symfind/filesystem/FileWrapper.h>

namespace SymFind
{

MappedFile::MappedFile() noexcept = default;

MappedFile::~MappedFile() noexcept = default;

bool MappedFile::open(const std::string &path, std::string *err_msg) noexcept
{
    _db_file.open(path, "r");

    if (!_db_file)
    {
        SET_ERR_MSG(err_msg, std::format("{}: {}", std::strerror(_db_file.get_errno()), path));
        return false;
    }

    // Read file size
    auto size_ex = FileWrapper::get_file_size(_db_file);
    if (!size_ex.has_value())
    {
        SET_ERR_MSG(err_msg, std::strerror(size_ex.error()));
        return false;
    }
    _size = size_ex.value();

    // Read file descriptor
    auto fd_ex = _db_file.get_fd();
    if (!fd_ex.has_value())
    {
        SET_ERR_MSG(err_msg, std::strerror(fd_ex.error()));
        return false;
    }
    int fd = fd_ex.value(); // NOLINT

    // Memory map the file
    void *ptr = ::mmap(nullptr, _size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (ptr == MAP_FAILED)
    {
        SET_ERR_MSG(err_msg, "mmap() failed: " + std::string(std::strerror(errno)));
        return false;
    }
    _data = static_cast<const std::byte *>(ptr);

    return true;
}

const std::byte *MappedFile::data() const
{
    return _data;
}

std::size_t MappedFile::size() const
{
    return _size;
}

void MappedFile::close() noexcept
{
    if (_data != nullptr)
    {
        ::munmap(const_cast<std::byte *>(_data), _size); // NOLINT
        _data = nullptr;
    }

    _size = 0;
}

bool MappedFile::operator!() const noexcept
{
    return !_db_file.is_open();
}

bool MappedFile::operator!=(bool com_val) const noexcept
{
    return _db_file.is_open() != com_val;
}

MappedFile::operator bool() const noexcept
{
    return _db_file.is_open();
}

FileWrapper::Errno_t MappedFile::get_errno() const noexcept
{
    return _db_file.get_errno();
}

} // namespace SymFind
