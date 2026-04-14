#pragma once

#include <cstdint>
#include <memory>

#include "BindMount.h"
#include "Config.h"
#include "DirWrapper.h"
#include "ExistingDB.h"

namespace SymFind
{

class Database final
{
public:
    explicit Database(std::shared_ptr<ConfigParser> conf) noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    std::pair<bool, std::string> scan() noexcept;

private:
    static std::pair<bool, std::string> scan_impl(const Database &this_p,
                                                  std::shared_ptr<DirWrapper> dir_wrapper) noexcept;

private: // NOLINT(readability-redundant-access-specifiers)
    std::shared_ptr<ConfigParser> _conf;
    BindMount::InstancePtr _bind_mount;
    // std::unique_ptr<DictBuilder> _dict_builder;
    std::unique_ptr<ExistingDB> _existing_db;
};

} // namespace SymFind
