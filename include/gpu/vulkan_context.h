#pragma once
#include <vulkan/vulkan.h>
#include <cstdint>
#include <stdexcept>

namespace RawrXD::GPU {

class VulkanContext {
public:
    static VulkanContext& getInstance();
    
    bool initialize();
    void shutdown();
    
    bool isInitialized() const { return m_initialized; }
    
    VkInstance instance() const { return m_instance; }
    VkDevice device() const { return m_device; }
    VkPhysicalDevice physicalDevice() const { return m_physicalDevice; }
    VkQueue computeQueue() const { return m_computeQueue; }
    uint32_t queueFamilyIndex() const { return m_queueFamilyIndex; }
    
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);

private:
    VulkanContext() = default;
    ~VulkanContext() { if (m_initialized) shutdown(); }
    
    VulkanContext(const VulkanContext&) = delete;
    VulkanContext& operator=(const VulkanContext&) = delete;
    
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkQueue m_computeQueue = VK_NULL_HANDLE;
    uint32_t m_queueFamilyIndex = 0;
    bool m_initialized = false;
};

} // namespace RawrXD::GPU
