// ============================================================================
// vision_kv_isolation.hpp — Vision/Text KV Cache Isolation Layer
// ============================================================================
// Separates the key-value cache for vision tokens from text tokens during
// transformer inference. This prevents vision patch embeddings from polluting
// the text KV cache, enabling:
//
//   - Independent eviction policies (vision KV can be shorter-lived)
//   - Cross-attention bias scaling (reduce vision influence on text)
//   - Memory budget separation (cap vision KV independently)
//   - Cache coherence for multi-image conversations (swap vision KV per image)
//
// Architecture:
//   Text KV:   [text_k_0 ... text_k_N] [text_v_0 ... text_v_N]
//   Vision KV: [vis_k_0  ... vis_k_M]  [vis_v_0  ... vis_v_M]
//
// During attention:
//   Q_text × K_combined^T = Q_text × [K_text | scale * K_vision]^T
//
// Integration: Called by transformer forward pass when vision tokens present.
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "vision_encoder.hpp"
#include <cstdint>
#include <cstddef>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstring>

namespace RawrXD {
namespace Vision {

// ============================================================================
// KV Isolation Policy
// ============================================================================
struct KVIsolationPolicy {
    bool     enabled;                  // Enable isolation (false = shared KV)
    float    crossAttentionScale;      // Scale factor for vision keys (0.0 - 1.0)
    uint32_t maxVisionKVTokens;        // Maximum vision tokens in KV cache
    uint32_t maxTextKVTokens;          // Maximum text tokens (0 = unlimited)
    bool     allowVisionSelfAttention; // Whether vision tokens attend to each other
    bool     decayVisionOverTime;      // Reduce vision influence as context grows
    float    decayRate;                // Per-token decay factor (0.999)

    KVIsolationPolicy()
        : enabled(true)
        , crossAttentionScale(0.5f)
        , maxVisionKVTokens(576)
        , maxTextKVTokens(0)
        , allowVisionSelfAttention(true)
        , decayVisionOverTime(false)
        , decayRate(0.999f)
    {}
};

// ============================================================================
// KV Region — A contiguous block of keys or values for one modality
// ============================================================================
struct KVRegion {
    std::vector<float> data;      // Flat buffer: [numTokens × headDim × numHeads]
    uint32_t tokenCount;          // Current number of tokens stored
    uint32_t maxTokens;           // Capacity
    uint32_t headDim;             // Dimension per attention head
    uint32_t numHeads;            // Number of attention heads
    uint32_t layerIndex;          // Which transformer layer this belongs to

    KVRegion()
        : tokenCount(0), maxTokens(0), headDim(0), numHeads(0), layerIndex(0)
    {}

    // Stride per token: headDim * numHeads
    uint32_t tokenStride() const { return headDim * numHeads; }

    // Total capacity in floats
    uint64_t capacityFloats() const {
        return static_cast<uint64_t>(maxTokens) * headDim * numHeads;
    }

    // Pointer to token at index
    float* tokenPtr(uint32_t tokenIdx) {
        return data.data() + static_cast<size_t>(tokenIdx) * tokenStride();
    }
    const float* tokenPtr(uint32_t tokenIdx) const {
        return data.data() + static_cast<size_t>(tokenIdx) * tokenStride();
    }
};

// ============================================================================
// Vision KV Cache — Paired key and value regions for vision tokens
// ============================================================================
struct VisionKVCache {
    KVRegion keys;
    KVRegion values;
    uint32_t layerIndex;
    bool     allocated;
    uint64_t memorySizeBytes;

    VisionKVCache()
        : layerIndex(0), allocated(false), memorySizeBytes(0)
    {}
};

// ============================================================================
// Cross-Attention Output — Result of vision×text attention computation
// ============================================================================
struct CrossAttentionResult {
    bool     success;
    uint32_t textTokens;
    uint32_t visionTokens;
    float    avgAttentionWeight;   // Average attention weight on vision tokens
    float    maxAttentionWeight;   // Peak attention on a vision token
    double   computeTimeMs;

    static CrossAttentionResult ok(uint32_t t, uint32_t v) {
        CrossAttentionResult r;
        r.success = true;
        r.textTokens = t;
        r.visionTokens = v;
        r.avgAttentionWeight = 0.0f;
        r.maxAttentionWeight = 0.0f;
        r.computeTimeMs = 0.0;
        return r;
    }
    static CrossAttentionResult error() {
        CrossAttentionResult r;
        r.success = false;
        r.textTokens = 0;
        r.visionTokens = 0;
        r.avgAttentionWeight = 0.0f;
        r.maxAttentionWeight = 0.0f;
        r.computeTimeMs = 0.0;
        return r;
    }
};

// ============================================================================
// KV Isolation Statistics
// ============================================================================
struct KVIsolationStats {
    uint64_t regionsAllocated;
    uint64_t totalVisionTokensStored;
    uint64_t totalTextTokensProcessed;
    uint64_t crossAttentionComputed;
    uint64_t visionKVEvictions;
    uint64_t totalMemoryBytes;
    double   avgCrossAttentionMs;
};

// ============================================================================
// VisionKVIsolator — Manages separated KV caches for vision/text
// ============================================================================
class VisionKVIsolator {
public:
    static VisionKVIsolator& instance();

    // -----------------------------------------------------------------------
    // Configuration
    // -----------------------------------------------------------------------
    void setPolicy(const KVIsolationPolicy& policy);
    const KVIsolationPolicy& getPolicy() const;

    // -----------------------------------------------------------------------
    // KV Region Lifecycle
    // -----------------------------------------------------------------------

    // Allocate a vision KV cache for a specific transformer layer.
    VisionResult allocateVisionKV(uint32_t layerIndex,
                                   uint32_t maxTokens,
                                   uint32_t headDim,
                                   uint32_t numHeads);

    // Free a specific layer's vision KV cache.
    VisionResult freeVisionKV(uint32_t layerIndex);

    // Free all vision KV caches.
    void freeAllVisionKV();

    // Check if a layer has vision KV allocated.
    bool hasVisionKV(uint32_t layerIndex) const;

    // -----------------------------------------------------------------------
    // KV Write Operations
    // -----------------------------------------------------------------------

    // Write vision keys and values for a batch of tokens at a given layer.
    VisionResult writeVisionKV(uint32_t layerIndex,
                                const float* keys,
                                const float* values,
                                uint32_t numTokens);

    // Write a single vision token's KV at a specific position.
    VisionResult writeVisionToken(uint32_t layerIndex,
                                   uint32_t tokenIndex,
                                   const float* key,
                                   const float* value);

    // -----------------------------------------------------------------------
    // Cross-Attention Computation
    // -----------------------------------------------------------------------

    // Compute cross-attention: text queries attend to vision keys/values.
    // output = softmax(Q_text × (scale * K_vision)^T) × V_vision
    // Output shape: [numTextTokens × headDim × numHeads]
    CrossAttentionResult computeCrossAttention(
        uint32_t layerIndex,
        const float* textQueries,     // [numTextTokens × headDim × numHeads]
        uint32_t numTextTokens,
        float* output);               // [numTextTokens × headDim × numHeads]

    // -----------------------------------------------------------------------
    // KV Merging — Merge vision KV into text KV cache for unified attention
    // -----------------------------------------------------------------------

    // Merge vision KV into a flat combined buffer.
    // Combined = [text_KV | scaled_vision_KV]
    VisionResult mergeVisionIntoTextKV(
        uint32_t layerIndex,
        const float* textKeys,       // [numTextTokens × headDim × numHeads]
        const float* textValues,
        uint32_t numTextTokens,
        float* combinedKeys,          // [numText + numVision, headDim × numHeads]
        float* combinedValues,
        uint32_t& totalTokens);       // Output: total token count

    // -----------------------------------------------------------------------
    // Vision KV Eviction
    // -----------------------------------------------------------------------

    // Evict oldest vision tokens to make room (FIFO within layer).
    VisionResult evictVisionTokens(uint32_t layerIndex, uint32_t count);

    // Apply time-decay to vision KV (reduce influence over time).
    VisionResult applyDecay(uint32_t layerIndex, uint32_t currentTextPosition);

    // -----------------------------------------------------------------------
    // Statistics
    // -----------------------------------------------------------------------
    KVIsolationStats getStats() const;
    void resetStats();

private:
    VisionKVIsolator();
    ~VisionKVIsolator() = default;
    VisionKVIsolator(const VisionKVIsolator&) = delete;
    VisionKVIsolator& operator=(const VisionKVIsolator&) = delete;

    // Internal: get mutable reference to layer's vision KV
    VisionKVCache* getLayerKV(uint32_t layerIndex);
    const VisionKVCache* getLayerKV(uint32_t layerIndex) const;

    // -----------------------------------------------------------------------
    // State
    // -----------------------------------------------------------------------
    mutable std::mutex mutex_;
    KVIsolationPolicy policy_;

    // Per-layer vision KV caches (indexed by layer)
    static constexpr uint32_t MAX_LAYERS = 128;
    VisionKVCache layerCaches_[MAX_LAYERS];

    // Statistics
    std::atomic<uint64_t> regionsAllocated_{0};
    std::atomic<uint64_t> totalVisionTokens_{0};
    std::atomic<uint64_t> totalTextTokens_{0};
    std::atomic<uint64_t> crossAttentionCount_{0};
    std::atomic<uint64_t> visionKVEvictions_{0};
    double crossAttentionTimeAccum_ = 0.0;
};

} // namespace Vision
} // namespace RawrXD
