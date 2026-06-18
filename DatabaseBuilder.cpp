#include "DatabaseBuilder.h"

#include <cstdint>
#include <string_view>
#include <system_error>

#include "Config.h"
#include "Database.h"
#include "ElfParser.h"
#include "FSScanner.h"
#include "FileWrapper.h"
#include "Threading.h"
#include "Global.h"

#define MAX_SYMBOLS_COUNT_IN_ELF_FILE 60'000

#define FILE_FULL_PATH_ALLOCATION_LENGTH 2048

namespace SymFind
{

// functions to add:
//      1. check existing db
//      2. multi-thread file-system search with considering not changed ELF files.

void worker(const FileIDList &file_ids, const FSScanner::FileList &found_files, HashMap &symtable, bool verbose)
{
    SymbolEntries parsed_symbol_entries;
    parsed_symbol_entries.reserve(MAX_SYMBOLS_COUNT_IN_ELF_FILE);
    std::string file_path(FILE_FULL_PATH_ALLOCATION_LENGTH, '\0');

    for (auto file_id : file_ids)
    {
        const FSScanner::FileInfo &file_info = found_files[file_id];
        file_path = FSScanner::get_file_info_full_path(file_info);

        std::string err_msg;
        if (!parse_symtables(file_path, parsed_symbol_entries, ACCEPT_ALL_SYMBOLS, &err_msg))
        {
            if (verbose)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                fprintf(stderr, "Parsing \"%s\" file failed: %s", file_path.c_str(), err_msg.c_str());
            }
            continue;
        }

        // Insert the parsed symbols to symbol table (this process will ruin "parsed_symbol_entries" list)
        for (auto &sym : parsed_symbol_entries)
        {
            HashMapAccessor acc;
            symtable.insert(acc, sym.name); // always gives valid accessor

            SymbolRef ref{.file_id = file_id, .metadata = std::move(sym.metadata)};
            acc->second.emplace_back(std::move(ref));
        }

        parsed_symbol_entries.clear();
    }
}

// TODO: This function has lots of duplication with SymFinder::launch_threads function. Separate file-id distribution logic and use in both.
void DatabaseBuilder::launch_threads(ThreadList &thread_list, WorkerType worker)
{
    const FSScanner::FileList &found_files = _fsscanner.get_found_files();

    const std::size_t N = found_files.size();
    const std::size_t T = thread_list.capacity();

    if (T == 0)
    {
        std::cerr << "Thread list is not reserved!\n";
        exit(EXIT_FAILURE);
    }

    std::size_t base = N / T;
    std::size_t remainder = N % T;

    std::uint32_t begin_index{0};
    std::uint32_t end_index{0};

    for (std::size_t i{0}; i < T; ++i)
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
                                        std::ref(_symtable),
                                        _conf->get_debug_pruning()));

        begin_index = end_index;
    }
}

DatabaseBuilder::DatabaseBuilder(ConfigParserPtr conf, const FSScanner &fsscanner) noexcept
    : _conf(std::move(conf)), _fsscanner(fsscanner)
{
}

bool DatabaseBuilder::extract_existing_db(std::string* err_msg) noexcept
{
    return Database::read_database(_existing_db, _conf->get_database_path(), err_msg);
}

bool DatabaseBuilder::build(std::string* err_msg) noexcept
{
    // Using existing database to rebuild database (if exists)
    bool has_existing_db = extract_existing_db();
    (void)has_existing_db;
    // TODO: Use existing database's data for rebuilding database.

    // (Re)building database
    const std::size_t found_files_count = _fsscanner.get_found_files().size();

    // Get standard number of threads which this hardware handles
    std::uint16_t THREADS_COUNT = get_proper_thread_count_to_process_list(found_files_count);

    // Create thread list
    ThreadList thread_list;
    thread_list.reserve(THREADS_COUNT);

    // Initialize threads and feed them to process
    launch_threads(thread_list, worker);

    return true;
}

} // namespace SymFind
