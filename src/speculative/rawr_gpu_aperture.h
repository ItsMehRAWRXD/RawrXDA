/*
====================================================================
 RAWR DDR5-to-GPU DIRECT APERTURE BYPASS
 Zero-copy system RAM mapping into GPU address space (AMD ROCm/HIP)
====================================================================

 Uses hipHostRegister(Mapped | ReadOnly) to pin DDR5 pages into the
 GPU's GART aperture. The GPU Command Processor fetches weights
 directly over PCIe 4.0 x16 (~31.5 GB/s) without CPU staging.

 Compile with hipcc:
   hipcc -O2 -o test_gpu_aperture.exe test_gpu_aperture.cpp rawr_gpu_aperture.cpp
====================================================================
*/

#ifndef RAWR_GPU_APERTURE_H
#define RAWR_GPU_APERTURE_H

#include <cstddef>
#include <cstdint>

namespace rawr {

// Aperture handle: opaque GPU-side virtual address for a DDR5 buffer
struct ApertureHandle {
    void* cpu_ptr   = nullptr;   // Original DDR5 pointer
    void* gpu_ptr   = nullptr;   // GPU virtual address (GART-mapped)
    size_t size     = 0;
    bool   pinned   = false;
    bool   mapped   = false;
};

// Pin DDR5 buffer and map into GPU address space.
// Returns false if HIP is unavailable or mapping fails.
bool aperture_map(ApertureHandle& out, void* cpu_ptr, size_t size);

// Unpin and unmap. Safe to call even if map failed.
void aperture_unmap(ApertureHandle& h);

// Prefetch a sub-range into GPU L2 (async, non-blocking)
void aperture_prefetch(const ApertureHandle& h, size_t offset, size_t len);

// Query GPU pointer from an already-mapped handle
inline void* aperture_gpu_ptr(const ApertureHandle& h) { return h.gpu_ptr; }

// Check if HIP runtime is present and a GPU is visible
bool aperture_gpu_available();

} // namespace rawr

#endif // RAWR_GPU_APERTURE_H
