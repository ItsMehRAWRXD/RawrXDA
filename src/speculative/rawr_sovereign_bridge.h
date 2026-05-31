// rawr_sovereign_bridge.h
// C++ bridge for DDR5-to-GPU direct aperture bypass
// Integrates with MASM primitives for aggressive memory management

#pragma once
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>
#include <atomic>
#include <mutex>

// MASM function declarations
extern "C" {
    void* RawrAllocateHugePages(size_t size);
    bool RawrPinMemory(void* ptr, size_t size);
    bool RawrUnpinMemory(void* ptr, size_t size);
    void RawrPrefetchMemory(void* ptr, size_t size);
    bool RawrSetThreadAffinityToNUMA0();
    void RawrFlushCacheLines(void* ptr, size_t size);
    void RawrMemoryBarrier();
    uint64_t RawrGetPhysicalAddress(void* ptr);
    bool RawrLargePagesAvailable();
}

namespace rawr {

// Capability flag for aperture-managed tensors
constexpr uint64_t RAWR_CAP_APERTURE = 1ULL << 63;

// ============================================================================
// PRIVILEGE MANAGEMENT (Enable SeLockMemoryPrivilege)
// ============================================================================

class PrivilegeManager {
public:
    static bool EnableLockMemoryPrivilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tp;
        LUID luid;
        
        // Open current process token
        if (!OpenProcessToken(GetCurrentProcess(), 
                              TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, 
                              &hToken)) {
            return false;
        }
        
        // Lookup SeLockMemoryPrivilege
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
// SOVEREIGN BRIDGE CONTROLLER
// ============================================================================

class SovereignBridge {
private:
    void* ddr5_pool_ = nullptr;
    size_t pool_size_ = 0;
    size_t used_bytes_ = 0;
    bool large_pages_active_ = false;
    bool numa_optimized_ = false;
    
    // Aperture allocation tracking
    struct ApertureBlock {
        void* ptr;
        size_t size;
        bool active;
        uint64_t last_access;
    };
    std::vector<ApertureBlock> blocks_;
    std::atomic<uint64_t> access_counter_{0};
    
    // VRAM budget for hot tensors (leave 2GB for overhead)
    static constexpr size_t VRAM_BUDGET = 14ULL * 1024 * 1024 * 1024; // 14GB
    size_t vram_used_ = 0;
    
public:
    SovereignBridge(size_t size_gb = 128) {
        pool_size_ = size_gb * 1024ULL * 1024 * 1024;
        
        // Try to enable large page privilege
        bool privilege_enabled = PrivilegeManager::EnableLockMemoryPrivilege();
        
        if (privilege_enabled && RawrLargePagesAvailable()) {
            // Attempt huge page allocation
            ddr5_pool_ = RawrAllocateHugePages(pool_size_);
            large_pages_active_ = (ddr5_pool_ != nullptr);
        }
        
        if (!ddr5_pool_) {
            // Fallback to standard VirtualAlloc
            ddr5_pool_ = VirtualAlloc(NULL, pool_size_, 
                                      MEM_COMMIT | MEM_RESERVE, 
                                      PAGE_READWRITE);
            large_pages_active_ = false;
        }
        
        if (!ddr5_pool_) {
            // Final fallback: use standard malloc (won't be aperture-compatible)
            ddr5_pool_ = malloc(pool_size_);
        }
        
        if (ddr5_pool_) {
            // Pin memory to prevent swapping
            RawrPinMemory(ddr5_pool_, pool_size_);
            
            // Optimize for NUMA node 0
            numa_optimized_ = RawrSetThreadAffinityToNUMA0();
            
            // Prefetch entire pool to warm memory controller
            RawrPrefetchMemory(ddr5_pool_, std::min(pool_size_, 256ULL * 1024 * 1024));
        }
    }
    
    ~SovereignBridge() {
        if (ddr5_pool_) {
            RawrUnpinMemory(ddr5_pool_, pool_size_);
            
            if (large_pages_active_) {
                // Large pages use VirtualFree with MEM_RELEASE
                VirtualFree(ddr5_pool_, 0, MEM_RELEASE);
            } else {
                VirtualFree(ddr5_pool_, 0, MEM_RELEASE);
            }
        }
    }
    
    // Check if a tensor should use aperture (overflow detection)
    bool ShouldUseAperture(size_t tensor_size) const {
        // If tensor won't fit in remaining VRAM budget, use aperture
        if (vram_used_ + tensor_size > VRAM_BUDGET) {
            return true;
        }
        // Large tensors (>500MB) always use aperture to preserve VRAM
        if (tensor_size > 500ULL * 1024 * 1024) {
            return true;
        }
        return false;
    }
    
    // Allocate space in DDR5 pool for aperture tensor
    void* AllocateApertureSpace(size_t size) {
        // Align to 2MB boundary for huge page efficiency
        size = (size + 0x1FFFFF) & ~0x1FFFFF;
        
        if (used_bytes_ + size > pool_size_) {
            // Pool exhausted - evict oldest block
            EvictOldestBlock(size);
        }
        
        void* ptr = static_cast<char*>(ddr5_pool_) + used_bytes_;
        used_bytes_ += size;
        
        blocks_.push_back({ptr, size, true, ++access_counter_});
        
        return ptr;
    }
    
    // Activate tensor for GPU access (prefetch + flush)
    void* ActivateAperture(void* weight_ptr, size_t tensor_size) {
        // 1. Prefetch into CPU cache to prime memory controller
        RawrPrefetchMemory(weight_ptr, std::min(tensor_size, 64ULL * 1024 * 1024));
        
        // 2. Flush cache lines to ensure GPU sees latest data
        RawrFlushCacheLines(weight_ptr, std::min(tensor_size, 256ULL * 1024 * 1024));
        
        // 3. Memory barrier for ordering
        RawrMemoryBarrier();
        
        // Update access tracking
        for (auto& block : blocks_) {
            if (block.ptr == weight_ptr) {
                block.last_access = ++access_counter_;
                break;
            }
        }
        
        return weight_ptr;
    }
    
    // Mark tensor as resident in VRAM
    void MarkVRAMResident(size_t size) {
        vram_used_ += size;
    }
    
    // Release VRAM reservation
    void ReleaseVRAM(size_t size) {
        if (size <= vram_used_) {
            vram_used_ -= size;
        }
    }
    
    // Getters
    bool LargePagesActive() const { return large_pages_active_; }
    bool NUMAOptimized() const { return numa_optimized_; }
    size_t PoolSize() const { return pool_size_; }
    size_t UsedBytes() const { return used_bytes_; }
    size_t VRAMUsed() const { return vram_used_; }
    size_t VRAMBudget() const { return VRAM_BUDGET; }
    void* PoolBase() const { return ddr5_pool_; }
    
private:
    void EvictOldestBlock(size_t required_size) {
        // Simple LRU eviction
        uint64_t oldest_access = access_counter_.load();
        size_t oldest_idx = 0;
        
        for (size_t i = 0; i < blocks_.size(); i++) {
            if (blocks_[i].last_access < oldest_access) {
                oldest_access = blocks_[i].last_access;
                oldest_idx = i;
            }
        }
        
        if (oldest_idx < blocks_.size()) {
            blocks_[oldest_idx].active = false;
            // Note: In production, would actually move data or compact pool
        }
    }
};

// ============================================================================
// ASYNC PREFETCH WORKER (for look-ahead scheduling)
// ============================================================================

class PrefetchWorker {
private:
    HANDLE thread_ = NULL;
    HANDLE event_ = NULL;
    std::atomic<bool> running_{false};
    
    struct PrefetchRequest {
        void* ptr;
        size_t size;
    };
    std::vector<PrefetchRequest> queue_;
    CRITICAL_SECTION queue_lock_;
    
    static DWORD WINAPI WorkerThread(LPVOID param) {
        auto* worker = static_cast<PrefetchWorker*>(param);
        worker->Run();
        return 0;
    }
    
    void Run() {
        while (running_) {
            WaitForSingleObject(event_, INFINITE);
            
            std::vector<PrefetchRequest> batch;
            {
                EnterCriticalSection(&queue_lock_);
                batch = std::move(queue_);
                queue_.clear();
                LeaveCriticalSection(&queue_lock_);
            }
            
            for (const auto& req : batch) {
                RawrPrefetchMemory(req.ptr, req.size);
            }
        }
    }
    
public:
    PrefetchWorker() {
        InitializeCriticalSection(&queue_lock_);
        event_ = CreateEvent(NULL, FALSE, FALSE, NULL);
    }
    
    ~PrefetchWorker() {
        Stop();
        DeleteCriticalSection(&queue_lock_);
        CloseHandle(event_);
    }
    
    void Start() {
        if (!running_) {
            running_ = true;
            thread_ = CreateThread(NULL, 0, WorkerThread, this, 0, NULL);
            RawrSetThreadAffinityToNUMA0();
        }
    }
    
    void Stop() {
        if (running_) {
            running_ = false;
            SetEvent(event_);
            WaitForSingleObject(thread_, INFINITE);
            CloseHandle(thread_);
            thread_ = NULL;
        }
    }
    
    void QueuePrefetch(void* ptr, size_t size) {
        EnterCriticalSection(&queue_lock_);
        queue_.push_back({ptr, size});
        LeaveCriticalSection(&queue_lock_);
        SetEvent(event_);
    }
};

// ============================================================================
// GLOBAL BRIDGE INSTANCE
// ============================================================================

inline SovereignBridge& GetSovereignBridge(size_t aperture_size_gb = 64) {
    static std::unique_ptr<SovereignBridge> bridge;
    static std::mutex bridge_mtx;
    std::lock_guard<std::mutex> lock(bridge_mtx);
    if (!bridge || !bridge->PoolBase()) {
        size_t trial_gb = aperture_size_gb;
        if (trial_gb == 0) trial_gb = 64;

        // Retry with smaller pools instead of hard fail (e.g. strict commit limits).
        while (trial_gb >= 8) {
            bridge = std::make_unique<SovereignBridge>(trial_gb);
            if (bridge->PoolBase()) {
                break;
            }
            trial_gb = (trial_gb * 3) / 4;
        }

        if (!bridge || !bridge->PoolBase()) {
            // Final attempt with minimal practical pool.
            bridge = std::make_unique<SovereignBridge>(8);
        }
    }
    return *bridge;
}

inline PrefetchWorker& GetPrefetchWorker() {
    static PrefetchWorker worker;
    return worker;
}

// ============================================================================
// HELPER FUNCTIONS
// ============================================================================

// Initialize the sovereign bridge system
inline bool InitializeSovereignBridge(size_t aperture_size_gb = 128) {
    auto& bridge = GetSovereignBridge(aperture_size_gb);
    auto& worker = GetPrefetchWorker();
    
    if (!bridge.PoolBase()) {
        return false;
    }
    
    worker.Start();
    
    return true;
}

// Shutdown the sovereign bridge system
inline void ShutdownSovereignBridge() {
    GetPrefetchWorker().Stop();
}

// Check if large pages are active
inline bool LargePagesEnabled() {
    return GetSovereignBridge().LargePagesActive();
}

// Get aperture statistics
inline void GetApertureStats(size_t& pool_size, size_t& used, size_t& vram_used) {
    auto& bridge = GetSovereignBridge();
    pool_size = bridge.PoolSize();
    used = bridge.UsedBytes();
    vram_used = bridge.VRAMUsed();
}

} // namespace rawr
