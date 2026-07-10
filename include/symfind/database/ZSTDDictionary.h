#pragma once

#include <memory>

#include <zstd.h>

namespace SymFind
{

struct CDictDeleter
{
    void operator()(ZSTD_CDict *dict) const noexcept;
};

struct DDictDeleter
{
    void operator()(ZSTD_DDict *dict) const noexcept;
};

using ZSTDCompressDictionaryPtr = std::unique_ptr<ZSTD_CDict, CDictDeleter>;
using ZSTDDecompressDictionaryPtr = std::unique_ptr<ZSTD_DDict, DDictDeleter>;

struct CompressionOptions
{
    ZSTDCompressDictionaryPtr cdict;
};

struct DecompressionOptions
{
    ZSTDDecompressDictionaryPtr ddict;
};

extern CompressionOptions no_dict;

} // namespace SymFind
