#pragma once

#include "Database.h"
#include "DatabaseBuilder.h"
#include "FSScanner.h"
#include "FileWrapper.h"
#include "Symbol.h"
#include "Trigram.h"
#include "ZSTDCompressor.h"
#include "ZSTDDictionary.h"
#include <optional>

// Sentinel used in TrigramSlot::trigram_code to mark an empty slot.
// A real trigram code only ever occupies the low 24 bits, so this value
// (all 32 bits set) can never collide with a legitimate code.
#define TRIGRAM_EMPTY_SLOT 0xFFFFFFFFU

namespace SymFind
{

SYMFIND_PACK(struct FileTable {
    std::uint32_t path_id{0};
    FileWrapper::Offset_t name_offset{0};
    std::uint32_t name_length{0};
});

SYMFIND_PACK(struct PathIndex {
    FileWrapper::Offset_t name_offset{0};
    std::uint32_t name_length{0};
});

SYMFIND_PACK(struct SymbolIndex {
    FileWrapper::Offset_t name_offset{0};
    std::uint32_t name_length{0};

    FileWrapper::Offset_t metadata_offset{0};
    std::uint32_t metadata_length{0};
});

struct TrigramEntry
{
    std::uint32_t code;
    std::vector<std::uint32_t> symbols;
};

using TrigramEntries = std::vector<TrigramEntry>;

// One slot of the open-addressed hash table. Deliberately NOT packed:
// natural 4-byte alignment keeps scalar loads cheap on every target,
// and 12 bytes/slot is already small (a 2M-slot table, comfortably
// covering ~1.4M unique trigrams at 0.7 load factor, is ~24MB
// uncompressed and compresses very well since empty slots are all the
// same bit pattern).
SYMFIND_PACK(struct TrigramSlot {
    std::uint32_t trigram_code{TRIGRAM_EMPTY_SLOT}; // TRIGRAM_EMPTY_SLOT if the slot is empty
    std::uint32_t posting_offset{0};                // index (not byte offset) into the posting array
    std::uint32_t posting_count{0};                 // number of symbol IDs for this trigram
});

using ByteType = std::uint8_t;
using ByteArray = std::vector<ByteType>;

template <typename T>
static void append_object(ByteArray &out, const T &obj)
{
    const auto *ptr = reinterpret_cast<const ByteType *>(&obj); // NOLINT
    out.insert(out.end(), ptr, ptr + sizeof(T)); // NOLINT
}

std::optional<Database::FileTableHeader> write_file_table(Database &db,
                                                          FileWrapper::Offset_t write_offset,
                                                          const FSScanner &fsscanner,
                                                          ZSTDCompressor &zstd_compressor,
                                                          CompressionOptions &comp_opts,
                                                          std::string *err_msg) noexcept
{
    Database::FileTableHeader ft_hdr;

    // Relative file name offset (the offset is calculated in file names block not whole database)
    std::uint32_t file_name_offset{0};
    const FSScanner::FileList &found_files = fsscanner.get_found_files();

    ByteArray file_index_block;
    std::string file_names_blob{};

    for (const auto &file_info : found_files)
    {
        const std::string &filename = file_info.name;
        auto file_name_length = static_cast<decltype(FileTable::name_length)>(filename.size());

        // Serialize file info
        FileTable file_table_entry{.path_id = file_info.path_id,
                                   .name_offset = file_name_offset,
                                   .name_length = file_name_length};
        append_object(file_index_block, file_table_entry);

        // Advance file name offset
        file_name_offset += file_name_length;

        // Append filename
        file_names_blob += filename;

        // Count files
        ft_hdr.files_count += 1;
    }

    // Compress file index block (WITHOUT DICTIONARY)
    std::string compressed_out;
    if (!zstd_compressor.compress(file_index_block.data(), file_index_block.size(), compressed_out, no_dict, err_msg))
    {
        return {};
    }

    ft_hdr.file_index_compressed_offset = write_offset;
    ft_hdr.file_index_compressed_length = compressed_out.length();
    ft_hdr.file_index_uncompressed_length = file_index_block.size();

    // Write compressed file index to database
    if (!Database::write_database_buffer(db,
                                         compressed_out.data(),
                                         compressed_out.length(),
                                         write_offset,
                                         err_msg))
    {
        return {};
    }

    // Compress file names (WITH DICTIONARY)
    compressed_out.clear();
    if (!zstd_compressor.compress(file_names_blob.data(), file_names_blob.size(), compressed_out, comp_opts, err_msg))
    {
        return {};
    }

    ft_hdr.file_names_compressed_offset = write_offset;
    ft_hdr.file_names_compressed_length = compressed_out.length();
    ft_hdr.file_names_uncompressed_length = file_names_blob.size();
    ft_hdr.total_compressed_length =
        ft_hdr.file_index_compressed_length + ft_hdr.file_names_compressed_length;

    // Write compressed file names to database
    if (!Database::write_database_buffer(db,
                                         compressed_out.data(),
                                         compressed_out.length(),
                                         write_offset,
                                         err_msg))
    {
        return {};
    }

    return {ft_hdr};
}

std::optional<Database::PathTableHeader> write_path_table(Database &db,
                                                          FileWrapper::Offset_t &write_offset,
                                                          const FSScanner::StringCache::StringList &path_list,
                                                          ZSTDCompressor &zstd_compressor,
                                                          CompressionOptions &comp_opts,
                                                          std::string *err_msg) noexcept
{
    Database::PathTableHeader pt_hdr;

    // Relative path name offset (the offset is calculated in path names block not whole database)
    std::uint32_t path_name_offset{0};

    ByteArray path_index_block;
    std::string path_names_blob{};

    for (const auto &path_name : path_list)
    {
        auto path_name_length = static_cast<decltype(PathIndex::name_length)>(path_name.size());

        // Serialize path index
        PathIndex path_table_entry{.name_offset = path_name_offset, .name_length = path_name_length};
        append_object(path_index_block, path_table_entry);

        // Advance path name offset
        path_name_offset += path_name_length;

        // Append filename
        path_names_blob += path_name;

        // Count files
        pt_hdr.paths_count += 1;
    }

    // Compress path index block (WITHOUT DICTIONARY)
    std::string compressed_out;
    if (!zstd_compressor.compress(path_index_block.data(), path_index_block.size(), compressed_out, no_dict, err_msg))
    {
        return {};
    }

    pt_hdr.path_index_compressed_offset = write_offset;
    pt_hdr.path_index_compressed_length = compressed_out.length();
    pt_hdr.path_index_uncompressed_length = path_index_block.size();

    // Write compressed path index to database
    if (!Database::write_database_buffer(db,
                                         compressed_out.data(),
                                         compressed_out.length(),
                                         write_offset,
                                         err_msg))
    {
        return {};
    }

    // Compress path names (WITH DICTIONARY)
    compressed_out.clear();
    if (!zstd_compressor.compress(path_names_blob.data(), path_names_blob.size(), compressed_out, comp_opts, err_msg))
    {
        return {};
    }

    pt_hdr.path_names_compressed_offset = write_offset;
    pt_hdr.path_names_compressed_length = compressed_out.length();
    pt_hdr.path_names_uncompressed_length = path_names_blob.size();
    pt_hdr.total_compressed_length =
        pt_hdr.path_index_compressed_length + pt_hdr.path_names_compressed_length;

    // Write compressed file names to database
    if (!Database::write_database_buffer(db,
                                         compressed_out.data(),
                                         compressed_out.length(),
                                         write_offset,
                                         err_msg))
    {
        return {};
    }

    return {pt_hdr};
}

std::optional<Database::SymbolIndexHeader> write_symbol_index(Database &db,
                                                              FileWrapper::Offset_t &write_offset,
                                                              const HashMap &symtable,
                                                              ZSTDCompressor &zstd_compressor,
                                                              CompressionOptions &comp_opts,
                                                              std::string *err_msg) noexcept
{
    Database::SymbolIndexHeader si_hdr;

    // Relative symbol name/metadata offset (the offset is calculated in symbol names/metadata block not whole database)
    std::uint32_t sym_name_offset{0};
    std::uint32_t sym_metadata_offset{0};

    ByteArray sym_index_block;
    ByteArray sym_metadata_blob;
    std::string sym_names_blob{};

    for (const auto &[sym_name, sym_refs] : symtable)
    {
        auto sym_name_length = static_cast<decltype(SymbolIndex::name_length)>(sym_name.size());

        // Serialize metadata
        decltype(SymbolIndex::metadata_length) sym_metadata_length{0};
        for (const auto &sym_ref : sym_refs)
        {
            sym_metadata_length += sizeof(sym_ref);
            append_object(sym_metadata_blob, sym_ref);
        }

        // Serialize symbol index
        SymbolIndex sym_index_entry{.name_offset = sym_name_offset,
                                    .name_length = sym_name_length,
                                    .metadata_offset = sym_metadata_offset,
                                    .metadata_length = sym_metadata_length};
        append_object(sym_index_block, sym_index_entry);

        // Advance symbol name and metadata offset
        sym_name_offset += sym_name_length;
        sym_metadata_offset = sym_metadata_length;

        // Append symbol name
        sym_names_blob += sym_name;

        // Count files
        si_hdr.symbols_count += 1;
    }

    // Compress symbol index block (WITHOUT DICTIONARY)
    std::string compressed_out;
    if (!zstd_compressor.compress(sym_index_block.data(), sym_index_block.size(), compressed_out, no_dict, err_msg))
    {
        return {};
    }

    si_hdr.symbol_index_compressed_offset = write_offset;
    si_hdr.symbol_index_compressed_length = compressed_out.length();
    si_hdr.symbol_index_uncompressed_length = sym_index_block.size();

    // Write compressed symbol index to database
    if (!Database::write_database_buffer(db,
                                         compressed_out.data(),
                                         compressed_out.length(),
                                         write_offset,
                                         err_msg))
    {
        return {};
    }

    // Compress symbol names (WITH DICTIONARY)
    compressed_out.clear();
    if (!zstd_compressor.compress(sym_names_blob.data(), sym_names_blob.size(), compressed_out, comp_opts, err_msg))
    {
        return {};
    }

    si_hdr.symbol_names_compressed_offset = write_offset;
    si_hdr.symbol_names_compressed_length = compressed_out.length();
    si_hdr.symbol_names_uncompressed_length = sym_names_blob.size();

    // Write compressed symbol names to database
    if (!Database::write_database_buffer(db,
                                         compressed_out.data(),
                                         compressed_out.length(),
                                         write_offset,
                                         err_msg))
    {
        return {};
    }

    // Compress symbol metadata (WITHOUT DICTIONARY)
    compressed_out.clear();
    if (!zstd_compressor.compress(sym_metadata_blob.data(), sym_metadata_blob.size(), compressed_out, no_dict))
    {
        return {};
    }

    si_hdr.symbol_metadata_compressed_offset = write_offset;
    si_hdr.symbol_metadata_compressed_length = compressed_out.length();
    si_hdr.symbol_metadata_uncompressed_length = sym_metadata_blob.size();

    si_hdr.total_compressed_length = si_hdr.symbol_index_compressed_length + si_hdr.symbol_names_compressed_length +
                                     si_hdr.symbol_metadata_compressed_length;

    // Write compressed symbol metadata to database
    if (!Database::write_database_buffer(db,
                                         compressed_out.data(),
                                         compressed_out.length(),
                                         write_offset,
                                         err_msg))
    {
        return {};
    }

    return {si_hdr};
}

inline uint64_t capacity_for_count(uint64_t count, double max_load_factor = 0.7) {
    uint64_t capacity = 16;
    while (capacity == 0 ||
           static_cast<double>(count) > static_cast<double>(capacity) * max_load_factor) {
        capacity <<= 1;
    }
    return capacity;
}

inline constexpr uint64_t fibonacci_hash(uint32_t trigram_code) {
    constexpr uint64_t golden_ratio64 = 0x9E3779B97F4A7C15ULL;
    return static_cast<uint64_t>(trigram_code) * golden_ratio64;
}

inline uint64_t trigram_home_slot(uint32_t trigram_code, uint32_t capacity_bits) {
    if (capacity_bits == 0) return 0;
    return fibonacci_hash(trigram_code) >> (64 - capacity_bits);
}

std::optional<Database::TrigramIndexHeader> write_trigram_index(Database &db,
                                                                FileWrapper::Offset_t &write_offset,
                                                                const HashMap &symtable,
                                                                ZSTDCompressor &zstd_compressor,
                                                                std::string *err_msg) noexcept
{
    Database::TrigramIndexHeader tg_hdr{};

    TrigramBuilder tg_builder;
    std::uint32_t symbols_count{0};

    // Append symbols to trigram
    for (const auto &[sym_name, sym_refs] : symtable)
    {
        tg_builder.add_word(sym_name, symbols_count);
    }

    const auto &trigram_to_symbols = tg_builder.get_trigram_to_symbols();

    std::uint64_t total_postings_count{0};

    TrigramEntries entries;
    entries.reserve(trigram_to_symbols.size());

    // Create entry list from "trigram_code->symbols" map
    for (const auto &[code, symbols] : trigram_to_symbols)
    {
        std::vector<std::uint32_t> syms = symbols;
        std::ranges::sort(syms);
        syms.erase(std::ranges::unique(syms).begin(), syms.end());

        total_postings_count += syms.size();

        entries.push_back(TrigramEntry{.code = code, .symbols = std::move(syms)});
    }

    const std::uint64_t unique_trigrams = entries.size();
    const std::uint64_t capacity = capacity_for_count(unique_trigrams);
    const auto capacity_bits = static_cast<std::uint32_t>(std::countr_zero(capacity));

    std::vector<TrigramSlot> trigram_slots(
        capacity,
        TrigramSlot{.trigram_code = TRIGRAM_EMPTY_SLOT, .posting_offset = 0, .posting_count = 0});

    const auto total_postings_length{static_cast<std::uint32_t>(total_postings_count * sizeof(std::uint32_t))};
    std::vector<std::uint32_t> postings;
    postings.reserve(total_postings_count);

    // Create trigram slots (index list) and append symbols to postings list.
    for (const auto &ent : entries)
    {
        std::uint64_t idx = trigram_home_slot(ent.code, capacity_bits);
        while (trigram_slots[idx].trigram_code != TRIGRAM_EMPTY_SLOT)
        {
            idx = (idx + 1) & (capacity - 1);
        }

        trigram_slots[idx].trigram_code = ent.code;
        trigram_slots[idx].posting_offset = static_cast<std::uint32_t>(postings.size());
        trigram_slots[idx].posting_count = static_cast<std::uint32_t>(ent.symbols.size());
        postings.insert(postings.end(), ent.symbols.begin(), ent.symbols.end());
    }

    // Create trigram slot block (serialize trigram slot list)
    ByteArray trigram_index_block;
    for (const auto &trigram_slot : trigram_slots)
    {
        append_object(trigram_index_block, trigram_slot);
    }

    // Compress trigram index block (WITHOUT DICTIONARY)
    std::string compressed_out{};
    if (!zstd_compressor.compress(trigram_index_block.data(), trigram_index_block.size(), compressed_out, no_dict))
    {
        return {};
    }

    tg_hdr.trigram_index_compressed_offset = write_offset;
    tg_hdr.trigram_index_compressed_length = compressed_out.length();
    tg_hdr.trigram_index_uncompressed_length = trigram_index_block.size();
    tg_hdr.trigram_index_capacity = capacity;

    // Write compressed trigram index block to database
    if (!Database::write_database_buffer(db,
                                         compressed_out.data(),
                                         compressed_out.length(),
                                         write_offset,
                                         err_msg))
    {
        return {};
    }

    // Compress trigram posting list (WITHOUT DICTIONARY)
    compressed_out.clear();
    if (!zstd_compressor.compress(postings.data(), total_postings_length, compressed_out, no_dict))
    {
        return {};
    }

    tg_hdr.trigram_postings_compressed_offset = write_offset;
    tg_hdr.trigram_postings_compressed_length = compressed_out.length();
    tg_hdr.trigram_postings_uncompressed_length = total_postings_length;
    tg_hdr.trigram_postings_count = total_postings_count;

    // Write compressed trigram posting list to database
    if (!Database::write_database_buffer(db, compressed_out.data(), compressed_out.length(), write_offset))
    {
        return {};
    }

    // Fill stats
    tg_hdr.symbols_count = tg_builder.symbol_count();
    tg_hdr.trigrams_count = tg_builder.unique_trigram_count();

    return {tg_hdr};
}

} // namespace SymFind
