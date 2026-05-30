#pragma once

#include <iterator>
#include <vector>
#include <algorithm>

#include <cinttypes>

// Compare [f_begin, f_end) and [s_begin, s_end)
template <typename Iterator>
bool iterators_are_the_same(const Iterator &f_begin,
                            const Iterator &f_end,
                            const Iterator &s_begin,
                            const Iterator &s_end) noexcept
{
    if (std::distance(f_begin, f_end) != std::distance(s_begin, s_end))
    {
        return false;
    }

    std::size_t distance = std::distance(f_begin, f_end);

    for (std::size_t i{}; i < distance; ++i)
    {
        if (*(f_begin + i) != *(s_begin + i))
        {
            return false;
        }
    }
    return true;
}

bool filename_has_extension(const std::string &filename, const std::string &extension)
{
    if (filename.empty() || extension.empty() || extension.size() > filename.size())
    {
        return false;
    }

    std::int32_t ext_size = extension.size(); // NOLINT
    const std::string::const_reverse_iterator e_rit_begin = extension.crbegin();
    const std::string::const_reverse_iterator e_rit_end = extension.crend();
    for (std::string::const_reverse_iterator f_rit = filename.crbegin(); f_rit != filename.crend() - ext_size; ++f_rit)
    {
        if (*f_rit == *e_rit_begin) [[unlikely]]
        {
            if (iterators_are_the_same(f_rit, f_rit + ext_size, e_rit_begin, e_rit_end))
            {
                // Ensure the matched string is not in the middle of a word (checking the previous character (the right character -- left to right)).
                if (f_rit != filename.crbegin() && *(f_rit - 1) != '.')
                {
                    continue;
                }

                // If the matched string is at begining of the filename and there is no character in its left side!
                if (f_rit == filename.crbegin())
                {
                    return true;
                }

                // Check there is no other extensions in the matched extension's right side (like *.so.3.gz)
                for (std::string::const_reverse_iterator after_rit = f_rit - 1; after_rit != filename.crbegin() - 1;
                     --after_rit)
                {
                    if (char ch = *after_rit; std::isdigit(ch) == 0 && ch != '.')
                    {
                        return false;
                    }
                }
                return true;
            }
        }
    }
    return false;
}

bool naiveMatch(const std::string &filename, const std::vector<std::string> &exts)
{
    return std::any_of(exts.begin(), exts.end(), [&filename](const std::string& ext) -> bool {
        return filename_has_extension(filename, ext);
    });
}
