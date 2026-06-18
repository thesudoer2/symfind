#pragma once

#include <iostream>

#include <oneapi/tbb.h>

#include "Config.h"
#include "Database.h"
#include "FSScanner.h"
#include "Global.h"
#include "NoCopy.h"
#include "NoMove.h"
#include "Symbol.h"
#include "Threading.h"

namespace SymFind
{

using HashMap = oneapi::tbb::concurrent_hash_map<SymbolName, SymbolRefs>;
using HashMapAccessor = HashMap::accessor;

class DatabaseBuilder final : NoCopy, NoMove
{
public:
    DatabaseBuilder(ConfigParserPtr conf, const FSScanner &fsscanner) noexcept;

    bool build(std::string* err_msg) noexcept;

private:
    using WorkerType = std::function<void(const FileIDList &, const FSScanner::FileList &, HashMap &, bool)>;

    void launch_threads(ThreadList &thread_list, WorkerType worker);

    __nodiscard bool extract_existing_db(std::string* err_msg = nullptr) noexcept;

private: // NOLINT(readability-redundant-access-specifiers)
    ConfigParserPtr _conf;
    const FSScanner &_fsscanner;
    HashMap _symtable;

    Database _existing_db;
};

} // namespace SymFind
