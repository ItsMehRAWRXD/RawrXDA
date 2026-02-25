// model_memory_hotpatch.hpp — Memory-Layer Hotpatching (Layer 1)
// Direct RAM patching using OS protection APIs.
// Operates on loaded model tensors in CPU/GPU mapped memory.
//
// Platform APIs: Windows VirtualProtect / POSIX mprotect
// Rule: No exceptions. No STL allocators inside patch code.
//       All pointer math uses uintptr_t.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#pragma once

#define WIN32_LEAN_AND_MEAN
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
// External ASM entry points (memory_patch.asm)
// ---------------------------------------------------------------------------
extern "C" {
    // Apply a raw memory patch with VirtualProtect wrapper
    int asm_apply_memory_patch(void* addr, size_t size, const void* data);
    // Revert a memory patch from backup
    int asm_revert_memory_patch(void* addr, size_t size, const void* backup);
    // Read memory safely (returns bytes read, 0 on fault)
    size_t asm_safe_memread(void* dest, const void* src, size_t len);
}

// ---------------------------------------------------------------------------
// Core API functions
// ---------------------------------------------------------------------------

// Apply a single memory patch at the given address.
// Automatically handles VirtualProtect to make memory writable,
// backs up original bytes, writes new data, restores protection.
PatchResult apply_memory_patch(void* addr, size_t size, const void* data);

// Revert a previously applied memory patch using backed-up bytes.
PatchResult revert_memory_patch(MemoryPatchEntry* entry);

// Apply a memory patch and record it in the entry for later rollback.
PatchResult apply_memory_patch_tracked(MemoryPatchEntry* entry);

// Query current memory protection flags for a region.
PatchResult query_memory_protection(void* addr, size_t size, DWORD* outProtect);

// Get global memory patch statistics.
const MemoryPatchStats& get_memory_patch_stats();

// Reset statistics counters.
void reset_memory_patch_stats();
