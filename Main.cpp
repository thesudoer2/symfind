#include <format>
#include <iostream>
#include <regex>
#include <string>

#include "Config.h"
#include "FSScanner.h"
#include "SymFinder.h"
#include "StringComparator.h"
#include "Symbol.h"

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


struct ProgramOptions
{
    SymFind::StringComparatorType search_type = SymFind::StringComparatorType::DEFAULT;
    SymbolDefinitionToPrint visibility = SymbolDefinitionToPrint::ONLY_DEFINED;
    std::filesystem::path root_path = "/";
    std::string symbol;
};


// -----------------------------------------------------------------------------
// Implementations
// -----------------------------------------------------------------------------

static void print_help(const char *program_name)
{
    std::cout <<
R"(Usage:
    )" << program_name << R"( [options] <symbol>

Options:
    -r, --regex
        Shortcut for: --search-type regex

    -f, --fuzz
        Shortcut for: --search-type fuzzy

    --search-type <default|regex|fuzzy>
        Defines how symbol matching is performed.

            default -> exact string comparison (default)
            regex   -> treat input as regex pattern
            fuzzy   -> fuzzy matching

    --symbol-visibility <defined|runtime|both>
        Controls which symbols are shown:

            defined -> only defined symbols (default)
            runtime -> only runtime/imported symbols
            both    -> show both categories

    --root <path>
        Set root directory for symbol search.

    -h, --help
        Show this help message.

Examples:
    )" << program_name << R"( malloc

    )" << program_name << R"( --search-type regex "std::.*vector"

    )" << program_name << R"( --search-type fuzzy pushbak

    )" << program_name << R"( --symbol-visibility runtime malloc

    )" << program_name << R"( --root /usr/lib64 printf
)";
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
            if (options.search_type == SymFind::StringComparatorType::DEFAULT &&
                argv[i] != std::string("default"))
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

    if (options.symbol.empty())
    {
        std::cerr << "ERROR: missing symbol argument\n";
        return false;
    }

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

    auto string_comparator = SymFind::make_string_comparator(options.search_type, options.symbol);

    auto ignore_symbol = [&string_comparator](const SymFind::SymbolEntry &sym_ent) -> bool {
        // TODO1: Take argument to show only "DEFINED" symbols or all symbols.
        return !symbol_should_be_stored(sym_ent,
                                        SymbolDefinitionToPrint::ONLY_DEFINED,
                                        *string_comparator);
    };

    SymFind::SymFinder symfinder(config_parser, fsscanner, ignore_symbol);

    return 0;
}
