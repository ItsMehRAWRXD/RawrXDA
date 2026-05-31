#pragma once
#include <cstdint>
#include <cstring>
#include <cmath>
#include <string>

namespace Prometheus {

// ============================================================================
// LowMemoryConfig — Optimized deployment for 64GB RAM + 16GB GPU
// ============================================================================

struct LowMemoryConfig {
    // Quantization
    enum class QuantLevel : uint8_t {
        INT8 = 8,
        INT4 = 4,
        INT3 = 3,
        INT2 = 2,
        Q4_K = 4,
        Q3_K = 3,
        Q2_K = 2,
    };

    QuantLevel modelQuant = QuantLevel::Q4_K;   // ~48GB for 120B
    QuantLevel kvQuant   = QuantLevel::INT4;      // 4-bit KV cache
    bool quantizeActivations = false;

    // Layer offloading
    uint32_t gpuLayers = 12;
    bool offloadDraftToGPU = true;
    bool offloadKVCacheToGPU = true;

    // Context management
    uint32_t maxContext = 65536;
    uint32_t slidingWindow = 8192;
    uint32_t kvCacheBudget = 16384;
    float kvEvictionRatio = 0.5f;

    // Compression
    bool enableKVCompression = true;
    float compressionRatio = 0.125f;
    bool useSparseAttention = true;
    uint32_t densePrefix = 512;
    uint32_t denseSuffix = 256;

    // Inference optimizations
    bool speculativeDecoding = true;
    uint32_t draftModelSize = 1;
    uint32_t speculativeTokens = 4;
    bool continuousBatching = false;

    // Batching
    uint32_t maxBatchSize = 1;
    uint32_t maxConcurrentRequests = 1;

    // Features
    bool enableVision = false;
    bool enableArtifacts = true;
    bool enableToolCalling = true;
    bool enableReasoning = true;

    // Streaming
    bool streamOutput = true;
    uint32_t chunkSize = 64;

    // Memory estimates
    uint64_t estimateModelMemory() const {
        uint64_t params = 120'000'000'000ULL;
        uint8_t bits = static_cast<uint8_t>(modelQuant);
        return (params * bits) / 8;
    }

    uint64_t estimateKVMemory(uint32_t context) const {
        uint64_t base = 2 * 80 * 12 * 128 * context;
        uint8_t bits = static_cast<uint8_t>(kvQuant);
        return (base * bits) / 8;
    }

    uint64_t estimateGPUMemory() const {
        uint64_t gpuLayerParams = (120'000'000'000ULL * gpuLayers) / 80;
        uint8_t bits = static_cast<uint8_t>(modelQuant);
        uint64_t gpuModel = (gpuLayerParams * bits) / 8;
        uint64_t draft = offloadDraftToGPU ? 2'000'000'000ULL : 0;
        uint64_t kv = estimateKVMemory(slidingWindow);
        uint64_t activations = 2ULL * 1024 * 1024 * 1024;
        return gpuModel + draft + kv + activations;
    }

    bool fitsInHardware() const {
        uint64_t systemRAM = estimateModelMemory() +
                            estimateKVMemory(maxContext) +
                            (offloadDraftToGPU ? 0 : 2'000'000'000ULL);
        uint64_t gpuVRAM = estimateGPUMemory();
        return systemRAM <= 60ULL * 1024 * 1024 * 1024 &&
               gpuVRAM   <= 15ULL * 1024 * 1024 * 1024;
    }
};

} // namespace Prometheus
