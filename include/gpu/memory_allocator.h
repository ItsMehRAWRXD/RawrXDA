#pragma once
#include "gpu/vulkan_context.h"
#include <cstddef>

namespace RawrXD::GPU {

struct GPUMemoryBlock {
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
    VkDeviceSize offset = 0;
};

class MemoryAllocator {
public:
    static MemoryAllocator& getInstance();
    
    GPUMemoryBlock allocate(VkDeviceSize size, VkBufferUsageFlags usage);
    void free(GPUMemoryBlock& block);
    
    void defragment();
    size_t getFreeMemory() const;
    size_t getTotalMemory() const;

private:
    MemoryAllocator() = default;
    ~MemoryAllocator() = default;
    
    struct AllocationEntry {
        GPUMemoryBlock block;
        bool inUse = false;
    };
    
    std::vector<AllocationEntry> m_allocations;
    size_t m_totalAllocated = 0;
};

} // namespace RawrXD::GPU
