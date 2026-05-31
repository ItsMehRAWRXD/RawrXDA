#pragma once
#include "prometheus_config.h"
#include "prometheus_engine.h"
#include "prometheus_weight_loader.h"

#include <windows.h>
#include <dxgi.h>
#include <chrono>
#include <iostream>
#include <cmath>
#include <string>
#include <intrin.h>

namespace Prometheus {

// =============================================================================
// KIMI MODEL VARIANTS
// =============================================================================

enum class KimiVariant {
    Unknown,
    Kimi_2_5_7B,
    Kimi_2_5_14B,
    Kimi_2_5_32B,
    Kimi_2_5_72B,
    Kimi_2_6_MoE,
};

struct KimiModelInfo {
    KimiVariant variant = KimiVariant::Unknown;
    const char* name = "";
    uint32_t hiddenDim = 0;
    uint32_t intermediateDim = 0;
    uint32_t numLayers = 0;
    uint32_t numHeads = 0;
    uint32_t numKVHeads = 0;
    uint32_t vocabSize = 0;
    bool isMoE = false;
    uint32_t numExperts = 0;
    uint32_t expertsPerToken = 0;
    uint32_t sharedExperts = 0;
    uint64_t paramsBillion = 0;
    uint64_t sizeMB_Q4 = 0;
    uint64_t sizeMB_Q3 = 0;
    uint64_t sizeMB_Q2 = 0;
    int estimatedTPS_Q4 = 0;
    int estimatedTPS_Q3 = 0;
    int estimatedTPS_Q2 = 0;
};

// =============================================================================
// PRE-CONFIGURED KIMI MODELS
// =============================================================================

namespace KimiModels {

inline KimiModelInfo Kimi_2_5_7B() {
    return {
        KimiVariant::Kimi_2_5_7B, "kimi-2.5-7b",
        4096, 11008, 32, 32, 8, 152000,
        false, 0, 0, 0,
        7, 4096, 3072, 2048,
        45, 50, 55
    };
}

inline KimiModelInfo Kimi_2_5_14B() {
    return {
        KimiVariant::Kimi_2_5_14B, "kimi-2.5-14b",
        5120, 13824, 40, 40, 10, 152000,
        false, 0, 0, 0,
        14, 8192, 6144, 4096,
        25, 30, 35
    };
}

inline KimiModelInfo Kimi_2_5_32B() {
    return {
        KimiVariant::Kimi_2_5_32B, "kimi-2.5-32b",
        6656, 17920, 56, 52, 8, 152000,
        false, 0, 0, 0,
        32, 18432, 14336, 9216,
        15, 18, 22
    };
}

inline KimiModelInfo Kimi_2_5_72B() {
    return {
        KimiVariant::Kimi_2_5_72B, "kimi-2.5-72b",
        8192, 29568, 80, 64, 8, 152000,
        false, 0, 0, 0,
        72, 40960, 30720, 20480,
        0, 12, 15
    };
}

inline KimiModelInfo Kimi_2_6_MoE() {
    return {
        KimiVariant::Kimi_2_6_MoE, "kimi-2.6-moe",
        6144, 16384, 48, 48, 6, 152000,
        true, 64, 4, 2,
        400, 49152, 36864, 24576,
        8, 12, 15
    };
}

} // namespace KimiModels

// =============================================================================
// HARDWARE PROFILE
// =============================================================================

struct HardwareProfile {
    uint64_t totalRAM_GB = 0;
    uint64_t totalVRAM_GB = 0;
    bool hasAVX512 = false;
    bool hasAMX = false;
    int cpuCores = 0;
    int cpuCacheL3_MB = 0;

    static HardwareProfile detect() {
        HardwareProfile profile{};

        MEMORYSTATUSEX memInfo{};
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        if (GlobalMemoryStatusEx(&memInfo)) {
            profile.totalRAM_GB = memInfo.ullTotalPhys / (1024ull * 1024 * 1024);
        }

#ifdef _WIN32
        IDXGIFactory* pFactory = nullptr;
        if (SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&pFactory)))) {
            IDXGIAdapter* pAdapter = nullptr;
            for (UINT i = 0; pFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
                DXGI_ADAPTER_DESC desc{};
                pAdapter->GetDesc(&desc);
                profile.totalVRAM_GB += desc.DedicatedVideoMemory / (1024ull * 1024 * 1024);
                pAdapter->Release();
            }
            pFactory->Release();
        }
#endif

        int cpuInfo[4] = {};
        __cpuid(cpuInfo, 1);
        profile.hasAVX512 = (cpuInfo[2] & (1 << 28)) != 0;

        __cpuid(cpuInfo, 7);
        profile.hasAMX = (cpuInfo[3] & (1 << 24)) != 0;

        SYSTEM_INFO sysInfo{};
        GetSystemInfo(&sysInfo);
        profile.cpuCores = static_cast<int>(sysInfo.dwNumberOfProcessors);

        profile.cpuCacheL3_MB = 96; // Default for 7800X3D
        return profile;
    }

    std::string summary() const {
        return std::to_string(totalRAM_GB) + "GB RAM + " +
               std::to_string(totalVRAM_GB) + "GB VRAM, " +
               std::to_string(cpuCores) + " cores, " +
               std::to_string(cpuCacheL3_MB) + "MB L3";
    }
};

// =============================================================================
// KIMI LOAD CONFIG
// =============================================================================

struct KimiLoadConfig {
    KimiVariant targetVariant = KimiVariant::Unknown;
    uint32_t weightBits = 4;      // 4 = Q4_K, 3 = Q3_K, 2 = Q2_K
    uint32_t gpuLayers = 0;
    uint32_t contextSize = 8192;
    bool useFlashAttention = true;
    bool useKVCache = true;
    uint32_t kvCacheBits = 4;
    bool speculativeDecoding = false;
    uint64_t estimatedVRAMUsage = 0;
    uint64_t estimatedRAMUsage = 0;
    int estimatedTPS = 0;
};

// =============================================================================
// KIMI LOADER
// =============================================================================

class KimiLoader {
public:
    static KimiLoadConfig autoConfigure(const HardwareProfile& hw, KimiVariant target = KimiVariant::Unknown);
    static WeightLoadResult loadModel(const std::string& modelPath, const HardwareProfile& hw, KimiVariant targetVariant = KimiVariant::Unknown);
    static ModelConfig createModelConfig(const KimiModelInfo& info, const KimiLoadConfig& cfg);

private:
    static KimiModelInfo getModelInfo(KimiVariant variant);
    static uint64_t getModelSizeMB(const KimiModelInfo& info, uint32_t weightBits);
    static int estimateTPS(const KimiModelInfo& info, const KimiLoadConfig& cfg, const HardwareProfile& hw);
};

} // namespace Prometheus
