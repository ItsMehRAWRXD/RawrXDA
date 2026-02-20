// model_memory_hotpatch.cpp — Memory-Layer Hotpatching (Layer 1) Implementation
// Direct RAM patching using OS protection APIs.
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
#include "model_memory_hotpatch.hpp"
#include <cstring>
#include <iostream>

// ---------------------------------------------------------------------------
// Global state
// ---------------------------------------------------------------------------
static std::mutex             g_memPatchMutex;
static MemoryPatchStats       g_memPatchStats;

// ---------------------------------------------------------------------------
// apply_memory_patch
// ---------------------------------------------------------------------------
PatchResult apply_memory_patch(void* addr, size_t size, const void* data) {
    if (!addr || !data || size == 0) {
        return PatchResult::error("Null address, data, or zero size", 1);
    }

    std::lock_guard<std::mutex> lock(g_memPatchMutex);

    // Attempt to make memory writable
    DWORD oldProtect = 0;
    if (!VirtualProtect(addr, size, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_memPatchStats.totalFailed.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (make writable) failed", static_cast<int>(GetLastError()));
    }
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    // Write patch data
    std::memcpy(addr, data, size);

    // Restore original protection
    DWORD dummy = 0;
    VirtualProtect(addr, size, oldProtect, &dummy);
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    // Flush instruction cache (critical if patching code)
    FlushInstructionCache(GetCurrentProcess(), addr, size);

    g_memPatchStats.totalApplied.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Memory patch applied");
}

// ---------------------------------------------------------------------------
// revert_memory_patch
// ---------------------------------------------------------------------------
PatchResult revert_memory_patch(MemoryPatchEntry* entry) {
    if (!entry) {
        return PatchResult::error("Null entry", 1);
    }
    if (!entry->applied) {
        return PatchResult::error("Patch not currently applied", 2);
    }
    if (entry->originalSize == 0) {
        return PatchResult::error("No backup data to restore", 3);
    }

    std::lock_guard<std::mutex> lock(g_memPatchMutex);

    void* addr = reinterpret_cast<void*>(entry->targetAddr);
    DWORD oldProtect = 0;
    if (!VirtualProtect(addr, entry->originalSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_memPatchStats.totalFailed.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (revert) failed", static_cast<int>(GetLastError()));
    }
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    std::memcpy(addr, entry->originalBytes, entry->originalSize);

    DWORD dummy = 0;
    VirtualProtect(addr, entry->originalSize, oldProtect, &dummy);
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    FlushInstructionCache(GetCurrentProcess(), addr, entry->originalSize);

    entry->applied = false;
    g_memPatchStats.totalReverted.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Memory patch reverted");
}

// ---------------------------------------------------------------------------
// apply_memory_patch_tracked
// ---------------------------------------------------------------------------
PatchResult apply_memory_patch_tracked(MemoryPatchEntry* entry) {
    if (!entry) {
        return PatchResult::error("Null entry", 1);
    }
    if (entry->patchSize == 0 || !entry->patchData) {
        return PatchResult::error("Invalid patch entry (zero size or null data)", 2);
    }
    if (entry->patchSize > sizeof(entry->originalBytes)) {
        return PatchResult::error("Patch size exceeds backup buffer (64 bytes max)", 3);
    }

    std::lock_guard<std::mutex> lock(g_memPatchMutex);

    void* addr = reinterpret_cast<void*>(entry->targetAddr);

    // Backup original bytes
    DWORD oldProtect = 0;
    if (!VirtualProtect(addr, entry->patchSize, PAGE_EXECUTE_READWRITE, &oldProtect)) {
        g_memPatchStats.totalFailed.fetch_add(1, std::memory_order_relaxed);
        return PatchResult::error("VirtualProtect (backup read) failed", static_cast<int>(GetLastError()));
    }
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    std::memcpy(entry->originalBytes, addr, entry->patchSize);
    entry->originalSize = entry->patchSize;

    // Write new data
    std::memcpy(addr, entry->patchData, entry->patchSize);

    // Restore protection
    DWORD dummy = 0;
    VirtualProtect(addr, entry->patchSize, oldProtect, &dummy);
    g_memPatchStats.protectionChanges.fetch_add(1, std::memory_order_relaxed);

    FlushInstructionCache(GetCurrentProcess(), addr, entry->patchSize);

    entry->applied = true;
    g_memPatchStats.totalApplied.fetch_add(1, std::memory_order_relaxed);
    return PatchResult::ok("Tracked memory patch applied");
}

// ---------------------------------------------------------------------------
// query_memory_protection
// ---------------------------------------------------------------------------
PatchResult query_memory_protection(void* addr, size_t size, DWORD* outProtect) {
    if (!addr || !outProtect) {
        return PatchResult::error("Null address or output pointer", 1);
    }

    MEMORY_BASIC_INFORMATION mbi;
    SIZE_T result = VirtualQuery(addr, &mbi, sizeof(mbi));
    if (result == 0) {
        return PatchResult::error("VirtualQuery failed", static_cast<int>(GetLastError()));
    }

    *outProtect = mbi.Protect;
    return PatchResult::ok("Protection queried");
}

// ---------------------------------------------------------------------------
// get_memory_patch_stats / reset_memory_patch_stats
// ---------------------------------------------------------------------------
const MemoryPatchStats& get_memory_patch_stats() {
    return g_memPatchStats;
}

void reset_memory_patch_stats() {
    g_memPatchStats.totalApplied.store(0, std::memory_order_relaxed);
    g_memPatchStats.totalReverted.store(0, std::memory_order_relaxed);
    g_memPatchStats.totalFailed.store(0, std::memory_order_relaxed);
    g_memPatchStats.protectionChanges.store(0, std::memory_order_relaxed);
}
