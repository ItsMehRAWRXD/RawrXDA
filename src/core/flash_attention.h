// ============================================================================
// flash_attention.h — C++ Bridge to FlashAttention_AVX512.asm
// ============================================================================
// Declares extern "C" ASM exports, the FlashAttentionConfig struct (which
// must stay layout-compatible with the ASM offset constants), and a C++
// wrapper class that integrates with EnterpriseLicense gating.
//
// ASM Source:  src/asm/FlashAttention_AVX512.asm
// License:     Gated behind FEATURE_FLASH_ATTENTION (0x40) — Pro tier
//
// Usage:
//   FlashAttentionEngine engine;
//   if (!engine.Initialize()) { /* no AVX-512 or no license */ }
//   engine.Forward(cfg);
// ============================================================================

#pragma once

#include <cstdint>
#include <cmath>
#include <string>

namespace RawrXD {

// ============================================================================
// FlashAttentionConfig — passed to ASM kernel
// ============================================================================
// MUST match the struct layout expected by FlashAttention_AVX512.asm:
//   CFG_Q         = 0    (8 bytes)
//   CFG_K         = 8    (8 bytes)
//   CFG_V         = 16   (8 bytes)
//   CFG_O         = 24   (8 bytes)
//   CFG_SEQ_M     = 32   (4 bytes)
//   CFG_SEQ_N     = 36   (4 bytes)
//   CFG_HEAD_DIM  = 40   (4 bytes)
//   CFG_NUM_HEADS = 44   (4 bytes)
//   CFG_NUM_KV    = 48   (4 bytes)
//   CFG_BATCH     = 52   (4 bytes)
//   CFG_SCALE     = 56   (4 bytes)
//   CFG_CAUSAL    = 60   (4 bytes)
// Total: 64 bytes
//
#pragma pack(push, 1)
struct FlashAttentionConfig {
    float*   Q;             // [batch * heads * seqM * headDim]  fp32 row-major
    float*   K;             // [batch * kvHeads * seqN * headDim] fp32 row-major
    float*   V;             // [batch * kvHeads * seqN * headDim] fp32 row-major
    float*   O;             // [batch * heads * seqM * headDim]  fp32 output
    int32_t  seqLenM;       // Query sequence length (M)
    int32_t  seqLenN;       // Key/Value sequence length (N)
    int32_t  headDim;       // Head dimension (D), typically 64/128
    int32_t  numHeads;      // Number of query heads
    int32_t  numKVHeads;    // Number of KV heads (GQA: numKVHeads <= numHeads)
    int32_t  batchSize;     // Batch size
    float    scale;         // 1.0f / sqrt(headDim), precomputed
    int32_t  causal;        // 1 = autoregressive causal mask, 0 = full attention

    /// Helper: fill scale from headDim
    void ComputeScale() {
        scale = 1.0f / std::sqrt(static_cast<float>(headDim));
    }

    /// Helper: set GQA heads equal (standard MHA)
    void SetMHA(int32_t heads) {
        numHeads   = heads;
        numKVHeads = heads;
    }

    /// Helper: configure for typical LLaMA-style GQA
    void SetGQA(int32_t qHeads, int32_t kvHeads) {
        numHeads   = qHeads;
        numKVHeads = kvHeads;
    }
};
#pragma pack(pop)

static_assert(sizeof(FlashAttentionConfig) == 64,
    "FlashAttentionConfig must be exactly 64 bytes to match ASM layout");

// ============================================================================
// Tile configuration (returned by FlashAttention_GetTileConfig)
// ============================================================================
struct FlashAttentionTileConfig {
    int32_t tileM;          // FLASH_TILE_M (default 64)
    int32_t tileN;          // FLASH_TILE_N (default 64)
    int32_t headDim;        // FLASH_HEAD_DIM (default 128)
    int32_t scratchBytes;   // Stack scratch requirement
};

// ============================================================================
// ASM Extern Declarations
// ============================================================================
extern "C" {
    /// Check if CPU supports AVX-512F + AVX-512BW + AVX-512VL + OS XSAVE.
    /// Returns: 1 if capable, 0 if not.
    int32_t FlashAttention_CheckAVX512();

    /// Initialize the Flash Attention subsystem (sets internal AVX-512 flag).
    /// Must be called before FlashAttention_Forward.
    /// Returns: 1 if ready, 0 if AVX-512 not available.
    int32_t FlashAttention_Init();

    /// Execute the tiled Flash-Attention v2 forward pass.
    /// @param cfg  Pointer to FlashAttentionConfig (64 bytes, must be filled).
    /// Returns: 0 on success, -1 if AVX-512 not available.
    int32_t FlashAttention_Forward(FlashAttentionConfig* cfg);

    /// Get tile configuration for diagnostics.
    /// @param out  Pointer to FlashAttentionTileConfig (16 bytes).
    /// Returns: 1.
    int32_t FlashAttention_GetTileConfig(FlashAttentionTileConfig* out);

    /// Performance counters (set by ASM)
    extern uint64_t g_FlashAttnCalls;
    extern uint64_t g_FlashAttnTiles;
}

// ============================================================================
// FlashAttentionEngine — C++ wrapper with license gating
// ============================================================================
// Integrates with EnterpriseLicense to enforce FEATURE_FLASH_ATTENTION (0x40).
// Also handles:
//   - CPUID capability check
//   - Memory alignment verification
//   - Diagnostic reporting
//
class FlashAttentionEngine {
public:
    FlashAttentionEngine()  = default;
    ~FlashAttentionEngine() = default;

    // Non-copyable
    FlashAttentionEngine(const FlashAttentionEngine&) = delete;
    FlashAttentionEngine& operator=(const FlashAttentionEngine&) = delete;

    /// Initialize: check license + AVX-512 capability.
    /// Returns true if the kernel is ready to run.
    bool Initialize();

    /// Execute Flash Attention forward pass.
    /// @param cfg  Fully populated config struct (Q/K/V/O must be aligned to 64 bytes).
    /// Returns 0 on success, negative on error.
    int32_t Forward(FlashAttentionConfig& cfg);

    /// Check if the engine is ready (licensed + AVX-512 capable).
    bool IsReady() const { return m_ready; }

    /// Check if AVX-512 is available (regardless of license).
    bool HasAVX512() const { return m_hasAVX512; }

    /// Check if the license permits Flash Attention.
    bool IsLicensed() const { return m_licensed; }

    /// Get tile configuration from ASM.
    FlashAttentionTileConfig GetTileConfig() const;

    /// Get total forward calls since initialization.
    uint64_t GetCallCount() const { return g_FlashAttnCalls; }

    /// Get total tiles processed since initialization.
    uint64_t GetTileCount() const { return g_FlashAttnTiles; }

    /// Get human-readable status string for diagnostics.
    std::string GetStatusString() const;

    /// Verify that all pointers in a config are 64-byte aligned.
    static bool ValidateAlignment(const FlashAttentionConfig& cfg);

private:
    bool m_ready     = false;
    bool m_hasAVX512 = false;
    bool m_licensed  = false;
};

} // namespace RawrXD
