#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <map>
#include <memory>
#include <cstdint>
#include <QMutex>
#include <QWaitCondition>

namespace RawrXD {
namespace GPU {

// GPU Memory Pool Statistics
struct MemoryStats {
    uint64_t total_allocated = 0;
    uint64_t total_used = 0;
    uint64_t total_free = 0;
    uint32_t allocation_count = 0;
    uint32_t deallocation_count = 0;
    uint32_t fragmentation_ratio = 0;
};

// GPU Memory Block Descriptor
struct MemoryBlock {
    uint64_t offset = 0;
    uint64_t size = 0;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void* cpu_mapped_ptr = nullptr;
    bool is_unified = false;
    bool is_in_use = false;
    uint32_t gpu_device_id = 0;
    uint64_t allocation_time = 0;
};

// GPU Memory Pool Manager
class GPUMemoryPool {
public:
    explicit GPUMemoryPool(uint32_t device_id, uint64_t pool_size = 4ULL * 1024 * 1024 * 1024); // 4GB default
    ~GPUMemoryPool();

    // Initialize memory pool
    bool initialize(VkDevice device, VkPhysicalDevice physical_device);

    // Allocate GPU memory
    VkDeviceMemory allocate(uint64_t size, VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    // Allocate unified memory (CPU accessible)
    VkDeviceMemory allocate_unified(uint64_t size);

    // Deallocate GPU memory
    bool deallocate(VkDeviceMemory memory);

    // Get CPU pointer for mapped memory
    void* get_cpu_pointer(VkDeviceMemory memory);

    // Map GPU memory to CPU
    void* map_memory(VkDeviceMemory memory, uint64_t offset, uint64_t size);

    // Unmap GPU memory
    void unmap_memory(VkDeviceMemory memory);

    // Get memory statistics
    MemoryStats get_statistics();

    // Defragment memory pool
    bool defragment();

    // Clear entire pool
    void clear();

    // Check if memory block exists
    bool contains(VkDeviceMemory memory);

    uint32_t get_device_id() const { return device_id_; }
    uint64_t get_total_size() const { return pool_size_; }
    uint64_t get_used_size() const;

private:
    uint32_t device_id_;
    uint64_t pool_size_;
    VkDevice device_ = VK_NULL_HANDLE;
    VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;

    std::vector<MemoryBlock> memory_blocks_;
    std::map<VkDeviceMemory, size_t> memory_map_;
    
    mutable QMutex mutex_;

    // Find free block suitable for allocation
    MemoryBlock* find_free_block(uint64_t size);

    // Compact memory fragmentation
    void compact_blocks();
};

// Unified GPU/CPU Memory Pool
class UnifiedMemoryPool {
public:
    explicit UnifiedMemoryPool(uint32_t device_id, uint64_t cpu_size = 2ULL * 1024 * 1024 * 1024,
                              uint64_t gpu_size = 4ULL * 1024 * 1024 * 1024);
    ~UnifiedMemoryPool();

    // Initialize unified memory
    bool initialize(VkDevice device, VkPhysicalDevice physical_device);

    // Allocate unified memory block (accessible from both CPU and GPU)
    void* allocate_unified(uint64_t size);

    // Allocate GPU-only memory with optional CPU mapping
    void* allocate_gpu(uint64_t size, bool cpu_accessible = false);

    // Allocate CPU-only memory
    void* allocate_cpu(uint64_t size);

    // Deallocate memory
    bool deallocate(void* ptr);

    // Synchronize CPU->GPU
    bool sync_cpu_to_gpu(void* ptr, uint64_t size);

    // Synchronize GPU->CPU
    bool sync_gpu_to_cpu(void* ptr, uint64_t size);

    // Get memory statistics
    MemoryStats get_gpu_statistics();
    MemoryStats get_cpu_statistics();

    // Prefetch memory to GPU
    bool prefetch_to_gpu(void* ptr, uint64_t size);

    // Prefetch memory to CPU
    bool prefetch_to_cpu(void* ptr, uint64_t size);

private:
    uint32_t device_id_;
    std::unique_ptr<GPUMemoryPool> gpu_pool_;
    
    // CPU memory allocations
    std::map<void*, uint64_t> cpu_allocations_;
    
    // Unified memory mappings
    std::map<void*, std::pair<void*, uint64_t>> unified_mappings_;

    mutable QMutex mutex_;
};

// Global GPU Memory Manager
class GPUMemoryManager {
public:
    static GPUMemoryManager& instance();

    bool initialize_device(uint32_t device_id, VkDevice device, 
                         VkPhysicalDevice physical_device);

    GPUMemoryPool* get_pool(uint32_t device_id);
    UnifiedMemoryPool* get_unified_pool(uint32_t device_id);

    // Multi-GPU operations
    bool allocate_multi_gpu(uint32_t count, uint64_t size_per_device, 
                          std::vector<void*>& ptrs);

    bool deallocate_multi_gpu(const std::vector<void*>& ptrs);

    // Get total GPU memory available
    uint64_t get_total_gpu_memory();

    // Performance profiling
    void enable_profiling(bool enable);
    MemoryStats get_profiling_statistics();

private:
    GPUMemoryManager() = default;
    ~GPUMemoryManager() = default;

    std::map<uint32_t, std::unique_ptr<GPUMemoryPool>> gpu_pools_;
    std::map<uint32_t, std::unique_ptr<UnifiedMemoryPool>> unified_pools_;
    
    bool profiling_enabled_ = false;
    MemoryStats profiling_stats_;

    mutable QMutex mutex_;
};

} // namespace GPU
} // namespace RawrXD
