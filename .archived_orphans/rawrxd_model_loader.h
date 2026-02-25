#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <windows.h>

// Conditional Vulkan: use real SDK if available, minimal stubs otherwise
#if __has_include(<vulkan/vulkan.h>)
#include <vulkan/vulkan.h>
#else
// Comprehensive Vulkan type stubs for compilation without SDK
// Uses a unique guard to prevent conflicts when included from rawrxd_inference.h
#ifndef RAWRXD_VK_FULL_STUBS_DEFINED
#define RAWRXD_VK_FULL_STUBS_DEFINED

#ifndef VK_NULL_HANDLE
#define VK_NULL_HANDLE nullptr
#endif
#ifndef VK_SUCCESS
#define VK_SUCCESS 0
#endif
#define VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO 12
#define VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO 5
#define VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO 39
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO 40
#define VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO 42
#define VK_STRUCTURE_TYPE_SUBMIT_INFO 4
#define VK_BUFFER_USAGE_STORAGE_BUFFER_BIT 0x00000020
#define VK_BUFFER_USAGE_TRANSFER_SRC_BIT 0x00000001
#define VK_BUFFER_USAGE_TRANSFER_DST_BIT 0x00000002
#define VK_SHARING_MODE_EXCLUSIVE 0
#define VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT 0x01
#define VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT 0x02
#define VK_MEMORY_PROPERTY_HOST_COHERENT_BIT 0x04
#define VK_COMMAND_POOL_CREATE_TRANSIENT_BIT 0x01
#define VK_COMMAND_BUFFER_LEVEL_PRIMARY 0
#define VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT 0x01
typedef void* VkBuffer;
typedef void* VkDevice;
typedef void* VkPhysicalDevice;
typedef void* VkDeviceMemory;
typedef void* VkQueue;
typedef void* VkCommandPool;
typedef void* VkCommandBuffer;
typedef uint32_t VkResult;
typedef uint32_t VkFlags;
typedef VkFlags VkMemoryPropertyFlags;
typedef VkFlags VkBufferUsageFlags;
struct VkMemoryType { VkMemoryPropertyFlags propertyFlags; uint32_t heapIndex; };
struct VkMemoryHeap { uint64_t size; VkFlags flags; };
struct VkPhysicalDeviceMemoryProperties { uint32_t memoryTypeCount; VkMemoryType memoryTypes[32]; uint32_t memoryHeapCount; VkMemoryHeap memoryHeaps[16]; };
struct VkBufferCreateInfo { uint32_t sType; const void* pNext{}; uint32_t flags{}; uint64_t size; VkBufferUsageFlags usage; uint32_t sharingMode; uint32_t queueFamilyIndexCount{}; const uint32_t* pQueueFamilyIndices{}; };
struct VkMemoryRequirements { uint64_t size; uint64_t alignment; uint32_t memoryTypeBits; };
struct VkMemoryAllocateInfo { uint32_t sType; const void* pNext{}; uint64_t allocationSize; uint32_t memoryTypeIndex; };
struct VkCommandPoolCreateInfo { uint32_t sType; const void* pNext{}; uint32_t flags; uint32_t queueFamilyIndex; };
struct VkCommandBufferAllocateInfo { uint32_t sType; const void* pNext{}; VkCommandPool commandPool; uint32_t level; uint32_t commandBufferCount; };
struct VkCommandBufferBeginInfo { uint32_t sType; const void* pNext{}; uint32_t flags; const void* pInheritanceInfo{}; };
struct VkBufferCopy { uint64_t srcOffset; uint64_t dstOffset; uint64_t size; };
struct VkSubmitInfo { uint32_t sType; const void* pNext{}; uint32_t waitSemaphoreCount{}; const void* pWaitSemaphores{}; const void* pWaitDstStageMask{}; uint32_t commandBufferCount; const VkCommandBuffer* pCommandBuffers; uint32_t signalSemaphoreCount{}; const void* pSignalSemaphores{}; };
inline VkResult vkCreateBuffer(VkDevice, const VkBufferCreateInfo*, const void*, VkBuffer*) { return 1; }
inline void vkGetBufferMemoryRequirements(VkDevice, VkBuffer, VkMemoryRequirements* r) { if(r) { r->size = 0; r->memoryTypeBits = 0; } }
inline VkResult vkAllocateMemory(VkDevice, const VkMemoryAllocateInfo*, const void*, VkDeviceMemory*) { return 1; }
inline VkResult vkBindBufferMemory(VkDevice, VkBuffer, VkDeviceMemory, uint64_t) { return 1; }
inline void vkGetPhysicalDeviceMemoryProperties(VkPhysicalDevice, VkPhysicalDeviceMemoryProperties* p) { if(p) p->memoryTypeCount = 0; }
inline VkResult vkMapMemory(VkDevice, VkDeviceMemory, uint64_t, uint64_t, uint32_t, void**) { return 1; }
inline void vkUnmapMemory(VkDevice, VkDeviceMemory) {}
inline void vkGetDeviceQueue(VkDevice, uint32_t, uint32_t, VkQueue*) {}
inline VkResult vkCreateCommandPool(VkDevice, const VkCommandPoolCreateInfo*, const void*, VkCommandPool*) { return 1; }
inline VkResult vkAllocateCommandBuffers(VkDevice, const VkCommandBufferAllocateInfo*, VkCommandBuffer*) { return 1; }
inline VkResult vkBeginCommandBuffer(VkCommandBuffer, const VkCommandBufferBeginInfo*) { return 1; }
inline void vkCmdCopyBuffer(VkCommandBuffer, VkBuffer, VkBuffer, uint32_t, const VkBufferCopy*) {}
inline VkResult vkEndCommandBuffer(VkCommandBuffer) { return 1; }
inline VkResult vkQueueSubmit(VkQueue, uint32_t, const VkSubmitInfo*, void*) { return 1; }
inline VkResult vkQueueWaitIdle(VkQueue) { return 1; }
inline void vkFreeCommandBuffers(VkDevice, VkCommandPool, uint32_t, const VkCommandBuffer*) {}
inline void vkDestroyCommandPool(VkDevice, VkCommandPool, const void*) {}
inline void vkDestroyBuffer(VkDevice, VkBuffer, const void*) {}
inline void vkFreeMemory(VkDevice, VkDeviceMemory, const void*) {}
#endif
#endif

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

// GGUF quantization block structures
#pragma pack(push, 1)
struct Q4_0_Block {
    uint16_t d; // float16 scale
    uint8_t qs[16]; // 32 x 4-bit nibbles
};

struct Q5_0_Block {
    uint16_t d;       // float16 scale
    uint8_t qh[4];    // 32 x 1-bit high bits
    uint8_t qs[16];   // 32 x 4-bit low nibbles
};

struct Q8_0_Block {
    uint16_t d;        // float16 scale
    int8_t qs[32];     // 32 x 8-bit quants
};

struct Q4_K_Block {
    uint16_t d;        // float16 super-block scale
    uint16_t dmin;     // float16 super-block min
    uint8_t scales[12]; // sub-block scales (packed 6-bit)
    uint8_t qs[128];   // 256 x 4-bit weights
};
#pragma pack(pop)

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
    int n_embd = 4096;
    int n_layers = 32;
    int n_heads = 32;
    int n_heads_kv = 32;
    int n_ctx = 4096;
    int vocab_size = 32000;

public:
    int getDim() const { return n_embd; }
    int getLayers() const { return n_layers; }
    int getHeads() const { return n_heads; }
    int getKVHeads() const { return n_heads_kv; }
    int getCtx() const { return n_ctx; }
    int getVocabSize() const { return vocab_size; }
    
    std::unordered_map<std::string, Tensor> tensors;
    
    // Helpers
    uint8_t* ParseMetadata(uint8_t* ptr, uint64_t count);
    uint8_t* ParseTensorInfo(uint8_t* ptr, Tensor& t);
    void LoadTensorAsync(Tensor& t);
    void DequantAndUploadQ4_0(Tensor& t, void* blocks, size_t N);
    void DequantAndUploadF16(Tensor& t, void* data, size_t N);
    void DequantAndUploadQ5_0(Tensor& t, void* blocks, size_t N);
    void DequantAndUploadQ8_0(Tensor& t, void* blocks, size_t N);
    void DequantAndUploadQ4_K(Tensor& t, void* blocks, size_t N);
    void UploadF32(Tensor& t, void* data, size_t N);
    void CreateGPUBuffer(Tensor& t, void* data, size_t size);
    void UploadViaStaging(void* data, size_t size, VkBuffer dstBuffer);
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    int64_t CalculateVRAMUsage();

    // CPU-side F16 to F32 helper
    static float HalfToFloat(uint16_t h);
    static uint16_t FloatToHalf(float f);
};
