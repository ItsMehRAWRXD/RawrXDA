// ============================================================================
// kimi_loader.cpp — Kimi 2.5/2.6 Hardware Auto-Tuning Loader
// ============================================================================
#include "kimi_loader.h"
#include "prometheus_weight_loader.h"

#include <algorithm>
#include <iostream>
#include <cmath>

namespace Prometheus {

// =============================================================================
// MODEL INFO LOOKUP
// =============================================================================

KimiModelInfo KimiLoader::getModelInfo(KimiVariant variant) {
    switch (variant) {
        case KimiVariant::Kimi_2_5_7B:  return KimiModels::Kimi_2_5_7B();
        case KimiVariant::Kimi_2_5_14B: return KimiModels::Kimi_2_5_14B();
        case KimiVariant::Kimi_2_5_32B: return KimiModels::Kimi_2_5_32B();
        case KimiVariant::Kimi_2_5_72B: return KimiModels::Kimi_2_5_72B();
        case KimiVariant::Kimi_2_6_MoE: return KimiModels::Kimi_2_6_MoE();
        default:                        return KimiModels::Kimi_2_5_32B();
    }
}

uint64_t KimiLoader::getModelSizeMB(const KimiModelInfo& info, uint32_t weightBits) {
    switch (weightBits) {
        case 2: return info.sizeMB_Q2;
        case 3: return info.sizeMB_Q3;
        case 4: return info.sizeMB_Q4;
        default: return info.sizeMB_Q4;
    }
}

// =============================================================================
// AUTO-CONFIGURE
// =============================================================================

KimiLoadConfig KimiLoader::autoConfigure(const HardwareProfile& hw, KimiVariant target) {
    KimiLoadConfig cfg{};
    cfg.contextSize = 8192;
    cfg.useFlashAttention = true;
    cfg.useKVCache = true;
    cfg.kvCacheBits = 4;
    cfg.speculativeDecoding = false;

    // Select model variant
    if (target != KimiVariant::Unknown) {
        cfg.targetVariant = target;
    } else {
        if (hw.totalRAM_GB >= 64 && hw.totalVRAM_GB >= 16) {
            cfg.targetVariant = KimiVariant::Kimi_2_5_32B;
            cfg.weightBits = 4;
        } else if (hw.totalRAM_GB >= 48) {
            cfg.targetVariant = KimiVariant::Kimi_2_5_32B;
            cfg.weightBits = 4;
        } else if (hw.totalRAM_GB >= 32) {
            cfg.targetVariant = KimiVariant::Kimi_2_5_14B;
            cfg.weightBits = 4;
        } else {
            cfg.targetVariant = KimiVariant::Kimi_2_5_7B;
            cfg.weightBits = 4;
        }
    }

    KimiModelInfo info = getModelInfo(cfg.targetVariant);

    // Downgrade quantization if model doesn't fit
    uint64_t modelSizeMB = getModelSizeMB(info, cfg.weightBits);
    uint64_t ramMB = hw.totalRAM_GB * 1024;
    if (modelSizeMB > ramMB && cfg.weightBits > 2) {
        cfg.weightBits = 3;
        modelSizeMB = getModelSizeMB(info, cfg.weightBits);
        if (modelSizeMB > ramMB && cfg.weightBits > 2) {
            cfg.weightBits = 2;
        }
    }

    // GPU layer offloading
    uint64_t vramMB = hw.totalVRAM_GB * 1024;
    uint64_t kvSizeMB = (cfg.contextSize * info.numLayers * info.numKVHeads * 128 * cfg.kvCacheBits) / (8 * 1024 * 1024);
    uint64_t activationMB = (info.hiddenDim * info.numLayers * 4) / (1024 * 1024);
    uint64_t gpuBudgetMB = vramMB > (kvSizeMB + activationMB) ? vramMB - kvSizeMB - activationMB : 0;

    uint64_t bytesPerLayerMB = modelSizeMB / info.numLayers;
    if (bytesPerLayerMB > 0 && gpuBudgetMB > 0) {
        cfg.gpuLayers = static_cast<uint32_t>((std::min)(gpuBudgetMB / bytesPerLayerMB, static_cast<uint64_t>(info.numLayers)));
    }

    // TPS estimate
    cfg.estimatedTPS = estimateTPS(info, cfg, hw);

    // Memory estimates
    cfg.estimatedVRAMUsage = (cfg.gpuLayers * bytesPerLayerMB + kvSizeMB + activationMB) * 1024 * 1024;
    cfg.estimatedRAMUsage = (modelSizeMB - (cfg.gpuLayers * bytesPerLayerMB) + kvSizeMB + activationMB) * 1024 * 1024;

    return cfg;
}

// =============================================================================
// TPS ESTIMATION
// =============================================================================

int KimiLoader::estimateTPS(const KimiModelInfo& info, const KimiLoadConfig& cfg, const HardwareProfile& hw) {
    int baseTPS = 0;
    switch (cfg.weightBits) {
        case 2: baseTPS = info.estimatedTPS_Q2; break;
        case 3: baseTPS = info.estimatedTPS_Q3; break;
        case 4: baseTPS = info.estimatedTPS_Q4; break;
        default: baseTPS = info.estimatedTPS_Q4; break;
    }
    if (baseTPS <= 0) baseTPS = 10;

    float cacheBonus = 1.0f;
    if (hw.cpuCacheL3_MB >= 96) cacheBonus = 1.30f;
    else if (hw.cpuCacheL3_MB >= 64) cacheBonus = 1.15f;

    float coreBonus = (hw.cpuCores >= 16) ? 1.20f : (hw.cpuCores >= 8) ? 1.00f : 0.80f;
    float avx512Bonus = hw.hasAVX512 ? 1.25f : 1.0f;

    float gpuBonus = 1.0f;
    if (cfg.gpuLayers > 0 && info.numLayers > 0) {
        float offloadRatio = static_cast<float>(cfg.gpuLayers) / static_cast<float>(info.numLayers);
        gpuBonus = 1.0f + (offloadRatio * 0.5f);
    }

    float ramBonus = 1.10f; // DDR5 5600 baseline

    int finalTPS = static_cast<int>(baseTPS * cacheBonus * coreBonus * avx512Bonus * gpuBonus * ramBonus);
    return (std::max)(1, finalTPS);
}

// =============================================================================
// MODEL CONFIG CREATION
// =============================================================================

ModelConfig KimiLoader::createModelConfig(const KimiModelInfo& info, const KimiLoadConfig& cfg) {
    ModelConfig mc{};
    mc.vocabSize = info.vocabSize;
    mc.hiddenDim = info.hiddenDim;
    mc.numLayers = info.numLayers;
    mc.numHeads = info.numHeads;
    mc.numKVHeads = info.numKVHeads;
    mc.headDim = info.hiddenDim / info.numHeads;
    mc.intermediateDim = info.intermediateDim;
    mc.maxPosition = cfg.contextSize;
    mc.weightBits = cfg.weightBits;
    mc.kvCacheBits = cfg.kvCacheBits;
    mc.useFlashAttention = cfg.useFlashAttention;
    mc.enableSpeculativeDecoding = cfg.speculativeDecoding;

    if (info.isMoE) {
        mc.numExperts = info.numExperts;
        mc.expertsPerToken = info.expertsPerToken;
        mc.sharedExperts = info.sharedExperts;
    }

    mc.ropeTheta = 10000000.0f;
    mc.ropeScaleFactor = 2.0f;

    return mc;
}

// =============================================================================
// LOAD MODEL
// =============================================================================

WeightLoadResult KimiLoader::loadModel(const std::string& modelPath, const HardwareProfile& hw, KimiVariant targetVariant) {
    KimiLoadConfig cfg = autoConfigure(hw, targetVariant);
    KimiModelInfo info = getModelInfo(cfg.targetVariant);

    std::cout << "═══════════════════════════════════════════════════════════\n";
    std::cout << " Kimi Loader - Hardware Auto-Tuning\n";
    std::cout << "─────────────────────────────────────────────────────────────\n";
    std::cout << " Hardware: " << hw.summary() << "\n";
    std::cout << "─────────────────────────────────────────────────────────────\n";
    std::cout << " Model:           " << info.name << "\n";
    std::cout << " Parameters:      " << info.paramsBillion << "B\n";
    std::cout << " Quantization:    Q" << cfg.weightBits << "_K (auto-selected)\n";
    std::cout << "─────────────────────────────────────────────────────────────\n";
    std::cout << " GPU Layers:      " << cfg.gpuLayers << "/" << info.numLayers << "\n";
    std::cout << " Context:         " << cfg.contextSize << " tokens\n";
    std::cout << " KV Cache:        " << cfg.kvCacheBits << "-bit\n";
    std::cout << "─────────────────────────────────────────────────────────────\n";
    std::cout << " VRAM Usage:      " << (cfg.estimatedVRAMUsage / (1024.0 * 1024.0)) << " MB\n";
    std::cout << " RAM Usage:       " << (cfg.estimatedRAMUsage / (1024.0 * 1024.0)) << " MB\n";
    std::cout << "─────────────────────────────────────────────────────────────\n";
    std::cout << " Estimated TPS:   " << cfg.estimatedTPS << " tokens/sec\n";
    std::cout << "─────────────────────────────────────────────────────────────\n";
    std::cout << " Status:          ";

    uint64_t ramBytes = hw.totalRAM_GB * 1024ull * 1024 * 1024;
    if (cfg.estimatedRAMUsage > ramBytes) {
        std::cout << "WARNING: RAM exceeded (will use swap)\n";
    } else {
        std::cout << "OK - Fits in RAM\n";
    }
    std::cout << "═══════════════════════════════════════════════════════════\n\n";

    // Load via Prometheus weight loader
    std::vector<TensorDesc> tensors;
    ModelConfig modelCfg = createModelConfig(info, cfg);
    WeightLoadResult result = GGUFLoader::load(modelPath, tensors, &modelCfg);

    if (result.success) {
        std::cout << "✅ Model loaded successfully!\n";
        std::cout << "Ready to generate at ~" << cfg.estimatedTPS << " TPS\n\n";
    } else {
        std::cerr << "❌ Failed to load model: " << result.error << "\n";
    }

    return result;
}

} // namespace Prometheus
