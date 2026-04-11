#include "ExistingDB.h"

#include <memory>
#include <string>
#include <utility>

#include <zstd.h>

#include "Database.h"
#include "Config.h"
#include "FileWrapper.h"

namespace SymFind
{

ExistingDB::ExistingDB(std::shared_ptr<ConfigParser> conf)
    : _conf(std::move(conf)), _file{nullptr}
{
    _error = true; // NOLINT
}

bool ExistingDB::get_error() const
{
    return _error;
}

} // namespace SymFind
