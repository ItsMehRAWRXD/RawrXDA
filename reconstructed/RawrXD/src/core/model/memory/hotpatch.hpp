// model_memory_hotpatch.hpp — Memory-Layer Hotpatching (Layer 1)
// Direct RAM patching using OS protection APIs.
// Operates on loaded model tensors in CPU/GPU mapped memory.
//
// Platform APIs: Windows VirtualProtect / POSIX mprotect
// Rule: No exceptions. No STL allocators inside patch code.
//       All pointer math uses uintptr_t.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>

// ---------------------------------------------------------------------------
// PatchResult — Structured result type (no exceptions)
// ---------------------------------------------------------------------------
#ifndef RAWRXD_PATCHRESULT_DEFINED
#define RAWRXD_PATCHRESULT_DEFINED
struct PatchResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static PatchResult ok(const char* msg = "Success") {
        PatchResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        return r;
    }

    static PatchResult error(const char* msg, int code = -1) {
        PatchResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        return r;
    }
};
#endif // RAWRXD_PATCHRESULT_DEFINED

// ---------------------------------------------------------------------------
// MemoryPatchEntry — Describes one in-memory patch operation
// ---------------------------------------------------------------------------
struct MemoryPatchEntry {
    uintptr_t       targetAddr;         // Absolute virtual address to patch
    size_t          patchSize;          // Number of bytes to write
    const void*     patchData;          // Pointer to replacement bytes
    uint8_t         originalBytes[64];  // Backup for rollback (max 64 bytes)
    size_t          originalSize;       // Actual size backed up
    bool            applied;            // Whether this patch is currently active
};

// ---------------------------------------------------------------------------
// MemoryPatchStats — Runtime statistics
// ---------------------------------------------------------------------------
struct MemoryPatchStats {
    std::atomic<uint64_t> totalApplied{0};
    std::atomic<uint64_t> totalReverted{0};
    std::atomic<uint64_t> totalFailed{0};
    std::atomic<uint64_t> protectionChanges{0};
};

// ---------------------------------------------------------------------------
// Enhanced External ASM entry points (memory_patch.asm)
// ---------------------------------------------------------------------------
extern "C" {
    // Original compatibility functions
    int asm_apply_memory_patch(void* addr, size_t size, const void* data);
    int asm_revert_memory_patch(void* addr, size_t size, const void* backup);
    size_t asm_safe_memread(void* dest, const void* src, size_t len);
    
    // Enhanced autonomous functions with SIMD acceleration
    int asm_apply_memory_patch_enhanced(void* addr, size_t size, const void* data, uint32_t flags);
    int asm_revert_memory_patch_autonomous(void* addr, size_t size, const void* backup, uint32_t flags);
    size_t asm_safe_memread_simd(void* dest, const void* src, size_t len, uint32_t simd_level);
    
    // Autonomous detection and optimization
    int asm_detect_memory_conflicts(void* addr, size_t size);
    int asm_validate_patch_integrity(void* addr, size_t size, uint32_t expected_checksum);
    int asm_optimize_cache_locality(void* addr, size_t size);
    uint64_t asm_monitor_patch_performance(void* addr, size_t size, uint32_t* metrics);
    
    // Autonomous healing and bulk operations
    int asm_autonomous_memory_heal(void* addr, size_t size, uint32_t healing_flags);
    int asm_simd_bulk_patch_apply(void** addrs, size_t* sizes, const void** patches, size_t count);
    int asm_hardware_transactional_patch(void* addr, size_t size, const void* data, uint32_t rtm_flags);
}

// ---------------------------------------------------------------------------
// Enhanced Autonomous Operation Flags
// ---------------------------------------------------------------------------
#define AUTO_DETECT_CONFLICTS   0x00000001
#define AUTO_RESOLVE_CONFLICTS  0x00000002
#define AUTO_OPTIMIZE_CACHE     0x00000004
#define AUTO_MONITOR_PERF       0x00000008
#define AUTO_HEAL_MEMORY        0x00000010
#define AUTO_USE_SIMD           0x00000020
#define AUTO_USE_TSX_RTM        0x00000040
#define AUTO_VALIDATE_INTEGRITY 0x00000080

// SIMD acceleration levels
#define SIMD_LEVEL_NONE         0
#define SIMD_LEVEL_SSE2         1
#define SIMD_LEVEL_AVX2         2
#define SIMD_LEVEL_AVX512       3

// ---------------------------------------------------------------------------
// Enhanced Core API functions
// ---------------------------------------------------------------------------

// Original compatibility functions
PatchResult apply_memory_patch(void* addr, size_t size, const void* data);
PatchResult revert_memory_patch(MemoryPatchEntry* entry);
PatchResult apply_memory_patch_tracked(MemoryPatchEntry* entry);

// Enhanced autonomous functions
PatchResult apply_memory_patch_enhanced(void* addr, size_t size, const void* data, 
                                      uint32_t flags = AUTO_DETECT_CONFLICTS | AUTO_OPTIMIZE_CACHE);
PatchResult apply_memory_patch_simd(void* addr, size_t size, const void* data, 
                                  uint32_t simd_level = SIMD_LEVEL_AVX512);
PatchResult apply_memory_patch_transactional(void* addr, size_t size, const void* data,
                                            uint32_t rtm_flags = 0);

// Bulk operations for multiple patches
PatchResult apply_memory_patches_bulk(MemoryPatchEntry* entries, size_t count,
                                    uint32_t flags = AUTO_DETECT_CONFLICTS);
PatchResult revert_memory_patches_bulk(MemoryPatchEntry* entries, size_t count);

// Autonomous detection and validation
PatchResult detect_memory_conflicts(void* addr, size_t size);
PatchResult validate_patch_integrity(void* addr, size_t size, uint32_t expected_checksum);
PatchResult autonomous_memory_heal(void* base_addr, size_t region_size);

// Performance monitoring and optimization
struct PatchPerformanceMetrics {
    uint64_t cycles_elapsed;
    uint64_t cache_misses;
    uint32_t simd_level_used;
    uint32_t conflicts_detected;
    uint32_t healing_operations;
};

PatchResult get_patch_performance_metrics(void* addr, size_t size, PatchPerformanceMetrics* metrics);

// Query current memory protection flags for a region.
PatchResult query_memory_protection(void* addr, size_t size, DWORD* outProtect);

// Get global memory patch statistics.
const MemoryPatchStats& get_memory_patch_stats();

// Reset statistics counters.
void reset_memory_patch_stats();
