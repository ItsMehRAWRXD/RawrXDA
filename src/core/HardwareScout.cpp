// ============================================================================
// HardwareScout.cpp — Hardware Interrogation Implementation
// ============================================================================
#include "HardwareScout.h"
#include "amd_gpu_accelerator.h"
#include <intrin.h>
#include <windows.h>
#include <iostream>

namespace RawrXD {
namespace Core {

HardwareProfile HardwareScout::GetCurrentProfile() {
    HardwareProfile profile;
    
    // 1. CPU Checks
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    profile.hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;
    
    int cpuInfo7[4];
    __cpuidex(cpuInfo7, 7, 0);
    profile.hasAVX512 = (cpuInfo7[1] & (1 << 16)) != 0;

    // 2. GPU Checks (Vulkan/AMD Native via amd_gpu_accelerator singleton)
    auto& accel = AMDGPUAccelerator::instance();
    if (!accel.isInitialized()) {
        accel.initialize(GPUBackend::Auto);
    }

    profile.hasVulkan = (accel.getActiveBackend() == GPUBackend::Vulkan);
    profile.gpuName = accel.getGPUName();
    profile.vramBytes = accel.getVRAMBytes();

    // Intel Arc/Battlemage Specific Detection
    bool isIntelArc = (profile.gpuName.find("Intel") != std::string::npos && 
                      (profile.gpuName.find("Arc") != std::string::npos || profile.gpuName.find("Battlemage") != std::string::npos));
    
    if (isIntelArc) {
        // Force Tier 0 if XMX (Intel's WMMA equivalent) is present
        if (accel.hasFeature(AMDFeatureFlag::XMX)) {
            profile.tier = ExecutionTier::VRAM_ACCELERATED;
        }
    }

    // 3. Tier Decision Matrix
    // Scenario A: 7800 XT / Intel Arc / High-end Discrete
    if (accel.isGPUEnabled() && profile.vramBytes > 4ULL * 1024 * 1024 * 1024) {
        profile.tier = ExecutionTier::VRAM_ACCELERATED;
    }
    // Scenario B: Modern CPU / No GPU
    else if (profile.hasAVX512) {
        profile.tier = ExecutionTier::CPU_AVX512;
    }
    // Scenario C: Balanced Modern
    else if (profile.hasAVX2) {
        profile.tier = ExecutionTier::CPU_AVX2;
    }
    // Scenario D: Compatibility
    else {
        profile.tier = ExecutionTier::CPU_GENERIC;
    }

    return profile;
}

const char* HardwareScout::TierToString(ExecutionTier tier) {
    switch (tier) {
        case ExecutionTier::VRAM_ACCELERATED: return "VRAM_ACCELERATED (High-End)";
        case ExecutionTier::CPU_AVX512:       return "CPU_AVX512 (Mid-Range)";
        case ExecutionTier::CPU_AVX2:         return "CPU_AVX2 (Standard)";
        case ExecutionTier::CPU_GENERIC:      return "CPU_GENERIC (Legacy)";
        default:                              return "UNKNOWN";
    }
}

} // namespace Core
} // namespace RawrXD
