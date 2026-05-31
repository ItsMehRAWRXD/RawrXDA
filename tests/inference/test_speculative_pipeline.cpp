// ============================================================================
// test_speculative_pipeline.cpp — Unit tests for SpeculativePipeline
// ============================================================================
// Uses: GoogleTest (already configured in tests/CMakeLists.txt)
// Build: cmake --build . --target test_speculative_pipeline
// Run: ctest -R test_speculative_pipeline -V
// ============================================================================

#include <gtest/gtest.h>
#include "inference/speculative_pipeline.h"
#include <cmath>

using namespace RawrXD::Inference;

class SpeculativePipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Small config for fast unit tests
        config.maxDraftTokens = 8;
        config.minDraftTokens = 1;
        config.temperature = 0.8f;
        config.topP = 0.95f;
        config.topK = 40;
        config.adaptiveDraftEnabled = true;
        config.acceptThreshold = 0.9f;

        pipeline = std::make_unique<SpeculativePipeline>(config);
    }

    SpeculativeConfig config;
    std::unique_ptr<SpeculativePipeline> pipeline;
};

// ---------------------------------------------------------------------------
// Configuration validation
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, Config_ValidDefaults) {
    EXPECT_EQ(config.maxDraftTokens, 8u);
    EXPECT_EQ(config.minDraftTokens, 1u);
    EXPECT_FLOAT_EQ(config.temperature, 0.8f);
    EXPECT_FLOAT_EQ(config.topP, 0.95f);
    EXPECT_EQ(config.topK, 40);
    EXPECT_TRUE(config.adaptiveDraftEnabled);
}

// ---------------------------------------------------------------------------
// Draft stage (mocked — tests plumbing)
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, DraftStage_ReturnsTokens) {
    std::vector<float> logits(32000, 0.0f);
    logits[42] = 5.0f;  // Make token 42 the clear winner

    auto draftTokens = pipeline->draftStage(logits);
    EXPECT_FALSE(draftTokens.empty());
    EXPECT_LE(draftTokens.size(), config.maxDraftTokens);
}

TEST_F(SpeculativePipelineTest, DraftStage_RespectsMaxDraft) {
    std::vector<float> logits(32000, 1.0f);
    config.maxDraftTokens = 3;
    pipeline = std::make_unique<SpeculativePipeline>(config);

    auto draftTokens = pipeline->draftStage(logits);
    EXPECT_LE(draftTokens.size(), 3u);
}

// ---------------------------------------------------------------------------
// Verify stage
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, VerifyStage_AcceptsMatchingTokens) {
    std::vector<uint32_t> draft = {42, 43, 44};
    std::vector<std::vector<float>> targetLogits;
    for (size_t i = 0; i < draft.size(); ++i) {
        std::vector<float> logits(32000, -100.0f);
        logits[draft[i]] = 10.0f;  // Target agrees with draft
        targetLogits.push_back(logits);
    }

    auto result = pipeline->verifyStage(draft, targetLogits);
    EXPECT_EQ(result.acceptedCount, draft.size());
    EXPECT_TRUE(result.allAccepted);
    EXPECT_EQ(result.acceptedTokens, draft);
}

TEST_F(SpeculativePipelineTest, VerifyStage_RejectsMismatch) {
    std::vector<uint32_t> draft = {42, 43, 44};
    std::vector<std::vector<float>> targetLogits;

    // First token matches
    std::vector<float> logits0(32000, -100.0f);
    logits0[42] = 10.0f;
    targetLogits.push_back(logits0);

    // Second token mismatches — target prefers 99
    std::vector<float> logits1(32000, -100.0f);
    logits1[99] = 10.0f;
    targetLogits.push_back(logits1);

    auto result = pipeline->verifyStage(draft, targetLogits);
    EXPECT_EQ(result.acceptedCount, 1u);
    EXPECT_FALSE(result.allAccepted);
    EXPECT_EQ(result.acceptedTokens.size(), 1u);
    EXPECT_EQ(result.acceptedTokens[0], 42u);
}

// ---------------------------------------------------------------------------
// Adaptive draft size
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, AdaptiveDraft_IncreasesOnSuccess) {
    std::vector<uint32_t> draft = {1, 2, 3, 4, 5};
    std::vector<std::vector<float>> targetLogits;
    for (auto t : draft) {
        std::vector<float> logits(32000, -100.0f);
        logits[t] = 10.0f;
        targetLogits.push_back(logits);
    }

    auto result = pipeline->verifyStage(draft, targetLogits);
    EXPECT_TRUE(result.allAccepted);

    uint32_t newDraftSize = pipeline->adaptDraftSize(result);
    EXPECT_GE(newDraftSize, draft.size());
}

TEST_F(SpeculativePipelineTest, AdaptiveDraft_DecreasesOnFailure) {
    std::vector<uint32_t> draft = {1, 2, 3, 4, 5};
    std::vector<std::vector<float>> targetLogits;

    // Only first token matches
    std::vector<float> logits0(32000, -100.0f);
    logits0[1] = 10.0f;
    targetLogits.push_back(logits0);

    std::vector<float> logits1(32000, -100.0f);
    logits1[99] = 10.0f;
    targetLogits.push_back(logits1);

    auto result = pipeline->verifyStage(draft, targetLogits);
    EXPECT_FALSE(result.allAccepted);

    uint32_t newDraftSize = pipeline->adaptDraftSize(result);
    EXPECT_LT(newDraftSize, draft.size());
}

// ---------------------------------------------------------------------------
// Softmax sampling
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, Softmax_SumsToOne) {
    std::vector<float> logits = {1.0f, 2.0f, 3.0f, 4.0f};
    auto probs = pipeline->softmax(logits);

    float sum = 0.0f;
    for (auto p : probs) sum += p;
    EXPECT_NEAR(sum, 1.0f, 1e-5f);
}

TEST_F(SpeculativePipelineTest, Softmax_HigherLogitHigherProb) {
    std::vector<float> logits = {0.0f, 10.0f};
    auto probs = pipeline->softmax(logits);
    EXPECT_GT(probs[1], probs[0]);
}

// ---------------------------------------------------------------------------
// Top-k / Top-p filtering
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, TopKFilter_KeepsTopK) {
    std::vector<float> probs = {0.1f, 0.5f, 0.2f, 0.15f, 0.05f};
    auto filtered = pipeline->applyTopK(probs, 2);

    int nonZero = 0;
    for (auto p : filtered) if (p > 0.0f) ++nonZero;
    EXPECT_EQ(nonZero, 2);
    EXPECT_GT(filtered[1], 0.0f);  // 0.5 should survive
    EXPECT_GT(filtered[2], 0.0f);  // 0.2 should survive
}

TEST_F(SpeculativePipelineTest, TopPFilter_KeepsCumulativeMass) {
    std::vector<float> probs = {0.5f, 0.3f, 0.15f, 0.05f};
    auto filtered = pipeline->applyTopP(probs, 0.8f);

    float sum = 0.0f;
    int nonZero = 0;
    for (auto p : filtered) {
        if (p > 0.0f) {
            sum += p;
            ++nonZero;
        }
    }
    EXPECT_LE(sum, 0.81f);  // 0.8 + epsilon
    EXPECT_GE(nonZero, 2);   // Should keep at least top 2
}

// ---------------------------------------------------------------------------
// End-to-end pipeline
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, Pipeline_EndToEnd_AcceptsAll) {
    // Simulate perfect draft
    std::vector<float> initialLogits(32000, -100.0f);
    initialLogits[100] = 10.0f;

    auto draft = pipeline->draftStage(initialLogits);
    ASSERT_FALSE(draft.empty());

    std::vector<std::vector<float>> targetLogits;
    for (auto t : draft) {
        std::vector<float> logits(32000, -100.0f);
        logits[t] = 10.0f;
        targetLogits.push_back(logits);
    }

    auto result = pipeline->verifyStage(draft, targetLogits);
    EXPECT_EQ(result.acceptedCount, draft.size());
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, Stats_Accumulate) {
    auto stats = pipeline->getStats();
    EXPECT_EQ(stats.totalDraftTokens, 0u);
    EXPECT_EQ(stats.totalAcceptedTokens, 0u);

    // Run a draft+verify cycle
    std::vector<float> logits(32000, 0.0f);
    auto draft = pipeline->draftStage(logits);
    std::vector<std::vector<float>> targetLogits(draft.size(), logits);
    pipeline->verifyStage(draft, targetLogits);

    stats = pipeline->getStats();
    EXPECT_EQ(stats.totalDraftTokens, draft.size());
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
