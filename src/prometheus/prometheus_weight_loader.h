#pragma once
#include "prometheus_config.h"
#include "prometheus_moe.h"
#include "prometheus_attention.h"

#include <cstdint>
#include <string>
#include <vector>
#include <fstream>

namespace Prometheus {

// =============================================================================
// WEIGHT LOADER — GGUF / Safetensors → Prometheus Engine
// =============================================================================

enum class WeightFormat : uint8_t {
    Unknown = 0,
    GGUF    = 1,   // llama.cpp GGUF (Q4_0, Q8_0, FP16, FP32)
    SafeTensors = 2,  // HuggingFace Safetensors
    RawrXD  = 3,   // Native RawrXD format (.rxa)
};

struct WeightLoadResult {
    bool success = false;
    std::string error;
    size_t bytesLoaded = 0;
    size_t tensorsLoaded = 0;
    double loadTimeMs = 0.0;
    WeightFormat detectedFormat = WeightFormat::Unknown;
};

// Tensor descriptor for a single weight block
struct TensorDesc {
    std::string name;           // e.g. "blk.0.attn_q.weight"
    std::vector<uint32_t> shape;
    uint32_t dtype = 0;         // GGML type enum
    const void* data = nullptr;
    size_t bytes = 0;
};

// =============================================================================
// GGUF LOADER
// =============================================================================

class GGUFLoader {
public:
    // Load GGUF file and return tensor descriptors
    static WeightLoadResult load(
        const std::string& path,
        std::vector<TensorDesc>& outTensors,
        ModelConfig* outConfig = nullptr
    );

    // Map GGUF tensor names to Prometheus layer indices
    static bool mapTensorToLayer(
        const std::string& ggufName,
        uint32_t& layerIdx,
        std::string& component    // "attn_q", "attn_k", "attn_v", "attn_o",
                                  // "moe_gate", "moe_up", "moe_gate_proj", "moe_down",
                                  // "shared_up", "shared_gate", "shared_down",
                                  // "norm_attn", "norm_ffn"
    );

private:
    static constexpr uint32_t GGUF_MAGIC = 0x46554747; // "GGUF"
    static constexpr uint32_t GGUF_VERSION = 3;

    struct GGUFHeader {
        uint32_t magic;
        uint32_t version;
        uint64_t tensorCount;
        uint64_t metadataCount;
    };

    static bool readHeader(std::ifstream& file, GGUFHeader& header);
    static bool readMetadata(std::ifstream& file, uint64_t count, ModelConfig* cfg);
    static bool readTensors(std::ifstream& file, uint64_t count, std::vector<TensorDesc>& out);
};

// =============================================================================
// SAFETENSORS LOADER
// =============================================================================

class SafeTensorsLoader {
public:
    static WeightLoadResult load(
        const std::string& path,
        std::vector<TensorDesc>& outTensors,
        ModelConfig* outConfig = nullptr
    );

private:
    // Safetensors JSON header parsing
    static bool parseHeader(const std::string& json, std::vector<TensorDesc>& out);
};

// =============================================================================
// WEIGHT INITIALIZER (for testing / random weights)
// =============================================================================

class WeightInitializer {
public:
    // Initialize random weights for a model config (testing only)
    static void initRandom(
        const ModelConfig& cfg,
        std::vector<float>& embed,
        std::vector<MoELayer>& moeLayers,
        std::vector<AttentionLayer>& attnLayers
    );

    // Xavier/Glorot initialization
    static void xavierInit(float* data, size_t fanIn, size_t fanOut, uint32_t seed);

    // Kaiming/He initialization (for SiLU/SwiGLU)
    static void kaimingInit(float* data, size_t fanIn, uint32_t seed);
};

// =============================================================================
// MEMORY ESTIMATOR
// =============================================================================

struct MemoryEstimate {
    uint64_t modelWeightsBytes = 0;      // Quantized weights
    uint64_t kvCacheBytes = 0;          // KV cache for max context
    uint64_t activationBytes = 0;          // Per-layer activations
    uint64_t expertBuffersBytes = 0;     // Expert routing buffers
    uint64_t totalBytes = 0;
    uint64_t recommendedVRAMBytes = 0;   // With 20% overhead
};

class MemoryEstimator {
public:
    static MemoryEstimate estimate(const ModelConfig& cfg, uint32_t batchSize = 1);

    // Per-component breakdown
    static uint64_t estimateEmbedding(const ModelConfig& cfg);
    static uint64_t estimateAttention(const ModelConfig& cfg);
    static uint64_t estimateMoE(const ModelConfig& cfg);
    static uint64_t estimateKVCache(const ModelConfig& cfg, uint32_t seqLen);
};

// =============================================================================
// LATENCY ESTIMATOR
// =============================================================================

struct LatencyEstimate {
    double prefillMs = 0.0;       // Time to process prompt
    double tokenMs = 0.0;         // Time per generated token
    double tps = 0.0;             // Tokens per second
    double memoryBandwidthGBps = 0.0;  // Required memory BW
};

class LatencyEstimator {
public:
    // Estimate based on hardware profile
    static LatencyEstimate estimate(
        const ModelConfig& cfg,
        double memoryBandwidthGBps,   // e.g. 800 GB/s for H100
        double computeTFLOPS,         // e.g. 67 TFLOPS for H100 FP16
        uint32_t promptLen = 1024,
        uint32_t genLen = 256
    );

    // Estimate for CPU inference (AVX-512)
    static LatencyEstimate estimateCPU(
        const ModelConfig& cfg,
        uint32_t numThreads,
        uint32_t promptLen = 1024,
        uint32_t genLen = 256
    );
};

} // namespace Prometheus
