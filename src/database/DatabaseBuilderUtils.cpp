#include <symfind/database/DatabaseBuilderUtils.h>

#include <symfind/database/Database.h>
#include <symfind/database/DatabaseWriter.h>
#include <symfind/core/Trigram.h>
#include <symfind/core/TrigramUtils.h>
#include <symfind/database/ZSTDCompressor.h>

namespace SymFind
{

template <typename T>
void append_object(ByteArray &out, const T &obj)
{
    const auto *ptr = reinterpret_cast<const ByteType *>(&obj); // NOLINT
    out.insert(out.end(), ptr, ptr + sizeof(T));                // NOLINT
}

std::optional<FileTableBuildResult> build_file_table(const FSScanner &fsscanner,
                                                     const CompressionOptions &comp_opts, // by value: cheap, and
                                                     std::string *err_msg) noexcept       // avoids any doubt about
{                                                                                         // lifetime across threads
    FileTableBuildResult result;
    ZSTDCompressor zstd_compressor; // OWN CCtx/DCtx -- never shared across threads

    FileWrapper::Offset_t file_name_offset{0};
    const FSScanner::FileList &found_files = fsscanner.get_found_files();

    ByteArray file_index_block;
    std::string file_names_blob;

    for (const auto &file_info : found_files)
    {
        const std::string &filename = file_info.name;
        auto file_name_length = static_cast<decltype(FileTable::name_length)>(filename.size());

        FileTable file_table_entry{.path_id = file_info.path_id,
                                   .name_offset = file_name_offset,
                                   .name_length = file_name_length};
        append_object(file_index_block, file_table_entry);

        file_name_offset += file_name_length;
        file_names_blob += filename;
        result.files_count += 1;
    }

    if (!zstd_compressor.compress(file_index_block.data(),
                                  file_index_block.size(),
                                  result.file_index_compressed,
                                  no_dict,
                                  err_msg))
    {
        return {};
    }
    result.file_index_uncompressed_length = file_index_block.size();

    if (!zstd_compressor.compress(file_names_blob.data(),
                                  file_names_blob.size(),
                                  result.file_names_compressed,
                                  comp_opts,
                                  err_msg))
    {
        return {};
    }
    result.file_names_uncompressed_length = file_names_blob.size();

    return {std::move(result)};
}

std::optional<Database::FileTableHeader> commit_file_table(DatabaseWriter &db, // NOLINT
                                                           FileWrapper::Offset_t &write_offset,
                                                           const FileTableBuildResult &built,
                                                           std::string *err_msg) noexcept
{
    Database::FileTableHeader ft_hdr;
    ft_hdr.files_count = built.files_count;

    ft_hdr.file_index_compressed_offset = write_offset;
    ft_hdr.file_index_compressed_length = built.file_index_compressed.size();
    ft_hdr.file_index_uncompressed_length = built.file_index_uncompressed_length;
    if (!DatabaseWriter::write_database_buffer(db,
                                               built.file_index_compressed.data(),
                                               built.file_index_compressed.size(),
                                               write_offset,
                                               err_msg))
    {
        return {};
    }

    ft_hdr.file_names_compressed_offset = write_offset;
    ft_hdr.file_names_compressed_length = built.file_names_compressed.size();
    ft_hdr.file_names_uncompressed_length = built.file_names_uncompressed_length;
    if (!DatabaseWriter::write_database_buffer(db,
                                               built.file_names_compressed.data(),
                                               built.file_names_compressed.size(),
                                               write_offset,
                                               err_msg))
    {
        return {};
    }

    ft_hdr.total_compressed_length = ft_hdr.file_index_compressed_length + ft_hdr.file_names_compressed_length;
    return {ft_hdr};
}

std::optional<PathTableBuildResult> build_path_table(const FSScanner::StringCache::StringList &path_list,
                                                     const CompressionOptions &comp_opts,
                                                     std::string *err_msg) noexcept
{
    PathTableBuildResult result;
    ZSTDCompressor zstd_compressor;

    std::uint32_t path_name_offset{0};
    ByteArray path_index_block;
    std::string path_names_blob;

    for (const auto &path_name : path_list)
    {
        auto path_name_length = static_cast<decltype(PathIndex::name_length)>(path_name.size());
        PathIndex path_table_entry{.name_offset = path_name_offset, .name_length = path_name_length};
        append_object(path_index_block, path_table_entry);

        path_name_offset += path_name_length;
        path_names_blob += path_name;
        result.paths_count += 1;
    }

    if (!zstd_compressor.compress(path_index_block.data(),
                                  path_index_block.size(),
                                  result.path_index_compressed,
                                  no_dict,
                                  err_msg))
    {
        return {};
    }
    result.path_index_uncompressed_length = path_index_block.size();

    if (!zstd_compressor.compress(path_names_blob.data(),
                                  path_names_blob.size(),
                                  result.path_names_compressed,
                                  comp_opts,
                                  err_msg))
    {
        return {};
    }
    result.path_names_uncompressed_length = path_names_blob.size();

    return {std::move(result)};
}

std::optional<Database::PathTableHeader> commit_path_table(DatabaseWriter &db, // NOLINT
                                                           FileWrapper::Offset_t &write_offset,
                                                           const PathTableBuildResult &built,
                                                           std::string *err_msg) noexcept
{
    Database::PathTableHeader pt_hdr;
    pt_hdr.paths_count = built.paths_count;

    pt_hdr.path_index_compressed_offset = write_offset;
    pt_hdr.path_index_compressed_length = built.path_index_compressed.size();
    pt_hdr.path_index_uncompressed_length = built.path_index_uncompressed_length;
    if (!DatabaseWriter::write_database_buffer(db,
                                               built.path_index_compressed.data(),
                                               built.path_index_compressed.size(),
                                               write_offset,
                                               err_msg))
    {
        return {};
    }

    pt_hdr.path_names_compressed_offset = write_offset;
    pt_hdr.path_names_compressed_length = built.path_names_compressed.size();
    pt_hdr.path_names_uncompressed_length = built.path_names_uncompressed_length;
    if (!DatabaseWriter::write_database_buffer(db,
                                               built.path_names_compressed.data(),
                                               built.path_names_compressed.size(),
                                               write_offset,
                                               err_msg))
    {
        return {};
    }

    pt_hdr.total_compressed_length = pt_hdr.path_index_compressed_length + pt_hdr.path_names_compressed_length;
    return {pt_hdr};
}

std::optional<SymbolIndexBuildResult> build_symbol_index(const HashMap &symtable,
                                                         const CompressionOptions &comp_opts,
                                                         std::string *err_msg) noexcept
{
    SymbolIndexBuildResult result;
    ZSTDCompressor zstd_compressor;

    std::uint32_t sym_name_offset{0};
    std::uint32_t sym_metadata_offset{0};

    ByteArray sym_index_block;
    ByteArray sym_metadata_blob;
    std::string sym_names_blob;

    for (const auto &[sym_name, sym_refs] : symtable)
    {
        auto sym_name_length = static_cast<decltype(SymbolIndex::name_length)>(sym_name.size());
        decltype(SymbolIndex::metadata_length) sym_metadata_length{0};

        for (const auto &sym_ref : sym_refs)
        {
            sym_metadata_length += sizeof(sym_ref);
            append_object(sym_metadata_blob, sym_ref);
        }

        SymbolIndex sym_index_entry{.name_offset = sym_name_offset,
                                    .name_length = sym_name_length,
                                    .metadata_offset = sym_metadata_offset,
                                    .metadata_length = sym_metadata_length};
        append_object(sym_index_block, sym_index_entry);

        sym_name_offset += sym_name_length;
        sym_metadata_offset += sym_metadata_length;

        sym_names_blob += sym_name;
        result.symbols_count += 1;
    }

    if (!zstd_compressor.compress(sym_index_block.data(),
                                  sym_index_block.size(),
                                  result.symbol_index_compressed,
                                  no_dict,
                                  err_msg))
    {
        return {};
    }
    result.symbol_index_uncompressed_length = sym_index_block.size();

    if (!zstd_compressor.compress(sym_names_blob.data(),
                                  sym_names_blob.size(),
                                  result.symbol_names_compressed,
                                  comp_opts,
                                  err_msg))
    {
        return {};
    }
    result.symbol_names_uncompressed_length = sym_names_blob.size();

    if (!zstd_compressor.compress(sym_metadata_blob.data(),
                                  sym_metadata_blob.size(),
                                  result.symbol_metadata_compressed,
                                  no_dict,
                                  err_msg))
    {
        return {};
    }
    result.symbol_metadata_uncompressed_length = sym_metadata_blob.size();

    return {std::move(result)};
}

std::optional<Database::SymbolIndexHeader> commit_symbol_index(DatabaseWriter &db, // NOLINT
                                                               FileWrapper::Offset_t &write_offset,
                                                               const SymbolIndexBuildResult &built,
                                                               std::string *err_msg) noexcept
{
    Database::SymbolIndexHeader si_hdr;
    si_hdr.symbols_count = built.symbols_count;

    si_hdr.symbol_index_compressed_offset = write_offset;
    si_hdr.symbol_index_compressed_length = built.symbol_index_compressed.size();
    si_hdr.symbol_index_uncompressed_length = built.symbol_index_uncompressed_length;
    if (!DatabaseWriter::write_database_buffer(db,
                                               built.symbol_index_compressed.data(),
                                               built.symbol_index_compressed.size(),
                                               write_offset,
                                               err_msg))
    {
        return {};
    }

    si_hdr.symbol_names_compressed_offset = write_offset;
    si_hdr.symbol_names_compressed_length = built.symbol_names_compressed.size();
    si_hdr.symbol_names_uncompressed_length = built.symbol_names_uncompressed_length;
    if (!DatabaseWriter::write_database_buffer(db,
                                               built.symbol_names_compressed.data(),
                                               built.symbol_names_compressed.size(),
                                               write_offset,
                                               err_msg))
    {
        return {};
    }

    si_hdr.symbol_metadata_compressed_offset = write_offset;
    si_hdr.symbol_metadata_compressed_length = built.symbol_metadata_compressed.size();
    si_hdr.symbol_metadata_uncompressed_length = built.symbol_metadata_uncompressed_length;
    if (!DatabaseWriter::write_database_buffer(db,
                                               built.symbol_metadata_compressed.data(),
                                               built.symbol_metadata_compressed.size(),
                                               write_offset,
                                               err_msg))
    {
        return {};
    }

    si_hdr.total_compressed_length = si_hdr.symbol_index_compressed_length + si_hdr.symbol_names_compressed_length +
                                     si_hdr.symbol_metadata_compressed_length;
    return {si_hdr};
}

std::optional<TrigramIndexBuildResult> build_trigram_index(const HashMap &symtable, std::string *err_msg) noexcept
{
    TrigramIndexBuildResult result;
    ZSTDCompressor zstd_compressor;

    TrigramBuilder tg_builder;
    std::uint32_t sym_idx{0};
    for (const auto &[sym_name, sym_refs] : symtable)
    {
        tg_builder.add_word(sym_name, sym_idx++);
    }

    const auto &trigram_to_symbols = tg_builder.get_trigram_to_symbols();

    std::uint64_t total_postings_count{0};
    TrigramEntries entries;
    entries.reserve(trigram_to_symbols.size());

    for (const auto &[code, symbols] : trigram_to_symbols)
    {
        std::vector<std::uint32_t> syms = symbols;
        std::ranges::sort(syms);
        syms.erase(std::ranges::unique(syms).begin(), syms.end());
        total_postings_count += syms.size();
        entries.push_back(TrigramEntry{.code = code, .symbols = std::move(syms)});
    }

    const std::uint64_t unique_trigrams = entries.size();
    const std::uint64_t capacity = trigram_capacity_for_count(unique_trigrams);
    const auto capacity_bits = static_cast<std::uint32_t>(std::countr_zero(capacity));

    std::vector<TrigramSlot> trigram_slots(
        capacity,
        TrigramSlot{.trigram_code = TRIGRAM_EMPTY_SLOT, .posting_offset = 0, .posting_count = 0});

    std::vector<std::uint32_t> postings;
    postings.reserve(total_postings_count);

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

    ByteArray trigram_index_block;
    for (const auto &trigram_slot : trigram_slots)
    {
        append_object(trigram_index_block, trigram_slot);
    }

    if (!zstd_compressor.compress(trigram_index_block.data(),
                                  trigram_index_block.size(),
                                  result.trigram_index_compressed,
                                  no_dict,
                                  err_msg))
    {
        return {};
    }
    result.trigram_index_uncompressed_length = trigram_index_block.size();
    result.trigram_index_capacity = capacity;

    const auto total_postings_length = static_cast<std::uint32_t>(total_postings_count * sizeof(std::uint32_t));
    if (!zstd_compressor
             .compress(postings.data(), total_postings_length, result.trigram_postings_compressed, no_dict, err_msg))
    {
        return {};
    }
    result.trigram_postings_uncompressed_length = total_postings_length;
    result.trigram_postings_count = total_postings_count;

    result.symbols_count = tg_builder.symbol_count();
    result.trigrams_count = tg_builder.unique_trigram_count();

    return {std::move(result)};
}

std::optional<Database::TrigramIndexHeader> commit_trigram_index(DatabaseWriter &db, // NOLINT
                                                                 FileWrapper::Offset_t &write_offset,
                                                                 const TrigramIndexBuildResult &built,
                                                                 std::string *err_msg) noexcept
{
    Database::TrigramIndexHeader tg_hdr{};

    tg_hdr.trigram_index_compressed_offset = write_offset;
    tg_hdr.trigram_index_compressed_length = built.trigram_index_compressed.size();
    tg_hdr.trigram_index_uncompressed_length = built.trigram_index_uncompressed_length;
    tg_hdr.trigram_index_capacity = built.trigram_index_capacity;
    if (!DatabaseWriter::write_database_buffer(db,
                                               built.trigram_index_compressed.data(),
                                               built.trigram_index_compressed.size(),
                                               write_offset,
                                               err_msg))
    {
        return {};
    }

    tg_hdr.trigram_postings_compressed_offset = write_offset;
    tg_hdr.trigram_postings_compressed_length = built.trigram_postings_compressed.size();
    tg_hdr.trigram_postings_uncompressed_length = built.trigram_postings_uncompressed_length;
    tg_hdr.trigram_postings_count = built.trigram_postings_count;
    if (!DatabaseWriter::write_database_buffer(db,
                                               built.trigram_postings_compressed.data(),
                                               built.trigram_postings_compressed.size(),
                                               write_offset,
                                               err_msg))
    {
        return {};
    }

    tg_hdr.symbols_count = built.symbols_count;
    tg_hdr.trigrams_count = built.trigrams_count;

    return {tg_hdr};
}

} // namespace SymFind
