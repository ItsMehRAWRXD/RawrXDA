#include "Win32IDE.h"
#include "IDELogger.h"
#include <windows.h>

/**
 * @file Win32IDE_TensorBuffers.cpp
 * @brief Batch 3 (18/118): Tensor Buffer Lifecycle Management.
 * Manages the allocation and alignment of buffers for MASM kernels.
 */

namespace RawrXD::Memory {

// Resolves: Tensor_AllocateAligned
extern "C" void* Tensor_AllocateAligned(size_t size, size_t alignment) {
    // 3.43M TPS requires strict SIMD alignment (usually 64-byte for AVX-512)
    void* ptr = _aligned_malloc(size, alignment);
    if (!ptr) {
        LOG_ERROR("[Tensor] Aligned allocation failed.");
    }
    return ptr;
}

// Resolves: Tensor_Free
extern "C" void Tensor_Free(void* ptr) {
    if (ptr) {
        _aligned_free(ptr);
    }
}

} // namespace RawrXD::Memory
