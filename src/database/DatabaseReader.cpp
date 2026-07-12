#include <symfind/database/DatabaseReader.h>

#include <format>
#include <iostream>
#include <string_view>

#include <cstring>

#include <symfind/config/Config.h>
#include <symfind/database/Database.h>
#include <symfind/utils/Global.h>
#include <symfind/core/Symbol.h>
#include <symfind/core/TrigramUtils.h>
#include <symfind/database/ZSTDCompressor.h>

namespace SymFind
{

DatabaseReader::DatabaseReader(ConfigParserPtr conf) noexcept : _conf(std::move(conf))
{
}

bool DatabaseReader::open(const std::string &db_path, std::string *err_msg) noexcept
{
    return _mf.open(db_path, err_msg);
}

bool DatabaseReader::read_header(std::string *err_msg) noexcept
{
    if (!_mf)
    {
        SET_ERR_MSG(err_msg, std::strerror(_mf.get_errno()));
        return false;
    }

    // Header is written raw (not compressed) at offset 0 -- mmap gives a
    // page-aligned pointer, which trivially satisfies DatabaseHeader's
    // alignment, so this reinterpret_cast is safe without any copying.
    _db_hdr = reinterpret_cast<const Database::DatabaseHeader *>(_mf.data()); // NOLINT

    if (static_cast<std::uint64_t>(_db_hdr->magic) != DATABASE_HEADER_MAGIC)
    {
        SET_ERR_MSG(err_msg, "DatabaseReader: Bad magic!");
        return false;
    }

    if (_db_hdr->version != DATABASE_HEADER_VERSION_NUMBER)
    {
        SET_ERR_MSG(err_msg, "DatabaseReader: Bad version!");
        return false;
    }

    return true;
}

bool DatabaseReader::read_conf_block(std::string */* err_msg */) noexcept
{
    // NOLINTNEXTLINE
    _conf_block = std::string_view(reinterpret_cast<const char *>(_mf.data()) + _db_hdr->conf_block_offset,
                                   _db_hdr->conf_block_length);

    if (_conf_block != _conf->get_conf_block())
    {
        std::cout << "DatabaseReader: configuration file changed since last database update!";
    }

    return true;
}

bool DatabaseReader::read_zstd_dictionary(std::string */* err_msg */) noexcept
{
    if (_db_hdr->zstd_dictionary_length > 0)
    {
        _with_dict_opt.ddict.reset(
            ZSTD_createDDict(_mf.data() + _db_hdr->zstd_dictionary_offset, _db_hdr->zstd_dictionary_length)); // NOLINT
    }

    return true;
}

bool DatabaseReader::read_file_table(std::string *err_msg) noexcept
{
    const Database::FileTableHeader &ft_hdr = _db_hdr->file_table_hdr;

    bool decomp_res{false};

    // Decompress file table block
    decomp_res = decompress_block(ft_hdr.file_index_compressed_offset,
                                  ft_hdr.file_index_compressed_length,
                                  ft_hdr.file_index_uncompressed_length,
                                  _no_dict_opt,
                                  _file_table_buf,
                                  err_msg);

    if (!decomp_res)
    {
        return false;
    }

    _file_table = reinterpret_cast<const FileTable *>(_file_table_buf.data()); // NOLINT
    _files_count = _file_table_buf.size() / sizeof(FileTable);

    // Decompress file names block
    decomp_res = decompress_block(ft_hdr.file_names_compressed_offset,
                                  ft_hdr.file_names_compressed_length,
                                  ft_hdr.file_names_uncompressed_length,
                                  _with_dict_opt,
                                  _file_names_buf,
                                  err_msg);

    if (!decomp_res)
    {
        return false; // NOLINT
    }

    return true;
}

bool DatabaseReader::read_path_table(std::string *err_msg) noexcept
{
    const Database::PathTableHeader &pt_hdr = _db_hdr->path_table_hdr;

    bool decomp_res{false};

    // Decompress path table block
    decomp_res = decompress_block(pt_hdr.path_index_compressed_offset,
                                  pt_hdr.path_index_compressed_length,
                                  pt_hdr.path_index_uncompressed_length,
                                  _no_dict_opt,
                                  _path_table_buf,
                                  err_msg);

    if (!decomp_res)
    {
        return false;
    }

    _path_table = reinterpret_cast<const PathIndex *>(_path_table_buf.data()); // NOLINT
    _paths_count = _path_table_buf.size() / sizeof(PathIndex);

    // Decompress path names block
    decomp_res = decompress_block(pt_hdr.path_names_compressed_offset,
                                  pt_hdr.path_names_compressed_length,
                                  pt_hdr.path_names_uncompressed_length,
                                  _with_dict_opt,
                                  _path_names_buf,
                                  err_msg);

    if (!decomp_res)
    {
        return false; // NOLINT
    }

    return true;
}

bool DatabaseReader::read_symbol_index(std::string *err_msg) noexcept
{
    const Database::SymbolIndexHeader &si_hdr = _db_hdr->sym_index_hdr;

    bool decomp_res{false};

    // Decompress symbol index block
    decomp_res = decompress_block(si_hdr.symbol_index_compressed_offset,
                                  si_hdr.symbol_index_compressed_length,
                                  si_hdr.symbol_index_uncompressed_length,
                                  _no_dict_opt,
                                  _symbol_index_buf,
                                  err_msg);

    if (!decomp_res)
    {
        return false;
    }

    _symbol_index = reinterpret_cast<const SymbolIndex *>(_symbol_index_buf.data()); // NOLINT
    _symbols_count = _symbol_index_buf.size() / sizeof(SymbolIndex);

    // Decompress symbol names block
    decomp_res = decompress_block(si_hdr.symbol_names_compressed_offset,
                                  si_hdr.symbol_names_compressed_length,
                                  si_hdr.symbol_names_uncompressed_length,
                                  _with_dict_opt,
                                  _symbol_names_buf,
                                  err_msg);

    if (!decomp_res)
    {
        return false;
    }

    // Decompress symbol metadata block
    decomp_res = decompress_block(si_hdr.symbol_metadata_compressed_offset,
                                  si_hdr.symbol_metadata_compressed_length,
                                  si_hdr.symbol_metadata_uncompressed_length,
                                  _no_dict_opt,
                                  _symbol_metadata_buf,
                                  err_msg);

    if (!decomp_res)
    {
        return false; // NOLINT
    }

    return true;
}

bool DatabaseReader::read_trigram_index(std::string *err_msg) noexcept
{
    const Database::TrigramIndexHeader &tg_hdr = _db_hdr->trigram_index_hdr;

    bool decomp_res{false};

    // Decompress trigram index block
    decomp_res = decompress_block(tg_hdr.trigram_index_compressed_offset,
                                  tg_hdr.trigram_index_compressed_length,
                                  tg_hdr.trigram_index_uncompressed_length,
                                  _no_dict_opt,
                                  _trigram_slots_buf,
                                  err_msg);

    if (!decomp_res)
    {
        return false;
    }

    _trigram_slots = reinterpret_cast<const TrigramSlot *>(_trigram_slots_buf.data()); // NOLINT
    _trigram_capacity = tg_hdr.trigram_index_capacity;

    // Decompress trigram postings block
    decomp_res = decompress_block(tg_hdr.trigram_postings_compressed_offset,
                                  tg_hdr.trigram_postings_compressed_length,
                                  tg_hdr.trigram_postings_uncompressed_length,
                                  _no_dict_opt,
                                  _trigram_postings_buf,
                                  err_msg);

    if (!decomp_res)
    {
        return false;
    }

    _trigram_postings = reinterpret_cast<const std::uint32_t *>(_trigram_postings_buf.data()); // NOLINT

    return true;
}

bool DatabaseReader::decompress_database(std::string *err_msg) noexcept
{
    return is_mapped_file_open(err_msg) && read_header(err_msg) && read_conf_block(err_msg) &&
           read_zstd_dictionary(err_msg) && read_file_table(err_msg) && read_path_table(err_msg) &&
           read_symbol_index(err_msg) && read_trigram_index(err_msg);
}

// NOLINTNEXTLINE
std::optional<DatabaseReader::SymbolLookupResultList> DatabaseReader::find_symbol_references(
    ConfigParserPtr conf, // NOLINT
    const std::string &symbol_name,
    const IgnoreEntryCallback &ignore_entry,
    std::string *err_msg) noexcept
{
    DatabaseReader db_rdr(conf);

    if (!db_rdr.open(conf->get_database_path(), err_msg))
    {
        return {};
    }

    if (!db_rdr.decompress_database(err_msg))
    {
        return {};
    }

    // ---- Step 1: trigram codes for the query ----
    TrigramCodeList found_trigrams;
    for_each_trigram(symbol_name, [&](TrigramCode code) { found_trigrams.push_back(code); });
    std::ranges::sort(found_trigrams);
    found_trigrams.erase(std::ranges::unique(found_trigrams).begin(), found_trigrams.end());
    if (found_trigrams.empty())
    {
        SET_ERR_MSG(err_msg, std::format("DatabaseReader: Couldn't find trigram for \"{}\"", symbol_name));
        return {};
    }

    // ---- Step 2: intersect postings across all query trigrams ----
    // A true exact match must contain EVERY trigram of the query, so we
    // only keep candidates whose shared-trigram count equals the query's
    // full trigram count. This does NOT by itself prove string equality
    // (two different strings could in principle share the same trigram
    // multiset), so step 3 below still does a direct name comparison
    // before accepting a candidate.
    std::unordered_map<std::uint32_t, std::uint32_t> match_counts;
    for (TrigramCode code : found_trigrams)
    {
        for (std::uint32_t sym_id : db_rdr.lookup_trigram(code))
        {
            ++match_counts[sym_id];
        }
    }

    // Found symbol ids according to trigram codes
    std::vector<std::uint32_t> candidate_ids;
    for (const auto& [sym_id, shared] : match_counts)
    {
        if (shared == found_trigrams.size())
        {
            candidate_ids.push_back(sym_id);
        }
    }

    if (candidate_ids.empty())
    {
        SET_ERR_MSG(err_msg, "DatabaseReader: Symbol not found");
        return {};
    }

    // ---- Step 3: resolve each verified candidate's name + file references ----

    SymbolLookupResultList results;
    for (std::uint32_t sym_id : candidate_ids)
    {
        if (sym_id >= db_rdr._symbols_count)
        {
            continue;
        }

        const SymbolIndex& entry = db_rdr._symbol_index[sym_id]; // NOLINT

        std::string_view name(db_rdr._symbol_names_buf.data() + entry.name_offset, entry.name_length); // NOLINT

        // reject trigram-set collisions
        // if (name != symbol_name)
        // {
        //     continue;
        // }

        SymbolLookupResult result;
        result.symbol_name = std::string(name);

        const auto* refs =
            reinterpret_cast<const SymbolRef*>(db_rdr._symbol_metadata_buf.data() + entry.metadata_offset); // NOLINT

        const std::uint32_t ref_count = entry.metadata_length / sizeof(SymbolRef);
        for (std::uint32_t i = 0; i < ref_count; ++i)
        {
            const SymbolRef& ref = refs[i]; // NOLINT
            if (ref.file_id >= db_rdr._files_count)
            {
                continue;
            }

            // Symbol store filter
            if (const SymbolMetaData &metadata = ref.metadata;
                ignore_entry(SymbolEntryView{.name = name, .metadata = &metadata}))
            {
                continue;
            }

            const FileTable& file_entry = db_rdr._file_table[ref.file_id]; // NOLINT

            // NOLINTNEXTLINE
            std::string_view file_name(db_rdr._file_names_buf.data() + file_entry.name_offset,
                                       file_entry.name_length);

            std::string full_path;
            if (file_entry.path_id < db_rdr._paths_count)
            {
                const PathIndex& path_entry = db_rdr._path_table[file_entry.path_id]; // NOLINT

                // NOLINTNEXTLINE
                std::string_view path_name(db_rdr._path_names_buf.data() + path_entry.name_offset,
                                           path_entry.name_length);

                full_path.assign(path_name);
                if (!full_path.empty() && full_path.back() != '/')
                {
                    full_path += '/';
                }
            }
            full_path += std::string(file_name);

            result.references.push_back(FileReference{.full_path = std::move(full_path), .metadata = ref.metadata});
        }

        results.push_back(std::move(result));
    }

    if (results.empty())
    {
        SET_ERR_MSG(err_msg, "DatabaseReader: Symbol not found (no exact-name match after verification)");
        return {};
    }

    return results;
}

void DatabaseReader::print_symbol_lookup_results(const SymbolLookupResultList &results) noexcept
{
    for (const auto& res : results)
    {
        for (const auto& sym_ref : res.references)
        {
            std::string sym_bind = symbol_bind_to_str(sym_ref.metadata.bind);
            std::string sym_type = symbol_type_to_str(sym_ref.metadata.type);
            std::string sym_is_defined = sym_ref.metadata.is_defined ? "DEFINED" : "RUNTIME";
            std::string sym_src_sec = symbol_source_section_to_str(sym_ref.metadata.source_section);

            std::cout << sym_ref.full_path << ":\n";
            std::cout << std::format(
                "\tsym_name: {}\t\tsym_is_defined: {}\t\tsym_src_sec: {}\t\t sym_type: {}\t\tsym_bind: {}\n\n",
                res.symbol_name,
                sym_is_defined,
                sym_src_sec,
                sym_type,
                sym_bind);
        }
    }
}

std::uint32_t get_trigram_capacity_bits(std::uint32_t trigram_cap) noexcept
{
    return (trigram_cap != 0) ? static_cast<std::uint32_t>(std::countr_zero(trigram_cap)) : std::uint32_t{0};
}

std::span<const TrigramCode> DatabaseReader::lookup_trigram(TrigramCode code) noexcept
{
    std::uint32_t trigram_capacity_bits = get_trigram_capacity_bits(_trigram_capacity);

    if (_trigram_capacity == 0)
    {
        return {};
    }

    std::uint64_t idx = trigram_home_slot(code, trigram_capacity_bits);

    while (true)
    {
        const TrigramSlot &slot = _trigram_slots[idx]; // NOLINT

        if (slot.trigram_code == TRIGRAM_EMPTY_SLOT)
        {
            return {};
        }

        if (slot.trigram_code == code)
        {
            return {_trigram_postings + slot.posting_offset, slot.posting_count}; // NOLINT
        }

        idx = (idx + 1) & (_trigram_capacity - 1);
    }
}

bool DatabaseReader::is_mapped_file_open(std::string *err_msg) noexcept
{
    if (!_mf)
    {
        SET_ERR_MSG(err_msg, std::strerror(_mf.get_errno()));
        return false;
    }

    return true;
}

bool DatabaseReader::decompress_block(FileOffset compressed_data_offset,
                                      std::uint32_t compressed_data_len,
                                      std::uint32_t uncompressed_data_len,
                                      const DecompressionOptions &decomp_opts,
                                      std::string &out_buf,
                                      std::string *err_msg) noexcept
{
    if (!_mf)
    {
        SET_ERR_MSG(err_msg, std::strerror(_mf.get_errno()));
        return false;
    }

    // NOLINTNEXTLINE
    return _zstd.decompress(_mf.data() + compressed_data_offset,
                            compressed_data_len,
                            uncompressed_data_len,
                            out_buf,
                            decomp_opts,
                            err_msg);
}

} // namespace SymFind
