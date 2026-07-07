#pragma once

#include <string>

#include "FileWrapper.h"
#include "Global.h"
#include "NoCopy.h"
#include "NoMove.h"

namespace SymFind
{

// Minimal RAII wrapper: mmap the whole database file once, read-only,
// private mapping.
class MappedFile final : NoCopy, NoMove
{
public:
    MappedFile() noexcept;

    ~MappedFile() noexcept;

public: // NOLINT
    bool open(const std::string &path, std::string *err_msg = nullptr) noexcept;

    __nodiscard const std::byte *data() const;

    __nodiscard std::size_t size() const;

    bool operator!() const noexcept;

    bool operator!=(bool com_val) const noexcept;

    explicit operator bool() const noexcept;

    FileWrapper::Errno_t get_errno() const noexcept;

private:
    void close() noexcept;

private: // NOLINT
    FileWrapper _db_file;

    const std::byte *_data{nullptr};
    std::uint64_t _size{0};
};

} // namespace SymFind
