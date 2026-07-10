#pragma once

#include <optional>

#include <symfind/database/DatabaseWriter.h>
#include <symfind/filesystem/FSScanner.h>
#include <symfind/filesystem/FileWrapper.h>
#include <symfind/core/Symbol.h>
#include <symfind/database/ZSTDDictionary.h>

namespace SymFind
{

// The following `Result` structs are defiend to keep `build_...` functions outputs.

struct FileTableBuildResult
{
    std::string file_index_compressed;
    std::string file_names_compressed;
    std::uint64_t file_index_uncompressed_length{0};
    std::uint64_t file_names_uncompressed_length{0};
    std::uint32_t files_count{0};
};

struct PathTableBuildResult
{
    std::string path_index_compressed;
    std::string path_names_compressed;
    std::uint64_t path_index_uncompressed_length{0};
    std::uint64_t path_names_uncompressed_length{0};
    std::uint32_t paths_count{0};
};

struct SymbolIndexBuildResult
{
    std::string symbol_index_compressed;
    std::string symbol_names_compressed;
    std::string symbol_metadata_compressed;
    std::uint64_t symbol_index_uncompressed_length{0};
    std::uint64_t symbol_names_uncompressed_length{0};
    std::uint64_t symbol_metadata_uncompressed_length{0};
    std::uint32_t symbols_count{0};
};

struct TrigramIndexBuildResult
{
    std::string trigram_index_compressed;
    std::string trigram_postings_compressed;
    std::uint64_t trigram_index_uncompressed_length{0};
    std::uint64_t trigram_postings_uncompressed_length{0};
    std::uint64_t trigram_postings_count{0};
    std::uint64_t trigram_index_capacity{0};
    std::uint32_t symbols_count{0};
    std::uint64_t trigrams_count{0};
};


using ByteType = std::uint8_t;
using ByteArray = std::vector<ByteType>;

using HashMap = ankerl::unordered_dense::map<SymbolName, SymbolRefs, robin_hood::hash<SymbolName>>;
using HashMapList = std::vector<HashMap>;

template <typename T>
static void append_object(ByteArray &out, const T &obj);

std::optional<FileTableBuildResult> build_file_table(const FSScanner &fsscanner,
                                                     const CompressionOptions &comp_opts,
                                                     std::string *err_msg) noexcept;

std::optional<Database::FileTableHeader> commit_file_table(DatabaseWriter &db, // NOLINT
                                                           FileWrapper::Offset_t &write_offset,
                                                           const FileTableBuildResult &built,
                                                           std::string *err_msg) noexcept;

std::optional<PathTableBuildResult> build_path_table(const FSScanner::StringCache::StringList &path_list,
                                                     const CompressionOptions &comp_opts,
                                                     std::string *err_msg) noexcept;

std::optional<Database::PathTableHeader> commit_path_table(DatabaseWriter &db, // NOLINT
                                                           FileWrapper::Offset_t &write_offset,
                                                           const PathTableBuildResult &built,
                                                           std::string *err_msg) noexcept;

std::optional<SymbolIndexBuildResult> build_symbol_index(const HashMap &symtable,
                                                         const CompressionOptions &comp_opts,
                                                         std::string *err_msg) noexcept;

std::optional<Database::SymbolIndexHeader> commit_symbol_index(DatabaseWriter &db, // NOLINT
                                                               FileWrapper::Offset_t &write_offset,
                                                               const SymbolIndexBuildResult &built,
                                                               std::string *err_msg) noexcept;

std::optional<TrigramIndexBuildResult> build_trigram_index(const HashMap &symtable, std::string *err_msg) noexcept;

std::optional<Database::TrigramIndexHeader> commit_trigram_index(DatabaseWriter &db, // NOLINT
                                                                 FileWrapper::Offset_t &write_offset,
                                                                 const TrigramIndexBuildResult &built,
                                                                 std::string *err_msg) noexcept;

} // namespace SymFind
