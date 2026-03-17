#pragma once
#include <windows.h>
#include <cstdint>

// Forward declaration of ggml_tensor struct if needed
struct ggml_tensor {
    void* data;
};

class SwarmLink_RingBuffer {
private:
    HANDLE hFile;
    HANDLE hIOCP;
    void* buffer;
    uint32_t chunkSize;
    
public:
    SwarmLink_RingBuffer(const wchar_t* filePath, uint32_t size);
    ~SwarmLink_RingBuffer();

    bool LoadChunkAsync(uint64_t offset, OVERLAPPED* overlapped);
    void WaitAndProcessIOCP();
};

extern "C" {
    // Hot-fixes the tensor data pointer to point to the active L2/mapped NVMe region
    void SwarmLink_UpdateTensor(ggml_tensor* tensor, void* new_data_ptr);

    // ASM Fast Path for page-aligned memory copies (SIMD Optimized)
    void SwarmLink_FastCopySIMD(void* dest, const void* src, size_t numBytes);
}
