#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

// ankerl::unordered_dense (and robin_hood) must be processed with NORMAL access
// semantics: their nested `class iter_t` template is declared once in a private
// and once in a public section, so `#define private public` would break them.
#include <ankerl/unordered_dense.h>
#include <robin_hood.h>

// White-box unit tests: expose the private members of DatabaseBuilder (in
// particular merge_symtables()) only for this translation unit. The two
// conan headers above are already inside their include guards, so the macro
// below never reaches them.
#define private public
#include <symfind/database/DatabaseBuilder.h>
#undef private

namespace SymFind
{

namespace
{

// Creates a unique temp directory path (used for the throwaway config file).
std::string MakeTempPathPrefix()
{
    return (std::filesystem::temp_directory_path() / ("symfind_builder_test_" + std::to_string(::getpid()))).string();
}

// Builds a ConfigParser whose database_path and database_scan_path are both
// the empty string (ConfigParser::store_config only stores the empty-string
// value for these keys). This keeps every test hermetic: an empty database
// path can never touch the real database at /var/lib/symfind/symfind.db.
ConfigParserPtr MakeConfigWithEmptyPaths()
{
    static int counter = 0;
    const auto path = std::filesystem::path(MakeTempPathPrefix() + "_" + std::to_string(++counter) + ".json");
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

SymbolMetaData MakeDefinedFuncMetadata(std::uint32_t offset)
{
    return SymbolMetaData{true,
                          SymbolSourceSection::SYMTAB,
                          SymbolType::FUNC,
                          SymbolBind::GLOBAL,
                          SymbolVisibility::DEFAULT,
                          offset};
}

bool MetadataEqual(const SymbolMetaData &a, const SymbolMetaData &b)
{
    return a.is_defined == b.is_defined && a.source_section == b.source_section && a.type == b.type &&
           a.bind == b.bind && a.visibility == b.visibility && a.offset == b.offset;
}

bool RefEqual(const SymbolRef &a, const SymbolRef &b)
{
    return a.file_id == b.file_id && MetadataEqual(a.metadata, b.metadata);
}

} // namespace (anonymouse)

// =============================================================================
// 1. Construction behavior
// =============================================================================
//
// The constructor opens Database(_conf->get_database_path()) in write mode, so
// it must never run against the default path (the real system database). With
// an empty database_path the open fails cleanly instead.

TEST(DatabaseBuilderConstructorTest, ConstructsWithoutTouchingRealDatabase)
{
    const auto conf = MakeConfigWithEmptyPaths();
    FSScanner scanner(conf, nullptr, nullptr);
    auto dict_builder = std::make_shared<DictionaryBuilder>(4096);

    DatabaseBuilder builder(conf, scanner, dict_builder);
    // No accessors are exposed; reaching construction alive is the assertion
    // (the ctor is noexcept, so any throw would terminate the binary).
}

TEST(DatabaseBuilderConstructorTest, ConstructsWithNullDictionaryBuilder)
{
    const auto conf = MakeConfigWithEmptyPaths();
    FSScanner scanner(conf, nullptr, nullptr);

    DatabaseBuilder builder(conf, scanner, nullptr);
}

// =============================================================================
// 2. build() end-to-end failure path
// =============================================================================

TEST(DatabaseBuilderBuildTest, BuildReturnsFalseWhenDatabasePathIsEmpty)
{
    const auto conf = MakeConfigWithEmptyPaths();
    FSScanner scanner(conf, nullptr, nullptr);
    DatabaseBuilder builder(conf, scanner, nullptr);

    std::string err;
    EXPECT_FALSE(builder.build(&err));
    EXPECT_FALSE(err.empty()); // store_db must report why it failed
}

// =============================================================================
// 3. merge_symtables() tests
// =============================================================================
//
// merge_symtables() folds several per-thread hash maps into one: new keys get
// their refs moved in (and, while a dictionary builder exists, a single name
// sample), existing keys get the incoming refs appended.

TEST(DatabaseBuilderMergeSymtablesTest, EmptyInputsProduceEmptyResult)
{
    const auto conf = MakeConfigWithEmptyPaths();
    FSScanner scanner(conf, nullptr, nullptr);
    DatabaseBuilder builder(conf, scanner, nullptr);

    HashMapList tables;
    HashMap merged;

    builder.merge_symtables(tables, merged);
    EXPECT_TRUE(merged.empty());

    tables.emplace_back();
    builder.merge_symtables(tables, merged);
    EXPECT_TRUE(merged.empty());
}

TEST(DatabaseBuilderMergeSymtablesTest, DisjointKeysAreCollected)
{
    const auto conf = MakeConfigWithEmptyPaths();
    FSScanner scanner(conf, nullptr, nullptr);
    DatabaseBuilder builder(conf, scanner, nullptr);

    const SymbolMetaData md = MakeDefinedFuncMetadata(0xAA);

    HashMapList tables(2);

    HashMap& table_a = tables[0];
    table_a["alpha"] = {SymbolRef{0, md}};
    table_a["beta"] = {SymbolRef{1, md}, SymbolRef{2, md}};

    HashMap& table_b = tables[1];
    table_b["gamma"] = {SymbolRef{3, md}};


    HashMap merged;
    builder.merge_symtables(tables, merged);

    ASSERT_EQ(merged.size(), 3u);
    ASSERT_EQ(merged.at("alpha").size(), 1u);
    ASSERT_EQ(merged.at("beta").size(), 2u);
    ASSERT_EQ(merged.at("gamma").size(), 1u);

    EXPECT_EQ(merged.at("alpha")[0].file_id, 0u);
    EXPECT_EQ(merged.at("beta")[0].file_id, 1u);
    EXPECT_EQ(merged.at("beta")[1].file_id, 2u);
    EXPECT_EQ(merged.at("gamma")[0].file_id, 3u);
    EXPECT_TRUE(MetadataEqual(merged.at("alpha")[0].metadata, md));
}

TEST(DatabaseBuilderMergeSymtablesTest, SharedKeysConcatenateReferencesInOrder)
{
    const auto conf = MakeConfigWithEmptyPaths();
    FSScanner scanner(conf, nullptr, nullptr);
    DatabaseBuilder builder(conf, scanner, nullptr);

    const SymbolMetaData md_a = MakeDefinedFuncMetadata(0x10);
    const SymbolMetaData md_b{false,
                              SymbolSourceSection::DYNSYM,
                              SymbolType::DATA_OBJ,
                              SymbolBind::WEAK,
                              SymbolVisibility::HIDDEN,
                              0x20};

    HashMapList tables(2);

    HashMap& table_a = tables[0];
    table_a["dup"] = {SymbolRef{1, md_a}, SymbolRef{2, md_a}};
    table_a["only_a"] = {SymbolRef{9, md_a}};

    HashMap& table_b = tables[1];
    table_b["dup"] = {SymbolRef{3, md_b}};
    table_b["only_b"] = {SymbolRef{4, md_b}};

    HashMap merged;
    builder.merge_symtables(tables, merged);

    ASSERT_EQ(merged.size(), 3u);

    const auto &dup = merged.at("dup");
    ASSERT_EQ(dup.size(), 3u); // first table's refs, then second table's
    EXPECT_TRUE(RefEqual(dup[0], SymbolRef{1, md_a}));
    EXPECT_TRUE(RefEqual(dup[1], SymbolRef{2, md_a}));
    EXPECT_TRUE(RefEqual(dup[2], SymbolRef{3, md_b}));

    ASSERT_EQ(merged.at("only_a").size(), 1u);
    EXPECT_TRUE(RefEqual(merged.at("only_a")[0], SymbolRef{9, md_a}));

    ASSERT_EQ(merged.at("only_b").size(), 1u);
    EXPECT_TRUE(RefEqual(merged.at("only_b")[0], SymbolRef{4, md_b}));
}

TEST(DatabaseBuilderMergeSymtablesTest, SamplesUniqueNewSymbolNamesIntoDictionary)
{
    const auto conf = MakeConfigWithEmptyPaths();
    FSScanner scanner(conf, nullptr, nullptr);
    auto dict_builder = std::make_shared<DictionaryBuilder>(4096);
    DatabaseBuilder builder(conf, scanner, dict_builder);

    const SymbolMetaData md = MakeDefinedFuncMetadata(0x1);

    HashMapList tables(2);

    HashMap& table = tables[0];
    for (std::uint32_t i = 0; i < 100; ++i)
    {
        table["network_parse_packet_symbol_" + std::to_string(i)] = {SymbolRef{i, md}};
    }

    // A duplicate across a second table must NOT be sampled twice.
    HashMap& table2 = tables[1];
    table2["network_parse_packet_symbol_0"] = {SymbolRef{200, md}};

    HashMap merged;
    builder.merge_symtables(tables, merged);

    ASSERT_EQ(merged.size(), 100u);
    ASSERT_EQ(merged.at("network_parse_packet_symbol_0").size(), 2u);

    // The dictionary must be trainable from the ~100 samples collected above.
    std::string dictionary;
    std::string err;
    ASSERT_TRUE(dict_builder->train(dictionary, &err)) << err;
    EXPECT_FALSE(dictionary.empty());
}

} // namespace SymFind
