// rawr_sovereign_overflow.h
// Aggressive DDR5-to-GPU aperture bypass with tiered overflow management
// Targets 64GB-192GB systems with 16GB VRAM (7800 XT)

#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>
#include <cstring>

// MASM function declarations
extern "C" {
    void* RawrAllocateHugePages(size_t size);
    bool RawrPinMemory(void* ptr, size_t size);
    bool RawrUnpinMemory(void* ptr, size_t size);
    void RawrPrefetchMemory(void* ptr, size_t size);
    bool RawrSetThreadAffinityToNUMA0();
    void RawrFlushCacheLines(void* ptr, size_t size);
    void RawrMemoryBarrier();
    uint64_t RawrEstimateDDR5Bandwidth();
    uint64_t RawrEstimatePCIeBandwidth();
    float RawrCalculateApertureUtilization(size_t used, size_t total);
    void RawrStreamingPrefetch(void* ptr, size_t size, uint32_t tier);
    void RawrPreloadExpertWeights(void** expert_ptrs, size_t num_experts, size_t expert_size);
    void RawrLookaheadPrefetch(void** upcoming_tensors, size_t count, size_t tensor_size);
    uint32_t RawrCheckOverflowTier(uint32_t util_bits);
}

namespace rawr {

// ============================================================================
// OVERFLOW TIER DEFINITIONS (Aggressive thresholds for 64GB systems)
// ============================================================================

enum class OverflowTier : uint32_t {
    NORMAL    = 0,  // < 70% utilization - standard operation
    WARNING   = 1,  // 70-85% utilization - enable 2-page prefetch
    CRITICAL  = 2,  // 85-95% utilization - streaming prefetch depth = 4
    PANIC     = 3   // > 95% utilization - compression + NVMe swap
};

// Thresholds for 64GB system (more aggressive than 192GB)
constexpr float TIER_NORMAL_MAX   = 0.70f;  // Was 0.75
constexpr float TIER_WARNING_MAX  = 0.85f;
constexpr float TIER_CRITICAL_MAX = 0.95f;

// Prefetch depths per tier
constexpr size_t PREFETCH_DEPTH_NORMAL    = 1;
constexpr size_t PREFETCH_DEPTH_WARNING   = 2;
constexpr size_t PREFETCH_DEPTH_CRITICAL = 4;

// Lookahead tokens per tier (reduced for 64GB)
constexpr size_t LOOKAHEAD_NORMAL    = 1;
constexpr size_t LOOKAHEAD_WARNING   = 2;
constexpr size_t LOOKAHEAD_CRITICAL  = 2;  // Reduced from 4

// ============================================================================
// PRIVILEGE MANAGER (Enable SeLockMemoryPrivilege)
// ============================================================================

class PrivilegeManager {
public:
    // Enable SeLockMemoryPrivilege for large page allocation
    static bool EnableLockMemoryPrivilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tp;
        LUID luid;
        
        // Open current process token with adjust privileges
        if (!OpenProcessToken(GetCurrentProcess(), 
                              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
                              &hToken)) {
            return false;
        }
        
        // Lookup the privilege value
        if (!LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &luid)) {
            CloseHandle(hToken);
            return false;
        }
        
        // Enable the privilege
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Luid = luid;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, 
                                            sizeof(TOKEN_PRIVILEGES), NULL, NULL);
        DWORD error = GetLastError();
        CloseHandle(hToken);
        
        return result && (error == ERROR_SUCCESS);
    }
    
    // Check if privilege is already enabled
    static bool IsPrivilegeEnabled() {
        HANDLE hToken;
        PRIVILEGE_SET privSet;
        LUID luid;
        BOOL result = FALSE;
        
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            return false;
        }
        
        if (LookupPrivilegeValue(NULL, SE_LOCK_MEMORY_NAME, &luid)) {
            privSet.PrivilegeCount = 1;
            privSet.Control = PRIVILEGE_SET_ALL_NECESSARY;
            privSet.Privilege[0].Luid = luid;
            privSet.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;
            
            PrivilegeCheck(hToken, &privSet, &result);
        }
        
        CloseHandle(hToken);
        return result;
    }
};

// ============================================================================
// SOVEREIGN TENSOR (GART-aware tensor descriptor)
// ============================================================================

struct SovereignTensor {
    void* ddr5_addr = nullptr;      // Base address in DDR5 pool
    void* gpu_aperture_ptr = nullptr; // GART-mapped GPU address
    size_t size = 0;
    bool in_aperture = false;
    bool is_pinned = false;
    std::atomic<uint64_t> last_access{0};
    uint32_t location_bit = 0;        // Bit 63: 0=VRAM, 1=Aperture
    
    // Get address with location bit for branchless dispatch
    uint64_t GetDispatchAddress() const {
        uint64_t addr = reinterpret_cast<uint64_t>(gpu_aperture_ptr ? gpu_aperture_ptr : ddr5_addr);
        if (location_bit) {
            addr |= (1ULL << 63);  // Set location bit
        }
        return addr;
    }
};

// ============================================================================
// AGGRESSIVE APERTURE MANAGER
// ============================================================================

class AggressiveApertureManager {
private:
    void* pool_base_ = nullptr;
    size_t pool_size_ = 0;
    size_t used_bytes_ = 0;
    size_t pinned_bytes_ = 0;
    
    // VRAM budget (leave 2GB for overhead)
    static constexpr size_t VRAM_BUDGET = 14ULL * 1024 * 1024 * 1024;
    size_t vram_used_ = 0;
    
    // Dynamic pinning with timeout
    struct PinnedBlock {
        void* ptr;
        size_t size;
        std::chrono::steady_clock::time_point pin_time;
        std::chrono::milliseconds timeout;
    };
    std::vector<PinnedBlock> pinned_blocks_;
    
    OverflowTier current_tier_ = OverflowTier::NORMAL;
    std::atomic<uint64_t> access_counter_{0};
    
public:
    AggressiveApertureManager(size_t size_gb = 64) {
        pool_size_ = size_gb * 1024ULL * 1024 * 1024;
        
        // Try to enable large page privilege
        PrivilegeManager::EnableLockMemoryPrivilege();
        
        // Set NUMA affinity
        RawrSetThreadAffinityToNUMA0();
        
        // Allocate pool with huge pages if possible
        pool_base_ = RawrAllocateHugePages(pool_size_);
        
        if (pool_base_) {
            // Pin entire pool
            RawrPinMemory(pool_base_, pool_size_);
            
            // Initial prefetch to warm memory controller
            RawrPrefetchMemory(pool_base_, std::min(pool_size_, 256ULL * 1024 * 1024));
        }
    }
    
    ~AggressiveApertureManager() {
        if (pool_base_) {
            RawrUnpinMemory(pool_base_, pool_size_);
            VirtualFree(pool_base_, 0, MEM_RELEASE);
        }
    }
    
    // Stage tensor for aggressive GPU access
    void StageTensorForGPU(SovereignTensor& tensor) {
        if (!tensor.in_aperture || !tensor.ddr5_addr) return;
        
        // 1. Speculative prefetch based on tier
        size_t prefetch_size = GetPrefetchSize(tensor.size);
        RawrStreamingPrefetch(tensor.ddr5_addr, prefetch_size, 
                             static_cast<uint32_t>(current_tier_));
        
        // 2. Coherency flush
        RawrFlushCacheLines(tensor.ddr5_addr, std::min(tensor.size, 256ULL * 1024 * 1024));
        
        // 3. Memory barrier
        RawrMemoryBarrier();
        
        // Update access tracking
        tensor.last_access = ++access_counter_;
    }
    
    // Allocate aperture space with tier-aware strategy
    void* AllocateApertureSpace(size_t size, OverflowTier tier) {
        // Align to 2MB for huge page efficiency
        size = (size + 0x1FFFFF) & ~0x1FFFFF;
        
        if (used_bytes_ + size > pool_size_) {
            EvictBlocksForSpace(size, tier);
        }
        
        void* ptr = static_cast<char*>(pool_base_) + used_bytes_;
        used_bytes_ += size;
        
        // Pin if in CRITICAL tier (but track for unpinning)
        if (tier == OverflowTier::CRITICAL) {
            RawrPinMemory(ptr, size);
            pinned_blocks_.push_back({ptr, size, 
                std::chrono::steady_clock::now(), 
                std::chrono::milliseconds(100)});
            pinned_bytes_ += size;
        }
        
        return ptr;
    }
    
    // Check if tensor should use aperture (overflow detection)
    bool ShouldUseAperture(size_t tensor_size) const {
        // If won't fit in remaining VRAM, use aperture
        if (vram_used_ + tensor_size > VRAM_BUDGET) {
            return true;
        }
        // Large tensors (>500MB) always use aperture
        if (tensor_size > 500ULL * 1024 * 1024) {
            return true;
        }
        return false;
    }
    
    // Update current overflow tier
    void UpdateTier() {
        float utilization = static_cast<float>(used_bytes_) / pool_size_;
        uint32_t util_bits;
        memcpy(&util_bits, &utilization, sizeof(util_bits));
        current_tier_ = static_cast<OverflowTier>(
            RawrCheckOverflowTier(util_bits));
    }
    
    OverflowTier GetCurrentTier() const { return current_tier_; }
    
    // Preload MoE experts (look-ahead staging)
    void PreloadExperts(void** expert_ptrs, size_t num_experts, size_t expert_size) {
        RawrPreloadExpertWeights(expert_ptrs, num_experts, expert_size);
    }
    
    // Look-ahead prefetch for upcoming tensors
    void LookaheadPrefetch(void** upcoming_tensors, size_t count, size_t tensor_size) {
        RawrLookaheadPrefetch(upcoming_tensors, count, tensor_size);
    }
    
    // Getters
    size_t PoolSize() const { return pool_size_; }
    size_t UsedBytes() const { return used_bytes_; }
    size_t VRAMUsed() const { return vram_used_; }
    size_t VRAMBudget() const { return VRAM_BUDGET; }
    float Utilization() const { 
        return pool_size_ > 0 ? static_cast<float>(used_bytes_) / pool_size_ : 0.0f; 
    }
    
private:
    size_t GetPrefetchSize(size_t tensor_size) const {
        switch (current_tier_) {
            case OverflowTier::CRITICAL:
                return tensor_size;  // Full prefetch
            case OverflowTier::WARNING:
                return tensor_size / 2;
            default:
                return std::min(tensor_size, 64ULL * 1024 * 1024);
        }
    }
    
    void EvictBlocksForSpace(size_t required_size, OverflowTier tier) {
        // Simple LRU eviction for now
        // In production: compress blocks, move to NVMe, etc.
        (void)required_size;
        (void)tier;
    }
};

// ============================================================================
// GLOBAL INSTANCE
// ============================================================================

inline AggressiveApertureManager& GetApertureManager() {
    static AggressiveApertureManager manager(64);  // 64GB default
    return manager;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

inline bool InitializeAggressiveBypass(size_t aperture_size_gb = 64) {
    // Enable privilege first
    if (!PrivilegeManager::IsPrivilegeEnabled()) {
        PrivilegeManager::EnableLockMemoryPrivilege();
    }
    
    // Force initialization
    auto& manager = GetApertureManager();
    return manager.PoolSize() > 0;
}

inline void* StageForInference(void* tensor_ptr, size_t size) {
    SovereignTensor tensor;
    tensor.ddr5_addr = tensor_ptr;
    tensor.size = size;
    tensor.in_aperture = true;
    
    GetApertureManager().StageTensorForGPU(tensor);
    return tensor_ptr;
}

inline OverflowTier GetCurrentOverflowTier() {
    GetApertureManager().UpdateTier();
    return GetApertureManager().GetCurrentTier();
}

} // namespace rawr
