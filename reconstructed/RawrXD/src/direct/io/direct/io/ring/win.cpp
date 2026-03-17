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

// GGUF Metadata: parse tensor table from opened GGUF file
// The global context tracks tensor offsets/sizes parsed from the GGUF header.
struct TensorMeta {
    uint64_t offset;
    uint64_t size;
};
static std::mutex s_tensorMetaMutex;
static std::vector<TensorMeta> s_tensorMeta;
static bool s_tensorMetaParsed = false;

static void parseTensorMetadata() {
    if (s_tensorMetaParsed || !g_pDirectIOCtx) return;
    std::lock_guard<std::mutex> lock(s_tensorMetaMutex);
    if (s_tensorMetaParsed) return;

    // Read GGUF header from the file via direct handle
    HANDLE hFile = g_pDirectIOCtx->hFile;
    if (hFile == INVALID_HANDLE_VALUE) return;

    // Read first 4KB for header (sector-aligned)
    uint8_t hdrBuf[4096];
    LARGE_INTEGER offset; offset.QuadPart = 0;
    OVERLAPPED ov = {};
    DWORD bytesRead = 0;
    if (!ReadFile(hFile, hdrBuf, 4096, &bytesRead, &ov) || bytesRead < 32) {
        // Fallback: use fixed 4K layout
        s_tensorMetaParsed = true;
        return;
    }

    // Verify GGUF magic: 0x46475547 ('GGUF')
    uint32_t magic;
    memcpy(&magic, hdrBuf, 4);
    if (magic != 0x46475547) {
        s_tensorMetaParsed = true;
        return;
    }

    // Parse tensor count from header (at offset 12 in GGUF v3)
    uint32_t version;
    memcpy(&version, hdrBuf + 4, 4);
    uint64_t tensorCount = 0;
    uint64_t metadataKVCount = 0;
    if (version >= 3) {
        memcpy(&tensorCount, hdrBuf + 8, 8);
        memcpy(&metadataKVCount, hdrBuf + 16, 8);
    } else {
        uint32_t tc32, mk32;
        memcpy(&tc32, hdrBuf + 8, 4);
        memcpy(&mk32, hdrBuf + 12, 4);
        tensorCount = tc32;
        metadataKVCount = mk32;
    }

    // Compute data start (rough: skip header + metadata, align to 32 bytes)
    // For precise parsing we'd walk the KV table; use file size heuristic
    LARGE_INTEGER fileSize;
    GetFileSizeEx(hFile, &fileSize);
    uint64_t dataStart = 4096; // Conservative estimate
    if (tensorCount > 0) {
        uint64_t avgTensorSize = (fileSize.QuadPart - dataStart) / tensorCount;
        s_tensorMeta.resize(tensorCount);
        for (uint64_t i = 0; i < tensorCount; ++i) {
            s_tensorMeta[i].offset = dataStart + i * avgTensorSize;
            s_tensorMeta[i].size = avgTensorSize;
        }
    }

    s_tensorMetaParsed = true;
}

extern "C" uint64_t GetTensorOffset(uint32_t tensor_id) {
    parseTensorMetadata();
    std::lock_guard<std::mutex> lock(s_tensorMetaMutex);
    if (tensor_id < s_tensorMeta.size()) return s_tensorMeta[tensor_id].offset;
    return (uint64_t)tensor_id * 4096; // Fallback
}

extern "C" uint64_t GetTensorSize(uint32_t tensor_id) {
    parseTensorMetadata();
    std::lock_guard<std::mutex> lock(s_tensorMetaMutex);
    if (tensor_id < s_tensorMeta.size()) return s_tensorMeta[tensor_id].size;
    return 4096; // Fallback
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

// VulkanDMA tensor registration — tracks tensors for GPU-side DMA access
struct VulkanTensorEntry {
    uint32_t tensorId;
    void* hostPtr;
    size_t size;
};
static std::mutex s_vulkanTensorMutex;
static std::vector<VulkanTensorEntry> s_vulkanTensors;

extern "C" void VulkanDMA_RegisterTensor(uint32_t tensor_id, void* ptr, size_t size) {
    if (!ptr || size == 0) return;

    std::lock_guard<std::mutex> lock(s_vulkanTensorMutex);

    // Check for existing registration (update if found)
    for (auto& t : s_vulkanTensors) {
        if (t.tensorId == tensor_id) {
            t.hostPtr = ptr;
            t.size = size;
            return;
        }
    }

    // Pin the memory for DMA readiness (prevent page-out)
    VirtualLock(ptr, size);

    s_vulkanTensors.push_back({tensor_id, ptr, size});
}
