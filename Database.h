#pragma once

#include <cstdint>
#include <memory>

#include "BindMount.h"
#include "Config.h"
#include "DirWrapper.h"
#include "ExistingDB.h"
#include "FileWrapper.h"

// NOLINTBEGIN(readability-identifier-length)

namespace SymFind
{

class Database final
{
private:
    struct StringCache final
    {
    public:
        using StringID = std::uint32_t;
        using String = std::string;

    private:
        using StringMap = std::unordered_map<String, StringID>;
        using StringList = std::vector<String>;

    public:
        StringCache() noexcept = default;
        ~StringCache() noexcept = default;

        StringCache(const StringCache&) noexcept = delete;
        StringCache(StringCache&&) noexcept = delete;

        StringCache& operator=(const StringCache&) noexcept = delete;
        StringCache& operator=(StringCache&&) noexcept = delete;

        StringID store_string(const String& str) noexcept;
        String get_string(StringID id) const noexcept;

    private:
        StringMap _str_to_id;
        StringList _id_to_str;
    };

public:
    struct FileInfo
    {
        std::string name;
        StringCache::StringID path_id;
    };

    using FileList = std::vector<FileInfo>;

public: // NOLINT(readability-redundant-access-specifiers)
    explicit Database(std::shared_ptr<ConfigParser> conf) noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    std::pair<bool, std::string> scan() noexcept;

    const FileList& get_found_files() const noexcept;

private:
    static std::pair<bool, std::string> scan_fs(Database &this_p,
                                                  std::shared_ptr<DirWrapper> dir_wrapper) noexcept;

private: // NOLINT(readability-redundant-access-specifiers)
    BindMount::InstancePtr _bind_mount;
    std::shared_ptr<ConfigParser> _conf;

    FileList _found_files;
    StringCache _found_files_paths_cache;

    std::unique_ptr<ExistingDB> _existing_db;
    // std::unique_ptr<DictBuilder> _dict_builder;
};

} // namespace SymFind

// NOLINTEND(readability-identifier-length)
