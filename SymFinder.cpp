#include "SymFinder.h"

#include <format>
#include <functional>
#include <memory>
#include <thread>
#include <vector>
#include <regex>

#include "ElfParser.h"
#include "FSScanner.h"
#include "HWHelper.h"
#include "StringComparator.h"
#include "Symbol.h"
#include "Threading.h"

#if 0
#define HARDWARE_CONCURRENCY_COUNT SymFind::get_hardware_concurrency()
#else // Debug
#define HARDWARE_CONCURRENCY_COUNT 1
#endif

#define MAX_MATCHED_SYMBOLS_COUNTS 10

namespace SymFind
{

void worker(const FileIDList &file_ids, const FSScanner::FileList &found_files, IgnoreSymbolCallback ignore_symbol, bool verbose)
{
    SymbolEntries parsed_symbol_entries;
    parsed_symbol_entries.reserve(MAX_MATCHED_SYMBOLS_COUNTS);

    for (auto file_id : file_ids)
    {
        const FSScanner::FileInfo &file_info = found_files[file_id];
        std::string file_path = FSScanner::get_file_info_full_path(file_info);

        std::string err_msg;
        if (!parse_symtables(file_path, parsed_symbol_entries, ignore_symbol, &err_msg))
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
            // TODO: Take argument to show only "DEFINED" symbols or all symbols.
            // if (sym.name == target_sym_name && sym.metadata->is_defined)
            {
                std::string sym_bind = SymFind::symbol_bind_to_str(sym.metadata->bind);
                std::string sym_type = SymFind::symbol_type_to_str(sym.metadata->type);
                std::string sym_is_defined = sym.metadata->is_defined ? "DEFINED" : "RUNTIME";
                std::string sym_src_sec = SymFind::symbol_source_section_to_str(sym.metadata->source_section);

                std::cout << file_path << ":\n";
                std::cout << std::format(
                    "\tsym_name: {}\t\tsym_is_defined: {}\t\tsym_src_sec: {}\t\t sym_type: {}\t\tsym_bind: {}\n\n",
                    sym.name,
                    sym_is_defined,
                    sym_src_sec,
                    sym_type,
                    sym_bind);
            }
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

SymFinder::SymFinder(ConfigParserPtr conf, const FSScanner &fsscanner, IgnoreSymbolCallback ignore_symbol) noexcept
    : _conf(std::move(conf)), _fsscanner(fsscanner)
{
    const std::size_t found_files_count = _fsscanner.get_found_files().size();

    // Get standard number of threads which this hardware handles
    std::uint16_t THREADS_COUNT = get_proper_thread_count_to_process_list(found_files_count);

    // Create thread list
    ThreadList thread_list;
    thread_list.reserve(THREADS_COUNT);

    // Initialize threads and feed them to process
    launch_threads(thread_list, worker, std::move(ignore_symbol));
}

} // namespace SymFind
