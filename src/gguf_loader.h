#pragma once
#include "RawrXD_Interfaces.h"
#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>


// We need windows.h for Handles in the Loader
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#ifdef RAWR_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#else
// Fake Vulkan types for interface compatibility
typedef void* VkDevice;
typedef void* VkPhysicalDevice;
typedef void* VkQueue;
typedef void* VkCommandPool;
typedef void* VkCommandBuffer;
#define VK_QUEUE_TRANSFER_BIT 0x01
#endif

// Basic types for GGUF (defined in RawrXD_Interfaces.h)
using RawrXD::GGMLType;
using RawrXD::GGUFHeader;
using RawrXD::GGUFMetadata;
using RawrXD::TensorInfo;


/*
class GGUFLoaderVulkan {
public:
    GGUFLoaderVulkan();
    ~GGUFLoaderVulkan();

    bool Open(const std::string& filepath);
    void Close();

    // Original API
    bool ParseHeader();

    // New API from user request (adapted)
    bool Load(VkDevice vkDevice, VkPhysicalDevice vkPhysDevice);

    // Helpers
    uint64_t GetMetadata(const std::string& key);
    TensorInfo& GetTensor(const std::string& name);

private:
    std::ifstream file_;
    std::string filepath_;
    bool is_open_;

    GGUFHeader header_val; // Renamed to avoid collision with struct type

    // Handles for Memory Mapping
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = nullptr;
    void* mappedView = nullptr;
    size_t fileSize = 0;

    // Vulkan Context
    VkDevice device;
    VkPhysicalDevice physDevice;
    VkQueue transferQueue;
    VkCommandPool cmdPool;
    VkCommandBuffer cmdBuffer;

    std::mutex tensorMutex;
    std::unordered_map<std::string, TensorInfo> tensors;

    // Internal loading methods
    void CreateVulkanResources();
    void LoadTensorAsync(TensorInfo& info);
    void UploadF32(TensorInfo& info, void* src, size_t count);
    void DequantAndUploadQ4_0(TensorInfo& info, void* src, size_t count);
    // ... Add others as needed, simplified for this integration

    void BeginCommandBuffer();
    void EndCommandBuffer();
    uint32_t FindMemoryType(uint32_t typeFilter, uint32_t props);
    uint32_t FindQueueFamilyIndex(VkPhysicalDevice device, uint32_t queueFlags);
};
*/

// Interface for GGUF Loaders
// Deprecated in favor of RawrXD::IGGUFLoader, keeping for local compatibility if needed
// but directing everything to the RawrXD namespace versions.
typedef RawrXD::IGGUFLoader IGGUFLoader;

class GGUFLoader : public RawrXD::IGGUFLoader
{
  public:
    GGUFLoader();
    virtual ~GGUFLoader();

    static uint64_t AlignTo32Bytes(uint64_t offset) { return (offset + 31ULL) & ~31ULL; }

    bool Open(const std::string& filepath) override;
    bool Close() override;

    bool ParseHeader() override;
    RawrXD::GGUFHeader GetHeader() const override { return header_; }

    bool ParseMetadata() override;
    RawrXD::GGUFMetadata GetMetadata() const override { return metadata_; }

    // Lane E: lightweight integrity checks and trivial repair path.
    bool VerifyIntegrity(std::string* reason = nullptr);
    bool RepairTrivialIssues(std::string* report = nullptr);

    std::vector<RawrXD::TensorInfo> GetTensorInfo() const override { return tensors_; }
    bool LoadTensorRange(size_t start_idx, size_t count, std::vector<uint8_t>& data) override;

    // Implementation of new methods to avoid abstract class issues
    size_t GetTensorByteSize(const RawrXD::TensorInfo& tensor) const override { return tensor.size; }
    std::string GetTypeString(RawrXD::GGMLType type) const override { return "f32"; }
    bool BuildTensorIndex() override { return true; }
    bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512) override { return true; }
    bool UnloadZone(const std::string& zone_name) override { return true; }
    bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) override;
    uint64_t GetFileSize() const override;
    uint64_t GetCurrentMemoryUsage() const override { return 0; }
    std::vector<std::string> GetLoadedZones() const override { return {}; }
    std::vector<std::string> GetAllZones() const override { return {}; }
    std::vector<RawrXD::TensorInfo> GetAllTensorInfo() const override { return tensors_; }

    virtual bool Load(VkDevice vkDevice, VkPhysicalDevice vkPhysDevice);
    virtual void CreateVulkanResources();

    enum class CompressionType
    {
        NONE,
        BRUTAL_GZIP,
        ZLIB,
        DEFLATE
    };
    virtual bool SetCompressionType(CompressionType type);
    virtual bool IsCompressed() const { return false; }
    virtual bool DecompressData(const std::vector<uint8_t>& in, std::vector<uint8_t>& out);
    virtual bool CompressData(const std::vector<uint8_t>& in, std::vector<uint8_t>& out);

    const void* GetBaseAddress() const { return mappedView; }

    struct UnsupportedTypeInfo
    {
        uint32_t type_value;
        std::string type_name;
        std::vector<std::string> tensor_names;
    };

    virtual bool HasUnsupportedQuantizationTypes() const;
    virtual std::vector<UnsupportedTypeInfo> GetUnsupportedQuantizationTypes() const;
    virtual std::string GetRecommendedConversionType() const;

    std::vector<UnsupportedTypeInfo> unsupported_types_structs_;
    std::map<std::string, RawrXD::TensorInfo*> tensor_index_map_;

    VkDevice device = nullptr;
    VkPhysicalDevice physDevice = nullptr;
    VkQueue transferQueue = nullptr;
    VkCommandPool cmdPool = nullptr;
    VkCommandBuffer cmdBuffer = nullptr;

    // Windows Handles for MappedView
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = nullptr;
    uint64_t fileSize = 0;

    CompressionType compression_type_ = CompressionType::NONE;
    std::mutex tensorMutex;

    template <typename T> bool ReadValue(T& val);
    bool ReadString(std::string& str);
    size_t CalculateTensorSize(const std::vector<uint64_t>& shape, RawrXD::GGMLType type) const;

  protected:
    std::string filepath_;
    std::ifstream file_;
    bool is_open_;

    RawrXD::GGUFHeader header_;
    RawrXD::GGUFMetadata metadata_;
    std::vector<RawrXD::TensorInfo> tensors_;
    std::vector<std::string> unsupported_types_;

    void* mappedView = nullptr;

    void LoadTensorAsync(RawrXD::TensorInfo& info);
    void UploadF32(RawrXD::TensorInfo& info, void* src, size_t count);
    void DequantAndUploadQ4_0(RawrXD::TensorInfo& info, void* src, size_t count);
    void BeginCommandBuffer();
    void EndCommandBuffer();
    uint32_t FindMemoryType(uint32_t typeFilter, uint32_t props);
    uint32_t FindQueueFamilyIndex(VkPhysicalDevice device, uint32_t queueFlags);
};
