// ============================================================================
// nvidia_cuda_accelerator.h — NVIDIA CUDA GPU Acceleration (Driver API)
// ============================================================================
// Phase 31: Runtime-loaded NVIDIA CUDA backend via Driver API (nvcuda.dll).
// No CUDA Toolkit / nvcc required at compile time — pure C++20 + LoadLibrary.
//
// Mirrors Intel/AMD accelerator pattern:
//   - Singleton, PatchResult-style results, no exceptions
//   - Dynamic runtime loading via LoadLibrary("nvcuda.dll")
//   - Function-pointer callbacks (no std::function in hot path)
//   - DXGI vendor-ID 0x10DE detection
//
// Backend: cuInit → cuDeviceGet → cuCtxCreate → cuMemAlloc → cuLaunchKernel
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef NVIDIA_CUDA_ACCELERATOR_H
#define NVIDIA_CUDA_ACCELERATOR_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <unordered_map>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// ============================================================================
// CUDA Driver API opaque types (avoid cuda.h dependency)
// ============================================================================

using CUdevice    = int;
using CUcontext   = void*;
using CUmodule    = void*;
using CUfunction  = void*;
using CUdeviceptr = unsigned long long;
using CUstream    = void*;
using CUevent     = void*;
using CUresult    = int;

constexpr CUresult CUDA_SUCCESS = 0;

// Event flags (from cuda.h — stable ABI)
constexpr unsigned int CU_EVENT_DEFAULT       = 0x00;
constexpr unsigned int CU_EVENT_BLOCKING_SYNC = 0x01;
constexpr unsigned int CU_EVENT_DISABLE_TIMING = 0x02;

// ============================================================================
// NVIDIA GPU Architecture Classification
// ============================================================================

enum class NvidiaGPUArch : uint8_t {
    Unknown     = 0,
    Maxwell     = 1,   // SM 5.x  (GTX 9xx)
    Pascal      = 2,   // SM 6.x  (GTX 10xx, Tesla P100)
    Volta       = 3,   // SM 7.0  (Tesla V100, Titan V)
    Turing      = 4,   // SM 7.5  (RTX 20xx, GTX 16xx)
    Ampere      = 5,   // SM 8.x  (RTX 30xx, A100)
    Ada         = 6,   // SM 8.9  (RTX 40xx, L40)
    Hopper      = 7,   // SM 9.0  (H100, H200)
    Blackwell   = 8,   // SM 10.0 (B100, B200, GB200)
};

// ============================================================================
// NVIDIA CUDA Feature Flags
// ============================================================================

enum class NvidiaFeatureFlag : uint32_t {
    TensorCores     = 0x0001,  // Tensor Core support (Volta+)
    FP16            = 0x0002,  // Native FP16 compute
    BF16            = 0x0004,  // Native BF16 (Ampere+)
    INT8            = 0x0008,  // INT8 Tensor Cores
    INT4            = 0x0010,  // INT4 Tensor Cores (Ada+)
    FP8             = 0x0020,  // FP8 (Hopper+)
    TF32            = 0x0040,  // TF32 automatic precision (Ampere+)
    FP64            = 0x0080,  // FP64 double precision
    NVLink          = 0x0100,  // NVLink interconnect
    AsyncCopy       = 0x0200,  // Async memory copy engine
    CoopGroups      = 0x0400,  // Cooperative groups
    DynamicParallel = 0x0800,  // Dynamic parallelism
    UnifiedMem      = 0x1000,  // CUDA Unified Memory (managed)
    MPS             = 0x2000,  // Multi-Process Service
    MIG             = 0x4000,  // Multi-Instance GPU (A100+)
    FlashAttn       = 0x8000,  // Flash Attention acceleration (Ampere+)
};

// ============================================================================
// NVIDIA Acceleration Scope (mirrors AMD/Intel pattern)
// ============================================================================

enum class NvidiaAccelScope : uint8_t {
    Inference        = 0x01,
    Quantization     = 0x02,
    ModelSurgery     = 0x04,
    SwarmCompute     = 0x08,
    KVCache          = 0x10,
    Embedding        = 0x20,
    AllReduce        = 0x40,
    All              = 0xFF
};

// ============================================================================
// NVIDIA GPU Memory Pool
// ============================================================================

struct NvidiaGPUMemoryPool {
    uint64_t totalBytes;
    uint64_t freeBytes;
    uint64_t usedBytes;
    uint64_t peakBytes;
    uint64_t allocCount;
    uint64_t freeCount;

    NvidiaGPUMemoryPool()
        : totalBytes(0), freeBytes(0), usedBytes(0), peakBytes(0)
        , allocCount(0), freeCount(0)
    {}

    uint64_t availableBytes() const { return freeBytes; }
    double usagePercent() const { return totalBytes > 0 ? (100.0 * usedBytes / totalBytes) : 0; }
};

// ============================================================================
// NVIDIA GPU Buffer Handle
// ============================================================================

struct NvidiaGPUBuffer {
    CUdeviceptr devicePtr;
    void*       hostPtr;
    uint64_t    sizeBytes;
    uint32_t    bufferId;
    bool        mapped;

    NvidiaGPUBuffer()
        : devicePtr(0), hostPtr(nullptr), sizeBytes(0)
        , bufferId(0), mapped(false)
    {}
};

// ============================================================================
// NVIDIA KV-Cache Configuration
// ============================================================================

struct NvidiaKVCacheConfig {
    uint32_t numLayers;     // Number of transformer layers
    uint32_t numHeads;      // Number of attention heads per layer
    uint32_t headDim;       // Dimension per head (e.g., 128)
    uint32_t maxSeqLen;     // Maximum sequence length (context window)

    NvidiaKVCacheConfig()
        : numLayers(0), numHeads(0), headDim(0), maxSeqLen(0) {}
    NvidiaKVCacheConfig(uint32_t layers, uint32_t heads, uint32_t dim, uint32_t maxSeq)
        : numLayers(layers), numHeads(heads), headDim(dim), maxSeqLen(maxSeq) {}

    uint64_t bytesPerLayer() const {
        // K + V, each [maxSeqLen, headDim] per head, float32
        return 2ULL * numHeads * maxSeqLen * headDim * sizeof(float);
    }
    uint64_t totalBytes() const { return numLayers * bytesPerLayer(); }
};

// ============================================================================
// NVIDIA KV-Cache State (GPU-resident per-layer K/V tensors)
// ============================================================================
// Each layer has numHeads * 2 buffers (K and V), each [maxSeqLen, headDim].
// Append writes one row at currentPos; cached attention reads [0..currentPos).

struct NvidiaKVCacheLayer {
    std::vector<NvidiaGPUBuffer> keyHeads;    // [numHeads] — each [maxSeqLen, headDim]
    std::vector<NvidiaGPUBuffer> valueHeads;  // [numHeads] — each [maxSeqLen, headDim]
};

struct NvidiaKVCache {
    NvidiaKVCacheConfig config;
    std::vector<NvidiaKVCacheLayer> layers;   // [numLayers]
    uint32_t currentPos;                      // Next write position (# cached tokens)
    bool     initialized;

    NvidiaKVCache() : currentPos(0), initialized(false) {}

    uint32_t cachedTokens() const { return currentPos; }
    bool     isFull() const { return config.maxSeqLen > 0 && currentPos >= config.maxSeqLen; }
    uint64_t usedBytes() const {
        return config.numLayers * 2ULL * config.numHeads * currentPos * config.headDim * sizeof(float);
    }
};

// ============================================================================
// GPU Weight Tensor — one GGUF tensor uploaded to GPU memory
// ============================================================================

enum class NvidiaWeightFormat : uint8_t {
    F32  = 0,   // Native float32 — ready for compute
    F16  = 1,   // Half precision — needs conversion for f32 kernels
    Q4_0 = 2,   // 4-bit quantized blocks (18 bytes per 32 elements)
    Q8_0 = 3,   // 8-bit quantized blocks (34 bytes per 32 elements)
    Raw  = 0xFF // Opaque — uploaded as-is, format interpreted by caller
};

struct NvidiaGPUWeight {
    std::string          name;         // GGUF tensor name (e.g. "blk.0.attn_q.weight")
    NvidiaGPUBuffer      buffer;       // GPU memory holding the tensor data
    NvidiaWeightFormat   format;       // How data is stored on GPU
    std::vector<uint64_t> shape;       // Original dimensions (e.g. [4096, 4096])
    uint64_t             elements;     // Total scalar element count
    uint64_t             rawBytes;     // Bytes on GPU (may be quantized)

    NvidiaGPUWeight() : format(NvidiaWeightFormat::Raw), elements(0), rawBytes(0) {}
};

// ============================================================================
// GPU Weight Map — all model weights resident on GPU
// ============================================================================

struct NvidiaGPUWeightMap {
    std::vector<NvidiaGPUWeight>                         weights;
    std::unordered_map<std::string, size_t>              nameIndex;   // name → index
    uint64_t                                             totalGPUBytes;
    bool                                                 loaded;

    NvidiaGPUWeightMap() : totalGPUBytes(0), loaded(false) {}

    const NvidiaGPUWeight* find(const std::string& name) const {
        auto it = nameIndex.find(name);
        return (it != nameIndex.end()) ? &weights[it->second] : nullptr;
    }
};

// ============================================================================
// GPU Sampler Configuration
// ============================================================================

struct NvidiaSamplerConfig {
    float    temperature;         // Logit temperature scaling (0 = greedy)
    uint32_t topK;                // Top-K filter (0 = disabled)
    float    topP;                // Nucleus (top-p) threshold
    float    repetitionPenalty;   // Repetition penalty multiplier (1.0 = off)
    uint64_t seed;                // RNG seed (0 = random)
    float    minP;                // Min-P threshold (0 = disabled)

    NvidiaSamplerConfig()
        : temperature(0.7f), topK(40), topP(0.9f)
        , repetitionPenalty(1.1f), seed(0), minP(0.0f) {}

    bool isGreedy() const { return temperature <= 0.0f; }
};

// ============================================================================
// GPU Generation Configuration
// ============================================================================

struct NvidiaGenerationConfig {
    uint32_t maxTokens;           // Maximum tokens to generate
    uint32_t eosTokenId;          // End-of-sequence token ID
    uint32_t bosTokenId;          // Beginning-of-sequence token ID
    uint32_t padTokenId;          // Padding token ID
    uint32_t vocabSize;           // Vocabulary size (for logit buffer sizing)
    uint32_t numLayers;           // Transformer layer count
    uint32_t numHeads;            // Attention head count
    uint32_t headDim;             // Per-head dimension
    uint32_t hiddenDim;           // Model hidden dimension
    uint32_t ffnDim;              // FFN intermediate dimension
    uint32_t repPenaltyWindow;    // How many recent tokens to penalize
    NvidiaSamplerConfig sampler;

    NvidiaGenerationConfig()
        : maxTokens(256), eosTokenId(2), bosTokenId(1)
        , padTokenId(0), vocabSize(32000), numLayers(32)
        , numHeads(32), headDim(128), hiddenDim(4096)
        , ffnDim(11008), repPenaltyWindow(64) {}
};

// ============================================================================
// GPU Generation Result
// ============================================================================

struct NvidiaGenerationResult {
    std::vector<uint32_t> tokens;      // Generated token IDs
    uint32_t              promptLen;   // Number of prompt tokens processed
    uint32_t              genLen;      // Number of tokens generated
    double                totalMs;     // Total wall-clock time
    double                prefillMs;   // Prompt prefill time
    double                decodeMs;    // Autoregressive decode time
    double                tokPerSec;   // Decode throughput
    bool                  hitEOS;      // Terminated via EOS token
    bool                  success;
    const char*           detail;

    NvidiaGenerationResult()
        : promptLen(0), genLen(0), totalMs(0), prefillMs(0)
        , decodeMs(0), tokPerSec(0), hitEOS(false)
        , success(false), detail("not started") {}
};

// ============================================================================
// NVIDIA Acceleration Result (PatchResult-compatible)
// ============================================================================

struct NvidiaAccelResult {
    bool        success;
    const char* detail;
    int         errorCode;
    double      elapsedMs;
    double      throughputGFLOPS;

    static NvidiaAccelResult ok(const char* msg) {
        NvidiaAccelResult r;
        r.success = true; r.detail = msg; r.errorCode = 0;
        r.elapsedMs = 0; r.throughputGFLOPS = 0;
        return r;
    }
    static NvidiaAccelResult error(const char* msg, int code = -1) {
        NvidiaAccelResult r;
        r.success = false; r.detail = msg; r.errorCode = code;
        r.elapsedMs = 0; r.throughputGFLOPS = 0;
        return r;
    }
};

// ============================================================================
// NVIDIA GPU Accelerator Statistics
// ============================================================================

struct NvidiaAccelStats {
    std::atomic<uint64_t> gpuDispatches{0};
    std::atomic<uint64_t> cpuFallbacks{0};
    std::atomic<uint64_t> gpuAllocBytes{0};
    std::atomic<uint64_t> gpuFreeBytes{0};
    std::atomic<uint64_t> gpuCopyH2D{0};
    std::atomic<uint64_t> gpuCopyD2H{0};
    std::atomic<uint64_t> gpuComputeMs{0};
    std::atomic<uint64_t> gpuWaitMs{0};
    std::atomic<uint64_t> tensorCoreDispatches{0};
    std::atomic<uint64_t> cudaCoreDispatches{0};
    std::atomic<uint64_t> toggleOnCount{0};
    std::atomic<uint64_t> toggleOffCount{0};
    double peakTFLOPS{0};
    double avgSMOccupancy{0};
};

// ============================================================================
// Callback types (no std::function in hot paths)
// ============================================================================

typedef void (*NvidiaGPUToggleCallback)(bool enabled, void* userData);
typedef void (*NvidiaGPUErrorCallback)(const char* msg, int code, void* userData);
typedef void (*NvidiaGPUMemoryCallback)(uint64_t used, uint64_t total, void* userData);

// ============================================================================
// CUDA Driver API Function Pointer Table
// ============================================================================

struct CudaDriverAPI {
    // -- Initialization --
    CUresult (*cuInit)(unsigned int flags);
    CUresult (*cuDriverGetVersion)(int* version);

    // -- Device Management --
    CUresult (*cuDeviceGetCount)(int* count);
    CUresult (*cuDeviceGet)(CUdevice* device, int ordinal);
    CUresult (*cuDeviceGetName)(char* name, int len, CUdevice dev);
    CUresult (*cuDeviceTotalMem_v2)(size_t* bytes, CUdevice dev);
    CUresult (*cuDeviceGetAttribute)(int* pi, int attrib, CUdevice dev);
    CUresult (*cuDeviceComputeCapability)(int* major, int* minor, CUdevice dev);

    // -- Context Management --
    CUresult (*cuCtxCreate_v2)(CUcontext* ctx, unsigned int flags, CUdevice dev);
    CUresult (*cuCtxDestroy_v2)(CUcontext ctx);
    CUresult (*cuCtxSetCurrent)(CUcontext ctx);
    CUresult (*cuCtxSynchronize)();

    // -- Memory Management --
    CUresult (*cuMemAlloc_v2)(CUdeviceptr* dptr, size_t bytesize);
    CUresult (*cuMemFree_v2)(CUdeviceptr dptr);
    CUresult (*cuMemcpyHtoD_v2)(CUdeviceptr dstDevice, const void* srcHost, size_t byteCount);
    CUresult (*cuMemcpyDtoH_v2)(void* dstHost, CUdeviceptr srcDevice, size_t byteCount);
    CUresult (*cuMemcpyDtoD_v2)(CUdeviceptr dstDevice, CUdeviceptr srcDevice, size_t byteCount);
    CUresult (*cuMemGetInfo_v2)(size_t* free, size_t* total);

    // -- Stream Management --
    CUresult (*cuStreamCreate)(CUstream* phStream, unsigned int flags);
    CUresult (*cuStreamDestroy_v2)(CUstream hStream);
    CUresult (*cuStreamSynchronize)(CUstream hStream);

    // -- Event Management --
    CUresult (*cuEventCreate)(CUevent* phEvent, unsigned int flags);
    CUresult (*cuEventDestroy_v2)(CUevent hEvent);
    CUresult (*cuEventRecord)(CUevent hEvent, CUstream hStream);
    CUresult (*cuEventSynchronize)(CUevent hEvent);
    CUresult (*cuEventElapsedTime)(float* pMilliseconds, CUevent hStart, CUevent hEnd);
    CUresult (*cuStreamWaitEvent)(CUstream hStream, CUevent hEvent, unsigned int flags);

    // -- Async Memory Operations --
    CUresult (*cuMemcpyHtoDAsync_v2)(CUdeviceptr dstDevice, const void* srcHost,
                                     size_t byteCount, CUstream hStream);
    CUresult (*cuMemcpyDtoHAsync_v2)(void* dstHost, CUdeviceptr srcDevice,
                                     size_t byteCount, CUstream hStream);

    // -- Module/Kernel Management --
    CUresult (*cuModuleLoadData)(CUmodule* module, const void* image);
    CUresult (*cuModuleLoadDataEx)(CUmodule* module, const void* image,
                                   unsigned int numOptions, void* options, void** optionValues);
    CUresult (*cuModuleUnload)(CUmodule hmod);
    CUresult (*cuModuleGetFunction)(CUfunction* hfunc, CUmodule hmod, const char* name);
    CUresult (*cuLaunchKernel)(CUfunction f,
                               unsigned int gridDimX, unsigned int gridDimY, unsigned int gridDimZ,
                               unsigned int blockDimX, unsigned int blockDimY, unsigned int blockDimZ,
                               unsigned int sharedMemBytes, CUstream hStream,
                               void** kernelParams, void** extra);

    bool loaded;
    CudaDriverAPI() : loaded(false) { memset(this, 0, sizeof(*this)); }
};

// ============================================================================
// Multi-Stream Pipeline Pool
// ============================================================================

struct NvidiaStreamSlot {
    CUstream stream   = nullptr;
    CUevent  event    = nullptr;    // Recorded after kernel on this stream
    bool     busy     = false;      // Currently has pending work
};

struct NvidiaStreamPool {
    static constexpr uint32_t MAX_STREAMS = 4;  // Compute + 2x transfer + spare

    NvidiaStreamSlot slots[MAX_STREAMS];         // Pre-allocated streams
    CUevent          fenceEvent = nullptr;       // Global fence for device sync
    uint32_t         count      = 0;             // Active stream count
    bool             initialized = false;

    // Pinned host staging buffer for async H2D/D2H
    void*            pinnedStaging     = nullptr;
    uint64_t         pinnedStagingSize = 0;
};

// ============================================================================
// NvidiaCudaAccelerator — Singleton Master Toggle
// ============================================================================

class NvidiaCudaAccelerator {
public:
    static NvidiaCudaAccelerator& instance();

    // ===== Lifecycle =====
    NvidiaAccelResult initialize(int preferredDevice = 0);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ===== MASTER TOGGLE =====
    NvidiaAccelResult enableGPU();
    NvidiaAccelResult disableGPU();
    bool isGPUEnabled() const { return m_gpuEnabled.load(std::memory_order_acquire); }
    NvidiaAccelResult toggleGPU();

    // ===== Scope Toggles (per-subsystem) =====
    NvidiaAccelResult enableScope(NvidiaAccelScope scope);
    NvidiaAccelResult disableScope(NvidiaAccelScope scope);
    bool isScopeEnabled(NvidiaAccelScope scope) const;
    uint8_t getEnabledScopes() const { return m_enabledScopes.load(std::memory_order_acquire); }

    // ===== Backend Info =====
    const char*      getBackendName() const { return "NVIDIA CUDA (Driver API)"; }
    NvidiaGPUArch    getArchitecture() const { return m_arch; }
    uint32_t         getFeatureFlags() const { return m_features; }
    bool             hasFeature(NvidiaFeatureFlag feature) const;
    std::string      getGPUName() const { return m_gpuName; }
    int              getDeviceCount() const { return m_deviceCount; }
    int              getActiveDevice() const { return m_activeDevice; }
    uint64_t         getVRAMBytes() const { return m_vramBytes; }
    int              getSMCount() const { return m_smCount; }
    int              getComputeCapMajor() const { return m_ccMajor; }
    int              getComputeCapMinor() const { return m_ccMinor; }
    int              getDriverVersion() const { return m_driverVersion; }

    // ===== Memory Management =====
    NvidiaAccelResult allocGPU(uint64_t sizeBytes, NvidiaGPUBuffer& outBuffer);
    NvidiaAccelResult freeGPU(NvidiaGPUBuffer& buffer);
    NvidiaAccelResult copyToGPU(NvidiaGPUBuffer& dst, const void* hostSrc, uint64_t bytes);
    NvidiaAccelResult copyFromGPU(void* hostDst, const NvidiaGPUBuffer& src, uint64_t bytes);
    NvidiaAccelResult queryMemInfo();
    const NvidiaGPUMemoryPool& getMemoryPool() const { return m_memPool; }

    // ===== Compute Dispatch =====
    NvidiaAccelResult dispatchMatMul(const NvidiaGPUBuffer& A, const NvidiaGPUBuffer& B,
                                    NvidiaGPUBuffer& C,
                                    uint32_t M, uint32_t N, uint32_t K);
    NvidiaAccelResult dispatchSoftmax(const NvidiaGPUBuffer& input, NvidiaGPUBuffer& output,
                                     uint32_t rows, uint32_t cols);
    NvidiaAccelResult dispatchRMSNorm(const NvidiaGPUBuffer& input, const NvidiaGPUBuffer& weight,
                                     NvidiaGPUBuffer& output, uint32_t size, float epsilon);
    NvidiaAccelResult dispatchRoPE(NvidiaGPUBuffer& qk, uint32_t seqLen, uint32_t headDim,
                                  uint32_t posOffset);
    // Fused scaled dot-product attention: O = softmax(Q·K^T / sqrt(d_k)) · V
    // Supports optional causal masking. Operates on a single attention head.
    // Q: [seqM, headDim]   K: [seqN, headDim]   V: [seqN, headDim]   O: [seqM, headDim]
    NvidiaAccelResult dispatchAttention(const NvidiaGPUBuffer& Q, const NvidiaGPUBuffer& K,
                                       const NvidiaGPUBuffer& V, NvidiaGPUBuffer& O,
                                       uint32_t seqM, uint32_t seqN, uint32_t headDim,
                                       float scale, bool causal);

    // ===== KV-Cache Management =====
    // Allocate per-layer, per-head K/V buffers on the GPU.
    NvidiaAccelResult initKVCache(const NvidiaKVCacheConfig& config);
    // Append one token's K/V for a given layer and head into the cache at currentPos.
    // keyRow / valueRow: host pointers to [headDim] floats.
    NvidiaAccelResult appendKV(uint32_t layer, uint32_t head,
                               const float* keyRow, const float* valueRow);
    // Advance the cache position after all layers/heads have been appended for one token.
    void advanceKVPos();
    // Run attention using cached K/V for a single head.
    // Q: [1, headDim] (single query token), O: [1, headDim] output.
    // Uses K/V from cache[layer][head][0..currentPos].
    NvidiaAccelResult dispatchCachedAttention(const NvidiaGPUBuffer& Q, NvidiaGPUBuffer& O,
                                              uint32_t layer, uint32_t head,
                                              float scale, bool causal);
    // Reset position counter (memory stays allocated).
    void resetKVCache();
    // Deallocate all GPU memory for the KV cache.
    NvidiaAccelResult freeKVCache();
    // Query cache state.
    const NvidiaKVCache& getKVCache() const { return m_kvCache; }
    bool isKVCacheReady() const { return m_kvCache.initialized; }

    // ===== GPU Weight Loading =====
    // Upload a single tensor to GPU memory. Data is stored in its native format.
    NvidiaAccelResult uploadWeight(const std::string& name,
                                   const void* hostData, uint64_t sizeBytes,
                                   NvidiaWeightFormat format,
                                   const std::vector<uint64_t>& shape);
    // Dequantize a Q4_0 weight on-device to F32 output buffer.
    NvidiaAccelResult dequantQ4_0(const NvidiaGPUWeight& qWeight, NvidiaGPUBuffer& f32Out);
    // Free a single weight.
    NvidiaAccelResult freeWeight(const std::string& name);
    // Free all loaded weights.
    NvidiaAccelResult freeAllWeights();
    // Query weight map.
    const NvidiaGPUWeightMap& getWeightMap() const { return m_weightMap; }
    bool isWeightLoaded(const std::string& name) const { return m_weightMap.find(name) != nullptr; }
    uint64_t totalWeightBytes() const { return m_weightMap.totalGPUBytes; }

    // ===== Sampling =====
    // GPU argmax (greedy): parallel reduction on GPU, returns token ID.
    NvidiaAccelResult dispatchArgmax(const NvidiaGPUBuffer& logits, uint32_t vocabSize,
                                    uint32_t& outTokenId);
    // Full sampling pipeline: temperature → top-k → top-p → sample.
    // For greedy (temperature ≤ 0), uses GPU argmax. Otherwise copies logits
    // to host and applies CPU top-k/top-p (fast for 32K–128K vocab).
    NvidiaAccelResult dispatchSample(const NvidiaGPUBuffer& logits, uint32_t vocabSize,
                                    const NvidiaSamplerConfig& config,
                                    const uint32_t* recentTokens, uint32_t recentCount,
                                    uint32_t& outTokenId);

    // ===== Token Generation Loop =====
    // Full autoregressive generation: prefill prompt → decode loop → return tokens.
    // Requires weights loaded and KV-cache initialized.
    NvidiaGenerationResult generateTokens(const std::vector<uint32_t>& promptTokens,
                                          const NvidiaGenerationConfig& config);

    // Single forward pass: embed → layers → logits. Returns logits on GPU.
    // outputLogits must be pre-allocated to vocabSize * sizeof(float).
    NvidiaAccelResult forwardPass(uint32_t tokenId, const NvidiaGenerationConfig& config,
                                 NvidiaGPUBuffer& outputLogits, NvidiaGPUBuffer& hiddenState);

    // ===== Stream Management =====
    NvidiaAccelResult createStream(CUstream& outStream);
    NvidiaAccelResult destroyStream(CUstream stream);
    NvidiaAccelResult syncStream(CUstream stream);
    NvidiaAccelResult syncDevice();

    // ===== Event Management =====
    NvidiaAccelResult createEvent(CUevent& outEvent, unsigned int flags = CU_EVENT_DEFAULT);
    NvidiaAccelResult destroyEvent(CUevent event);
    NvidiaAccelResult recordEvent(CUevent event, CUstream stream = nullptr);
    NvidiaAccelResult syncEvent(CUevent event);
    NvidiaAccelResult streamWaitEvent(CUstream stream, CUevent event);
    NvidiaAccelResult eventElapsedMs(CUevent start, CUevent end, float& outMs);

    // ===== Multi-Stream Pipeline =====
    NvidiaAccelResult initStreamPool(uint32_t numStreams = NvidiaStreamPool::MAX_STREAMS);
    NvidiaAccelResult destroyStreamPool();
    bool isStreamPoolReady() const { return m_streamPool.initialized; }
    const NvidiaStreamPool& getStreamPool() const { return m_streamPool; }

    // Async memory transfers (for pipelined overlap)
    NvidiaAccelResult copyToGPUAsync(NvidiaGPUBuffer& dst, const void* hostSrc,
                                    uint64_t bytes, CUstream stream);
    NvidiaAccelResult copyFromGPUAsync(void* hostDst, const NvidiaGPUBuffer& src,
                                      uint64_t bytes, CUstream stream);

    // Stream-aware dispatch variants (kernel on specific stream)
    NvidiaAccelResult dispatchMatMulOnStream(const NvidiaGPUBuffer& A, const NvidiaGPUBuffer& B,
                                            NvidiaGPUBuffer& C, uint32_t M, uint32_t N,
                                            uint32_t K, CUstream stream);
    NvidiaAccelResult dispatchRMSNormOnStream(const NvidiaGPUBuffer& input,
                                             const NvidiaGPUBuffer& weight,
                                             NvidiaGPUBuffer& output, uint32_t size,
                                             float epsilon, CUstream stream);

    // Pipelined forward pass using stream pool for compute/transfer overlap
    NvidiaAccelResult forwardPassPipelined(uint32_t tokenId,
                                          const NvidiaGenerationConfig& config,
                                          NvidiaGPUBuffer& outputLogits,
                                          NvidiaGPUBuffer& hiddenState);

    // Pipelined autoregressive generation with multi-stream overlap
    NvidiaGenerationResult generateTokensPipelined(
        const std::vector<uint32_t>& promptTokens,
        const NvidiaGenerationConfig& config);

    // ===== PTX Module Loading =====
    NvidiaAccelResult loadPTXModule(const void* ptxData, size_t ptxSize, CUmodule& outModule);
    NvidiaAccelResult unloadModule(CUmodule mod);
    NvidiaAccelResult getKernelFunction(CUmodule mod, const char* name, CUfunction& outFunc);
    NvidiaAccelResult launchKernel(CUfunction func,
                                  uint32_t gridX, uint32_t gridY, uint32_t gridZ,
                                  uint32_t blockX, uint32_t blockY, uint32_t blockZ,
                                  uint32_t sharedMem, CUstream stream,
                                  void** params);

    // ===== Callbacks =====
    void setToggleCallback(NvidiaGPUToggleCallback cb, void* userData);
    void setErrorCallback(NvidiaGPUErrorCallback cb, void* userData);
    void setMemoryCallback(NvidiaGPUMemoryCallback cb, void* userData);

    // ===== Statistics =====
    const NvidiaAccelStats& getStats() const { return m_stats; }
    void resetStats();

    // ===== Driver API Access (for advanced users) =====
    const CudaDriverAPI& getDriverAPI() const { return m_api; }

private:
    NvidiaCudaAccelerator();
    ~NvidiaCudaAccelerator();
    NvidiaCudaAccelerator(const NvidiaCudaAccelerator&) = delete;
    NvidiaCudaAccelerator& operator=(const NvidiaCudaAccelerator&) = delete;

    // Driver loading
    bool loadDriverLibrary();
    bool resolveDriverFunctions();
    NvidiaGPUArch classifyArch(int ccMajor, int ccMinor) const;
    uint32_t detectFeatures(int ccMajor, int ccMinor) const;

    // Driver state
    HMODULE         m_hCudaLib = nullptr;
    CudaDriverAPI   m_api;
    CUdevice        m_device = 0;
    CUcontext       m_context = nullptr;

    // Device info
    std::string     m_gpuName;
    NvidiaGPUArch   m_arch = NvidiaGPUArch::Unknown;
    uint32_t        m_features = 0;
    uint64_t        m_vramBytes = 0;
    int             m_deviceCount = 0;
    int             m_activeDevice = 0;
    int             m_smCount = 0;
    int             m_ccMajor = 0;
    int             m_ccMinor = 0;
    int             m_driverVersion = 0;
    int             m_clockRateMHz = 0;
    int             m_memBusWidth = 0;

    // State
    std::atomic<bool>    m_initialized{false};
    std::atomic<bool>    m_gpuEnabled{false};
    std::atomic<uint8_t> m_enabledScopes{0};
    std::mutex           m_mutex;
    uint32_t             m_nextBufferId = 1;

    // Memory pool tracking
    NvidiaGPUMemoryPool m_memPool;
    NvidiaAccelStats    m_stats;

    // KV-cache
    NvidiaKVCache m_kvCache;

    // GPU weight storage
    NvidiaGPUWeightMap m_weightMap;

    // Multi-stream pipeline pool
    NvidiaStreamPool m_streamPool;

    // Sampler RNG state (PCG32 — 10-20x faster than mt19937)
    uint64_t m_rngState = 0x853c49e6748fea9bULL;
    uint64_t m_rngInc   = 0xda3e39cb94b95bdbULL;
    uint32_t pcg32Next();
    float    pcg32Nextf(); // [0, 1)

    // CPU-side sampling helpers (called from dispatchSample for stochastic modes)
    uint32_t sampleTopKTopP(float* logits, uint32_t vocabSize,
                            const NvidiaSamplerConfig& config,
                            const uint32_t* recentTokens, uint32_t recentCount);

    // Callbacks
    NvidiaGPUToggleCallback m_toggleCb   = nullptr;
    void*                   m_toggleData = nullptr;
    NvidiaGPUErrorCallback  m_errorCb    = nullptr;
    void*                   m_errorData  = nullptr;
    NvidiaGPUMemoryCallback m_memoryCb   = nullptr;
    void*                   m_memoryData = nullptr;
};

#endif // NVIDIA_CUDA_ACCELERATOR_H
