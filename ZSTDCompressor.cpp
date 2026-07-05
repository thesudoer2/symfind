#include "ZSTDCompressor.h"

#include <string>
#include <vector>

#include <zstd.h>

#include "Global.h"
#include "ZSTDDictionary.h"

namespace SymFind
{

// -----------------------------------------------------------------------------
// CCtxDeleter implementation
// -----------------------------------------------------------------------------

void ZSTDCompressor::CCtxDeleter::operator()(ZSTD_CCtx *ctx) const noexcept
{
    if (ctx == nullptr)
    {
        ZSTD_freeCCtx(ctx);
    }
}

// -----------------------------------------------------------------------------
// DCtxDeleter implementation
// -----------------------------------------------------------------------------

void ZSTDCompressor::DCtxDeleter::operator()(ZSTD_DCtx *ctx) const noexcept
{
    if (ctx == nullptr)
    {
        ZSTD_freeDCtx(ctx);
    }
}

// -----------------------------------------------------------------------------
// ZSTDCompressor implementation
// -----------------------------------------------------------------------------

ZSTDCompressor::ZSTDCompressor(std::string *err_msg) noexcept : _cctx(ZSTD_createCCtx()), _dctx(ZSTD_createDCtx())
{
    if (_cctx == nullptr || _dctx == nullptr)
    {
        SET_ERR_MSG(err_msg, "Failed to create ZSTD contexts");
    }
}

bool ZSTDCompressor::compress(void *input,
                              std::uint32_t input_size,
                              std::string &output,
                              const CompressionOptions &comp_opts,
                              std::string *err_msg) const noexcept
{
    // Resize output buffer with max compressed output size
    output.resize(ZSTD_compressBound(input_size));

    size_t result = 0;

    auto *cdict = comp_opts.cdict.get();

    if (cdict != nullptr)
    {
        result = ZSTD_compress_usingCDict(_cctx.get(), output.data(), output.size(), input, input_size, cdict);
    }
    else
    {
        result = ZSTD_compressCCtx(_cctx.get(), output.data(), output.size(), input, input_size, 6);
    }

    if (ZSTD_isError(result) == 1)
    {
        SET_ERR_MSG(err_msg, ZSTD_getErrorName(result));
        return false;
    }

    output.resize(result);
    return true;
}

bool ZSTDCompressor::decompress(void *input,
                                std::uint32_t input_size,
                                std::size_t uncompressed_size,
                                std::string &output,
                                DecompressionOptions &decomp_opts,
                                std::string *err_msg) const noexcept
{
    output.resize(uncompressed_size);

    size_t result = 0;

    auto *ddict = decomp_opts.ddict.get();

    if (ddict != nullptr)
    {
        result = ZSTD_decompress_usingDDict(_dctx.get(),
                                            output.data(),
                                            output.size(),
                                            input,
                                            input_size,
                                            ddict);
    }
    else
    {
        result = ZSTD_decompressDCtx(_dctx.get(), output.data(), output.size(), input, input_size);
    }

    if (ZSTD_isError(result) == 1)
    {
        SET_ERR_MSG(err_msg, ZSTD_getErrorName(result));
        return false;
    }

    output.resize(result);
    return true;
}

} // namespace SymFind
