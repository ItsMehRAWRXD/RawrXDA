#pragma once

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <stack>

namespace RawrXD {

/**
 * InferenceMemoryPool - High-performance memory allocation for inference
 *
 * Reduces allocation overhead and memory fragmentation by pre-allocating
 * and reusing memory blocks for common inference operations.
 *
 * Key optimizations:
 * - Thread-safe buffer pooling for tensor computations
 * - Eager allocation to avoid runtime blocking
 * - Automatic cleanup on idle periods
 * - Memory pressure monitoring
 */
class InferenceMemoryPool {
public:
    enum class BufferType {
        SmallBuffer = 64 * 1024,        // 64 KB - attention heads
        MediumBuffer = 256 * 1024,      // 256 KB - layer computations
        LargeBuffer = 4 * 1024 * 1024,  // 4 MB - layer outputs
        HugeBuffer = 16 * 1024 * 1024,  // 16 MB - KV cache
    };

    struct PoolStats {
        size_t total_allocated = 0;
        size_t peak_usage = 0;
        size_t current_usage = 0;
        uint64_t allocations = 0;
        uint64_t reuses = 0;
        double reuse_rate = 0.0;
        size_t fragmentation_percent = 0;
    };

    InferenceMemoryPool();
    ~InferenceMemoryPool();

    /**
     * Acquire a buffer from the pool.
     * If available, returns immediately; otherwise allocates new one.
     * Safe from any thread.
     */
    std::shared_ptr<uint8_t> AcquireBuffer(BufferType type, size_t exact_size = 0);

    /**
     * Release buffer back to pool for reuse.
     * Safe to call from any thread.
     */
    void ReleaseBuffer(std::shared_ptr<uint8_t>& buffer);

    /**
     * Pre-warm the pool by allocating typical inference buffers.
     * Call during initialization to reduce first-inference latency.
     */
    void Prewarm();

    /**
     * Get current pool statistics.
     */
    const PoolStats& GetStats() const { return m_stats; }

    /**
     * Reset statistics.
     */
    void ResetStats() { m_stats = PoolStats(); }

    /**
     * Trim unused buffers to reduce memory footprint.
     * Call periodically during idle time.
     */
    void TrimUnused();

    /**
     * Disable pooling (for testing).
     */
    void SetEnabled(bool enabled) { m_enabled = enabled; }

private:
    bool m_enabled = true;
    
    struct PoolBuffer {
        std::shared_ptr<uint8_t> data;
        size_t capacity = 0;
        bool in_use = false;
    };

    std::unordered_map<size_t, std::vector<PoolBuffer>> m_pools;
    mutable std::mutex m_pool_mutex;
    PoolStats m_stats;

    std::shared_ptr<uint8_t> allocate_new(size_t size);
    size_t round_size_up(BufferType type, size_t exact_size);
};

/**
 * InferenceGarbageCollector - Reduces GC pause times during inference
 *
 * Coordinates with the memory pool to minimize GC impact by:
 * - Marking object lifetime boundaries
 * - Triggering collection at safe points
 * - Monitoring heap fragmentation
 */
class InferenceGarbageCollector {
public:
    struct GCStats {
        uint64_t collections_triggered = 0;
        uint64_t safe_points_hit = 0;
        uint64_t pause_time_ms = 0;
        size_t heap_size_bytes = 0;
        size_t fragmentation_percent = 0;
    };

    InferenceGarbageCollector();
    ~InferenceGarbageCollector() = default;

    /**
     * Mark the beginning of an inference phase (e.g., start of layer).
     * Allows GC to track object lifetimes.
     */
    void BeginPhase(const char* phase_name);

    /**
     * Mark the end of an inference phase.
     * Triggers collection if safe and fragmentation is high.
     */
    void EndPhase();

    /**
     * Trigger a collection cycle if safe to do so.
     * Returns true if collection occurred.
     */
    bool TriggerIfNeeded();

    /**
     * Explicitly trigger collection at a safe point.
     */
    void TriggerAtSafePoint();

    /**
     * Get GC statistics.
     */
    const GCStats& GetStats() const { return m_stats; }

    /**
     * Reset statistics.
     */
    void ResetStats() { m_stats = GCStats(); }

    /**
     * Enable/disable automatic collection.
     */
    void SetAutomaticCollection(bool enabled) { m_auto_collection = enabled; }

private:
    bool m_auto_collection = true;
    const char* m_current_phase = nullptr;
    GCStats m_stats;
    mutable std::mutex m_mutex;

    size_t get_heap_fragmentation();
};

} // namespace RawrXD

