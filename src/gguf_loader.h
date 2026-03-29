#pragma once
#include "RawrXD_Interfaces.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <fstream>
#include <cstdint>
#include <variant>
#include <unordered_map>
#include <mutex>

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
using RawrXD::TensorInfo;
using RawrXD::GGUFMetadata;


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

class GGUFLoader : public RawrXD::IGGUFLoader {
public:
    GGUFLoader();
    virtual ~GGUFLoader();
    
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

    // Implementation of new methods — delegating to member data
    size_t GetTensorByteSize(const RawrXD::TensorInfo& tensor) const override { return tensor.size; }

    std::string GetTypeString(RawrXD::GGMLType type) const override {
        switch (type) {
            case RawrXD::GGMLType::F32:     return "f32";
            case RawrXD::GGMLType::F16:     return "f16";
            case RawrXD::GGMLType::Q4_0:    return "q4_0";
            case RawrXD::GGMLType::Q4_1:    return "q4_1";
            case RawrXD::GGMLType::Q5_0:    return "q5_0";
            case RawrXD::GGMLType::Q5_1:    return "q5_1";
            case RawrXD::GGMLType::Q8_0:    return "q8_0";
            case RawrXD::GGMLType::Q8_1:    return "q8_1";
            case RawrXD::GGMLType::Q2_K:    return "q2_k";
            case RawrXD::GGMLType::Q3_K:    return "q3_k";
            case RawrXD::GGMLType::Q4_K:    return "q4_k";
            case RawrXD::GGMLType::Q5_K:    return "q5_k";
            case RawrXD::GGMLType::Q6_K:    return "q6_k";
            case RawrXD::GGMLType::Q8_K:    return "q8_k";
            case RawrXD::GGMLType::I8:      return "i8";
            case RawrXD::GGMLType::I16:     return "i16";
            case RawrXD::GGMLType::I32:     return "i32";
            case RawrXD::GGMLType::I64:     return "i64";
            case RawrXD::GGMLType::F64:     return "f64";
            case RawrXD::GGMLType::F16_HALF:return "f16_half";
            case RawrXD::GGMLType::IQ2_XXS: return "iq2_xxs";
            case RawrXD::GGMLType::IQ2_XS:  return "iq2_xs";
            case RawrXD::GGMLType::IQ3_XXS: return "iq3_xxs";
            case RawrXD::GGMLType::IQ1_S:   return "iq1_s";
            case RawrXD::GGMLType::IQ4_NL:  return "iq4_nl";
            case RawrXD::GGMLType::IQ3_S:   return "iq3_s";
            case RawrXD::GGMLType::IQ2_S:   return "iq2_s";
            case RawrXD::GGMLType::IQ4_XS:  return "iq4_xs";
            case RawrXD::GGMLType::IQ1_M:   return "iq1_m";
            default: return "unknown";
        }
    }

    bool BuildTensorIndex() override {
        std::lock_guard<std::mutex> lock(tensorMutex);
        tensor_index_map_.clear();
        for (auto& t : tensors_) {
            tensor_index_map_[t.name] = &t;
        }
        return !tensors_.empty();
    }

    bool LoadZone(const std::string& zone_name, uint64_t max_memory_mb = 512) override {
        std::lock_guard<std::mutex> lock(tensorMutex);
        if (!is_open_ || !mappedView) return false;
        loaded_zones_.insert(zone_name);
        return true;
    }

    bool UnloadZone(const std::string& zone_name) override {
        std::lock_guard<std::mutex> lock(tensorMutex);
        return loaded_zones_.erase(zone_name) > 0;
    }
    bool LoadTensorZone(const std::string& tensor_name, std::vector<uint8_t>& data) override;
    uint64_t GetFileSize() const override;
    uint64_t GetCurrentMemoryUsage() const override {
        size_t usage = 0;
        for (const auto& t : tensors_) usage += t.size;
        return static_cast<uint64_t>(usage);
    }

    std::vector<std::string> GetLoadedZones() const override {
        return {loaded_zones_.begin(), loaded_zones_.end()};
    }

    std::vector<std::string> GetAllZones() const override {
        // Each tensor group prefix is a zone
        std::set<std::string> zones;
        for (const auto& t : tensors_) {
            auto dot = t.name.find('.');
            if (dot != std::string::npos) zones.insert(t.name.substr(0, dot));
            else zones.insert(t.name);
        }
        return {zones.begin(), zones.end()};
    }
    std::vector<RawrXD::TensorInfo> GetAllTensorInfo() const override { return tensors_; }

    virtual bool Load(VkDevice vkDevice, VkPhysicalDevice vkPhysDevice);
    virtual void CreateVulkanResources();

    enum class CompressionType { NONE, BRUTAL_GZIP, ZLIB, DEFLATE };
    virtual bool SetCompressionType(CompressionType type);
    virtual bool IsCompressed() const { return false; }
    virtual bool DecompressData(const std::vector<uint8_t>& in, std::vector<uint8_t>& out);
    virtual bool CompressData(const std::vector<uint8_t>& in, std::vector<uint8_t>& out);

    const void* GetBaseAddress() const { return mappedView; }

    struct UnsupportedTypeInfo {
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

    template<typename T>
    bool ReadValue(T& val);
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
    std::set<std::string> loaded_zones_;

    void* mappedView = nullptr;

    void LoadTensorAsync(RawrXD::TensorInfo& info);
    void UploadF32(RawrXD::TensorInfo& info, void* src, size_t count);
    void DequantAndUploadQ4_0(RawrXD::TensorInfo& info, void* src, size_t count);
    void BeginCommandBuffer();
    void EndCommandBuffer();
    uint32_t FindMemoryType(uint32_t typeFilter, uint32_t props);
    uint32_t FindQueueFamilyIndex(VkPhysicalDevice device, uint32_t queueFlags);
};
