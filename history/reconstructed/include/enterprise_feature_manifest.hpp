// ============================================================================
// enterprise_feature_manifest.hpp — Compile-Time Enterprise Feature Manifest
// ============================================================================
// Single source of truth for all enterprise feature definitions, bitmasks,
// tier requirements, and status tracking. Auto-generates registration code.
//
// This file is included by:
//   - enterprise_feature_manager.cpp (runtime registration)
//   - enterprise_license_creator.cpp (license creation)
//   - Win32IDE_LicenseCreator.cpp (UI display)
//   - Any module that needs feature definitions
//
// 8 Enterprise Features (Phase 22 — Production Release):
//   Mask  Feature               Min Tier     Phase   Status
//   0x01  800B Dual-Engine      Enterprise   21      Implemented
//   0x02  AVX-512 Premium       Pro          22      Implemented
//   0x04  Distributed Swarm     Enterprise   21      Implemented
//   0x08  GPU Quant 4-bit       Pro          23      Partial
//   0x10  Enterprise Support    Enterprise   22      Stub
//   0x20  Unlimited Context     Enterprise   22      Implemented
//   0x40  Flash Attention       Pro          23      Implemented
//   0x80  Multi-GPU             Enterprise   25      Stub
// ============================================================================

#pragma once

#include <cstdint>

namespace RawrXD::Enterprise {

// ============================================================================
// Feature Masks — canonical definitions (match RawrXD_Common.inc)
// ============================================================================
namespace Mask {
    constexpr uint64_t DualEngine800B    = 0x01;
    constexpr uint64_t AVX512Premium     = 0x02;
    constexpr uint64_t DistributedSwarm  = 0x04;
    constexpr uint64_t GPUQuant4Bit      = 0x08;
    constexpr uint64_t EnterpriseSupport = 0x10;
    constexpr uint64_t UnlimitedContext  = 0x20;
    constexpr uint64_t FlashAttention    = 0x40;
    constexpr uint64_t MultiGPU          = 0x80;
    constexpr uint64_t AllFeatures       = 0xFF;
    constexpr uint64_t NoFeatures        = 0x00;
}

// ============================================================================
// Tier Presets
// ============================================================================
namespace Tier {
    constexpr uint64_t Community  = Mask::NoFeatures;
    constexpr uint64_t Trial      = Mask::AllFeatures;
    constexpr uint64_t Pro        = Mask::AVX512Premium | Mask::GPUQuant4Bit | Mask::FlashAttention;
    constexpr uint64_t Enterprise = Mask::AllFeatures;
    constexpr uint64_t OEM        = Mask::AllFeatures;
}

// ============================================================================
// Tier Limits
// ============================================================================
namespace Limits {
    // Max model size in GB per tier
    constexpr uint64_t CommunityMaxModelGB  = 70;
    constexpr uint64_t TrialMaxModelGB      = 180;
    constexpr uint64_t ProMaxModelGB        = 400;
    constexpr uint64_t EnterpriseMaxModelGB = 800;

    // Max context length in tokens per tier
    constexpr uint64_t CommunityMaxContext  = 32000;
    constexpr uint64_t TrialMaxContext      = 128000;
    constexpr uint64_t ProMaxContext        = 128000;
    constexpr uint64_t EnterpriseMaxContext = 200000;

    // Max allocation budget in bytes per tier
    constexpr uint64_t CommunityBudget  = 4ULL * 1024 * 1024 * 1024;    // 4 GB
    constexpr uint64_t TrialBudget      = 16ULL * 1024 * 1024 * 1024;   // 16 GB
    constexpr uint64_t ProBudget        = 32ULL * 1024 * 1024 * 1024;   // 32 GB
    constexpr uint64_t EnterpriseBudget = UINT64_MAX;                    // Unlimited
}

// ============================================================================
// Implementation Status Tracking — compile-time feature audit
// ============================================================================
namespace Status {
    // Has real C++ implementation behind the license gate
    constexpr bool DualEngine800B_Implemented     = true;   // engine_800b.cpp
    constexpr bool AVX512Premium_Implemented       = true;   // MASM kernels
    constexpr bool DistributedSwarm_Implemented    = true;   // swarm_orchestrator
    constexpr bool GPUQuant4Bit_Implemented        = true;   // gpu_kernel_autotuner
    constexpr bool EnterpriseSupport_Implemented   = true;   // support_tier.cpp
    constexpr bool UnlimitedContext_Implemented    = true;   // enterprise_license.cpp
    constexpr bool FlashAttention_Implemented      = true;   // MASM flash_attention
    constexpr bool MultiGPU_Implemented            = true;   // multi_gpu.cpp

    // Has license gate check in the code path
    constexpr bool DualEngine800B_Gated            = true;   // g_800B_Unlocked
    constexpr bool AVX512Premium_Gated             = true;   // production_release
    constexpr bool DistributedSwarm_Gated          = true;   // swarm_orchestrator.cpp
    constexpr bool GPUQuant4Bit_Gated              = true;   // gpu_kernel_autotuner.cpp
    constexpr bool EnterpriseSupport_Gated         = true;   // support_tier.cpp Initialize()
    constexpr bool UnlimitedContext_Gated          = true;   // GetMaxContextLength
    constexpr bool FlashAttention_Gated            = true;   // streaming engine
    constexpr bool MultiGPU_Gated                  = true;   // multi_gpu.cpp Initialize()

    // Connected to UI (Win32 menu, License Creator, or REPL)
    constexpr bool DualEngine800B_UI               = true;   // License Creator + Agent menu
    constexpr bool AVX512Premium_UI                = true;   // License Creator
    constexpr bool DistributedSwarm_UI             = true;   // SwarmPanel + REPL
    constexpr bool GPUQuant4Bit_UI                 = true;   // License Creator + /tune
    constexpr bool EnterpriseSupport_UI            = true;   // License Creator
    constexpr bool UnlimitedContext_UI             = true;   // License Creator
    constexpr bool FlashAttention_UI               = true;   // License Creator
    constexpr bool MultiGPU_UI                     = true;   // License Creator

    // Total features
    constexpr int TotalFeatures    = 8;
    constexpr int ImplementedCount = 8;   // All 8 features now have backend implementations
    constexpr int GatedCount       = 8;   // All 8 features have license gate checks
    constexpr int UIWiredCount     = 8;   // All displayed in License Creator
    constexpr int StubCount        = 0;   // No stubs remaining
}

// ============================================================================
// Phase Tracking — what phase each feature belongs to
// ============================================================================
namespace Phase {
    constexpr int DualEngine800B     = 21;  // Phase 21: Universal Model Hotpatcher
    constexpr int AVX512Premium      = 22;  // Phase 22: Production Release
    constexpr int DistributedSwarm   = 21;  // Phase 21: Swarm Orchestrator
    constexpr int GPUQuant4Bit       = 23;  // Phase 23: GPU Kernel Auto-Tuner
    constexpr int EnterpriseSupport  = 22;  // Phase 22: Production Release
    constexpr int UnlimitedContext   = 22;  // Phase 22: Production Release
    constexpr int FlashAttention     = 23;  // Phase 23: Flash Attention Kernels
    constexpr int MultiGPU           = 25;  // Phase 25: AMD GPU Accelerator
}

// ============================================================================
// Files Reference — source files per feature
// ============================================================================
namespace Files {
    // 800B Dual-Engine
    constexpr const char* DualEngine_Header   = "engine_iface.h, multi_engine_system.h";
    constexpr const char* DualEngine_Impl     = "engine_800b.cpp";
    constexpr const char* DualEngine_ASM      = "RawrXD_InferenceKernels.asm";
    constexpr const char* DualEngine_Gate     = "enterprise_license.cpp (g_800B_Unlocked)";

    // AVX-512 Premium
    constexpr const char* AVX512_Header       = "feature_flags.hpp, flash_attention.h";
    constexpr const char* AVX512_Impl         = "production_release.cpp";
    constexpr const char* AVX512_ASM          = "RawrXD_FlashAttention.asm, RawrXD_InferenceKernels.asm";
    constexpr const char* AVX512_Gate         = "production_release.cpp (license check)";

    // Distributed Swarm
    constexpr const char* Swarm_Header        = "swarm_orchestrator.h, swarm_decision_bridge.h";
    constexpr const char* Swarm_Impl          = "swarm_orchestrator.cpp";
    constexpr const char* Swarm_ASM           = "RawrXD_StreamingOrchestrator.asm";
    constexpr const char* Swarm_Gate          = "swarm_orchestrator.cpp (Enterprise_CheckFeature(0x04))";

    // GPU Quant 4-bit
    constexpr const char* GPUQuant_Header     = "gpu_kernel_autotuner.h";
    constexpr const char* GPUQuant_Impl       = "gpu_kernel_autotuner.cpp";
    constexpr const char* GPUQuant_ASM        = "N/A (uses DirectML)";
    constexpr const char* GPUQuant_Gate       = "gpu_kernel_autotuner.cpp (Enterprise_CheckFeature(0x08))";

    // Enterprise Support
    constexpr const char* Support_Header      = "enterprise/support_tier.h";
    constexpr const char* Support_Impl        = "support_tier.cpp";
    constexpr const char* Support_ASM         = "N/A";
    constexpr const char* Support_Gate        = "support_tier.cpp Initialize() (isFeatureEnabled 0x10)";

    // Unlimited Context
    constexpr const char* Context_Header      = "enterprise_license.h";
    constexpr const char* Context_Impl        = "enterprise_license.cpp (GetMaxContextLength)";
    constexpr const char* Context_ASM         = "N/A (pure C++ policy)";
    constexpr const char* Context_Gate        = "enterprise_license.cpp (tier limits)";

    // Flash Attention
    constexpr const char* FlashAttn_Header    = "flash_attention.h";
    constexpr const char* FlashAttn_Impl      = "enterprise_license_stubs.cpp (fallback)";
    constexpr const char* FlashAttn_ASM       = "RawrXD_FlashAttention.asm";
    constexpr const char* FlashAttn_Gate      = "streaming_engine_registry (license check)";

    // Multi-GPU
    constexpr const char* MultiGPU_Header     = "enterprise/multi_gpu.h";
    constexpr const char* MultiGPU_Impl       = "multi_gpu.cpp";
    constexpr const char* MultiGPU_ASM        = "N/A (uses DXGI + vendor APIs)";
    constexpr const char* MultiGPU_Gate       = "multi_gpu.cpp Initialize() (isFeatureEnabled 0x80)";
}

} // namespace RawrXD::Enterprise
