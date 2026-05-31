#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <chrono>

// ============================================================================
// CORRECTED C++ WRAPPER FOR AGGRESSIVE APERTURE OVERFLOW MANAGEMENT
// Matches actual MASM x64 calling conventions & signatures from
// rawr_aperture_bypass.asm
// ============================================================================

namespace rawr::speculative::aperture {

// ============================================================================
// EXTERN DECLARATIONS - MASM IMPLEMENTATIONS
// ============================================================================

// Bandwidth-aware adaptive streaming
// Dynamically adjusts stride based on DDR5/PCIe bandwidth ratio
extern "C" void RawrBandwidthAwareStream(
    void* src, 
    void* dst, 
    size_t size, 
    uint64_t ddr5_bw, 
    uint64_t pcie_bw
);

// Proactive tensor eviction for 8x MoE swarm
extern "C" uint32_t RawrProactiveEvictSwarm(
    void** tensor_ptrs,
    uint64_t* last_access,
    uint32_t* access_count,
    size_t count,
    uint64_t current_time,
    uint64_t threshold_us,
    uint32_t min_access_count
);

// Expert deduplication mask (checks which experts are duplicate)
extern "C" uint8_t RawrExpertDedupMask(
    void** expert_ptrs,
    uint64_t* expert_hashes,
    size_t num_experts,
    uint64_t threshold_hash_dist
);

// Swarm slot assignment for cache reuse
extern "C" void RawrSwarmAssignSlots(
    uint8_t* agent_expert_ids,
    size_t num_agents,
    uint8_t* cache_slot_map,
    size_t num_slots
);

// Memory management primitives
extern "C" void* RawrAllocateHugePages(size_t size);
extern "C" bool RawrPinMemory(void* ptr, size_t size);
extern "C" bool RawrUnpinMemory(void* ptr, size_t size);
extern "C" void RawrPrefetchMemory(void* ptr, size_t size);

// Aggressive streaming variant
extern "C" void RawrAggressiveStream(
    void* src, 
    void* dst, 
    size_t size
);

// Thread affinity & synchronization
extern "C" bool RawrSetThreadAffinityToNUMA0();
extern "C" void RawrFlushCacheLines(void* ptr, size_t size);
extern "C" void RawrMemoryBarrier();

// Physical address & aperture control
extern "C" uint64_t RawrGetPhysicalAddress(void* ptr);
extern "C" bool RawrActivateApertureBypass(
    void* ddr5_base, 
    size_t size, 
    uint32_t flags
);
extern "C" bool RawrDeactivateApertureBypass(
    void* ddr5_base, 
    size_t size
);

// Prefetch & preload variants
extern "C" void RawrStreamingPrefetch(
    void* ptr, 
    size_t size, 
    uint32_t tier
);
extern "C" void RawrPreloadExpertWeights(
    void** expert_ptrs, 
    size_t num_experts, 
    size_t expert_size
);
extern "C" void RawrLookaheadPrefetch(
    void** upcoming_tensors, 
    size_t count, 
    size_t tensor_size
);

// Double-buffer swap (atomic xchg with memory)
// RCX: pointer to active_ptr (qword)
// RDX: new shadow buffer address
// Returns old active_ptr in RAX
extern "C" void* RAWR_DoubleBuffer_Swap(
    void** active_ptr,
    void* new_shadow
);

// Expert cache probe (checks if expert block is warm/cached)
// RCX: expert_id (0-7 for 8x MoE)
// RDX: pointer to expert_last_access table (8 x uint64)
// R8:  current_time_us
// R9:  warm_threshold_us
// Returns 1 if warm, 0 if cold (in RAX/EAX)
extern "C" int RAWR_ExpertCache_Probe(
    uint64_t expert_id,
    uint64_t* expert_last_access_table,
    uint64_t current_time_us,
    uint64_t warm_threshold_us
);

// PCIe flush barrier (clflushopt + sfence)
// RCX: ptr (start address)
// RDX: size (bytes to flush)
extern "C" void RAWR_PCIe_FlushBarrier(
    void* ptr,
    size_t size
);

// Swarm slot prefetch (multi-agent cache coordination)
extern "C" void RAWR_SwarmSlot_Prefetch(
    int slot_id,
    void* addr,
    size_t bytes
);

// Pressure model & tier control
extern "C" float RawrComputePressure(
    float vram_used_frac,
    float growth_rate
);

extern "C" uint32_t RawrSetPrefetchDepth(float pressure);

// ============================================================================
// TIER DEFINITIONS FOR AGGRESSIVE OVERFLOW
// ============================================================================

enum class OverflowTier : uint8_t {
    TIER_STEADY    = 0,    // < 55% VRAM (baseline)
    TIER_HYBRID    = 1,    // 55-70% (blend DDR5+VRAM)
    TIER_STRIDE    = 2,    // 70-82% (adaptive stride)
    TIER_EMERGENCY = 3,    // > 82% (max PCIe + compression)
};

// ============================================================================
// LRU CACHE FOR EXPERT WEIGHTS (16-entry, tier-1 cache)
// ============================================================================

struct ExpertCacheEntry {
    void* ptr;
    uint64_t hash;
    uint64_t last_access_us;
    uint32_t access_count;
    bool is_valid;
};

class ExpertLRUCache {
public:
    static constexpr size_t CACHE_SIZE = 16;

    ExpertLRUCache();
    ~ExpertLRUCache();

    // Add or update expert in cache
    void add_expert(const void* ptr, uint64_t hash);

    // Probe whether expert is cached (warm)
    bool probe_expert(const void* ptr, uint64_t hash);

    // Evict cold entries (LRU selection-sorted)
    std::vector<void*> evict_cold_lru(size_t count);

    // Clear all entries
    void clear();

    // Get current cache size
    size_t size() const { return entries_.size(); }

private:
    std::vector<ExpertCacheEntry> entries_;
    uint64_t last_eviction_time_;
};

// ============================================================================
// AGGRESSIVE OVERFLOW PRESSURE CONTROLLER
// ============================================================================

class AperturePressureController {
public:
    AperturePressureController();
    ~AperturePressureController();

    // Calculate current pressure [0.0, 1.0+]
    float calculate_pressure(
        size_t vram_used_bytes,
        size_t vram_total_bytes,
        size_t ddr5_allocated_bytes
    );

    // Fast tier detection
    OverflowTier detect_tier_fast(float pressure);

    // Adjust underlying MASM primitives based on pressure
    void adjust_primitives(OverflowTier tier);

    // Double-buffer lifecycle
    struct DoubleBuffer {
        void* active_ptr;
        void* shadow_ptr;
        size_t capacity_bytes;
    };

    DoubleBuffer allocate_double_buffer(size_t size);
    void set_external_double_buffer(DoubleBuffer db, void* new_shadow);
    void stream_to_shadow(const void* src, size_t size);
    void commit_shadow(DoubleBuffer& db);
    void free_double_buffer(DoubleBuffer& db);

    // Expert cache operations
    void cache_expert(void* ptr, uint64_t hash);
    bool probe_expert(void* ptr, uint64_t hash);
    void evict_expert(void* ptr);
    std::vector<void*> evict_cold_experts(size_t count);

    // Prefetch depth control
    void prefetch_swarm_slot(int slot_id, void* addr, size_t bytes);
    uint32_t tune_prefetch(float pressure);

    // Accessors
    float last_pressure() const { return last_pressure_; }
    OverflowTier current_tier() const { return current_tier_; }
    uint64_t timestamp_us() const;

private:
    ExpertLRUCache expert_cache_;
    float last_pressure_;
    OverflowTier current_tier_;
    uint32_t current_prefetch_depth_;
    uint64_t base_time_us_;
};

// ============================================================================
// UNIFIED MEMORY APERTURE INTERFACE
// Integrates pressure controller + double-buffer + expert streaming
// ============================================================================

class UnifiedMemoryAperture {
public:
    struct Config {
        size_t vram_bytes;          // Total VRAM (e.g. 16 GB)
        size_t ddr5_bytes;          // Total DDR5 accessible (e.g. 64 GB)
        uint64_t ddr5_bandwidth_bps; // Bytes per second
        uint64_t pcie_bandwidth_bps; // Bytes per second
        bool use_huge_pages;
    };

    UnifiedMemoryAperture(const Config& cfg);
    ~UnifiedMemoryAperture();

    // Model lifecycle
    bool open_model(const void* model_data, size_t model_size);
    void close();

    // Compute buffer lifecycle
    void* acquire_compute_buffer(size_t size);
    void release_compute_buffer(void* ptr);

    // Expert bypass control
    bool activate_expert_bypass();
    void deactivate_expert_bypass();

    // Prefetch upcoming tensors
    void prefetch_upcoming(void** upcoming_ptrs, size_t count, size_t tensor_size);

    // Allocate/deallocate from aperture (bump-style)
    void* allocate(size_t size);
    void deallocate(void* ptr);

    // Stream expert weights through pipeline
    // 1. Probe cache, 2. Allocate in aperture, 3. NT stream, 4. Cache update
    bool stream_expert(
        size_t expert_id,
        const void* expert_data,
        size_t expert_size
    );

    // Layer swap with double-buffering
    // 1. Init shadow, 2. Stream layer data, 3. Atomic commit
    bool begin_layer_swap();
    bool stream_layer_to_shadow(const void* layer_data, size_t layer_size);
    bool commit_layer_swap();

    // Pressure & predictive control
    float update_pressure();
    float predict_tier_transition(size_t upcoming_bytes);
    void prefetch_for_agent(size_t upcoming_bytes);

    // Swarm-oriented batch operations
    std::vector<void*> proactive_evict_swarm(size_t num_tensors);
    uint8_t expert_dedup_mask(void** experts, size_t num_experts);
    void swarm_assign_slots(uint8_t* agent_experts, size_t num_agents);
    void bandwidth_aware_stream(
        const void* src,
        void* dst,
        size_t size
    );

    // Status
    float current_pressure() const;
    OverflowTier current_tier() const;
    size_t aperture_used_bytes() const { return aperture_used_bytes_; }

private:
    Config config_;
    AperturePressureController pressure_controller_;
    AperturePressureController::DoubleBuffer active_double_buffer_;
    void* aperture_base_;
    size_t aperture_used_bytes_;
    uint64_t model_size_bytes_;
};

// ============================================================================
// INTEGRATION TEST HARNESS
// ============================================================================

class AggressiveOverflowTest {
public:
    static bool run_all();
    static bool test_bandwidth_aware_stream();
    static bool test_pressure_estimation();
    static bool test_double_buffer_swap();
    static bool test_expert_cache();
    static bool test_unified_aperture();
};

} // namespace rawr::speculative::aperture
