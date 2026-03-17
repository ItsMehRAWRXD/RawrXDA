#include <windows.h>
#include <vector>
#include <map>
#include <mutex>
#include <algorithm>

// ============================================================================
// RAWRXD MEMORY MODULE TEMPLATE
// This DLL implements optimized memory management for a specific context size.
// ============================================================================

// Export definitions
#define EXPORT __declspec(dllexport)

// Global memory tracker
struct GlobalState {
    std::mutex mutex;
    std::map<void*, size_t> allocations;
    size_t totalBytes;
};

GlobalState* g_state = nullptr;

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        g_state = new GlobalState();
        g_state->totalBytes = 0;
        break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
        break;
    case DLL_PROCESS_DETACH:
        if (g_state) {
            delete g_state;
            g_state = nullptr;
        }
        break;
    }
    return TRUE;
}

extern "C" {

    // Allocate a buffer for the context window
    // This example uses simple VirtualAlloc but could use:
    // - Huge Pages (CreateFileMapping with SEC_LARGE_PAGES)
    // - NUMA-aware allocation (VirtualAllocExNuma)
    // - GPU Direct Memory (if applicable)
    EXPORT void* AllocateContextBuffer(size_t size) {
        if (!g_state) return nullptr;

        // Try to allocate with large pages if possible (implementation specific)
        void* ptr = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        
        if (ptr) {
            std::lock_guard<std::mutex> lock(g_state->mutex);
            g_state->allocations[ptr] = size;
            g_state->totalBytes += size;
        }

        return ptr;
    }

    // Free the allocated buffer
    EXPORT void FreeContextBuffer(void* buffer) {
        if (!g_state || !buffer) return;

        std::lock_guard<std::mutex> lock(g_state->mutex);
        auto it = g_state->allocations.find(buffer);
        if (it != g_state->allocations.end()) {
            g_state->totalBytes -= it->second;
            g_state->allocations.erase(it);
            VirtualFree(buffer, 0, MEM_RELEASE);
        }
    }

    // Optimize the buffer (e.g., locking pages in RAM, defragging)
    EXPORT bool OptimizeContextBuffer(void* buffer, size_t size) {
        if (!buffer || size == 0) return false;

        // Example optimization: Lock pages in physical memory to prevent paging to disk
        // This requires SeLockMemoryPrivilege
        if (VirtualLock(buffer, size)) {
            return true;
        }

        return false;
    }

    // Get statistics
    EXPORT size_t GetTotalAllocatedBytes() {
        if (!g_state) return 0;
        std::lock_guard<std::mutex> lock(g_state->mutex);
        return g_state->totalBytes;
    }
}
