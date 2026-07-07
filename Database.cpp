#include "Database.h"

#include "FileWrapper.h"
#include "Global.h"

namespace SymFind
{

std::uint64_t string_to_hex(const char arr[], int size)
{
    std::uint64_t hex{0};
    for (int i = 0; i < size; ++i)
    {
        hex = (hex << 8) | static_cast<unsigned char>(arr[i]);
    }
    return hex;
}

Database::Database(const std::string &db_path, const std::string &mode) noexcept
    : _db_file(db_path, mode)
{
}

bool Database::operator!() const noexcept
{
    return !_db_file.is_open();
}

bool Database::operator!=(bool com_val) const noexcept
{
    return _db_file.is_open() != com_val;
}

Database::operator bool() const noexcept
{
    return _db_file.is_open();
}

FileWrapper::Errno_t Database::get_errno() const noexcept
{
    return _db_file.get_errno();
}

} // namespace SymFind
