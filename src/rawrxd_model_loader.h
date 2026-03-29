#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>
#include <string>
#include <functional>
#ifdef RAWR_ENABLE_VULKAN
#include <vulkan/vulkan.h>
#else
// Standard Win32/CPU build - Vulkan handles not needed
#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE 0
#endif
typedef void* VkBuffer;
typedef void* VkDeviceMemory;
typedef void* VkDevice;
typedef void* VkPhysicalDevice;
typedef struct { uint32_t memoryTypeCount; } VkPhysicalDeviceMemoryProperties;
typedef uint32_t VkMemoryPropertyFlags;
#endif
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

// struct GGUFHeader removed - use definition in RawrXD_Interfaces.h via gguf_loader.h if needed
// Or rely on the fact that RawrXD_Interfaces.h is the source of truth.

// GGUF Q4_0 block structure
struct Q4_0_Block {
    uint16_t d; // float16 scale
    uint8_t qs[16]; // 32 nibbles
};

class RawrXDModelLoader {
public:
    using ModelLoadErrorCallback = std::function<void(const std::string& stage, const std::string& message)>;

    bool Load(const wchar_t* path, VkDevice device, VkPhysicalDevice physDevice);
    float* GetTensor(const std::string& name);
    void SetLoadErrorCallback(ModelLoadErrorCallback callback);
    const std::string& GetLastLoadErrorMessage() const;
    
private:
    VkDevice device;
    VkPhysicalDeviceMemoryProperties memProps;
    HANDLE hFile, hMapping;
    void* mappedView;
    uint64_t fileSize;
    
    // Metadata
    int n_embd = 4096;
    int n_layers = 32;
    int n_heads = 32;
    int n_heads_kv = 32;
    int n_ctx = 4096;
    int vocab_size = 32000;
    int n_ffn = 0;  // feed_forward_length (0 = infer from dim*4)
    std::string metadataArchitecture;
    std::string metadataTokenizerModel;
    uint32_t metadataFileType = 0xFFFFFFFFu;  // GGUF file_type identifier
    bool m_gpuUploadEnabled = true;
    std::string m_lastLoadErrorStage;
    std::string m_lastLoadErrorMessage;
    ModelLoadErrorCallback m_loadErrorCallback;

public:
    int getDim() const { return n_embd; }
    int getLayers() const { return n_layers; }
    int getHeads() const { return n_heads; }
    int getKVHeads() const { return n_heads_kv; }
    int getCtx() const { return n_ctx; }
    int getVocabSize() const { return vocab_size; }
    int getFFNDim() const { return n_ffn; }
    
    std::unordered_map<std::string, Tensor> tensors;
    
    // Helpers
    uint8_t* ParseMetadata(uint8_t* ptr, uint64_t count);
    uint8_t* ParseTensorInfo(uint8_t* ptr, Tensor& t);
    void LoadTensorAsync(Tensor& t);
    void DequantAndUploadQ4_0(Tensor& t, void* blocks, size_t N);
    void DequantAndUploadQ8_0(Tensor& t, void* blocks, size_t N);
    void DequantAndUploadQ4_K(Tensor& t, void* blocks, size_t N);
    void UploadF32(Tensor& t, void* data, size_t N);
    void CreateGPUBuffer(Tensor& t, void* data, size_t size);
    void UploadViaStaging(void* data, size_t size, VkBuffer dstBuffer);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    int64_t CalculateVRAMUsage();
    
    // Backend mode and file type validation
    bool IsSupportedFileType(uint32_t fileType) const;
    bool ResolveBackendModeAndPreflight(const wchar_t* path, uint64_t modelBytes, std::string& lane, std::string& reason);
};
