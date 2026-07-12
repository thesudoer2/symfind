#pragma once

#include <filesystem>
#include <iostream>

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

enum class SymbolDefinitionToPrint : std::uint8_t
{
    UNKNOWN = 0,
    ONLY_DEFINED,
    ONLY_RUNTIME,
    SHOW_BOTH,
};

enum RunningMethod : uint8_t
{
    NOT_SET = 0x00,
    BUILD_DB = 0x02,
    READ_DB = 0x04,
    FREE_RUN = 0x08,
};

struct ProgramOptions
{
    RunningMethod running_method = RunningMethod::NOT_SET;
    StringComparatorType search_type = StringComparatorType::DEFAULT;
    SymbolDefinitionToPrint visibility = SymbolDefinitionToPrint::ONLY_DEFINED;
    std::filesystem::path scan_root_path = "/";
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
