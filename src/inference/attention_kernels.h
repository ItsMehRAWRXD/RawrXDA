/**
 * @file attention_kernels.h
 * @brief Optimized attention mechanisms (FlashAttention, etc.)
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <cstddef>

namespace RawrXD::Inference {

// ============================================================================
// Multi-Head Attention
// ============================================================================

/**
 * @brief Standard multi-head attention
 * @param query Query tensor (batchSize * numHeads x seqLen x headDim)
 * @param key Key tensor (batchSize * numHeads x seqLen x headDim)
 * @param value Value tensor (batchSize * numHeads x seqLen x headDim)
 * @param output Output tensor (batchSize * numHeads x seqLen x headDim)
 * @param batchSize Batch size
 * @param seqLen Sequence length
 * @param numHeads Number of attention heads
 * @param headDim Dimension per head
 * @param mask Optional attention mask (batchSize x seqLen x seqLen)
 * @param scale Attention scale factor (default: 1/sqrt(headDim))
 */
void multi_head_attention(const float* query, const float* key, const float* value,
                         float* output,
                         int batchSize, int seqLen, int numHeads, int headDim,
                         const float* mask = nullptr,
                         float scale = 0.0f);

// ============================================================================
// FlashAttention (Memory-Efficient)
// ============================================================================

/**
 * @brief FlashAttention-style memory-efficient attention
 * @param query Query tensor
 * @param key Key tensor
 * @param value Value tensor
 * @param output Output tensor
 * @param batchSize Batch size
 * @param seqLen Sequence length
 * @param numHeads Number of attention heads
 * @param headDim Dimension per head
 * @param mask Optional attention mask
 * @param scale Attention scale factor
 * @param blockSize Block size for tiling (default: 64)
 */
void flash_attention(const float* query, const float* key, const float* value,
                    float* output,
                    int batchSize, int seqLen, int numHeads, int headDim,
                    const float* mask = nullptr,
                    float scale = 0.0f,
                    int blockSize = 64);

// ============================================================================
// Grouped-Query Attention (GQA)
// ============================================================================

/**
 * @brief Grouped-query attention (fewer KV heads than Q heads)
 * @param query Query tensor (batchSize * numQHeads x seqLen x headDim)
 * @param key Key tensor (batchSize * numKVHeads x seqLen x headDim)
 * @param value Value tensor (batchSize * numKVHeads x seqLen x headDim)
 * @param output Output tensor (batchSize * numQHeads x seqLen x headDim)
 * @param batchSize Batch size
 * @param seqLen Sequence length
 * @param numQHeads Number of query heads
 * @param numKVHeads Number of key/value heads
 * @param headDim Dimension per head
 * @param mask Optional attention mask
 * @param scale Attention scale factor
 */
void grouped_query_attention(const float* query, const float* key, const float* value,
                            float* output,
                            int batchSize, int seqLen, int numQHeads, int numKVHeads,
                            int headDim,
                            const float* mask = nullptr,
                            float scale = 0.0f);

// ============================================================================
// Sliding Window Attention
// ============================================================================

/**
 * @brief Sliding window attention (local attention only)
 * @param query Query tensor
 * @param key Key tensor
 * @param value Value tensor
 * @param output Output tensor
 * @param batchSize Batch size
 * @param seqLen Sequence length
 * @param numHeads Number of attention heads
 * @param headDim Dimension per head
 * @param windowSize Attention window size
 * @param scale Attention scale factor
 */
void sliding_window_attention(const float* query, const float* key, const float* value,
                             float* output,
                             int batchSize, int seqLen, int numHeads, int headDim,
                             int windowSize,
                             float scale = 0.0f);

// ============================================================================
// KV Cache Attention (for autoregressive generation)
// ============================================================================

/**
 * @brief Attention with KV cache for autoregressive generation
 * @param query Query tensor (batchSize * numHeads x seqLen x headDim)
 * @param keyCache Cached keys (batchSize * numHeads x cacheLen x headDim)
 * @param valueCache Cached values (batchSize * numHeads x cacheLen x headDim)
 * @param output Output tensor (batchSize * numHeads x seqLen x headDim)
 * @param batchSize Batch size
 * @param numHeads Number of attention heads
 * @param headDim Dimension per head
 * @param cacheLen Length of KV cache
 * @param seqLen Query sequence length
 * @param scale Attention scale factor
 */
void kv_cache_attention(const float* query,
                         const float* keyCache, const float* valueCache,
                         float* output,
                         int batchSize, int numHeads, int headDim,
                         int cacheLen, int seqLen,
                         float scale = 0.0f);

} // namespace RawrXD::Inference
