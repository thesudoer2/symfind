#include "Database.h"

#include <format>

#include <cstring>

#include "FileWrapper.h"

#define SET_ERR_MSG(err_msg_buf, err_msg)                                                                              \
    {                                                                                                                  \
        if ((err_msg_buf) != nullptr)                                                                                  \
            *(err_msg_buf) = err_msg;                                                                                  \
    }

static constexpr char DATABASE_HEADER_MAGIC[DATABASE_HEADER_MAGIC_SIZE]{'\0', 's', 'y', 'm', 'f', 'i', 'n', 'd', '\0'};

namespace SymFind
{

Database::Database() noexcept : _last_read_status(DatabaseReadStatus::NEVER_READ)
{
}

// NOLINTBEGIN
bool Database::read_database(Database &db, const std::string &db_path, std::string *err_msg) noexcept
{
    if ((bool)(db._last_read_status & DatabaseReadStatus::FAIL_READ))
    {
        return false;
    }

    FileWrapper db_file(db_path, "rb");
    if (!db_file)
    {
        SET_ERR_MSG(err_msg, std::strerror(db_file.get_errno()));
        goto RETURN_FAIL;
    }

    try
    {
        // Read database header
        if (!FileWrapper::read(db_file, &db._hdr, sizeof(DatabaseHeader), 0))
        {
            SET_ERR_MSG(err_msg, "database read failed!");
            goto RETURN_FAIL;
        }

        // Compare database header's magic number
        if (std::memcmp(db._hdr.magic, DATABASE_HEADER_MAGIC, DATABASE_HEADER_MAGIC_SIZE) != 0)
        {
            SET_ERR_MSG(err_msg, "database had header mismatch, ignoring...");
            goto RETURN_FAIL;
        }

        // Read conf block from database
        if (db._hdr.conf_block_length_bytes == 0)
        {
            SET_ERR_MSG(err_msg, "database config block size is ZERO!");
            goto RETURN_FAIL;
        }

        db._conf_block.resize(db._hdr.conf_block_length_bytes);
        if (!FileWrapper::read(db_file,
                            db._conf_block.data(),
                            db._hdr.conf_block_length_bytes,
                            db._hdr.conf_block_offset_bytes))
        {
            SET_ERR_MSG(err_msg, "reading conf-block failed!");
            goto RETURN_FAIL;
        }

        // Read zstd dictionary from database
        if (db._hdr.zstd_dictionary_length_bytes == 0)
        {
            SET_ERR_MSG(err_msg, "database config block size is ZERO!");
            goto RETURN_FAIL;
        }

        db._zstd_dictionary.resize(db._hdr.zstd_dictionary_length_bytes);
        if (!FileWrapper::read(db_file,
                            db._zstd_dictionary.data(),
                            db._hdr.zstd_dictionary_length_bytes,
                            db._hdr.zstd_dictionary_offset_bytes))
        {
            SET_ERR_MSG(err_msg, "reading zstd-dictionary failed!");
            goto RETURN_FAIL;
        }

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
// NOLINTEND

} // namespace SymFind
