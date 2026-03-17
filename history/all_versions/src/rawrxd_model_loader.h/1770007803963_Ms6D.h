#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <vulkan/vulkan.h>
#include <windows.h>

struct Tensor {
    std::string name;
    std::vector<uint64_t> dims;
    uint32_t type;
    uint64_t offset;
    void* data; // Mapped pointer (Raw)
    
    // Vulkan
    VkBuffer gpuBuffer = VK_NULL_HANDLE;
    VkDeviceMemory gpuMemory = VK_NULL_HANDLE;
    bool onGPU = false;
    
    // CPU Float cache for reference implementation
    std::vector<float> cpuFloatData;
};

struct GGUFHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t tensor_count;
    uint64_t kv_count;
};

// GGUF Q4_0 block structure
struct Q4_0_Block {
    uint16_t d; // float16 scale
    uint8_t qs[16]; // 32 nibbles
};

class RawrXDModelLoader {
public:
    bool Load(const wchar_t* path, VkDevice device, VkPhysicalDevice physDevice);
    float* GetTensor(const std::string& name);
    
private:
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memProps;
    HANDLE hFile, hMapping;
    void* mappedView;
    uint64_t fileSize;
    
    // Metadata
    int n_embd, n_layers, n_heads, n_heads_kv, n_ctx;
    
    std::unordered_map<std::string, Tensor> tensors;
    
    // Helpers
    uint8_t* ParseMetadata(uint8_t* ptr, uint64_t count);
    uint8_t* ParseTensorInfo(uint8_t* ptr, Tensor& t);
    void LoadTensorAsync(Tensor& t);
    void DequantAndUploadQ4_0(Tensor& t, void* blocks, size_t N);
    void UploadF32(Tensor& t, void* data, size_t N);
    void CreateGPUBuffer(Tensor& t, void* data, size_t size);
    void UploadViaStaging(void* data, size_t size, VkBuffer dstBuffer);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    int64_t CalculateVRAMUsage();
};
