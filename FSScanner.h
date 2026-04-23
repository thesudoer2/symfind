#pragma once

#include <cstdint>
#include <memory>

#include "BindMount.h"
#include "Config.h"
#include "DirWrapper.h"
#include "FileWrapper.h"

// NOLINTBEGIN(readability-identifier-length)

namespace SymFind
{

class FSScanner final
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
    explicit FSScanner(ConfigParserPtr conf, BindMount::InstancePtr bind_mount) noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    std::pair<bool, std::string> scan() noexcept;

    __nodiscard const FileList& get_found_files() const noexcept;

    static std::string get_file_info_full_path(const FileInfo&) noexcept;

private:
    static std::pair<bool, std::string> scan_fs(FSScanner &this_p,
                                                  std::shared_ptr<DirWrapper> dir_wrapper) noexcept;

private: // NOLINT(readability-redundant-access-specifiers)
    static StringCache _found_files_paths_cache;

    ConfigParserPtr _conf;
    BindMount::InstancePtr _bind_mount;

    FileList _found_files;
};

} // namespace SymFind

// NOLINTEND(readability-identifier-length)
