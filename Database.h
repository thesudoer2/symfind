#pragma once

#include <string>
#include <vector>

#include <cstddef>

#include "FileWrapper.h"
#include "Global.h"

#define DATABASE_HEADER_MAGIC_SIZE 9

namespace SymFind
{

class DatabaseBuilder;

class Database final
{
    friend class DatabaseBuilder;

public:
    SYMFIND_PACK(struct FileTableHeader{
        FileWrapper::Offset_t file_index_compressed_offset{0};
        std::uint32_t file_index_compressed_length{0};
        std::uint32_t file_index_uncompressed_length{0};

        FileWrapper::Offset_t file_names_compressed_offset{0};
        std::uint32_t file_names_compressed_length{0};
        std::uint32_t file_names_uncompressed_length{0};

        std::uint32_t total_compressed_length{0};

        std::uint64_t files_count{0};
    });

    SYMFIND_PACK(struct PathTableHeader{
        FileWrapper::Offset_t path_index_compressed_offset{0};
        std::uint32_t path_index_compressed_length{0};
        std::uint32_t path_index_uncompressed_length{0};

        FileWrapper::Offset_t path_names_compressed_offset{0};
        std::uint32_t path_names_compressed_length{0};
        std::uint32_t path_names_uncompressed_length{0};

        std::uint32_t total_compressed_length{0};

        std::uint64_t paths_count{0};
    });

    SYMFIND_PACK(struct SymbolIndexHeader {
        FileWrapper::Offset_t symbol_index_compressed_offset{0};
        std::uint32_t symbol_index_compressed_length{0};
        std::uint32_t symbol_index_uncompressed_length{0};

        FileWrapper::Offset_t symbol_names_compressed_offset{0};
        std::uint32_t symbol_names_compressed_length{0};
        std::uint32_t symbol_names_uncompressed_length{0};

        FileWrapper::Offset_t symbol_metadata_compressed_offset{0};
        std::uint32_t symbol_metadata_compressed_length{0};
        std::uint32_t symbol_metadata_uncompressed_length{0};

        std::uint32_t total_compressed_length{0};

        std::uint64_t symbols_count{0};
    });

    SYMFIND_PACK(struct TrigramIndexHeader {
        FileWrapper::Offset_t trigram_index_compressed_offset{0};
        std::uint32_t trigram_index_compressed_length{0};
        std::uint32_t trigram_index_uncompressed_length{0};
        std::uint64_t trigram_index_capacity{0}; // number of slots in the hash table; power of two

        FileWrapper::Offset_t trigram_postings_compressed_offset{0};
        std::uint32_t trigram_postings_compressed_length{0};
        std::uint32_t trigram_postings_uncompressed_length{0};
        std::uint64_t trigram_postings_count{0};  // number of uint32_t entries across all posting lists

        std::uint64_t trigrams_count{0};   // number of trigrams (stats only)
        std::uint64_t symbols_count{0};   // number of distinct symbols indexed (stats only)
    });

    SYMFIND_PACK(struct DatabaseHeader {
        char magic[DATABASE_HEADER_MAGIC_SIZE]{};

        std::uint32_t version{1};

        //
        // Config block
        //
        std::uint64_t conf_block_offset{0};
        std::uint64_t conf_block_length{0};

        //
        // ZSTD dictionary
        //
        std::uint64_t zstd_dictionary_offset{0};
        std::uint64_t zstd_dictionary_length{0};

        //
        // File table
        //
        FileTableHeader file_table_hdr;

        //
        // Path table
        //
        PathTableHeader path_table_hdr;

        //
        // Symbol name index
        //
        SymbolIndexHeader sym_index_hdr;

        //
        // Trigram index
        //
        TrigramIndexHeader trigram_index_hdr;
    });

    explicit Database(const std::string &db_path) noexcept;

    bool operator!() const noexcept;
    bool operator!=(bool com_val) const noexcept;

    explicit operator bool() const noexcept;

public: // NOLINT
    __nodiscard FileWrapper::Errno_t get_errno() const noexcept;

    static bool read_database(Database &db, std::string *err_msg = nullptr) noexcept;

    static bool write_database_buffer(Database &db,
                                      const void *buf,
                                      size_t len,
                                      FileWrapper::Offset_t &offset,
                                      std::string *err_msg = nullptr) noexcept;

private:
    enum DatabaseReadStatus : std::uint8_t
    {
        NEVER_READ = 0x00,
        FAIL_READ = 0x02,
        SUCC_READ = 0x04,
    };

    DatabaseHeader _hdr;

    DatabaseReadStatus _last_read_status;

    FileWrapper _db_file;
};

} // namespace SymFind
