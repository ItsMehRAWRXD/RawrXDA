#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <vulkan/vulkan.h>
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#pragma pack(push, 1)
struct GGUFHeader {
    uint32_t magic;      // "GGUF"
    uint32_t version;    // 3
    uint64_t tensor_count;
    uint64_t kv_count;
};

struct Q4_0_Block {
    uint16_t scale;      // FP16
    uint8_t qs[16];      // 32x 4-bit packed
};

struct Q5_0_Block {
    uint16_t scale;
    uint8_t qh[4];       // high bits (32x 1-bit)
    uint8_t qs[16];      // low 4 bits
};

struct Q8_0_Block {
    uint16_t scale;
    int8_t qs[32];
};

struct Q4_K_Block {
    uint16_t scale[2];   // super-block scales/mins (FP16)
    uint8_t scales[12];  // quantized sub-block scales (packed 6-bit)
    uint8_t qs[128];     // 256x 4-bit weights
};
#pragma pack(pop)

class RawrXDModelLoader {
    HANDLE hFile = INVALID_HANDLE_VALUE;
    HANDLE hMapping = nullptr;
    void* mappedView = nullptr;
    size_t fileSize = 0;
    
    VkDevice device = VK_NULL_HANDLE;
    VkPhysicalDeviceMemoryProperties memProps{};
    
public:
    struct Tensor {
        std::string name;
        uint32_t type;     // GGUF type enum
        std::vector<uint64_t> dims;
        size_t offset;
        void* data = nullptr;        // CPU mmap pointer
        VkBuffer gpuBuffer = VK_NULL_HANDLE;  // GPU memory
        VkDeviceMemory gpuMemory = VK_NULL_HANDLE;
        bool onGPU = false;
    };
    
    std::unordered_map<std::string, Tensor> tensors;
    uint32_t n_layers = 0;
    uint32_t n_heads = 0;
    uint32_t n_embd = 0;
    uint32_t n_vocab = 0;
    uint32_t n_ctx = 4096;
    
    struct GGUFMetadata {
       // Simplified metadata holder if needed
    };

    RawrXDModelLoader() = default;
    ~RawrXDModelLoader();

    bool Load(const wchar_t* path, VkDevice vkDevice, VkPhysicalDevice physDevice);
    
    // Helper to get metadata value (implementation detail: usually needs a generic KV store)
    // For now, assume simple integer or string retrieval or implement a basic one
    template<typename T>
    T GetMetadata(const std::string& key) {
        // Placeholder: Needs actual metadata map populated in Load
        // The detailed impl in prompt didn't show the metadata storage, just "ParseMetadata" call.
        // I will implement a basic map.
        return T(); 
    }
    
    // Explicit specialization placeholder for simple types
    uint32_t GetMetadataInt(const std::string& key);

private:
   std::unordered_map<std::string, uint32_t> intMetadata;

    uint8_t* ParseMetadata(uint8_t* ptr, uint64_t kv_count);
    uint8_t* ParseTensorInfo(uint8_t* ptr, Tensor& t);
    
    void LoadTensorAsync(Tensor& t);
    
    void DequantAndUploadQ4_0(Tensor& t, Q4_0_Block* blocks, size_t N);
    void DequantAndUploadQ5_0(Tensor& t, Q5_0_Block* blocks, size_t N);
    void DequantAndUploadQ8_0(Tensor& t, Q8_0_Block* blocks, size_t N);
    void DequantAndUploadQ4_K(Tensor& t, Q4_K_Block* blocks, size_t N);
    
    void UploadF32(Tensor& t, void* data, size_t N);
    void UploadF16(Tensor& t, void* data, size_t N);
    
    void CreateGPUBuffer(Tensor& t, void* data, size_t size);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags props);
    void UploadViaStaging(void* data, size_t size, VkBuffer dstBuffer);
    
    int64_t CalculateVRAMUsage();
};
