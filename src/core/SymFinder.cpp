#include <symfind/core/SymFinder.h>

#include <format>
#include <memory>

#include <symfind/elf/ElfParser.h>
#include <symfind/filesystem/FSScanner.h>
#include <symfind/core/Symbol.h>
#include <symfind/utils/Threading.h>

#define MAX_MATCHED_SYMBOLS_COUNTS 10

namespace SymFind
{

void worker(const FileIDList &file_ids,
            const FSScanner::FileList &found_files,
            const IgnoreSymbolCallback &ignore_symbol,
            bool verbose)
{
    SymbolEntries parsed_symbol_entries;
    parsed_symbol_entries.reserve(MAX_MATCHED_SYMBOLS_COUNTS);

    TryStoreSymbolCallback try_store_symbol_callback = [&parsed_symbol_entries](SymbolEntry &&syment) -> void {
        parsed_symbol_entries.emplace_back(std::move(syment));
    };

    for (auto file_id : file_ids)
    {
        const FSScanner::FileInfo &file_info = found_files[file_id];
        std::string file_path = FSScanner::get_file_info_full_path(file_info);

        std::string err_msg;
        if (!parse_symtables(file_path, try_store_symbol_callback, ignore_symbol, &err_msg))
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
            // if (sym.name == target_sym_name && sym.metadata.is_defined)
            {
                std::string sym_is_defined = sym.metadata.is_defined ? "DEFINED" : "UNDEFINED";
                std::string sym_bind = SymFind::symbol_bind_to_str(sym.metadata.bind);
                std::string sym_type = SymFind::symbol_type_to_str(sym.metadata.type);
                std::string sym_visibility = SymFind::symbol_visibility_to_str(sym.metadata.visibility);
                std::string sym_src_sec = SymFind::symbol_source_section_to_str(sym.metadata.source_section);

                std::cout << file_path << ":\n";
                std::cout << std::format(
                    "\t{}sym_name:{} {} | {}DEF:{} {} | {}SRC SECTION:{} {} | {}TYPE:{} {} | {}BIND:{} {} | {}VISIBILITY:{} {}\n\n",
                    COLOR_YELLOW,
                    COLOR_RESET,
                    sym.name,
                    COLOR_YELLOW,
                    COLOR_RESET,
                    sym_is_defined,
                    COLOR_YELLOW,
                    COLOR_RESET,
                    sym_src_sec,
                    COLOR_YELLOW,
                    COLOR_RESET,
                    sym_type,
                    COLOR_YELLOW,
                    COLOR_RESET,
                    sym_bind,
                    COLOR_YELLOW,
                    COLOR_RESET,
                    sym_visibility
                );
            }
        }

        parsed_symbol_entries.clear();
    }
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
