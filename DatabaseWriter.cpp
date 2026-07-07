#include "DatabaseWriter.h"

#include <format>

namespace SymFind
{

bool DatabaseWriter::write_database_buffer(DatabaseWriter &db,
                                           const void *buf,
                                           size_t len,
                                           FileWrapper::Offset_t &offset,
                                           std::string *err_msg) noexcept
{
    if (!db._db_file)
    {
        SET_ERR_MSG(err_msg, "database write failed: database not open!");
        return false;
    }

    try
    {
        bool write_res = FileWrapper::write(db._db_file, buf, len, offset);
        offset = write_res ? offset + len : offset;

        return write_res;
    }
    catch (const std::exception &ex)
    {
        SET_ERR_MSG(err_msg, std::format("database write failed: {}", ex.what()));
    }
    catch (...)
    {
        SET_ERR_MSG(err_msg, "Something went wrong!");
    }

    return false;
}

} // namespace SymFind
