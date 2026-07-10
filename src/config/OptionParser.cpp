#include <symfind/config/OptionParser.h>

namespace SymFind
{

void print_help(const char *program_name) noexcept
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

RunningMethod parse_running_method(const std::string &value) noexcept
{
    if (value == RUNNING_METHOD_BUILD_DB)
    {
        return RunningMethod::BUILD_DB;
    }
    else if (value == RUNNING_METHOD_READ_DB) // NOLINT
    {
        return RunningMethod::READ_DB;
    }
    else if (value == RUNNING_METHOD_FREE_RUN)
    {
        return RunningMethod::FREE_RUN;
    }

    return RunningMethod::NOT_SET;
}

SymbolDefinitionToPrint parse_visibility(const std::string &value) noexcept
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

StringComparatorType parse_search_type(const std::string &value) noexcept
{
    if (value == "default" || value == "simple")
    {
        return StringComparatorType::DEFAULT;
    }

    if (value == "regex")
    {
        return StringComparatorType::REGEX;
    }

    if (value == "fuzzy")
    {
        return StringComparatorType::FUZZY;
    }

    return StringComparatorType::UNKNOWN;
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
            print_help(argv[0]); // NOLINT
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

            std::string running_method_str = argv[++i]; // NOLINT
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
            options.search_type = StringComparatorType::REGEX;
            continue;
        }

        // Shortcut: fuzzy
        if (arg == "-f" || arg == "--fuzz")
        {
            options.search_type = StringComparatorType::FUZZY;
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

            options.search_type = parse_search_type(argv[++i]); // NOLINT
            if (options.search_type == StringComparatorType::DEFAULT && argv[i] != std::string("default")) // NOLINT
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

            options.visibility = parse_visibility(argv[++i]); // NOLINT

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

            options.root_path = argv[++i]; // NOLINT
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
        std::cerr << "ERROR: missing symbol name\n";
        return false;
    }

    // use_debug = options.be_verbose;

    return true;
}

bool symfinder_symbol_should_be_stored(const SymbolEntry &sym_ent,
                                       SymbolDefinitionToPrint def_to_print,
                                       const StringComparator &compare_sym_names) noexcept
{
    if (compare_sym_names(sym_ent.name))
    {
        const auto &sym_metadata = sym_ent.metadata;
        if (sym_metadata.is_defined && (def_to_print == SymbolDefinitionToPrint::ONLY_DEFINED ||
                                         def_to_print == SymbolDefinitionToPrint::SHOW_BOTH))
        {
            return true;
        }

        if (sym_metadata.is_defined && (def_to_print == SymbolDefinitionToPrint::ONLY_DEFINED ||
                                         def_to_print == SymbolDefinitionToPrint::SHOW_BOTH))
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

} // namespace SymFind
