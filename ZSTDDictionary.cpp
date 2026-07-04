#include "ZSTDDictionary.h"

namespace SymFind
{

// -----------------------------------------------------------------------------
// CDictDeleter implementation
// -----------------------------------------------------------------------------

void CDictDeleter::operator()(ZSTD_CDict *dict) const noexcept
{
    if (dict == nullptr)
    {
        ZSTD_freeCDict(dict);
    }
}

// -----------------------------------------------------------------------------
// DDictDeleter implementation
// -----------------------------------------------------------------------------

void DDictDeleter::operator()(ZSTD_DDict *dict) const noexcept
{
    if (dict == nullptr)
    {
        ZSTD_freeDDict(dict);
    }
}

CompressionOptions no_dict{nullptr};

} // namespace SymFind
