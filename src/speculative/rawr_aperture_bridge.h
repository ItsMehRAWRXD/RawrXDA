// rawr_aperture_bridge.h
// C++ bridge for DDR5-to-GPU direct aperture bypass
// Handles privilege escalation and integrates with architecture-agnostic runtime

#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <functional>

namespace rawr {

// ============================================================================
// ASM FUNCTION DECLARATIONS (from rawr_aperture_bypass.asm)
// ============================================================================

extern "C" {
    void* RawrAllocateHugePages(size_t size);
    bool RawrPinMemory(void* ptr, size_t size);
    bool RawrUnpinMemory(void* ptr, size_t size);
    void RawrPrefetchMemory(void* ptr, size_t size);
    void RawrAggressiveStream(void* src, void* dst, size_t size);
    bool RawrSetThreadAffinityToNUMA0();
    void RawrFlushCacheLines(void* ptr, size_t size);
    void RawrMemoryBarrier();
    uint64_t RawrGetPhysicalAddress(void* ptr);
    bool RawrLargePagesAvailable();
    
    // New aggressive overflow management functions
    uint32_t RawrCheckOverflowTier(uint32_t util_bits);
    bool RawrActivateApertureBypass(void* ddr5_base, size_t size, uint32_t flags);
    bool RawrDeactivateApertureBypass(void* ddr5_base, size_t size);
    void RawrStreamingPrefetch(void* ptr, size_t size, uint32_t tier);
    void RawrPreloadExpertWeights(void** expert_ptrs, size_t num_experts, size_t expert_size);
    uint64_t RawrEstimateDDR5Bandwidth();
    uint64_t RawrEstimatePCIeBandwidth();
    float RawrCalculateApertureUtilization(size_t used, size_t total);
    void RawrLookaheadPrefetch(void** upcoming_tensors, size_t count, size_t tensor_size);
    
    // Aggressive overflow management functions
    uint32_t RawrPredictOverflowTime(uint32_t current_bits, uint32_t growth_bits, uint64_t total_memory);
    uint32_t RawrAdjustThrottleForBandwidth(uint32_t base_throttle, uint64_t ddr5_bw, uint64_t pcie_bw, uint64_t gpu_compute_bw);
    void RawrRecordTierTransition(uint32_t from_tier, uint32_t to_tier);
    void RawrAggressivePrefetch(void* ptr, size_t size, uint32_t tier, uint64_t available_bw);
    uint32_t RawrCheckOverflowTierAggressive(uint32_t util_bits);
    uint32_t RawrPredictiveTierPromotion(uint32_t util_bits, uint32_t growth_bits, uint64_t ddr5_bw, uint64_t pcie_bw);
    uint32_t RawrBandwidthAwareThrottle(uint32_t util_bits, uint64_t ddr5_bw, uint64_t pcie_bw, uint32_t current_tier);
    size_t   RawrEmergencyTensorCompress(void* src, void* dst, size_t size, uint32_t tier);
    uint32_t RawrProactiveColdEviction(void** tensor_ptrs, uint64_t* last_access, size_t count,
                                       uint64_t current_time, uint64_t threshold_ns, uint32_t tier);
    void     RawrLookaheadPrefetchDeep(void** upcoming_tensors, size_t count, size_t tensor_size, uint32_t depth);
    void     RawrGetExtendedOverflowStats(uint32_t* tier_counts, uint64_t* total_evictions,
                                          uint64_t* total_bytes, uint32_t* predictive_promos,
                                          uint32_t* emergency_comps, uint32_t* proactive_evicts,
                                          uint32_t* bw_throttles);

    // NT streaming + double-buffer primitives (100 TPS path)
    // Streams DDR5 -> aperture using non-temporal instructions.
    // Best throughput when src/dst are 64B aligned; implementation now has an
    // unaligned-safe fallback path for stress scenarios.
    void  RAWR_Aggressive_Stream(const void* src, void* dst, size_t size);

    // Atomically swaps active/shadow buffer pointers and returns previous active pointer.
    // Use immediately before GPU consumes next layer.
    void* RAWR_DoubleBuffer_Swap(void** active_ptr, void* new_shadow);

    // Returns non-zero if expert_id is warm based on last-access timestamps.
    int   RAWR_ExpertCache_Probe(uint64_t expert_id, const uint64_t* last_access_table,
                                  uint64_t current_time_us, uint64_t warm_threshold_us);

    // Flushes cache lines for a host buffer into the PCIe-visible aperture window.
    void  RAWR_PCIe_FlushBarrier(void* ptr, size_t size);

    // Slot-aware prefetch policy for multi-agent swarm routing.
    void  RAWR_SwarmSlot_Prefetch(void* expert_ptr, size_t expert_size, uint32_t agent_slot);

    // Zero-copy barrier primitive for DDR5->PCIe aperture visibility.
    void  RawrClflushoptRange(void* ptr, size_t size);

    // Swarm coordination primitives (8x MoE)
    void     RawrBandwidthAwareStream(void* src, void* dst, size_t size, uint64_t ddr5_bw, uint64_t pcie_bw);
    uint32_t RawrProactiveEvictSwarm(void** tensor_ptrs, uint64_t* last_access, uint32_t* access_count,
                                      size_t count, uint64_t current_time, uint64_t threshold_us,
                                      uint32_t min_access_count);
    uint8_t  RawrExpertDedupMask(void** expert_ptrs, uint64_t* expert_hashes,
                                   uint64_t* aperture_hashes, size_t num_experts, size_t aperture_count);
    void     RawrSwarmAssignSlots(uint8_t* agent_expert_ids, size_t num_agents,
                                   uint8_t* slot_assignments, uint8_t* slot_expert_cache, size_t num_slots);
}

// ============================================================================
// PRIVILEGE MANAGEMENT
// ============================================================================

class PrivilegeManager {
public:
    // Enable SeLockMemoryPrivilege for large page allocation
    static bool EnableLockMemoryPrivilege();
    
    // Check if large pages are available (privilege + system support)
    static bool CanUseLargePages();
    
    // Get last error message
    static std::string GetLastErrorMessage();
    
private:
    static bool SetPrivilege(HANDLE hToken, LPCTSTR lpszPrivilege, BOOL bEnablePrivilege);
};

// ============================================================================
// APERTURE MEMORY ALLOCATOR
// ============================================================================

enum class MemoryLocation {
    VRAM_LOCAL,      // Physical GPU VRAM (0-16GB)
    DDR5_APerture,   // DDR5 via GART (16GB-208GB)
    SYSTEM_RAM       // Standard system memory (unpinned)
};

struct ApertureAllocation {
    void* cpu_ptr = nullptr;           // CPU-accessible address
    void* gpu_ptr = nullptr;           // GPU virtual address (GART-mapped)
    size_t size = 0;
    MemoryLocation location = MemoryLocation::SYSTEM_RAM;
    bool pinned = false;
    bool large_pages = false;
    
    ~ApertureAllocation() { release(); }
    
    void release();
    bool pin();
    bool unpin();
    void prefetch() const;
    void flush_cache() const;
};

class ApertureAllocator {
public:
    // Initialize allocator (must call after EnableLockMemoryPrivilege)
    static bool Initialize();
    
    // Allocate memory in aperture space
    static std::unique_ptr<ApertureAllocation> Allocate(
        size_t size, 
        bool use_large_pages = true,
        bool pin = true);
    
    // Map existing memory to GPU aperture
    static bool MapToGPUAperture(ApertureAllocation& alloc);
    static bool UnmapFromGPUAperture(ApertureAllocation& alloc);
    
    // Get total aperture size (VRAM + mapped DDR5)
    static size_t GetTotalApertureSize();
    static size_t GetUsedApertureSize();
    static size_t GetAvailableApertureSize();
    
private:
    static size_t s_total_aperture;
    static size_t s_used_aperture;
    static bool s_initialized;
};

// ============================================================================
// DUAL-STAGE POINTER SYSTEM
// ============================================================================

// 64-bit pointer with embedded location bit
// Bit 63: 0 = VRAM, 1 = Aperture
using DualPointer = uint64_t;

inline DualPointer MakeDualPointer(void* ptr, MemoryLocation loc) {
    uint64_t p = reinterpret_cast<uint64_t>(ptr);
    if (loc == MemoryLocation::DDR5_APerture) {
        p |= (1ULL << 63);  // Set aperture bit
    } else {
        p &= ~(1ULL << 63); // Clear aperture bit
    }
    return p;
}

inline void* GetPointer(DualPointer dp) {
    return reinterpret_cast<void*>(dp & ~(1ULL << 63));
}

inline MemoryLocation GetLocation(DualPointer dp) {
    return (dp & (1ULL << 63)) ? MemoryLocation::DDR5_APerture : MemoryLocation::VRAM_LOCAL;
}

// ============================================================================
// AGGRESSIVE OVERFLOW MANAGEMENT - TIERED THRESHOLDS
// ============================================================================

// Overflow tier constants (matching assembly)
enum class OverflowTier : uint32_t {
    NORMAL = 0,      // < 70% utilization (was 75% for 192GB, lowered for 64GB)
    WARNING = 1,    // 70-85% utilization - start prefetch
    THROTTLE = 2,   // 85-95% utilization - enable bypass
    CRITICAL = 3    // > 95% utilization - PANIC: direct DDR5 path + compression
};

// ============================================================================
// OVERFLOW STATISTICS
// ============================================================================

struct OverflowStats {
    uint32_t tier_counts[4];      // Count of events per tier
    uint64_t total_evictions;     // Total number of evictions
    uint64_t total_bytes_evicted; // Total bytes evicted
    
    void reset() {
        memset(tier_counts, 0, sizeof(tier_counts));
        total_evictions = 0;
        total_bytes_evicted = 0;
    }
};

// ============================================================================
// TIER TRANSITION HISTOGRAM
// ============================================================================

struct TierTransitionHistogram {
    uint32_t transitions[4][4];  // [from_tier][to_tier]
    
    void record(uint32_t from_tier, uint32_t to_tier) {
        if (from_tier < 4 && to_tier < 4) {
            transitions[from_tier][to_tier]++;
        }
    }
    
    void reset() {
        memset(transitions, 0, sizeof(transitions));
    }
    
    uint32_t total_transitions() const {
        uint32_t total = 0;
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < 4; j++) {
                total += transitions[i][j];
            }
        }
        return total;
    }
};

// Tier thresholds (configurable for 64GB systems)
struct OverflowThresholds {
    float tier1_warning = 0.70f;    // 70% - start prefetch (was 75% for 192GB)
    float tier2_throttle = 0.85f;   // 85% - enable bypass
    float tier3_critical = 0.95f;   // 95% - PANIC: compression + NVMe swap
    
    // Absolute thresholds for 64GB system RAM
    // tier1: 44.8 GB, tier2: 54.4 GB, tier3: 60.8 GB
    static constexpr size_t RAM_64GB = 64ULL * 1024 * 1024 * 1024;
    static constexpr size_t VRAM_16GB = 16ULL * 1024 * 1024 * 1024;
    static constexpr size_t UNIFIED_POOL_80GB = RAM_64GB + VRAM_16GB;
};

// ============================================================================
// APERTURE BYPASS CONTROLLER
// ============================================================================

class ApertureBypassController {
public:
    // Bypass flags
    enum BypassFlags : uint32_t {
        FLAG_PREFETCH = 0x01,        // Enable prefetch
        FLAG_NON_COHERENT = 0x02,    // Bypass cache coherency
        FLAG_READ_ONLY = 0x04,       // Read-only weights
        FLAG_AGGRESSIVE = 0x08       // Maximum aggression
    };
    
    // Initialize bypass controller
    static bool Initialize(const OverflowThresholds& thresholds = {});
    
    // Check current overflow tier based on utilization
    static OverflowTier CheckOverflowTier(float utilization);
    
    // Activate bypass for a memory region
    static bool ActivateBypass(void* ddr5_base, size_t size, uint32_t flags);
    
    // Deactivate bypass
    static bool DeactivateBypass(void* ddr5_base, size_t size);
    
    // Streaming prefetch with tier awareness
    static void StreamingPrefetch(void* ptr, size_t size, OverflowTier tier);
    
    // Lookahead prefetch for DAG execution
    static void LookaheadPrefetch(void** upcoming_tensors, size_t count, size_t tensor_size);
    
    // Preload MoE expert weights
    static void PreloadExpertWeights(void** expert_ptrs, size_t num_experts, size_t expert_size);
    
    // Get bandwidth estimates
    static uint64_t GetDDR5Bandwidth();
    static uint64_t GetPCIeBandwidth();
    
    // Calculate utilization
    static float CalculateUtilization(size_t used, size_t total);
    
    // Get current thresholds
    static const OverflowThresholds& GetThresholds();
    
    // Configure thresholds
    static void ConfigureThresholds(const OverflowThresholds& thresholds);
    
    // Get overflow statistics
    static OverflowStats GetOverflowStats();
    
    // Reset overflow statistics
    static void ResetOverflowStats();
    
    // Predict time until overflow (in milliseconds)
    static uint32_t PredictOverflowTime(float current_util, float growth_rate);
    
    // Adjust throttle based on bandwidth bottlenecks
    static uint32_t AdjustThrottleForBandwidth(
        uint32_t base_throttle,
        uint64_t ddr5_bw,
        uint64_t pcie_bw,
        uint64_t gpu_compute_bw);
    
    // Record tier transition for analysis
    static void RecordTierTransition(uint32_t from_tier, uint32_t to_tier);
    
    // Aggressive prefetch with bandwidth awareness
    static void AggressivePrefetch(
        void* ptr,
        size_t size,
        OverflowTier tier,
        uint64_t available_bandwidth);
    
    // Proactive eviction of cold tensors
    static void ProactiveEvict(
        void** tensor_ptrs,
        uint64_t* last_access_times,
        size_t count,
        uint64_t current_time,
        uint64_t threshold_ns);
    
    // Tier-aware allocation
    static void* TierAwareAlloc(
        size_t size,
        OverflowTier tier,
        uint32_t flags);
    
    // Emergency compression path
    static void EmergencyCompress(
        void* src,
        void* dst,
        size_t size,
        uint32_t compression_level);
    
private:
    static OverflowThresholds s_thresholds;
    static bool s_initialized;
    static size_t s_total_aperture;
    static size_t s_used_aperture;
    static OverflowStats s_overflow_stats;
    static TierTransitionHistogram s_transition_histogram;
};

// ============================================================================
// TENSOR MEMORY MANAGER
// ============================================================================

struct TensorMemory {
    std::string name;
    DualPointer data;
    size_t size;
    uint32_t layer_idx;
    bool is_expert;  // For MoE models
    
    // Prefetch next tensor while current is computing
    void PrefetchAsync() const;
    void Flush() const;
};

class TensorMemoryManager {
public:
    // Initialize with model size estimate
    static bool Initialize(size_t estimated_model_size);
    
    // Allocate tensor memory
    static TensorMemory* AllocateTensor(
        const std::string& name,
        size_t size,
        uint32_t layer_idx,
        bool is_expert = false);
    
    // Get tensor by name
    static TensorMemory* GetTensor(const std::string& name);
    
    // Pre-fetch expert weights for MoE
    static void PrefetchExperts(const std::vector<uint32_t>& expert_indices);
    
    // Release all allocations
    static void ReleaseAll();
    
    // Get memory statistics
    static void GetStats(size_t& vram_used, size_t& aperture_used, size_t& total_used);
    
private:
    static std::vector<std::unique_ptr<TensorMemory>> s_tensors;
    static size_t s_vram_budget;      // 16GB for 7800 XT
    static size_t s_aperture_budget;  // 192GB DDR5
    static size_t s_vram_used;
    static size_t s_aperture_used;
};

// ============================================================================
// EXECUTION DAG INTEGRATION
// ============================================================================

// Node types for dual-stage execution
enum class ExecutionStage {
    VRAM_COMPUTE,      // Standard VRAM execution
    APERTURE_STREAM,   // Stream from DDR5 aperture
    PREFETCH_HINT      // Prefetch next tensor
};

struct DualStageNode {
    ExecutionStage stage;
    DualPointer tensor_ptr;
    size_t size;
    std::function<void()> kernel;
    std::vector<uint32_t> prefetch_hints;  // Expert indices to prefetch
};

class DualStageExecutor {
public:
    // Initialize executor
    static bool Initialize();
    
    // Execute node with automatic stage selection
    static void Execute(const DualStageNode& node);
    
    // Batch execute with prefetching
    static void ExecuteBatch(const std::vector<DualStageNode>& nodes);
    
    // Set VRAM budget (default 16GB for 7800 XT)
    static void SetVRAMBudget(size_t bytes);
    
private:
    static size_t s_vram_budget;
    static size_t s_vram_used;
};

// ============================================================================
// GPU DIRECT ACCESS (AMD 7800 XT specific)
// ============================================================================

#ifdef RAWR_ENABLE_HIP
#include <hip/hip_runtime.h>

class GPUDirectAccess {
public:
    // Register DDR5 memory with GPU for direct access
    static bool RegisterMemory(void* ptr, size_t size);
    static bool UnregisterMemory(void* ptr);
    
    // Get GPU virtual address for registered memory
    static void* GetGPUVirtualAddress(void* cpu_ptr);
    
    // Async prefetch to GPU (overlaps with compute)
    static bool PrefetchAsync(void* ptr, size_t size, hipStream_t stream);
    
    // Check if pointer is in registered region
    static bool IsRegistered(void* ptr);
    
private:
    static std::unordered_map<void*, void*> s_cpu_to_gpu;
};

#endif // RAWR_ENABLE_HIP

// ============================================================================
// PERFORMANCE MONITORING
// ============================================================================

struct ApertureMetrics {
    size_t bytes_transferred = 0;
    size_t transfer_time_us = 0;
    size_t prefetch_hits = 0;
    size_t prefetch_misses = 0;
    size_t tlb_misses = 0;
    
    double GetBandwidthGBps() const {
        return transfer_time_us > 0 ? 
            (bytes_transferred / (1024.0 * 1024.0 * 1024.0)) / (transfer_time_us / 1e6) : 0.0;
    }
    
    double GetPrefetchHitRate() const {
        size_t total = prefetch_hits + prefetch_misses;
        return total > 0 ? (double)prefetch_hits / total : 0.0;
    }
};

class ApertureProfiler {
public:
    static void BeginTransfer();
    static void EndTransfer(size_t bytes);
    static void RecordPrefetchHit();
    static void RecordPrefetchMiss();
    static void RecordTLBMiss();
    
    static ApertureMetrics GetMetrics();
    static void ResetMetrics();
    static void PrintReport();
    
private:
    static ApertureMetrics s_metrics;
    static size_t s_transfer_start_time;
};

// ============================================================================
// CONVENIENCE API
// ============================================================================

// One-call initialization
inline bool InitializeApertureSystem() {
    if (!PrivilegeManager::EnableLockMemoryPrivilege()) {
        return false;
    }
    if (!ApertureAllocator::Initialize()) {
        return false;
    }
    if (!TensorMemoryManager::Initialize(200ULL * 1024 * 1024 * 1024)) { // 200GB
        return false;
    }
    return DualStageExecutor::Initialize();
}

// Allocate model weights with automatic VRAM/aperture placement
inline TensorMemory* AllocateModelWeights(
    const std::string& name,
    size_t size,
    uint32_t layer_idx,
    bool is_hot_path = false) {
    
    // Hot path (attention) goes to VRAM, cold path (MoE experts) to aperture
    bool use_vram = is_hot_path && (size < 1024 * 1024 * 1024); // < 1GB
    
    auto* tensor = TensorMemoryManager::AllocateTensor(name, size, layer_idx, !use_vram);
    
    if (tensor) {
        // Check if this is an aperture tensor (bit 63 set in DualPointer)
        MemoryLocation loc = GetLocation(tensor->data);
        if (loc == MemoryLocation::DDR5_APerture) {
            // Prefetch aperture tensors
            tensor->PrefetchAsync();
        }
    }
    
    return tensor;
}

} // namespace rawr
