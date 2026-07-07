#pragma once

#include <cinttypes>
#include <cstdio>

#include <bitset>
#include <memory>
#include <string>
#include <vector>

#include "Expected.h"
#include "Global.h"

namespace SymFind
{

using FileID = std::uint32_t;
using FileIDList = std::vector<FileID>;

class FileWrapper final
{
private:
    struct FileCloser
    {
        void operator()(FILE *fp) const noexcept // NOLINT(readability-identifier-length)
        {
            if (fp != nullptr)
            {
                std::fclose(fp); // NOLINT(cppcoreguidelines-owning-memory)
            }
        }
    };

    using FilePtr = std::unique_ptr<FILE, FileCloser>;

public:
    using Errno_t = std::int32_t;
    using Offset_t = std::uint32_t;

public: // NOLINT(readability-redundant-access-specifiers)
    FileWrapper() noexcept;
    ~FileWrapper() noexcept;

    FileWrapper(const FileWrapper &) noexcept = delete;
    FileWrapper(FileWrapper &&) noexcept = delete;

    FileWrapper &operator=(const FileWrapper &) noexcept = delete;
    FileWrapper &operator=(FileWrapper &&) noexcept = delete;

    FileWrapper(const std::string &file_path, const std::string &mode) noexcept;

    bool operator!() const noexcept;
    bool operator!=(bool com_val) const noexcept;

    explicit operator bool() const noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    expected<std::int32_t, Errno_t> get_fd() const noexcept;

    const FILE *get_cfp() const noexcept;
    FILE *get_fp() noexcept;

    bool open(const std::string &file_path, const std::string &mode) noexcept;
    void close() noexcept;

    __nodiscard bool is_open() const noexcept;

    __nodiscard Errno_t get_errno() const noexcept;

    __nodiscard std::string get_file_path() const noexcept;

public: // NOLINT(readability-redundant-access-specifiers)
    static expected<FileWrapper::Offset_t, FileWrapper::Errno_t> seek(FileWrapper &file,
                                                                      Offset_t offset,
                                                                      int destination) noexcept;

    static expected<Offset_t, Errno_t> tellp(const FileWrapper &file) noexcept;

    static bool read(const FileWrapper &file, void *ptr, size_t len, Offset_t offset) noexcept;

    static bool write(const FileWrapper &file, const void *buf, size_t len, Offset_t offset) noexcept;

    static expected<std::uint64_t, Errno_t> get_file_size(FileWrapper &file) noexcept;

private:
    FilePtr _fp{nullptr};
    mutable Errno_t _last_errno = -1;
    bool _is_open = false;
};

} // namespace SymFind
