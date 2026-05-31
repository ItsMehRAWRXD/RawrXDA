/**
 * @file model_loader.h
 * @brief Enhanced model loading with quantization support
 * 
 * @author RawrXD Inference Team
 * @version 1.0.0
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <functional>
#include <variant>

namespace RawrXD::Inference {

// ============================================================================
// Model Format
// ============================================================================

enum class ModelFormat {
    Unknown,
    GGUF,
    PyTorch,
    SafeTensors,
    ONNX
};

// ============================================================================
// Tensor Type
// ============================================================================

enum class TensorType {
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    F16 = 10,
    F32 = 0
};

// ============================================================================
// Quantization Type
// ============================================================================

enum class QuantizationType {
    None,
    Q4_0,
    Q4_1,
    Q5_0,
    Q5_1,
    Q8_0,
    F16
};

// ============================================================================
// Metadata Type
// ============================================================================

enum class MetadataType {
    Uint8 = 0,
    Int8 = 1,
    Uint16 = 2,
    Int16 = 3,
    Uint32 = 4,
    Int32 = 5,
    Float32 = 6,
    Bool = 7,
    String = 8,
    Array = 9,
    Uint64 = 10,
    Int64 = 11,
    Float64 = 12
};

// ============================================================================
// Metadata Entry
// ============================================================================

struct MetadataEntry {
    std::string key;
    MetadataType type;
    std::variant<uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t,
               float, bool, std::string, uint64_t, int64_t, double> value;
};

// ============================================================================
// Tensor Info
// ============================================================================

struct TensorInfo {
    std::string name;
    std::vector<uint64_t> dimensions;
    TensorType type;
    uint64_t offset;
    std::vector<uint8_t> data;
};

// ============================================================================
// GGUF Header
// ============================================================================

struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensorCount;
    uint64_t metadataCount;
};

// ============================================================================
// Model
// ============================================================================

struct Model {
    std::string name;
    std::string architecture;
    uint32_t version = 0;
    ModelFormat format = ModelFormat::Unknown;
    QuantizationType quantizationType = QuantizationType::None;
    uint32_t quantizationVersion = 0;
    
    uint32_t contextLength = 0;
    uint32_t embeddingLength = 0;
    uint32_t blockCount = 0;
    uint32_t feedForwardLength = 0;
    uint32_t attentionHeadCount = 0;
    uint32_t vocabSize = 0;
    
    std::map<std::string, MetadataEntry> metadata;
    std::map<std::string, TensorInfo> tensors;
};

// ============================================================================
// Model Info
// ============================================================================

struct ModelInfo {
    std::string name;
    std::string architecture;
    uint32_t version = 0;
    ModelFormat format = ModelFormat::Unknown;
    QuantizationType quantizationType = QuantizationType::None;
    uint32_t contextLength = 0;
    uint32_t embeddingLength = 0;
    uint32_t blockCount = 0;
    uint32_t attentionHeadCount = 0;
    uint32_t vocabSize = 0;
    size_t totalSize = 0;
    int tensorCount = 0;
};

// ============================================================================
// Loader Configuration
// ============================================================================

struct LoaderConfig {
    bool useMemoryMapping = true;
    bool verifyChecksum = false;
    size_t maxMemoryUsage = 0; // 0 = unlimited
};

// ============================================================================
// Progress Callback
// ============================================================================

using ProgressCallback = std::function<void(const std::string& tensorName, float progress)>;

// ============================================================================
// Model Loader
// ============================================================================

class ModelLoader {
public:
    explicit ModelLoader(const LoaderConfig& config);
    ~ModelLoader();
    
    // Model loading
    bool loadModel(const std::string& path, Model& model);
    bool loadGGUF(const std::string& path, Model& model);
    bool loadPyTorch(const std::string& path, Model& model);
    bool loadSafeTensors(const std::string& path, Model& model);
    
    // Quantization
    bool quantizeModel(const Model& input, Model& output, QuantizationType type);
    
    // Progress callback
    void setProgressCallback(ProgressCallback callback);
    
    // Model info
    ModelInfo getModelInfo(const Model& model) const;
    
private:
    void extractCommonMetadata(Model& model);
    size_t calculateTensorSize(const TensorInfo& tensor);
    bool shouldQuantizeTensor(const std::string& name, const TensorInfo& tensor);
    bool quantizeTensor(TensorInfo& tensor, QuantizationType type);
    
    LoaderConfig m_config;
    mutable std::mutex m_mutex;
    ProgressCallback m_progressCallback;
};

} // namespace RawrXD::Inference
