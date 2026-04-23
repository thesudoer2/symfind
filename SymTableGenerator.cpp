#include "SymTableGenerator.h"

#include <thread>
#include <vector>

#include "FSScanner.h"
#include "HWHelper.h"
#include "Symbol.h"
#include "Threading.h"
#include "ElfParser.h"

#if 1
#define HARDWARE_CONCURRENCY_COUNT get_hardware_concurrency()
#else // Debug
#define HARDWARE_CONCURRENCY_COUNT 1
#endif

#define MAX_SYMBOLS_COUNT 60'000

namespace SymFind
{

void worker(const FileIDList& file_ids, const FSScanner::FileList &found_files, HashMap &symtable, bool verbose)
{
    SymbolEntries parsed_symbol_entries;
    parsed_symbol_entries.reserve(MAX_SYMBOLS_COUNT);

    for (auto file_id : file_ids)
    {
        const FSScanner::FileInfo& file_info = found_files[file_id];
        std::string file_path = FSScanner::get_file_info_full_path(file_info);

        std::string err_msg;
        if (!parse_symtables(file_path, parsed_symbol_entries, &err_msg))
        {
            if (verbose)
            {
                // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg)
                fprintf(stderr, "Parsing \"%s\" file failed: %s\n", file_path.c_str(), err_msg.c_str());
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

std::uint16_t get_proper_thread_count_to_process_list(std::size_t list_size)
{
    static const std::uint16_t THREADS_COUNT = HARDWARE_CONCURRENCY_COUNT;
    static const std::uint32_t MINIMUM_FILES_PER_THREAD = 20;

    auto CURRENT_FILES_PER_THREAD = static_cast<std::uint32_t>(list_size / THREADS_COUNT);

    if (CURRENT_FILES_PER_THREAD >= MINIMUM_FILES_PER_THREAD)
    {
        return THREADS_COUNT;
    }
    return std::max<std::uint16_t>(1, static_cast<std::uint16_t>(list_size / MINIMUM_FILES_PER_THREAD));
}

SymTableGenerator::SymTableGenerator(ConfigParserPtr conf, const FSScanner &fsscanner) noexcept : _conf(std::move(conf)), _fsscanner(fsscanner)
{
    const std::size_t found_files_count = _fsscanner.get_found_files().size();

    // Get standard number of threads which this hardware handles
    std::uint16_t THREADS_COUNT = get_proper_thread_count_to_process_list(found_files_count);

    // Create thread list
    ThreadList thread_list;
    thread_list.reserve(THREADS_COUNT);

    // Initialize threads and feed them to process
    launch_threads(thread_list, worker);
}

SymbolRefsPtr SymTableGenerator::find_sym(const std::string &sym_name) const noexcept
{
    HashMapAccessor acc;
    _symtable.find(acc, sym_name);
    if (acc.empty())
    {
        return nullptr;
    }
    return &acc->second;
}

} // namespace SymFind
