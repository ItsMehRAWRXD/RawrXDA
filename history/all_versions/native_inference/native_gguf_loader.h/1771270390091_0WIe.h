// native_gguf_loader.h - Enhanced for RXUC-TerraForm Universal Compiler
// Compatible with C/Rust/Python/Go/JS/Zig/Nim/Carbon/Mojo syntax
// Zero dependencies, kernel-mode capable

#pragma once

#include <variant>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <fstream>

// GGUF Data Types
enum class QuantType {
    F32 = 0,
    F16 = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q4_K = 4,
    Q5_K = 5,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    Q8_K = 10,
    Q3_K = 11,
    Q2_K = 12,
    Q6_K = 13,
    F16_HALF = 14,
    // Add more as needed
};

// GGUF File Format Structures (GGUF v3 specification)
struct NativeGGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_count;
};

struct NativeGGUFTensorInfo {
    std::string name;
    uint32_t n_dims;
    std::vector<uint64_t> dims;
    QuantType type;
    uint64_t offset;  // Offset relative to tensor data start
    size_t size_bytes;
};

struct NativeGGUFMetadata {
    std::string key;
    uint32_t type;
    std::variant<uint8_t, int8_t, uint16_t, int16_t, uint32_t, int32_t, float, uint64_t, int64_t, double, bool, std::string, std::vector<uint8_t>> value;
};

// C-compatible interface
typedef struct NativeGGUFLoaderHandle* native_gguf_loader_t;

// Main Loader Class
class NativeGGUFLoader {
public:
    NativeGGUFLoader();
    ~NativeGGUFLoader();

    // File operations
    bool Open(const std::string& filepath);
    bool Close();

    // Header access
    const NativeGGUFHeader& GetHeader() const { return header_; }
    bool IsOpen() const { return is_open_; }

    // Metadata access
    const std::unordered_map<std::string, NativeGGUFMetadata>& GetMetadata() const { return metadata_; }

    // Tensor operations
    size_t GetTensorCount() const { return tensors_.size(); }
    const std::vector<NativeGGUFTensorInfo>& GetTensors() const { return tensors_; }
    const std::vector<NativeGGUFTensorInfo> GetTensorInfo() const { return tensors_; } // Compatibility
    const NativeGGUFTensorInfo* GetTensor(const std::string& name) const;

    // Data loading
    bool LoadTensorData(const std::string& name, void* buffer, size_t buffer_size);
    bool LoadTensorData(const NativeGGUFTensorInfo& tensor, void* buffer, size_t buffer_size);

    // Utility functions
    std::string GetFilePath() const { return filepath_; }
    uint64_t GetFileSize() const;

private:
    // Parsing functions
    bool ParseHeader();
    bool ParseMetadata();
    bool ParseTensorInfo();

    // Internal helpers
    template<typename T>
    bool ReadValue(T& value);
    bool ReadString(std::string& str);
    bool ReadArray(std::vector<uint8_t>& arr, uint32_t element_type, uint64_t length);

    // Member variables
    std::string filepath_;
    std::ifstream file_;
    bool is_open_;
    NativeGGUFHeader header_;
    std::unordered_map<std::string, NativeGGUFMetadata> metadata_;
    std::vector<NativeGGUFTensorInfo> tensors_;
    std::unordered_map<std::string, size_t> tensor_index_;
    uint64_t tensor_data_offset_;  // Start of tensor data section
    uint64_t alignment_ = 32;      // GGUF alignment
};

// Utility functions for GGUF format
namespace GGUFUtils {
    // Data type constants (GGUF format)
    enum DataType {
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
        IQ1_M = 29
    };

    // Metadata type constants
    enum MetadataType {
        UINT8 = 0,
        INT8 = 1,
        UINT16 = 2,
        INT16 = 3,
        UINT32 = 4,
        INT32 = 5,
        FLOAT32 = 6,
        UINT64 = 7,
        INT64 = 8,
        FLOAT64 = 9,
        BOOL = 10,
        STRING = 11,
        ARRAY = 12
    };

    // Magic number
    const uint32_t GGUF_MAGIC = 0x46554747; // "GGUF" in little endian

    // Version
    const uint32_t GGUF_VERSION = 3;

    // Get data type size in bytes
    size_t GetDataTypeSize(DataType type);

    // Get block size for quantized types
    size_t GetBlockSize(DataType type);

    // Check if data type is quantized
    bool IsQuantized(DataType type);
}

// TerraForm-compatible interface for custom compilers
extern "C" {
    // C interface for TerraForm integration
    typedef struct NativeGGUFLoaderHandle* native_gguf_loader_t;

    native_gguf_loader_t native_gguf_loader_create();
    void native_gguf_loader_destroy(native_gguf_loader_t loader);
    bool native_gguf_loader_open(native_gguf_loader_t loader, const char* filepath);
    bool native_gguf_loader_close(native_gguf_loader_t loader);
    size_t native_gguf_loader_get_tensor_count(native_gguf_loader_t loader);
    bool native_gguf_loader_load_tensor(native_gguf_loader_t loader, const char* name, void* buffer, size_t buffer_size);
}