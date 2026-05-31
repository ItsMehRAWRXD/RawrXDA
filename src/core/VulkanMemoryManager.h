#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include <mutex>
#include <string>

namespace RawrXD {
namespace Core {

class VulkanMemoryManager {
public:
    static VulkanMemoryManager& instance();

    bool Initialize();
    void Cleanup();

    // VRAM allocation/copy helpers
    VkBuffer CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkDeviceMemory& bufferMemory);
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size);
    
    // Low-level VRAM upload for GGUF tensors
    void* MapVRAM(VkDeviceMemory memory, VkDeviceSize size);
    void UnmapVRAM(VkDeviceMemory memory);
    
    VkDevice GetDevice() { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() { return m_physicalDevice; }
    VkQueue GetGraphicsQueue() { return m_graphicsQueue; }
    uint32_t GetMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    VulkanMemoryManager() = default;
    
    VkInstance m_instance = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkCommandPool m_commandPool = VK_NULL_HANDLE;
    bool m_initialized = false;
    std::mutex m_mutex;
};

} // namespace Core
} // namespace RawrXD
