#pragma once

#include <array>
#include <string>
#include <vector>

#include <cinttypes>
#include <cstddef>

#include "FileWrapper.h"
#include "Global.h"

#define DATABASE_HEADER_MAGIC_SIZE 9

namespace SymFind
{

using ByteArray = std::vector<std::byte>;

class Database final
{
public:
    SYMFIND_PACK(struct DatabaseHeader {
        char magic[DATABASE_HEADER_MAGIC_SIZE]{};

        uint32_t version{0};

        uint64_t conf_block_length_bytes{0};
        FileWrapper::Offset_t conf_block_offset_bytes{0};

        uint64_t zstd_dictionary_length_bytes{0};
        FileWrapper::Offset_t zstd_dictionary_offset_bytes{0};
    });

    Database() noexcept;

    static bool read_database(Database &db, const std::string &db_path, std::string *err_msg = nullptr) noexcept;

private:
    enum DatabaseReadStatus : std::uint8_t
    {
        NEVER_READ = 0x00,
        FAIL_READ = 0x02,
        SUCC_READ = 0x04,
    };

    DatabaseHeader _hdr;

    ByteArray _conf_block;
    ByteArray _zstd_dictionary;

    DatabaseReadStatus _last_read_status;
};

} // namespace SymFind
