// Unit tests for SymFind::get_proper_thread_count_to_process_list()
//
// The function decides how many worker threads to spin up for a file list of
// a given size, based on HARDWARE_CONCURRENCY_COUNT and a minimum of 20
// files/thread. Since HARDWARE_CONCURRENCY_COUNT is resolved at compile time
// from the actual machine's core count (via SymFind::get_hardware_concurrency()),
// these tests read that same value through the macro rather than hardcoding
// a thread count, so they pass on any build machine / CI runner.

#include <gtest/gtest.h>

#include <symfind/utils/Threading.h>

#include <cstddef>
#include <cstdint>

namespace SymFind
{

namespace
{

// Snapshot of the value this translation unit resolves HARDWARE_CONCURRENCY_COUNT
// to, so every test below can reason about it without assuming a fixed core count.
const std::uint16_t kHwThreads = HARDWARE_CONCURRENCY_COUNT;

} // namespace (anonymouse)

TEST(ThreadCountTest, EmptyListReturnsOneThread)
{
    // 0 files / any thread count == 0 files-per-thread, which is below the
    // MINIMUM_FILES_PER_THREAD (20) threshold, so we fall back to max(1, 0/20) == 1.
    EXPECT_EQ(SymFind::get_proper_thread_count_to_process_list(0), 1);
}

TEST(ThreadCountTest, TinyListReturnsOneThreadRegardlessOfHardware)
{
    // With fewer than 20 files total, list_size / 20 == 0, so
    // max(1, list_size / 20) == 1 no matter how many hardware threads exist.
    EXPECT_EQ(SymFind::get_proper_thread_count_to_process_list(1), 1);
    EXPECT_EQ(SymFind::get_proper_thread_count_to_process_list(15), 1);
    EXPECT_EQ(SymFind::get_proper_thread_count_to_process_list(19), 1);
}

TEST(ThreadCountTest, TwentyFiveFilesStaysAtOneThreadOnAnyHardware)
{
    // 25 files: on a 1-core machine, 25 >= 20 files/thread, so THREADS_COUNT (1)
    // is returned directly. On a >=2-core machine, 25/hw < 20, so we fall through
    // to max(1, 25/20) == 1. Either branch yields 1, so this is hardware-independent.
    EXPECT_EQ(SymFind::get_proper_thread_count_to_process_list(25), 1);
}

TEST(ThreadCountTest, ListJustBelowMinimumPerThreadUsesFallbackFormula)
{
    // Below the per-thread minimum but comfortably nonzero: verify the fallback
    // formula max(1, list_size / MINIMUM_FILES_PER_THREAD) directly, using an
    // input small enough (< 20 * kHwThreads) to guarantee we hit that branch.
    const std::size_t list_size = 39; // 39 / 20 == 1
    if (list_size / kHwThreads < 20)
    {
        EXPECT_EQ(SymFind::get_proper_thread_count_to_process_list(list_size), 1);
    }
}

TEST(ThreadCountTest, LargeListReturnsFullHardwareConcurrency)
{
    // Large enough that every thread still gets >= 20 files even if the
    // machine had far more cores than expected.
    const std::size_t list_size = static_cast<std::size_t>(kHwThreads) * 20 * 4;
    EXPECT_EQ(SymFind::get_proper_thread_count_to_process_list(list_size), kHwThreads);
}

TEST(ThreadCountTest, ExactlyMinimumFilesPerThreadUsesFullConcurrency)
{
    // At exactly the threshold (>=20 files/thread), full concurrency should
    // still be used, since the check is >=, not >.
    const std::size_t list_size = static_cast<std::size_t>(kHwThreads) * 20;
    EXPECT_EQ(SymFind::get_proper_thread_count_to_process_list(list_size), kHwThreads);
}

TEST(ThreadCountTest, NeverReturnsZero)
{
    for (std::size_t n : {std::size_t{0}, std::size_t{1}, std::size_t{7}, std::size_t{20}, std::size_t{1000}})
    {
        EXPECT_GE(SymFind::get_proper_thread_count_to_process_list(n), 1)
            << "list_size=" << n;
    }
}

TEST(ThreadCountTest, NeverExceedsHardwareConcurrency)
{
    for (std::size_t n : {std::size_t{0}, std::size_t{50}, std::size_t{500}, std::size_t{5000}, std::size_t{50000}})
    {
        EXPECT_LE(SymFind::get_proper_thread_count_to_process_list(n), kHwThreads)
            << "list_size=" << n;
    }
}

} // namespace SymFind
