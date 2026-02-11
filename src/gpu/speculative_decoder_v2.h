// ============================================================================
// speculative_decoder_v2.h — Speculative Decoding Engine (Upgraded)
// ============================================================================
// Full draft-verify-accept pipeline with batch verification, acceptance rate
// tracking, tree-based speculation, and dynamic draft length adjustment.
// Replaces the basic stub at src/gpu/speculative_decoder.h.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>
#include <chrono>
#include <deque>

namespace RawrXD {
namespace Speculative {

// ============================================================================
// Result Type
// ============================================================================

struct SpecResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static SpecResult ok(const char* msg = "OK") {
        SpecResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }

    static SpecResult error(const char* msg, int code = -1) {
        SpecResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};

// ============================================================================
// Token
// ============================================================================

struct Token {
    int         id;
    float       logprob;
    std::string text;
};

// ============================================================================
// Speculation Config
// ============================================================================

struct SpeculationConfig {
    int         maxDraftTokens     = 5;   // Max tokens per draft step
    int         minDraftTokens     = 1;   // Min draft tokens
    float       acceptanceThreshold = 0.3f; // Min prob ratio for acceptance
    bool        adaptiveDraftLen   = true; // Auto-adjust draft length
    bool        treeSpeculation    = false; // Use tree-based speculation
    int         treeBranching      = 2;    // Branches per tree node
    int         treeDepth          = 3;    // Max tree depth
    float       temperatureDraft   = 0.0f; // Draft model temperature
    float       temperatureTarget  = 0.0f; // Target model temperature (0 = greedy)
};

// ============================================================================
// Acceptance Stats
// ============================================================================

struct AcceptanceStats {
    uint64_t    totalDrafted;
    uint64_t    totalAccepted;
    uint64_t    totalRejected;
    uint64_t    totalVerified;      // Total verification calls
    float       acceptanceRate;     // Running acceptance rate
    float       tokensPerSecond;    // Effective generation speed
    float       speedupRatio;       // vs. single-model baseline
    int         currentDraftLen;    // Current adaptive draft length
    float       avgDraftLatencyMs;
    float       avgVerifyLatencyMs;
};

// ============================================================================
// Model Interface — abstract model for draft/target
// ============================================================================

struct ModelInference {
    // Generate logprobs for next token given context
    // Returns: vector of (token_id, logprob) for top-K
    using LogprobCallback = std::vector<std::pair<int, float>>(*)(
        const std::vector<int>& context, int topK, void* userData);

    // Batch logprobs: verify multiple continuation points at once
    using BatchLogprobCallback = std::vector<std::vector<std::pair<int, float>>>(*)(
        const std::vector<std::vector<int>>& contexts, int topK, void* userData);

    // Decode token ID to text
    using DecodeCallback = std::string(*)(int tokenId, void* userData);

    // Encode text to token IDs
    using EncodeCallback = std::vector<int>(*)(const std::string& text, void* userData);

    LogprobCallback      logprobs       = nullptr;
    BatchLogprobCallback batchLogprobs  = nullptr;
    DecodeCallback       decode         = nullptr;
    EncodeCallback       encode         = nullptr;
    void*                userData       = nullptr;
    std::string          modelId;
};

// ============================================================================
// Token Generation Callback
// ============================================================================

using TokenCallback = void(*)(const Token& token, bool isDraft, void* userData);

// ============================================================================
// Speculative Decoder V2
// ============================================================================

class SpeculativeDecoderV2 {
public:
    SpeculativeDecoderV2();
    ~SpeculativeDecoderV2();

    // Singleton
    static SpeculativeDecoderV2& Global();

    // ---- Model Setup ----

    SpecResult setDraftModel(const ModelInference& model);
    SpecResult setTargetModel(const ModelInference& model);

    // ---- Configuration ----

    void setConfig(const SpeculationConfig& config);
    const SpeculationConfig& getConfig() const { return m_config; }

    // ---- Generation ----

    // Generate tokens using speculative decoding
    // Returns verified token IDs
    struct GenerateResult {
        bool success;
        const char* detail;
        std::vector<Token> tokens;
        AcceptanceStats stats;

        static GenerateResult ok(std::vector<Token> toks, AcceptanceStats s) {
            GenerateResult r;
            r.success = true;
            r.detail  = "Generated";
            r.tokens  = std::move(toks);
            r.stats   = s;
            return r;
        }

        static GenerateResult error(const char* msg) {
            GenerateResult r;
            r.success = false;
            r.detail  = msg;
            return r;
        }
    };

    GenerateResult generate(const std::vector<int>& promptTokens,
                             int maxNewTokens);

    // Generate with streaming callback
    GenerateResult generateStreaming(const std::vector<int>& promptTokens,
                                     int maxNewTokens,
                                     TokenCallback callback,
                                     void* userData);

    // Generate from text (uses encode/decode)
    GenerateResult generateFromText(const std::string& prompt,
                                     int maxNewTokens);

    // ---- Statistics ----

    AcceptanceStats getStats() const;
    void resetStats();

    // ---- Control ----

    void abort();   // Cancel ongoing generation
    bool isGenerating() const { return m_generating.load(); }

private:
    // Core speculation loop
    struct DraftResult {
        std::vector<int>   tokenIds;
        std::vector<float> logprobs;
    };

    // Draft: generate candidate tokens with draft model
    DraftResult draft(const std::vector<int>& context, int numTokens);

    // Verify: check draft tokens against target model
    struct VerifyResult {
        int acceptedCount;          // How many draft tokens were accepted
        Token correctionToken;      // Target model's token if last draft rejected
        bool allAccepted;
    };

    VerifyResult verify(const std::vector<int>& context,
                         const DraftResult& drafted);

    // Batch verify for tree speculation
    VerifyResult verifyBatch(const std::vector<int>& context,
                              const std::vector<DraftResult>& branches);

    // Adaptive draft length adjustment
    void adjustDraftLength();

    // Update running stats
    void updateStats(int drafted, int accepted, float draftMs, float verifyMs);

    // Models
    ModelInference m_draftModel;
    ModelInference m_targetModel;
    bool           m_draftReady  = false;
    bool           m_targetReady = false;

    // Config
    SpeculationConfig m_config;

    // Stats
    mutable std::mutex m_statsMutex;
    AcceptanceStats    m_stats = {};

    // Running averages for adaptive draft length
    struct RunningAverage {
        std::deque<float> window;
        size_t maxSize = 50;
        void push(float val) {
            window.push_back(val);
            while (window.size() > maxSize) window.pop_front();
        }
        float mean() const {
            if (window.empty()) return 0.0f;
            float sum = 0.0f;
            for (float v : window) sum += v;
            return sum / static_cast<float>(window.size());
        }
    };

    RunningAverage m_acceptRateAvg;
    RunningAverage m_draftLatencyAvg;
    RunningAverage m_verifyLatencyAvg;

    // State
    std::atomic<bool> m_generating{false};
    std::atomic<bool> m_abortRequested{false};
    mutable std::mutex m_mutex;
};

} // namespace Speculative
} // namespace RawrXD
