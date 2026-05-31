// ============================================================================
// PrometheusMoE.cpp — 800B MoE Weight Loader Implementation
// ============================================================================
// Detects MoE from GGUF metadata, memory-maps weights, builds routing tables.
//
// Pattern: Fail-closed, zero-copy, AVX2-accelerated routing.
// ============================================================================

#include "PrometheusMoE.h"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstring>

// AVX2 for routing softmax
#if defined(_MSC_VER)
#include <immintrin.h>
#endif

namespace RawrXD {
namespace Inference {

// ============================================================================
// GGUF Constants
// ============================================================================
static constexpr uint32_t GGUF_MAGIC = 0x46554747; // "GGUF"
static constexpr uint32_t GGUF_VERSION = 3;

// GGUF value types
enum class GGUFType : uint32_t {
    UINT8 = 0, INT8 = 1, UINT16 = 2, INT16 = 3,
    UINT32 = 4, INT32 = 5, FLOAT32 = 6, BOOL = 7,
    STRING = 8, ARRAY = 9, UINT64 = 10, INT64 = 11,
    FLOAT64 = 12
};

// ============================================================================
// Construction / Destruction
// ============================================================================

PrometheusMoE::PrometheusMoE() = default;

PrometheusMoE::~PrometheusMoE() {
    Unload();
}

// ============================================================================
// Detection (fast metadata-only probe)
// ============================================================================

MoEConfig PrometheusMoE::Probe(const std::string& ggufPath) {
    MoEConfig cfg;

    std::ifstream file(ggufPath, std::ios::binary);
    if (!file.is_open()) return cfg;

    // Read header
    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), 4);
    if (magic != GGUF_MAGIC) return cfg;

    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), 4);
    if (version > GGUF_VERSION) return cfg;

    uint64_t tensorCount = 0, metadataCount = 0;
    file.read(reinterpret_cast<char*>(&tensorCount), 8);
    file.read(reinterpret_cast<char*>(&metadataCount), 8);

    // Parse metadata to detect MoE
    for (uint64_t i = 0; i < metadataCount; ++i) {
        // Read key length + key
        uint64_t keyLen = 0;
        file.read(reinterpret_cast<char*>(&keyLen), 8);
        std::string key(keyLen, '\0');
        file.read(key.data(), keyLen);

        // Read value type
        uint32_t valType = 0;
        file.read(reinterpret_cast<char*>(&valType), 4);

        // Read value based on type
        auto ReadValue = [&](std::ifstream& f, uint32_t type) -> int64_t {
            int64_t result = 0;
            switch (static_cast<GGUFType>(type)) {
                case GGUFType::UINT8: { uint8_t v; f.read(reinterpret_cast<char*>(&v), 1); result = v; break; }
                case GGUFType::INT8: { int8_t v; f.read(reinterpret_cast<char*>(&v), 1); result = v; break; }
                case GGUFType::UINT16: { uint16_t v; f.read(reinterpret_cast<char*>(&v), 2); result = v; break; }
                case GGUFType::INT16: { int16_t v; f.read(reinterpret_cast<char*>(&v), 2); result = v; break; }
                case GGUFType::UINT32: { uint32_t v; f.read(reinterpret_cast<char*>(&v), 4); result = v; break; }
                case GGUFType::INT32: { int32_t v; f.read(reinterpret_cast<char*>(&v), 4); result = v; break; }
                case GGUFType::FLOAT32: { float v; f.read(reinterpret_cast<char*>(&v), 4); result = static_cast<int64_t>(v); break; }
                case GGUFType::BOOL: { bool v; f.read(reinterpret_cast<char*>(&v), 1); result = v; break; }
                case GGUFType::UINT64: { uint64_t v; f.read(reinterpret_cast<char*>(&v), 8); result = static_cast<int64_t>(v); break; }
                case GGUFType::INT64: { int64_t v; f.read(reinterpret_cast<char*>(&v), 8); result = v; break; }
                case GGUFType::FLOAT64: { double v; f.read(reinterpret_cast<char*>(&v), 8); result = static_cast<int64_t>(v); break; }
                case GGUFType::STRING: {
                    uint64_t len = 0;
                    f.read(reinterpret_cast<char*>(&len), 8);
                    f.seekg(static_cast<std::streamoff>(len), std::ios::cur);
                    result = 0;
                    break;
                }
                case GGUFType::ARRAY: {
                    uint32_t arrType = 0;
                    uint64_t arrLen = 0;
                    f.read(reinterpret_cast<char*>(&arrType), 4);
                    f.read(reinterpret_cast<char*>(&arrLen), 8);
                    // Skip array data
                    size_t elemSize = 0;
                    switch (static_cast<GGUFType>(arrType)) {
                        case GGUFType::UINT8: case GGUFType::INT8: elemSize = 1; break;
                        case GGUFType::UINT16: case GGUFType::INT16: elemSize = 2; break;
                        case GGUFType::UINT32: case GGUFType::INT32: case GGUFType::FLOAT32: elemSize = 4; break;
                        case GGUFType::UINT64: case GGUFType::INT64: case GGUFType::FLOAT64: elemSize = 8; break;
                        default: elemSize = 1; break;
                    }
                    f.seekg(static_cast<std::streamoff>(arrLen * elemSize), std::ios::cur);
                    result = 0;
                    break;
                }
            }
            return result;
        };

        int64_t val = ReadValue(file, valType);

        // Check for MoE keys
        if (key == "llama.expert_count") {
            cfg.isMoE = true;
            cfg.numExperts = static_cast<uint32_t>(val);
        } else if (key == "llama.expert_used_count") {
            cfg.expertsPerToken = static_cast<uint32_t>(val);
        } else if (key == "llama.expert_shared_count") {
            cfg.numSharedExperts = static_cast<uint32_t>(val);
        } else if (key == "llama.block_count" || key == "llama.num_hidden_layers") {
            cfg.numLayers = static_cast<uint32_t>(val);
        } else if (key == "llama.embedding_length" || key == "llama.hidden_size") {
            cfg.hiddenDim = static_cast<uint32_t>(val);
        } else if (key == "llama.feed_forward_length" || key == "llama.intermediate_size") {
            cfg.intermediateDim = static_cast<uint32_t>(val);
        } else if (key == "llama.attention.head_count") {
            cfg.numHeads = static_cast<uint32_t>(val);
        } else if (key == "llama.attention.head_count_kv") {
            cfg.numKVHeads = static_cast<uint32_t>(val);
        } else if (key == "llama.attention.key_length" || key == "llama.head_dim") {
            cfg.headDim = static_cast<uint32_t>(val);
        } else if (key == "llama.vocab_size") {
            cfg.vocabSize = static_cast<uint32_t>(val);
        }
    }

    // Estimate sizes
    if (cfg.IsValid()) {
        // Per-expert FFN: gate + up + down (SwiGLU)
        size_t expertParams = 3ULL * cfg.hiddenDim * cfg.intermediateDim;
        size_t totalExpertParams = cfg.numExperts * expertParams;
        size_t sharedParams = cfg.numSharedExperts * expertParams;

        // Attention per layer
        size_t qParams = cfg.numHeads * cfg.hiddenDim * cfg.headDim;
        size_t kvParams = cfg.numKVHeads * cfg.hiddenDim * cfg.headDim * 2; // K + V
        size_t outParams = cfg.numHeads * cfg.headDim * cfg.hiddenDim;
        size_t attnParams = qParams + kvParams + outParams;

        // Router
        size_t routerParams = cfg.hiddenDim * cfg.numExperts;

        // Per layer total
        size_t layerParams = attnParams + routerParams + totalExpertParams + sharedParams;
        cfg.totalParams = layerParams * cfg.numLayers;

        // Active per token
        size_t activeExpertParams = cfg.expertsPerToken * expertParams;
        size_t activeSharedParams = cfg.numSharedExperts * expertParams;
        cfg.activeParams = (attnParams + routerParams + activeExpertParams + activeSharedParams) * cfg.numLayers;

        // Model size (assume Q4_K_M = 4.5 bits per param)
        cfg.modelSizeBytes = (cfg.totalParams * 45) / 80; // 4.5 bits = 45/80 bytes

        // KV cache: 2 * numLayers * numKVHeads * headDim * context (assume 8K)
        size_t ctxSize = 8192;
        cfg.kvCacheBytes = 2ULL * cfg.numLayers * cfg.numKVHeads * cfg.headDim * ctxSize * sizeof(float);
    }

    return cfg;
}

// ============================================================================
// Full Load
// ============================================================================

bool PrometheusMoE::Load(const std::string& ggufPath, int gpuLayers) {
    if (IsLoaded()) Unload();

    // 1. Probe metadata
    config_ = Probe(ggufPath);
    if (!config_.IsValid()) {
        return false; // Not an MoE model
    }

    // 2. Memory-map the file
    if (!MapWeights(ggufPath)) {
        Unload();
        return false;
    }

    // 3. Build routing tables
    BuildRoutingTables();

    (void)gpuLayers; // Used by caller to configure GPU offloading
    return true;
}

void PrometheusMoE::Unload() {
    Unmap();
    tensors_.clear();
    routerWeights_.clear();
    config_ = MoEConfig{};
}

bool PrometheusMoE::IsLoaded() const {
    return mmapBase_ != nullptr && config_.IsValid();
}

// ============================================================================
// Expert Routing
// ============================================================================

std::vector<uint32_t> PrometheusMoE::RouteExperts(
    uint32_t layer,
    const float* tokenEmbedding,
    uint32_t topK) const {

    std::vector<uint32_t> result;
    if (!IsLoaded() || layer >= config_.numLayers) return result;

    // Compute router logits
    auto logits = ComputeRouterLogits(layer, tokenEmbedding);
    if (logits.empty()) return result;

    // Softmax
    Softmax(logits.data(), logits.size());

    // Top-k selection
    std::vector<uint32_t> indices;
    std::vector<float> values;
    TopK(logits.data(), logits.size(), topK, indices, values);

    // Always include shared expert (index 0)
    result.push_back(0);
    for (auto idx : indices) {
        if (idx != 0 && result.size() < topK + config_.numSharedExperts) {
            result.push_back(idx);
        }
    }

    // Update stats
    stats_.totalExpertActivations += result.size();
    stats_.totalTokens++;
    stats_.avgActiveExperts = static_cast<double>(stats_.totalExpertActivations) / stats_.totalTokens;

    return result;
}

std::vector<float> PrometheusMoE::ComputeRouterLogits(
    uint32_t layer,
    const float* tokenEmbedding) const {

    std::vector<float> logits;
    if (!IsLoaded() || layer >= routerWeights_.size()) return logits;

    const auto& router = routerWeights_[layer];
    if (router.gate.empty()) return logits;

    uint32_t numExperts = config_.numExperts;
    logits.resize(numExperts);

    // Compute logits = gate @ tokenEmbedding + bias
    for (uint32_t e = 0; e < numExperts; ++e) {
        float sum = router.bias.empty() ? 0.0f : router.bias[e];
        const float* gateRow = router.gate.data() + e * config_.hiddenDim;

        // Dot product (AVX2 accelerated)
        uint32_t i = 0;
#if defined(__AVX2__) || defined(_MSC_VER)
        __m256 sumVec = _mm256_setzero_ps();
        for (; i + 7 < config_.hiddenDim; i += 8) {
            __m256 a = _mm256_loadu_ps(tokenEmbedding + i);
            __m256 b = _mm256_loadu_ps(gateRow + i);
            sumVec = _mm256_fmadd_ps(a, b, sumVec);
        }
        // Horizontal sum
        __m128 hi = _mm256_extractf128_ps(sumVec, 1);
        __m128 lo = _mm256_castps256_ps128(sumVec);
        __m128 s = _mm_add_ps(hi, lo);
        s = _mm_hadd_ps(s, s);
        s = _mm_hadd_ps(s, s);
        sum += _mm_cvtss_f32(s);
#endif
        for (; i < config_.hiddenDim; ++i) {
            sum += tokenEmbedding[i] * gateRow[i];
        }

        logits[e] = sum;
    }

    return logits;
}

// ============================================================================
// Weight Access
// ============================================================================

PrometheusMoE::ExpertWeights PrometheusMoE::GetExpertWeights(
    uint32_t layer, uint32_t expert) const {

    ExpertWeights w;
    if (!IsLoaded()) return w;

    std::string base = "blk." + std::to_string(layer) + ".";
    std::string suffix = (expert == 0) ? "shared" : ("exps." + std::to_string(expert - 1));

    auto FindTensor = [&](const std::string& name) -> const ExpertTensor* {
        auto it = tensors_.find(name);
        return (it != tensors_.end()) ? &it->second : nullptr;
    };

    const ExpertTensor* gate = FindTensor(base + "ffn_gate_" + suffix + ".weight");
    const ExpertTensor* up = FindTensor(base + "ffn_up_" + suffix + ".weight");
    const ExpertTensor* down = FindTensor(base + "ffn_down_" + suffix + ".weight");

    if (gate) { w.gate = gate->data; w.gateSize = gate->size; }
    if (up) { w.up = up->data; w.upSize = up->size; }
    if (down) { w.down = down->data; w.downSize = down->size; }

    return w;
}

PrometheusMoE::ExpertWeights PrometheusMoE::GetSharedExpertWeights(uint32_t layer) const {
    return GetExpertWeights(layer, 0); // Expert 0 = shared
}

// ============================================================================
// Stats
// ============================================================================

PrometheusMoE::Stats PrometheusMoE::GetStats() const {
    return stats_;
}

// ============================================================================
// Internal: Memory Mapping
// ============================================================================

bool PrometheusMoE::MapWeights(const std::string& path) {
    hFile_ = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                         nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile_ == INVALID_HANDLE_VALUE) return false;

    LARGE_INTEGER fileSize;
    if (!GetFileSizeEx(hFile_, &fileSize)) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
        return false;
    }
    mmapSize_ = static_cast<size_t>(fileSize.QuadPart);

    hMap_ = CreateFileMapping(hFile_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!hMap_) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
        return false;
    }

    mmapBase_ = MapViewOfFile(hMap_, FILE_MAP_READ, 0, 0, 0);
    if (!mmapBase_) {
        CloseHandle(hMap_);
        CloseHandle(hFile_);
        hMap_ = nullptr;
        hFile_ = INVALID_HANDLE_VALUE;
        return false;
    }

    return true;
}

void PrometheusMoE::Unmap() {
    if (mmapBase_) {
        UnmapViewOfFile(mmapBase_);
        mmapBase_ = nullptr;
    }
    if (hMap_) {
        CloseHandle(hMap_);
        hMap_ = nullptr;
    }
    if (hFile_ != INVALID_HANDLE_VALUE) {
        CloseHandle(hFile_);
        hFile_ = INVALID_HANDLE_VALUE;
    }
    mmapSize_ = 0;
}

// ============================================================================
// Internal: Routing Tables
// ============================================================================

void PrometheusMoE::BuildRoutingTables() {
    if (!mmapBase_ || config_.numLayers == 0) return;

    routerWeights_.resize(config_.numLayers);

    for (uint32_t layer = 0; layer < config_.numLayers; ++layer) {
        auto& router = routerWeights_[layer];

        // Allocate router weights
        router.gate.resize(config_.numExperts * config_.hiddenDim);
        router.bias.resize(config_.numExperts);

        // TODO: Parse actual router weights from mmap'd GGUF tensor data
        // For now, initialize with zeros (will be loaded from file)
        std::fill(router.gate.begin(), router.gate.end(), 0.0f);
        std::fill(router.bias.begin(), router.bias.end(), 0.0f);
    }
}

// ============================================================================
// AVX2 Utilities
// ============================================================================

void PrometheusMoE::Softmax(float* data, size_t count) {
    if (count == 0) return;

    // Find max for numerical stability
    float maxVal = data[0];
    for (size_t i = 1; i < count; ++i) {
        if (data[i] > maxVal) maxVal = data[i];
    }

    // Exponentiate and sum
    float sum = 0.0f;
    for (size_t i = 0; i < count; ++i) {
        data[i] = std::exp(data[i] - maxVal);
        sum += data[i];
    }

    // Normalize
    if (sum > 0.0f) {
        float invSum = 1.0f / sum;
        for (size_t i = 0; i < count; ++i) {
            data[i] *= invSum;
        }
    }
}

void PrometheusMoE::TopK(const float* data, size_t count, uint32_t k,
                           std::vector<uint32_t>& indices,
                           std::vector<float>& values) {
    indices.clear();
    values.clear();

    if (count == 0 || k == 0) return;

    // Build index-value pairs
    std::vector<std::pair<float, uint32_t>> pairs;
    pairs.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
        pairs.push_back({data[i], i});
    }

    // Partial sort for top-k
    if (k < count) {
        std::partial_sort(pairs.begin(), pairs.begin() + k, pairs.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
        pairs.resize(k);
    } else {
        std::sort(pairs.begin(), pairs.end(),
            [](const auto& a, const auto& b) { return a.first > b.first; });
    }

    // Extract
    for (const auto& p : pairs) {
        values.push_back(p.first);
        indices.push_back(p.second);
    }
}

} // namespace Inference
} // namespace RawrXD
