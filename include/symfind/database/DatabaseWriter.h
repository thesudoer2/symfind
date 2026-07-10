#pragma once

#include <symfind/database/Database.h>

namespace SymFind
{

class DatabaseWriter final : public Database
{
public:
    using Database::Database; // Inherit all constructors of Database class

    static bool write_database_buffer(DatabaseWriter &db,
                                      const void *buf,
                                      size_t len,
                                      FileWrapper::Offset_t &offset,
                                      std::string *err_msg = nullptr) noexcept;
};

} // namespace
