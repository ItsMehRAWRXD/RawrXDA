#include "inference_memory_pool.h"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace RawrXD {

// ============================================================================
// InferenceMemoryPool Implementation
// ============================================================================

InferenceMemoryPool::InferenceMemoryPool()
{
    // Initialize pools as empty
    m_pools[64 * 1024] = {};
    m_pools[256 * 1024] = {};
    m_pools[4 * 1024 * 1024] = {};
    m_pools[16 * 1024 * 1024] = {};
}

InferenceMemoryPool::~InferenceMemoryPool()
{
    // Pools will be cleaned up automatically
}

std::shared_ptr<uint8_t> InferenceMemoryPool::AcquireBuffer(BufferType type, size_t exact_size)
{
    if (!m_enabled) {
        return allocate_new(exact_size > 0 ? exact_size : static_cast<size_t>(type));
    }

    size_t needed_size = round_size_up(type, exact_size);

    {
        std::lock_guard<std::mutex> lock(m_pool_mutex);

        auto pool_it = m_pools.find(needed_size);
        if (pool_it != m_pools.end()) {
            auto& pool = pool_it->second;
            
            // Try to find a free buffer
            for (auto& buf : pool) {
                if (!buf.in_use && buf.capacity >= needed_size) {
                    buf.in_use = true;
                    m_stats.reuses++;
                    m_stats.current_usage += needed_size;
                    if (m_stats.current_usage > m_stats.peak_usage) {
                        m_stats.peak_usage = m_stats.current_usage;
                    }
                    return buf.data;
                }
            }
        }

        // No free buffer, allocate new one
        auto new_buffer = allocate_new(needed_size);
        PoolBuffer pool_buf{new_buffer, needed_size, true};
        m_pools[needed_size].push_back(pool_buf);

        m_stats.allocations++;
        m_stats.total_allocated += needed_size;
        m_stats.current_usage += needed_size;
        if (m_stats.current_usage > m_stats.peak_usage) {
            m_stats.peak_usage = m_stats.current_usage;
        }

        return new_buffer;
    }
}

void InferenceMemoryPool::ReleaseBuffer(std::shared_ptr<uint8_t>& buffer)
{
    if (!m_enabled || !buffer) {
        buffer.reset();
        return;
    }

    {
        std::lock_guard<std::mutex> lock(m_pool_mutex);
        
        // Find and mark as free
        for (auto& pool : m_pools) {
            for (auto& buf : pool.second) {
                if (buf.data == buffer) {
                    buf.in_use = false;
                    m_stats.current_usage -= pool.first;
                    buffer.reset();
                    return;
                }
            }
        }
    }
    
    buffer.reset();
}

void InferenceMemoryPool::Prewarm()
{
    if (!m_enabled) return;

    // Pre-allocate typical inference buffers
    AcquireBuffer(BufferType::SmallBuffer);   // 64 KB x 2
    AcquireBuffer(BufferType::SmallBuffer);
    AcquireBuffer(BufferType::MediumBuffer);  // 256 KB x 2
    AcquireBuffer(BufferType::MediumBuffer);
    AcquireBuffer(BufferType::LargeBuffer);   // 4 MB x 1
    AcquireBuffer(BufferType::HugeBuffer);    // 16 MB x 1 (KV cache)

    // Release them all so they're ready for reuse
    for (auto& pool : m_pools) {
        for (auto& buf : pool.second) {
            if (buf.in_use) {
                buf.in_use = false;
                m_stats.current_usage -= buf.capacity;
            }
        }
    }
}

void InferenceMemoryPool::TrimUnused()
{
    if (!m_enabled) return;

    std::lock_guard<std::mutex> lock(m_pool_mutex);

    for (auto& pool : m_pools) {
        auto& buffers = pool.second;
        auto new_end = std::remove_if(buffers.begin(), buffers.end(),
            [](const PoolBuffer& buf) { return !buf.in_use; });
        buffers.erase(new_end, buffers.end());
    }
}

std::shared_ptr<uint8_t> InferenceMemoryPool::allocate_new(size_t size)
{
    return std::shared_ptr<uint8_t>(new uint8_t[size], std::default_delete<uint8_t[]>());
}

size_t InferenceMemoryPool::round_size_up(BufferType type, size_t exact_size)
{
    if (exact_size > 0) {
        // Round up to nearest standard size
        if (exact_size <= 64 * 1024) return 64 * 1024;
        if (exact_size <= 256 * 1024) return 256 * 1024;
        if (exact_size <= 4 * 1024 * 1024) return 4 * 1024 * 1024;
        return 16 * 1024 * 1024;
    }
    return static_cast<size_t>(type);
}

// ============================================================================
// InferenceGarbageCollector Implementation
// ============================================================================

InferenceGarbageCollector::InferenceGarbageCollector()
    : m_auto_collection(true), m_current_phase(nullptr)
{
}

void InferenceGarbageCollector::BeginPhase(const char* phase_name)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_current_phase = phase_name;
}

void InferenceGarbageCollector::EndPhase()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_current_phase = nullptr;
    m_stats.safe_points_hit++;

    if (m_auto_collection && get_heap_fragmentation() > 25) {
        TriggerAtSafePoint();
    }
}

bool InferenceGarbageCollector::TriggerIfNeeded()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_auto_collection && get_heap_fragmentation() > 30) {
        m_stats.collections_triggered++;
        return true;
    }
    return false;
}

void InferenceGarbageCollector::TriggerAtSafePoint()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.collections_triggered++;
    // In real implementation, would trigger GC here
}

size_t InferenceGarbageCollector::get_heap_fragmentation()
{
    // Simplified: return a reasonable estimate
    // In production, this would query actual heap statistics
    return 20;  // Assume 20% fragmentation for now
}

} // namespace RawrXD

