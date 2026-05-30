#pragma once

#include <memory>
#include <string>

#include <cxxabi.h>

// -----------------------------------------------------------------------------
// Implementation B – unique_ptr, always allocates fresh
// -----------------------------------------------------------------------------

std::string  demangle_unique_ptr(const std::string &sym_name) noexcept
{
    int status = 0;

    std::unique_ptr<char, void (*)(void *)> demangled(abi::__cxa_demangle(sym_name.c_str(), nullptr, nullptr, &status),
                                                      std::free);

    if (status == 0 && demangled)
    {
        return demangled.get();
    }

    return sym_name;
}
