#include <chrono>
#include <iostream>
#include <vector>

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include "benchmark_dataset.h"
#include "demangler_allocate_buffer_everytime.h"
#include "demangler_reuse_buffer.h"


// Allocate something much larger than L3 cache
std::vector<uint64_t> trash_buffer(64 * 1024 * 1024 / sizeof(uint64_t)); // ~64-256 MB

void flush_cache()
{
    // Touch every cache line
    for (size_t i = 0; i < trash_buffer.size(); i += 64 / sizeof(uint64_t))
    {
        trash_buffer[i] += 1; // Write + read
    }

    // Optional: prevent compiler from optimizing away
    volatile uint64_t sink = trash_buffer[0];
    (void)sink;
}

class Timer
{
public:
    Timer()
    {
        _start_timepoint = std::chrono::high_resolution_clock::now();
    }

    ~Timer()
    {
        auto stop_timepoint = std::chrono::high_resolution_clock::now();

        auto start =
            std::chrono::time_point_cast<std::chrono::microseconds>(_start_timepoint).time_since_epoch().count();
        auto stop = std::chrono::time_point_cast<std::chrono::microseconds>(stop_timepoint).time_since_epoch().count();

        auto duration = stop - start;
        std::cout << "\tDuration : " << duration << " (us)\n";
    }

private:
    std::chrono::time_point<std::chrono::high_resolution_clock> _start_timepoint;
};

void DoNotOptimize(void* ptr) {
    asm volatile("" : : "g"(ptr) : "memory");
}

// Tracking memory allocations
// void* operator new(size_t n) {
//     std::cout << "Allocaing " << n << " memory\n";
//     return std::malloc(n);
// }

int main()
{
    std::cout << "Running demangler function on dataset...\n";
    {
        flush_cache();
        Timer tm;
        for (const auto &sym : kMangledSymbols)
        {
            std::string demangled_sym = demangle_unique_ptr(sym);
            DoNotOptimize(&demangled_sym);
        }
    }

    std::cout << "=================================\n";

    std::cout << "Running demangler class on dataset...\n";
    {
        Demangler demangler;
        flush_cache();

        Timer tm;
        for (const auto &sym : kMangledSymbols)
        {
            std::string demangled_sym = demangler.demangle(sym);
            DoNotOptimize(&demangled_sym);
        }
    }

    return 0;
}
