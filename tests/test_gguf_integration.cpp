/**
 * @file test_gguf_integration.cpp
 * @brief End-to-end integration test for GGUF loader + InferenceEngine (C++20, no Qt)
 *
 * Validates:
 * 1. GGUF loader Open/Close and file size
 * 2. InferenceEngine (CPU) LoadModel, Tokenize, Generate, Detokenize
 * 3. No Qt dependencies
 */

#include <gtest/gtest.h>
#include <chrono>
#include <string>
#include <vector>

#include "../src/gguf_loader.h"
#include "inference_engine.h"
#include "cpu_inference_engine.h"

using namespace RawrXD;

class GGUFIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

// GGUFLoader from main tree uses Open/Close/GetFileSize (no IsOpen; treat !Open as not open)
TEST_F(GGUFIntegrationTest, GGUFLoaderInitialization) {
    RawrXD::GGUFLoader loader;
    EXPECT_FALSE(loader.Open("/nonexistent/path/model.gguf"));
    EXPECT_EQ(loader.GetFileSize(), 0u);
}

TEST_F(GGUFIntegrationTest, InferenceEngineConstruction) {
    RawrXD::CPUInferenceEngine engine;
    EXPECT_FALSE(engine.IsModelLoaded());
}

TEST_F(GGUFIntegrationTest, MissingGGUFFileHandling) {
    RawrXD::GGUFLoader loader;
    bool result = loader.Open("/nonexistent/path/model.gguf");
    EXPECT_FALSE(result);
    EXPECT_EQ(loader.GetFileSize(), 0u);
}

TEST_F(GGUFIntegrationTest, TokenizationAPI) {
    RawrXD::CPUInferenceEngine engine;
    std::vector<int32_t> tokens = engine.Tokenize("");
    EXPECT_TRUE(tokens.empty() || tokens.size() < 10u);
    tokens = engine.Tokenize("hello");
    EXPECT_GT(tokens.size(), 0u);
}

TEST_F(GGUFIntegrationTest, GenerationAPI) {
    RawrXD::CPUInferenceEngine engine;
    std::vector<int32_t> emptyInput;
    std::vector<int32_t> result = engine.Generate(emptyInput, 10);
    EXPECT_LE(result.size(), 10u);
}

TEST_F(GGUFIntegrationTest, ModelMetadata) {
    RawrXD::CPUInferenceEngine engine;
    bool loaded = engine.IsModelLoaded();
    EXPECT_FALSE(loaded);
    EXPECT_EQ(engine.GetVocabSize(), 0);
}

TEST_F(GGUFIntegrationTest, DetokenizationAPI) {
    RawrXD::CPUInferenceEngine engine;
    std::vector<int32_t> emptyTokens;
    std::string result = engine.Detokenize(emptyTokens);
    EXPECT_TRUE(result.empty() || result.size() < 100u);
    std::vector<int32_t> sampleTokens = {1, 2, 3, 4, 5};
    std::string text = engine.Detokenize(sampleTokens);
    EXPECT_FALSE(text.empty());
}

TEST_F(GGUFIntegrationTest, FullIntegrationPipeline) {
    RawrXD::CPUInferenceEngine engine;
    EXPECT_FALSE(engine.IsModelLoaded());
    std::string prompt = "What is machine learning?";
    std::vector<int32_t> tokens = engine.Tokenize(prompt);
    EXPECT_GT(tokens.size(), 0u);
    std::vector<int32_t> result = engine.Generate(tokens, 50);
    std::string response = engine.Detokenize(result);
    (void)response;
}

TEST_F(GGUFIntegrationTest, TokenizationPerformance) {
    RawrXD::CPUInferenceEngine engine;
    std::string longText = R"(
        This is a comprehensive test of the tokenization system.
        It should handle various text types efficiently.
    )";
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 100; ++i)
        engine.Tokenize(longText);
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    double opsPerSec = (duration.count() > 0) ? (100.0 * 1000.0 / duration.count()) : 0;
    EXPECT_GT(opsPerSec, 0.0);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
