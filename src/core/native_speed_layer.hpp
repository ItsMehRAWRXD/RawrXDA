// ============================================================================
// native_speed_layer.hpp — Native Speed Layer: Zero-Overhead Inference Engine
// ============================================================================
// A zero-dependency, SIMD-accelerated computation layer that provides:
//   - Direct memory-mapped GGUF tensor access (no copy, no parse overhead)
//   - AVX2/FMA3 + AVX-512 dispatched GEMM/GEMV micro-kernels
//   - Fused MLP kernels (Linear → GeLU → Linear) to minimize round-trips
//   - Non-temporal streaming stores for large matrix output
//   - KV cache with sliding-window compression
//   - CPU feature detection at init, runtime dispatch per-kernel
//   - Ring-buffer telemetry for latency tracking
//
// Integration:
//   - Feeds into LocalAICore for full inference pipeline
//   - Consumed by InferenceStateMachine for state transitions
//   - FlashAttentionEngine for attention computation
//   - GPU backend bridge for hybrid CPU/GPU dispatch
//
// Pattern: PatchResult-style, no exceptions, factory results.
// Rule:    NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================

#pragma once

#include "model_memory_hotpatch.hpp"  // PatchResult
#include <cstdint>
#include <cstddef>
#include <mutex>
#include <atomic>
#include <chrono>

namespace RawrXD {
namespace NativeSpeed {

// ============================================================================
// CPU Feature Flags — detected at runtime via CPUID
// ============================================================================
struct CPUFeatures {
    bool hasSSE42      = false;
    bool hasAVX        = false;
    bool hasAVX2       = false;
    bool hasFMA3       = false;
    bool hasAVX512F    = false;
    bool hasAVX512BW   = false;
    bool hasAVX512VL   = false;
    bool hasAVX512VNNI = false;  // INT8 dot-prod acceleration
    bool hasF16C       = false;  // FP16 ↔ FP32 conversion
    bool hasBMI2       = false;  // Bit manipulation (PDEP/PEXT)
    uint32_t cacheLineSize  = 64;
    uint32_t l1CacheKB      = 32;
    uint32_t l2CacheKB      = 256;
    uint32_t l3CacheMB      = 8;
    uint32_t coreCount      = 1;
    uint32_t threadCount    = 1;

    /// Best SIMD width in floats (8 for AVX2, 16 for AVX-512)
    uint32_t SimdWidthFloats() const {
        if (hasAVX512F) return 16;
        if (hasAVX2)    return 8;
        if (hasSSE42)   return 4;
        return 1;
    }

    /// Best SIMD width in bytes
    uint32_t SimdWidthBytes() const { return SimdWidthFloats() * 4; }
};

/// Detect CPU features via CPUID (called once at init)
CPUFeatures DetectCPUFeatures();

// ============================================================================
// ASM Forward Declarations (KV & MoE Aperture)
// ============================================================================
extern "C" {
    void KV_ApertureMap(void* pBase, size_t nSize);
    void KV_PageFlush(void* pData, size_t nBytes);
    
    void* SparseGather_Initialize(void* config, void* telemetry_handle);
    int SparseGather_Execute(void* context, float* input, float* router_logits, float* output, uint32_t layer_index);
    void SparseGather_FlushCache(void* context);
    void SparseGather_GetStats(void* context, uint64_t* loaded, uint64_t* skipped);
    
    // Sampler (AVX-512)
    void Sampler_SoftMax_TopK_Fused(float* logits, uint32_t* indices, int n, int K);
}

// ============================================================================
// MmapRegion — Windows file mapping handles
// ============================================================================
struct MmapRegion {
    void*   base        = nullptr;
    size_t  size        = 0;
    void*   fileHandle  = (void*)-1; // INVALID_HANDLE_VALUE
    void*   mappingHandle = nullptr;
    bool    valid       = false;
};

/// Memory-map a file
PatchResult MmapFile(const char* path, MmapRegion* out, bool writable);
/// Release a memory-mapped region
PatchResult MmapRelease(MmapRegion* region);

// ============================================================================
// Quantization & Dequantization
// ============================================================================
enum class QuantType : uint32_t {
    F32     = 0,
    F16     = 1,
    Q4_0    = 2,
    Q4_1    = 3,
    Q5_0    = 6,
    Q5_1    = 7,
    Q8_0    = 8,
    Q8_1    = 9,
    Q2_K    = 10,
    Q3_K    = 11,
    Q4_K    = 12,
    Q5_K    = 13,
    Q6_K    = 14,
    IQ2_XXS = 16,
    IQ2_XS  = 17,
};

/// Bytes per block for a given quant type (returns 0 if unknown)
uint32_t QuantBlockSize(QuantType qt);
/// Elements per block for a given quant type
uint32_t QuantBlockElements(QuantType qt);

/// Type for dequantization functions: (src, dst, numBlocks)
typedef void (*DequantFn)(const void*, float*, uint64_t);

/// Get the best dequantization kernel for the current CPU
DequantFn GetDequantKernel(QuantType qt, const CPUFeatures& cpu);

// ============================================================================
// TensorView — Zero-copy view into mmap'd GGUF tensor data
// ============================================================================
struct TensorView {
    void*       data;           // Raw pointer into mmap region
    uint64_t    offset;         // Byte offset from file start
    uint32_t    dims[4];        // Shape: [d0, d1, d2, d3], unused dims = 1
    uint32_t    ndims;          // Number of active dimensions (1-4)
    uint32_t    typeId;         // GGUF type enum (F32=0, F16=1, Q4_0=2, etc.)
    uint64_t    sizeBytes;      // Total byte size of this tensor
    const char* name;           // Tensor name string (points into mmap)
    uint32_t    nameLen;        // Tensor name length

    /// Total element count
    uint64_t ElementCount() const {
        uint64_t n = 1;
        for (uint32_t i = 0; i < ndims; ++i) n *= dims[i];
        return n;
    }

    /// Is this a quantized type?
    bool IsQuantized() const { return typeId >= 2; }

    /// fp32 pointer (only valid if typeId == 0)
    float* AsFloat32() const { return static_cast<float*>(data); }

    /// fp16 pointer (only valid if typeId == 1)
    uint16_t* AsFloat16() const { return static_cast<uint16_t*>(data); }
};

// ============================================================================
// KernelDispatchTable — pointers to detected SIMD implementation functions
// ============================================================================
struct KernelDispatchTable {
    // GEMM: C[M×N] += A[M×K] * B[K×N]
    void (*sgemm)(const float* A, const float* B, float* C,
                  int M, int N, int K);

    // SGEMV: y[M] += A[M×K] * x[K]
    void (*sgemv)(const float* A, const float* x, float* y,
                  int M, int K);

    // FP16 GEMM: C[M×N] += A_f16[M×K] * B_f16[K×N] (output fp32)
    void (*hgemm)(const uint16_t* A, const uint16_t* B, float* C,
                  int M, int N, int K);

    // Fused MLP: out = GeLU(x @ W1 + b1) @ W2 + b2
    void (*fused_mlp)(const float* x, const float* W1, const float* b1,
                      const float* W2, const float* b2, float* out,
                      int seqLen, int hiddenDim, int ffnDim);

    // RMSNorm: y = x * rsqrt(mean(x^2) + eps) * weight
    void (*rmsnorm)(const float* x, const float* weight, float* y,
                    int dim, float eps);

    // SoftMax: y = softmax(x) in-place, length n
    void (*softmax)(float* x, int n);

    // Sampler: apply temperature scaling in-place
    void (*sampler_temp)(float* x, int n, float invTemp);

    // Sampler: find maximum value
    float (*sampler_max)(const float* x, int n);

    // Sampler: compute exponential sum for SoftMax (sum(exp(x - maxVal)))
    float (*sampler_expsum)(const float* x, int n, float maxVal);

    // Sampler: fused Top-K
    void (*sampler_topk)(float* x, uint32_t* indices, int n, int K);

    // SiLU: x = silu(x) in-place, length n
    void (*silu)(float* x, int n);

    // RoPE: apply rotary positional embedding in-place
    void (*rope)(float* q, float* k, int headDim, int nHeads,
                 int nKVHeads, int pos, float theta);

    // Vector dot product: result = dot(a, b) of length n
    float (*vdot)(const float* a, const float* b, int n);

    // Quantized matmul: C += dequant(A_quant) * B_f32
    void (*qgemv)(const void* A_quant, QuantType aType,
                  const float* x, float* y, int M, int K);

    // Memory copy with non-temporal stores (for large results)
    void (*nt_memcpy)(void* dst, const void* src, size_t bytes);

    KernelDispatchTable() { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// LatencyTracker — Ring-buffer for per-operation timing
// ============================================================================
struct LatencySample {
    uint64_t    timestampNs;    // ns since epoch
    uint32_t    opType;         // Operation enum
    uint32_t    layerIndex;     // Which transformer layer
    float       durationUs;     // Microseconds for this op
};

static constexpr uint32_t LATENCY_RING_SIZE = 4096;

struct LatencyRing {
    LatencySample       samples[LATENCY_RING_SIZE];
    std::atomic<uint32_t> writePos{0};

    void Record(uint32_t opType, uint32_t layer, float durationUs);

    /// Average latency over last N samples for given opType
    float AverageUs(uint32_t opType, uint32_t lastN = 64) const;

    /// P99 latency over last N samples
    float P99Us(uint32_t opType, uint32_t lastN = 256) const;
};

// ============================================================================
// NativeSpeedLayer — Main orchestrator
// ============================================================================
class NativeSpeedLayer {
public:
    NativeSpeedLayer();
    ~NativeSpeedLayer();

    NativeSpeedLayer(const NativeSpeedLayer&) = delete;
    NativeSpeedLayer& operator=(const NativeSpeedLayer&) = delete;

    static NativeSpeedLayer& Instance() {
        static NativeSpeedLayer instance;
        return instance;
    }

    PatchResult Init();
    PatchResult Shutdown();
    bool IsReady() const { return m_ready.load(std::memory_order_acquire); }

    PatchResult MapGGUF(const char* path);
    PatchResult UnmapGGUF();
    uint32_t TensorCount() const { return m_tensorCount; }
    const TensorView* GetTensor(uint32_t index) const;
    const TensorView* FindTensor(const char* name) const;

    void SGEMM(const float* A, const float* B, float* C, int M, int N, int K);
    void SGEMV(const float* A, const float* x, float* y, int M, int K);
    void HGEMM(const uint16_t* A, const uint16_t* B, float* C, int M, int N, int K);
    void FusedMLP(const float* x, const float* W1, const float* b1,
                  const float* W2, const float* b2, float* out,
                  int seqLen, int hiddenDim, int ffnDim);
    void RMSNorm(const float* x, const float* weight, float* y, int dim, float eps = 1e-5f);
    void SiLU(float* x, int n);
    void SoftMax(float* x, int n);
    void SamplerApplyTemperature(float* x, int n, float invTemp);
    float SamplerFindMax(const float* x, int n);
    float SamplerExpSum(const float* x, int n, float maxVal);
    void SamplerSoftMaxTopKFused(float* logits, uint32_t* indices, int n, int K);
    void RoPE(float* q, float* k, int headDim, int nHeads,
              int nKVHeads, int pos, float theta = 10000.0f);
    void QGEMV(const void* A_quant, QuantType aType, const float* x, float* y, int M, int K);
    float VDot(const float* a, const float* b, int n);

    const CPUFeatures&          GetCPUFeatures()   const { return m_cpu; }
    const KernelDispatchTable&  GetDispatchTable()  const { return m_dispatch; }
    const LatencyRing&          GetLatencyRing()    const { return m_latency; }

    uint64_t TotalFLOPs() const { return m_totalFLOPs.load(std::memory_order_relaxed); }
    uint64_t TotalBytesRead() const { return m_totalBytesRead.load(std::memory_order_relaxed); }
    float PeakTokensPerSec() const { return m_peakTokensPerSec.load(std::memory_order_relaxed); }
    void GetDiagnostics(char* buf, size_t bufLen) const;

private:
    PatchResult PopulateDispatchTable();
    PatchResult ParseGGUFTensorDirectory();
    void MeasureAndRecord(uint32_t opType, uint32_t layer,
                          std::chrono::high_resolution_clock::time_point start);

    std::atomic<bool>           m_ready{false};
    CPUFeatures                 m_cpu;
    KernelDispatchTable         m_dispatch;
    LatencyRing                 m_latency;

    MmapRegion                  m_ggufMap;
    TensorView*                 m_tensors        = nullptr;
    uint32_t                    m_tensorCount     = 0;
    uint32_t                    m_tensorCapacity  = 0;

    std::atomic<uint64_t>       m_totalFLOPs{0};
    std::atomic<uint64_t>       m_totalBytesRead{0};
    std::atomic<float>          m_peakTokensPerSec{0.0f};

    mutable std::mutex          m_mutex;
};

// ============================================================================
// KV Cache — Sliding window with optional compression
// ============================================================================
struct KVCacheConfig {
    uint32_t    maxSeqLen;
    uint32_t    headDim;
    uint32_t    nKVHeads;
    uint32_t    nLayers;
    uint32_t    windowSize;
    bool        useSVDCompress;
    float       svdRetainRatio;

    KVCacheConfig()
        : maxSeqLen(4096), headDim(128), nKVHeads(8), nLayers(32),
          windowSize(512), useSVDCompress(false), svdRetainRatio(0.9f) {}
};

class KVCache {
public:
    KVCache();
    ~KVCache();

    KVCache(const KVCache&) = delete;
    KVCache& operator=(const KVCache&) = delete;

    PatchResult Init(const KVCacheConfig& cfg);
    PatchResult Release();
    PatchResult Store(uint32_t layer, uint32_t pos, const float* K, const float* V);
    PatchResult Retrieve(uint32_t layer, uint32_t startPos, uint32_t endPos,
                         float* outK, float* outV) const;
    PatchResult CompressWindow(uint32_t layer, uint32_t currentPos);
    uint32_t CurrentLength() const { return m_currentLen; }
    uint64_t MemoryUsageBytes() const;

private:
    KVCacheConfig   m_config;
    float**         m_keyBuffers    = nullptr;
    float**         m_valueBuffers  = nullptr;
    float**         m_compressedK   = nullptr;
    float**         m_compressedV   = nullptr;
    uint32_t        m_currentLen    = 0;
    bool            m_initialized   = false;
    mutable std::mutex m_mutex;
};

} // namespace NativeSpeed
} // namespace RawrXD
