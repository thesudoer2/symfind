#pragma once

#include <memory>
#include <string_view>
#include <string>
#include <vector>

#include <zdict.h>

#include <symfind/utils/NoCopy.h>
#include <symfind/utils/NoMove.h>
#include <symfind/database/ZSTDDictionary.h>

namespace SymFind
{

class DictionaryBuilder : NoCopy, NoMove
{
public:
    // TODO: Try "256 * 1024" to see whether the larger size provides a meaningful compression improvement or not.
    explicit DictionaryBuilder(std::size_t max_dictionary_size = 128 * 1024 /* KiB */);

    void reserve(std::size_t samples) noexcept;

    void add_sample(std::string_view sample) noexcept;

    bool train(std::string &dictionary, std::string *err_msg = nullptr) noexcept;

    void clear() noexcept;

    static ZSTDCompressDictionaryPtr create_compression_dictionary(const void *dict,
                                                       std::size_t size,
                                                       int compression_level = 6) noexcept;

    static ZSTDCompressDictionaryPtr create_decompression_dictionary(const void *dict,
                                                       std::size_t size,
                                                       int compression_level = 6) noexcept;

private:
    std::vector<char> _samples;
    std::vector<std::size_t> _sample_sizes;

    std::size_t _max_dictionary_size;
};

using DictionaryBuilderPtr = std::shared_ptr<DictionaryBuilder>;

} // namespace SymFind
