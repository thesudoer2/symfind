#include <gtest/gtest.h>

#include <algorithm>
#include <bit>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <ranges>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include <symfind/config/Config.h>
#include <symfind/database/Database.h>
#include <symfind/database/DatabaseBuilderUtils.h>
#include <symfind/database/DatabaseWriter.h>
#include <symfind/database/ZSTDCompressor.h>
#include <symfind/database/ZSTDDictionary.h>
#include <symfind/core/Symbol.h>
#include <symfind/core/Trigram.h>
#include <symfind/core/TrigramUtils.h>
#include <symfind/filesystem/FSScanner.h>
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
        const auto candidate = base / ("symfind_builderutils_test_" + std::to_string(::getpid()) + "_" + std::to_string(attempt));
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

// Decompresses a zstd frame stored in `compressed` back into its original
// `uncompressed_length` bytes. Returns an empty string on failure.
std::string Decompress(const std::string &compressed, std::uint64_t uncompressed_length)
{
    ZSTDCompressor zstd;
    DecompressionOptions opts{.ddict = nullptr};
    std::string out;
    if (!zstd.decompress(compressed.data(),
                         static_cast<std::uint32_t>(compressed.size()),
                         static_cast<std::size_t>(uncompressed_length),
                         out,
                         opts))
    {
        return {};
    }
    return out;
}

// Decodes the element at `index` (0-based) of a blob of packed structs.
template <typename T>
T StructAt(const std::string &blob, std::size_t element_index)
{
    T value{};
    std::memcpy(&value, blob.data() + element_index * sizeof(T), sizeof(T)); // NOLINT
    return value;
}

std::string SliceAt(const std::string &blob, std::size_t offset, std::size_t length)
{
    return blob.substr(offset, length);
}

// Scans `scan_root` with a fresh FSScanner and returns it (move). The scanner
// object must outlive anything that holds a reference to it.
FSScanner MakeScannedScanner(const std::filesystem::path &scan_root)
{
    auto conf = std::make_shared<ConfigParser>();
    conf->set_database_scan_path(scan_root.string());

    FSScanner scanner(conf, nullptr, nullptr);
    auto [ok, msg] = scanner.scan(); // NOLINT
    if (!ok)
    {
        ADD_FAILURE() << "scan() failed: " << msg;
    }
    return scanner;
}

// Creates the small symtable used by most index tests below. Two symbols share
// trigrams ("foo" and "food"), one has multiple references, one has none.
HashMap MakeSymtable()
{
    HashMap table;

    SymbolMetaData md_a{true, SymbolSourceSection::SYMTAB, SymbolType::FUNC, SymbolBind::GLOBAL, SymbolVisibility::DEFAULT, 0x1234};
    SymbolMetaData md_b{false, SymbolSourceSection::DYNSYM, SymbolType::DATA_OBJ, SymbolBind::WEAK, SymbolVisibility::HIDDEN, 0x9};
    SymbolMetaData md_c{true, SymbolSourceSection::SYMTAB, SymbolType::FILE_SYM, SymbolBind::LOCAL, SymbolVisibility::PROTECTED, 0x0};

    table["foo"] = {SymbolRef{1, md_a}, SymbolRef{2, md_b}};
    table["food"] = {SymbolRef{3, md_c}};
    table["bar"] = {};
    table["qux"] = {SymbolRef{0, md_a}};

    return table;
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
// 1. build_file_table() tests
// =============================================================================
//
// build_file_table() serializes the FSScanner's found files (only .so/.a/.o
// are tracked) into two compressed blocks: the FileTable array (path_id +
// name_offset + name_length) and the concatenated file-name blob.

TEST(DatabaseBuilderUtilsFileTableBuildTest, EmptyScanProducesEmptyFileTable)
{
    const TempDirGuard guard;
    FSScanner scanner = MakeScannedScanner(guard.path);

    std::string err;
    auto result = build_file_table(scanner, no_dict, &err);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->files_count, 0u);
    EXPECT_EQ(result->file_index_uncompressed_length, 0u);
    EXPECT_EQ(result->file_names_uncompressed_length, 0u);
    EXPECT_FALSE(result->file_index_compressed.empty());
    EXPECT_FALSE(result->file_names_compressed.empty());
}

TEST(DatabaseBuilderUtilsFileTableBuildTest, TracksOnlyKnownExecutableExtensions)
{
    const TempDirGuard guard;
    std::ofstream(guard.path / "libalpha.so").put('x');
    std::ofstream(guard.path / "libbeta.so").put('x');
    std::ofstream(guard.path / "gamma.o").put('x');
    std::ofstream(guard.path / "delta.a").put('x');
    std::ofstream(guard.path / "readme.txt").put('x');
    std::ofstream(guard.path / "notes.md").put('x');

    FSScanner scanner = MakeScannedScanner(guard.path);

    std::string err;
    auto result = build_file_table(scanner, no_dict, &err);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->files_count, 4u);

    const auto &found = scanner.get_found_files();
    ASSERT_EQ(found.size(), 4u);

    std::vector<std::string> found_names;
    for (const auto &file : found)
    {
        found_names.push_back(file.name);
    }
    std::vector<std::string> expected_names{"libalpha.so", "libbeta.so", "gamma.o", "delta.a"};
    std::ranges::sort(found_names);
    std::ranges::sort(expected_names);
    EXPECT_EQ(found_names, expected_names);

    const std::string index = Decompress(result->file_index_compressed, result->file_index_uncompressed_length);
    const std::string names = Decompress(result->file_names_compressed, result->file_names_uncompressed_length);

    ASSERT_EQ(index.size(), result->files_count * sizeof(FileTable));
    ASSERT_EQ(names.size(), result->file_names_uncompressed_length);

    std::vector<std::string> roundtrip_names;
    std::vector<FileWrapper::Offset_t> seen_path_ids;
    for (std::uint32_t i = 0; i < result->files_count; ++i)
    {
        const FileTable entry = StructAt<FileTable>(index, i);
        EXPECT_LE(entry.name_offset + entry.name_length, names.size());
        roundtrip_names.push_back(SliceAt(names, entry.name_offset, entry.name_length));
        EXPECT_TRUE(entry.path_id < 1000u);
        seen_path_ids.push_back(entry.path_id);

        // Every entry must decode to the matching scanner file's path_id.
        ASSERT_EQ(found[i].name, roundtrip_names.back());
        EXPECT_EQ(found[i].path_id, entry.path_id);
    }

    std::ranges::sort(roundtrip_names);
    EXPECT_EQ(roundtrip_names, expected_names);

    // All files live in the same directory, so every path_id is identical.
    EXPECT_EQ(seen_path_ids.size(), 4u);
    EXPECT_EQ(seen_path_ids, std::vector<FileWrapper::Offset_t>(4, seen_path_ids.front()));
}

TEST(DatabaseBuilderUtilsFileTableBuildTest, RecursesIntoSubdirectories)
{
    const TempDirGuard guard;
    std::ofstream(guard.path / "top.so").put('x');
    std::filesystem::create_directory(guard.path / "nested");
    std::ofstream(guard.path / "nested" / "deep.o").put('x');

    // A non-tracked subdirectory entry must not invalidate the whole scan.
    std::filesystem::create_directory(guard.path / "empty_dir");

    FSScanner scanner = MakeScannedScanner(guard.path);

    std::string err;
    auto result = build_file_table(scanner, no_dict, &err);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->files_count, 2u);

    // "top.so" (root dir) and "deep.o" (nested dir) -> two distinct path_ids.
    std::set<FileWrapper::Offset_t> distinct_path_ids;
    for (const auto &file : scanner.get_found_files())
    {
        distinct_path_ids.insert(file.path_id);
    }
    EXPECT_EQ(distinct_path_ids.size(), 2u);
}

// =============================================================================
// 2. build_path_table() tests
// =============================================================================

TEST(DatabaseBuilderUtilsPathTableBuildTest, EmptyListProducesEmptyPathTable)
{
    const FSScanner::StringCache::StringList paths;

    std::string err;
    auto result = build_path_table(paths, no_dict, &err);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->paths_count, 0u);
    EXPECT_EQ(result->path_index_uncompressed_length, 0u);
    EXPECT_EQ(result->path_names_uncompressed_length, 0u);
}

TEST(DatabaseBuilderUtilsPathTableBuildTest, SinglePathRoundTrips)
{
    const FSScanner::StringCache::StringList paths{"/usr/lib"};

    std::string err;
    auto result = build_path_table(paths, no_dict, &err);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->paths_count, 1u);

    const std::string index = Decompress(result->path_index_compressed, result->path_index_uncompressed_length);
    const std::string names = Decompress(result->path_names_compressed, result->path_names_uncompressed_length);

    ASSERT_EQ(index.size(), sizeof(PathIndex));
    const PathIndex entry = StructAt<PathIndex>(index, 0);
    EXPECT_EQ(entry.name_offset, 0u);
    EXPECT_EQ(entry.name_length, std::string("/usr/lib").size());
    EXPECT_EQ(SliceAt(names, entry.name_offset, entry.name_length), "/usr/lib");
}

TEST(DatabaseBuilderUtilsPathTableBuildTest, MultiplePathsIncludingDuplicatesRoundTrip)
{
    const FSScanner::StringCache::StringList paths{"/usr/lib", "/usr", "/usr/lib", "x"};

    std::string err;
    auto result = build_path_table(paths, no_dict, &err);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->paths_count, 4u);

    const std::string index = Decompress(result->path_index_compressed, result->path_index_uncompressed_length);
    const std::string names = Decompress(result->path_names_compressed, result->path_names_uncompressed_length);

    ASSERT_EQ(index.size(), 4 * sizeof(PathIndex));
    ASSERT_EQ(names.size(), result->path_names_uncompressed_length);

    std::vector<std::string> decoded_names;
    for (std::uint32_t i = 0; i < result->paths_count; ++i)
    {
        const PathIndex entry = StructAt<PathIndex>(index, i);
        EXPECT_LE(entry.name_offset + entry.name_length, names.size());
        decoded_names.push_back(SliceAt(names, entry.name_offset, entry.name_length));
    }

    EXPECT_EQ(decoded_names, paths); // order and duplicates preserved
}

// =============================================================================
// 3. build_symbol_index() tests
// =============================================================================

TEST(DatabaseBuilderUtilsSymbolIndexBuildTest, EmptySymtableProducesEmptySymbolIndex)
{
    const HashMap symtable;

    std::string err;
    auto result = build_symbol_index(symtable, no_dict, &err);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->symbols_count, 0u);
    EXPECT_EQ(result->symbol_index_uncompressed_length, 0u);
    EXPECT_EQ(result->symbol_names_uncompressed_length, 0u);
    EXPECT_EQ(result->symbol_metadata_uncompressed_length, 0u);
}

TEST(DatabaseBuilderUtilsSymbolIndexBuildTest, SymbolsWithMultipleAndNoReferencesRoundTrip)
{
    const HashMap symtable = MakeSymtable();

    std::string err;
    auto result = build_symbol_index(symtable, no_dict, &err);

    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->symbols_count, 4u);

    const std::string index = Decompress(result->symbol_index_compressed, result->symbol_index_uncompressed_length);
    const std::string names = Decompress(result->symbol_names_compressed, result->symbol_names_uncompressed_length);
    const std::string metadata = Decompress(result->symbol_metadata_compressed, result->symbol_metadata_uncompressed_length);

    ASSERT_EQ(index.size(), result->symbols_count * sizeof(SymbolIndex));
    ASSERT_EQ(names.size(), result->symbol_names_uncompressed_length);
    ASSERT_EQ(metadata.size(), result->symbol_metadata_uncompressed_length);

    std::map<std::string, std::vector<SymbolRef>> decoded;
    for (std::uint32_t i = 0; i < result->symbols_count; ++i)
    {
        const SymbolIndex entry = StructAt<SymbolIndex>(index, i);
        ASSERT_LE(entry.metadata_offset + entry.metadata_length, metadata.size());
        ASSERT_EQ(entry.metadata_length % sizeof(SymbolRef), 0u);

        std::vector<SymbolRef> refs;
        const std::size_t ref_count = entry.metadata_length / sizeof(SymbolRef);
        for (std::size_t r = 0; r < ref_count; ++r)
        {
            const SymbolRef ref = StructAt<SymbolRef>(metadata, entry.metadata_offset / sizeof(SymbolRef) + r);
            refs.push_back(ref);
        }
        const std::string name = SliceAt(names, entry.name_offset, entry.name_length);
        decoded.emplace(name, refs);
    }

    ASSERT_EQ(decoded.size(), 4u);

    const auto &foo_refs = decoded.at("foo");
    ASSERT_EQ(foo_refs.size(), 2u);
    EXPECT_TRUE(RefEqual(foo_refs[0], SymbolRef{1, {true, SymbolSourceSection::SYMTAB, SymbolType::FUNC, SymbolBind::GLOBAL, SymbolVisibility::DEFAULT, 0x1234}}));
    EXPECT_TRUE(RefEqual(foo_refs[1], SymbolRef{2, {false, SymbolSourceSection::DYNSYM, SymbolType::DATA_OBJ, SymbolBind::WEAK, SymbolVisibility::HIDDEN, 0x9}}));

    const auto &food_refs = decoded.at("food");
    ASSERT_EQ(food_refs.size(), 1u);
    EXPECT_TRUE(RefEqual(food_refs[0], SymbolRef{3, {true, SymbolSourceSection::SYMTAB, SymbolType::FILE_SYM, SymbolBind::LOCAL, SymbolVisibility::PROTECTED, 0x0}}));

    EXPECT_TRUE(decoded.at("bar").empty()); // symbol with zero references
    ASSERT_EQ(decoded.at("qux").size(), 1u);
    EXPECT_TRUE(RefEqual(decoded.at("qux")[0],
                         SymbolRef{0, {true, SymbolSourceSection::SYMTAB, SymbolType::FUNC, SymbolBind::GLOBAL, SymbolVisibility::DEFAULT, 0x1234}}));
}

// =============================================================================
// 4. build_trigram_index() tests
// =============================================================================

TEST(DatabaseBuilderUtilsTrigramIndexBuildTest, EmptySymtableProducesAllEmptySlots)
{
    const HashMap symtable;

    std::string err;
    auto result = build_trigram_index(symtable, &err);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->trigram_index_capacity, 16u);
    EXPECT_EQ(result->trigram_postings_count, 0u);
    EXPECT_EQ(result->symbols_count, 0u);
    EXPECT_EQ(result->trigrams_count, 0u);

    const std::string slots = Decompress(result->trigram_index_compressed, result->trigram_index_uncompressed_length);
    ASSERT_EQ(slots.size(), sizeof(TrigramSlot) * 16);
    for (std::size_t i = 0; i < 16; ++i)
    {
        const TrigramSlot slot = StructAt<TrigramSlot>(slots, i);
        EXPECT_EQ(slot.trigram_code, TRIGRAM_EMPTY_SLOT);
        EXPECT_EQ(slot.posting_offset, 0u);
        EXPECT_EQ(slot.posting_count, 0u);
    }
}

TEST(DatabaseBuilderUtilsTrigramIndexBuildTest, SlotsAndPostingsMatchReferenceBuilder)
{
    const HashMap symtable = MakeSymtable();

    std::string err;
    auto result = build_trigram_index(symtable, &err);

    ASSERT_TRUE(result.has_value());

    // Independent reference: rebuild the trigram->symbols map with the same
    // iteration order and symbol ids as build_trigram_index does internally.
    TrigramBuilder ref_builder;
    std::uint32_t sym_idx{0};
    for (const auto &[sym_name, sym_refs] : symtable)
    {
        (void)sym_refs;
        ref_builder.add_word(sym_name, sym_idx++);
    }
    EXPECT_EQ(result->symbols_count, ref_builder.symbol_count());
    EXPECT_EQ(result->trigrams_count, ref_builder.unique_trigram_count());

    TrigramEntries expected_entries;
    for (const auto &[code, symbols] : ref_builder.get_trigram_to_symbols())
    {
        std::vector<std::uint32_t> syms = symbols;
        std::ranges::sort(syms);
        syms.erase(std::ranges::unique(syms).begin(), syms.end());
        expected_entries.push_back(TrigramEntry{.code = code, .symbols = std::move(syms)});
    }

    const std::uint64_t capacity = trigram_capacity_for_count(expected_entries.size());
    EXPECT_EQ(result->trigram_index_capacity, capacity);
    EXPECT_EQ(capacity % 2, 0u); // power of two
    EXPECT_TRUE(capacity > 0);

    ASSERT_EQ(
        result->trigram_index_uncompressed_length,
        capacity * sizeof(TrigramSlot));
    ASSERT_EQ(result->trigram_postings_uncompressed_length, result->trigram_postings_count * sizeof(std::uint32_t));

    const std::string slots = Decompress(result->trigram_index_compressed, result->trigram_index_uncompressed_length);
    const std::string postings = Decompress(result->trigram_postings_compressed, result->trigram_postings_uncompressed_length);
    ASSERT_EQ(slots.size(), capacity * sizeof(TrigramSlot));

    const auto capacity_bits = static_cast<std::uint32_t>(std::countr_zero(capacity));

    // Total non-empty slot count must equal the number of unique trigrams.
    std::size_t non_empty{0};
    for (std::uint64_t i = 0; i < capacity; ++i)
    {
        const TrigramSlot slot = StructAt<TrigramSlot>(slots, i);
        if (slot.trigram_code != TRIGRAM_EMPTY_SLOT)
        {
            ++non_empty;
        }
    }
    EXPECT_EQ(non_empty, expected_entries.size());

    // Each expected trigram must be findable via linear probing with the exact
    // symbol id set in postings.
    for (const auto &ent : expected_entries)
    {
        std::uint64_t idx = trigram_home_slot(ent.code, capacity_bits);
        while (true)
        {
            const TrigramSlot slot = StructAt<TrigramSlot>(slots, idx);
            ASSERT_NE(slot.trigram_code, TRIGRAM_EMPTY_SLOT) << "trigram not stored despite load factor guarantee";
            if (slot.trigram_code == ent.code)
            {
                ASSERT_EQ(slot.posting_count, ent.symbols.size());
                ASSERT_LE(slot.posting_offset + slot.posting_count, postings.size() / sizeof(std::uint32_t));
                for (std::uint32_t p = 0; p < slot.posting_count; ++p)
                {
                    std::uint32_t got = 0;
                    std::memcpy(&got, postings.data() + (slot.posting_offset + p) * sizeof(std::uint32_t), sizeof(std::uint32_t));
                    EXPECT_EQ(got, ent.symbols[p]);
                }
                break;
            }
            idx = (idx + 1) & (capacity - 1);
        }
    }
}

TEST(DatabaseBuilderUtilsTrigramIndexBuildTest, SharedTrigramsIncludeBothSymbolIds)
{
    // "foo" (id 0) and "food" (id 1) share the trigrams (\0,\0,f), (\0,f,o)
    // and (f,o,o); all three must be stored with a posting list {0, 1}.
    const HashMap symtable = MakeSymtable();

    std::string err;
    auto result = build_trigram_index(symtable, &err);
    ASSERT_TRUE(result.has_value());

    const std::string slots = Decompress(result->trigram_index_compressed, result->trigram_index_uncompressed_length);
    const std::string postings = Decompress(result->trigram_postings_compressed, result->trigram_postings_uncompressed_length);

    const auto capacity_bits = static_cast<std::uint32_t>(std::countr_zero(result->trigram_index_capacity));

    const TrigramCode shared_codes[3] = {encode_trigram(0, 0, 'f'), encode_trigram(0, 'f', 'o'), encode_trigram('f', 'o', 'o')};
    for (const TrigramCode code : shared_codes)
    {
        std::uint64_t idx = trigram_home_slot(code, capacity_bits);
        bool found = false;
        while (true)
        {
            const TrigramSlot slot = StructAt<TrigramSlot>(slots, idx);
            ASSERT_NE(slot.trigram_code, TRIGRAM_EMPTY_SLOT);
            if (slot.trigram_code == code)
            {
                ASSERT_EQ(slot.posting_count, 2u);
                std::vector<std::uint32_t> ids;
                for (std::uint32_t p = 0; p < slot.posting_count; ++p)
                {
                    std::uint32_t got = 0;
                    std::memcpy(&got, postings.data() + (slot.posting_offset + p) * sizeof(std::uint32_t), sizeof(std::uint32_t));
                    ids.push_back(got);
                }
                std::ranges::sort(ids);
                EXPECT_EQ(ids, (std::vector<std::uint32_t>{0, 1}));
                found = true;
                break;
            }
            idx = (idx + 1) & (result->trigram_index_capacity - 1);
        }
        EXPECT_TRUE(found);
    }
}

// =============================================================================
// 5. commit_* tests
// =============================================================================

TEST(DatabaseBuilderUtilsCommitTableTest, ContiguousCommitProducesBytesMatchingBuildResults)
{
    const TempDirGuard guard;
    std::ofstream(guard.path / "libalpha.so").put('x');
    std::ofstream(guard.path / "libbeta.so").put('x');

    FSScanner scanner = MakeScannedScanner(guard.path);

    const FSScanner::StringCache::StringList paths{"/usr/lib", "/usr"};
    const HashMap symtable = MakeSymtable();

    std::string build_err;
    const auto file_table = build_file_table(scanner, no_dict, &build_err);
    const auto path_table = build_path_table(paths, no_dict, &build_err);
    const auto sym_index = build_symbol_index(symtable, no_dict, &build_err);
    const auto trigram_index = build_trigram_index(symtable, &build_err);
    ASSERT_TRUE(file_table && path_table && sym_index && trigram_index);

    DatabaseWriter db((guard.path / "db.bin").string());

    FileWrapper::Offset_t offset{0};
    std::string err;

    const auto ft_hdr = commit_file_table(db, offset, *file_table, &err);
    ASSERT_TRUE(ft_hdr.has_value());
    const auto ft_offset = offset;
    EXPECT_EQ(ft_offset, file_table->file_index_compressed.size() + file_table->file_names_compressed.size());
    EXPECT_EQ(ft_hdr->file_index_compressed_offset, 0u);
    EXPECT_EQ(ft_hdr->file_index_compressed_length, file_table->file_index_compressed.size());
    EXPECT_EQ(ft_hdr->file_index_uncompressed_length, file_table->file_index_uncompressed_length);
    EXPECT_EQ(ft_hdr->file_names_compressed_offset, ft_hdr->file_index_compressed_length);
    EXPECT_EQ(ft_hdr->file_names_compressed_length, file_table->file_names_compressed.size());
    EXPECT_EQ(ft_hdr->files_count, file_table->files_count);
    EXPECT_EQ(ft_hdr->total_compressed_length, ft_offset);

    const auto pt_hdr = commit_path_table(db, offset, *path_table, &err);
    ASSERT_TRUE(pt_hdr.has_value());
    const auto pt_offset = offset;
    EXPECT_EQ(pt_offset, ft_offset + path_table->path_index_compressed.size() + path_table->path_names_compressed.size());
    EXPECT_EQ(pt_hdr->path_index_compressed_offset, ft_offset);
    EXPECT_EQ(pt_hdr->path_names_compressed_offset, ft_offset + path_table->path_index_compressed.size());
    EXPECT_EQ(pt_hdr->paths_count, path_table->paths_count);

    const auto si_hdr = commit_symbol_index(db, offset, *sym_index, &err);
    ASSERT_TRUE(si_hdr.has_value());
    const auto si_offset = offset;
    const auto si_expected_total =
        sym_index->symbol_index_compressed.size() + sym_index->symbol_names_compressed.size() +
        sym_index->symbol_metadata_compressed.size();
    EXPECT_EQ(si_offset, pt_offset + si_expected_total);
    EXPECT_EQ(si_hdr->symbol_index_compressed_offset, pt_offset);
    EXPECT_EQ(si_hdr->symbol_names_compressed_offset, pt_offset + sym_index->symbol_index_compressed.size());
    EXPECT_EQ(si_hdr->symbol_metadata_compressed_offset,
              pt_offset + sym_index->symbol_index_compressed.size() + sym_index->symbol_names_compressed.size());
    EXPECT_EQ(si_hdr->symbols_count, sym_index->symbols_count);

    const auto tg_hdr = commit_trigram_index(db, offset, *trigram_index, &err);
    ASSERT_TRUE(tg_hdr.has_value());
    const auto tg_offset = offset;
    const auto tg_expected_total =
        trigram_index->trigram_index_compressed.size() + trigram_index->trigram_postings_compressed.size();
    EXPECT_EQ(tg_offset, si_offset + tg_expected_total);
    EXPECT_EQ(tg_hdr->trigram_index_compressed_offset, si_offset);
    EXPECT_EQ(tg_hdr->trigram_postings_compressed_offset, si_offset + trigram_index->trigram_index_compressed.size());
    EXPECT_EQ(tg_hdr->trigram_index_capacity, trigram_index->trigram_index_capacity);
    EXPECT_EQ(tg_hdr->trigram_postings_count, trigram_index->trigram_postings_count);
    EXPECT_EQ(tg_hdr->symbols_count, trigram_index->symbols_count);
    EXPECT_EQ(tg_hdr->trigrams_count, trigram_index->trigrams_count);

    // The on-disk bytes for every block must match the built compressed blobs.
    const std::string disk = ReadFileAsString(guard.path / "db.bin");
    ASSERT_EQ(disk.size(), tg_offset);

    auto check_block = [&disk](FileWrapper::Offset_t offset, const std::string &blob) {
        ASSERT_LE(static_cast<std::size_t>(offset) + blob.size(), disk.size());
        EXPECT_EQ(SliceAt(disk, offset, blob.size()), blob);
    };

    check_block(ft_hdr->file_index_compressed_offset, file_table->file_index_compressed);
    check_block(ft_hdr->file_names_compressed_offset, file_table->file_names_compressed);
    check_block(pt_hdr->path_index_compressed_offset, path_table->path_index_compressed);
    check_block(pt_hdr->path_names_compressed_offset, path_table->path_names_compressed);
    check_block(si_hdr->symbol_index_compressed_offset, sym_index->symbol_index_compressed);
    check_block(si_hdr->symbol_names_compressed_offset, sym_index->symbol_names_compressed);
    check_block(si_hdr->symbol_metadata_compressed_offset, sym_index->symbol_metadata_compressed);
    check_block(tg_hdr->trigram_index_compressed_offset, trigram_index->trigram_index_compressed);
    check_block(tg_hdr->trigram_postings_compressed_offset, trigram_index->trigram_postings_compressed);
}

TEST(DatabaseBuilderUtilsCommitTableTest, AllCommitFunctionsFailOnUnopenableDatabase)
{
    DatabaseWriter db("/nonexistent_dir_that_does_not_exist_symfind/x.db");
    if (static_cast<bool>(db))
    {
        GTEST_SKIP() << "unexpectedly opened a database in a nonexistent directory";
    }

    const FileTableBuildResult empty_file_table;
    const PathTableBuildResult empty_path_table;
    const SymbolIndexBuildResult empty_symbol_index;
    const TrigramIndexBuildResult empty_trigram_index;

    FileWrapper::Offset_t offset{0};
    std::string err;

    EXPECT_FALSE(commit_file_table(db, offset, empty_file_table, &err).has_value());
    EXPECT_FALSE(err.empty());
    EXPECT_FALSE(commit_path_table(db, offset, empty_path_table, &err).has_value());
    EXPECT_FALSE(commit_symbol_index(db, offset, empty_symbol_index, &err).has_value());
    EXPECT_FALSE(commit_trigram_index(db, offset, empty_trigram_index, &err).has_value());
    EXPECT_EQ(offset, 0u); // no write succeeded, so nothing was consumed
}

} // namespace SymFind
