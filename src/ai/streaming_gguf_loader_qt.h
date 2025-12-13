#pragma once
#include "memory_mapped_file.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <iostream>

/**
 * Enterprise-grade streaming GGUF loader with memory-mapped file access
 * Fixes metadata parsing bugs in original GGUFLoaderQt
 * Supports models from 3GB to 70GB without loading entire file into RAM
 */

// GGUF format constants
static const uint32_t GGUF_MAGIC = 0x46554747; // "GGUF" in little-endian
static const uint32_t GGUF_VERSION_3 = 3;

// GGUF value types (https://github.com/ggerganov/ggml/blob/master/docs/gguf.md)
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

// GGML tensor types
enum class GGMLType : uint32_t {
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

struct GGUFHeader {
    uint32_t magic;           // 0x46554747 ("GGUF")
    uint32_t version;         // Currently 3
    uint64_t tensor_count;
    uint64_t metadata_count;
};

struct GGUFMetadataValue {
    GGUFValueType type;
    std::vector<uint8_t> data;  // Raw value data
    
    // Convenience accessors
    std::string AsString() const;
    uint32_t AsUInt32() const;
    int32_t AsInt32() const;
    uint64_t AsUInt64() const;
    int64_t AsInt64() const;
    float AsFloat32() const;
    double AsFloat64() const;
    bool AsBool() const;
};

struct GGUFTensorInfo {
    std::string name;
    std::vector<uint64_t> shape;
    GGMLType type;
    size_t offset;        // Offset in file
    size_t size_bytes;    // Total size in bytes
    bool is_loaded = false;
};

class StreamingGGUFLoaderQt {
private:
    MemoryMappedFile mappedFile;
    GGUFHeader header;
    std::unordered_map<std::string, GGUFMetadataValue> metadata;
    std::vector<GGUFTensorInfo> tensors;
    std::unordered_map<std::string, size_t> tensorNameMap;
    
    size_t currentFileOffset = 0;
    bool isLoaded = false;
    
    // Parsing methods
    bool parseHeader();
    bool parseMetadata();
    bool parseTensorInfo();
    void buildTensorIndex();
    
    // Helper methods for reading from mapped memory
    template<typename T>
    bool readValue(T& value, size_t offset);
    
    bool readString(std::string& str, size_t offset, size_t& newOffset);
    size_t getTensorDataSize(GGMLType type, const std::vector<uint64_t>& shape) const;
    size_t getTypeSize(GGMLType type) const;
    
public:
    StreamingGGUFLoaderQt();
    ~StreamingGGUFLoaderQt();
    
    // Main loading interface
    bool loadModel(const std::string& filePath);
    void close();
    bool isModelLoaded() const { return isLoaded; }
    
    // Smart tensor access - loads on demand
    std::vector<uint8_t> getTensorData(const std::string& tensorName);
    std::vector<uint8_t> getTensorData(size_t tensorIndex);
    
    // Batch tensor operations
    std::vector<std::vector<uint8_t>> getMultipleTensors(const std::vector<std::string>& tensorNames);
    
    // Metadata access
    bool hasMetadata(const std::string& key) const;
    GGUFMetadataValue getMetadata(const std::string& key) const;
    std::vector<std::string> getAllMetadataKeys() const;
    
    // Model info
    size_t getTensorCount() const { return tensors.size(); }
    size_t getMetadataCount() const { return metadata.size(); }
    std::string getModelName() const;
    std::string getModelArchitecture() const;
    uint64_t getModelContextLength() const;
    
    // Memory statistics
    struct MemoryStats {
        size_t totalFileSize;
        size_t loadedTensorsCount;
        size_t totalTensorsCount;
    };
    
    MemoryStats getMemoryStats() const;
    
    // Error reporting
    std::string getLastError() const { return lastError; }
    
private:
    std::string lastError;
    void setError(const std::string& error);
};

// Template implementation
template<typename T>
inline bool StreamingGGUFLoaderQt::readValue(T& value, size_t offset) {
    const T* ptr = mappedFile.GetStruct<T>(offset);
    if (!ptr) {
        setError("Failed to read value at offset " + std::to_string(offset));
        return false;
    }
    value = *ptr;
    return true;
}
