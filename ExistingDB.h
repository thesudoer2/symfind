#pragma once

#include <zstd.h>

#include "Config.h"
#include "DirWrapper.h"
#include "DatabaseHeader.h"

namespace SymFind
{

class ExistingDB
{
public:
    explicit ExistingDB(std::shared_ptr<ConfigParser> conf);
    ~ExistingDB() noexcept = default;

    ExistingDB(const ExistingDB&) noexcept = delete;
    ExistingDB(ExistingDB&&) noexcept = delete;

    ExistingDB& operator=(const ExistingDB&) noexcept = delete;
    ExistingDB& operator=(ExistingDB&&) noexcept = delete;

public: // NOLINT(readability-redundant-access-specifiers)
    __nodiscard bool get_error() const;

private:
    DatabaseHeader _hdr;

    std::shared_ptr<ConfigParser> _conf;

    std::unique_ptr<FileWrapper> _file;

    std::uint32_t _current_docid = 0;

    std::string _current_filename_block;
    const char *_current_filename_ptr = nullptr;
    const char *_current_filename_end = nullptr;

    off_t _compressed_dir_time_pos;
    std::string _compressed_dir_time;
    std::string _current_dir_time_block;
    const char *_current_dir_time_ptr = nullptr;
    const char *_current_dir_time_end = nullptr;

    std::pair<std::string, DirWrapper::DirTime> _unread_record;

    // Used in one-shot mode, repeatedly.
    ZSTD_DCtx *_ctx;

    // Used in streaming mode.
    ZSTD_DCtx *_dir_time_ctx;

    ZSTD_DDict *_ddict = nullptr;

    // If true, we've discovered an error or EOF, and will return only
    // empty data from here.
    bool _eof = false;
    bool _error = false;
};

} // namespace SymFind
