// src/direct_io/direct_io_ring_win.cpp
#include "direct_io_ring.h"
#include <windows.h>
#include <ioringapi.h>
#include <atomic>
#include <mutex>
#include <iostream>

struct DirectIOContext {
    HIORING hRing;
    HANDLE hFile;
    std::atomic<int> pending;
};

// Global states for MASM visibility
DirectIOContext* g_pDirectIOCtx = nullptr;
void* g_zoneBuffer = nullptr;
uint64_t g_BurstTick = 0;

extern "C" bool DirectIO_Init(DirectIOContext** ctx, const char* filepath) {
    *ctx = new DirectIOContext();
    (*ctx)->pending = 0;

    (*ctx)->hFile = CreateFileA(filepath, 
                               GENERIC_READ, 
                               FILE_SHARE_READ, 
                               nullptr, 
                               OPEN_EXISTING, 
                               FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, 
                               nullptr);

    if ((*ctx)->hFile == INVALID_HANDLE_VALUE) return false;

    IORING_CREATE_FLAGS flags = {};
    HRESULT hr = CreateIoRing(IORING_VERSION_3, flags, 1024, 1024, &(*ctx)->hRing);
    if (FAILED(hr)) {
        hr = CreateIoRing(IORING_VERSION_2, flags, 1024, 1024, &(*ctx)->hRing);
    }

    if (SUCCEEDED(hr)) {
        g_pDirectIOCtx = *ctx;
        return true;
    }
    return false;
}

extern "C" bool DirectIO_Prefetch(DirectIOContext* ctx, uint64_t offset, size_t size, void* dst) {
    if (!ctx || !ctx->hRing) return false;

    // Direct I/O check: offset and size must be sector aligned (usually 512)
    if (offset % 512 != 0 || size % 512 != 0) {
        std::cerr << "Alignment error: offset=" << offset << ", size=" << size << std::endl;
        return false;
    }

    IORING_HANDLE_REF handle_ref = IoRingHandleRefFromHandle(ctx->hFile);
    IORING_BUFFER_REF buffer_ref = IoRingBufferRefFromPointer(dst);

    HRESULT hr = BuildIoRingReadFile(ctx->hRing, 
                                    handle_ref, 
                                    buffer_ref, 
                                    (uint32_t)size, 
                                    offset, 
                                    (uint64_t)offset, // Use offset as ID for simplicity
                                    IOSQE_FLAGS_NONE);
    
    if (SUCCEEDED(hr)) {
        ctx->pending++;
        uint32_t submitted = 0;
        SubmitIoRing(ctx->hRing, 0, 0, &submitted);
        return true;
    } else {
        std::cerr << "BuildIoRingReadFile failed: 0x" << std::hex << hr << std::dec << std::endl;
    }
    return false;
}

extern "C" int DirectIO_Poll(DirectIOContext* ctx) {
    if (!ctx || !ctx->hRing) return 0;

    IORING_CQE cqe;
    int count = 0;
    while (PopIoRingCompletion(ctx->hRing, &cqe) == S_OK) {
        ctx->pending--;
        count++;
    }
    return count;
}

extern "C" int DirectIO_GetPendingCount(DirectIOContext* ctx) {
    if (!ctx) return 0;
    return ctx->pending;
}

extern "C" void DirectIO_Shutdown(DirectIOContext* ctx) {
    if (!ctx) return;
    if (ctx->hRing) CloseIoRing(ctx->hRing);
    if (ctx->hFile != INVALID_HANDLE_VALUE) CloseHandle(ctx->hFile);
    delete ctx;
    if (g_pDirectIOCtx == ctx) g_pDirectIOCtx = nullptr;
}

// Stubs for Metadata (Real implementation would parse GGUF)
extern "C" uint64_t GetTensorOffset(uint32_t tensor_id) {
    return (uint64_t)tensor_id * 4096;
}

extern "C" uint64_t GetTensorSize(uint32_t tensor_id) {
    return 4096;
}

extern "C" void* ResolveZonePointer(uint32_t zone_index) {
    if (!g_zoneBuffer) return nullptr;
    return (uint8_t*)g_zoneBuffer + (zone_index * 128 * 1024 * 1024); // 128MB zones
}

static std::vector<uint32_t> s_BurstPlan = {0, 1, 2, 3}; 

extern "C" uint32_t GetBurstCount() {
    return (uint32_t)s_BurstPlan.size();
}

extern "C" uint32_t* GetBurstPlan() {
    return s_BurstPlan.data();
}

extern "C" void VulkanDMA_RegisterTensor(uint32_t tensor_id, void* ptr, size_t size) {
    // Placeholder for Vulkan Bridge
    (void)tensor_id; (void)ptr; (void)size;
}
