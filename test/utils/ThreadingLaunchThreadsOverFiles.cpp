#include <gtest/gtest.h>

#include <symfind/utils/Threading.h>

#include <algorithm>
#include <cstdint>
#include <mutex>
#include <numeric>
#include <vector>

namespace SymFind
{

namespace
{

struct CapturedCall
{
    SymFind::FileIDList file_ids;
    std::size_t found_files_size{};
    int marker{};
    bool flag{};
};

// Thread-safe sink: each worker invocation (running on its own std::jthread)
// records into this under a mutex. Safe to read only after all jthreads have
// been joined (i.e. after the owning ThreadList has been cleared/destroyed).
struct ResultCollector
{
    std::mutex mtx;
    std::vector<CapturedCall> calls;

    void record(SymFind::FileIDList file_ids, std::size_t found_files_size, int marker, bool flag)
    {
        std::lock_guard<std::mutex> lock(mtx);
        calls.push_back(CapturedCall{std::move(file_ids), found_files_size, marker, flag});
    }
};

std::vector<std::uint32_t> flatten_and_sort_ids(const std::vector<CapturedCall> &calls)
{
    std::vector<std::uint32_t> all_ids;
    for (const auto &call : calls)
    {
        all_ids.insert(all_ids.end(), call.file_ids.begin(), call.file_ids.end());
    }
    std::sort(all_ids.begin(), all_ids.end());
    return all_ids;
}

} // namespace (anonymouse)

TEST(LaunchThreadsOverFilesTest, SpawnsExactlyTWorkersAndCoversAllFileIds)
{
    ResultCollector collector;
    std::vector<int> fake_found_files(23); // stand-in for FSScanner::FileList; only .size() is used

    auto worker =
        [&collector](SymFind::FileIDList file_ids, const std::vector<int> &found_files, int marker, bool flag) {
            collector.record(std::move(file_ids), found_files.size(), marker, flag);
        };

    SymFind::ThreadList thread_list;
    const std::size_t T = 4;
    thread_list.reserve(T);

    SymFind::launch_threads_over_files(thread_list, fake_found_files, worker, /*marker=*/42, /*flag=*/true);

    thread_list.clear(); // std::jthread joins on destruction -> all workers finished after this line

    ASSERT_EQ(collector.calls.size(), T);

    for (const auto &call : collector.calls)
    {
        EXPECT_EQ(call.found_files_size, 23u);
        EXPECT_EQ(call.marker, 42);
        EXPECT_TRUE(call.flag);
    }

    auto all_ids = flatten_and_sort_ids(collector.calls);
    std::vector<std::uint32_t> expected(23);
    std::iota(expected.begin(), expected.end(), 0u);
    EXPECT_EQ(all_ids, expected);
}

TEST(LaunchThreadsOverFilesTest, RemainderIsDistributedOneEachToEarliestThreads)
{
    // 10 files / 3 threads -> base=3, remainder=1: chunk sizes 4, 3, 3.
    ResultCollector collector;
    std::vector<int> fake_found_files(10);

    auto worker =
        [&collector](SymFind::FileIDList file_ids, const std::vector<int> &found_files, int marker, bool flag) {
            collector.record(std::move(file_ids), found_files.size(), marker, flag);
        };

    SymFind::ThreadList thread_list;
    thread_list.reserve(3);
    SymFind::launch_threads_over_files(thread_list, fake_found_files, worker, /*marker=*/0, /*flag=*/false);
    thread_list.clear();

    ASSERT_EQ(collector.calls.size(), 3u);
    std::vector<std::size_t> sizes;
    for (const auto &call : collector.calls)
    {
        sizes.push_back(call.file_ids.size());
    }
    std::sort(sizes.rbegin(), sizes.rend()); // descending, since order across threads isn't guaranteed
    EXPECT_EQ(sizes, (std::vector<std::size_t>{4, 3, 3}));
}

TEST(LaunchThreadsOverFilesTest, FewerFilesThanThreadsLeavesSomeChunksEmpty)
{
    ResultCollector collector;
    std::vector<int> fake_found_files(3);

    auto worker =
        [&collector](SymFind::FileIDList file_ids, const std::vector<int> &found_files, int marker, bool flag) {
            collector.record(std::move(file_ids), found_files.size(), marker, flag);
        };

    SymFind::ThreadList thread_list;
    thread_list.reserve(5);
    SymFind::launch_threads_over_files(thread_list, fake_found_files, worker, /*marker=*/0, /*flag=*/false);
    thread_list.clear();

    ASSERT_EQ(collector.calls.size(), 5u);
    auto all_ids = flatten_and_sort_ids(collector.calls);
    EXPECT_EQ(all_ids, (std::vector<std::uint32_t>{0, 1, 2}));

    std::size_t empty_chunks = 0;
    for (const auto &call : collector.calls)
    {
        if (call.file_ids.empty())
        {
            ++empty_chunks;
        }
    }
    EXPECT_EQ(empty_chunks, 2u);
}

TEST(LaunchThreadsOverFilesTest, ZeroFilesStillSpawnsTEmptyWorkers)
{
    ResultCollector collector;
    std::vector<int> fake_found_files; // N = 0

    auto worker =
        [&collector](SymFind::FileIDList file_ids, const std::vector<int> &found_files, int marker, bool flag) {
            collector.record(std::move(file_ids), found_files.size(), marker, flag);
        };

    SymFind::ThreadList thread_list;
    thread_list.reserve(4);
    SymFind::launch_threads_over_files(thread_list, fake_found_files, worker, /*marker=*/0, /*flag=*/false);
    thread_list.clear();

    ASSERT_EQ(collector.calls.size(), 4u);
    for (const auto &call : collector.calls)
    {
        EXPECT_TRUE(call.file_ids.empty());
    }
}

// Death test: launch_threads_over_files calls exit(EXIT_FAILURE) if thread_list's
// capacity is 0 (i.e. reserve() was never called). EXPECT_EXIT runs the given
// statement in a forked subprocess, so it can safely test a path that
// terminates the process without killing the test binary itself.
TEST(LaunchThreadsOverFilesTest, ExitsWithFailureIfThreadListCapacityIsZero)
{
    EXPECT_EXIT(
        {
            std::vector<int> fake_found_files(5);
            auto worker = [](SymFind::FileIDList, const std::vector<int> &, int) {};

            SymFind::ThreadList thread_list; // never reserved -> capacity() == 0
            SymFind::launch_threads_over_files(thread_list, fake_found_files, worker, /*marker=*/0);
        },
        ::testing::ExitedWithCode(EXIT_FAILURE),
        "Thread list is not reserved!");
}

} // namespace SymFind
