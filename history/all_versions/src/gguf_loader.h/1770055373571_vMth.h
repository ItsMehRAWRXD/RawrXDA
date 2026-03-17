#pragma once

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <cstdint>
#include <variant>
#include <unordered_map>
#include <mutex>

// We need windows.h for Handles in the Loader
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Vulkan Forward Declares to avoid full include in header
typedef struct VkDevice_T* VkDevice;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkBuffer_T* VkBuffer;
typedef struct VkDeviceMemory_T* VkDeviceMemory;
typedef struct VkQueue_T* VkQueue;
typedef struct VkCommandPool_T* VkCommandPool;
typedef struct VkCommandBuffer_T* VkCommandBuffer;


// Basic types for GGUF
enum class GGMLType : uint32_t {
    F32  = 0,
    F16  = 1,
    Q4_0 = 2,
    Q4_1 = 3,
    Q5_0 = 6,
    Q5_1 = 7,
    Q8_0 = 8,
    Q8_1 = 9,
    // K-Quants
    Q2_K = 10,
    Q3_K = 11,
    Q4_K = 12,
    Q5_K = 13,
    Q6_K = 14,
    Q8_K = 15,
    I8,
    I16,
    I32,
    COUNT 
};

struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t metadata_kv_count;
    uint64_t metadata_offset; // Added
};

struct TensorInfo {
    std::string name;
    std::vector<uint64_t> shape; // dims
    GGMLType type;
    uint64_t offset;
    size_t size;
    size_t size_bytes; // Added
    
    // GPU Resources
    void* cpuData = nullptr;
    VkBuffer gpuBuffer = nullptr; // VK_NULL_HANDLE
    VkDeviceMemory gpuMemory = nullptr; // VK_NULL_HANDLE
    bool onGPU = false;
};

struct GGUFMetadata {
    std::map<std::string, std::string> kv_pairs; 
    
    // Structured data extracted from KV pairs
    uint32_t architecture_type = 0;
    uint32_t layer_count = 0;
    uint32_t context_length = 0;
    uint32_t embedding_dim = 0;
    uint32_t vocab_size = 0;
    uint32_t head_count = 0;

    // Tokenizer data
    std::vector<std::string> tokens;
    std::vector<float> token_scores;
    std::vector<uint32_t> token_types;
    int32_t tokenizer_model_id = -1; // -1 for unknown/default
};


// Removed duplicate/corrupt GGUFLoader definition


// Interface for GGUF Loaders
struct IGGUFLoader {
    virtual ~IGGUFLoader() = default;
    
    virtual bool Open(const std::string& filepath) = 0;
    virtual bool Close() = 0;
    
    virtual bool ParseHeader() = 0;
    virtual GGUFHeader GetHeader() const = 0;
    
    virtual bool ParseMetadata() = 0;
    virtual GGUFMetadata GetMetadata() const = 0;
    
    virtual std::vector<TensorInfo> GetTensorInfo() const = 0;
    virtual bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) = 0;

    // Enhanced / Streaming Methods
    virtual size_t GetTensorByteSize(const TensorInfo& tensor) const { return 0; }
    virtual std::string GetTypeString(GGMLType type) const { return "unknown"; }
    virtual bool BuildTensorIndex() { return false; }
    virtual bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512) { return false; }
    virtual bool UnloadZone(const std::string& zone_name) { return false; }
    virtual bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) { return false; }
    virtual uint64_t GetFileSize() const { return 0; }
    virtual uint64_t GetCurrentMemoryUsage() const { return 0; }
    virtual std::vector<std::string> GetLoadedZones() const { return {}; }
    virtual std::vector<std::string> GetAllZones() const { return {}; }
    virtual std::vector<TensorInfo> GetAllTensorInfo() const { return GetTensorInfo(); }
};

class GGUFLoader : public IGGUFLoader {
public:
    GGUFLoader();
    virtual ~GGUFLoader();
    
    bool Open(const std::string& filepath) override;
    bool Close() override;
    
    bool ParseHeader() override;
    GGUFHeader GetHeader() const override { return header_; }
    
    bool ParseMetadata() override; // Implemented in cpp
    GGUFMetadata GetMetadata() const override { return metadata_; }
    
    std::vector<TensorInfo> GetTensorInfo() const override { return tensors_; }
    bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) override;

    // Helper for subclasses or internal use
    const void* GetBaseAddress() const { return mappedView; }

    template<typename T>
    bool ReadValue(T& val) {
        if (!file_.is_open()) return false;
        file_.read(reinterpret_cast<char*>(&val), sizeof(T));
        return file_.good();
    }

    // Nested types
    enum class CompressionType { NONE, BRUTAL_GZIP, ZLIB, DEFLATE, GZIP, ZSTD }; // Updated types
    struct UnsupportedTypeInfo {
        std::string typeName;
        std::string param;
    };

    // Missing overrides
    size_t GetTensorByteSize(const TensorInfo& tensor) const override;
    std::string GetTypeString(GGMLType type) const override;
    uint64_t GetFileSize() const override;
    bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) override;

    // Missing methods
    bool ReadString(std::string& value);
    uint64_t CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const;
    bool SetCompressionType(CompressionType type);
    bool DecompressData(const std::vector<uint8_t>& compressed, std::vector<uint8_t>& raw_data);
    bool CompressData(const std::vector<uint8_t>& raw_data, std::vector<uint8_t>& compressed);
    bool HasUnsupportedQuantizationTypes() const;
    std::vector<UnsupportedTypeInfo> GetUnsupportedQuantizationTypes() const;
    std::string GetRecommendedConversionType() const;

    // Load method from previous error log (Load(VkDevice...))
    bool Load(VkDevice vkDevice, VkPhysicalDevice vkPhysDevice);

    bool IsCompressed() const { return compression_type_ != CompressionType::None; }

private:
    VkDevice device;
    VkPhysicalDevice physDevice;
    HANDLE hFile = INVALID_HANDLE_VALUE;
    // size_t fileSize = 0; // GetFileSize is virtual, maybe use member? yes, fileSize used in cpp
    size_t fileSize = 0;
    HANDLE hMapping = nullptr;
    void* mappedView = nullptr;
    
    std::unordered_map<std::string, TensorInfo*> tensor_index_;
    CompressionType compression_type_ = CompressionType::None;

    // State members (matching cpp implementation)
    std::string filepath_;
    std::ifstream file_;
    bool is_open_ = false;
    
    GGUFHeader header_;
    GGUFMetadata metadata_;
    std::vector<TensorInfo> tensors_;
    std::vector<UnsupportedTypeInfo> unsupported_types_;

    // Vulkan objects used in cpp
    VkQueue transferQueue;
    VkCommandPool cmdPool;
    VkCommandBuffer cmdBuffer;
    std::mutex tensorMutex;

    // Internal methods
    void CreateVulkanResources();
    uint32_t FindQueueFamilyIndex(VkPhysicalDevice device, uint32_t queueFlags);
    uint32_t FindMemoryType(uint32_t typeFilter, uint32_t props);
    void LoadTensorAsync(TensorInfo& info);
    void UploadF32(TensorInfo& info, void* src, size_t count);
    void DequantAndUploadQ4_0(TensorInfo& info, void* src, size_t count);
    void BeginCommandBuffer();
    void EndCommandBuffer();
};
