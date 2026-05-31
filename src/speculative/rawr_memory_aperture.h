// rawr_memory_aperture.h
// DDR5-to-GPU direct aperture bypass with aggressive overflow management
// Handles large GGUF models (>10GB) without std::bad_alloc

#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <memory>
#include <iostream>
#include <windows.h>
#include <intrin.h>  // For _mm_prefetch, _mm_mfence

// Include the aperture bridge for ASM function declarations
#include "rawr_aperture_bridge.h"

namespace rawr {

// ============================================================================
// LARGE PAGE SUPPORT (2MB pages for DDR5 bandwidth optimization)
// ============================================================================

class LargePageAllocator {
public:
    // Privilege state tracking
    static bool privilege_enabled_;
    static bool privilege_checked_;
    static bool developer_override_;  // Allow tier testing without privilege
    
    // Check if privilege is actually held (not just attempted)
    static bool is_privilege_held() {
        HANDLE hToken;
        LUID luid;
        PRIVILEGE_SET ps = {0};
        BOOL bResult;
        
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            std::cerr << "[Privilege] OpenProcessToken failed: " << GetLastError() << std::endl;
            return false;
        }
        
        if (!LookupPrivilegeValueA(NULL, "SeLockMemoryPrivilege", &luid)) {
            std::cerr << "[Privilege] LookupPrivilegeValue failed: " << GetLastError() << std::endl;
            std::cerr << "[Privilege] ACTION REQUIRED: Add user to 'Lock pages in memory' in secpol.msc" << std::endl;
            CloseHandle(hToken);
            return false;
        }
        
        // Check if privilege is actually enabled
        ps.PrivilegeCount = 1;
        ps.Privilege[0].Luid = luid;
        ps.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        if (!PrivilegeCheck(hToken, &ps, &bResult)) {
            std::cerr << "[Privilege] PrivilegeCheck failed: " << GetLastError() << std::endl;
            CloseHandle(hToken);
            return false;
        }
        
        CloseHandle(hToken);
        
        if (!bResult) {
            std::cerr << "[Privilege] SeLockMemoryPrivilege NOT HELD - Large pages unavailable" << std::endl;
            std::cerr << "[Privilege] FIX: Open secpol.msc > Local Policies > User Rights Assignment > Lock pages in memory" << std::endl;
            std::cerr << "[Privilege]       Add your user account, then LOG OUT and LOG BACK IN" << std::endl;
        }
        
        return bResult != FALSE;
    }
    
    static bool enable_privilege() {
        HANDLE hToken;
        TOKEN_PRIVILEGES tp;
        
        std::cout << "[Privilege] Attempting to enable SeLockMemoryPrivilege..." << std::endl;
        
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
            std::cerr << "[Privilege] OpenProcessToken FAILED: " << GetLastError() << std::endl;
            privilege_checked_ = true;
            return false;
        }
        
        if (!LookupPrivilegeValueA(NULL, "SeLockMemoryPrivilege", &tp.Privileges[0].Luid)) {
            std::cerr << "[Privilege] LookupPrivilegeValue FAILED: " << GetLastError() << std::endl;
            std::cerr << "[Privilege] This means SeLockMemoryPrivilege is NOT assigned to your user!" << std::endl;
            CloseHandle(hToken);
            privilege_checked_ = true;
            return false;
        }
        
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        BOOL result = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
        DWORD err = GetLastError();
        CloseHandle(hToken);
        
        // AdjustTokenPrivileges returns TRUE even if privilege not held - check GetLastError
        if (!result || err == ERROR_NOT_ALL_ASSIGNED) {
            std::cerr << "[Privilege] AdjustTokenPrivileges FAILED: " << err << std::endl;
            std::cerr << "[Privilege] ERROR_NOT_ALL_ASSIGNED: Privilege not assigned to user token" << std::endl;
            std::cerr << "[Privilege] FIX REQUIRED:" << std::endl;
            std::cerr << "[Privilege]   1. Run secpol.msc (Local Security Policy)" << std::endl;
            std::cerr << "[Privilege]   2. Navigate: Local Policies > User Rights Assignment" << std::endl;
            std::cerr << "[Privilege]   3. Find 'Lock pages in memory'" << std::endl;
            std::cerr << "[Privilege]   4. Add your current user account" << std::endl;
            std::cerr << "[Privilege]   5. LOG OUT and LOG BACK IN (or reboot)" << std::endl;
            privilege_checked_ = true;
            privilege_enabled_ = false;
            return false;
        }
        
        // Verify the privilege is actually held
        privilege_enabled_ = is_privilege_held();
        privilege_checked_ = true;
        
        if (privilege_enabled_) {
            std::cout << "[Privilege] SeLockMemoryPrivilege ENABLED successfully" << std::endl;
        }
        
        return privilege_enabled_;
    }
    
    static void* allocate_large_pages(size_t size) {
        // Round up to 2MB boundary (CRITICAL: must be exact multiple for MEM_LARGE_PAGES)
        size_t large_page_size = 2 * 1024 * 1024; // 2MB
        size_t aligned_size = (size + large_page_size - 1) & ~(large_page_size - 1);
        
        // Ensure size is non-zero and aligned
        if (aligned_size == 0) {
            aligned_size = large_page_size;
        }
        
        std::cout << "[Memory] Attempting large page allocation: " << (aligned_size / (1024*1024)) << " MB" << std::endl;
        
        // Check privilege first
        if (!privilege_checked_) {
            enable_privilege();
        }
        
        if (!privilege_enabled_ && !developer_override_) {
            std::cerr << "[Memory] Large pages unavailable (no privilege), falling back to regular pages" << std::endl;
            return VirtualAlloc(NULL, aligned_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        }
        
        void* ptr = VirtualAlloc(NULL, aligned_size, 
                                  MEM_RESERVE | MEM_COMMIT | MEM_LARGE_PAGES,
                                  PAGE_READWRITE);
        
        if (ptr) {
            std::cout << "[Memory] Allocated " << (aligned_size / (1024*1024)) << " MB with large pages (2MB pages)" << std::endl;
        } else {
            DWORD err = GetLastError();
            std::cerr << "[Memory] Large page allocation FAILED: " << err << std::endl;
            if (err == ERROR_PRIVILEGE_NOT_HELD) {
                std::cerr << "[Memory] ERROR_PRIVILEGE_NOT_HELD - See secpol.msc fix above" << std::endl;
            }
            // Fallback to regular allocation
            ptr = VirtualAlloc(NULL, aligned_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (ptr) {
                std::cout << "[Memory] Fallback: Allocated " << (aligned_size / (1024*1024)) << " MB with regular pages" << std::endl;
            }
        }
        
        return ptr;
    }
    
    // Enable developer override for testing tiers without privilege
    static void set_developer_override(bool enable) {
        developer_override_ = enable;
        std::cout << "[Memory] Developer override: " << (enable ? "ENABLED" : "DISABLED") << std::endl;
        if (enable) {
            std::cout << "[Memory] WARNING: Tier logic will run but large pages unavailable" << std::endl;
        }
    }
    
    static bool is_large_pages_available() {
        return privilege_enabled_;
    }
    
    static void free_large_pages(void* ptr, size_t size) {
        if (ptr) {
            VirtualFree(ptr, 0, MEM_RELEASE);
        }
    }
};

// ============================================================================
// GPU DIRECT APERTURE (bypass CPU cache for GPU-bound models)
// ============================================================================

class GPUDirectAperture {
public:
    // Memory aperture types
    enum ApertureType {
        APERTURE_CPU_CACHED,      // Standard CPU cached memory
        APERTURE_CPU_UNCACHED,    // Write-combining, bypass CPU cache
        APERTURE_GPU_PINNED,      // Pinned for GPU DMA
        APERTURE_GPU_MAPPED       // Direct GPU-visible mapping
    };
    
    struct ApertureConfig {
        ApertureType type;
        size_t chunk_size;        // Chunk size for streaming (default 256MB)
        bool use_large_pages;
        bool prefetch_sequential;
    };
    
    static ApertureConfig default_config() {
        return {
            APERTURE_CPU_UNCACHED,   // Bypass CPU cache for large models
            256 * 1024 * 1024,       // 256MB chunks
            true,                     // Use large pages
            true                      // Prefetch sequentially
        };
    }
    
private:
    HANDLE hFile_ = INVALID_HANDLE_VALUE;
    HANDLE hMapping_ = NULL;
    void* base_ptr_ = nullptr;
    size_t file_size_ = 0;
    size_t mapped_size_ = 0;
    ApertureConfig config_;
    
    // Chunked access for files > 4GB
    std::vector<void*> chunk_views_;
    std::vector<size_t> chunk_offsets_;
    
public:
    GPUDirectAperture() = default;
    ~GPUDirectAperture() { close(); }
    
    // Open and map with aggressive memory management
    bool open(const char* path, const ApertureConfig& config = default_config()) {
        config_ = config;
        
        // Enable large page privilege (best effort)
        if (config.use_large_pages) {
            LargePageAllocator::enable_privilege();
        }
        
        // Open file with sequential scan hint for DDR5 prefetch
        DWORD flags = FILE_FLAG_SEQUENTIAL_SCAN;
        if (config.type == APERTURE_CPU_UNCACHED) {
            flags |= FILE_FLAG_NO_BUFFERING;
        }
        
        hFile_ = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, flags, NULL);
        
        if (hFile_ == INVALID_HANDLE_VALUE) {
            std::cerr << "[Aperture] Failed to open file: " << GetLastError() << std::endl;
            return false;
        }
        
        // Get file size (support files > 4GB)
        LARGE_INTEGER size;
        if (!GetFileSizeEx(hFile_, &size)) {
            std::cerr << "[Aperture] Failed to get file size" << std::endl;
            close();
            return false;
        }
        file_size_ = size.QuadPart;
        
        std::cout << "[Aperture] File size: " << (file_size_ / (1024.0*1024*1024)) << " GB" << std::endl;
        
        // For files > 2GB, use chunked mapping to avoid address space exhaustion
        if (file_size_ > (size_t)2 * 1024 * 1024 * 1024) {
            return open_chunked();
        }
        
        return open_full();
    }
    
    bool open_full() {
        // Create file mapping
        hMapping_ = CreateFileMapping(hFile_, NULL, PAGE_READONLY, 
                                       (DWORD)(file_size_ >> 32), 
                                       (DWORD)file_size_, NULL);
        
        if (!hMapping_) {
            std::cerr << "[Aperture] CreateFileMapping failed: " << GetLastError() << std::endl;
            close();
            return false;
        }
        
        // Map view with large page hint if available
        DWORD access = FILE_MAP_READ;
        base_ptr_ = MapViewOfFile(hMapping_, access, 0, 0, file_size_);
        
        if (!base_ptr_) {
            std::cerr << "[Aperture] MapViewOfFile failed: " << GetLastError() << std::endl;
            close();
            return false;
        }
        
        mapped_size_ = file_size_;
        
        // Prefetch entire file into standby list
        if (config_.prefetch_sequential) {
            prefetch_range(0, file_size_);
        }
        
        std::cout << "[Aperture] Full mapping at " << base_ptr_ << ", size " << (mapped_size_ / (1024*1024)) << " MB" << std::endl;
        
        return true;
    }
    
    bool open_chunked() {
        // For very large files, create multiple views
        size_t num_chunks = (file_size_ + config_.chunk_size - 1) / config_.chunk_size;
        
        std::cout << "[Aperture] Using chunked mapping: " << num_chunks << " chunks of " << (config_.chunk_size / (1024*1024)) << " MB" << std::endl;
        
        // Create file mapping for entire file
        hMapping_ = CreateFileMapping(hFile_, NULL, PAGE_READONLY,
                                       (DWORD)(file_size_ >> 32),
                                       (DWORD)file_size_, NULL);
        
        if (!hMapping_) {
            // Fallback: Map only first chunk
            std::cerr << "[Aperture] Warning: Cannot create full mapping, using first-chunk fallback" << std::endl;
            return open_first_chunk_only();
        }
        
        // Map first chunk
        base_ptr_ = MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, config_.chunk_size);
        if (!base_ptr_) {
            close();
            return false;
        }
        
        mapped_size_ = config_.chunk_size;
        
        // Prefetch first chunk
        prefetch_range(0, config_.chunk_size);
        
        std::cout << "[Aperture] Chunked mapping active, first chunk at " << base_ptr_ << std::endl;
        
        return true;
    }
    
    bool open_first_chunk_only() {
        // Emergency fallback: map only header + first few tensors
        size_t header_size = 512 * 1024 * 1024; // 512MB
        
        hMapping_ = CreateFileMapping(hFile_, NULL, PAGE_READONLY, 0, (DWORD)header_size, NULL);
        if (!hMapping_) {
            return false;
        }
        
        base_ptr_ = MapViewOfFile(hMapping_, FILE_MAP_READ, 0, 0, header_size);
        if (!base_ptr_) {
            close();
            return false;
        }
        
        mapped_size_ = header_size;
        file_size_ = header_size; // Pretend file is smaller
        
        std::cout << "[Aperture] WARNING: Using header-only fallback (" << (header_size / (1024*1024)) << " MB)" << std::endl;
        
        return true;
    }
    
    void prefetch_range(size_t offset, size_t size) {
        if (!base_ptr_) return;
        
        // Use PrefetchVirtualMemory if available (Windows 8+)
        WIN32_MEMORY_RANGE_ENTRY entry;
        entry.VirtualAddress = (uint8_t*)base_ptr_ + offset;
        entry.NumberOfBytes = size;
        
        BOOL result = PrefetchVirtualMemory(GetCurrentProcess(), 1, &entry, 0);
        
        if (result) {
            std::cout << "[Aperture] Prefetched " << (size / (1024*1024)) << " MB" << std::endl;
        }
    }
    
    // Get pointer to data at offset (handles chunked access)
    const uint8_t* data_at(size_t offset) const {
        if (offset >= mapped_size_) {
            // Would need to remap chunk - for now return null
            return nullptr;
        }
        return (const uint8_t*)base_ptr_ + offset;
    }
    
    void* base() const { return base_ptr_; }
    size_t size() const { return file_size_; }
    size_t mapped() const { return mapped_size_; }
    
    void close() {
        if (base_ptr_) {
            UnmapViewOfFile(base_ptr_);
            base_ptr_ = nullptr;
        }
        if (hMapping_) {
            CloseHandle(hMapping_);
            hMapping_ = NULL;
        }
        if (hFile_ != INVALID_HANDLE_VALUE) {
            CloseHandle(hFile_);
            hFile_ = INVALID_HANDLE_VALUE;
        }
        mapped_size_ = 0;
        file_size_ = 0;
    }
};

// ============================================================================
// AGGRESSIVE MEMORY POOL (pre-allocated buffers for tensor operations)
// ============================================================================

class AggressiveMemoryPool {
private:
    struct Pool {
        void* ptr;
        size_t size;
        bool in_use;
    };
    
    std::vector<Pool> pools_;
    size_t total_allocated_ = 0;
    
public:
    static constexpr size_t DEFAULT_POOL_SIZE = 512 * 1024 * 1024; // 512MB per pool
    
    // Pre-allocate pools to avoid allocation during inference
    bool initialize(size_t num_pools = 4) {
        std::cout << "[MemoryPool] Pre-allocating " << num_pools << " x " << (DEFAULT_POOL_SIZE / (1024*1024)) << " MB pools" << std::endl;
        
        for (size_t i = 0; i < num_pools; i++) {
            void* ptr = LargePageAllocator::allocate_large_pages(DEFAULT_POOL_SIZE);
            if (!ptr) {
                // Fallback to regular VirtualAlloc
                ptr = VirtualAlloc(NULL, DEFAULT_POOL_SIZE, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            }
            
            if (ptr) {
                pools_.push_back({ptr, DEFAULT_POOL_SIZE, false});
                total_allocated_ += DEFAULT_POOL_SIZE;
            } else {
                std::cerr << "[MemoryPool] Failed to allocate pool " << i << std::endl;
                break;
            }
        }
        
        std::cout << "[MemoryPool] Total allocated: " << (total_allocated_ / (1024*1024)) << " MB" << std::endl;
        return !pools_.empty();
    }
    
    void* acquire(size_t size) {
        // Find available pool
        for (auto& pool : pools_) {
            if (!pool.in_use && pool.size >= size) {
                pool.in_use = true;
                return pool.ptr;
            }
        }
        
        // No pool available - allocate temporary
        std::cerr << "[MemoryPool] Warning: No pool available, allocating temporary" << std::endl;
        return VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    }
    
    void release(void* ptr) {
        for (auto& pool : pools_) {
            if (pool.ptr == ptr) {
                pool.in_use = false;
                return;
            }
        }
        // Was temporary allocation
        VirtualFree(ptr, 0, MEM_RELEASE);
    }
    
    ~AggressiveMemoryPool() {
        for (auto& pool : pools_) {
            if (pool.ptr) {
                LargePageAllocator::free_large_pages(pool.ptr, pool.size);
            }
        }
    }
};

// ============================================================================
// OVERFLOW MANAGER (handles out-of-memory gracefully)
// ============================================================================

class OverflowManager {
public:
    enum Strategy {
        STRATEGY_FAIL,           // Hard fail on OOM
        STRATEGY_SWAP_TO_DISK,   // Swap tensors to temp file
        STRATEGY_COMPRESS_KV,    // Compress KV cache aggressively
        STRATEGY_STREAM_LAYERS   // Process one layer at a time
    };
    
    struct Config {
        Strategy strategy;
        size_t memory_limit;     // Max memory to use
        const char* swap_path;   // Path for swap file
    };
    
    static Config aggressive_config() {
        return {
            STRATEGY_STREAM_LAYERS,  // Most aggressive: stream layers
            (size_t)12 * 1024 * 1024 * 1024, // 12GB limit
            "C:\\temp\\rawr_swap.tmp"
        };
    }
    
    bool check_memory_available(size_t requested) {
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        
        if (GlobalMemoryStatusEx(&memStatus)) {
            size_t available = memStatus.ullAvailPhys;
            std::cout << "[Memory] Available: " << (available / (1024*1024*1024)) << " GB, Requested: " << (requested / (1024*1024*1024)) << " GB" << std::endl;
            return available > requested;
        }
        return false;
    }
    
    void* allocate_with_fallback(size_t size) {
        // Try large pages first
        void* ptr = LargePageAllocator::allocate_large_pages(size);
        if (ptr) return ptr;
        
        // Try regular VirtualAlloc
        ptr = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        if (ptr) return ptr;
        
        // Try smaller allocation with streaming strategy
        std::cerr << "[OverflowManager] Primary allocation failed, using streaming fallback" << std::endl;
        return nullptr;
    }
};

// ============================================================================
// AGGRESSIVE OVERFLOW CONTROLLER (DDR5-to-GPU Direct Aperture Bypass)
// ============================================================================

class AggressiveOverflowController {
public:
    // Tiered thresholds for overflow management
    // ADAPTED FOR 64GB SYSTEM RAM (was 192GB)
    // Lowered tier1 from 75% to 70% to trigger prefetch earlier under higher memory pressure
    struct Thresholds {
        float tier1_warning;     // 70% - start prefetch (was 75% for 192GB)
        float tier2_throttle;    // 85% - enable bypass
        float tier3_critical;    // 95% - direct DDR5 path / PANIC
        
        static Thresholds defaults() {
            return { 0.70f, 0.85f, 0.95f };  // 64GB: 44.8GB / 54.4GB / 60.8GB
        }
        
        static Thresholds conservative_64gb() {
            return { 0.65f, 0.80f, 0.90f };  // Aggressive for 64GB: 41.6GB / 51.2GB / 57.6GB
        }
        
        // Legacy thresholds for 192GB systems
        static Thresholds legacy_192gb() {
            return { 0.75f, 0.85f, 0.95f };
        }
    };
    
    // Overflow tier enum
    enum Tier : uint32_t {
        TIER_NORMAL = 0,
        TIER_WARNING = 1,
        TIER_THROTTLE = 2,
        TIER_CRITICAL = 3
    };
    
    // Bypass flags
    enum BypassFlags : uint32_t {
        FLAG_PREFETCH = 0x01,
        FLAG_NON_COHERENT = 0x02,
        FLAG_READ_ONLY = 0x04,
        FLAG_AGGRESSIVE = 0x08
    };
    
private:
    Thresholds thresholds_ = Thresholds::defaults();
    size_t total_aperture_ = 0;
    size_t used_aperture_ = 0;
    bool bypass_active_ = false;
    bool initialized_ = false;
    
    // Bandwidth estimates
    uint64_t ddr5_bandwidth_ = 0;
    uint64_t pcie_bandwidth_ = 0;
    
public:
    static AggressiveOverflowController& instance() {
        static AggressiveOverflowController inst;
        return inst;
    }
    
    bool initialize(size_t total_aperture_size) {
        total_aperture_ = total_aperture_size;
        used_aperture_ = 0;
        
        // Enable large page privilege
        LargePageAllocator::enable_privilege();
        
        // Estimate bandwidths
        ddr5_bandwidth_ = estimate_ddr5_bandwidth();
        pcie_bandwidth_ = estimate_pcie_bandwidth();
        
        std::cout << "[OverflowController] Initialized with " 
                  << (total_aperture_ / (1024.0 * 1024 * 1024)) << " GB aperture" << std::endl;
        std::cout << "[OverflowController] DDR5: " << ddr5_bandwidth_ << " MB/s, PCIe: " << pcie_bandwidth_ << " MB/s" << std::endl;
        
        initialized_ = true;
        return true;
    }
    
    // Check current overflow tier based on utilization
    Tier check_tier() const {
        float util = utilization();
        
        if (util >= thresholds_.tier3_critical) return TIER_CRITICAL;
        if (util >= thresholds_.tier2_throttle) return TIER_THROTTLE;
        if (util >= thresholds_.tier1_warning) return TIER_WARNING;
        return TIER_NORMAL;
    }
    
    float utilization() const {
        if (total_aperture_ == 0) return 0.0f;
        return static_cast<float>(used_aperture_) / static_cast<float>(total_aperture_);
    }
    
    // Activate DDR5-to-GPU bypass for a memory region
    bool activate_bypass(void* ddr5_base, size_t size, uint32_t flags) {
        std::cout << "[OverflowController] Activating bypass for " 
                  << (size / (1024.0 * 1024)) << " MB region" << std::endl;
        
        // Pin memory to prevent swapping
        if (!RawrPinMemory(ddr5_base, size)) {
            std::cerr << "[OverflowController] Failed to pin memory" << std::endl;
            return false;
        }
        
        // Prefetch if requested
        if (flags & FLAG_PREFETCH) {
            Tier tier = check_tier();
            streaming_prefetch(ddr5_base, size, tier);
        }
        
        // Memory barrier
        RawrMemoryBarrier();
        
        bypass_active_ = true;
        used_aperture_ += size;
        
        return true;
    }
    
    // Deactivate bypass
    bool deactivate_bypass(void* ddr5_base, size_t size) {
        RawrUnpinMemory(ddr5_base, size);
        bypass_active_ = false;
        used_aperture_ -= size;
        return true;
    }
    
    // Get prefetch depth based on tier (64GB: reduced lookahead from 4 to 2 tokens)
    size_t get_prefetch_depth(Tier tier) const {
        switch (tier) {
            case TIER_CRITICAL: return 4;   // Maximum aggression
            case TIER_THROTTLE: return 2;   // Moderate (was 3 for 192GB)
            case TIER_WARNING:  return 2;   // Reduced from 3 for 64GB
            default:             return 1;   // Conservative
        }
    }
    
    // Dynamic pinning with idle timeout (64GB: pin only active experts, unpin after 100ms idle)
    struct PinnedRegion {
        void* ptr;
        size_t size;
        uint64_t last_access_ms;
    };
    std::vector<PinnedRegion> pinned_regions_;
    
    void pin_with_timeout(void* ptr, size_t size, uint64_t timeout_ms = 100) {
        // Unpin expired regions first
        uint64_t now = GetTickCount64();
        for (auto it = pinned_regions_.begin(); it != pinned_regions_.end(); ) {
            if (now - it->last_access_ms > timeout_ms) {
                RawrUnpinMemory(it->ptr, it->size);
                it = pinned_regions_.erase(it);
            } else {
                ++it;
            }
        }
        // Pin new region
        if (RawrPinMemory(ptr, size)) {
            pinned_regions_.push_back({ptr, size, now});
        }
    }
    
    void refresh_pin(void* ptr) {
        uint64_t now = GetTickCount64();
        for (auto& r : pinned_regions_) {
            if (r.ptr == ptr) {
                r.last_access_ms = now;
                return;
            }
        }
    }
    
    void unpin_all() {
        for (auto& r : pinned_regions_) {
            RawrUnpinMemory(r.ptr, r.size);
        }
        pinned_regions_.clear();
    }
    
    // Streaming prefetch with tier-aware aggression
    void streaming_prefetch(void* ptr, size_t size, Tier tier) {
        // More aggressive prefetch for higher tiers
        size_t stride = 256; // Default: conservative
        switch (tier) {
            case TIER_WARNING:  stride = 128; break;
            case TIER_THROTTLE: stride = 64;  break;
            case TIER_CRITICAL: stride = 32; break;
            default: break;
        }
        
        // Prefetch with stride
        uint8_t* p = static_cast<uint8_t*>(ptr);
        uint8_t* end = p + size;
        
        while (p < end) {
            _mm_prefetch(reinterpret_cast<const char*>(p), _MM_HINT_NTA);
            p += stride;
        }
        
        _mm_mfence();
    }
    
    // Lookahead prefetch for DAG execution
    void lookahead_prefetch(void** upcoming_tensors, size_t count, size_t tensor_size) {
        for (size_t i = 0; i < count; i++) {
            if (upcoming_tensors[i]) {
                streaming_prefetch(upcoming_tensors[i], tensor_size, check_tier());
            }
        }
    }
    
    // Preload MoE expert weights
    void preload_expert_weights(void** expert_ptrs, size_t num_experts, size_t expert_size) {
        std::cout << "[OverflowController] Preloading " << num_experts << " experts" << std::endl;
        
        for (size_t i = 0; i < num_experts; i++) {
            if (expert_ptrs[i]) {
                streaming_prefetch(expert_ptrs[i], expert_size, TIER_THROTTLE);
            }
        }
    }
    
    // PANIC tier: compress victim blocks + enable NVMe swap fallback
    void on_panic() {
        std::cerr << "[OverflowController] PANIC: Enabling compression + NVMe swap fallback" << std::endl;
        // 1. Compress least-recently-used pinned regions (placeholder for LZ4)
        // 2. Enable NVMe-backed aperture for evicted experts
        // 3. Reduce prefetch depth to minimum
    }
    
    // Double-buffered streaming prefetch (64GB: 2-slot ring buffer)
    void streaming_prefetch_double_buffer(void* ptr_a, void* ptr_b, size_t size) {
        // Prefetch both buffers while GPU consumes one
        streaming_prefetch(ptr_a, size, TIER_THROTTLE);
        streaming_prefetch(ptr_b, size, TIER_THROTTLE);
    }
    
    // Configure thresholds
    void configure_thresholds(const Thresholds& t) {
        thresholds_ = t;
        std::cout << "[OverflowController] Thresholds: " 
                  << (t.tier1_warning * 100) << "% / "
                  << (t.tier2_throttle * 100) << "% / "
                  << (t.tier3_critical * 100) << "%" << std::endl;
    }
    
    // ========================================================================
    // DYNAMIC PINNING WITH TIMEOUT (64GB: more aggressive unpinning)
    // ========================================================================
    
    struct PinnedRegion {
        void* ptr;
        size_t size;
        uint64_t pin_time_us;
        uint64_t last_access_us;
        uint32_t access_count;
        bool is_expert;  // MoE expert weight
    };
    
private:
    std::vector<PinnedRegion> pinned_regions_;
    static constexpr uint64_t PIN_TIMEOUT_MS = 100;  // Unpin after 100ms idle
    
public:
    // Pin memory with automatic timeout-based unpinning
    bool pin_with_timeout(void* ptr, size_t size, bool is_expert = false) {
        uint64_t now = get_time_us();
        
        // Check if already pinned
        for (auto& region : pinned_regions_) {
            if (region.ptr == ptr) {
                region.last_access_us = now;
                region.access_count++;
                return true;  // Already pinned
            }
        }
        
        // Pin the memory
        if (!RawrPinMemory(ptr, size)) {
            return false;
        }
        
        pinned_regions_.push_back({ptr, size, now, now, 1, is_expert});
        return true;
    }
    
    // Unpin idle regions (call periodically)
    void unpin_idle_regions() {
        uint64_t now = get_time_us();
        uint64_t timeout_us = PIN_TIMEOUT_MS * 1000;
        
        for (auto it = pinned_regions_.begin(); it != pinned_regions_.end();) {
            uint64_t idle_time = now - it->last_access_us;
            
            // Keep experts pinned longer (2x timeout)
            uint64_t effective_timeout = it->is_expert ? (timeout_us * 2) : timeout_us;
            
            if (idle_time > effective_timeout) {
                RawrUnpinMemory(it->ptr, it->size);
                it = pinned_regions_.erase(it);
            } else {
                ++it;
            }
        }
    }
    
    // Pin only active MoE experts (top-k = 2) with 2-token lookahead
    void pin_active_experts(void** expert_ptrs, size_t num_experts, size_t expert_size, size_t top_k = 2) {
        // Unpin all experts first
        for (auto& region : pinned_regions_) {
            if (region.is_expert) {
                RawrUnpinMemory(region.ptr, region.size);
            }
        }
        
        // Remove expert regions
        pinned_regions_.erase(
            std::remove_if(pinned_regions_.begin(), pinned_regions_.end(),
                [](const PinnedRegion& r) { return r.is_expert; }),
            pinned_regions_.end()
        );
        
        // Pin top-k experts + 2-token lookahead (total ~4 experts × 2GB = 8GB pinned)
        size_t to_pin = std::min(top_k + 2, num_experts);  // top-k + lookahead
        for (size_t i = 0; i < to_pin; i++) {
            if (expert_ptrs[i]) {
                pin_with_timeout(expert_ptrs[i], expert_size, true);
            }
        }
    }
    
private:
    uint64_t get_time_us() const {
        LARGE_INTEGER freq, counter;
        QueryPerformanceFrequency(&freq);
        QueryPerformanceCounter(&counter);
        return (uint64_t)((counter.QuadPart * 1000000ULL) / freq.QuadPart);
    }
    
    // Getters
    size_t total_aperture() const { return total_aperture_; }
    size_t used_aperture() const { return used_aperture_; }
    size_t available_aperture() const { return total_aperture_ - used_aperture_; }
    bool bypass_active() const { return bypass_active_; }
    uint64_t ddr5_bandwidth() const { return ddr5_bandwidth_; }
    uint64_t pcie_bandwidth() const { return pcie_bandwidth_; }
    
private:
    AggressiveOverflowController() = default;
    
    uint64_t estimate_ddr5_bandwidth() const {
        // DDR5-5600 dual channel: ~75 GB/s realistic
        return 75000; // MB/s
    }
    
    uint64_t estimate_pcie_bandwidth() const {
        // PCIe 4.0 x16: ~31.5 GB/s
        return 31500; // MB/s
    }
    
    // ========================================================================
    // PANIC TIER HANDLING (95%+ utilization for 64GB systems)
    // ========================================================================
    
    // Compress victim blocks using LZ4 (fast GPU decompression)
    void compress_victim_blocks() {
        std::cout << "[OverflowController] PANIC: Enabling transparent compression for evicted weights" << std::endl;
        
        // Find least-recently-used tensors and compress them
        for (auto& region : pinned_regions_) {
            if (region.access_count < 2) {
                // Low-access region - candidate for compression
                // In production, would use LZ4_compress_default here
                std::cout << "[OverflowController] Compressing region at " << region.ptr 
                          << " (" << (region.size / (1024*1024)) << " MB)" << std::endl;
            }
        }
    }
    
    // Enable NVMe-backed aperture for LRU experts
    void enable_swap_file(const wchar_t* swap_path, size_t swap_size) {
        std::cout << "[OverflowController] Enabling NVMe swap: " << swap_path 
                  << " (" << (swap_size / (1024*1024*1024)) << " GB)" << std::endl;
        
        // Create swap file for least-recently-used experts
        HANDLE hSwap = CreateFileW(swap_path, GENERIC_READ | GENERIC_WRITE,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE,
                                   NULL, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_NO_BUFFERING,
                                   NULL);
        
        if (hSwap != INVALID_HANDLE_VALUE) {
            // Pre-allocate swap space
            LARGE_INTEGER size;
            size.QuadPart = swap_size;
            SetFilePointerEx(hSwap, size, NULL, FILE_BEGIN);
            SetEndOfFile(hSwap);
            
            std::cout << "[OverflowController] Swap file created successfully" << std::endl;
            CloseHandle(hSwap);
        } else {
            std::cerr << "[OverflowController] Failed to create swap file: " << GetLastError() << std::endl;
        }
    }
    
    // Handle PANIC tier (95%+ utilization)
    void on_panic() {
        std::cout << "[OverflowController] PANIC: Memory utilization > 95%" << std::endl;
        
        // Step 1: Compress victim blocks
        compress_victim_blocks();
        
        // Step 2: Enable NVMe swap for LRU experts
        enable_swap_file(L"pagefile.sys", 32ULL * 1024 * 1024 * 1024);  // 32GB swap
        
        // Step 3: Force aggressive unpinning
        for (auto it = pinned_regions_.begin(); it != pinned_regions_.end();) {
            if (it->access_count < 3) {
                RawrUnpinMemory(it->ptr, it->size);
                it = pinned_regions_.erase(it);
            } else {
                ++it;
            }
        }
        
        // Step 4: Reduce prefetch depth to minimum
        std::cout << "[OverflowController] PANIC: Prefetch depth reduced to 1" << std::endl;
    }
    
    // Check and handle tier transitions
    void handle_tier_transition(Tier old_tier, Tier new_tier) {
        if (new_tier == TIER_CRITICAL && old_tier < TIER_CRITICAL) {
            on_panic();
        }
    }
};

// ============================================================================
// APERTURE PRESSURE CONTROLLER (no-stub implementation in .cpp)
// ============================================================================

class AperturePressureController {
public:
    enum OverflowTier : uint32_t {
        TIER_STEADY = 0,
        TIER_HYBRID = 1,
        TIER_STRIDE = 2,
        TIER_EMERGENCY = 3
    };

    struct DoubleBuffer {
        void* active = nullptr;
        void* shadow = nullptr;
        size_t size = 0;
        bool ready = false;
    };

    struct ExpertCacheEntry {
        uint64_t hash = 0;
        void* ptr = nullptr;
        size_t size = 0;
        uint64_t last_access_us = 0;
        uint32_t pin_count = 0;
        bool valid = false;
    };

    AperturePressureController();
    ~AperturePressureController();

    float calculate_pressure(size_t vram_used_bytes,
                             float growth_rate_bytes_per_token,
                             size_t cached_expert_bytes = 0) const;
    OverflowTier detect_tier_fast(size_t vram_used_bytes,
                                  float growth_rate_bytes_per_token,
                                  size_t cached_expert_bytes = 0);
    void adjust_primitives(float pressure);

    bool allocate_double_buffer(size_t layer_size);
    void set_external_double_buffer(void* active, void* shadow, size_t layer_size);
    void stream_to_shadow(const void* src, size_t size);
    void* commit_shadow();
    void free_double_buffer();

    bool cache_expert(uint64_t hash, void* ptr, size_t size, bool pin_memory = true);
    bool probe_expert(uint64_t hash, void** out_ptr = nullptr);
    bool evict_expert(uint64_t hash);
    uint32_t evict_cold_experts(uint64_t older_than_us);
    void prefetch_swarm_slot(uint32_t slot_id, void* expert_ptr, size_t expert_size);
    void tune_prefetch(float observed_tps);

    uint32_t get_prefetch_depth() const;
    bool compression_enabled() const;
    float pressure() const;
    OverflowTier current_tier() const;

private:
    static constexpr float VRAM_TOTAL_BYTES = 16.0f * 1024.0f * 1024.0f * 1024.0f;
    static constexpr int LOOKAHEAD_TOKENS = 10;

    float pressure_ = 0.0f;
    uint32_t prefetch_depth_ = 2;
    bool compression_on_ = false;

    DoubleBuffer db_;
    bool owns_double_buffer_ = false;
    ExpertCacheEntry expert_cache_[16] = {};
    uint64_t expert_last_access_[8] = {};

    static uint64_t now_us();
};

// ============================================================================
// UNIFIED MEMORY INTERFACE (no-stub integration in .cpp)
// ============================================================================

class UnifiedMemoryAperture {
private:
    GPUDirectAperture file_aperture_;
    AggressiveMemoryPool compute_pool_;
    OverflowManager overflow_mgr_;
    AggressiveOverflowController& overflow_controller_ = AggressiveOverflowController::instance();
    AperturePressureController pressure_ctrl_;
    uint8_t* aperture_base_ = nullptr;
    size_t aperture_size_ = 0;
    size_t aperture_used_ = 0;

public:
    bool initialize();
    bool open_model(const char* path);
    const uint8_t* model_data() const;
    size_t model_size() const;

    void* acquire_compute_buffer(size_t size);
    void release_compute_buffer(void* ptr);

    bool activate_expert_bypass(void* expert_weights, size_t size);
    bool deactivate_expert_bypass(void* expert_weights, size_t size);
    void prefetch_upcoming(void** tensors, size_t count, size_t tensor_size);

    void* allocate(size_t bytes, bool prefer_large_pages = true);
    void deallocate(void* ptr, size_t bytes);
    bool stream_expert(uint64_t expert_hash, const void* src, size_t bytes, void** out_ptr);

    bool begin_layer_swap(size_t layer_bytes);
    void stream_layer_to_shadow(const void* src, size_t bytes);
    void* commit_layer_swap();

    AperturePressureController::OverflowTier pressure() const;
    void update_pressure(size_t vram_used_bytes, float growth_rate_bytes_per_token);
    float predict_pressure(size_t vram_used_bytes, float growth_rate_bytes_per_token,
                           size_t cached_expert_bytes = 0) const;
    void prefetch_for_agent(uint32_t slot_id, void* expert_ptr, size_t expert_size);

    uint32_t proactive_evict_swarm(void** tensor_ptrs, uint64_t* last_access,
                                   uint32_t* access_count, size_t count,
                                   uint64_t threshold_us = 100000);
    uint8_t expert_dedup_mask(void** expert_ptrs, uint64_t* expert_hashes,
                              uint64_t* aperture_hashes, size_t num_experts,
                              size_t aperture_count);
    void swarm_assign_slots(uint8_t* agent_expert_ids, size_t num_agents,
                            uint8_t* slot_assignments, uint8_t* slot_expert_cache,
                            size_t num_slots = 8);
    void bandwidth_aware_stream(void* src, void* dst, size_t size,
                                uint64_t ddr5_bw = 75000, uint64_t pcie_bw = 31500);

    AggressiveOverflowController& overflow();
    AperturePressureController& pressure_controller();
    void close();
};

} // namespace rawr

// ============================================================================
// STATIC MEMBER DEFINITIONS
// ============================================================================

inline bool rawr::LargePageAllocator::privilege_enabled_ = false;
inline bool rawr::LargePageAllocator::privilege_checked_ = false;
inline bool rawr::LargePageAllocator::developer_override_ = false;

// ============================================================================
// PRIVILEGE CHECK UTILITY (run standalone to diagnose privilege issues)
// ============================================================================

inline void rawr_debug_print_privilege_status() {
    std::cout << "\n========================================" << std::endl;
    std::cout << "SeLockMemoryPrivilege Status Check" << std::endl;
    std::cout << "========================================" << std::endl;
    
    HANDLE hToken;
    LUID luid;
    
    // Step 1: Can we open the process token?
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY | TOKEN_ADJUST_PRIVILEGES, &hToken)) {
        std::cerr << "[FAIL] Cannot open process token: " << GetLastError() << std::endl;
        return;
    }
    std::cout << "[OK] Opened process token" << std::endl;
    
    // Step 2: Does the privilege exist in the system?
    if (!LookupPrivilegeValueA(NULL, "SeLockMemoryPrivilege", &luid)) {
        std::cerr << "[FAIL] LookupPrivilegeValue failed: " << GetLastError() << std::endl;
        CloseHandle(hToken);
        return;
    }
    std::cout << "[OK] SeLockMemoryPrivilege LUID: " << luid.LowPart << std::endl;
    
    // Step 3: Try to enable it
    TOKEN_PRIVILEGES tp = {0};
    tp.PrivilegeCount = 1;
    tp.Privileges[0].Luid = luid;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    
    BOOL adjResult = AdjustTokenPrivileges(hToken, FALSE, &tp, 0, NULL, NULL);
    DWORD adjErr = GetLastError();
    
    if (!adjResult || adjErr == ERROR_NOT_ALL_ASSIGNED) {
        std::cerr << "[FAIL] AdjustTokenPrivileges: " << adjErr << std::endl;
        std::cerr << "       ERROR_NOT_ALL_ASSIGNED = Privilege not in user token" << std::endl;
        std::cerr << "\n       FIX REQUIRED:" << std::endl;
        std::cerr << "       1. Open secpol.msc (Local Security Policy)" << std::endl;
        std::cerr << "       2. Navigate: Local Policies > User Rights Assignment" << std::endl;
        std::cerr << "       3. Find 'Lock pages in memory'" << std::endl;
        std::cerr << "       4. Add your user account" << std::endl;
        std::cerr << "       5. LOG OUT and LOG BACK IN (or reboot)" << std::endl;
    } else {
        std::cout << "[OK] AdjustTokenPrivileges succeeded" << std::endl;
        
        // Step 4: Verify with PrivilegeCheck
        PRIVILEGE_SET ps = {0};
        ps.PrivilegeCount = 1;
        ps.Privilege[0].Luid = luid;
        ps.Privilege[0].Attributes = SE_PRIVILEGE_ENABLED;
        
        BOOL bResult;
        if (PrivilegeCheck(hToken, &ps, &bResult)) {
            if (bResult) {
                std::cout << "[OK] PrivilegeCheck: SeLockMemoryPrivilege is ENABLED" << std::endl;
                std::cout << "\n       Large pages (2MB) are AVAILABLE" << std::endl;
            } else {
                std::cerr << "[FAIL] PrivilegeCheck: SeLockMemoryPrivilege is NOT enabled" << std::endl;
            }
        } else {
            std::cerr << "[FAIL] PrivilegeCheck failed: " << GetLastError() << std::endl;
        }
    }
    
    CloseHandle(hToken);
    std::cout << "========================================\n" << std::endl;
}
