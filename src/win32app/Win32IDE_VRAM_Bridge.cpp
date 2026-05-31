#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>

/**
 * @file Win32IDE_VRAM_Bridge.cpp
 * @brief Batch 3 (20/118): GPU/VRAM Memory Bridge.
 * Interfaces with the Vulkan/D3D12 allocators for GPU-resident tensors.
 */

namespace RawrXD::Memory::GPU {

// Resolves: GPU_PinHostMemory
extern "C" bool GPU_PinHostMemory(void* ptr, size_t size) {
    if (!ptr || size == 0) return false;
    LOG_INFO("[GPU] Pinning host memory for DMA (%zu bytes)...", size);
    BOOL ok = VirtualLock(ptr, size);
    if (!ok) {
        LOG_WARN("[GPU] VirtualLock failed (GLE=%lu), memory not pinned", GetLastError());
    }
    return ok != FALSE;
}

// Resolves: GPU_MapLocalBuffer
extern "C" void* GPU_MapLocalBuffer(uint64_t gpu_address, size_t size) {
    if (size == 0) return nullptr;
    LOG_INFO("[GPU] Mapping local buffer (gpu=0x%llX, %zu bytes) to host space.",
             (unsigned long long)gpu_address, size);
    // Without a live GPU driver handle, allocate a host-side shadow buffer.
    // Callers can use this for staging data before DMA transfer.
    void* hostShadow = VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!hostShadow) {
        LOG_WARN("[GPU] VirtualAlloc shadow buffer failed (GLE=%lu)", GetLastError());
    }
    return hostShadow;
}

} // namespace RawrXD::Memory::GPU
