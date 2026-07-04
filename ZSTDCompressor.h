#pragma once

#include <string>
#include <memory>
#include <vector>

#include <zstd.h>

#include "NoCopy.h"
#include "NoMove.h"
#include "Global.h"
#include "ZSTDDictionary.h"

namespace SymFind
{

class ZSTDCompressor : NoCopy, NoMove
{
private:
    struct CCtxDeleter
    {
        void operator()(ZSTD_CCtx *ctx) const noexcept;
    };

    struct DCtxDeleter
    {
        void operator()(ZSTD_DCtx *ctx) const noexcept;
    };

public:
    explicit ZSTDCompressor(std::string *err_msg = nullptr) noexcept;

    bool compress(void *input,
                  std::uint32_t input_size,
                  std::string &output,
                  CompressionOptions &comp_opts,
                  std::string *err_msg = nullptr) const noexcept;

    bool decompress(void *input,
                    std::uint32_t input_size,
                    std::size_t uncompressed_size,
                    std::string &output,
                    DecompressionOptions &decomp_opts,
                    std::string *err_msg = nullptr) const noexcept;

private:
    std::unique_ptr<ZSTD_CCtx, CCtxDeleter> _cctx;
    std::unique_ptr<ZSTD_DCtx, DCtxDeleter> _dctx;
};

} // namespace SymFind
