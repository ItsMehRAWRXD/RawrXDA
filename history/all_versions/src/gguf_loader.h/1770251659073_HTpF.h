#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <fstream>
#include <unordered_map>

// Forward declarations to avoid heavy includes in header
namespace CPUInference {

// Compression type enumeration for GGUF tensors
enum class CompressionType : uint32_t {
    NONE = 0,
    DEFLATE = 1,
    BRUTAL_GZIP = 2,
    ZLIB = 3,
};

enum class GGMLType : uint32_t {
    F32 = 0,
    F16 = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q4_K = 10,
    Q5_K = 11,
    Q3_K = 12,
    Q2_K = 9,
    Q6_K = 13,
    Q8_0 = 7,
    Q5_1 = 5,
    F16_HALF = 4,
};

struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
    uint64_t metadata_offset;
};

struct TensorInfo {
    std::string name;
    std::vector<uint64_t> shape;
    GGMLType type;
    uint64_t offset;
    uint64_t size_bytes;
};

struct GGUFMetadata {
    std::map<std::string, std::string> kv_pairs;
    uint32_t architecture_type;
    uint32_t layer_count;
    uint32_t head_count;
    uint32_t context_length;
    uint32_t embedding_dim;
    uint32_t vocab_size;
    std::vector<std::string> tokens;
    std::vector<float> token_scores;
    std::vector<uint32_t> token_types;
};

class IGGUFLoader {
public:
    virtual ~IGGUFLoader() = default;
    
    // Core Lifecycle
    virtual bool Open(const std::string& filepath) = 0;
    virtual bool Close() = 0;
    
    // Metadata Parsing
    virtual bool ParseHeader() = 0;
    virtual GGUFHeader GetHeader() const = 0;
    virtual bool ParseMetadata() = 0;
    virtual GGUFMetadata GetMetadata() const = 0;
    
    // Tensor Access
    virtual std::vector<TensorInfo> GetTensorInfo() const = 0;
    virtual bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) = 0;
    virtual bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) = 0;
    virtual size_t GetTensorByteSize(const TensorInfo& tensor) const = 0;
    virtual std::string GetTypeString(GGMLType type) const = 0;
    virtual uint64_t GetFileSize() const = 0;
    
    // Streaming / Zone Management
    virtual bool BuildTensorIndex() = 0;
    virtual bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512) = 0;
    virtual bool UnloadZone(const std::string& zone_name) = 0;
    virtual std::vector<std::string> GetLoadedZones() const = 0;
    virtual std::vector<std::string> GetAllZones() const = 0;
    virtual std::vector<TensorInfo> GetAllTensorInfo() const = 0;
    virtual uint64_t GetCurrentMemoryUsage() const = 0;
};

} // namespace CPUInference
