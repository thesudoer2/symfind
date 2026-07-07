#pragma once

#include <optional>

#include "Config.h"
#include "Database.h"
#include "MemoryMapFile.h"
#include "Symbol.h"
#include "TrigramUtils.h"
#include "ZSTDCompressor.h"
#include "ZSTDDictionary.h"

namespace SymFind
{

using IgnoreEntryCallback = std::function<bool(const SymFind::SymbolEntryView &)>;

class DatabaseReader final
{
public:
    struct FileReference
    {
        std::string full_path;
        SymbolMetaData metadata;
    };

    struct SymbolLookupResult
    {
        std::string symbol_name;
        std::vector<FileReference> references;
    };

    using SymbolLookupResultList = std::vector<SymbolLookupResult>;

public: // NOLINT
    static std::optional<SymbolLookupResultList> find_symbol_references(ConfigParserPtr conf,
                                                                        const std::string &symbol_name,
                                                                        const IgnoreEntryCallback &ignore_entry,
                                                                        std::string *err_msg = nullptr) noexcept;

    static void print_symbol_lookup_results(const SymbolLookupResultList &results) noexcept;

private:
    explicit DatabaseReader(ConfigParserPtr conf) noexcept;

    bool open(const std::string &db_path, std::string *err_msg = nullptr) noexcept;

    bool decompress_database(std::string *err_msg = nullptr) noexcept;

    bool read_header(std::string *err_msg = nullptr) noexcept;
    bool read_conf_block(std::string *err_msg = nullptr) noexcept;
    bool read_zstd_dictionary(std::string *err_msg = nullptr) noexcept;
    bool read_file_table(std::string *err_msg = nullptr) noexcept;
    bool read_path_table(std::string *err_msg = nullptr) noexcept;
    bool read_symbol_index(std::string *err_msg = nullptr) noexcept;
    bool read_trigram_index(std::string *err_msg = nullptr) noexcept;

    __nodiscard bool is_mapped_file_open(std::string *err_msg = nullptr) noexcept;

    bool decompress_block(FileOffset compressed_data_offset,
                          std::uint32_t compressed_data_len,
                          std::uint32_t uncompressed_data_len,
                          const DecompressionOptions &decomp_opts,
                          std::string &out_buf,
                          std::string *err_msg = nullptr) noexcept;

    std::span<const std::uint32_t> lookup_trigram(TrigramCode code) noexcept;

private: // NOLINT
    MappedFile _mf;
    ConfigParserPtr _conf;
    ZSTDCompressor _zstd;

    // ---- Database Blocks ----

    // Database header block
    const Database::DatabaseHeader *_db_hdr{nullptr};

    // Configuration block
    std::string_view _conf_block;

    // ZSTD dictionary block
    DecompressionOptions _with_dict_opt{.ddict = nullptr};
    const DecompressionOptions _no_dict_opt{.ddict = nullptr};

    // File table block
    const FileTable *_file_table{nullptr};
    std::string _file_table_buf;
    std::string _file_names_buf;
    std::uint32_t _files_count{0};

    // Path table block
    const PathIndex *_path_table{nullptr};
    std::string _path_table_buf;
    std::string _path_names_buf;
    std::uint32_t _paths_count{0};

    // Symbol index block
    const SymbolIndex *_symbol_index{nullptr};
    std::string _symbol_index_buf;
    std::string _symbol_names_buf;
    std::string _symbol_metadata_buf;
    std::uint32_t _symbols_count{0};

    // Trigram index block
    const TrigramSlot *_trigram_slots{nullptr};
    std::string _trigram_slots_buf;
    const std::uint32_t *_trigram_postings{nullptr};
    std::string _trigram_postings_buf;
    std::uint32_t _trigram_capacity{0};
};

} // namespace SymFind
