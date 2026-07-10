#pragma once

#include <vector>

#include <robin_hood.h>

#include <ankerl/unordered_dense.h>

#include <symfind/config/Config.h>
#include <symfind/database/Database.h>
#include <symfind/database/DatabaseBuilderUtils.h>
#include <symfind/core/DictionaryBuilder.h>
#include <symfind/filesystem/FSScanner.h>
#include <symfind/utils/Global.h>
#include <symfind/utils/NoCopy.h>
#include <symfind/utils/NoMove.h>
#include <symfind/core/Symbol.h>

namespace SymFind
{

class DatabaseBuilder final : NoCopy, NoMove
{
public:
    DatabaseBuilder(ConfigParserPtr conf, const FSScanner &fsscanner, DictionaryBuilderPtr dict_builder_ptr) noexcept;

    bool build(std::string* err_msg) noexcept;

private:
    using WorkerType = std::function<void(const FileIDList &, const FSScanner::FileList &, HashMap &, bool)>;

    static void launch_threads(WorkerType worker,
                               const FSScanner &fsscanner,
                               HashMapList &symtables,
                               std::uint16_t thread_count,
                               bool show_debug_messages = false);

    static bool store_db(const FSScanner &fsscanner,
                         ConfigParserPtr conf,
                         const HashMap &symtable,
                         DictionaryBuilderPtr dict_builder,
                         const std::string &db_path,
                         std::string *err_msg = nullptr) noexcept;

    __nodiscard bool extract_existing_db(std::string* err_msg = nullptr) noexcept;

    void merge_symtables(HashMapList &symtables, HashMap &merged_symtable) noexcept;

private: // NOLINT(readability-redundant-access-specifiers)
    ConfigParserPtr _conf;
    const FSScanner &_fsscanner;

    Database _existing_db;

    DictionaryBuilderPtr _dict_builder_ptr;
    std::uint64_t symbol_name_samples{};
};

} // namespace SymFind
