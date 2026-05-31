// ============================================================================
// flash_attention_integration.cpp — FlashAttention Integration for rawr_monolith_v2
// ============================================================================
// This file provides the integration layer between FlashAttention AVX-512 kernel
// and the standard attention implementation in rawr_monolith_v2.cpp.
//
// Integration Point: attention_head() function in rawr_monolith_v2.cpp
// Strategy: Dispatch to FlashAttention when available, fallback to standard
// ============================================================================

#include "flash_attention_bridge.h"
#include <vector>
#include <cmath>
#include <iostream>
#include <memory>

namespace rawrxd {
namespace monolith {

// ============================================================================
// FlashAttention Integration for Multi-Head Attention
// ============================================================================

/**
 * @brief Execute multi-head attention using FlashAttention if available
 * 
 * @param Q Query tensor [n_heads * head_dim]
 * @param K_cache Key cache [seq_len][n_heads * head_dim]
 * @param V_cache Value cache [seq_len][n_heads * head_dim]
 * @param n_heads Number of attention heads
 * @param head_dim Dimension of each head
 * @param seq_len Current sequence length
 * @param causal Whether to apply causal mask (true for autoregressive)
 * @param out Output tensor [n_heads * head_dim] (pre-allocated)
 * @return true if FlashAttention was used, false if standard fallback
 */
bool DispatchMultiHeadAttention(
    const std::vector<float>& Q,
    const std::vector<std::vector<float>>& K_cache,
    const std::vector<std::vector<float>>& V_cache,
    int n_heads,
    int head_dim,
    int seq_len,
    bool causal,
    std::vector<float>& out)
{
    // Check if FlashAttention is available
    if (!RawrXD::IsFlashAttentionAvailable()) {
        return false; // Fall back to standard attention
    }
    
    // Prepare contiguous Q/K/V tensors for FlashAttention
    // FlashAttention expects:
    //   Q: [batch * heads * seqM * headDim] (batch=1, seqM=1 for single token)
    //   K: [batch * kvHeads * seqN * headDim] (batch=1, seqN=seq_len)
    //   V: [batch * kvHeads * seqN * headDim] (batch=1, seqN=seq_len)
    //   O: [batch * heads * seqM * headDim] (batch=1, seqM=1 for single token)
    
    const int32_t batchSize = 1;
    const int32_t seqLenM = 1;  // Single query token
    const int32_t seqLenN = seq_len;  // Full KV cache
    const int32_t numHeads = n_heads;
    const int32_t numKVHeads = n_heads;  // Standard MHA (not GQA)
    
    // Allocate aligned buffers (64-byte alignment for AVX-512)
    // Using aligned_alloc for C++17 compatibility
    auto allocate_aligned = [](size_t size, size_t alignment) -> float* {
        void* ptr = nullptr;
        #ifdef _WIN32
            ptr = _aligned_malloc(size * sizeof(float), alignment);
        #else
            posix_memalign(&ptr, alignment, size * sizeof(float));
        #endif
        return static_cast<float*>(ptr);
    };
    
    auto free_aligned = [](float* ptr) {
        #ifdef _WIN32
            _aligned_free(ptr);
        #else
            free(ptr);
        #endif
    };
    
    // Allocate Q (single token, all heads)
    float* Q_aligned = allocate_aligned(numHeads * head_dim, 64);
    if (!Q_aligned) {
        std::cerr << "[FlashAttention] Failed to allocate aligned Q buffer\n";
        return false;
    }
    
    // Allocate K (full sequence, all heads)
    float* K_aligned = allocate_aligned(numKVHeads * seqLenN * head_dim, 64);
    if (!K_aligned) {
        free_aligned(Q_aligned);
        std::cerr << "[FlashAttention] Failed to allocate aligned K buffer\n";
        return false;
    }
    
    // Allocate V (full sequence, all heads)
    float* V_aligned = allocate_aligned(numKVHeads * seqLenN * head_dim, 64);
    if (!V_aligned) {
        free_aligned(Q_aligned);
        free_aligned(K_aligned);
        std::cerr << "[FlashAttention] Failed to allocate aligned V buffer\n";
        return false;
    }
    
    // Allocate O (single token, all heads)
    float* O_aligned = allocate_aligned(numHeads * head_dim, 64);
    if (!O_aligned) {
        free_aligned(Q_aligned);
        free_aligned(K_aligned);
        free_aligned(V_aligned);
        std::cerr << "[FlashAttention] Failed to allocate aligned O buffer\n";
        return false;
    }
    
    // Copy Q (already contiguous)
    std::copy(Q.begin(), Q.end(), Q_aligned);
    
    // Copy K and V from cache (need to reorganize from [seq_len][n_heads * head_dim]
    // to [n_heads][seq_len][head_dim])
    for (int h = 0; h < numKVHeads; h++) {
        for (int s = 0; s < seqLenN; s++) {
            for (int d = 0; d < head_dim; d++) {
                // K_cache[s][h * head_dim + d] -> K_aligned[h * seqLenN * head_dim + s * head_dim + d]
                K_aligned[h * seqLenN * head_dim + s * head_dim + d] = K_cache[s][h * head_dim + d];
                V_aligned[h * seqLenN * head_dim + s * head_dim + d] = V_cache[s][h * head_dim + d];
            }
        }
    }
    
    // Dispatch to FlashAttention
    bool success = RawrXD::DispatchFlashAttention(
        Q_aligned,
        K_aligned,
        V_aligned,
        O_aligned,
        seqLenM,
        seqLenN,
        head_dim,
        numHeads,
        numKVHeads,
        batchSize,
        causal
    );
    
    if (success) {
        // Copy output back to out vector
        // O_aligned is [n_heads * head_dim] (single token)
        std::copy(O_aligned, O_aligned + numHeads * head_dim, out.begin());
    }
    
    // Free aligned buffers
    free_aligned(Q_aligned);
    free_aligned(K_aligned);
    free_aligned(V_aligned);
    free_aligned(O_aligned);
    
    return success;
}

/**
 * @brief Standard attention fallback (original implementation)
 * 
 * This is the original attention_head implementation from rawr_monolith_v2.cpp
 * Used when FlashAttention is not available or fails.
 */
std::vector<float> StandardAttentionHead(
    const std::vector<float>& q,
    const std::vector<std::vector<float>>& K_cache,
    const std::vector<std::vector<float>>& V_cache,
    int head_idx,
    int head_dim)
{
    int seq_len = K_cache.size();
    std::vector<float> scores(seq_len);
    
    // Q·K^T / sqrt(d)
    float scale = 1.0f / sqrtf((float)head_dim);
    for (int i = 0; i < seq_len; i++) {
        float dot = 0;
        for (int j = 0; j < head_dim; j++)
            dot += q[head_idx * head_dim + j] * K_cache[i][head_idx * head_dim + j];
        scores[i] = dot * scale;
    }
    
    // Causal mask (already handled by only attending to past tokens)
    
    // Softmax
    float maxv = *std::max_element(scores.begin(), scores.end());
    float sum = 0;
    for (auto& v : scores) { v = expf(v - maxv); sum += v; }
    for (auto& v : scores) v /= sum;
    
    // Softmax(QK^T) · V
    std::vector<float> out(head_dim, 0);
    for (int i = 0; i < seq_len; i++)
        for (int j = 0; j < head_dim; j++)
            out[j] += scores[i] * V_cache[i][head_idx * head_dim + j];
    return out;
}

/**
 * @brief Unified attention dispatch - tries FlashAttention first, falls back to standard
 * 
 * @param q Query tensor [n_heads * head_dim]
 * @param K_cache Key cache [seq_len][n_heads * head_dim]
 * @param V_cache Value cache [seq_len][n_heads * head_dim]
 * @param n_heads Number of attention heads
 * @param head_dim Dimension of each head
 * @param seq_len Current sequence length
 * @param causal Whether to apply causal mask (true for autoregressive)
 * @return Output tensor [n_heads * head_dim]
 */
std::vector<float> UnifiedAttentionDispatch(
    const std::vector<float>& q,
    const std::vector<std::vector<float>>& K_cache,
    const std::vector<std::vector<float>>& V_cache,
    int n_heads,
    int head_dim,
    int seq_len,
    bool causal = true)
{
    // Try FlashAttention first
    std::vector<float> out(n_heads * head_dim, 0);
    
    if (DispatchMultiHeadAttention(q, K_cache, V_cache, n_heads, head_dim, seq_len, causal, out)) {
        // FlashAttention succeeded
        return out;
    }
    
    // Fall back to standard attention (head by head)
    std::fill(out.begin(), out.end(), 0);
    for (int h = 0; h < n_heads; h++) {
        auto head_result = StandardAttentionHead(q, K_cache, V_cache, h, head_dim);
        for (int d = 0; d < head_dim; d++)
            out[h * head_dim + d] = head_result[d];
    }
    
    return out;
}

} // namespace monolith
} // namespace rawrxd