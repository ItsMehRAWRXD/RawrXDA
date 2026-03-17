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
};

struct TensorInfo {
    std::string name;
    std::vector<uint64_t> shape; // dims
    GGMLType type;
    uint64_t offset;
    size_t size;
    
    // GPU Resources
    void* cpuData = nullptr;
    VkBuffer gpuBuffer = nullptr; // VK_NULL_HANDLE
    VkDeviceMemory gpuMemory = nullptr; // VK_NULL_HANDLE
    bool onGPU = false;
};

struct GGUFMetadata {
    // ...existing code...
    uint32_t type;
    uint32_t length;
    uint64_t offset;
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

protected:
    std::string filepath_;
    std::ifstream file_;
    bool is_open_;
    
    GGUFHeader header_;
    GGUFMetadata metadata_;
    std::vector<TensorInfo> tensors_;
    std::vector<std::string> unsupported_types_;
};
