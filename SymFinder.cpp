#include "SymFinder.h"

#include <format>
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

enum class SymbolDefinitionToPrint : std::uint8_t
{
    UNKNOWN = 0,
    ONLY_DEFINED,
    ONLY_RUNTIME,
    SHOW_BOTH,
};

static const std::string sym_regexp{"^([0-9a-zA-Z_])+"};

bool symbol_should_be_stored(const SymbolEntry &sym_ent,
                             SymbolDefinitionToPrint def_to_print,
                             const StringComparator &compare_sym_names) noexcept
{
    if (compare_sym_names(sym_ent.name))
    {
        const auto &sym_metadata = sym_ent.metadata;
        if (sym_metadata->is_defined && (def_to_print == SymbolDefinitionToPrint::ONLY_DEFINED ||
                                         def_to_print == SymbolDefinitionToPrint::SHOW_BOTH))
        {
            return true;
        }

        if (sym_metadata->is_defined && (def_to_print == SymbolDefinitionToPrint::ONLY_DEFINED ||
                                         def_to_print == SymbolDefinitionToPrint::SHOW_BOTH))
        {
            return true;
        }
    }

    return false;
}

std::string regex_escape(const std::string& text)
{
    static const std::string special_chars = R"(.^$|()[]{}*+?\/)";
    std::string escaped;
    escaped.reserve(text.size() * 2);  // rough estimate

    for (char c : text)
    {
        if (special_chars.find(c) != std::string::npos)
        {
            escaped += '\\';
        }
        escaped += c;
    }

    return escaped;
}

void worker(const FileIDList &file_ids, const FSScanner::FileList &found_files, bool verbose)
{
    // std::string target_sym_name{"printf"};
    // std::string target_sym_name{"std::__cxx11::messages<char>::_M_convert_from_char(char*) const"};
    // std::string target_sym_name{"std::__cxx11::messages<char>::_M_convert_from_char"};
    // std::string target_sym_name{"_ZNKSt8messagesIcE20_M_convert_from_charEPc"};
    // std::string target_sym_name{"convert_from_char"};
    // std::string target_sym_name{"QBasicMutex::unlockInternalFutex(void*)"};
    // std::string target_sym_name{"_ZN4llvm15SmallVectorBaseIjE8grow_podEPvmm"};
    // std::string target_sym_name{"llvm::SmallVectorBase<unsigned int>::grow_pod"};
    std::string target_sym_name{"llvm::SmallVectorBase<unsigned int>::grow_pod(void*, unsigned long, unsigned long)"};
    // std::string target_sym_name{":grow_pod"};

    // Only for Regex matcher
    // target_sym_name = regex_escape(target_sym_name);

    // std::cout << ">>>>> escaped: \"" << target_sym_name << "\"\n";
    // exit(85);

    // TODO2: Take argument or something to decide using "string_default_comparator" or "string_fuzzy_comparator" or "string_regex_comparator".
    auto string_comparator = make_string_comparator(StringComparatorType::REGEX, target_sym_name);
    // auto string_comparator = make_string_comparator(StringComparatorType::FUZZY, target_sym_name);
    // auto string_comparator = make_string_comparator(StringComparatorType::DEFAULT, target_sym_name);

    auto ignore_symbol = [&string_comparator](const SymbolEntry &sym_ent) -> bool {
        // TODO1: Take argument to show only "DEFINED" symbols or all symbols.
        return !symbol_should_be_stored(sym_ent,
                                        SymbolDefinitionToPrint::ONLY_DEFINED,
                                        *string_comparator);
    };

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

        // >>>>> DEBUGGING
        // std::cout << "parsed_symbol_entries.size(): " << parsed_symbol_entries.size()
        //           << ", parsed_symbol_entries.capacity(): " << parsed_symbol_entries.capacity() << std::endl;
        // <<<<< DEBUGGING
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

SymFinder::SymFinder(ConfigParserPtr conf, const FSScanner &fsscanner) noexcept
    : _conf(std::move(conf)), _fsscanner(fsscanner)
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

} // namespace SymFind
