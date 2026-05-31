// ============================================================================
// native_speed_layer.hpp — Native Speed Layer: Zero-Overhead Inference Engine
// ============================================================================
#pragma once

#include "model_memory_hotpatch.hpp"
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <chrono>

namespace RawrXD {
namespace NativeSpeed {

struct CPUFeatures {
    bool hasSSE42      = false;
    bool hasAVX        = false;
    bool hasAVX2       = false;
    bool hasFMA3       = false;
    bool hasAVX512F    = false;
    bool hasAVX512BW   = false;
    bool hasAVX512VL   = false;
    bool hasAVX512VNNI = false;
    bool hasF16C       = false;
    bool hasBMI2       = false;
    uint32_t cacheLineSize  = 64;
    uint32_t l1CacheKB      = 32;
    uint32_t l2CacheKB      = 256;
    uint32_t l3CacheMB      = 8;
    uint32_t coreCount      = 1;
    uint32_t threadCount    = 1;

    uint32_t SimdWidthFloats() const {
        if (hasAVX512F) return 16;
        if (hasAVX2)    return 8;
        if (hasSSE42)   return 4;
        return 1;
    }
    uint32_t SimdWidthBytes() const { return SimdWidthFloats() * 4; }
};

CPUFeatures DetectCPUFeatures();

extern "C" {
    void KV_ApertureMap(void* pBase, size_t nSize);
    void KV_PageFlush(void* pData, size_t nBytes);
}

enum class QuantType : uint32_t {
    F32 = 0, F16 = 1, Q4_0 = 2, Q4_1 = 3, Q5_0 = 6, Q5_1 = 7, Q8_0 = 8, Q8_1 = 9
};

struct TensorView {
    void*       data;
    uint64_t    offset;
    uint32_t    dims[4];
    uint32_t    ndims;
    uint32_t    typeId;
    uint64_t    sizeBytes;
    const char* name;
    uint32_t    nameLen;

    uint64_t ElementCount() const {
        uint64_t n = 1;
        for (uint32_t i = 0; i < ndims; ++i) n *= dims[i];
        return n;
    }
};

struct KernelDispatchTable {
    void (*sgemm)(const float* A, const float* B, float* C, int M, int N, int K);
    void (*sgemv)(const float* A, const float* x, float* y, int M, int K);
    void (*rmsnorm)(const float* x, const float* weight, float* y, int dim, float eps);
    void (*softmax)(float* x, int n);
    void (*silu)(float* x, int n);
    KernelDispatchTable() { memset(this, 0, sizeof(*this)); }
};

class NativeSpeedLayer {
public:
    static NativeSpeedLayer& Instance() { static NativeSpeedLayer instance; return instance; }
    PatchResult Init();
    const CPUFeatures& GetCPUFeatures() const { return m_cpu; }
private:
    CPUFeatures m_cpu;
    KernelDispatchTable m_dispatch;
};

} // namespace NativeSpeed
} // namespace RawrXD
