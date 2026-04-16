#include "ExistingDB.h"

#include <memory>
#include <utility>

#include <zstd.h>

#include "Config.h"

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
