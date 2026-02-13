// ============================================================================
// vision_kv_isolation.cpp — Vision/Text KV Cache Isolation Implementation
// ============================================================================
// Implements separated KV cache management for vision and text tokens.
// Provides cross-attention computation, KV merging, decay, and eviction.
//
// Cross-Attention algorithm:
//   For each text query token q_i, each attention head h:
//     attn_weights[j] = (q_i · k_vision_j) * scale / sqrt(headDim)
//     attn_weights = softmax(attn_weights)
//     output[i,h] = sum_j(attn_weights[j] * v_vision_j)
//
// This implements scaled dot-product attention between text queries and
// vision keys/values with the configurable crossAttentionScale factor.
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#include "vision_kv_isolation.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <numeric>

namespace RawrXD {
namespace Vision {

// ============================================================================
// Singleton
// ============================================================================

VisionKVIsolator& VisionKVIsolator::instance() {
    static VisionKVIsolator inst;
    return inst;
}

VisionKVIsolator::VisionKVIsolator()
    : policy_()
{
    for (uint32_t i = 0; i < MAX_LAYERS; ++i) {
        layerCaches_[i].layerIndex = i;
    }
}

// ============================================================================
// Configuration
// ============================================================================

void VisionKVIsolator::setPolicy(const KVIsolationPolicy& policy) {
    std::lock_guard<std::mutex> lock(mutex_);
    policy_ = policy;
}

const KVIsolationPolicy& VisionKVIsolator::getPolicy() const {
    return policy_;
}

// ============================================================================
// Internal: Layer KV Access
// ============================================================================

VisionKVCache* VisionKVIsolator::getLayerKV(uint32_t layerIndex) {
    if (layerIndex >= MAX_LAYERS) return nullptr;
    return &layerCaches_[layerIndex];
}

const VisionKVCache* VisionKVIsolator::getLayerKV(uint32_t layerIndex) const {
    if (layerIndex >= MAX_LAYERS) return nullptr;
    return &layerCaches_[layerIndex];
}

// ============================================================================
// Allocate Vision KV Cache
// ============================================================================

VisionResult VisionKVIsolator::allocateVisionKV(uint32_t layerIndex,
                                                  uint32_t maxTokens,
                                                  uint32_t headDim,
                                                  uint32_t numHeads) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (layerIndex >= MAX_LAYERS) {
        return VisionResult::error("Layer index out of range", 1);
    }

    if (maxTokens == 0 || headDim == 0 || numHeads == 0) {
        return VisionResult::error("Invalid KV dimensions", 2);
    }

    // Enforce policy cap
    uint32_t effectiveMax = std::min(maxTokens, policy_.maxVisionKVTokens);

    VisionKVCache& cache = layerCaches_[layerIndex];

    // Free existing allocation if any
    if (cache.allocated) {
        cache.keys.data.clear();
        cache.values.data.clear();
    }

    // Allocate key region
    cache.keys.maxTokens = effectiveMax;
    cache.keys.headDim = headDim;
    cache.keys.numHeads = numHeads;
    cache.keys.tokenCount = 0;
    cache.keys.layerIndex = layerIndex;
    cache.keys.data.resize(cache.keys.capacityFloats(), 0.0f);

    // Allocate value region
    cache.values.maxTokens = effectiveMax;
    cache.values.headDim = headDim;
    cache.values.numHeads = numHeads;
    cache.values.tokenCount = 0;
    cache.values.layerIndex = layerIndex;
    cache.values.data.resize(cache.values.capacityFloats(), 0.0f);

    cache.layerIndex = layerIndex;
    cache.allocated = true;
    cache.memorySizeBytes = (cache.keys.capacityFloats() + cache.values.capacityFloats())
                            * sizeof(float);

    regionsAllocated_.fetch_add(1, std::memory_order_relaxed);

    return VisionResult::ok("Vision KV allocated");
}

// ============================================================================
// Free Vision KV
// ============================================================================

VisionResult VisionKVIsolator::freeVisionKV(uint32_t layerIndex) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (layerIndex >= MAX_LAYERS) {
        return VisionResult::error("Layer index out of range", 1);
    }

    VisionKVCache& cache = layerCaches_[layerIndex];
    if (!cache.allocated) {
        return VisionResult::ok("Already freed");
    }

    cache.keys.data.clear();
    cache.keys.data.shrink_to_fit();
    cache.values.data.clear();
    cache.values.data.shrink_to_fit();
    cache.keys.tokenCount = 0;
    cache.values.tokenCount = 0;
    cache.allocated = false;
    cache.memorySizeBytes = 0;

    return VisionResult::ok("Vision KV freed");
}

void VisionKVIsolator::freeAllVisionKV() {
    std::lock_guard<std::mutex> lock(mutex_);
    for (uint32_t i = 0; i < MAX_LAYERS; ++i) {
        VisionKVCache& cache = layerCaches_[i];
        if (cache.allocated) {
            cache.keys.data.clear();
            cache.keys.data.shrink_to_fit();
            cache.values.data.clear();
            cache.values.data.shrink_to_fit();
            cache.keys.tokenCount = 0;
            cache.values.tokenCount = 0;
            cache.allocated = false;
            cache.memorySizeBytes = 0;
        }
    }
}

bool VisionKVIsolator::hasVisionKV(uint32_t layerIndex) const {
    if (layerIndex >= MAX_LAYERS) return false;
    return layerCaches_[layerIndex].allocated;
}

// ============================================================================
// Write Vision KV — Batch write
// ============================================================================

VisionResult VisionKVIsolator::writeVisionKV(uint32_t layerIndex,
                                               const float* keys,
                                               const float* values,
                                               uint32_t numTokens) {
    std::lock_guard<std::mutex> lock(mutex_);

    VisionKVCache* cache = getLayerKV(layerIndex);
    if (!cache || !cache->allocated) {
        return VisionResult::error("Vision KV not allocated for this layer", 1);
    }

    if (!keys || !values) {
        return VisionResult::error("Null key/value pointers", 2);
    }

    // Check capacity
    if (cache->keys.tokenCount + numTokens > cache->keys.maxTokens) {
        // Evict oldest tokens to make room
        uint32_t needed = (cache->keys.tokenCount + numTokens) - cache->keys.maxTokens;
        uint32_t stride = cache->keys.tokenStride();

        // Shift data left by 'needed' tokens (FIFO eviction)
        uint32_t remaining = cache->keys.tokenCount - needed;
        if (remaining > 0) {
            memmove(cache->keys.data.data(),
                    cache->keys.data.data() + static_cast<size_t>(needed) * stride,
                    static_cast<size_t>(remaining) * stride * sizeof(float));
            memmove(cache->values.data.data(),
                    cache->values.data.data() + static_cast<size_t>(needed) * stride,
                    static_cast<size_t>(remaining) * stride * sizeof(float));
        }
        cache->keys.tokenCount = remaining;
        cache->values.tokenCount = remaining;
        visionKVEvictions_.fetch_add(needed, std::memory_order_relaxed);
    }

    // Append new tokens
    uint32_t stride = cache->keys.tokenStride();
    uint32_t offset = cache->keys.tokenCount;

    memcpy(cache->keys.tokenPtr(offset), keys,
           static_cast<size_t>(numTokens) * stride * sizeof(float));
    memcpy(cache->values.tokenPtr(offset), values,
           static_cast<size_t>(numTokens) * stride * sizeof(float));

    cache->keys.tokenCount += numTokens;
    cache->values.tokenCount += numTokens;

    totalVisionTokens_.fetch_add(numTokens, std::memory_order_relaxed);

    return VisionResult::ok("Vision KV written");
}

// ============================================================================
// Write Single Vision Token
// ============================================================================

VisionResult VisionKVIsolator::writeVisionToken(uint32_t layerIndex,
                                                  uint32_t tokenIndex,
                                                  const float* key,
                                                  const float* value) {
    std::lock_guard<std::mutex> lock(mutex_);

    VisionKVCache* cache = getLayerKV(layerIndex);
    if (!cache || !cache->allocated) {
        return VisionResult::error("Vision KV not allocated", 1);
    }

    if (tokenIndex >= cache->keys.maxTokens) {
        return VisionResult::error("Token index out of range", 2);
    }

    uint32_t stride = cache->keys.tokenStride();
    memcpy(cache->keys.tokenPtr(tokenIndex), key, stride * sizeof(float));
    memcpy(cache->values.tokenPtr(tokenIndex), value, stride * sizeof(float));

    if (tokenIndex >= cache->keys.tokenCount) {
        cache->keys.tokenCount = tokenIndex + 1;
        cache->values.tokenCount = tokenIndex + 1;
    }

    return VisionResult::ok("Vision token written");
}

// ============================================================================
// Cross-Attention — Text queries attending to vision KV
// ============================================================================
// For each text token i, each head h:
//   score[j] = (Q[i,h] · K_vision[j,h]) * crossAttentionScale / sqrt(headDim)
//   attn[j] = softmax(score)
//   output[i,h] = sum_j(attn[j] * V_vision[j,h])

CrossAttentionResult VisionKVIsolator::computeCrossAttention(
    uint32_t layerIndex,
    const float* textQueries,
    uint32_t numTextTokens,
    float* output)
{
    auto startTime = std::chrono::high_resolution_clock::now();

    std::lock_guard<std::mutex> lock(mutex_);

    const VisionKVCache* cache = getLayerKV(layerIndex);
    if (!cache || !cache->allocated || cache->keys.tokenCount == 0) {
        return CrossAttentionResult::error();
    }

    if (!textQueries || !output) {
        return CrossAttentionResult::error();
    }

    uint32_t V = cache->keys.tokenCount;  // Vision tokens
    uint32_t T = numTextTokens;            // Text tokens
    uint32_t D = cache->keys.headDim;      // Head dimension
    uint32_t H = cache->keys.numHeads;     // Number of heads
    uint32_t stride = D * H;              // Floats per token

    float invSqrtD = 1.0f / sqrtf(static_cast<float>(D));
    float scale = policy_.crossAttentionScale * invSqrtD;

    // Attention weights buffer: [V] scores per text token per head
    std::vector<float> attnWeights(V);

    float totalAttnOnVision = 0.0f;
    float maxAttnWeight = 0.0f;

    // For each text token
    for (uint32_t t = 0; t < T; ++t) {
        const float* q_base = textQueries + static_cast<size_t>(t) * stride;
        float* out_base = output + static_cast<size_t>(t) * stride;

        // Zero output for this token
        memset(out_base, 0, stride * sizeof(float));

        // For each attention head
        for (uint32_t h = 0; h < H; ++h) {
            const float* q = q_base + h * D;

            // Step 1: Compute raw attention scores Q · K^T
            float maxScore = -1e30f;
            for (uint32_t v = 0; v < V; ++v) {
                const float* k = cache->keys.tokenPtr(v) + h * D;

                float dot = 0.0f;
                for (uint32_t d = 0; d < D; ++d) {
                    dot += q[d] * k[d];
                }
                attnWeights[v] = dot * scale;
                if (attnWeights[v] > maxScore) {
                    maxScore = attnWeights[v];
                }
            }

            // Step 2: Softmax with numerical stability
            float sumExp = 0.0f;
            for (uint32_t v = 0; v < V; ++v) {
                attnWeights[v] = expf(attnWeights[v] - maxScore);
                sumExp += attnWeights[v];
            }
            if (sumExp > 0.0f) {
                float invSum = 1.0f / sumExp;
                for (uint32_t v = 0; v < V; ++v) {
                    attnWeights[v] *= invSum;
                }
            }

            // Step 3: Weighted sum of values
            float* out_head = out_base + h * D;
            for (uint32_t v = 0; v < V; ++v) {
                const float* val = cache->values.tokenPtr(v) + h * D;
                float w = attnWeights[v];

                if (w > maxAttnWeight) maxAttnWeight = w;
                totalAttnOnVision += w;

                for (uint32_t d = 0; d < D; ++d) {
                    out_head[d] += w * val[d];
                }
            }
        }
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    crossAttentionCount_.fetch_add(1, std::memory_order_relaxed);
    totalTextTokens_.fetch_add(T, std::memory_order_relaxed);
    crossAttentionTimeAccum_ += elapsedMs;

    CrossAttentionResult result = CrossAttentionResult::ok(T, V);
    result.computeTimeMs = elapsedMs;
    result.maxAttentionWeight = maxAttnWeight;
    if (T > 0 && H > 0 && V > 0) {
        result.avgAttentionWeight = totalAttnOnVision /
                                    static_cast<float>(T * H * V);
    }

    return result;
}

// ============================================================================
// Merge Vision KV into Text KV — For unified attention pass
// ============================================================================

VisionResult VisionKVIsolator::mergeVisionIntoTextKV(
    uint32_t layerIndex,
    const float* textKeys,
    const float* textValues,
    uint32_t numTextTokens,
    float* combinedKeys,
    float* combinedValues,
    uint32_t& totalTokens)
{
    std::lock_guard<std::mutex> lock(mutex_);

    const VisionKVCache* cache = getLayerKV(layerIndex);

    uint32_t visionTokens = 0;
    uint32_t stride = 0;

    if (cache && cache->allocated && cache->keys.tokenCount > 0) {
        visionTokens = cache->keys.tokenCount;
        stride = cache->keys.tokenStride();
    }

    if (stride == 0 && numTextTokens > 0) {
        // No vision KV — just copy text through
        // Caller must have set stride from their own context
        totalTokens = numTextTokens;
        return VisionResult::ok("No vision KV to merge");
    }

    totalTokens = numTextTokens + visionTokens;

    // Layout: [text tokens | vision tokens]
    // Copy text keys/values first
    if (numTextTokens > 0 && textKeys && combinedKeys) {
        memcpy(combinedKeys, textKeys,
               static_cast<size_t>(numTextTokens) * stride * sizeof(float));
    }
    if (numTextTokens > 0 && textValues && combinedValues) {
        memcpy(combinedValues, textValues,
               static_cast<size_t>(numTextTokens) * stride * sizeof(float));
    }

    // Append vision keys/values with scaling
    if (visionTokens > 0 && combinedKeys && combinedValues) {
        float* dstKeys = combinedKeys + static_cast<size_t>(numTextTokens) * stride;
        float* dstValues = combinedValues + static_cast<size_t>(numTextTokens) * stride;

        // Copy vision keys with cross-attention scaling
        const float* srcKeys = cache->keys.data.data();
        const float* srcValues = cache->values.data.data();
        size_t visionFloats = static_cast<size_t>(visionTokens) * stride;

        for (size_t i = 0; i < visionFloats; ++i) {
            dstKeys[i] = srcKeys[i] * policy_.crossAttentionScale;
        }

        // Copy vision values (no scaling on values)
        memcpy(dstValues, srcValues, visionFloats * sizeof(float));
    }

    return VisionResult::ok("Vision KV merged");
}

// ============================================================================
// Evict Vision Tokens — FIFO removal from the front
// ============================================================================

VisionResult VisionKVIsolator::evictVisionTokens(uint32_t layerIndex,
                                                   uint32_t count) {
    std::lock_guard<std::mutex> lock(mutex_);

    VisionKVCache* cache = getLayerKV(layerIndex);
    if (!cache || !cache->allocated) {
        return VisionResult::error("No vision KV at this layer", 1);
    }

    uint32_t toEvict = std::min(count, cache->keys.tokenCount);
    if (toEvict == 0) {
        return VisionResult::ok("Nothing to evict");
    }

    uint32_t remaining = cache->keys.tokenCount - toEvict;
    uint32_t stride = cache->keys.tokenStride();

    if (remaining > 0) {
        // Shift remaining tokens to the front
        memmove(cache->keys.data.data(),
                cache->keys.data.data() + static_cast<size_t>(toEvict) * stride,
                static_cast<size_t>(remaining) * stride * sizeof(float));
        memmove(cache->values.data.data(),
                cache->values.data.data() + static_cast<size_t>(toEvict) * stride,
                static_cast<size_t>(remaining) * stride * sizeof(float));
    }

    cache->keys.tokenCount = remaining;
    cache->values.tokenCount = remaining;

    visionKVEvictions_.fetch_add(toEvict, std::memory_order_relaxed);

    return VisionResult::ok("Vision tokens evicted");
}

// ============================================================================
// Apply Time Decay — Reduce vision KV influence as text context grows
// ============================================================================
// Multiplies all vision key vectors by decay^(currentTextPosition - insertPosition)
// This naturally reduces vision attention weight as more text is generated.

VisionResult VisionKVIsolator::applyDecay(uint32_t layerIndex,
                                            uint32_t currentTextPosition) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!policy_.decayVisionOverTime) {
        return VisionResult::ok("Decay disabled");
    }

    VisionKVCache* cache = getLayerKV(layerIndex);
    if (!cache || !cache->allocated || cache->keys.tokenCount == 0) {
        return VisionResult::ok("No vision KV to decay");
    }

    uint32_t stride = cache->keys.tokenStride();
    float rate = policy_.decayRate;

    // Apply geometric decay: older vision tokens get more decay
    // Token 0 is oldest, token N-1 is newest
    for (uint32_t v = 0; v < cache->keys.tokenCount; ++v) {
        // Distance from "insertion": approximate as position relative to text
        uint32_t age = currentTextPosition; // All vision tokens are "old" vs text
        float decayFactor = powf(rate, static_cast<float>(age + (cache->keys.tokenCount - v)));

        // Clamp to minimum to avoid near-zero keys
        decayFactor = std::max(decayFactor, 0.01f);

        float* keyPtr = cache->keys.tokenPtr(v);
        for (uint32_t d = 0; d < stride; ++d) {
            keyPtr[d] *= decayFactor;
        }
    }

    return VisionResult::ok("Decay applied");
}

// ============================================================================
// Statistics
// ============================================================================

KVIsolationStats VisionKVIsolator::getStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    KVIsolationStats stats = {};
    stats.regionsAllocated = regionsAllocated_.load();
    stats.totalVisionTokensStored = totalVisionTokens_.load();
    stats.totalTextTokensProcessed = totalTextTokens_.load();
    stats.crossAttentionComputed = crossAttentionCount_.load();
    stats.visionKVEvictions = visionKVEvictions_.load();

    // Compute total current memory
    uint64_t mem = 0;
    for (uint32_t i = 0; i < MAX_LAYERS; ++i) {
        if (layerCaches_[i].allocated) {
            mem += layerCaches_[i].memorySizeBytes;
        }
    }
    stats.totalMemoryBytes = mem;

    if (stats.crossAttentionComputed > 0) {
        stats.avgCrossAttentionMs = crossAttentionTimeAccum_ /
                                    static_cast<double>(stats.crossAttentionComputed);
    }

    return stats;
}

void VisionKVIsolator::resetStats() {
    std::lock_guard<std::mutex> lock(mutex_);
    regionsAllocated_.store(0);
    totalVisionTokens_.store(0);
    totalTextTokens_.store(0);
    crossAttentionCount_.store(0);
    visionKVEvictions_.store(0);
    crossAttentionTimeAccum_ = 0.0;
}

} // namespace Vision
} // namespace RawrXD
