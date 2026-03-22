// UnifiedMemoryPool.h — Unified tiered memory management for RawrXD inference
// Provides: CPU RAM pool, GPU VRAM pool, disk-backed overflow, KV cache compression,
// memory prefetching, smart eviction, pressure monitoring, and zero-copy transfers.
// Enhancement 1: Tiered storage (CPU/GPU/Disk) with automatic data migration
// Enhancement 2: KV cache 4-bit quantization with selective decompression
// Enhancement 3: Attention-pattern-based memory prefetching
// Enhancement 4: LRU + frequency-weighted eviction policies
// Enhancement 5: Real-time memory pressure monitoring with callbacks
// Enhancement 6: GPU VRAM fragmentation reduction via defragmentation
// Enhancement 7: Zero-copy tensor staging buffers
// Enhancement 8: Memory coalescing for cache-line alignment
// Enhancement 9: Lazy tensor loading with demand paging
// Enhancement 10: Memory-mapped model weight segments
// Enhancement 11: Activation checkpointing for memory reduction
// Enhancement 12: Adaptive batch size based on available memory
// Enhancement 13: NUMA-aware allocation routing
// Enhancement 14: Memory bandwidth throttle & monitoring
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <functional>
#include <atomic>
#include <chrono>
#include <optional>

namespace RawrXD {

// ─── Memory tiers ────────────────────────────────────────────────────────────
enum class MemoryTier {
    L2_CPU_Cache,   // Hot working set — pinned near L2 cache
    CPU_RAM,        // Standard heap allocation
    GPU_VRAM,       // Device-local GPU memory
    Disk_Mapped,    // Memory-mapped file on disk
    Compressed      // In-memory LZ4/quantized compressed
};

// ─── Eviction policies ───────────────────────────────────────────────────────
enum class EvictionPolicy {
    LRU,             // Least-recently used
    LFU,             // Least-frequently used
    ARC,             // Adaptive replacement cache
    FreqWeightedLRU  // LRU with frequency multiplier (default)
};

// ─── Memory block metadata ───────────────────────────────────────────────────
struct MemoryBlock {
    uint64_t            id           = 0;
    void*               ptr          = nullptr;
    size_t              size         = 0;
    MemoryTier          tier         = MemoryTier::CPU_RAM;
    std::string         tag;                // e.g. "kv_cache::layer_3::keys"
    uint64_t            access_count = 0;
    std::chrono::steady_clock::time_point last_access{};
    bool                pinned       = false;  // Never evict
    bool                dirty        = false;  // Modified since last sync
    uint8_t             numa_node    = 0;      // NUMA affinity
};

// ─── Prefetch hint ───────────────────────────────────────────────────────────
struct PrefetchHint {
    std::vector<uint64_t> block_ids;          // Blocks to prefetch
    int                   priority     = 0;   // Higher = sooner
    float                 confidence   = 1.f; // Prediction confidence
};

// ─── Memory pressure callback ────────────────────────────────────────────────
struct PressureEvent {
    MemoryTier  tier;
    float       used_fraction;   // 0..1
    size_t      bytes_available;
    bool        critical;        // >90% used
};

using PressureCallback = std::function<void(const PressureEvent&)>;

// ─── Bandwidth sample ────────────────────────────────────────────────────────
struct BandwidthSample {
    std::chrono::steady_clock::time_point ts;
    size_t bytes_read  = 0;
    size_t bytes_write = 0;
};

// ─── Unified Memory Pool ─────────────────────────────────────────────────────
class UnifiedMemoryPool {
public:
    // ---- Construction & singleton ----
    explicit UnifiedMemoryPool(size_t cpu_budget_mb  = 8192,
                                size_t gpu_budget_mb  = 16384,
                                size_t disk_budget_mb = 65536);
    ~UnifiedMemoryPool();

    static UnifiedMemoryPool& Instance();

    // ---- Enhancement 1: Tiered allocation ----
    // Allocate a named block, automatically placed in best tier
    uint64_t Allocate(size_t bytes, const std::string& tag,
                      MemoryTier preferred = MemoryTier::CPU_RAM,
                      bool pinned = false);

    // Free a block and return memory to pool
    void Free(uint64_t block_id);

    // Get raw pointer for a block (migrates to CPU_RAM if needed)
    void* GetPtr(uint64_t block_id);

    // ---- Enhancement 2: KV cache 4-bit quantization ----
    // Compress KV cache block to Q4_0, return compressed block id
    uint64_t CompressKVBlock(uint64_t block_id);

    // Decompress Q4_0 KV block back to FP32 in-place
    void DecompressKVBlock(uint64_t compressed_id, uint64_t target_id);

    // Compress all KV cache blocks not in active window
    void CompressInactiveKVBlocks(int active_seq_start, int active_seq_end);

    // ---- Enhancement 3: Attention-pattern-based prefetching ----
    // Submit prefetch hints based on predicted next-token attention positions
    void SubmitPrefetchHints(const std::vector<PrefetchHint>& hints);

    // Prefetch blocks for upcoming transformer layer index
    void PrefetchLayer(int layer_idx, int seq_pos);

    // ---- Enhancement 4: Smart eviction ----
    void SetEvictionPolicy(EvictionPolicy policy);
    EvictionPolicy GetEvictionPolicy() const { return m_eviction_policy; }

    // Evict bytes to free memory in specified tier
    size_t Evict(size_t bytes_needed, MemoryTier tier);

    // Pin/unpin block from eviction
    void PinBlock(uint64_t block_id, bool pin);

    // ---- Enhancement 5: Memory pressure monitoring ----
    void RegisterPressureCallback(PressureCallback cb);
    void SetPressureThreshold(MemoryTier tier, float warn_frac, float crit_frac);
    PressureEvent GetCurrentPressure(MemoryTier tier) const;

    // ---- Enhancement 6: GPU VRAM defragmentation ----
    // Compact GPU allocations to reduce fragmentation
    void DefragmentGPUVRAM();
    float GetGPUFragmentationRatio() const;

    // ---- Enhancement 7: Zero-copy tensor staging ----
    // Allocate a staging buffer that maps to both CPU and GPU virtual addresses
    uint64_t AllocateStagingBuffer(size_t bytes, const std::string& tag);
    void* GetStagingCPUPtr(uint64_t staging_id);
    void* GetStagingGPUPtr(uint64_t staging_id);
    void FlushStagingToGPU(uint64_t staging_id);
    void InvalidateStagingFromGPU(uint64_t staging_id);

    // ---- Enhancement 8: Memory coalescing ----
    // Realign allocations to cache-line boundaries (64 bytes)
    void CoalesceAllocations(MemoryTier tier);
    static size_t AlignToCacheLine(size_t bytes) { return (bytes + 63) & ~63ULL; }

    // ---- Enhancement 9: Lazy tensor loading ----
    // Register a model tensor as demand-loadable (loaded on first GetPtr)
    uint64_t RegisterLazyTensor(const std::string& file_path,
                                 uint64_t file_offset,
                                 size_t byte_size,
                                 const std::string& tag);
    bool IsTensorLoaded(uint64_t block_id) const;

    // ---- Enhancement 10: Memory-mapped segments ----
    // Map a region of a model file directly into address space
    uint64_t MapModelSegment(const std::string& path,
                              uint64_t offset,
                              size_t length,
                              const std::string& tag);
    void UnmapModelSegment(uint64_t block_id);

    // ---- Enhancement 11: Activation checkpointing ----
    // Save activation tensors to compressed storage, recompute on backward
    uint64_t CheckpointActivation(const float* data, size_t elements,
                                   int layer_idx, int seq_pos);
    bool RestoreActivation(uint64_t checkpoint_id, float* out, size_t elements);
    void ClearActivationCheckpoints(int layer_idx);

    // ---- Enhancement 12: Adaptive batch sizing ----
    // Returns max safe batch size given current memory pressure
    int SuggestBatchSize(size_t bytes_per_token,
                          int max_batch  = 64,
                          float headroom = 0.20f) const;

    // ---- Enhancement 13: NUMA-aware allocation ----
    void SetPreferredNUMANode(uint8_t node);
    uint64_t AllocateOnNUMA(size_t bytes, uint8_t numa_node, const std::string& tag);

    // ---- Enhancement 14: Bandwidth monitoring ----
    void RecordTransfer(size_t bytes, bool is_write);
    BandwidthSample GetCurrentBandwidth() const;
    double GetReadBandwidthGBps()  const;
    double GetWriteBandwidthGBps() const;
    void   SetBandwidthThrottleMBps(double max_mbs);

    // ---- Statistics ----
    size_t GetUsedBytes(MemoryTier tier) const;
    size_t GetBudgetBytes(MemoryTier tier) const;
    size_t GetTotalAllocatedBytes() const;
    size_t GetTotalFreeBytes() const;
    int    GetBlockCount() const;
    double GetCacheHitRate() const;
    void   PrintStats() const;

    // ---- Lifecycle ----
    void Reset();            // Free all non-pinned blocks
    void SetGPUDevice(int device_index);  // Select GPU device index

private:
    // Internal allocation helpers
    void*   AllocCPU(size_t bytes, uint8_t numa_node);
    void    FreeCPU(void* ptr);
    void*   AllocGPU(size_t bytes);
    void    FreeGPU(void* ptr);
    void*   AllocDisk(size_t bytes, const std::string& tag);
    void    FreeDisk(void* ptr);

    // Eviction internals
    std::vector<uint64_t> GetEvictionCandidates(MemoryTier tier, size_t bytes_needed);
    bool   MigrateBlock(uint64_t id, MemoryTier target_tier);

    // Pressure checking (called after every alloc/free)
    void   CheckPressure(MemoryTier tier);

    // Prefetch worker
    void   PrefetchWorker();

    // Block registry
    mutable std::shared_mutex           m_registry_mutex;
    std::unordered_map<uint64_t, MemoryBlock> m_blocks;
    std::atomic<uint64_t>               m_next_id{1};

    // Tier budgets
    std::atomic<size_t> m_cpu_used{0}, m_gpu_used{0}, m_disk_used{0};
    size_t              m_cpu_budget, m_gpu_budget, m_disk_budget;

    // Policy
    EvictionPolicy      m_eviction_policy = EvictionPolicy::FreqWeightedLRU;

    // Pressure callbacks
    std::mutex                          m_cb_mutex;
    std::vector<PressureCallback>       m_pressure_callbacks;
    float                               m_warn_frac = 0.80f;
    float                               m_crit_frac = 0.90f;

    // Bandwidth tracking
    mutable std::mutex          m_bw_mutex;
    std::vector<BandwidthSample> m_bw_samples;
    double                      m_bw_throttle_mbs = 0.0; // 0 = unlimited

    // Staging buffers map
    std::unordered_map<uint64_t, std::pair<void*,void*>> m_staging; // cpu,gpu ptrs

    // NUMA preference
    uint8_t m_preferred_numa = 0;

    // GPU device
    int     m_gpu_device = 0;

    // Activation checkpoint store (layer → list of blocks)
    std::unordered_map<int, std::vector<uint64_t>> m_checkpoints;

    // Cache hit tracking
    mutable std::atomic<uint64_t> m_cache_hits{0};
    mutable std::atomic<uint64_t> m_cache_total{0};

    static UnifiedMemoryPool* s_instance;
};

} // namespace RawrXD
