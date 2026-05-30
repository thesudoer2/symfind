#pragma once

#include <string>

#include <gelf.h>

#include "Global.h"
#include "Symbol.h"
#include "Expected.h"

namespace SymFind
{

using SymbolShouldBeIgnoredCallback = std::function<bool(const SymbolEntry &)>;

bool parse_symtables(const std::string &file,
                     SymbolEntries &parsed_symbol_entries,
                     const SymbolShouldBeIgnoredCallback &ignore_symbol_callback,
                     std::string *err_msg = nullptr) noexcept;

} // namespace SymFind
