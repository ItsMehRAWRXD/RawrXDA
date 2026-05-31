// ============================================================================
// flash_attention_bridge.h — FlashAttention Integration Bridge Header
// ============================================================================
// Declares the FlashAttention bridge for integration with RawrXD inference pipeline.
//
// Integration Point: src/inference/rawr_monolith_v2.cpp::attention_head()
// License: FEATURE_FLASH_ATTENTION (0x40) — Pro tier
// ============================================================================

#pragma once

#include <cstdint>
#include <string>

namespace RawrXD
{

/**
 * @brief Execute FlashAttention forward pass with fallback
 *
 * @param Q Query tensor [batch * heads * seqM * headDim]
 * @param K Key tensor [batch * kvHeads * seqN * headDim]
 * @param V Value tensor [batch * kvHeads * seqN * headDim]
 * @param O Output tensor [batch * heads * seqM * headDim]
 * @param seqLenM Query sequence length (M)
 * @param seqLenN Key/Value sequence length (N)
 * @param headDim Head dimension (D)
 * @param numHeads Number of query heads
 * @param numKVHeads Number of KV heads (GQA)
 * @param batchSize Batch size
 * @param causal Whether to apply causal mask (autoregressive)
 * @return true if FlashAttention was used, false if standard fallback
 *
 * Contract (v1): Q/K/V/O are **FP32**, contiguous, 64-byte aligned (CPU AVX-512 path).
 * FP8-packed KV + per-block scales and a Vulkan fused dequant+attention entry point are
 * intentionally **not** wired here yet — see `src/inference/flash_attention_vulkan_fp8.h`
 * (`DispatchFlashAttentionVulkanFP8`) rather than overloading these `float*` parameters.
 */
bool DispatchFlashAttention(float* Q, float* K, float* V, float* O, int32_t seqLenM, int32_t seqLenN, int32_t headDim,
                            int32_t numHeads, int32_t numKVHeads, int32_t batchSize, bool causal = true);

/**
 * @brief Check if FlashAttention is available (licensed + AVX-512 capable)
 *
 * @return true if FlashAttention can be used, false otherwise
 */
bool IsFlashAttentionAvailable();

/**
 * @brief Get FlashAttention performance counters
 *
 * @param outCalls Output: total forward calls since initialization
 * @param outTiles Output: total tiles processed since initialization
 */
void GetFlashAttentionCounters(uint64_t& outCalls, uint64_t& outTiles);

/**
 * @brief Get FlashAttention status string for diagnostics
 *
 * @return Human-readable status string
 */
std::string GetFlashAttentionStatus();

}  // namespace RawrXD

// ============================================================================
// C Interface for Integration with Existing Code
// ============================================================================

#ifdef __cplusplus
extern "C"
{
#endif

    /**
     * @brief C interface for FlashAttention dispatch
     */
    __declspec(dllexport) bool RawrXD_DispatchFlashAttention(float* Q, float* K, float* V, float* O, int32_t seqLenM,
                                                             int32_t seqLenN, int32_t headDim, int32_t numHeads,
                                                             int32_t numKVHeads, int32_t batchSize, int32_t causal);

    /**
     * @brief C interface to check FlashAttention availability
     */
    __declspec(dllexport) bool RawrXD_IsFlashAttentionAvailable();

    /**
     * @brief C interface to get FlashAttention counters
     */
    __declspec(dllexport) void RawrXD_GetFlashAttentionCounters(uint64_t* outCalls, uint64_t* outTiles);

#ifdef __cplusplus
}
#endif