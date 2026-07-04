#pragma once

#include <string>

#include <gelf.h>

#include "Global.h"
#include "Symbol.h"
#include "Expected.h"

namespace SymFind
{

using TryStoreSymbolCallback = std::function<void(SymbolEntry &&)>;
using SymbolShouldBeIgnoredCallback = std::function<bool(const SymbolEntry &)>;

const auto ACCEPT_ALL_SYMBOLS = [](const SymbolEntry &) -> bool { return false; };

bool parse_symtables(const std::string &file,
                     const TryStoreSymbolCallback &try_store_symbol_callback,
                     const SymbolShouldBeIgnoredCallback &ignore_symbol_callback = ACCEPT_ALL_SYMBOLS,
                     std::string *err_msg = nullptr) noexcept;

} // namespace SymFind
