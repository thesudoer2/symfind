#pragma once

#include <string>

#include <cstdlib>

#include <cxxabi.h>

// -----------------------------------------------------------------------------
// Implementation A – reusable buffer via Demangler class
// -----------------------------------------------------------------------------

class Demangler final
{
public:
    Demangler() noexcept : _buffer_size(2048), _buffer(static_cast<char *>(std::malloc(_buffer_size)))
    {
    }

    std::string demangle(const std::string &sym_name)
    {
        int status = 0;

        char *result = abi::__cxa_demangle(sym_name.c_str(), _buffer, &_buffer_size, &status);

        if (status != 0 || result == nullptr)
            return sym_name;

        _buffer = result; // may have changed due to realloc
        return result;
    }

    ~Demangler() noexcept
    {
        std::free(_buffer);
    }

    Demangler(const Demangler &) noexcept = delete;
    Demangler(Demangler &&) noexcept = delete;
    Demangler &operator=(const Demangler &) noexcept = delete;
    Demangler &operator=(Demangler &&) noexcept = delete;

private:
    std::size_t _buffer_size;
    char *_buffer;
};
