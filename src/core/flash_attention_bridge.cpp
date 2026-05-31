// ============================================================================
// flash_attention_bridge.cpp — FlashAttention Integration Bridge
// ============================================================================
// Bridges FlashAttention AVX-512 kernel into RawrXD inference pipeline
// with graceful fallback to standard attention.
//
// Integration Point: src/inference/rawr_monolith_v2.cpp::attention_head()
// License: FEATURE_FLASH_ATTENTION (0x40) — Pro tier
// ============================================================================

#include "flash_attention.h"
#include <vector>
#include <memory>
#include <atomic>
#include <iostream>

namespace RawrXD {

// ============================================================================
// Global FlashAttention Engine (Lazy Initialization)
// ============================================================================

namespace {
    std::unique_ptr<FlashAttentionEngine> g_flashEngine;
    std::atomic<bool> g_flashInitialized{false};
    std::atomic<bool> g_flashAvailable{false};
    
    // Thread-safe initialization
    bool InitializeFlashAttentionOnce() {
        if (g_flashInitialized.load()) {
            return g_flashAvailable.load();
        }
        
        // Attempt initialization
        g_flashEngine = std::make_unique<FlashAttentionEngine>();
        bool success = g_flashEngine->Initialize();
        
        g_flashAvailable.store(success);
        g_flashInitialized.store(true);
        
        if (success) {
            std::cout << "[FlashAttention] AVX-512 kernel initialized successfully.\n";
        } else {
            std::cout << "[FlashAttention] AVX-512 not available, using standard attention.\n";
        }
        
        return success;
    }
}

// ============================================================================
// FlashAttention Bridge — Dispatch to AVX-512 kernel or fallback
// ============================================================================

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
 */
bool DispatchFlashAttention(
    float* Q,
    float* K,
    float* V,
    float* O,
    int32_t seqLenM,
    int32_t seqLenN,
    int32_t headDim,
    int32_t numHeads,
    int32_t numKVHeads,
    int32_t batchSize,
    bool causal = true)
{
    // Initialize FlashAttention engine (once)
    if (!InitializeFlashAttentionOnce()) {
        return false; // Fall back to standard attention
    }
    
    // Check if FlashAttention is ready
    if (!g_flashEngine || !g_flashEngine->IsReady()) {
        return false; // Fall back to standard attention
    }
    
    // Validate alignment (ZMM requires 64-byte alignment)
    if (!FlashAttentionEngine::ValidateAlignment(Q, K, V, O)) {
        std::cerr << "[FlashAttention] WARNING: Pointers not 64-byte aligned, falling back to standard attention.\n";
        return false;
    }
    
    // Configure FlashAttention
    FlashAttentionConfig cfg;
    cfg.Q = Q;
    cfg.K = K;
    cfg.V = V;
    cfg.O = O;
    cfg.seqLenM = seqLenM;
    cfg.seqLenN = seqLenN;
    cfg.headDim = headDim;
    cfg.numHeads = numHeads;
    cfg.numKVHeads = numKVHeads;
    cfg.batchSize = batchSize;
    cfg.causal = causal ? 1 : 0;
    cfg.ComputeScale();
    
    // Dispatch to AVX-512 kernel
    int32_t result = g_flashEngine->Forward(cfg);
    
    if (result != 0) {
        std::cerr << "[FlashAttention] Forward pass failed with code " << result << ", falling back to standard attention.\n";
        return false;
    }
    
    return true; // FlashAttention succeeded
}

/**
 * @brief Check if FlashAttention is available (licensed + AVX-512 capable)
 * 
 * @return true if FlashAttention can be used, false otherwise
 */
bool IsFlashAttentionAvailable() {
    return InitializeFlashAttentionOnce();
}

/**
 * @brief Get FlashAttention performance counters
 * 
 * @param outCalls Output: total forward calls since initialization
 * @param outTiles Output: total tiles processed since initialization
 */
void GetFlashAttentionCounters(uint64_t& outCalls, uint64_t& outTiles) {
    if (g_flashEngine) {
        outCalls = g_flashEngine->GetCallCount();
        outTiles = g_flashEngine->GetTileCount();
    } else {
        outCalls = 0;
        outTiles = 0;
    }
}

/**
 * @brief Get FlashAttention status string for diagnostics
 * 
 * @return Human-readable status string
 */
std::string GetFlashAttentionStatus() {
    if (g_flashEngine) {
        return g_flashEngine->GetStatusString();
    }
    return "FlashAttention not initialized";
}

} // namespace RawrXD

// ============================================================================
// C Interface for Integration with Existing Code
// ============================================================================

extern "C" {

/**
 * @brief C interface for FlashAttention dispatch
 * 
 * This provides a C-compatible interface for integration with existing
 * C/C++ code that doesn't use C++ classes.
 */
__declspec(dllexport) bool RawrXD_DispatchFlashAttention(
    float* Q,
    float* K,
    float* V,
    float* O,
    int32_t seqLenM,
    int32_t seqLenN,
    int32_t headDim,
    int32_t numHeads,
    int32_t numKVHeads,
    int32_t batchSize,
    int32_t causal)
{
    return RawrXD::DispatchFlashAttention(
        Q, K, V, O,
        seqLenM, seqLenN, headDim,
        numHeads, numKVHeads, batchSize,
        causal != 0
    );
}

/**
 * @brief C interface to check FlashAttention availability
 */
__declspec(dllexport) bool RawrXD_IsFlashAttentionAvailable() {
    return RawrXD::IsFlashAttentionAvailable();
}

/**
 * @brief C interface to get FlashAttention counters
 */
__declspec(dllexport) void RawrXD_GetFlashAttentionCounters(uint64_t* outCalls, uint64_t* outTiles) {
    if (outCalls && outTiles) {
        RawrXD::GetFlashAttentionCounters(*outCalls, *outTiles);
    }
}

} // extern "C"