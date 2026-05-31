// ============================================================================
// speculative_pipeline.h — Real Draft+Verify Speculative Decoding Pipeline
// ============================================================================
// Architecture:
//   Draft Model (small, fast) → generates K speculative tokens
//   Target Model (large, accurate) → verifies all K tokens in parallel
//   Acceptance: tokens match target distribution → keep
//   Rejection: first mismatch → discard rest, resample from target
//
// KV cache management:
//   - Draft KV cache: separate, small cache for draft model
//   - Target KV cache: main cache, extended by accepted tokens
//   - On rejection: rollback target KV to last accepted position
//
// Thread safety: pipeline is single-threaded per sequence.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <atomic>
#include <random>

namespace RawrXD::Inference {

// Forward declarations
class KVCacheOwnershipTracker;

// ---------------------------------------------------------------------------
// Token probability bundle
// ---------------------------------------------------------------------------
struct TokenProb {
    uint32_t tokenId;
    float    logProb;       // log probability from the model
    float    temperature;     // temperature used for sampling
};

// ---------------------------------------------------------------------------
// Speculative token with verification status
// ---------------------------------------------------------------------------
struct SpeculativeToken {
    uint32_t tokenId;           // draft token ID
    float    draftLogProb;      // log prob from draft model
    float    targetLogProb;     // log prob from target model
    bool     accepted;          // verified by target model
    uint32_t position;          // position in sequence
};

// ---------------------------------------------------------------------------
// Speculative decoding result
// ---------------------------------------------------------------------------
struct SpeculativeResult {
    std::vector<uint32_t>    acceptedTokens;     // tokens that passed verification
    uint32_t                    numDrafted;          // total tokens drafted
    uint32_t                    numAccepted;         // tokens accepted
    float                       acceptanceRate;      // numAccepted / numDrafted
    bool                        finished;            // EOS reached
    std::string                 finishReason;        // "stop", "length", "reject"
};

// ---------------------------------------------------------------------------
// Draft model interface (abstract)
// ---------------------------------------------------------------------------
class IDraftModel {
public:
    virtual ~IDraftModel() = default;
    virtual bool load(const std::string& path) = 0;
    virtual void unload() = 0;
    virtual bool isLoaded() const = 0;
    virtual std::vector<TokenProb> draftTokens(
        const std::vector<uint32_t>& context,
        uint32_t count,
        float temperature
    ) = 0;
    virtual void clearCache() = 0;
};

// ---------------------------------------------------------------------------
// Target model interface (abstract)
// ---------------------------------------------------------------------------
class ITargetModel {
public:
    virtual ~ITargetModel() = default;
    virtual bool load(const std::string& path) = 0;
    virtual void unload() = 0;
    virtual bool isLoaded() const = 0;
    // Verify draft tokens: returns acceptance mask + corrected token at first rejection
    virtual std::vector<TokenProb> verifyTokens(
        const std::vector<uint32_t>& context,
        const std::vector<uint32_t>& draftTokens,
        float temperature
    ) = 0;
    virtual void clearCache() = 0;
};

// ---------------------------------------------------------------------------
// Speculative Pipeline Configuration
// ---------------------------------------------------------------------------
struct SpeculativePipelineConfig {
    uint32_t maxDraftTokens = 5;           // K: number of tokens to draft
    float    temperature = 0.7f;           // sampling temperature
    float    topP = 0.95f;                 // nucleus sampling
    uint32_t topK = 40;                    // top-k sampling
    float    minAcceptanceRate = 0.5f;     // below this, reduce K
    float    maxAcceptanceRate = 0.9f;     // above this, increase K
    uint32_t maxTokens = 4096;            // max total tokens to generate
    uint32_t eosTokenId = 2;              // end-of-sequence token
    bool     adaptiveDraftSize = true;     // dynamically adjust K based on acceptance rate
};

// ---------------------------------------------------------------------------
// Speculative Decoding Pipeline
// ---------------------------------------------------------------------------
class SpeculativePipeline {
public:
    explicit SpeculativePipeline(const SpeculativePipelineConfig& cfg = {});
    ~SpeculativePipeline();

    // --- Model setup ---
    bool loadDraftModel(const std::string& path);
    bool loadTargetModel(const std::string& path);
    void unloadModels();
    bool isReady() const;

    // --- Generation ---
    SpeculativeResult generate(
        const std::vector<uint32_t>& prompt,
        std::function<void(const std::vector<uint32_t>&)> onToken = nullptr
    );

    // --- Streaming generation ---
    void generateStreaming(
        const std::vector<uint32_t>& prompt,
        std::function<void(uint32_t token, bool done)> onToken
    );

    // --- Stats ---
    struct Stats {
        uint64_t totalDrafted;
        uint64_t totalAccepted;
        uint64_t totalRejected;
        uint64_t totalVerifyCalls;
        double   avgAcceptanceRate;
        double   avgDraftSize;
        uint64_t totalTokensGenerated;
    };
    Stats getStats() const;
    void resetStats();

    // --- KV cache ownership ---
    void setOwnershipTracker(std::shared_ptr<KVCacheOwnershipTracker> tracker);

private:
    SpeculativePipelineConfig m_cfg;
    std::unique_ptr<IDraftModel> m_draftModel;
    std::unique_ptr<ITargetModel> m_targetModel;
    std::shared_ptr<KVCacheOwnershipTracker> m_ownershipTracker;

    // Stats
    mutable std::mutex m_statsMutex;
    uint64_t m_totalDrafted{0};
    uint64_t m_totalAccepted{0};
    uint64_t m_totalRejected{0};
    uint64_t m_totalVerifyCalls{0};
    double   m_avgAcceptanceRate{0.0};
    double   m_avgDraftSize{0.0};

    // Adaptive draft size
    uint32_t m_currentDraftSize;
    std::mt19937 m_rng;

    // --- Internal pipeline stages ---
    std::vector<TokenProb> draftStage(
        const std::vector<uint32_t>& context,
        uint32_t count
    );
    std::vector<TokenProb> verifyStage(
        const std::vector<uint32_t>& context,
        const std::vector<TokenProb>& draftTokens
    );
    uint32_t acceptTokens(
        const std::vector<TokenProb>& draftTokens,
        const std::vector<TokenProb>& targetProbs,
        std::vector<uint32_t>& acceptedOut
    );
    void adaptDraftSize(float acceptanceRate);

    // --- Sampling ---
    uint32_t sampleToken(const std::vector<float>& logits);
    void applyTemperature(std::vector<float>& logits, float temp);
    void applyTopP(std::vector<float>& logits, float p);
    void applyTopK(std::vector<float>& logits, uint32_t k);
};

} // namespace RawrXD::Inference
