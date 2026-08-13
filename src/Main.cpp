#include <iostream>
#include <string>

#include <cstdint>
#include <cstdlib>

#include <symfind/config/Config.h>
#include <symfind/config/OptionParser.h>
#include <symfind/core/DictionaryBuilder.h>
#include <symfind/core/StringComparator.h>
#include <symfind/core/SymFinder.h>
#include <symfind/core/Symbol.h>
#include <symfind/database/Database.h>
#include <symfind/database/DatabaseBuilder.h>
#include <symfind/database/DatabaseReader.h>
#include <symfind/filesystem/FSScanner.h>
#include <symfind/utils/StoreDeciderUtils.h>

#define PROGRAM_NAME "symfind"

#define ERR_MSG_DEFAULT_SIZE 1024

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

} // namespace


int main(int argc, char **argv)
{
    // Parse user input options
    SymFind::ProgramOptions options;
    if (!parse_arguments(argc, argv, options))
    {
        SymFind::print_help(PROGRAM_NAME);
        return EXIT_FAILURE;
    }

    if (options.print_help)
    {
        SymFind::print_help(PROGRAM_NAME);
        return EXIT_SUCCESS;
    }

    if (options.show_version)
    {
        SymFind::show_version(PROGRAM_NAME);
        return EXIT_SUCCESS;
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

    // Override some configurations if user has entered equivalent option
    if (!options.scan_root_path.empty())
    {
        config_parser->set_database_scan_path(options.scan_root_path);
    }

    // Initialize bind-mount
    SymFind::BindMount::init(config_parser);

    SymFind::DictionaryBuilderPtr dict_builder(new SymFind::DictionaryBuilder);

    // Create FSScanner
    SymFind::FSScanner fsscanner(config_parser, SymFind::BindMount::getInstancePtr(), dict_builder);

    bool scan_res = false;
    std::tie(scan_res, err_msg) = fsscanner.scan();
    if (!scan_res)
    {
        std::cerr << "Filesystem scan failed: " << err_msg << '\n';
        return 1;
    }

    // Run...

    if ((bool)(options.running_method & SymFind::RunningMethod_BUILD_DB))
    {
        std::string err_msg(ERR_MSG_DEFAULT_SIZE, '\0');

        SymFind::DatabaseBuilder db_builder(config_parser, fsscanner, dict_builder);
        bool build_res = db_builder.build(&err_msg);
        if (!build_res)
        {
            std::cout << err_msg << '\n';
            return 1;
        }
    }
    else if ((bool)(options.running_method & SymFind::RunningMethod_READ_DB))
    {
        std::string err_msg(ERR_MSG_DEFAULT_SIZE, '\0');

        // TODO: At the moment, regex and fuzzy search types do not work. Fix it in future!
        auto string_comparator = SymFind::make_string_comparator(options.search_type, options.symbol);

        auto ignore_entry = [&string_comparator](const SymFind::SymbolEntryView &sym_ent_v) -> bool {
            // TODO: Take argument to show only "DEFINED" symbols or all symbols.
            return !database_reader_entry_should_be_stored(sym_ent_v,
                                            SymFind::SymbolDefinitionToPrint_ONLYDEFINED,
                                            *string_comparator);
        };

        std::optional<SymFind::DatabaseReader::SymbolLookupResultList> lookup_res_opt =
            SymFind::DatabaseReader::find_symbol_references(config_parser, options.symbol, ignore_entry, &err_msg);
        if (!lookup_res_opt.has_value())
        {
            std::cerr << err_msg << '\n';
            return 1;
        }

        SymFind::DatabaseReader::print_symbol_lookup_results(lookup_res_opt.value());
    }
    else if ((bool)(options.running_method & SymFind::RunningMethod_FREE_RUN))
    {
        auto string_comparator = SymFind::make_string_comparator(options.search_type, options.symbol);

        auto ignore_symbol = [&string_comparator](const SymFind::SymbolEntry &sym_ent) -> bool {
            // TODO: Take argument to show only "DEFINED" symbols or all symbols.
            return !symfinder_symbol_should_be_stored(sym_ent,
                                                      SymFind::SymbolDefinitionToPrint_ONLYDEFINED,
                                                      *string_comparator);
        };

        SymFind::SymFinder symfinder(config_parser, fsscanner, ignore_symbol);
    }

    return 0;
}
