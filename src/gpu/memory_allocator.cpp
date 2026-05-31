#include "gpu/memory_allocator.h"
#include "gpu/vulkan_context.h"
#include <new>

namespace RawrXD::GPU {
    MemoryAllocator& MemoryAllocator::getInstance() {
        static MemoryAllocator inst;
        return inst;
    }

    GPUMemoryBlock MemoryAllocator::allocate(VkDeviceSize size, VkBufferUsageFlags usage) {
        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer buffer = VK_NULL_HANDLE;
        if (vkCreateBuffer(VulkanContext::getInstance().device(), &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
            throw std::bad_alloc();

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(VulkanContext::getInstance().device(), buffer, &memReqs);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReqs.size;
        allocInfo.memoryTypeIndex = VulkanContext::getInstance().findMemoryType(
            memReqs.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VkDeviceMemory memory = VK_NULL_HANDLE;
        if (vkAllocateMemory(VulkanContext::getInstance().device(), &allocInfo, nullptr, &memory) != VK_SUCCESS) {
            vkDestroyBuffer(VulkanContext::getInstance().device(), buffer, nullptr);
            throw std::bad_alloc();
        }

        vkBindBufferMemory(VulkanContext::getInstance().device(), buffer, memory, 0);

        GPUMemoryBlock block{buffer, memory, size, 0};
        m_totalAllocated += size;
        return block;
    }

    void MemoryAllocator::free(GPUMemoryBlock& block) {
        if (block.memory) vkFreeMemory(VulkanContext::getInstance().device(), block.memory, nullptr);
        if (block.buffer) vkDestroyBuffer(VulkanContext::getInstance().device(), block.buffer, nullptr);
        if (block.size > 0) m_totalAllocated -= block.size;
        block = {};
    }

    void MemoryAllocator::defragment() {
        // Placeholder for defragmentation logic
    }

    size_t MemoryAllocator::getFreeMemory() const {
        // Placeholder - would query actual GPU memory
        return 0;
    }

    size_t MemoryAllocator::getTotalMemory() const {
        return m_totalAllocated;
    }
}
