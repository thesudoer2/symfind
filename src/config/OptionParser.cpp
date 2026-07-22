#include <symfind/config/OptionParser.h>

#include <format>
#include <iostream>

#include <symfind/core/StringComparator.h>
#include <symfind/database/Database.h>

namespace SymFind
{

void print_help(const char *program_name) noexcept
{
    std::cout <<
        R"(Usage:
    )" << program_name
              << R"( [options] <symbol>

Options:
    -m, --running-method <)"
              << RUNNING_METHODS << R"(>
        Choose program's running behavior.

        NOTE: You can choose only one running method.
        NOTE: Default behavior of running method is "read_db".

    -r, --regex
        Shortcut for: --search-type regex

    -f, --fuzz
        Shortcut for: --search-type fuzzy

    --search-type <)"
              << SEARCH_TYPES << R"(>
        Defines how symbol matching is performed.

            )" << SEARCH_TYPE_DEFAULT
              << R"( -> exact string comparison (default)
            )" << SEARCH_TYPE_REGEX
              << R"(   -> treat input as regex pattern
            )" << SEARCH_TYPE_FUZZY
              << R"(   -> fuzzy matching

    --symbol-visibility <defined|runtime|both>
        Controls which symbols are shown:

            defined -> only defined symbols (default)
            runtime -> only runtime/imported symbols
            both    -> show both categories

    -s, --scan-path <path>
        Set root directory to search symbols (default is '/').

    -d, --debug
        Be verbose and show more logs.

    -v, --version
        Show version number

    -h, --help
        Show this help message.

Examples:
    )" << program_name
              << R"( malloc

    )" << program_name
              << R"( --search-type regex "std::vector"

    )" << program_name
              << R"( --search-type fuzzy stdcout

    )" << program_name
              << R"( --scan-path /usr/ printf
)";
}

void show_version(const char *program_name) noexcept
{
    std::cout << program_name << " version: " << DATABASE_HEADER_VERSION_STR << '\n';
}

RunningMethod parse_running_method(const std::string &value) noexcept
{
    if (value == RUNNING_METHOD_BUILD_DB)
    {
        return RunningMethod_BUILD_DB;
    }
    else if (value == RUNNING_METHOD_READ_DB) // NOLINT
    {
        return RunningMethod_READ_DB;
    }
    else if (value == RUNNING_METHOD_FREE_RUN)
    {
        return RunningMethod_FREE_RUN;
    }

    return RunningMethod_NOT_SET;
}

SymbolDefinitionToPrint parse_visibility(const std::string &value) noexcept
{
    if (value == "defined" || value == "only_defined")
    {
        return SymbolDefinitionToPrint_ONLYDEFINED;
    }

    if (value == "runtime" || value == "only_runtime")
    {
        return SymbolDefinitionToPrint_ONLYRUNTIME;
    }

    if (value == "both" || value == "show_both")
    {
        return SymbolDefinitionToPrint_SHOWFULL;
    }

    return SymbolDefinitionToPrint_UNKNOWN;
}

StringComparatorType parse_search_type(const std::string &value) noexcept
{
    if (value == "default")
    {
        return StringComparatorType_DEFAULT;
    }

    if (value == "regex")
    {
        return StringComparatorType_REGEX;
    }

    if (value == "fuzzy")
    {
        return StringComparatorType_FUZZY;
    }

    return StringComparatorType_UNKNOWN;
}

bool parse_arguments(int argc, char **argv, ProgramOptions &options) noexcept // NOLINT
{
    if (argc < 2)
    {
        return false;
    }

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i]; // NOLINT

        // Help
        if (arg == "-h" || arg == "--help")
        {
            options.print_help = true;
            continue;
        }

        // Version
        if (arg == "-v" || arg == "--version")
        {
            options.show_version = true;
            continue;
        }

        // Build Database
        if (arg == "-m" || arg == "--running-method")
        {
            if (options.running_method != RunningMethod_NOT_SET)
            {
                std::cerr << "ERROR: Multiple running methods entered!\n";
                return false;
            }

            std::string running_method_str = argv[++i]; // NOLINT
            auto parsed_running_method = parse_running_method(running_method_str);
            if ((bool)(parsed_running_method & RunningMethod_NOT_SET))
            {
                std::cerr << "ERROR: Invalid running method: " << running_method_str << '\n';
                return false;
            }
            options.running_method = parsed_running_method;
            continue;
        }

        // Shortcut: regex
        if (arg == "-r" || arg == "--regex")
        {
            options.search_type = StringComparatorType_REGEX;
            continue;
        }

        // Shortcut: fuzzy
        if (arg == "-f" || arg == "--fuzz")
        {
            options.search_type = StringComparatorType_FUZZY;
            continue;
        }

        // search-type
        if (arg == "--search-type")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "ERROR: --search-type option requires a value\n";
                return false;
            }

            options.search_type = parse_search_type(argv[++i]); // NOLINT
            if (options.search_type == StringComparatorType_UNKNOWN)
            {
                std::cerr << std::format("ERROR: invalid --search-type value: {}\n", argv[i]); // NOLINT
                return false;
            }

            continue;
        }

        // visibility
        if (arg == "--symbol-visibility")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "ERROR: --symbol-visibility option requires a value\n";
                return false;
            }

            options.visibility = parse_visibility(argv[++i]); // NOLINT

            if (options.visibility == SymbolDefinitionToPrint_UNKNOWN)
            {
                std::cerr << "ERROR: invalid --symbol-visibility value\n";
                return false;
            }

            continue;
        }

        // scan path
        if (arg == "-s" || arg == "--scan-path")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "ERROR: -s, --scan-path option requires a path\n";
                return false;
            }

            options.scan_root_path = argv[++i]; // NOLINT
            continue;
        }

        // verbose
        if (arg == "-d" || arg == "--debug")
        {
            options.debug_mode = true;
        }

        // positional symbol
        if (arg[0] != '-')
        {
            if (!options.symbol.empty())
            {
                std::cerr << "ERROR: multiple symbols provided\n";
                return false;
            }

            options.symbol = arg;
            continue;
        }

        std::cerr << "ERROR: unknown option: " << arg << "\n";
        return false;
    }

    if (options.print_help || options.show_version)
    {
        return true;
    }

    // Set default running mode
    if ((bool)(options.running_method & RunningMethod_NOT_SET))
    {
        options.running_method = RunningMethod_READ_DB;
    }

    // Check input symbol name in non BUILD_DB mode
    if (options.symbol.empty() && options.running_method != RunningMethod_BUILD_DB)
    {
        std::cerr << "ERROR: missing symbol name\n";
        return false;
    }

    // Log unused options in BUILD_DB mode
    if (options.running_method == RunningMethod_BUILD_DB)
    {
        if (options.search_type != StringComparatorType_NOT_SET)
        {
            std::cerr << "WARNING! When you're using `BUILD_DB' mode, using custom search type doesn't make sence!\n";
        }

        if (options.visibility != SymbolDefinitionToPrint_NOT_SET)
        {
            std::cerr << "WARNING! When you're using `BUILD_DB' mode, using custom symbol visibility option doesn't "
                         "make sence!\n";
        }
    }

    // Set default string comparator
    if ((bool)(options.search_type & StringComparatorType_NOT_SET))
    {
        options.search_type = StringComparatorType_DEFAULT;
    }

    // Set default visibilty option
    if ((bool)(options.visibility & SymbolDefinitionToPrint_NOT_SET))
    {
        options.visibility = SymbolDefinitionToPrint_ONLYDEFINED;
    }

    // Check logic
    if (!options.scan_root_path.empty() && options.running_method != RunningMethod_FREE_RUN)
    {
        std::cerr << "WARNING! When you're not using `FREE_RUN' mode, using custom scan path doesn't make sence!\n";
        options.scan_root_path.clear();
    }

    // use_debug = options.debug_mode;

    return true;
}

bool symfinder_symbol_should_be_stored(const SymbolEntry &sym_ent,
                                       SymbolDefinitionToPrint def_to_print,
                                       const StringComparator &compare_sym_names) noexcept
{
    if (compare_sym_names(sym_ent.name))
    {
        const auto &sym_metadata = sym_ent.metadata;
        if (sym_metadata.is_defined && (def_to_print == SymbolDefinitionToPrint_ONLYDEFINED ||
                                         def_to_print == SymbolDefinitionToPrint_SHOWFULL))
        {
            return true;
        }

        if (sym_metadata.is_defined && (def_to_print == SymbolDefinitionToPrint_ONLYDEFINED ||
                                         def_to_print == SymbolDefinitionToPrint_SHOWFULL))
        {
            return true;
        }
    }

    return false;
}

bool database_reader_entry_should_be_stored(const SymFind::SymbolEntryView &sym_ent_v,
                                            SymbolDefinitionToPrint def_to_print,
                                            const StringComparator &compare_sym_names) noexcept
{
    // TODO: Avoid copying symbol name.
    if (compare_sym_names(sym_ent_v.name))
    {
        const auto &sym_metadata = sym_ent_v.metadata;
        if (sym_metadata->is_defined && (def_to_print == SymbolDefinitionToPrint_ONLYDEFINED ||
                                         def_to_print == SymbolDefinitionToPrint_SHOWFULL))
        {
            return true;
        }

        if (sym_metadata->is_defined && (def_to_print == SymbolDefinitionToPrint_ONLYDEFINED ||
                                         def_to_print == SymbolDefinitionToPrint_SHOWFULL))
        {
            return true;
        }
    }

    return false;
}

} // namespace SymFind
