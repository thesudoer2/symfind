#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <string>

#include <unistd.h>

#include <symfind/database/Database.h>

namespace SymFind
{

// string_to_hex() is defined in Database.cpp but deliberately not declared in
// Database.h; the magic-value tests below forward-declare it directly.
std::uint64_t string_to_hex(const char arr[], int size);

namespace
{

// Creates a unique, writable temp directory for a test and removes it (with
// everything inside) when the guard goes out of scope.
std::filesystem::path MakeTempDir()
{
    const std::filesystem::path base = std::filesystem::temp_directory_path();
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const auto candidate = base / ("symfind_db_test_" + std::to_string(::getpid()) + "_" + std::to_string(attempt));
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

} // namespace

// =============================================================================
// 1. string_to_hex() tests
// =============================================================================
//
// string_to_hex() packs `size` bytes big-endian into a std::uint64_t,
// shifting left by 8 per byte. It is not NUL-terminated: it reads exactly
// `size` bytes regardless of embedded NULs. This is what Database.cpp.17 uses
// to derive DATABASE_HEADER_MAGIC from the "symfind\0" byte sequence.

TEST(StringToHexTest, EmptyInputReturnsZero)
{
    EXPECT_EQ(string_to_hex("", 0), 0u);
}

TEST(StringToHexTest, SizeZeroAlwaysReturnsZero)
{
    const char bytes[] = {'A', 'B'};
    EXPECT_EQ(string_to_hex(bytes, 0), 0u);
}

TEST(StringToHexTest, SingleBytePacksAsItsByteValue)
{
    const char bytes[] = {'A'};
    EXPECT_EQ(string_to_hex(bytes, 1), 0x41u);
}

TEST(StringToHexTest, TwoBytesAreConcatenatedBigEndian)
{
    const char bytes[] = {'a', 'B'};
    EXPECT_EQ(string_to_hex(bytes, 2), 0x6142u);
}

TEST(StringToHexTest, AccumulatesLeftToRightOverManyBytes)
{
    // "hi!" -> 0x68 | (0x69<<8) going up: 0x68, then 0x6869, then 0x686921
    const char bytes[] = {'h', 'i', '!'};
    EXPECT_EQ(string_to_hex(bytes, 3), 0x686921u);
}

TEST(StringToHexTest, NulBytesArePlainDataNotTerminators)
{
    const char bytes[] = {'a', '\0', 'b'};
    EXPECT_EQ(string_to_hex(bytes, 3), 0x610062u);
}

TEST(StringToHexTest, MatchesDatabaseHeaderMagic)
{
    // DATABASE_HEADER_MAGIC is the 8 byte sequence "symfind\0" read big-endian.
    const char magic_bytes[] = {'s', 'y', 'm', 'f', 'i', 'n', 'd', '\0'};
    EXPECT_EQ(string_to_hex(magic_bytes, 8), DATABASE_HEADER_MAGIC);
}

// =============================================================================
// 2. Database open-state and operator tests
// =============================================================================
//
// Database wraps a FileWrapper opened with a mode string (default "wb+"). The
// state operators are thin passthroughs to FileWrapper::is_open():
//   bool(db)       -> is_open
//   !db            -> !is_open
//   db != b        -> is_open != b

TEST(DatabaseOpenTest, OpensWritableTempFileWithExplicitMode)
{
    TempDirGuard guard;
    Database db((guard.path / "test.db").string(), "wb+");

    EXPECT_EQ(static_cast<bool>(db), true);
    EXPECT_EQ(!db, false);
    EXPECT_EQ(db != false, true); // is_open != false
    EXPECT_EQ(db != true, false); // is_open != true
}

TEST(DatabaseOpenTest, OpensWithDefaultMode)
{
    // Default mode is "wb+" (create/truncate).
    TempDirGuard guard;
    Database db((guard.path / "default_mode.db").string());

    EXPECT_EQ(static_cast<bool>(db), true);
}

TEST(DatabaseOpenTest, OpensExistingFileInReadMode)
{
    TempDirGuard guard;
    const std::string path = (guard.path / "readonly.db").string();
    {
        Database create(path, "wb+");
        EXPECT_TRUE(static_cast<bool>(create));
    }

    Database reader(path, "rb");
    EXPECT_EQ(static_cast<bool>(reader), true);
    EXPECT_EQ(reader != false, true);
}

TEST(DatabaseOpenTest, FailsToOpenWhenParentDirectoryDoesNotExist)
{
    const std::string missing_parent =
        (std::filesystem::temp_directory_path() /
         ("symfind_missing_" + std::to_string(::getpid()) + "_dir")) / "x.db";

    // Make sure the parent directory truly does not exist.
    std::error_code ec;
    std::filesystem::remove_all(std::filesystem::path(missing_parent).parent_path(), ec);

    Database db(missing_parent, "wb+");

    EXPECT_EQ(static_cast<bool>(db), false);
    EXPECT_EQ(!db, true);
    EXPECT_EQ(db != false, false); // is_open(false) != false
    EXPECT_EQ(db != true, true);   // is_open(false) != true
    EXPECT_NE(db.get_errno(), 0);  // fopen() failed with a reason
}

TEST(DatabaseOpenTest, OperatorConsistencyAcrossState)
{
    TempDirGuard guard;

    Database open_db((guard.path / "a.db").string(), "wb+");
    Database closed_db((guard.path / "missing_dir" / "b.db").string(), "wb+");

    // For an open database every operator agrees with is_open() == true.
    EXPECT_EQ(static_cast<bool>(open_db), true);
    EXPECT_EQ(!open_db, false);

    // For a failed open every operator agrees with is_open() == false.
    EXPECT_EQ(static_cast<bool>(closed_db), false);
    EXPECT_EQ(!closed_db, true);
    EXPECT_NE(closed_db.get_errno(), 0);
}

} // namespace SymFind