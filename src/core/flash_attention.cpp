// ============================================================================
// flash_attention.cpp — FlashAttentionEngine implementation
// ============================================================================
// Wraps the AVX-512 ASM kernel with license gating, alignment checks,
// and diagnostic reporting.
//
// ASM Source:  src/asm/FlashAttention_AVX512.asm
// Header:     src/core/flash_attention.h
// License:    FEATURE_FLASH_ATTENTION (0x40) — Pro tier minimum
// ============================================================================

#include "flash_attention.h"
#include "enterprise_license.h"
#include <iostream>
#include <sstream>
#include <cstdint>

#include "logging/logger.h"
static Logger s_logger("flash_attention");

namespace RawrXD {

// ============================================================================
// FlashAttentionEngine::Initialize
// ============================================================================
bool FlashAttentionEngine::Initialize() {
    // Step 1: License gate — FEATURE_FLASH_ATTENTION (0x40)
    m_licensed = EnterpriseLicense::Instance().HasFeature(
        EnterpriseFeature::FlashAttention);

    if (!m_licensed) {
        s_logger.info("[FlashAttention] License check FAILED — ");
        m_ready = false;
        return false;
    }

    // Step 2: AVX-512 capability check (calls CPUID in ASM)
    int32_t avxResult = FlashAttention_Init();
    m_hasAVX512 = (avxResult == 1);

    if (!m_hasAVX512) {
        s_logger.info("[FlashAttention] AVX-512 check FAILED — ");
        m_ready = false;
        return false;
    }

    // Step 3: Report tile configuration
    FlashAttentionTileConfig tileCfg = GetTileConfig();
    s_logger.info("[FlashAttention] AVX-512 kernel initialized.\n");

    m_ready = true;
    return true;
}

// ============================================================================
// FlashAttentionEngine::Forward
// ============================================================================
int32_t FlashAttentionEngine::Forward(FlashAttentionConfig& cfg) {
    if (!m_ready) {
        s_logger.error( "[FlashAttention] Forward called but engine not ready. "
                  << "Call Initialize() first." << std::endl;
        return -2;
    }

    // Validate pointer alignment (ZMM requires 64-byte alignment)
    if (!ValidateAlignment(cfg)) {
        s_logger.error( "[FlashAttention] ERROR: Q/K/V/O pointers are not "
                  << "64-byte aligned. Use _aligned_malloc(size, 64)." << std::endl;
        return -3;
    }

    // Validate dimensions
    if (cfg.seqLenM <= 0 || cfg.seqLenN <= 0 || cfg.headDim <= 0 ||
        cfg.numHeads <= 0 || cfg.numKVHeads <= 0 || cfg.batchSize <= 0) {
        s_logger.error( "[FlashAttention] ERROR: Invalid dimensions in config."
                  << std::endl;
        return -4;
    }

    // Validate headDim is a multiple of 16 (ZMM register width in floats)
    if (cfg.headDim % 16 != 0) {
        s_logger.error( "[FlashAttention] ERROR: headDim (" << cfg.headDim
                  << ") must be a multiple of 16 for AVX-512." << std::endl;
        return -5;
    }

    // Validate GQA: numHeads must be divisible by numKVHeads
    if (cfg.numHeads % cfg.numKVHeads != 0) {
        s_logger.error( "[FlashAttention] ERROR: numHeads (" << cfg.numHeads
                  << ") must be divisible by numKVHeads (" << cfg.numKVHeads
                  << ") for GQA." << std::endl;
        return -6;
    }

    // Auto-compute scale if not set
    if (cfg.scale <= 0.0f || cfg.scale > 1.0f) {
        cfg.ComputeScale();
    }

    // Dispatch to ASM kernel
    return FlashAttention_Forward(&cfg);
}

// ============================================================================
// FlashAttentionEngine::GetTileConfig
// ============================================================================
FlashAttentionTileConfig FlashAttentionEngine::GetTileConfig() const {
    FlashAttentionTileConfig out = {};
    FlashAttention_GetTileConfig(&out);
    return out;
}

// ============================================================================
// FlashAttentionEngine::GetStatusString
// ============================================================================
std::string FlashAttentionEngine::GetStatusString() const {
    std::ostringstream ss;
    ss << "FlashAttention AVX-512 Engine Status:\n"
       << "  Ready:     " << (m_ready     ? "YES" : "NO") << "\n"
       << "  AVX-512:   " << (m_hasAVX512 ? "YES" : "NO") << "\n"
       << "  Licensed:  " << (m_licensed  ? "YES" : "NO") << "\n"
       << "  Calls:     " << g_FlashAttnCalls << "\n"
       << "  Tiles:     " << g_FlashAttnTiles;

    if (m_ready) {
        FlashAttentionTileConfig tc = {};
        FlashAttention_GetTileConfig(&tc);
        ss << "\n  TileM:     " << tc.tileM
           << "\n  TileN:     " << tc.tileN
           << "\n  HeadDim:   " << tc.headDim
           << "\n  Scratch:   " << tc.scratchBytes << " bytes";
    }

    return ss.str();
}

// ============================================================================
// FlashAttentionEngine::ValidateAlignment
// ============================================================================
bool FlashAttentionEngine::ValidateAlignment(const FlashAttentionConfig& cfg) {
    constexpr uintptr_t ALIGN_MASK = 63;  // 64-byte alignment check
    if (reinterpret_cast<uintptr_t>(cfg.Q) & ALIGN_MASK) return false;
    if (reinterpret_cast<uintptr_t>(cfg.K) & ALIGN_MASK) return false;
    if (reinterpret_cast<uintptr_t>(cfg.V) & ALIGN_MASK) return false;
    if (reinterpret_cast<uintptr_t>(cfg.O) & ALIGN_MASK) return false;
    return true;
}

} // namespace RawrXD
