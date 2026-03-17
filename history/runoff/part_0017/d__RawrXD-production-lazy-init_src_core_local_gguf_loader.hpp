// ============================================================================
// Local GGUF Loader - Production-Ready Direct File I/O
// ============================================================================
// Reads GGUF files directly from disk without cloud dependencies
// Supports GGUF version 3, all quantization types, full metadata extraction
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <optional>

namespace RawrXD {
namespace Core {

// ============================================================================
// GGUF CONSTANTS (GGUF Version 3)
// ============================================================================

constexpr uint32_t GGUF_MAGIC = 0x46554747;  // "GGUF" in little-endian
constexpr uint32_t GGUF_VERSION = 3;

// GGUF Value Types
enum class GGUFValueType : uint32_t {
    UINT8 = 0,
    INT8 = 1,
    UINT16 = 2,
    INT16 = 3,
    UINT32 = 4,
    INT32 = 5,
    FLOAT32 = 6,
    BOOL = 7,
    STRING = 8,
    ARRAY = 9,
    UINT64 = 10,
    INT64 = 11,
    FLOAT64 = 12
};

// GGUF Tensor Data Types (Quantization)
enum class GGUFTensorType : uint32_t {
    F32 = 0,
    F16 = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    Q2_K = 10,
    Q3_K = 11,
    Q4_K = 12,
    Q5_K = 13,
    Q6_K = 14,
    Q8_K = 15,
    IQ2_XXS = 16,
    IQ2_XS = 17,
    IQ3_XXS = 18,
    IQ1_S = 19,
    IQ4_NL = 20,
    IQ3_S = 21,
    IQ2_S = 22,
    IQ4_XS = 23,
    I8 = 24,
    I16 = 25,
    I32 = 26,
    I64 = 27,
    F64 = 28,
    IQ1_M = 29,
    BF16 = 30
};

// ============================================================================
// GGUF METADATA STRUCTURES
// ============================================================================

struct GGUFHeader {
    uint32_t magic;        // GGUF_MAGIC
    uint32_t version;      // GGUF_VERSION
    uint64_t tensor_count; // Number of tensors
    uint64_t metadata_kv_count; // Number of metadata key-value pairs
    
    bool isValid() const {
        return magic == GGUF_MAGIC && version == GGUF_VERSION;
    }
};

struct GGUFMetadataKV {
    uint64_t key_length;
    std::string key;
    GGUFValueType value_type;
    
    // Value (size depends on type)
    union {
        uint8_t uint8_value;
        int8_t int8_value;
        uint16_t uint16_value;
        int16_t int16_value;
        uint32_t uint32_value;
        int32_t int32_value;
        float float32_value;
        uint64_t uint64_value;
        int64_t int64_value;
        double float64_value;
        bool bool_value;
    } value;
    
    std::string string_value; // For STRING type
    std::vector<uint8_t> array_value; // For ARRAY type
};

struct GGUFTensorInfo {
    uint64_t name_length;
    std::string name;
    uint32_t n_dimensions;
    std::vector<uint64_t> dimensions;
    GGUFTensorType tensor_type;
    uint64_t offset; // Offset in file where tensor data starts
};

struct ModelMetadata {
    std::string path;
    std::string name;
    uint32_t context_length = 0;
    uint32_t hidden_size = 0;
    uint32_t head_count = 0;
    uint32_t head_count_kv = 0;
    uint32_t layer_count = 0;
    uint32_t vocab_size = 0;
    GGUFTensorType quant_type = GGUFTensorType::F16;
    uint64_t file_size_bytes = 0;
    std::string architecture; // "llama", "gemma", etc.
    float rope_theta = 10000.0f;
    uint32_t tensor_alignment = 32;
    bool is_valid = false;
    
    // Quantization info
    std::string quant_type_str() const;
    bool is_quantized() const;
    uint32_t bits_per_weight() const;
    
    // Validation
    bool validate() const;
    std::string toString() const;
};

// ============================================================================
// LOCAL GGUF LOADER CLASS
// ============================================================================

class LocalGGUFLoader {
public:
    LocalGGUFLoader() = default;
    ~LocalGGUFLoader() = default;
    
    // Non-copyable (file handles)
    LocalGGUFLoader(const LocalGGUFLoader&) = delete;
    LocalGGUFLoader& operator=(const LocalGGUFLoader&) = delete;
    
    // Load GGUF file from disk
    bool load(const std::string& file_path);
    
    // Get extracted metadata
    const ModelMetadata& getMetadata() const { return metadata_; }
    
    // Get tensor info (for advanced usage)
    const std::vector<GGUFTensorInfo>& getTensorInfo() const { return tensors_; }
    
    // Validation
    bool isLoaded() const { return file_ != nullptr; }
    bool validateFile(const std::string& file_path);
    
    // Error handling
    std::string getLastError() const { return last_error_; }
    bool hasError() const { return !last_error_.empty(); }
    
    // Static utilities
    static std::string tensorTypeToString(GGUFTensorType type);
    static uint32_t getBitsPerWeight(GGUFTensorType type);
    static bool isQuantized(GGUFTensorType type);
    
private:
    FILE* file_ = nullptr;
    GGUFHeader header_;
    ModelMetadata metadata_;
    std::vector<GGUFTensorInfo> tensors_;
    std::string last_error_;
    
    // Internal methods
    bool openFile(const std::string& path);
    void closeFile();
    bool readHeader();
    bool readMetadata();
    bool readTensorInfo();
    bool readString(std::string& out_str, uint64_t length);
    bool readValue(GGUFMetadataKV& kv);
    bool seekToTensorData(const GGUFTensorInfo& tensor);
    
    // Error reporting
    void setError(const std::string& error);
    void clearError();
};

// ============================================================================
// ERROR CODES
// ============================================================================

enum class GGUFLoderError {
    SUCCESS = 0,
    FILE_NOT_FOUND = 1,
    FILE_NOT_READABLE = 2,
    INVALID_MAGIC = 3,
    UNSUPPORTED_VERSION = 4,
    INVALID_TENSOR_COUNT = 5,
    INVALID_METADATA_COUNT = 6,
    READ_ERROR = 7,
    INVALID_TENSOR_TYPE = 8,
    INVALID_DIMENSION_COUNT = 9,
    INVALID_VALUE_TYPE = 10,
    OUT_OF_MEMORY = 11
};

} // namespace Core
} // namespace RawrXD