#pragma once
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <fstream>
#include <unordered_map>

class VulkanCompute;
struct VulkanTensor;

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
    uint32_t context_length;
    uint32_t embedding_dim;
    uint32_t vocab_size;
    std::vector<std::string> tokens;
};

class IGGUFLoader {
public:
    virtual ~IGGUFLoader() = default;
    virtual bool Open(const std::string& filepath) = 0;
    virtual bool Close() = 0;
    virtual bool ParseHeader() = 0;
    virtual GGUFHeader GetHeader() const = 0;
    virtual bool ParseMetadata() = 0;
    virtual GGUFMetadata GetMetadata() const = 0;
    virtual std::vector<TensorInfo> GetTensorInfo() const = 0;
    virtual bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) = 0;
    virtual bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) = 0;
    virtual size_t GetTensorByteSize(const TensorInfo& tensor) const = 0;
    virtual std::string GetTypeString(GGMLType type) const = 0;
    virtual uint64_t GetFileSize() const = 0;
    // Streaming friendly methods (no-op for non-streaming loader)
    virtual bool BuildTensorIndex() = 0;
    virtual bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512) = 0;
    virtual bool UnloadZone(const std::string& zone_name) = 0;
    virtual std::vector<std::string> GetLoadedZones() const = 0;
    virtual std::vector<std::string> GetAllZones() const = 0;
    virtual std::vector<TensorInfo> GetAllTensorInfo() const = 0;
    virtual uint64_t GetCurrentMemoryUsage() const = 0;
};

class GGUFLoader : public IGGUFLoader {
public:
    GGUFLoader();
    ~GGUFLoader();

    bool Open(const std::string& filepath) override;
    bool Close() override;
    
    // Header operations
    bool ParseHeader() override;
    GGUFHeader GetHeader() const override { return header_; }
    
    // Metadata operations
    bool ParseMetadata() override;
    GGUFMetadata GetMetadata() const override { return metadata_; }
    const std::vector<std::string>& GetVocabulary() const { return metadata_.tokens; }
    
    // Tensor operations
    std::vector<TensorInfo> GetTensorInfo() const override { return tensors_; }
    bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) override;
    bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) override;
    void AttachVulkanEngine(VulkanCompute* engine) { vulkan_engine_ = engine; }
    bool UploadAllTensorsToVulkan();
    bool UploadTensorToVulkan(const std::string& tensor_name);
    const std::unordered_map<std::string, VulkanTensor>& GetVulkanTensors() const { return vulkan_tensors_; }
    
    // Utility functions
    size_t GetTensorByteSize(const TensorInfo& tensor) const override;
    std::string GetTypeString(GGMLType type) const override;
    uint64_t GetFileSize() const override;
    
    // Streaming interface stubs (non-streaming loader - minimal implementations)
    bool BuildTensorIndex() override { return true; }  // Already built during ParseHeader
    bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512) override { return true; }
    bool UnloadZone(const std::string& zone_name) override { return true; }
    std::vector<std::string> GetLoadedZones() const override { return {"all"}; }
    std::vector<std::string> GetAllZones() const override { return {"all"}; }
    std::vector<TensorInfo> GetAllTensorInfo() const override { return tensors_; }
    uint64_t GetCurrentMemoryUsage() const override { return 0; }

private:
    std::string filepath_;
    mutable std::ifstream file_;
    GGUFHeader header_;
    GGUFMetadata metadata_;
    std::vector<TensorInfo> tensors_;
    bool is_open_;
    VulkanCompute* vulkan_engine_{nullptr};
    std::unordered_map<std::string, VulkanTensor> vulkan_tensors_;
    bool use_dummy_mode_{false};  // Skip tensor loading for huge files
    uint64_t file_size_{0};
    
    // Memory-mapped file support (Windows)
    void* mmap_base_{nullptr};
    void* file_handle_{nullptr};  // HANDLE on Windows
    void* map_handle_{nullptr};   // HANDLE on Windows
    bool use_mmap_{false};
    
    // Internal parsing helpers
    template<typename T>
    bool ReadValue(T& value);
    bool ReadString(std::string& value);
    bool ReadMetadataKV(const std::string& key, std::string& value);
    uint64_t CalculateTensorSize(const std::vector<uint64_t>& shape, GGMLType type) const;
    bool CreateDummyModel();
    bool InitializeMemoryMap();
    void CleanupMemoryMap();
    const void* GetMappedSlice(uint64_t offset, uint64_t size) const;
};
