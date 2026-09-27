#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include <symfind/database/DatabaseWriter.h>
#include <symfind/filesystem/FileWrapper.h>

namespace SymFind
{

namespace
{

// Creates a unique, writable temp directory for a test and removes it (with
// everything inside) when the guard goes out of scope.
std::filesystem::path MakeTempDir()
{
    const std::filesystem::path base = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const auto candidate = base / ("symfind_writer_test_" + std::to_string(::getpid()) + "_" + std::to_string(attempt));
        std::error_code ec;
        if (std::filesystem::create_directories(candidate, ec))
        {
            return candidate;
        }
    }
    return base;
}

struct TempDirGuard
{
    std::filesystem::path path{MakeTempDir()};
    ~TempDirGuard()
    {
        std::error_code ec;
        std::filesystem::remove_all(path, ec);
    }
};

// Reads an entire file into a std::string, preserving embedded NUL bytes.
std::string ReadFileAsString(const std::filesystem::path &file_path)
{
    std::ifstream file(file_path, std::ios::binary);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

} // namespace

// =============================================================================
// 1. Basic write behavior
// =============================================================================
//
// write_database_buffer() writes `len` bytes at `offset` (pwrite semantics) and,
// on success, advances the caller's offset by `len`. The bytes are written as-is
// to the underlying file, so the file content must come back unchanged.

TEST(DatabaseWriterTest, OpenWriterIsValid)
{
    TempDirGuard guard;
    DatabaseWriter writer((guard.path / "db.bin").string());

    EXPECT_EQ(static_cast<bool>(writer), true);
    EXPECT_EQ(!writer, false);
}

TEST(DatabaseWriterTest, WriteCreatesFileOnDisk)
{
    TempDirGuard guard;
    const auto file_path = guard.path / "created.db";
    DatabaseWriter writer(file_path.string());

    EXPECT_TRUE(std::filesystem::exists(file_path));
    EXPECT_EQ(std::filesystem::file_size(file_path), 0u); // "wb+" truncates
}

TEST(DatabaseWriterTest, WritesBufferAndAdvancesOffset)
{
    TempDirGuard guard;
    DatabaseWriter writer((guard.path / "db.bin").string());

    FileWrapper::Offset_t offset{0};
    const char part1[] = "hello";
    const char part2[] = " world";

    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, part1, sizeof(part1) - 1, offset));
    EXPECT_EQ(offset, static_cast<FileWrapper::Offset_t>(sizeof(part1) - 1));

    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, part2, sizeof(part2) - 1, offset));
    EXPECT_EQ(offset, static_cast<FileWrapper::Offset_t>(sizeof(part1) - 1 + sizeof(part2) - 1));

    EXPECT_EQ(ReadFileAsString(guard.path / "db.bin"), "hello world");
}

TEST(DatabaseWriterTest, MultipleFragmentedWritesProduceContiguousFile)
{
    TempDirGuard guard;
    DatabaseWriter writer((guard.path / "db.bin").string());

    FileWrapper::Offset_t offset{0};
    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, "aa", 2, offset));
    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, "bbb", 3, offset));
    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, "", 0, offset));
    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, "cccc", 4, offset));

    EXPECT_EQ(offset, 9u);
    EXPECT_EQ(ReadFileAsString(guard.path / "db.bin"), "aabbbcccc");
}

TEST(DatabaseWriterTest, WriteAtExplicitOffsetOverwrites)
{
    TempDirGuard guard;
    DatabaseWriter writer((guard.path / "db.bin").string());

    FileWrapper::Offset_t offset{0};
    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, "abc", 3, offset));
    EXPECT_EQ(offset, 3u);

    // Overwrite bytes 1..2 with "XY" -> file becomes "aXY".
    FileWrapper::Offset_t at_one{1};
    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, "XY", 2, at_one));
    EXPECT_EQ(at_one, 3u);

    EXPECT_EQ(ReadFileAsString(guard.path / "db.bin"), "aXY");
}

// =============================================================================
// 2. Binary data handling
// =============================================================================

TEST(DatabaseWriterTest, WritesArbitraryBinaryPreservingNulBytes)
{
    TempDirGuard guard;
    DatabaseWriter writer((guard.path / "db.bin").string());

    const std::vector<std::uint8_t> data{0x00, 0x01, 0x7F, 0xFF, 0x00, 0x41, 0x00};
    FileWrapper::Offset_t offset{0};

    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, data.data(), data.size(), offset));
    EXPECT_EQ(offset, data.size());

    const std::string actual = ReadFileAsString(guard.path / "db.bin");
    ASSERT_EQ(actual.size(), data.size());
    EXPECT_TRUE(std::equal(actual.begin(),
                           actual.end(),
                           data.begin(),
                           [](char a, std::uint8_t b) { return static_cast<unsigned char>(a) == b; }));
}

TEST(DatabaseWriterTest, LargeWriteRoundTrips)
{
    TempDirGuard guard;
    DatabaseWriter writer((guard.path / "db.bin").string());

    std::vector<std::uint8_t> data(1024 * 1024);
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        data[i] = static_cast<std::uint8_t>((i * 31 + 7) & 0xFFu);
    }

    FileWrapper::Offset_t offset{0};
    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, data.data(), data.size(), offset));
    EXPECT_EQ(offset, data.size());

    const std::string actual = ReadFileAsString(guard.path / "db.bin");
    ASSERT_EQ(actual.size(), data.size());
    EXPECT_TRUE(std::equal(actual.begin(),
                           actual.end(),
                           data.begin(),
                           [](char a, std::uint8_t b) { return static_cast<unsigned char>(a) == b; }));
}

TEST(DatabaseWriterTest, ZeroLengthWriteSucceedsWithoutAdvancingOffset)
{
    TempDirGuard guard;
    DatabaseWriter writer((guard.path / "db.bin").string());

    FileWrapper::Offset_t offset{0};
    EXPECT_TRUE(DatabaseWriter::write_database_buffer(writer, static_cast<const void *>(""), 0, offset));
    EXPECT_EQ(offset, 0u);
    EXPECT_EQ(ReadFileAsString(guard.path / "db.bin"), "");
}

// =============================================================================
// 3. Failure paths
// =============================================================================

TEST(DatabaseWriterTest, WriteToUnopenableDatabaseReturnsFalseAndSetsErrMsg)
{
    DatabaseWriter writer("/nonexistent_dir_that_does_not_exist_symfind/db.bin");

    FileWrapper::Offset_t offset{0};
    std::string err_msg;
    const char data[] = "x";

    EXPECT_FALSE(DatabaseWriter::write_database_buffer(writer, data, 1, offset, &err_msg));
    EXPECT_EQ(offset, 0u); // offset must not advance on failure
    EXPECT_FALSE(err_msg.empty());
}

TEST(DatabaseWriterTest, WriteToUnopenableDatabaseWithNullErrMsgDoesNotCrash)
{
    DatabaseWriter writer("/nonexistent_dir_that_does_not_exist_symfind/db.bin");

    FileWrapper::Offset_t offset{0};
    EXPECT_FALSE(DatabaseWriter::write_database_buffer(writer, "x", 1, offset, nullptr));
    EXPECT_EQ(offset, 0u);
}

} // namespace SymFind