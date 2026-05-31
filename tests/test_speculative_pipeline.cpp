// ============================================================================
// test_speculative_pipeline.cpp — Unit tests for SpeculativePipeline
// ============================================================================
// Uses RawrXD's existing GoogleTest v1.14.0 infrastructure
// ============================================================================

#include <gtest/gtest.h>
#include "inference/speculative_pipeline.h"
#include "inference/kv_cache_ownership.h"
#include <math>

using namespace RawrXD::Inference;

// ============================================================================
// Mock Draft Model
// ============================================================================
class MockDraftModel : public IDraftModel {
public:
    bool load(const std::string&) override { loaded_ = true; return true; }
    void unload() override { loaded_ = false; }
    bool isLoaded() const override { return loaded_; }

    std::vector<TokenProb> draftTokens(
        const std::vector<uint32_t>& context,
        uint32_t count,
        float temperature
    ) override {
        std::vector<TokenProb> result;
        for (uint32_t i = 0; i < count; ++i) {
            TokenProb tp;
            tp.tokenId = nextTokenId_++;
            tp.logProb = -0.5f * static_cast<float>(i);
            tp.temperature = temperature;
            result.push_back(tp);
        }
        return result;
    }

    void clearCache() override {}

private:
    bool loaded_ = false;
    uint32_t nextTokenId_ = 1000;
};

// ============================================================================
// Mock Target Model
// ============================================================================
class MockTargetModel : public ITargetModel {
public:
    bool load(const std::string&) override { loaded_ = true; return true; }
    void unload() override { loaded_ = false; }
    bool isLoaded() const override { return loaded_; }

    std::vector<TokenProb> verifyTokens(
        const std::vector<uint32_t>& context,
        const std::vector<uint32_t>& draftTokens,
        float temperature
    ) override {
        std::vector<TokenProb> result;
        for (size_t i = 0; i < draftTokens.size(); ++i) {
            TokenProb tp;
            tp.tokenId = draftTokens[i];
            // Accept first N tokens, reject rest
            if (i < acceptCount_) {
                tp.logProb = -0.1f; // high prob = accept
            } else {
                tp.logProb = -10.0f; // low prob = reject
            }
            tp.temperature = temperature;
            result.push_back(tp);
        }
        return result;
    }

    uint32_t sampleNextToken(
        const std::vector<uint32_t>& context,
        float temperature
    ) override {
        return fallbackToken_;
    }

    void clearCache() override {}

    void setAcceptCount(uint32_t n) { acceptCount_ = n; }
    void setFallbackToken(uint32_t t) { fallbackToken_ = t; }

private:
    bool loaded_ = false;
    uint32_t acceptCount_ = 2;
    uint32_t fallbackToken_ = 9999;
};

// ============================================================================
// Test Fixture
// ============================================================================
class SpeculativePipelineTest : public ::testing::Test {
protected:
    void SetUp() override {
        draftModel = std::make_shared<MockDraftModel>();
        targetModel = std::make_shared<MockTargetModel>();
        kvTracker = std::make_shared<KVCacheOwnershipTracker>(128);

        pipeline = std::make_unique<SpeculativePipeline>(
            draftModel, targetModel, kvTracker
        );

        draftModel->load("mock_draft.gguf");
        targetModel->load("mock_target.gguf");
    }

    std::shared_ptr<MockDraftModel> draftModel;
    std::shared_ptr<MockTargetModel> targetModel;
    std::shared_ptr<KVCacheOwnershipTracker> kvTracker;
    std::unique_ptr<SpeculativePipeline> pipeline;
};

// ---------------------------------------------------------------------------
// Configuration
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, DefaultConfig) {
    auto config = pipeline->getConfig();
    EXPECT_EQ(config.maxDraftTokens, 5u);
    EXPECT_EQ(config.minDraftTokens, 1u);
    EXPECT_FLOAT_EQ(config.temperature, 0.8f);
    EXPECT_FLOAT_EQ(config.topP, 0.9f);
    EXPECT_EQ(config.topK, 40u);
}

TEST_F(SpeculativePipelineTest, SetConfig) {
    SpeculativeConfig cfg;
    cfg.maxDraftTokens = 10;
    cfg.temperature = 0.5f;
    pipeline->setConfig(cfg);

    auto retrieved = pipeline->getConfig();
    EXPECT_EQ(retrieved.maxDraftTokens, 10u);
    EXPECT_FLOAT_EQ(retrieved.temperature, 0.5f);
}

// ---------------------------------------------------------------------------
// Generation
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, GenerateTokensBasic) {
    std::vector<uint32_t> context = {1, 2, 3};
    auto result = pipeline->generate(context, 10);

    EXPECT_FALSE(result.tokens.empty());
    EXPECT_EQ(result.tokensGenerated, result.tokens.size());
    EXPECT_TRUE(result.acceptanceRate >= 0.0f && result.acceptanceRate <= 1.0f);
}

TEST_F(SpeculativePipelineTest, GenerateRespectsMaxTokens) {
    std::vector<uint32_t> context = {1, 2, 3};
    uint32_t maxTokens = 5;
    auto result = pipeline->generate(context, maxTokens);

    EXPECT_LE(result.tokensGenerated, maxTokens);
}

TEST_F(SpeculativePipelineTest, GenerateWithEmptyContext) {
    std::vector<uint32_t> context;
    auto result = pipeline->generate(context, 5);
    EXPECT_FALSE(result.tokens.empty());
}

// ---------------------------------------------------------------------------
// Acceptance rate tracking
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, AcceptanceRateTracked) {
    std::vector<uint32_t> context = {1, 2, 3};

    // First generation
    auto r1 = pipeline->generate(context, 5);
    float rate1 = pipeline->getAcceptanceRate();

    // Second generation
    auto r2 = pipeline->generate(context, 5);
    float rate2 = pipeline->getAcceptanceRate();

    // Rate should be between 0 and 1
    EXPECT_GE(rate1, 0.0f);
    EXPECT_LE(rate1, 1.0f);
    EXPECT_GE(rate2, 0.0f);
    EXPECT_LE(rate2, 1.0f);
}

TEST_F(SpeculativePipelineTest, ResetStats) {
    std::vector<uint32_t> context = {1, 2, 3};
    pipeline->generate(context, 5);
    EXPECT_GT(pipeline->getTotalDrafted(), 0u);

    pipeline->resetStats();
    EXPECT_EQ(pipeline->getTotalDrafted(), 0u);
    EXPECT_EQ(pipeline->getTotalAccepted(), 0u);
    EXPECT_FLOAT_EQ(pipeline->getAcceptanceRate(), 0.0f);
}

// ---------------------------------------------------------------------------
// Draft size adaptation
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, AdaptDraftSize) {
    // Set initial config
    SpeculativeConfig cfg;
    cfg.maxDraftTokens = 8;
    cfg.minDraftTokens = 1;
    pipeline->setConfig(cfg);

    // Generate some tokens to build stats
    std::vector<uint32_t> context = {1, 2, 3};
    for (int i = 0; i < 10; ++i) {
        pipeline->generate(context, 5);
    }

    // Adapt should adjust draft size based on acceptance rate
    pipeline->adaptDraftSize();
    auto newConfig = pipeline->getConfig();

    // Draft size should stay within bounds
    EXPECT_GE(newConfig.maxDraftTokens, cfg.minDraftTokens);
    EXPECT_LE(newConfig.maxDraftTokens, 16u); // upper cap
}

// ---------------------------------------------------------------------------
// Token probability utilities
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, SoftmaxProbabilities) {
    std::vector<float> logits = {1.0f, 2.0f, 3.0f};
    auto probs = pipeline->computeSoftmax(logits);

    EXPECT_EQ(probs.size(), logits.size());

    // Sum should be ~1.0
    float sum = 0.0f;
    for (auto p : probs) sum += p;
    EXPECT_NEAR(sum, 1.0f, 0.001f);

    // Higher logit = higher probability
    EXPECT_GT(probs[2], probs[1]);
    EXPECT_GT(probs[1], probs[0]);
}

TEST_F(SpeculativePipelineTest, TopPFiltering) {
    std::vector<float> probs = {0.5f, 0.3f, 0.15f, 0.05f};
    auto filtered = pipeline->applyTopP(probs, 0.8f);

    // Should keep tokens until cumulative prob >= 0.8
    // 0.5 + 0.3 = 0.8, so first 2 should be kept
    EXPECT_GT(filtered[0], 0.0f);
    EXPECT_GT(filtered[1], 0.0f);
    // Remaining should be zeroed
    EXPECT_EQ(filtered[2], 0.0f);
    EXPECT_EQ(filtered[3], 0.0f);
}

TEST_F(SpeculativePipelineTest, TopKFiltering) {
    std::vector<float> probs = {0.5f, 0.3f, 0.15f, 0.05f};
    auto filtered = pipeline->applyTopK(probs, 2);

    // Top 2 should be kept
    EXPECT_GT(filtered[0], 0.0f);
    EXPECT_GT(filtered[1], 0.0f);
    EXPECT_EQ(filtered[2], 0.0f);
    EXPECT_EQ(filtered[3], 0.0f);
}

// ---------------------------------------------------------------------------
// Error handling
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, GenerateWithUnloadedModels) {
    draftModel->unload();

    std::vector<uint32_t> context = {1, 2, 3};
    auto result = pipeline->generate(context, 5);

    // Should still produce tokens (fallback to target model)
    EXPECT_FALSE(result.tokens.empty());
}

// ---------------------------------------------------------------------------
// Stats
// ---------------------------------------------------------------------------
TEST_F(SpeculativePipelineTest, StatsAccumulate) {
    std::vector<uint32_t> context = {1, 2, 3};

    uint32_t totalDraftedBefore = pipeline->getTotalDrafted();
    pipeline->generate(context, 5);
    uint32_t totalDraftedAfter = pipeline->getTotalDrafted();

    EXPECT_GT(totalDraftedAfter, totalDraftedBefore);
}

// ============================================================================
// Main
// ============================================================================
int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
