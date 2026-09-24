#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

#include <symfind/core/DictionaryBuilder.h>

namespace SymFind
{

namespace
{

// Helper: generate a plausible sample corpus that is large/diverse enough for
// ZDICT_trainFromBuffer to succeed. Each sample is a snippet resembling
// symbol or source-like text; 100 such samples have proven sufficient with
// max_dictionary_size = 4096 and also with the default 128 KiB.
std::vector<std::string> MakeTrainingCorpus(std::size_t count = 100)
{
    std::vector<std::string> out;
    out.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
    {
        out.push_back("int main_function_" + std::to_string(i) +
                      "() { return 0; } // sample symbol name foo_bar_baz_" + std::to_string(i) +
                      " hello world example data for dictionary training");
    }
    return out;
}

} // namespace (anonymouse)

// =============================================================================
// 1. Construction, reserve and basic add_sample
// =============================================================================

TEST(DictionaryBuilderTest, DefaultConstructionDoesNotThrow)
{
    EXPECT_NO_THROW(DictionaryBuilder{});
    DictionaryBuilder builder;
    (void)builder;
}

TEST(DictionaryBuilderTest, ExplicitMaxDictionarySizeConstruction)
{
    EXPECT_NO_THROW(DictionaryBuilder(4096));
    EXPECT_NO_THROW(DictionaryBuilder(128 * 1024));
    EXPECT_NO_THROW(DictionaryBuilder(1024));
}

TEST(DictionaryBuilderTest, ReserveDoesNotThrow)
{
    DictionaryBuilder builder(4096);
    EXPECT_NO_THROW(builder.reserve(0));
    EXPECT_NO_THROW(builder.reserve(10));
    EXPECT_NO_THROW(builder.reserve(1000));
}

TEST(DictionaryBuilderTest, AddEmptySampleIsNoOpAndTrainStillFails)
{
    DictionaryBuilder builder(4096);
    builder.add_sample("");
    builder.add_sample(std::string_view{});
    std::string dict;
    std::string err;
    bool ok = builder.train(dict, &err);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(err.empty());
}

TEST(DictionaryBuilderTest, AddSampleEmptyStringViewDoesNotAffectSubsequentTrain)
{
    DictionaryBuilder builder(4096);
    // Adding only empty samples should behave identically to no samples.
    for (int i = 0; i < 10; ++i)
        builder.add_sample("");
    std::string dict;
    EXPECT_FALSE(builder.train(dict, nullptr));
}

// =============================================================================
// 2. train() failure paths
// =============================================================================

TEST(DictionaryBuilderTest, TrainWithNoSamplesFailsAndSetsError)
{
    DictionaryBuilder builder(4096);
    std::string dict;
    std::string err;
    bool ok = builder.train(dict, &err);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(err.empty()) << "err_msg should be populated on failure";
}

TEST(DictionaryBuilderTest, TrainWithNoSamplesAndNullErrMsgFailsGracefully)
{
    DictionaryBuilder builder(4096);
    std::string dict;
    EXPECT_FALSE(builder.train(dict, nullptr));
}

TEST(DictionaryBuilderTest, TrainWithSingleSmallSampleFails)
{
    DictionaryBuilder builder(4096);
    builder.add_sample("hello");
    std::string dict;
    std::string err;
    bool ok = builder.train(dict, &err);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(err.empty());
}

TEST(DictionaryBuilderTest, TrainWithInsufficientSamplesFails)
{
    DictionaryBuilder builder(4096);
    // One medium sample alone is still insufficient for ZDICT.
    builder.add_sample("hello world this is a sample for dictionary training");
    std::string dict;
    std::string err;
    EXPECT_FALSE(builder.train(dict, &err));
    EXPECT_FALSE(err.empty());
}

// =============================================================================
// 3. train() success path
// =============================================================================

TEST(DictionaryBuilderTest, TrainWithSufficientSamplesSucceeds)
{
    DictionaryBuilder builder(4096);
    auto corpus = MakeTrainingCorpus(100);
    for (const auto &s : corpus)
        builder.add_sample(s);

    std::string dict;
    std::string err;
    bool ok = builder.train(dict, &err);
    EXPECT_TRUE(ok) << "err=" << err;
    EXPECT_TRUE(err.empty()) << "err should be empty on success, got: " << err;
    EXPECT_FALSE(dict.empty());
    EXPECT_LE(dict.size(), 4096u);
    EXPECT_GT(dict.size(), 0u);
}

TEST(DictionaryBuilderTest, TrainWithDefaultMaxDictionarySizeSucceeds)
{
    DictionaryBuilder builder; // default 128 KiB
    auto corpus = MakeTrainingCorpus(150);
    for (const auto &s : corpus)
        builder.add_sample(s);

    std::string dict;
    std::string err;
    bool ok = builder.train(dict, &err);
    EXPECT_TRUE(ok) << "err=" << err;
    EXPECT_FALSE(dict.empty());
    EXPECT_LE(dict.size(), static_cast<std::size_t>(128 * 1024));
}

TEST(DictionaryBuilderTest, TrainWithNullErrMsgSucceedsWhenCorpusIsLargeEnough)
{
    DictionaryBuilder builder(4096);
    auto corpus = MakeTrainingCorpus(100);
    for (auto &s : corpus)
        builder.add_sample(s);

    std::string dict;
    EXPECT_TRUE(builder.train(dict, nullptr));
    EXPECT_FALSE(dict.empty());
}

TEST(DictionaryBuilderTest, TrainProducesDictionaryWithinMaxSize)
{
    for (std::size_t max_size : {4096, 8192, 16384})
    {
        DictionaryBuilder builder(max_size);
        for (auto &s : MakeTrainingCorpus(120))
            builder.add_sample(s);

        std::string dict;
        std::string err;
        ASSERT_TRUE(builder.train(dict, &err)) << "max_size=" << max_size << " err=" << err;
        EXPECT_LE(dict.size(), max_size) << "max_size=" << max_size;
        EXPECT_GT(dict.size(), 0u);
    }
}

TEST(DictionaryBuilderTest, TrainAfterReserveSucceeds)
{
    DictionaryBuilder builder(4096);
    builder.reserve(100);
    for (auto &s : MakeTrainingCorpus(100))
        builder.add_sample(s);

    std::string dict;
    std::string err;
    EXPECT_TRUE(builder.train(dict, &err)) << "err=" << err;
    EXPECT_FALSE(dict.empty());
}

// =============================================================================
// 4. clear() behavior
// =============================================================================

TEST(DictionaryBuilderTest, ClearResetsStateSoTrainFailsAgain)
{
    DictionaryBuilder builder(4096);
    for (auto &s : MakeTrainingCorpus(100))
        builder.add_sample(s);

    std::string dict;
    std::string err;
    ASSERT_TRUE(builder.train(dict, &err));

    builder.clear();

    std::string dict2;
    std::string err2;
    EXPECT_FALSE(builder.train(dict2, &err2));
    EXPECT_FALSE(err2.empty());
}

TEST(DictionaryBuilderTest, CanAddSamplesAgainAfterClear)
{
    DictionaryBuilder builder(4096);
    for (auto &s : MakeTrainingCorpus(100))
        builder.add_sample(s);

    std::string dict;
    ASSERT_TRUE(builder.train(dict, nullptr));

    builder.clear();
    // Re-populate after clear and train again should succeed.
    for (auto &s : MakeTrainingCorpus(100))
        builder.add_sample(s);

    std::string dict2;
    std::string err2;
    EXPECT_TRUE(builder.train(dict2, &err2)) << "err=" << err2;
    EXPECT_FALSE(dict2.empty());
}

TEST(DictionaryBuilderTest, ClearOnEmptyBuilderIsSafe)
{
    DictionaryBuilder builder(4096);
    EXPECT_NO_THROW(builder.clear());
    std::string dict;
    EXPECT_FALSE(builder.train(dict, nullptr));
}

TEST(DictionaryBuilderTest, ClearDoesNotThrowAfterAddingSamples)
{
    DictionaryBuilder builder(4096);
    builder.add_sample("hello world sample");
    EXPECT_NO_THROW(builder.clear());
}

// =============================================================================
// 5. create_compression_dictionary / create_decompression_dictionary
// =============================================================================

TEST(DictionaryBuilderTest, CreateCompressionDictionaryFromValidDictReturnsNonNull)
{
    DictionaryBuilder builder(4096);
    for (auto &s : MakeTrainingCorpus(100))
        builder.add_sample(s);

    std::string dict;
    ASSERT_TRUE(builder.train(dict, nullptr));
    ASSERT_FALSE(dict.empty());

    auto cdict = DictionaryBuilder::create_compression_dictionary(dict.data(), dict.size(), 3);
    EXPECT_NE(cdict, nullptr);

    // Also test with different compression levels.
    auto cdict2 = DictionaryBuilder::create_compression_dictionary(dict.data(), dict.size(), 1);
    EXPECT_NE(cdict2, nullptr);

    auto cdict3 = DictionaryBuilder::create_compression_dictionary(dict.data(), dict.size(), 6);
    EXPECT_NE(cdict3, nullptr);
}

TEST(DictionaryBuilderTest, CreateDecompressionDictionaryFromValidDictReturnsNonNull)
{
    DictionaryBuilder builder(4096);
    for (auto &s : MakeTrainingCorpus(100))
        builder.add_sample(s);

    std::string dict;
    ASSERT_TRUE(builder.train(dict, nullptr));

    // Note: current implementation forwards to ZSTD_createCDict even for the
    // decompression variant; the test documents that it still returns a
    // non-null handle rather than asserting the ZSTD_DDict type.
    auto ddict = DictionaryBuilder::create_decompression_dictionary(dict.data(), dict.size(), 3);
    EXPECT_NE(ddict, nullptr);

    auto ddict2 = DictionaryBuilder::create_decompression_dictionary(dict.data(), dict.size(), 6);
    EXPECT_NE(ddict2, nullptr);
}

TEST(DictionaryBuilderTest, CreateCompressionDictionaryDefaultCompressionLevel)
{
    DictionaryBuilder builder(4096);
    for (auto &s : MakeTrainingCorpus(100))
        builder.add_sample(s);

    std::string dict;
    ASSERT_TRUE(builder.train(dict, nullptr));

    // Default argument compression_level == 6
    auto cdict = DictionaryBuilder::create_compression_dictionary(dict.data(), dict.size());
    EXPECT_NE(cdict, nullptr);
}

TEST(DictionaryBuilderTest, CreateDictionariesWithNullBufferStillReturnsHandle)
{
    // ZSTD_createCDict with nullptr/0 still returns a non-null handle in the
    // current zstd version (it creates an empty dictionary). Document this.
    auto cdict = DictionaryBuilder::create_compression_dictionary(nullptr, 0, 3);
    EXPECT_NE(cdict, nullptr);

    auto ddict = DictionaryBuilder::create_decompression_dictionary(nullptr, 0, 3);
    EXPECT_NE(ddict, nullptr);
}

// =============================================================================
// 6. End-to-end: train, clear, retrain with different sample sizes
// =============================================================================

TEST(DictionaryBuilderTest, EndToEndTrainClearRetrain)
{
    DictionaryBuilder builder(8192);

    // First training with 100 samples.
    for (auto &s : MakeTrainingCorpus(100))
        builder.add_sample(s);
    std::string dict1;
    ASSERT_TRUE(builder.train(dict1, nullptr));
    EXPECT_LE(dict1.size(), 8192u);
    EXPECT_GT(dict1.size(), 0u);

    builder.clear();

    // Second training with more samples should also succeed and may produce
    // a different dictionary size.
    for (auto &s : MakeTrainingCorpus(200))
        builder.add_sample(s);
    std::string dict2;
    ASSERT_TRUE(builder.train(dict2, nullptr));
    EXPECT_LE(dict2.size(), 8192u);
    EXPECT_GT(dict2.size(), 0u);
}

} // namespace SymFind
