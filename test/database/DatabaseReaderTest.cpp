#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

#include <unistd.h>

#include <symfind/config/Config.h>
#include <symfind/database/DatabaseReader.h>
#include <symfind/core/Symbol.h>

namespace SymFind
{

// Forward declaration of the file-local free function exercised below; it is
// deliberately not exposed through any header because callers only ever hand it
// a power of two (see build_trigram_index).
std::uint32_t get_trigram_capacity_bits(std::uint32_t trigram_cap) noexcept;

namespace
{

// Builds a ConfigParser whose database_path is the empty string. An empty path
// makes the reader's open() fail immediately, which is the only hermetic way to
// exercise the failure path without touching /var/lib/symfind/symfind.db.
ConfigParserPtr MakeConfigWithEmptyPaths()
{
    static int counter = 0;
    const auto path = std::filesystem::temp_directory_path() /
                      ("symfind_reader_test_" + std::to_string(::getpid()) + "_" + std::to_string(++counter) + ".json");
    {
        std::ofstream file(path);
        file << R"({ "database_path": "", "database_scan_path": "" })";
    }

    auto conf = std::make_shared<ConfigParser>();
    std::string err;
    const bool ok = conf->parse(path, &err); // NOLINT
    if (!ok)
    {
        ADD_FAILURE() << "config parse failed: " << err;
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    return conf;
}

// The canonical ignore callback: keep everything.
bool KeepAllEntries(const SymbolEntryView & /*entry*/)
{
    return false;
}

} // namespace

// =============================================================================
// 1. get_trigram_capacity_bits() tests
// =============================================================================
//
// get_trigram_capacity_bits() maps a slot-table capacity (a power of two) to
// the number of top bits used by the multiplicative hash:
//   0 <-> 0, and a power of two n <-> log2(n).
// For non-powers of two it reports std::countr_zero's value.

TEST(TrigramCapacityBitsTest, ZeroMapsToZero)
{
    EXPECT_EQ(get_trigram_capacity_bits(0), 0u);
}

TEST(TrigramCapacityBitsTest, PowersOfTwoMapToTheirExponent)
{
    EXPECT_EQ(get_trigram_capacity_bits(1), 0u);
    EXPECT_EQ(get_trigram_capacity_bits(2), 1u);
    EXPECT_EQ(get_trigram_capacity_bits(4), 2u);
    EXPECT_EQ(get_trigram_capacity_bits(8), 3u);
    EXPECT_EQ(get_trigram_capacity_bits(16), 4u);
    EXPECT_EQ(get_trigram_capacity_bits(256), 8u);
    EXPECT_EQ(get_trigram_capacity_bits(65536), 16u);
    EXPECT_EQ(get_trigram_capacity_bits(1u << 31), 31u);
}

TEST(TrigramCapacityBitsTest, NonPowerOfTwoReportsCountrZero)
{
    // std::countr_zero(6) == 1, std::countr_zero(12) == 2. These values never
    // come from build_trigram_index() (capacities are always powers of two),
    // but the forward-declared function must behave deterministically anyway.
    EXPECT_EQ(get_trigram_capacity_bits(6), 1u);
    EXPECT_EQ(get_trigram_capacity_bits(12), 2u);
}

// =============================================================================
// 2. find_symbol_references() failure paths
// =============================================================================

TEST(FindSymbolReferencesTest, ReturnsNulloptAndErrWhenDatabaseCannotOpen)
{
    const auto conf = MakeConfigWithEmptyPaths();

    std::string err;
    const auto result = DatabaseReader::find_symbol_references(conf, "printf", KeepAllEntries, &err);

    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(err.empty());
}

TEST(FindSymbolReferencesTest, ReturnsNulloptWithNullErrMsgWithoutCrashing)
{
    const auto conf = MakeConfigWithEmptyPaths();

    const auto result = DatabaseReader::find_symbol_references(conf, "printf", KeepAllEntries, nullptr);

    EXPECT_FALSE(result.has_value());
}

// =============================================================================
// 3. print_symbol_lookup_results() no-crash tests
// =============================================================================

namespace
{

DatabaseReader::SymbolLookupResultList MakeLookupResults()
{
    DatabaseReader::SymbolLookupResultList list;

    DatabaseReader::SymbolLookupResult first;
    first.symbol_name = "foo";
    first.references.push_back(DatabaseReader::FileReference{
        .full_path = "/usr/lib/liba.so",
        .metadata = {true, SymbolSourceSection::SYMTAB, SymbolType::FUNC, SymbolBind::GLOBAL, SymbolVisibility::DEFAULT, 0x10},
    });
    first.references.push_back(DatabaseReader::FileReference{
        .full_path = "/lib/libb.so",
        .metadata = {false, SymbolSourceSection::DYNSYM, SymbolType::DATA_OBJ, SymbolBind::WEAK, SymbolVisibility::HIDDEN, 0x20},
    });
    list.push_back(std::move(first));

    DatabaseReader::SymbolLookupResult orphan;
    orphan.symbol_name = "no_refs";
    list.push_back(std::move(orphan)); // zero references

    return list;
}

} // namespace

TEST(PrintSymbolLookupResultsTest, EmptyResultListDoesNotCrash)
{
    const DatabaseReader::SymbolLookupResultList empty;
    DatabaseReader::print_symbol_lookup_results(empty);
}

TEST(PrintSymbolLookupResultsTest, PopulatedResultListPrintsAllReferences)
{
    DatabaseReader::SymbolLookupResultList results = MakeLookupResults();
    DatabaseReader::print_symbol_lookup_results(results);

    DatabaseReader::SymbolLookupResultList single;
    DatabaseReader::SymbolLookupResult res;
    res.symbol_name = "solo";
    res.references.push_back(DatabaseReader::FileReference{
        .full_path = "/usr/lib/solo.so",
        .metadata = {true, SymbolSourceSection::SYMTAB, SymbolType::FUNC, SymbolBind::GLOBAL, SymbolVisibility::DEFAULT, 0x1},
    });
    single.push_back(std::move(res));
    DatabaseReader::print_symbol_lookup_results(single);
}

} // namespace SymFind