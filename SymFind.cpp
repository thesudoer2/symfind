#include <iostream>
#include <string>
#include <format>

#include "Config.h"
#include "FSScanner.h"
#include "SymTableGenerator.h"
#include "Symbol.h"

namespace
{

#ifdef DEFAULT_CONFIG_PATH
constexpr const char *config_path = DEFAULT_CONFIG_PATH;
#else
constexpr const char *config_path("/etc/symfind/symfind.conf");
#endif

} // namespace


int main(int /* argc */, char ** /* argv */)
{
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

    SymFind::SymTableGenerator symtable_gen(config_parser, fsscanner);

    // Find symbol
    std::string sym_name = "printf";
    SymFind::SymbolRefsPtr symrefs_ptr = symtable_gen.find_sym(sym_name);
    if (symrefs_ptr == nullptr)
    {
        std::cout << "No such symbol found: " << sym_name << "\n";
        return 1;
    }

    for (const auto &symref : *symrefs_ptr)
    {
        SymFind::FSScanner::FileInfo file_info = fsscanner.get_found_files()[symref.file_id];
        std::string file_path = SymFind::FSScanner::get_file_info_full_path(file_info);

        std::string sym_bind = SymFind::symbol_bind_to_str(symref.metadata->bind);
        std::string sym_type = SymFind::symbol_type_to_str(symref.metadata->type);
        std::string sym_is_defined = symref.metadata->is_defined ? "DEFINED" : "RUNTIME";
        std::string sym_src_sec = SymFind::symbol_source_section_to_str(symref.metadata->source_section);

        std::cout << file_path << ":\n";
        std::cout << std::format(
            "\tsym_name: {}\t\tsym_is_defined: {}\t\tsym_src_sec: {}\t\t sym_type: {}\t\tsym_bind: {}\n\n",
            sym_name,
            sym_is_defined,
            sym_src_sec,
            sym_type,
            sym_bind);
    }

    // >>>>> DEBUGGING
    // for (auto &[key, value] : symtable_gen._symtable)
    // {
    //     std::cout << "symname: " << key << "\n";
    // }
    // <<<<< DEBUGGING

    return 0;
}
