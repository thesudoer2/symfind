#pragma once

#include <random>
#include <string>
#include <vector>

// -- Test-data helpers -------------------------------------------------------

static const std::vector<std::string> kExtensions = {".so", ".a", ".o"};

// A realistic mix: ~50 % matching, various lengths
static std::vector<std::string> makeFilenames(std::size_t n, unsigned seed = 42)
{
    static const std::vector<std::string> pool = {
        // matching
        "libfoo.so",
        "libbar.a",
        "main.o",
        "libc.so.6",
        "libtest.a.6",
        "someobj.o.6",
        "libssl.so.3",
        "libcrypto.a",
        "util.o",
        "libc.so.6.1.0",
        "libz.so.1",
        // non-matching
        "somelib.solo",
        "readme.txt",
        "noext",
        "libfoo.so2",
        "file.so2.6",
        "image.png",
        "archive.tar.gz",
        "Makefile",
        "CMakeLists.txt",
        "libfoo.dylib",
    };

    std::mt19937 rng(seed);
    std::uniform_int_distribution<std::size_t> dist(0, pool.size() - 1);
    std::vector<std::string> out(n);
    for (auto &str : out)
    {
        str = pool[dist(rng)];
    }

    return out;
}
