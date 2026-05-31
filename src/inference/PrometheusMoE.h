// ============================================================================
// PrometheusMoE.h — 800B MoE Weight Loader for RawrXD Sovereign Inference
// ============================================================================
// Detects MoE architecture from GGUF metadata and routes expert weights.
// Integrates with LlamaNativeBridge (llama.cpp b3506+).
//
// Architecture: 80 layers, 44 experts, 2 active + 1 shared per token
// Total: ~800B params | Active: ~67B per token | Sparsity: 12:1
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <windows.h>

namespace RawrXD {
namespace Inference {

// ============================================================================
// MoE Architecture Configuration (from GGUF metadata)
// ============================================================================
struct MoEConfig {
    bool isMoE = false;

    // Architecture
    uint32_t numLayers = 0;
    uint32_t numExperts = 0;
    uint32_t expertsPerToken = 0;
    uint32_t numSharedExperts = 0;
    uint32_t hiddenDim = 0;
    uint32_t intermediateDim = 0;
    uint32_t numHeads = 0;
    uint32_t numKVHeads = 0;
    uint32_t headDim = 0;
    uint32_t vocabSize = 0;

    // Expert routing
    uint32_t topK = 2;              // Experts per token (active)
    float    routerBias = 0.0f;     // Gate bias
    bool     routerNorm = false;    // Normalize router logits

    // Memory estimates
    size_t totalParams = 0;
    size_t activeParams = 0;
    size_t modelSizeBytes = 0;      // Quantized size
    size_t kvCacheBytes = 0;

    // Quantization
    int quantType = 0;              // GGML_RXD_TYPE_Q4_0, etc.

    bool IsValid() const {
        return isMoE && numLayers > 0 && numExperts > 0
            && expertsPerToken > 0 && hiddenDim > 0;
    }

    // VRAM estimate (bytes)
    size_t EstimateVRAM() const {
        return modelSizeBytes + kvCacheBytes + (256 * 1024 * 1024); // +256MB overhead
    }
};

// ============================================================================
// Expert Weight Descriptor
// ============================================================================
struct ExpertTensor {
    std::string name;           // Full tensor name (e.g., "blk.0.ffn_gate_exps.0.weight")
    uint32_t    layer = 0;      // Layer index
    uint32_t    expert = 0;     // Expert index (0 = shared)
    std::string type;           // "gate", "up", "down", "gate_in", "gate_out"
    void*       data = nullptr; // Mapped pointer (R15 or mmap)
    size_t      size = 0;       // Bytes
    uint32_t    rows = 0;
    uint32_t    cols = 0;
};

// ============================================================================
// MoE Weight Loader
// ============================================================================
class PrometheusMoE {
public:
    PrometheusMoE();
    ~PrometheusMoE();

    // Non-copyable (owns mapped memory)
    PrometheusMoE(const PrometheusMoE&) = delete;
    PrometheusMoE& operator=(const PrometheusMoE&) = delete;

    // ------------------------------------------------------------------------
    // Detection + Loading
    // ------------------------------------------------------------------------

    // Probe GGUF file for MoE architecture (fast, reads only metadata)
    static MoEConfig Probe(const std::string& ggufPath);

    // Full load: mmap weights + build expert routing tables
    bool Load(const std::string& ggufPath, int gpuLayers = -1);
    void Unload();
    bool IsLoaded() const;

    // ------------------------------------------------------------------------
    // Expert Routing (called per-token during inference)
    // ------------------------------------------------------------------------

    // Select top-k experts for a given token embedding
    // Returns expert indices (0 = shared expert)
    std::vector<uint32_t> RouteExperts(
        uint32_t layer,
        const float* tokenEmbedding,  // [hiddenDim]
        uint32_t topK
    ) const;

    // Compute router logits for all experts
    std::vector<float> ComputeRouterLogits(
        uint32_t layer,
        const float* tokenEmbedding
    ) const;

    // ------------------------------------------------------------------------
    // Weight Access
    // ------------------------------------------------------------------------

    // Get expert FFN weights (gate, up, down) for a specific layer+expert
    // Returns pointers into mmap'd region (zero-copy)
    struct ExpertWeights {
        const void* gate = nullptr;   // [intermediateDim, hiddenDim]
        const void* up = nullptr;     // [intermediateDim, hiddenDim]
        const void* down = nullptr;   // [hiddenDim, intermediateDim]
        size_t      gateSize = 0;
        size_t      upSize = 0;
        size_t      downSize = 0;
    };

    ExpertWeights GetExpertWeights(uint32_t layer, uint32_t expert) const;

    // Shared expert (always active)
    ExpertWeights GetSharedExpertWeights(uint32_t layer) const;

    // ------------------------------------------------------------------------
    // Configuration
    // ------------------------------------------------------------------------
    const MoEConfig& GetConfig() const { return config_; }

    // ------------------------------------------------------------------------
    // Stats
    // ------------------------------------------------------------------------
    struct Stats {
        uint64_t totalTokens = 0;
        uint64_t totalExpertActivations = 0;
        double   avgActiveExperts = 0.0;
        uint64_t cacheHits = 0;
        uint64_t cacheMisses = 0;
    };
    Stats GetStats() const;

private:
    MoEConfig config_;

    // Memory mapping
    void* mmapBase_ = nullptr;
    size_t mmapSize_ = 0;
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    HANDLE hMap_ = nullptr;

    // Expert tensor registry
    std::unordered_map<std::string, ExpertTensor> tensors_;

    // Per-layer expert routing weights (pre-loaded)
    struct RouterWeights {
        std::vector<float> gate;     // [numExperts, hiddenDim]
        std::vector<float> bias;     // [numExperts]
    };
    std::vector<RouterWeights> routerWeights_; // Per layer

    // Stats
    mutable Stats stats_;

    // Internal
    bool ParseGGUFMetadata(const std::string& path);
    bool MapWeights(const std::string& path);
    void BuildRoutingTables();
    void Unmap();

    // AVX2 softmax for routing
    static void Softmax(float* data, size_t count);
    static void TopK(const float* data, size_t count, uint32_t k,
                     std::vector<uint32_t>& indices, std::vector<float>& values);
};

} // namespace Inference
} // namespace RawrXD
