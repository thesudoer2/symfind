#pragma once

#include <string>
#include <vector>

#include <symfind/filesystem/FileWrapper.h>
#include <symfind/utils/Global.h>
#include <symfind/utils/NoCopy.h>
#include <symfind/utils/NoMove.h>
#include <symfind/core/Symbol.h>

#define DATABASE_HEADER_MAGIC 0x73796D66696E6400ULL // Magic String: "symfind\0"

#define DATABASE_HEADER_VERSION 0x0000100 // Version: 0.1.0

// Sentinel used in TrigramSlot::trigram_code to mark an empty slot.
// A real trigram code only ever occupies the low 24 bits, so this value
// (all 32 bits set) can never collide with a legitimate code.
#define TRIGRAM_EMPTY_SLOT 0xFFFFFFFFU

namespace SymFind
{

using FileOffset = FileWrapper::Offset_t;
using BlockOffset = std::uint32_t;

class Database : NoCopy, NoMove
{
    friend class DatabaseBuilder;

public:
    SYMFIND_PACK(struct FileTableHeader {
        FileOffset file_index_compressed_offset{0};
        std::uint32_t file_index_compressed_length{0};
        std::uint32_t file_index_uncompressed_length{0};

        FileOffset file_names_compressed_offset{0};
        std::uint32_t file_names_compressed_length{0};
        std::uint32_t file_names_uncompressed_length{0};

        std::uint32_t total_compressed_length{0};

        std::uint64_t files_count{0};
    });

    SYMFIND_PACK(struct PathTableHeader {
        FileOffset path_index_compressed_offset{0};
        std::uint32_t path_index_compressed_length{0};
        std::uint32_t path_index_uncompressed_length{0};

        FileOffset path_names_compressed_offset{0};
        std::uint32_t path_names_compressed_length{0};
        std::uint32_t path_names_uncompressed_length{0};

        std::uint32_t total_compressed_length{0};

        std::uint64_t paths_count{0};
    });

    SYMFIND_PACK(struct SymbolIndexHeader {
        FileOffset symbol_index_compressed_offset{0};
        std::uint32_t symbol_index_compressed_length{0};
        std::uint32_t symbol_index_uncompressed_length{0};

        FileOffset symbol_names_compressed_offset{0};
        std::uint32_t symbol_names_compressed_length{0};
        std::uint32_t symbol_names_uncompressed_length{0};

        FileOffset symbol_metadata_compressed_offset{0};
        std::uint32_t symbol_metadata_compressed_length{0};
        std::uint32_t symbol_metadata_uncompressed_length{0};

        std::uint32_t total_compressed_length{0};

        std::uint64_t symbols_count{0};
    });

    SYMFIND_PACK(struct TrigramIndexHeader {
        FileOffset trigram_index_compressed_offset{0};
        std::uint32_t trigram_index_compressed_length{0};
        std::uint32_t trigram_index_uncompressed_length{0};
        std::uint64_t trigram_index_capacity{0}; // number of slots in the hash table; power of two

        FileOffset trigram_postings_compressed_offset{0};
        std::uint32_t trigram_postings_compressed_length{0};
        std::uint32_t trigram_postings_uncompressed_length{0};
        std::uint64_t trigram_postings_count{0}; // number of uint32_t entries across all posting lists

        std::uint64_t trigrams_count{0}; // number of trigrams (stats only)
        std::uint64_t symbols_count{0};  // number of distinct symbols indexed (stats only)
    });

    SYMFIND_PACK(struct DatabaseHeader {
        std::uint64_t magic{0};

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

    explicit Database(const std::string &db_path, const std::string &mode = "wb+") noexcept;

    virtual ~Database() = default;

    bool operator!() const noexcept;
    bool operator!=(bool com_val) const noexcept;

    explicit operator bool() const noexcept;

    __nodiscard FileWrapper::Errno_t get_errno() const noexcept;

protected:
    FileWrapper _db_file;
};

SYMFIND_PACK(struct FileTable {
    std::uint32_t path_id{0};
    BlockOffset name_offset{0};
    std::uint32_t name_length{0};
});

SYMFIND_PACK(struct PathIndex {
    BlockOffset name_offset{0};
    std::uint32_t name_length{0};
});

SYMFIND_PACK(struct SymbolIndex {
    BlockOffset name_offset{0};
    std::uint32_t name_length{0};

    BlockOffset metadata_offset{0};
    std::uint32_t metadata_length{0};
});

// The following structs are getting used for database blocks.

SYMFIND_PACK(struct SymbolRef {
    std::uint32_t file_id{0}; // reference to FSScanner::found_files entry
    SymbolMetaData metadata;
});

using SymbolRefs = std::vector<SymbolRef>;

// One slot of the open-addressed hash table. Deliberately NOT packed:
// natural 4-byte alignment keeps scalar loads cheap on every target,
// and 12 bytes/slot is already small (a 2M-slot table, comfortably
// covering ~1.4M unique trigrams at 0.7 load factor, is ~24MB
// uncompressed and compresses very well since empty slots are all the
// same bit pattern).
SYMFIND_PACK(struct TrigramSlot {
    std::uint32_t trigram_code{TRIGRAM_EMPTY_SLOT}; // TRIGRAM_EMPTY_SLOT if the slot is empty
    BlockOffset posting_offset{0};                  // index (not byte offset) into the posting array
    std::uint32_t posting_count{0};                 // number of symbol IDs for this trigram
});

} // namespace SymFind
