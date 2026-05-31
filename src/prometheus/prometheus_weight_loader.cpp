#include "prometheus_weight_loader.h"
#include <cmath>
#include <random>
#include <chrono>
#include <nlohmann/json.hpp>

namespace Prometheus {

// =============================================================================
// GGUF LOADER IMPLEMENTATION
// =============================================================================

WeightLoadResult GGUFLoader::load(
    const std::string& path,
    std::vector<TensorDesc>& outTensors,
    ModelConfig* outConfig
) {
    WeightLoadResult result;
    auto start = std::chrono::steady_clock::now();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        result.error = "Failed to open: " + path;
        return result;
    }

    GGUFHeader header;
    if (!readHeader(file, header)) {
        result.error = "Invalid GGUF header";
        return result;
    }

    result.detectedFormat = WeightFormat::GGUF;

    if (outConfig) {
        readMetadata(file, header.metadataCount, outConfig);
    } else {
        // Skip metadata
        file.seekg(header.metadataCount * 16, std::ios::cur);
    }

    if (!readTensors(file, header.tensorCount, outTensors)) {
        result.error = "Failed to read tensors";
        return result;
    }

    result.success = true;
    result.tensorsLoaded = outTensors.size();
    for (const auto& t : outTensors) result.bytesLoaded += t.bytes;

    auto end = std::chrono::steady_clock::now();
    result.loadTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

bool GGUFLoader::readHeader(std::ifstream& file, GGUFHeader& header) {
    file.read(reinterpret_cast<char*>(&header.magic), sizeof(header.magic));
    file.read(reinterpret_cast<char*>(&header.version), sizeof(header.version));
    file.read(reinterpret_cast<char*>(&header.tensorCount), sizeof(header.tensorCount));
    file.read(reinterpret_cast<char*>(&header.metadataCount), sizeof(header.metadataCount));

    if (header.magic != GGUF_MAGIC) return false;
    if (header.version != GGUF_VERSION) return false;
    return true;
}

bool GGUFLoader::readMetadata(std::ifstream& file, uint64_t count, ModelConfig* cfg) {
    for (uint64_t i = 0; i < count; ++i) {
        uint64_t keyLen;
        file.read(reinterpret_cast<char*>(&keyLen), sizeof(keyLen));
        std::string key(keyLen, '\0');
        file.read(key.data(), keyLen);

        uint32_t valueType;
        file.read(reinterpret_cast<char*>(&valueType), sizeof(valueType));

        // Parse known metadata keys
        if (cfg) {
            if (key == "general.architecture") {
                // Skip string value
                uint64_t strLen;
                file.read(reinterpret_cast<char*>(&strLen), sizeof(strLen));
                file.seekg(strLen, std::ios::cur);
            } else if (key == "llama.embedding_length") {
                file.read(reinterpret_cast<char*>(&cfg->hiddenDim), sizeof(uint32_t));
            } else if (key == "llama.block_count") {
                file.read(reinterpret_cast<char*>(&cfg->numLayers), sizeof(uint32_t));
            } else if (key == "llama.attention.head_count") {
                file.read(reinterpret_cast<char*>(&cfg->numHeads), sizeof(uint32_t));
            } else if (key == "llama.attention.head_count_kv") {
                file.read(reinterpret_cast<char*>(&cfg->numKVHeads), sizeof(uint32_t));
            } else if (key == "llama.context_length") {
                file.read(reinterpret_cast<char*>(&cfg->maxPosition), sizeof(uint32_t));
            } else {
                // Skip unknown value
                switch (valueType) {
                    case 0: file.seekg(1, std::ios::cur); break;  // uint8
                    case 1: file.seekg(1, std::ios::cur); break;  // int8
                    case 2: file.seekg(2, std::ios::cur); break;  // uint16
                    case 3: file.seekg(2, std::ios::cur); break;  // int16
                    case 4: file.seekg(4, std::ios::cur); break;  // uint32
                    case 5: file.seekg(4, std::ios::cur); break;  // int32
                    case 6: file.seekg(4, std::ios::cur); break;  // float32
                    case 7: file.seekg(1, std::ios::cur); break;  // bool
                    case 8: { uint64_t len; file.read(reinterpret_cast<char*>(&len), 8); file.seekg(len, std::ios::cur); break; }
                    case 9: { uint64_t len; file.read(reinterpret_cast<char*>(&len), 8); file.seekg(len, std::ios::cur); break; }
                    case 10: { uint64_t len; file.read(reinterpret_cast<char*>(&len), 8); file.seekg(len * 8, std::ios::cur); break; }
                    default: return false;
                }
            }
        }
    }
    return true;
}

bool GGUFLoader::readTensors(std::ifstream& file, uint64_t count, std::vector<TensorDesc>& out) {
    out.reserve(count);
    for (uint64_t i = 0; i < count; ++i) {
        uint64_t nameLen;
        file.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        std::string name(nameLen, '\0');
        file.read(name.data(), nameLen);

        uint32_t nDims;
        file.read(reinterpret_cast<char*>(&nDims), sizeof(nDims));

        TensorDesc desc;
        desc.name = name;
        desc.shape.resize(nDims);
        for (uint32_t d = 0; d < nDims; ++d) {
            file.read(reinterpret_cast<char*>(&desc.shape[d]), sizeof(uint64_t));
        }

        file.read(reinterpret_cast<char*>(&desc.dtype), sizeof(uint32_t));

        uint64_t offset;
        file.read(reinterpret_cast<char*>(&offset), sizeof(offset));

        // Calculate bytes from dtype and shape
        size_t numElements = 1;
        for (auto s : desc.shape) numElements *= s;

        switch (desc.dtype) {
            case 0: desc.bytes = numElements * 4; break;   // FP32
            case 1: desc.bytes = numElements * 2; break;   // FP16
            case 2: desc.bytes = (numElements / 32) * 18; break; // Q4_0
            case 3: desc.bytes = (numElements / 32) * 20; break; // Q4_1
            case 7: desc.bytes = (numElements / 32) * 34; break; // Q8_0
            default: desc.bytes = numElements * 4;
        }

        // Store file offset for lazy loading
        desc.data = reinterpret_cast<const void*>(offset);
        out.push_back(desc);
    }
    return true;
}

bool GGUFLoader::mapTensorToLayer(
    const std::string& ggufName,
    uint32_t& layerIdx,
    std::string& component
) {
    // Parse "blk.N.component.weight" format
    if (ggufName.substr(0, 4) != "blk.") return false;

    size_t dotPos = ggufName.find('.', 4);
    if (dotPos == std::string::npos) return false;

    layerIdx = std::stoul(ggufName.substr(4, dotPos - 4));

    std::string rest = ggufName.substr(dotPos + 1);
    if (rest == "attn_q.weight") component = "attn_q";
    else if (rest == "attn_k.weight") component = "attn_k";
    else if (rest == "attn_v.weight") component = "attn_v";
    else if (rest == "attn_o.weight") component = "attn_o";
    else if (rest == "attn_norm.weight") component = "norm_attn";
    else if (rest == "ffn_gate.weight") component = "moe_gate";
    else if (rest == "ffn_up.weight") component = "moe_up";
    else if (rest == "ffn_down.weight") component = "moe_down";
    else if (rest == "ffn_norm.weight") component = "norm_ffn";
    else return false;

    return true;
}

// =============================================================================
// SAFETENSORS LOADER IMPLEMENTATION
// =============================================================================

WeightLoadResult SafeTensorsLoader::load(
    const std::string& path,
    std::vector<TensorDesc>& outTensors,
    ModelConfig* outConfig
) {
    WeightLoadResult result;
    auto start = std::chrono::steady_clock::now();

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        result.error = "Failed to open: " + path;
        return result;
    }

    uint64_t headerLen;
    file.read(reinterpret_cast<char*>(&headerLen), sizeof(headerLen));

    std::string headerJson(headerLen, '\0');
    file.read(headerJson.data(), headerLen);

    if (!parseHeader(headerJson, outTensors)) {
        result.error = "Failed to parse Safetensors header";
        return result;
    }

    result.detectedFormat = WeightFormat::SafeTensors;
    result.success = true;
    result.tensorsLoaded = outTensors.size();

    auto end = std::chrono::steady_clock::now();
    result.loadTimeMs = std::chrono::duration<double, std::milli>(end - start).count();

    return result;
}

bool SafeTensorsLoader::parseHeader(const std::string& json, std::vector<TensorDesc>& out) {
    try {
        auto j = nlohmann::json::parse(json);
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.key() == "__metadata__") continue;

            TensorDesc desc;
            desc.name = it.key();
            auto& info = it.value();

            for (auto& dim : info["shape"]) {
                desc.shape.push_back(dim.get<uint32_t>());
            }

            std::string dtype = info["dtype"];
            if (dtype == "F32") desc.dtype = 0;
            else if (dtype == "F16") desc.dtype = 1;
            else if (dtype == "BF16") desc.dtype = 2;
            else desc.dtype = 0;

            auto offsets = info["data_offsets"];
            desc.bytes = offsets.at(1).get<size_t>() - offsets.at(0).get<size_t>();
            out.push_back(desc);
        }
        return true;
    } catch (...) {
        return false;
    }
}

// =============================================================================
// WEIGHT INITIALIZER IMPLEMENTATION
// =============================================================================

void WeightInitializer::initRandom(
    const ModelConfig& cfg,
    std::vector<float>& embed,
    std::vector<MoELayer>& moeLayers,
    std::vector<AttentionLayer>& attnLayers
) {
    std::mt19937 rng(42);
    std::normal_distribution<float> dist(0.0f, 0.02f);

    // Embedding
    embed.resize((size_t)cfg.vocabSize * cfg.hiddenDim);
    for (auto& v : embed) v = dist(rng);

    // MoE layers
    moeLayers.reserve(cfg.numLayers);
    for (uint32_t l = 0; l < cfg.numLayers; ++l) {
        moeLayers.emplace_back(
            cfg.hiddenDim,
            cfg.intermediateDim,
            cfg.numExperts,
            cfg.expertsPerToken,
            cfg.sharedExperts
        );

        auto& moe = moeLayers.back();

        // Gate weights
        xavierInit(moe.gateWeight().data(), cfg.hiddenDim, cfg.numExperts, 42 + l);

        // Expert weights
        for (uint32_t e = 0; e < cfg.numExperts; ++e) {
            xavierInit(moe.expert(e).upWeight.data(), cfg.hiddenDim, cfg.intermediateDim, 100 + e);
            kaimingInit(moe.expert(e).gateWeight.data(), cfg.hiddenDim, 200 + e);
            xavierInit(moe.expert(e).downWeight.data(), cfg.intermediateDim, cfg.hiddenDim, 300 + e);
        }

        // Shared experts
        for (uint32_t s = 0; s < cfg.sharedExperts; ++s) {
            xavierInit(moe.sharedExpert(s).upWeight.data(), cfg.hiddenDim, cfg.intermediateDim, 400 + s);
            kaimingInit(moe.sharedExpert(s).gateWeight.data(), cfg.hiddenDim, 500 + s);
            xavierInit(moe.sharedExpert(s).downWeight.data(), cfg.intermediateDim, cfg.hiddenDim, 600 + s);
        }
    }

    // Attention layers
    attnLayers.reserve(cfg.numLayers);
    for (uint32_t l = 0; l < cfg.numLayers; ++l) {
        attnLayers.emplace_back(
            cfg.hiddenDim,
            cfg.numHeads,
            cfg.numKVHeads,
            cfg.headDim,
            cfg.slidingWindow,
            cfg.globalStride
        );

        auto& attn = attnLayers.back();
        xavierInit(attn.qWeight().data(), cfg.hiddenDim, cfg.numHeads * cfg.headDim, 700 + l);
        xavierInit(attn.kWeight().data(), cfg.hiddenDim, cfg.numKVHeads * cfg.headDim, 800 + l);
        xavierInit(attn.vWeight().data(), cfg.hiddenDim, cfg.numKVHeads * cfg.headDim, 900 + l);
        xavierInit(attn.oWeight().data(), cfg.numHeads * cfg.headDim, cfg.hiddenDim, 1000 + l);
    }
}

void WeightInitializer::xavierInit(float* data, size_t fanIn, size_t fanOut, uint32_t seed) {
    std::mt19937 rng(seed);
    float limit = std::sqrt(6.0f / (fanIn + fanOut));
    std::uniform_real_distribution<float> dist(-limit, limit);
    for (size_t i = 0; i < fanIn * fanOut; ++i) {
        data[i] = dist(rng);
    }
}

void WeightInitializer::kaimingInit(float* data, size_t fanIn, uint32_t seed) {
    std::mt19937 rng(seed);
    float stddev = std::sqrt(2.0f / fanIn);
    std::normal_distribution<float> dist(0.0f, stddev);
    for (size_t i = 0; i < fanIn; ++i) {
        data[i] = dist(rng);
    }
}

// =============================================================================
// MEMORY ESTIMATOR IMPLEMENTATION
// =============================================================================

MemoryEstimate MemoryEstimator::estimate(const ModelConfig& cfg, uint32_t batchSize) {
    MemoryEstimate est;

    est.modelWeightsBytes = estimateEmbedding(cfg)
                          + estimateAttention(cfg)
                          + estimateMoE(cfg);

    est.kvCacheBytes = estimateKVCache(cfg, cfg.maxPosition) * batchSize;
    est.activationBytes = (uint64_t)cfg.hiddenDim * cfg.numLayers * 4 * sizeof(float) * batchSize;
    est.expertBuffersBytes = (uint64_t)cfg.intermediateDim * cfg.numExperts * sizeof(float) * batchSize;

    est.totalBytes = est.modelWeightsBytes + est.kvCacheBytes + est.activationBytes + est.expertBuffersBytes;
    est.recommendedVRAMBytes = static_cast<uint64_t>(est.totalBytes * 1.2);  // 20% overhead

    return est;
}

uint64_t MemoryEstimator::estimateEmbedding(const ModelConfig& cfg) {
    return (uint64_t)cfg.vocabSize * cfg.hiddenDim * (cfg.weightBits / 8.0f);
}

uint64_t MemoryEstimator::estimateAttention(const ModelConfig& cfg) {
    uint64_t q = (uint64_t)cfg.hiddenDim * cfg.numHeads * cfg.headDim;
    uint64_t kv = (uint64_t)cfg.hiddenDim * cfg.numKVHeads * cfg.headDim * 2;
    uint64_t o = (uint64_t)cfg.numHeads * cfg.headDim * cfg.hiddenDim;
    uint64_t norms = (uint64_t)cfg.hiddenDim * 2;
    return (q + kv + o + norms) * cfg.numLayers * (cfg.weightBits / 8.0f);
}

uint64_t MemoryEstimator::estimateMoE(const ModelConfig& cfg) {
    uint64_t gate = (uint64_t)cfg.hiddenDim * cfg.numExperts;
    uint64_t perExpert = (uint64_t)cfg.hiddenDim * cfg.intermediateDim * 3;  // up + gate + down
    uint64_t allExperts = perExpert * cfg.numExperts;
    uint64_t shared = perExpert * cfg.sharedExperts;
    return (gate + allExperts + shared) * cfg.numLayers * (cfg.weightBits / 8.0f);
}

uint64_t MemoryEstimator::estimateKVCache(const ModelConfig& cfg, uint32_t seqLen) {
    return 2ULL * cfg.numLayers * cfg.numKVHeads * cfg.headDim * seqLen * (cfg.kvCacheBits / 8.0f);
}

// =============================================================================
// LATENCY ESTIMATOR IMPLEMENTATION
// =============================================================================

LatencyEstimate LatencyEstimator::estimate(
    const ModelConfig& cfg,
    double memoryBandwidthGBps,
    double computeTFLOPS,
    uint32_t promptLen,
    uint32_t genLen
) {
    LatencyEstimate est;

    // Memory-bound: each token reads all active weights
    uint64_t activeBytesPerToken =
        (uint64_t)cfg.hiddenDim * cfg.hiddenDim * 4 +           // Q, K, V, O projections
        (uint64_t)cfg.hiddenDim * cfg.intermediateDim * 3 *     // Up, Gate, Down
        (cfg.expertsPerToken + cfg.sharedExperts);

    double memoryBoundTimeMs = (activeBytesPerToken / (1024.0 * 1024.0 * 1024.0)) / memoryBandwidthGBps * 1000.0;

    // Compute-bound: matmul FLOPs
    uint64_t flopsPerToken =
        (uint64_t)cfg.hiddenDim * cfg.hiddenDim * 4 +           // Attention projections
        (uint64_t)cfg.hiddenDim * cfg.intermediateDim * 2 *     // Up + Gate
        (cfg.expertsPerToken + cfg.sharedExperts) +
        (uint64_t)cfg.intermediateDim * cfg.hiddenDim *           // Down
        (cfg.expertsPerToken + cfg.sharedExperts);

    double computeBoundTimeMs = (flopsPerToken / 1e12) / computeTFLOPS * 1000.0;

    // Take max of memory-bound and compute-bound
    est.tokenMs = std::max(memoryBoundTimeMs, computeBoundTimeMs) * cfg.numLayers;

    // Prefill is parallel across sequence
    est.prefillMs = est.tokenMs * promptLen / 4.0;  // ~4x parallelism

    // TPS
    est.tps = 1000.0 / est.tokenMs;

    est.memoryBandwidthGBps = memoryBandwidthGBps;

    return est;
}

LatencyEstimate LatencyEstimator::estimateCPU(
    const ModelConfig& cfg,
    uint32_t numThreads,
    uint32_t promptLen,
    uint32_t genLen
) {
    // Estimate for AVX-512 CPU: ~200 GFLOPS, ~50 GB/s memory BW
    double cpuGFLOPS = numThreads * 25.0;   // ~25 GFLOPS per core AVX-512
    double cpuBW = numThreads * 6.0;         // ~6 GB/s per core

    return estimate(cfg, cpuBW, cpuGFLOPS / 1000.0, promptLen, genLen);
}

} // namespace Prometheus
