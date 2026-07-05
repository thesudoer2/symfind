#include "DatabaseBuilder.h"

#include <cstddef>
#include <iostream>

#include <cstdint>

#include <zstd.h>

#include "Config.h"
#include "Database.h"
#include "DatabaseBuilderUtils.h"
#include "DictionaryBuilder.h"
#include "ZSTDCompressor.h"
#include "ElfParser.h"
#include "FSScanner.h"
#include "FileWrapper.h"
#include "Threading.h"
#include "Global.h"
#include "ZSTDDictionary.h"

#define MAX_SYMBOLS_COUNT_IN_ELF_FILE 60'000

#define MAX_SYMBOL_NAME_SAMPLES 100'000

#define FILE_FULL_PATH_ALLOCATION_LENGTH 2048

namespace SymFind
{

void worker(const FileIDList &file_ids, const FSScanner::FileList &found_files, HashMap &symtable, bool verbose)
{
    std::string file_path(FILE_FULL_PATH_ALLOCATION_LENGTH, '\0');

    FileID file_id = 0;

    TryStoreSymbolCallback try_store_symbol_callback = [&symtable, &file_id](SymbolEntry &&syment) -> void {
        SymbolRef ref{.file_id = file_id, .metadata = syment.metadata};

        auto [iter, inserted] = symtable.try_emplace(std::move(syment.name));
        iter->second.push_back(ref);
    };

    for (size_t idx = 0; idx < file_ids.size(); ++idx)
    {
        file_id = file_ids[idx];

        const FSScanner::FileInfo &file_info = found_files[file_id];
        file_path = FSScanner::get_file_info_full_path(file_info);

        std::string err_msg;
        if (!parse_symtables(file_path, try_store_symbol_callback, ACCEPT_ALL_SYMBOLS, &err_msg))
        {
            if (verbose)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                fprintf(stderr, "Parsing \"%s\" file failed: %s", file_path.c_str(), err_msg.c_str());
            }
            continue;
        }
    }
}

// TODO: This function has lots of duplication with SymFinder::launch_threads function. Separate file-id distribution logic and use in both.
void DatabaseBuilder::launch_threads(WorkerType worker,
                                     const FSScanner &fsscanner,
                                     HashMapList &symtables,
                                     std::uint16_t thread_count,
                                     bool show_debug_messages)
{
    // Create thread list
    ThreadList thread_list;
    thread_list.reserve(thread_count);

    const FSScanner::FileList &found_files = fsscanner.get_found_files();

    const std::size_t file_count = found_files.size();

    if (thread_count == 0)
    {
        std::cerr << "Thread list is not reserved!\n";
        exit(EXIT_FAILURE);
    }

    std::size_t base = file_count / thread_count;
    std::size_t remainder = file_count % thread_count;

    std::uint32_t begin_index{0};
    std::uint32_t end_index{0};

    for (std::size_t i{0}; i < thread_count; ++i)
    {
        std::size_t chunk_size = base + (i < remainder ? 1 : 0);

        end_index = begin_index + chunk_size;

        FileIDList file_ids;
        file_ids.reserve(chunk_size);
        for (std::uint32_t j = begin_index; j < end_index; ++j)
        {
            file_ids.push_back(j);
        }

        // TODO: Use a better parameter or config to determine verbosity!
        thread_list.emplace_back(Thread(worker,
                                        std::move(file_ids),
                                        std::ref(found_files),
                                        std::ref(symtables[i]),
                                        show_debug_messages));

        begin_index = end_index;
    }
}

bool DatabaseBuilder::store_db(const FSScanner &fsscanner,
                               ConfigParserPtr conf,
                               const HashMap &symtable,
                               DictionaryBuilderPtr dict_builder,
                               const std::string &db_path,
                               std::string *err_msg) noexcept
{
    Database db(db_path); // NOLINT

    if (!db)
    {
        SET_ERR_MSG(err_msg, std::strerror(db.get_errno()));
        return false;
    }

    ZSTDCompressor zstd_compressor;

    CompressionOptions with_dict{.cdict = nullptr};

    FileWrapper::Offset_t ftell{0};

    // Write header placeholder (not actual header)
    Database::DatabaseHeader db_hdr;
    if (!Database::write_database_buffer(db, &db_hdr, sizeof(Database::DatabaseHeader), ftell))
    {
        return false;
    }

    std::string dictionary;
    if (dict_builder)
    {
        dict_builder->train(dictionary);
        with_dict.cdict = DictionaryBuilder::create_compression_dictionary(dictionary.data(), dictionary.size());

        // Write ZSTD dictionary
        db_hdr.zstd_dictionary_offset = ftell;
        db_hdr.zstd_dictionary_length = dictionary.length();

        if (!Database::write_database_buffer(db, dictionary.data(), dictionary.size(), ftell, err_msg))
        {
            return false;
        }
    }

    // Write conf block
    const std::string &conf_block_data = conf->get_conf_block();
    if (!Database::write_database_buffer(db, conf_block_data.data(), conf_block_data.size(), ftell))
    {
        return false;
    }

    // Write file table
    std::optional<Database::FileTableHeader> ft_hdr =
        write_file_table(db, ftell, fsscanner, zstd_compressor, with_dict, err_msg);
    if (!ft_hdr.has_value())
    {
        return false;
    }
    db_hdr.file_table_hdr = ft_hdr.value();

    // Write path id list
    const auto &path_list = FSScanner::_found_files_paths_cache._id_to_str;
    std::optional<Database::PathTableHeader> pt_hdr =
        write_path_table(db, ftell, path_list, zstd_compressor, with_dict, err_msg);
    if (!pt_hdr.has_value())
    {
        return false;
    }
    db_hdr.path_table_hdr = pt_hdr.value();

    // Write symbol index
    std::optional<Database::SymbolIndexHeader> si_hdr =
        write_symbol_index(db, ftell, symtable, zstd_compressor, with_dict, err_msg);
    if (!si_hdr.has_value())
    {
        return false;
    }
    db_hdr.sym_index_hdr = si_hdr.value();

    // Write trigram index
    std::optional<Database::TrigramIndexHeader> tg_hdr =
        write_trigram_index(db, ftell, symtable, zstd_compressor, err_msg);
    if (!tg_hdr.has_value())
    {
        return false;
    }
    db_hdr.trigram_index_hdr = tg_hdr.value();

    // Rewind to header and update it (rewrite it)
    ftell = 0;
    if (!Database::write_database_buffer(db, &db_hdr, sizeof(Database::DatabaseHeader), ftell))
    {
        return false; // NOLINT
    }

    return true;
}

DatabaseBuilder::DatabaseBuilder(ConfigParserPtr conf, const FSScanner &fsscanner, DictionaryBuilderPtr dict_builder_ptr) noexcept
    : _conf(std::move(conf)), _fsscanner(fsscanner), _existing_db(_conf->get_database_path()), _dict_builder_ptr(std::move(dict_builder_ptr))
{
}

bool DatabaseBuilder::extract_existing_db(std::string* err_msg) noexcept
{
    return Database::read_database(_existing_db, err_msg);
}

void DatabaseBuilder::merge_symtables(HashMapList &symtables, HashMap &merged_symtable) noexcept
{
    for (auto &symtable : symtables)
    {
        for (auto &[sym_name, sym_refs] : symtable)
        {
            auto [iter, inserted] = merged_symtable.try_emplace(std::move(sym_name));

            if (inserted) // New element inserted
            {
                // Add symbol name sample to dictionary if exists.
                if (_dict_builder_ptr && ++symbol_name_samples <= MAX_SYMBOL_NAME_SAMPLES)
                {
                    _dict_builder_ptr->add_sample(iter->first);
                }

                iter->second = std::move(sym_refs);
            }
            else // The key already existed
            {
                for (const auto &sym_ref : sym_refs)
                {
                    iter->second.push_back(sym_ref);
                }
            }
        }
    }
}

bool DatabaseBuilder::build(std::string* err_msg) noexcept
{
    // Using existing database to rebuild database (if exists)
    // bool has_existing_db = extract_existing_db(err_msg);
    // (void)has_existing_db;
    // TODO: Use existing database's data for rebuilding database.
    // TODO: Do not parse ELF files that not changed from last database building.

    // (Re)building database
    const std::size_t found_files_count = _fsscanner.get_found_files().size();

    // Get standard number of threads which this hardware handles
    std::uint16_t THREADS_COUNT = get_proper_thread_count_to_process_list(found_files_count);

    // Allocate symtable for each thread
    HashMapList symtables;
    symtables.resize(THREADS_COUNT);

    // Initialize threads and feed them to process
    launch_threads(worker, _fsscanner, symtables, THREADS_COUNT, _conf->get_debug_pruning());

    // Merge symbol tables
    HashMap merged_symtable;
    merge_symtables(symtables, merged_symtable);

    // Write and store database
    return store_db(_fsscanner, _conf, merged_symtable, _dict_builder_ptr, _conf->get_database_path(), err_msg);
}

} // namespace SymFind
