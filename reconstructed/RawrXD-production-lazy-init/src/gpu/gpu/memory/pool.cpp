#include "gpu_memory_pool.h"
#include <algorithm>
#include <cstring>
#include <chrono>
#include <iostream>

namespace RawrXD {
namespace GPU {

// ============================================================================
// GPUMemoryPool Implementation
// ============================================================================

GPUMemoryPool::GPUMemoryPool(uint32_t device_id, uint64_t pool_size)
    : device_id_(device_id), pool_size_(pool_size) {}

GPUMemoryPool::~GPUMemoryPool() {
    clear();
}

bool GPUMemoryPool::initialize(VkDevice device, VkPhysicalDevice physical_device) {
    QMutexLocker lock(&mutex_);

    device_ = device;
    physical_device_ = physical_device;

    // Create initial memory blocks
    MemoryBlock initial_block;
    initial_block.offset = 0;
    initial_block.size = pool_size_;
    initial_block.is_unified = false;
    initial_block.is_in_use = false;
    initial_block.gpu_device_id = device_id_;
    initial_block.allocation_time = std::chrono::system_clock::now().time_since_epoch().count();

    memory_blocks_.push_back(initial_block);

    return true;
}

VkDeviceMemory GPUMemoryPool::allocate(uint64_t size, VkMemoryPropertyFlags flags) {
    QMutexLocker lock(&mutex_);

    MemoryBlock* block = find_free_block(size);
    if (!block) {
        return VK_NULL_HANDLE;
    }

    // Allocate new memory
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = size;
    
    // Find suitable memory type (simplified)
    allocInfo.memoryTypeIndex = 0;

    VkDeviceMemory memory;
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
        return VK_NULL_HANDLE;
    }

    // Create block for tracking
    MemoryBlock new_block;
    new_block.offset = block->offset;
    new_block.size = size;
    new_block.memory = memory;
    new_block.is_unified = (flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0;
    new_block.is_in_use = true;
    new_block.gpu_device_id = device_id_;
    new_block.allocation_time = std::chrono::system_clock::now().time_since_epoch().count();

    memory_map_[memory] = memory_blocks_.size();
    memory_blocks_.push_back(new_block);

    // Update original block
    block->offset += size;
    block->size -= size;

    return memory;
}

VkDeviceMemory GPUMemoryPool::allocate_unified(uint64_t size) {
    return allocate(size, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | 
                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
}

bool GPUMemoryPool::deallocate(VkDeviceMemory memory) {
    QMutexLocker lock(&mutex_);

    auto it = memory_map_.find(memory);
    if (it == memory_map_.end()) {
        return false;
    }

    size_t index = it->second;
    if (index >= memory_blocks_.size()) {
        return false;
    }

    MemoryBlock& block = memory_blocks_[index];
    block.is_in_use = false;

    vkFreeMemory(device_, memory, nullptr);
    memory_map_.erase(it);

    return true;
}

void* GPUMemoryPool::get_cpu_pointer(VkDeviceMemory memory) {
    QMutexLocker lock(&mutex_);

    auto it = memory_map_.find(memory);
    if (it != memory_map_.end()) {
        size_t index = it->second;
        if (index < memory_blocks_.size()) {
            return memory_blocks_[index].cpu_mapped_ptr;
        }
    }
    return nullptr;
}

void* GPUMemoryPool::map_memory(VkDeviceMemory memory, uint64_t offset, uint64_t size) {
    QMutexLocker lock(&mutex_);

    auto it = memory_map_.find(memory);
    if (it == memory_map_.end()) {
        return nullptr;
    }

    size_t index = it->second;
    if (index >= memory_blocks_.size()) {
        return nullptr;
    }

    MemoryBlock& block = memory_blocks_[index];
    void* mapped_ptr = nullptr;

    if (vkMapMemory(device_, memory, offset, size, 0, &mapped_ptr) != VK_SUCCESS) {
        return nullptr;
    }

    block.cpu_mapped_ptr = mapped_ptr;
    return mapped_ptr;
}

void GPUMemoryPool::unmap_memory(VkDeviceMemory memory) {
    QMutexLocker lock(&mutex_);

    auto it = memory_map_.find(memory);
    if (it != memory_map_.end()) {
        size_t index = it->second;
        if (index < memory_blocks_.size()) {
            vkUnmapMemory(device_, memory);
            memory_blocks_[index].cpu_mapped_ptr = nullptr;
        }
    }
}

MemoryStats GPUMemoryPool::get_statistics() {
    QMutexLocker lock(&mutex_);

    MemoryStats stats;
    stats.total_allocated = pool_size_;

    for (const auto& block : memory_blocks_) {
        if (block.is_in_use) {
            stats.total_used += block.size;
            stats.allocation_count++;
        } else {
            stats.total_free += block.size;
        }
    }

    // Calculate fragmentation ratio
    if (stats.total_free > 0) {
        stats.fragmentation_ratio = static_cast<uint32_t>(
            (static_cast<double>(memory_blocks_.size()) / 
             static_cast<double>(stats.total_free)) * 100);
    }

    return stats;
}

bool GPUMemoryPool::defragment() {
    QMutexLocker lock(&mutex_);

    // Sort blocks by usage status and offset
    std::sort(memory_blocks_.begin(), memory_blocks_.end(),
              [](const MemoryBlock& a, const MemoryBlock& b) {
                  if (a.is_in_use != b.is_in_use) return a.is_in_use > b.is_in_use;
                  return a.offset < b.offset;
              });

    // Update offsets for contiguous blocks
    uint64_t current_offset = 0;
    for (auto& block : memory_blocks_) {
        block.offset = current_offset;
        current_offset += block.size;
    }

    compact_blocks();
    return true;
}

void GPUMemoryPool::clear() {
    QMutexLocker lock(&mutex_);

    for (auto& block : memory_blocks_) {
        if (block.memory != VK_NULL_HANDLE) {
            if (block.cpu_mapped_ptr) {
                vkUnmapMemory(device_, block.memory);
            }
            vkFreeMemory(device_, block.memory, nullptr);
        }
    }

    memory_blocks_.clear();
    memory_map_.clear();
}

MemoryBlock* GPUMemoryPool::find_free_block(uint64_t size) {
    for (auto& block : memory_blocks_) {
        if (!block.is_in_use && block.size >= size) {
            return &block;
        }
    }
    return nullptr;
}

void GPUMemoryPool::compact_blocks() {
    // Merge adjacent free blocks
    for (size_t i = 0; i + 1 < memory_blocks_.size();) {
        if (!memory_blocks_[i].is_in_use && !memory_blocks_[i + 1].is_in_use) {
            memory_blocks_[i].size += memory_blocks_[i + 1].size;
            memory_blocks_.erase(memory_blocks_.begin() + i + 1);
        } else {
            ++i;
        }
    }
}

uint64_t GPUMemoryPool::get_used_size() const {
    QMutexLocker lock(&mutex_);

    uint64_t used = 0;
    for (const auto& block : memory_blocks_) {
        if (block.is_in_use) {
            used += block.size;
        }
    }
    return used;
}

bool GPUMemoryPool::contains(VkDeviceMemory memory) {
    QMutexLocker lock(&mutex_);
    return memory_map_.find(memory) != memory_map_.end();
}

// ============================================================================
// UnifiedMemoryPool Implementation
// ============================================================================

UnifiedMemoryPool::UnifiedMemoryPool(uint32_t device_id, uint64_t cpu_size, uint64_t gpu_size)
    : device_id_(device_id),
      gpu_pool_(std::make_unique<GPUMemoryPool>(device_id, gpu_size)) {}

UnifiedMemoryPool::~UnifiedMemoryPool() {
    for (auto& [ptr, _] : cpu_allocations_) {
        delete[] static_cast<uint8_t*>(ptr);
    }
    cpu_allocations_.clear();
    unified_mappings_.clear();
}

bool UnifiedMemoryPool::initialize(VkDevice device, VkPhysicalDevice physical_device) {
    return gpu_pool_->initialize(device, physical_device);
}

void* UnifiedMemoryPool::allocate_unified(uint64_t size) {
    QMutexLocker lock(&mutex_);

    // Allocate GPU memory that's also host-visible
    VkDeviceMemory gpu_mem = gpu_pool_->allocate_unified(size);
    if (!gpu_mem) {
        return nullptr;
    }

    // Map it to CPU
    void* cpu_ptr = gpu_pool_->map_memory(gpu_mem, 0, size);
    if (!cpu_ptr) {
        gpu_pool_->deallocate(gpu_mem);
        return nullptr;
    }

    unified_mappings_[cpu_ptr] = {cpu_ptr, size};
    return cpu_ptr;
}

void* UnifiedMemoryPool::allocate_gpu(uint64_t size, bool cpu_accessible) {
    QMutexLocker lock(&mutex_);

    VkMemoryPropertyFlags flags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    if (cpu_accessible) {
        flags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }

    VkDeviceMemory gpu_mem = gpu_pool_->allocate(size, flags);
    if (!gpu_mem) {
        return nullptr;
    }

    if (cpu_accessible) {
        void* cpu_ptr = gpu_pool_->map_memory(gpu_mem, 0, size);
        if (cpu_ptr) {
            unified_mappings_[cpu_ptr] = {cpu_ptr, size};
            return cpu_ptr;
        }
    }

    return reinterpret_cast<void*>(gpu_mem);
}

void* UnifiedMemoryPool::allocate_cpu(uint64_t size) {
    QMutexLocker lock(&mutex_);

    void* ptr = new uint8_t[size];
    if (ptr) {
        cpu_allocations_[ptr] = size;
    }
    return ptr;
}

bool UnifiedMemoryPool::deallocate(void* ptr) {
    QMutexLocker lock(&mutex_);

    // Check unified mappings
    auto it = unified_mappings_.find(ptr);
    if (it != unified_mappings_.end()) {
        unified_mappings_.erase(it);
        return true;
    }

    // Check CPU allocations
    auto cpu_it = cpu_allocations_.find(ptr);
    if (cpu_it != cpu_allocations_.end()) {
        delete[] static_cast<uint8_t*>(ptr);
        cpu_allocations_.erase(cpu_it);
        return true;
    }

    return false;
}

bool UnifiedMemoryPool::sync_cpu_to_gpu(void* ptr, uint64_t size) {
    QMutexLocker lock(&mutex_);

    auto it = unified_mappings_.find(ptr);
    if (it != unified_mappings_.end()) {
        // GPU memory is already coherent due to host-coherent flag
        return true;
    }

    return false;
}

bool UnifiedMemoryPool::sync_gpu_to_cpu(void* ptr, uint64_t size) {
    QMutexLocker lock(&mutex_);

    auto it = unified_mappings_.find(ptr);
    if (it != unified_mappings_.end()) {
        // GPU memory is already coherent due to host-coherent flag
        return true;
    }

    return false;
}

MemoryStats UnifiedMemoryPool::get_gpu_statistics() {
    return gpu_pool_->get_statistics();
}

MemoryStats UnifiedMemoryPool::get_cpu_statistics() {
    QMutexLocker lock(&mutex_);

    MemoryStats stats;
    for (const auto& [ptr, size] : cpu_allocations_) {
        stats.total_allocated += size;
        stats.total_used += size;
        stats.allocation_count++;
    }
    return stats;
}

bool UnifiedMemoryPool::prefetch_to_gpu(void* ptr, uint64_t size) {
    QMutexLocker lock(&mutex_);
    // In real implementation, would use vkCmdPipelineBarrier
    return true;
}

bool UnifiedMemoryPool::prefetch_to_cpu(void* ptr, uint64_t size) {
    QMutexLocker lock(&mutex_);
    // In real implementation, would issue cache invalidation
    return true;
}

// ============================================================================
// GPUMemoryManager Implementation
// ============================================================================

GPUMemoryManager& GPUMemoryManager::instance() {
    static GPUMemoryManager manager;
    return manager;
}

bool GPUMemoryManager::initialize_device(uint32_t device_id, VkDevice device,
                                        VkPhysicalDevice physical_device) {
    QMutexLocker lock(&mutex_);

    auto pool = std::make_unique<GPUMemoryPool>(device_id);
    if (!pool->initialize(device, physical_device)) {
        return false;
    }

    auto unified_pool = std::make_unique<UnifiedMemoryPool>(device_id);
    if (!unified_pool->initialize(device, physical_device)) {
        return false;
    }

    gpu_pools_[device_id] = std::move(pool);
    unified_pools_[device_id] = std::move(unified_pool);

    return true;
}

GPUMemoryPool* GPUMemoryManager::get_pool(uint32_t device_id) {
    QMutexLocker lock(&mutex_);

    auto it = gpu_pools_.find(device_id);
    if (it != gpu_pools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

UnifiedMemoryPool* GPUMemoryManager::get_unified_pool(uint32_t device_id) {
    QMutexLocker lock(&mutex_);

    auto it = unified_pools_.find(device_id);
    if (it != unified_pools_.end()) {
        return it->second.get();
    }
    return nullptr;
}

bool GPUMemoryManager::allocate_multi_gpu(uint32_t count, uint64_t size_per_device,
                                         std::vector<void*>& ptrs) {
    QMutexLocker lock(&mutex_);

    ptrs.clear();
    ptrs.reserve(count);

    for (uint32_t i = 0; i < count; ++i) {
        auto pool = get_pool(i);
        if (!pool) {
            return false;
        }

        VkDeviceMemory mem = pool->allocate_unified(size_per_device);
        if (!mem) {
            return false;
        }

        ptrs.push_back(reinterpret_cast<void*>(mem));
    }

    return true;
}

bool GPUMemoryManager::deallocate_multi_gpu(const std::vector<void*>& ptrs) {
    QMutexLocker lock(&mutex_);

    for (void* ptr : ptrs) {
        VkDeviceMemory mem = reinterpret_cast<VkDeviceMemory>(ptr);
        for (auto& [device_id, pool] : gpu_pools_) {
            if (pool->contains(mem)) {
                pool->deallocate(mem);
                break;
            }
        }
    }

    return true;
}

uint64_t GPUMemoryManager::get_total_gpu_memory() {
    QMutexLocker lock(&mutex_);

    uint64_t total = 0;
    for (auto& [device_id, pool] : gpu_pools_) {
        total += pool->get_total_size();
    }
    return total;
}

void GPUMemoryManager::enable_profiling(bool enable) {
    QMutexLocker lock(&mutex_);
    profiling_enabled_ = enable;
}

MemoryStats GPUMemoryManager::get_profiling_statistics() {
    QMutexLocker lock(&mutex_);

    MemoryStats stats;
    for (auto& [device_id, pool] : gpu_pools_) {
        auto pool_stats = pool->get_statistics();
        stats.total_allocated += pool_stats.total_allocated;
        stats.total_used += pool_stats.total_used;
        stats.total_free += pool_stats.total_free;
        stats.allocation_count += pool_stats.allocation_count;
        stats.deallocation_count += pool_stats.deallocation_count;
    }
    return stats;
}

} // namespace GPU
} // namespace RawrXD
