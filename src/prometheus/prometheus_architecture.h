#pragma once
#include "prometheus_config.h"
#include "prometheus_weight_loader.h"

#include <map>
#include <regex>
#include <string>
#include <vector>

namespace Prometheus {

// =============================================================================
// ARCHITECTURE DETECTION
// =============================================================================

enum class ModelArchitecture : uint8_t {
    Unknown = 0,
    Llama,
    LlamaMoE,
    Qwen,
    QwenMoE,
    Kimi,
    DeepSeek,
    DeepSeekMoE,
    Mixtral,
    Phi,
    Gemma,
    Gemma2,
    CommandR,
    StarCoder,
    Custom
};

struct DetectedArchitecture {
    ModelArchitecture arch = ModelArchitecture::Unknown;
    std::string modelName;
    std::string variant;
    uint32_t hiddenDim = 0;
    uint32_t numLayers = 0;
    uint32_t numHeads = 0;
    uint32_t numKVHeads = 0;
    uint32_t vocabSize = 0;
    uint32_t headDim = 128;
    bool isMoE = false;
    uint32_t numExperts = 0;
    uint32_t expertsPerToken = 0;
    uint32_t sharedExperts = 0;
    float ropeTheta = 10000.0f;
    float ropeScaleFactor = 1.0f;
    std::map<std::string, std::string> metadata;
};

struct GGUFMetadata {
    std::map<std::string, uint32_t> ints;
    std::map<std::string, float> floats;
    std::map<std::string, std::string> strings;
    std::map<std::string, bool> bools;
};

class ArchitectureDetector {
public:
    static DetectedArchitecture detectFromGGUF(const GGUFMetadata& meta);
    static DetectedArchitecture detectFromSafetensors(const std::string& indexPath);
    static ModelConfig createConfig(const DetectedArchitecture& detected);

private:
    static ModelArchitecture matchArchitecture(const std::string& modelName);
    static void applyMoESpecifics(DetectedArchitecture& detected, const GGUFMetadata& meta);
};

// =============================================================================
// TENSOR MAPPER
// =============================================================================

class TensorMapper {
public:
    enum class TensorType {
        Embedding,
        OutputHead,
        OutputNorm,
        AttentionQ,
        AttentionK,
        AttentionV,
        AttentionO,
        AttentionNorm,
        MoEGate,
        MoEExpertUp,
        MoEExpertGate,
        MoEExpertDown,
        MoESharedUp,
        MoESharedGate,
        MoESharedDown,
        FFNUp,
        FFNGate,
        FFNDown,
        FFNNorm
    };

    struct Mapping {
        std::string modelTensorName;
        std::string prometheusTensorName;
        uint32_t layerIdx = 0;
        TensorType type = TensorType::FFNUp;
    };

    static std::vector<Mapping> mapLlama(const GGUFMetadata& meta);
    static std::vector<Mapping> mapQwen(const GGUFMetadata& meta);
    static std::vector<Mapping> mapKimi(const GGUFMetadata& meta);
    static std::vector<Mapping> mapDeepSeek(const GGUFMetadata& meta);
    static std::vector<Mapping> mapMixtral(const GGUFMetadata& meta);
    static std::vector<Mapping> mapGeneric(const GGUFMetadata& meta);

private:
    static uint32_t getNumLayers(const GGUFMetadata& meta);
    static void addAttentionMappings(std::vector<Mapping>& out, uint32_t layerIdx,
                                      const std::string& prefix,
                                      const std::string& qName,
                                      const std::string& kName,
                                      const std::string& vName,
                                      const std::string& oName);
    static void addFFNMappings(std::vector<Mapping>& out, uint32_t layerIdx,
                                const std::string& prefix,
                                const std::string& gateName,
                                const std::string& upName,
                                const std::string& downName);
    static void addMoEMappings(std::vector<Mapping>& out, uint32_t layerIdx,
                                const std::string& prefix,
                                uint32_t numExperts,
                                const std::string& expertPrefix);
};

// =============================================================================
// QUANTIZATION HANDLER
// =============================================================================

enum class QuantizationType : uint8_t {
    FP32 = 0,
    FP16 = 1,
    BF16 = 2,
    Q4_0 = 3,
    Q4_1 = 4,
    Q5_0 = 5,
    Q5_1 = 6,
    Q8_0 = 7,
    Q4_K = 8,
    Q5_K = 9,
    Q6_K = 10,
    Q8_K = 11,
    IQ4_XS = 12,
    IQ4_NL = 13
};

class QuantizationHandler {
public:
    static void dequantizeQ4_K(const void* src, float* dst, size_t num);
    static void dequantizeQ5_K(const void* src, float* dst, size_t num);
    static void dequantizeQ6_K(const void* src, float* dst, size_t num);
    static void dequantizeQ8_0(const void* src, float* dst, size_t num);
    static void dequantizeFP16(const void* src, float* dst, size_t num);
    static void dequantizeBF16(const void* src, float* dst, size_t num);

    static size_t getQuantizedSize(size_t numElements, QuantizationType quantType);
    static size_t getBlockSize(QuantizationType quantType);
    static size_t getTypeSize(QuantizationType quantType);
};

// =============================================================================
// REAL MODEL LOADER
// =============================================================================

struct LoadOptions {
    bool verbose = false;
    bool validateChecksums = false;
    QuantizationType targetQuant = QuantizationType::FP16;
    uint32_t maxVRAM_MB = 0;  // 0 = unlimited
};

struct TensorInfo {
    std::string name;
    std::vector<uint32_t> shape;
    QuantizationType quantType;
    const void* data = nullptr;
    size_t bytes = 0;
};

class RealModelLoader {
public:
    struct LoadResult {
        bool success = false;
        std::string errorMessage;
        DetectedArchitecture architecture;
        ModelConfig config;
        std::map<std::string, TensorInfo> tensors;
        size_t totalBytes = 0;
        size_t quantizedBytes = 0;
        size_t overheadBytes = 0;
    };

    static LoadResult load(const std::string& path,
                           const LoadOptions& options = {});
    static LoadResult loadGGUF(const std::string& path,
                                const LoadOptions& options = {});
    static LoadResult loadSafetensors(const std::string& indexPath,
                                       const std::string& weightsPath,
                                       const LoadOptions& options = {});

    static bool validateModel(const LoadResult& result);
    static void printModelInfo(const LoadResult& result);

private:
    static LoadResult loadGGUFTensors(std::ifstream& file,
                                       const GGUFMetadata& metadata,
                                       const std::vector<TensorMapper::Mapping>& mappings,
                                       const LoadOptions& options);
};

} // namespace Prometheus
