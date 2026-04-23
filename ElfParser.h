#pragma once

#include <string>

#include <gelf.h>

#include "Global.h"
#include "Symbol.h"
#include "Expected.h"

namespace SymFind
{

bool parse_symtables(const std::string &file, SymbolEntries& parsed_symbol_entries, std::string* err_msg = nullptr) noexcept;

} // namespace SymFind
