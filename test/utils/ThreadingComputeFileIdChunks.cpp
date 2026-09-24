//
// Unit tests for the file-id distribution ("chunking") logic.
//

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <numeric>
#include <vector>

#include <symfind/core/SymFinder.h>

namespace SymFind
{

TEST(ThreadingTest, ProducesExactlyTChunks)
{
    auto chunks = compute_file_id_chunks(/*N=*/100, /*T=*/4);
    EXPECT_EQ(chunks.size(), 4u);
}

TEST(ThreadingTest, EvenDivisionSplitsEqually)
{
    auto chunks = compute_file_id_chunks(/*N=*/100, /*T=*/4);
    for (const auto &chunk : chunks)
    {
        EXPECT_EQ(chunk.size(), 25u);
    }
}

TEST(ThreadingTest, RemainderGoesToEarliestChunksOnly)
{
    // 10 files over 3 threads -> base=3, remainder=1: sizes should be 4,3,3.
    auto chunks = compute_file_id_chunks(/*N=*/10, /*T=*/3);
    ASSERT_EQ(chunks.size(), 3u);
    EXPECT_EQ(chunks[0].size(), 4u);
    EXPECT_EQ(chunks[1].size(), 3u);
    EXPECT_EQ(chunks[2].size(), 3u);
}

TEST(ThreadingTest, ChunksCoverEveryIdExactlyOnceInOrder)
{
    const std::size_t N = 47;
    const std::size_t T = 5;
    auto chunks = compute_file_id_chunks(N, T);

    std::vector<std::uint32_t> flattened;
    for (const auto &chunk : chunks)
        flattened.insert(flattened.end(), chunk.begin(), chunk.end());
    ASSERT_EQ(flattened.size(), N);

    std::vector<std::uint32_t> expected(N);
    std::iota(expected.begin(), expected.end(), 0u);
    EXPECT_EQ(flattened, expected);
}

TEST(ThreadingTest, FewerFilesThanThreadsLeavesTrailingChunksEmpty)
{
    // 3 files over 5 threads -> base=0, remainder=3: sizes should be 1,1,1,0,0.
    auto chunks = compute_file_id_chunks(/*N=*/3, /*T=*/5);
    ASSERT_EQ(chunks.size(), 5u);
    EXPECT_EQ(chunks[0].size(), 1u);
    EXPECT_EQ(chunks[1].size(), 1u);
    EXPECT_EQ(chunks[2].size(), 1u);
    EXPECT_EQ(chunks[3].size(), 0u);
    EXPECT_EQ(chunks[4].size(), 0u);

    EXPECT_EQ(chunks[0][0], 0u);
    EXPECT_EQ(chunks[1][0], 1u);
    EXPECT_EQ(chunks[2][0], 2u);
}

TEST(ThreadingTest, ZeroFilesProducesAllEmptyChunks)
{
    auto chunks = compute_file_id_chunks(/*N=*/0, /*T=*/4);
    ASSERT_EQ(chunks.size(), 4u);
    for (const auto &chunk : chunks)
    {
        EXPECT_TRUE(chunk.empty());
    }
}

TEST(ThreadingTest, SingleThreadGetsEverything)
{
    auto chunks = compute_file_id_chunks(/*N=*/17, /*T=*/1);
    ASSERT_EQ(chunks.size(), 1u);
    EXPECT_EQ(chunks[0].size(), 17u);
    EXPECT_EQ(chunks[0].front(), 0u);
    EXPECT_EQ(chunks[0].back(), 16u);
}

TEST(ThreadingTest, ChunksAreContiguousAndAscending)
{
    auto chunks = compute_file_id_chunks(/*N=*/33, /*T=*/6);
    std::uint32_t expected_next = 0;
    for (const auto &chunk : chunks)
    {
        for (std::uint32_t id : chunk)
        {
            EXPECT_EQ(id, expected_next);
            ++expected_next;
        }
    }
    EXPECT_EQ(expected_next, 33u);
}

} // namespace SymFind
