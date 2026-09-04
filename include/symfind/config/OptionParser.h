#pragma once

#include <filesystem>

#include <cstdint>

#include <symfind/core/StringComparator.h>
#include <symfind/core/Symbol.h>

#define RUNNING_METHOD_BUILD_DB "build_db"
#define RUNNING_METHOD_READ_DB "read_db"
#define RUNNING_METHOD_FREE_RUN "free_run"

#define RUNNING_METHODS RUNNING_METHOD_BUILD_DB "|" RUNNING_METHOD_READ_DB "|" RUNNING_METHOD_FREE_RUN


#define SEARCH_TYPE_DEFAULT "default"
#define SEARCH_TYPE_REGEX "regex"
#define SEARCH_TYPE_FUZZY "fuzzy"

#define SEARCH_TYPES SEARCH_TYPE_DEFAULT "|" SEARCH_TYPE_REGEX "|" SEARCH_TYPE_FUZZY

// static bool use_debug = false; // NOLINT

namespace SymFind
{

enum SymbolDefinitionToPrint : std::uint8_t
{
    SymbolDefinitionToPrint_NOT_SET = 0x00,
    SymbolDefinitionToPrint_ONLYDEFINED = 0x01,
    SymbolDefinitionToPrint_ONLYRUNTIME = 0x02,
    SymbolDefinitionToPrint_SHOWFULL = 0x04,
    SymbolDefinitionToPrint_UNKNOWN = 0x08,
};

enum RunningMethod : uint8_t
{
    RunningMethod_NOT_SET = 0x00,
    RunningMethod_BUILD_DB = 0x02,
    RunningMethod_READ_DB = 0x04,
    RunningMethod_FREE_RUN = 0x08,
};

struct ProgramOptions
{
    RunningMethod running_method = RunningMethod_NOT_SET;
    StringComparatorType search_type = StringComparatorType_NOT_SET;
    SymbolDefinitionToPrint visibility = SymbolDefinitionToPrint_NOT_SET;
    std::filesystem::path scan_root_path{};
    std::string symbol;
    bool debug_mode = false;
    bool show_version = false;
    bool print_help = false;
};


// -----------------------------------------------------------------------------
// Implementations
// -----------------------------------------------------------------------------

void show_version(const char *program_name) noexcept;

void print_help(const char *program_name) noexcept;

RunningMethod parse_running_method(const std::string &value) noexcept;

SymbolDefinitionToPrint parse_visibility(const std::string &value) noexcept;

StringComparatorType parse_search_type(const std::string &value) noexcept;

bool parse_arguments(int argc, char **argv, ProgramOptions &options) noexcept;

bool symfinder_symbol_should_be_stored(const SymbolEntry &sym_ent,
                             SymbolDefinitionToPrint def_to_print,
                             const StringComparator &compare_sym_names) noexcept;

bool database_reader_entry_should_be_stored(const SymFind::SymbolEntryView &sym_ent_v,
                                            SymbolDefinitionToPrint def_to_print,
                                            const StringComparator &compare_sym_names) noexcept;

} // namespace SymFind
