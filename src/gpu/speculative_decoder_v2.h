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
#include <array>
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
    // Ensemble fan-out: when > 1 and treeSpeculation is true, generate this many
    // independent DraftTrees (with geometrically offset branch seeds) and pick the
    // path with the highest accepted-token count after verifyTree().  Values above
    // ~4 offer diminishing returns while increasing verify latency linearly.
    int         ensembleDrafts     = 1;
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

    // ===========================================================================
    // Draft Tree Speculation
    // ===========================================================================
    // A tree-structured draft generates branching candidate sequences rather than
    // a single linear draft.  The verifier walks all root-to-leaf paths and accepts
    // the longest prefix that matches the target model's choices.
    //
    // Node layout: index 0 is a virtual root (the current context end).
    // Children of node i are contiguous in the nodes[] array.
    // ===========================================================================
    struct DraftTreeNode {
        int   tokenId  = -1;
        float logprob  = 0.0f;
        int   parentIdx = -1;   // -1 = root
        int   depth     = 0;
        // Index of first child in nodes[]; -1 if leaf.
        int   firstChild = -1;
        int   childCount = 0;
    };

    struct DraftTree {
        std::vector<DraftTreeNode> nodes;
        std::vector<int>          leafIndices;  // indices of leaf nodes

        // Return the token path from root (exclusive) down to node at idx.
        std::vector<int>   pathTokenIds(int idx) const;
        std::vector<float> pathLogprobs(int idx) const;
    };

    // Build a draft tree from context.  branchFactor shrinks with depth:
    // depth 1 = treeBranching, depth 2 = treeBranching/2 (min 1), etc.
    DraftTree draftTree(const std::vector<int>& context,
                        int depth, int branchFactor);

    // Seeded variant used by ensemble: branchSeed perturbs which candidate
    // tokens are chosen at each node, yielding diverse draft trees.
    DraftTree draftTreeSeeded(const std::vector<int>& context,
                              int depth, int branchFactor, uint32_t branchSeed);

    // Verify: check draft tokens against target model
    struct VerifyResult {
        int   acceptedCount   = 0;    // How many draft tokens were accepted
        Token correctionToken;        // Target model's token if last draft rejected
        bool  allAccepted     = false;
        // Populated by verifyTree() only; empty for the linear verify() path.
        // When non-empty, generateStreaming()'s accept phase reads these instead
        // of DraftResult, so the caller does not need to re-walk the tree.
        std::vector<int>   acceptedTokenIds;
        std::vector<float> acceptedLogprobs;
    };

    // Walk the tree and find the path whose accepted prefix is the longest;
    // returns the VerifyResult for that path.
    VerifyResult verifyTree(const std::vector<int>& context,
                            const DraftTree& tree);

    VerifyResult verify(const std::vector<int>& context,
                         const DraftResult& drafted);

    // Batch verify for tree speculation
    VerifyResult verifyBatch(const std::vector<int>& context,
                              const std::vector<DraftResult>& branches);

    // Adaptive draft length adjustment (also responds to VRAM pressure)
    void adjustDraftLength();

    // Update running stats
    void updateStats(int drafted, int accepted, float draftMs, float verifyMs);

    // Models
    ModelInference m_draftModel;
    ModelInference m_targetModel;
    bool           m_draftReady  = false;
    bool           m_targetReady = false;

    // KV-delta result cache: avoids re-encoding contexts the target model
    // already processed.  In sequential generation, every context except the
    // current draft window is a prefix of a previously-computed context, so
    // the hit rate approaches 100%.  Cache capacity: kMaxEntries positions.
    struct KVCacheDelta {
        struct Entry {
            uint64_t contextHash = 0;
            std::vector<std::pair<int, float>> logprobs;
        };
        static constexpr size_t kMaxEntries = 32;
        std::array<Entry, kMaxEntries> entries = {};
        size_t writeHead = 0;
        size_t count     = 0;

        static uint64_t hashContext(const std::vector<int>& ctx) {
            uint64_t h = 0xcbf29ce484222325ull; // FNV-1a offset
            for (int t : ctx) {
                h ^= static_cast<uint64_t>(static_cast<uint32_t>(t));
                h *= 0x00000100000001b3ull;       // FNV prime
            }
            return h;
        }

        const std::vector<std::pair<int, float>>*
        lookup(const std::vector<int>& ctx) const {
            uint64_t h = hashContext(ctx);
            for (size_t i = 0; i < count; ++i) {
                size_t idx = (writeHead + kMaxEntries - 1 - i) % kMaxEntries;
                if (entries[idx].contextHash == h &&
                    !entries[idx].logprobs.empty())
                    return &entries[idx].logprobs;
            }
            return nullptr;
        }

        void insert(const std::vector<int>& ctx,
                    std::vector<std::pair<int, float>> lp) {
            entries[writeHead] = {hashContext(ctx), std::move(lp)};
            writeHead = (writeHead + 1) % kMaxEntries;
            if (count < kMaxEntries) ++count;
        }

        void reset() { writeHead = 0; count = 0; }
    };
    KVCacheDelta m_kvDelta;

    // Cross-generation KV-prefix reuse: track the full context at the END of
    // the last generateStreaming() call.  On the next call, if promptTokens
    // is a prefix extension of m_lastContext, we skip m_kvDelta.reset() and
    // retain all still-valid cache entries — they remain valid because the
    // context prefix is identical.
    std::vector<int>  m_lastContext;

    // VRAM budget signalled from outside.  adjustDraftLength() consults this
    // together with rawrxd_check_swap_trigger to auto-throttle draft length.
    std::atomic<uint64_t> m_vramUsageBytes{0};

public:
    // Inform the decoder how many VRAM bytes are currently allocated.
    // Thread-safe; safe to call from a monitoring thread.
    void setVramUsage(uint64_t bytes) noexcept { m_vramUsageBytes.store(bytes, std::memory_order_relaxed); }

private:

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
