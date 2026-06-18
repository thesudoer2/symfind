#include <cstdint>
#include <format>
#include <iostream>
#include <regex>
#include <string>

#include "Config.h"
#include "DatabaseBuilder.h"
#include "FSScanner.h"
#include "StringComparator.h"
#include "SymFinder.h"
#include "Symbol.h"

#define RUNNING_METHOD_BUILD_DB "build_db"
#define RUNNING_METHOD_READ_DB "read_db"
#define RUNNING_METHOD_FREE_RUN "free_run"

#define RUNNING_METHODS RUNNING_METHOD_BUILD_DB "|" RUNNING_METHOD_READ_DB "|" RUNNING_METHOD_FREE_RUN


#define SEARCH_TYPE_DEFAULT "default"
#define SEARCH_TYPE_REGEX "regex"
#define SEARCH_TYPE_FUZZY "fuzzy"

#define SEARCH_TYPES SEARCH_TYPE_DEFAULT "|" SEARCH_TYPE_REGEX "|" SEARCH_TYPE_FUZZY

bool use_debug = false;

namespace
{

// -----------------------------------------------------------------------------
// Variables & Types
// -----------------------------------------------------------------------------

#ifdef DEFAULT_CONFIG_PATH
constexpr const char *config_path = DEFAULT_CONFIG_PATH;
#else
constexpr const char *config_path("/etc/symfind/symfind.conf");
#endif

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
    SymFind::StringComparatorType search_type = SymFind::StringComparatorType::DEFAULT;
    SymbolDefinitionToPrint visibility = SymbolDefinitionToPrint::ONLY_DEFINED;
    std::filesystem::path root_path = "/";
    std::string symbol;
    bool be_verbose = false;
};


// -----------------------------------------------------------------------------
// Implementations
// -----------------------------------------------------------------------------

static void print_help(const char *program_name)
{
    std::cout <<
        R"(Usage:
    )" << program_name
              << R"( [options] <symbol>

Options:
    -r, --running-method <)"
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

    --root <path>
        Set root directory for symbol search.

    -v, --verbose
        Be verbose and show more logs.

    -h, --help
        Show this help message.

Examples:
    )" << program_name
              << R"( malloc

    )" << program_name
              << R"( --search-type regex "std::.*vector"

    )" << program_name
              << R"( --search-type fuzzy pushbak

    )" << program_name
              << R"( --symbol-visibility runtime malloc

    )" << program_name
              << R"( --root /usr/lib64 printf
)";
}

RunningMethod parse_running_method(const std::string &value)
{
    if (value == RUNNING_METHOD_BUILD_DB)
    {
        return RunningMethod::BUILD_DB;
    }
    else if (value == RUNNING_METHOD_READ_DB)
    {
        return RunningMethod::READ_DB;
    }
    else if (value == RUNNING_METHOD_FREE_RUN)
    {
        return RunningMethod::FREE_RUN;
    }

    return RunningMethod::NOT_SET;
}

SymbolDefinitionToPrint parse_visibility(const std::string &value)
{
    if (value == "defined" || value == "only_defined")
    {
        return SymbolDefinitionToPrint::ONLY_DEFINED;
    }

    if (value == "runtime" || value == "only_runtime")
    {
        return SymbolDefinitionToPrint::ONLY_RUNTIME;
    }

    if (value == "both" || value == "show_both")
    {
        return SymbolDefinitionToPrint::SHOW_BOTH;
    }

    return SymbolDefinitionToPrint::UNKNOWN;
}

SymFind::StringComparatorType parse_search_type(const std::string &value)
{
    if (value == "default" || value == "simple")
    {
        return SymFind::StringComparatorType::DEFAULT;
    }

    if (value == "regex")
    {
        return SymFind::StringComparatorType::REGEX;
    }

    if (value == "fuzzy")
    {
        return SymFind::StringComparatorType::FUZZY;
    }

    return SymFind::StringComparatorType::UNKNOWN;
}

bool parse_arguments(int argc, char **argv, ProgramOptions &options)
{
    if (argc < 2)
    {
        return false;
    }

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        // Help
        if (arg == "-h" || arg == "--help")
        {
            print_help(argv[0]);
            std::exit(0);
        }

        // Build Database
        if (arg == "-r" || arg == "--running-method")
        {
            if (options.running_method != RunningMethod::NOT_SET)
            {
                std::cerr << "ERROR: Multiple running methods entered!\n";
                return false;
            }

            std::string running_method_str = argv[++i];
            auto parsed_running_method = parse_running_method(running_method_str);
            if ((bool)(parsed_running_method & RunningMethod::NOT_SET))
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
            options.search_type = SymFind::StringComparatorType::REGEX;
            continue;
        }

        // Shortcut: fuzzy
        if (arg == "-f" || arg == "--fuzz")
        {
            options.search_type = SymFind::StringComparatorType::FUZZY;
            continue;
        }

        // search-type
        if (arg == "--search-type")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "ERROR: --search-type requires a value\n";
                return false;
            }

            options.search_type = parse_search_type(argv[++i]);
            if (options.search_type == SymFind::StringComparatorType::DEFAULT && argv[i] != std::string("default"))
            {
                std::cerr << "ERROR: invalid --search-type value\n";
                return false;
            }

            continue;
        }

        // visibility
        if (arg == "--symbol-visibility")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "ERROR: --symbol-visibility requires a value\n";
                return false;
            }

            options.visibility = parse_visibility(argv[++i]);

            if (options.visibility == SymbolDefinitionToPrint::UNKNOWN)
            {
                std::cerr << "ERROR: invalid --symbol-visibility value\n";
                return false;
            }

            continue;
        }

        // root
        if (arg == "--root")
        {
            if (i + 1 >= argc)
            {
                std::cerr << "ERROR: --root requires a path\n";
                return false;
            }

            options.root_path = argv[++i];
            continue;
        }

        // verbose
        if (arg == "-v" || arg == "--verbose")
        {
            options.be_verbose = true;
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

    if ((bool)(options.running_method & RunningMethod::NOT_SET))
    {
        options.running_method = RunningMethod::READ_DB;
    }

    if (options.symbol.empty() && options.running_method != RunningMethod::BUILD_DB)
    {
        std::cerr << "ERROR: missing symbol argument\n";
        return false;
    }

    use_debug = options.be_verbose;

    return true;
}

bool symbol_should_be_stored(const SymFind::SymbolEntry &sym_ent,
                             SymbolDefinitionToPrint def_to_print,
                             const SymFind::StringComparator &compare_sym_names) noexcept
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

} // namespace


int main(int argc, char **argv)
{
    // Parse user input options
    ProgramOptions options;
    if (!parse_arguments(argc, argv, options))
    {
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    // Parse configuration file
    std::string err_msg;
    auto config_parser = std::make_shared<SymFind::ConfigParser>();
    bool parse_res = config_parser->parse(config_path, &err_msg);

    if (!parse_res)
    {
        std::cerr << "Parsing configuration failed: " << err_msg << '\n';
        return 1;
    }

    // Initialize bind-mount
    SymFind::BindMount::init(config_parser);

    // Create FSScanner
    SymFind::FSScanner fsscanner(config_parser, SymFind::BindMount::getInstancePtr());

    bool scan_res = false;
    std::tie(scan_res, err_msg) = fsscanner.scan();
    if (!scan_res)
    {
        std::cerr << "Filesystem scan failed: " << err_msg << '\n';
        return 1;
    }

    // Run...

    if ((bool)(options.running_method & BUILD_DB))
    {
        std::string err_msg(1024, '\0');

        SymFind::DatabaseBuilder db_builder(config_parser, fsscanner);
        bool build_res = db_builder.build(&err_msg);
        if (!build_res)
        {
            std::cout << err_msg << '\n';
            return 1;
        }
    }
    else if ((bool)(options.running_method & READ_DB))
    {
        asm volatile("nop");
    }
    else if ((bool)(options.running_method & FREE_RUN))
    {
        auto string_comparator = SymFind::make_string_comparator(options.search_type, options.symbol);

        auto ignore_symbol = [&string_comparator](const SymFind::SymbolEntry &sym_ent) -> bool {
            // TODO1: Take argument to show only "DEFINED" symbols or all symbols.
            return !symbol_should_be_stored(sym_ent, SymbolDefinitionToPrint::ONLY_DEFINED, *string_comparator);
        };

        SymFind::SymFinder symfinder(config_parser, fsscanner, ignore_symbol);
    }

    return 0;
}
