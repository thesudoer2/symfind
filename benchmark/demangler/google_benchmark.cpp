#include <benchmark/benchmark.h>
#include <cstdlib>
#include <cxxabi.h>
#include <memory>
#include <string>

#include "benchmark_dataset.h"
#include "demangler_allocate_buffer_everytime.h"
#include "demangler_reuse_buffer.h"


// -----------------------------------------------------------------------------
// Benchmark A – reusable-buffer Demangler (thread_local, mirrors real usage)
// -----------------------------------------------------------------------------

static void BM_Demangler_Class(benchmark::State &state)
{
    // Mirrors the intended production usage: one object per thread, reused.
    static thread_local Demangler demangler;

    const std::size_t n = kMangledSymbols.size();
    std::size_t idx = 0;

    for (auto _ : state)
    {
        if (n == 0)
        {
            state.SkipWithError("kMangledSymbols is empty");
            break;
        }

        const std::string result = demangler.demangle(kMangledSymbols[idx % n]);
        benchmark::DoNotOptimize(result);
        ++idx;
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Demangler_Class)
    ->Name("BM_Demangler_Class/reuseBuffer")
    ->Threads(1) // single-thread baseline
    ->Threads(4) // multi-thread (each gets its own thread_local instance)
    ->UseRealTime();

// -----------------------------------------------------------------------------
// Benchmark B – unique_ptr / fresh-alloc variant
// -----------------------------------------------------------------------------

static void BM_Demangle_UniquePtr(benchmark::State &state)
{
    const std::size_t n = kMangledSymbols.size();
    std::size_t idx = 0;

    for (auto _ : state)
    {
        if (n == 0)
        {
            state.SkipWithError("kMangledSymbols is empty");
            break;
        }

        const std::string result = demangle_unique_ptr(kMangledSymbols[idx % n]);
        benchmark::DoNotOptimize(result);
        ++idx;
    }

    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}
BENCHMARK(BM_Demangle_UniquePtr)->Name("BM_Demangle_UniquePtr/freshAlloc")->Threads(1)->Threads(4)->UseRealTime();

// -----------------------------------------------------------------------------
// Per-symbol breakdowns (optional, comment out if the dataset is large)
// Useful to see whether one impl wins only on short or only on long names.
// -----------------------------------------------------------------------------

static void BM_Demangler_Class_PerSymbol(benchmark::State &state)
{
    static thread_local Demangler demangler;

    const auto &sym = kMangledSymbols[static_cast<std::size_t>(state.range(0))];

    for (auto _ : state)
    {
        const std::string result = demangler.demangle(sym);
        benchmark::DoNotOptimize(result);
    }

    state.SetLabel(sym.substr(0, 40)); // truncate label for readability
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

static void BM_Demangle_UniquePtr_PerSymbol(benchmark::State &state)
{
    const auto &sym = kMangledSymbols[static_cast<std::size_t>(state.range(0))];

    for (auto _ : state)
    {
        const std::string result = demangle_unique_ptr(sym);
        benchmark::DoNotOptimize(result);
    }

    state.SetLabel(sym.substr(0, 40));
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()));
}

// Register one case per symbol index.
// This runs automatically once kMangledSymbols is populated.
static void RegisterPerSymbolBenchmarks()
{
    for (std::size_t i = 0; i < kMangledSymbols.size(); ++i)
    {
        const auto idx = static_cast<int64_t>(i);
        benchmark::RegisterBenchmark(("BM_Demangler_Class_PerSymbol/" + std::to_string(i)).c_str(),
                                     BM_Demangler_Class_PerSymbol)
            ->Arg(idx)
            ->UseRealTime();

        benchmark::RegisterBenchmark(("BM_Demangle_UniquePtr_PerSymbol/" + std::to_string(i)).c_str(),
                                     BM_Demangle_UniquePtr_PerSymbol)
            ->Arg(idx)
            ->UseRealTime();
    }
}

// -----------------------------------------------------------------------------
// main
// -----------------------------------------------------------------------------

int main(int argc, char **argv)
{
    RegisterPerSymbolBenchmarks();

    benchmark::Initialize(&argc, argv);
    if (benchmark::ReportUnrecognizedArguments(argc, argv))
        return 1;

    benchmark::RunSpecifiedBenchmarks();
    benchmark::Shutdown();
    return 0;
}
