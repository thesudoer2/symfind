#include <benchmark/benchmark.h>
#include <string>
#include <vector>
#include <queue>
#include <array>
#include <random>
#include <sstream>

#include "iterate_filename.h"
#include "aho_corasick.h"


// -- Fixture so the automaton is built only once per benchmark ---------------

static const AhoCorasickHashMap kACH          = buildMatcherHashMap();
static const AhoCorasickArray kACA          = buildMatcherArray();
static const ReverseAhoCorasickArray rkACA          = buildMatcherArrayReverse();

static const auto         kFilenames10  = makeFilenames(10);
static const auto         kFilenames100 = makeFilenames(100);
static const auto         kFilenames10k = makeFilenames(10'000);
static const auto         kFilenames100k = makeFilenames(100'000);


// -- Benchmarks --------------------------------------------------------------

// 1. Build cost (trie construction + failure links)
static void BM_Build(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(buildMatcherHashMap());
}
BENCHMARK(BM_Build);

// 2. Single-query: one filename at a time (cache-warm automaton)
static void BM_SingleQuery_AhoCorasickHashMap(benchmark::State& state) {
    const auto& files = kFilenames10k;
    std::size_t i = 0;
    for (auto _ : state) {
        bool r = kACH.matches(files[i % files.size()]);
        benchmark::DoNotOptimize(r);
        ++i;
    }
}
BENCHMARK(BM_SingleQuery_AhoCorasickHashMap);

static void BM_SingleQuery_AhoCorasickArray(benchmark::State& state) {
    const auto& files = kFilenames10k;
    std::size_t i = 0;
    for (auto _ : state) {
        bool r = kACA.matches(files[i % files.size()]);
        benchmark::DoNotOptimize(r);
        ++i;
    }
}
BENCHMARK(BM_SingleQuery_AhoCorasickArray);

static void BM_SingleQuery_ReverseAhoCorasickArray(benchmark::State& state) {
    const auto& files = kFilenames10k;
    std::size_t i = 0;
    for (auto _ : state) {
        bool r = rkACA.matches(files[i % files.size()]);
        benchmark::DoNotOptimize(r);
        ++i;
    }
}
BENCHMARK(BM_SingleQuery_ReverseAhoCorasickArray);

static void BM_SingleQuery_Naive(benchmark::State& state) {
    const auto& files = kFilenames10k;
    std::size_t i = 0;
    for (auto _ : state) {
        bool r = naiveMatch(files[i % files.size()], kExtensions);
        benchmark::DoNotOptimize(r);
        ++i;
    }
}
BENCHMARK(BM_SingleQuery_Naive);

// 3. Batch queries — vary batch size to show scaling
template<const std::vector<std::string>* Files>
static void BM_Batch_AhoCorasickHashMap(benchmark::State& state) {
    for (auto _ : state) {
        bool any = false;
        for (const auto& f : *Files)
            any |= kACH.matches(f);
        benchmark::DoNotOptimize(any);
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(Files->size()));
}
BENCHMARK_TEMPLATE(BM_Batch_AhoCorasickHashMap, &kFilenames10);
BENCHMARK_TEMPLATE(BM_Batch_AhoCorasickHashMap, &kFilenames100);
BENCHMARK_TEMPLATE(BM_Batch_AhoCorasickHashMap, &kFilenames10k);
BENCHMARK_TEMPLATE(BM_Batch_AhoCorasickHashMap, &kFilenames100k);

template<const std::vector<std::string>* Files>
static void BM_Batch_AhoCorasickArray(benchmark::State& state) {
    for (auto _ : state) {
        bool any = false;
        for (const auto& f : *Files)
            any |= kACA.matches(f);
        benchmark::DoNotOptimize(any);
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(Files->size()));
}
BENCHMARK_TEMPLATE(BM_Batch_AhoCorasickArray, &kFilenames10);
BENCHMARK_TEMPLATE(BM_Batch_AhoCorasickArray, &kFilenames100);
BENCHMARK_TEMPLATE(BM_Batch_AhoCorasickArray, &kFilenames10k);
BENCHMARK_TEMPLATE(BM_Batch_AhoCorasickArray, &kFilenames100k);

template<const std::vector<std::string>* Files>
static void BM_Batch_ReverseAhoCorasickArray(benchmark::State& state) {
    for (auto _ : state) {
        bool any = false;
        for (const auto& f : *Files)
            any |= rkACA.matches(f);
        benchmark::DoNotOptimize(any);
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(Files->size()));
}
BENCHMARK_TEMPLATE(BM_Batch_ReverseAhoCorasickArray, &kFilenames10);
BENCHMARK_TEMPLATE(BM_Batch_ReverseAhoCorasickArray, &kFilenames100);
BENCHMARK_TEMPLATE(BM_Batch_ReverseAhoCorasickArray, &kFilenames10k);
BENCHMARK_TEMPLATE(BM_Batch_ReverseAhoCorasickArray, &kFilenames100k);

template<const std::vector<std::string>* Files>
static void BM_Batch_Naive(benchmark::State& state) {
    for (auto _ : state) {
        bool any = false;
        for (const auto& f : *Files)
            any |= naiveMatch(f, kExtensions);
        benchmark::DoNotOptimize(any);
    }
    state.SetItemsProcessed(state.iterations() *
                            static_cast<int64_t>(Files->size()));
}
BENCHMARK_TEMPLATE(BM_Batch_Naive, &kFilenames10);
BENCHMARK_TEMPLATE(BM_Batch_Naive, &kFilenames100);
BENCHMARK_TEMPLATE(BM_Batch_Naive, &kFilenames10k);
BENCHMARK_TEMPLATE(BM_Batch_Naive, &kFilenames100k);

// 4. Pathological input: very long filename
static void BM_LongFilename_AhoCorasickHashMap(benchmark::State& state) {
    std::string name(static_cast<std::size_t>(state.range(0)), 'x');
    name += ".so";
    for (auto _ : state) {
        bool r = kACH.matches(name);
        benchmark::DoNotOptimize(r);
    }
    state.SetBytesProcessed(state.iterations() *
                            static_cast<int64_t>(name.size()));
}
BENCHMARK(BM_LongFilename_AhoCorasickHashMap)->Range(64, 1 << 16);

static void BM_LongFilename_AhoCorasickArray(benchmark::State& state) {
    std::string name(static_cast<std::size_t>(state.range(0)), 'x');
    name += ".so";
    for (auto _ : state) {
        bool r = kACA.matches(name);
        benchmark::DoNotOptimize(r);
    }
    state.SetBytesProcessed(state.iterations() *
                            static_cast<int64_t>(name.size()));
}
BENCHMARK(BM_LongFilename_AhoCorasickArray)->Range(64, 1 << 16);

static void BM_LongFilename_ReverseAhoCorasickArray(benchmark::State& state) {
    std::string name(static_cast<std::size_t>(state.range(0)), 'x');
    name += ".so";
    for (auto _ : state) {
        bool r = rkACA.matches(name);
        benchmark::DoNotOptimize(r);
    }
    state.SetBytesProcessed(state.iterations() *
                            static_cast<int64_t>(name.size()));
}
BENCHMARK(BM_LongFilename_ReverseAhoCorasickArray)->Range(64, 1 << 16);

static void BM_LongFilename_Naive(benchmark::State& state) {
    std::string name(static_cast<std::size_t>(state.range(0)), 'x');
    name += ".so";
    for (auto _ : state) {
        bool r = naiveMatch(name, kExtensions);
        benchmark::DoNotOptimize(r);
    }
    state.SetBytesProcessed(state.iterations() *
                            static_cast<int64_t>(name.size()));
}
BENCHMARK(BM_LongFilename_Naive)->Range(64, 1 << 16);

// -- Entry point -------------------------------------------------------------
BENCHMARK_MAIN();
