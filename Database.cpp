#include "Database.h"

#include <format>

#include <cstring>

#include "FileWrapper.h"
#include "Global.h"

static constexpr char DATABASE_HEADER_MAGIC[DATABASE_HEADER_MAGIC_SIZE]{'\0', 's', 'y', 'm', 'f', 'i', 'n', 'd', '\0'};

namespace SymFind
{

Database::Database(const std::string &db_path) noexcept
    : _last_read_status(DatabaseReadStatus::NEVER_READ), _db_file(db_path, "wb+")
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

// NOLINTBEGIN
bool Database::read_database(Database &db, std::string *err_msg) noexcept
{
    if ((bool)(db._last_read_status & DatabaseReadStatus::FAIL_READ))
    {
        return false;
    }

    if (!db._db_file)
    {
        SET_ERR_MSG(err_msg, std::strerror(db.get_errno()));
        goto RETURN_FAIL;
    }

    try
    {
        // Read database header
        if (!FileWrapper::read(db._db_file, &db._hdr, sizeof(DatabaseHeader), 0))
        {
            SET_ERR_MSG(err_msg, std::format("database read failed: {}", std::strerror(db._db_file.get_errno())));
            goto RETURN_FAIL;
        }

        // Compare database header's magic number
        if (std::memcmp(db._hdr.magic, DATABASE_HEADER_MAGIC, DATABASE_HEADER_MAGIC_SIZE) != 0)
        {
            SET_ERR_MSG(err_msg, "database had header mismatch, ignoring...");
            goto RETURN_FAIL;
        }

        // // Read conf block from database
        // if (db._hdr.conf_block_length == 0)
        // {
        //     SET_ERR_MSG(err_msg, "database config block size is ZERO!");
        //     goto RETURN_FAIL;
        // }

        // db._conf_block.resize(db._hdr.conf_block_length);
        // if (!FileWrapper::read(db._db_file,
        //                        db._conf_block.data(),
        //                        db._hdr.conf_block_length,
        //                        db._hdr.conf_block_offset_bytes))
        // {
        //     SET_ERR_MSG(err_msg, "reading conf-block failed!");
        //     goto RETURN_FAIL;
        // }

        // // Read zstd dictionary from database
        // if (db._hdr.zstd_dictionary_length == 0)
        // {
        //     SET_ERR_MSG(err_msg, "database config block size is ZERO!");
        //     goto RETURN_FAIL;
        // }

        // db._zstd_dictionary.resize(db._hdr.zstd_dictionary_length);
        // if (!FileWrapper::read(db._db_file,
        //                        db._zstd_dictionary.data(),
        //                        db._hdr.zstd_dictionary_length,
        //                        db._hdr.zstd_dictionary_offset_bytes))
        // {
        //     SET_ERR_MSG(err_msg, "reading zstd-dictionary failed!");
        //     goto RETURN_FAIL;
        // }

        return true;
    }
    catch (const std::exception &ex)
    {
        SET_ERR_MSG(err_msg, std::format("Reading database failed: {}", ex.what()));
    }
    catch (...)
    {
        SET_ERR_MSG(err_msg, "Something went wrong!");
    }

RETURN_FAIL:
    db._last_read_status = DatabaseReadStatus::FAIL_READ;
    return false;
}

bool Database::write_database_buffer(Database &db,
                                     const void *buf,
                                     size_t len,
                                     FileWrapper::Offset_t &offset,
                                     std::string *err_msg) noexcept
{
    if (!db._db_file)
    {
        SET_ERR_MSG(err_msg, "database write failed: database not open!");
        goto RETURN_FAIL;
    }

    try
    {
        FileWrapper::write(db._db_file, buf, len, offset);
        auto ftell_expect = FileWrapper::tellp(db._db_file);
        if (!ftell_expect.has_value())
        {
            SET_ERR_MSG(err_msg, ftell_expect.error());
            goto RETURN_FAIL;
        }

        offset = ftell_expect.value();
        return true;
    }
    catch (const std::exception &ex)
    {
        SET_ERR_MSG(err_msg, std::format("database write failed: {}", ex.what()));
    }
    catch (...)
    {
        SET_ERR_MSG(err_msg, "Something went wrong!");
    }

RETURN_FAIL:
    db._last_read_status = DatabaseReadStatus::FAIL_READ;
    return false;
}
// NOLINTEND

} // namespace SymFind
