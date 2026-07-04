#include "DictionaryBuilder.h"

#include <cstring>

#include "Global.h"
#include "ZSTDDictionary.h"

#define AVERAGE_SAMPLE_SIZE 20 /* Bytes */

namespace SymFind
{

DictionaryBuilder::DictionaryBuilder(std::size_t max_dictionary_size) : _max_dictionary_size(max_dictionary_size)
{
}

void DictionaryBuilder::reserve(std::size_t samples) noexcept
{
    _sample_sizes.reserve(samples);
    _samples.reserve(samples * AVERAGE_SAMPLE_SIZE);
}

void DictionaryBuilder::add_sample(std::string_view sample) noexcept
{
    if (sample.empty())
    {
        return;
    }

    // Append sample size to size list
    _sample_sizes.push_back(sample.size());

    // Append sample bytes to sample byte list
    const auto old_size = _samples.size();

    _samples.resize(old_size + sample.size());

    std::memcpy(_samples.data() + old_size, sample.data(), sample.size());
}

bool DictionaryBuilder::train(std::string &dictionary, std::string *err_msg) noexcept
{
    dictionary.resize(_max_dictionary_size);

    const size_t dict_size = ZDICT_trainFromBuffer(dictionary.data(),
                                                   dictionary.size(),
                                                   _samples.data(),
                                                   _sample_sizes.data(),
                                                   _sample_sizes.size());

    if (ZDICT_isError(dict_size) == 1)
    {
        SET_ERR_MSG(err_msg, ZDICT_getErrorName(dict_size));
        return false;
    }

    dictionary.resize(dict_size);
    return true;
}

void DictionaryBuilder::clear() noexcept
{
    _samples.clear();
    _sample_sizes.clear();
}

ZSTDCompressDictionaryPtr DictionaryBuilder::create_compression_dictionary(const void *dict_buf,
                                                                           std::size_t dict_size,
                                                                           int compression_level) noexcept
{
    return ZSTDCompressDictionaryPtr(ZSTD_createCDict(dict_buf, dict_size, compression_level));
}

ZSTDCompressDictionaryPtr DictionaryBuilder::create_decompression_dictionary(const void *dict_buf,
                                                                             std::size_t dict_size,
                                                                             int compression_level) noexcept
{
    return ZSTDCompressDictionaryPtr(ZSTD_createCDict(dict_buf, dict_size, compression_level));
}

} // namespace SymFind
