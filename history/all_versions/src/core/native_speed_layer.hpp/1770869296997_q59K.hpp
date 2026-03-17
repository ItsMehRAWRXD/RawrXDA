// ============================================================================
// native_speed_layer.hpp — Zero-Overhead Native Speed Layer
// ============================================================================
// Provides the fastest possible inference path by eliminating all abstraction
// overhead. Direct memory-mapped model access, SIMD-vectorized compute,
// lock-free token streaming, and OS-level memory management.
//
// Three execution tiers:
//   Tier 0: Pure MASM AVX-512/AVX2 kernels (hottest path)
//   Tier 1: C++ SIMD intrinsics with cache-line alignment
//   Tier 2: Fallback scalar with auto-vectorization hints
//
// No exceptions. No STL allocators in hot path. No virtual dispatch.
// All results via PatchResult/NativeResult factory pattern.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <thread>
#include <functional>
#include <vector>
#include <string>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace RawrXD {
namespace Native {

// ============================================================================
// NativeResult — Structured result, no exceptions
// ============================================================================
struct NativeResult {
    bool        success;
    const char* detail;
    int         errorCode;
    uint64_t    latencyNs;   // Execution latency in nanoseconds

    static NativeResult ok(const char* msg = "OK") {
        NativeResult r;
        r.success   = true;
        r.detail    = msg;
        r.errorCode = 0;
        r.latencyNs = 0;
        return r;
    }

    static NativeResult error(const char* msg, int code = -1) {
        NativeResult r;
        r.success   = false;
        r.detail    = msg;
        r.errorCode = code;
        r.latencyNs = 0;
        return r;
    }
};

// ============================================================================
// CPU Feature Detection
// ============================================================================
struct CPUFeatures {
    bool hasSSE42;
    bool hasAVX;
    bool hasAVX2;
    bool hasFMA;
    bool hasAVX512F;
    bool hasAVX512BW;
    bool hasAVX512VL;
    bool hasAVX512VNNI;   // INT8 dot product
    bool hasAMXTile;      // Intel AMX
    uint32_t cacheLineSize;
    uint32_t l1CacheSize;
    uint32_t l2CacheSize;
    uint32_t l3CacheSize;
    uint32_t physicalCores;
    uint32_t logicalCores;
};

CPUFeatures DetectCPUFeatures();

// ============================================================================
// Memory Arena — OS-level allocation, cache-line aligned, no STL
// ============================================================================
class alignas(64) MemoryArena {
public:
    MemoryArena();
    ~MemoryArena();

    // Initialize arena with reserved virtual address space
    NativeResult init(size_t reserveBytes);

    // Allocate aligned memory from arena (64-byte aligned for AVX-512)
    void* allocAligned(size_t bytes, size_t alignment = 64);

    // Reset arena (invalidate all allocations, keep reserved pages)
    void reset();

    // Release all memory back to OS
    void release();

    // Stats
    size_t usedBytes()     const { return m_used.load(std::memory_order_relaxed); }
    size_t reservedBytes() const { return m_reserved; }
    size_t committedBytes()const { return m_committed.load(std::memory_order_relaxed); }

    // Non-copyable
    MemoryArena(const MemoryArena&) = delete;
    MemoryArena& operator=(const MemoryArena&) = delete;

private:
    uint8_t*          m_base;
    size_t            m_reserved;
    std::atomic<size_t> m_used;
    std::atomic<size_t> m_committed;
    size_t            m_pageSize;
    std::mutex        m_commitMutex;

    NativeResult commitPages(size_t upTo);
};

// ============================================================================
// Memory-Mapped Model — Direct file mapping, zero-copy tensor access
// ============================================================================
class MappedModel {
public:
    MappedModel();
    ~MappedModel();

    // Map a GGUF file directly into memory
    NativeResult mapFile(const char* filepath);

    // Unmap the model
    void unmap();

    // Get raw pointer to tensor data at offset
    const uint8_t* tensorData(size_t offset) const;

    // Get mapped base address
    const uint8_t* base() const { return m_mappedBase; }

    // Total mapped size
    size_t mappedSize() const { return m_mappedSize; }

    // Prefetch tensor data into CPU cache
    void prefetchRange(size_t offset, size_t length) const;

    // Lock pages in physical RAM (prevent swapping)
    NativeResult lockPages(size_t offset, size_t length);

    bool isMapped() const { return m_mappedBase != nullptr; }

private:
    uint8_t*    m_mappedBase;
    size_t      m_mappedSize;
#ifdef _WIN32
    HANDLE      m_fileHandle;
    HANDLE      m_mappingHandle;
#else
    int         m_fd;
#endif
};

// ============================================================================
// SIMD Dispatch Table — Runtime kernel selection based on CPU features
// ============================================================================

// Function pointer types for hot-path operations
using FnMatMul     = void(*)(const float* A, const float* B, float* C,
                             int M, int N, int K);
using FnVecDot     = float(*)(const float* a, const float* b, int n);
using FnSoftmax    = void(*)(float* data, int n);
using FnRMSNorm    = void(*)(float* data, const float* weight, int n, float eps);
using FnRoPE       = void(*)(float* data, int dim, int pos, int rotaryDim, float theta);
using FnGELU       = void(*)(float* data, int n);
using FnSiLU       = void(*)(float* data, int n);
using FnDequantQ4  = void(*)(const uint8_t* src, float* dst, int nElements);
using FnDequantQ8  = void(*)(const uint8_t* src, float* dst, int nElements);
using FnDequantQ4K = void(*)(const uint8_t* src, float* dst, int nElements);
using FnDequantQ6K = void(*)(const uint8_t* src, float* dst, int nElements);
using FnFlashAttn  = void(*)(const float* Q, const float* K, const float* V,
                             float* O, int seqM, int seqN, int headDim,
                             int numHeads, float scale, bool causal);
using FnFusedMLP   = void(*)(const float* input, const float* w1, const float* w2,
                             const float* w3, float* output, int dim, int hiddenDim);

struct SIMDDispatchTable {
    FnMatMul     matmul;
    FnVecDot     vecDot;
    FnSoftmax    softmax;
    FnRMSNorm    rmsNorm;
    FnRoPE       rope;
    FnGELU       gelu;
    FnSiLU       silu;
    FnDequantQ4  dequantQ4_0;
    FnDequantQ8  dequantQ8_0;
    FnDequantQ4K dequantQ4_K;
    FnDequantQ6K dequantQ6_K;
    FnFlashAttn  flashAttention;
    FnFusedMLP   fusedMLP;

    // Tier identifier
    int tier;  // 0=AVX512, 1=AVX2+FMA, 2=scalar
    const char* tierName;
};

// Build dispatch table based on detected CPU features
SIMDDispatchTable BuildDispatchTable(const CPUFeatures& cpu);

// ============================================================================
// Lock-Free Token Ring Buffer — Zero-allocation streaming output
// ============================================================================
class alignas(64) TokenRingBuffer {
public:
    static constexpr size_t CAPACITY = 4096;

    TokenRingBuffer();

    // Producer: push a token (returns false if full)
    bool push(int32_t tokenId);

    // Consumer: pop a token (returns false if empty)
    bool pop(int32_t& tokenId);

    // Check if empty
    bool empty() const;

    // Approximate count
    size_t count() const;

    // Reset
    void clear();

private:
    struct alignas(64) {
        std::atomic<uint64_t> head;
        char pad1[56];
    };
    struct alignas(64) {
        std::atomic<uint64_t> tail;
        char pad2[56];
    };
    int32_t m_buffer[CAPACITY];
};

// ============================================================================
// Thread Pool — Fixed-size, work-stealing, cache-aware
// ============================================================================
class NativeThreadPool {
public:
    using Task = std::function<void()>;

    NativeThreadPool();
    ~NativeThreadPool();

    // Initialize with specified thread count (0 = physical cores)
    NativeResult init(uint32_t numThreads = 0);

    // Submit a task
    void submit(Task task);

    // Parallel for — divide [0, count) among threads
    void parallelFor(int count, std::function<void(int start, int end, int threadId)> body);

    // Wait for all submitted tasks to complete
    void waitAll();

    // Shutdown
    void shutdown();

    uint32_t threadCount() const { return m_numThreads; }

private:
    struct WorkerState;

    std::vector<std::thread>       m_threads;
    std::vector<WorkerState*>      m_workerStates;
    std::atomic<bool>              m_running;
    uint32_t                       m_numThreads;

    void workerLoop(int threadId);
};

// ============================================================================
// Performance Counters — Lock-free telemetry
// ============================================================================
struct alignas(64) NativePerfCounters {
    std::atomic<uint64_t> tokensGenerated;
    std::atomic<uint64_t> totalInferenceNs;
    std::atomic<uint64_t> totalMatmulNs;
    std::atomic<uint64_t> totalAttentionNs;
    std::atomic<uint64_t> totalDequantNs;
    std::atomic<uint64_t> totalNormNs;
    std::atomic<uint64_t> cacheHits;
    std::atomic<uint64_t> cacheMisses;
    std::atomic<uint64_t> peakMemoryBytes;
    std::atomic<uint64_t> arenaAllocations;

    NativePerfCounters() {
        tokensGenerated.store(0, std::memory_order_relaxed);
        totalInferenceNs.store(0, std::memory_order_relaxed);
        totalMatmulNs.store(0, std::memory_order_relaxed);
        totalAttentionNs.store(0, std::memory_order_relaxed);
        totalDequantNs.store(0, std::memory_order_relaxed);
        totalNormNs.store(0, std::memory_order_relaxed);
        cacheHits.store(0, std::memory_order_relaxed);
        cacheMisses.store(0, std::memory_order_relaxed);
        peakMemoryBytes.store(0, std::memory_order_relaxed);
        arenaAllocations.store(0, std::memory_order_relaxed);
    }

    // Tokens per second (averaged)
    double tokensPerSecond() const {
        uint64_t tokens = tokensGenerated.load(std::memory_order_relaxed);
        uint64_t ns     = totalInferenceNs.load(std::memory_order_relaxed);
        if (ns == 0) return 0.0;
        return static_cast<double>(tokens) * 1e9 / static_cast<double>(ns);
    }

    void reset() {
        tokensGenerated.store(0, std::memory_order_relaxed);
        totalInferenceNs.store(0, std::memory_order_relaxed);
        totalMatmulNs.store(0, std::memory_order_relaxed);
        totalAttentionNs.store(0, std::memory_order_relaxed);
        totalDequantNs.store(0, std::memory_order_relaxed);
        totalNormNs.store(0, std::memory_order_relaxed);
        cacheHits.store(0, std::memory_order_relaxed);
        cacheMisses.store(0, std::memory_order_relaxed);
        peakMemoryBytes.store(0, std::memory_order_relaxed);
        arenaAllocations.store(0, std::memory_order_relaxed);
    }
};

// ============================================================================
// NativeSpeedLayer — The main orchestrator
// ============================================================================
class NativeSpeedLayer {
public:
    struct Config {
        uint32_t threadCount     = 0;     // 0 = auto-detect physical cores
        size_t   arenaReserveGB  = 4;     // Virtual address reservation
        size_t   kvCacheMaxMB    = 2048;  // KV cache budget
        bool     lockModelPages  = true;  // mlock model into RAM
        bool     prefetchLayers  = true;  // Prefetch next layer during compute
        bool     enableFlashAttn = true;  // Use Flash Attention if available
        bool     enableFusedMLP  = true;  // Fused Gate+Up+Down projection
        float    ropeThetaBase   = 10000.0f;
        int      maxBatchSize    = 1;
        int      maxSeqLen       = 4096;
    };

    NativeSpeedLayer();
    ~NativeSpeedLayer();

    // Initialize the speed layer (detects CPU, builds dispatch, starts threads)
    NativeResult initialize(const Config& config = Config());

    // Map a GGUF model file for zero-copy access
    NativeResult mapModel(const char* filepath);

    // Run a single forward pass on input tokens
    // Returns logits array of size [vocabSize]
    NativeResult forward(const int32_t* tokens, int numTokens, float* logitsOut);

    // Sample next token from logits (temperature + top-p)
    int32_t sampleToken(const float* logits, int vocabSize,
                        float temperature = 0.7f, float topP = 0.9f);

    // Full generation loop with streaming callback
    NativeResult generate(const int32_t* promptTokens, int promptLen,
                         int maxNewTokens,
                         std::function<bool(int32_t token)> onToken);

    // Get performance counters (lock-free read)
    const NativePerfCounters& perfCounters() const { return m_counters; }

    // Get CPU features
    const CPUFeatures& cpuFeatures() const { return m_cpuFeatures; }

    // Get dispatch table info
    const SIMDDispatchTable& dispatchTable() const { return m_dispatch; }

    // Get arena stats
    const MemoryArena& arena() const { return m_arena; }

    // Shutdown and release all resources
    void shutdown();

    // Is initialized?
    bool isReady() const { return m_ready.load(std::memory_order_acquire); }

private:
    // Internal transformer operations
    void transformerBlock(float* hidden, int layerIdx, int seqLen, int pos);
    void attention(float* hidden, int layerIdx, int seqLen, int pos);
    void feedForward(float* hidden, int layerIdx);
    void finalNorm(float* hidden);
    void logitsProjection(const float* hidden, float* logits);

    // KV Cache management
    struct KVCache {
        float* keys;      // [numLayers][maxSeq][headDim * numKVHeads]
        float* values;    // [numLayers][maxSeq][headDim * numKVHeads]
        int    currentLen;
        int    maxLen;
    };
    NativeResult initKVCache();
    void resetKVCache();

    // Model metadata (parsed from GGUF header)
    struct ModelParams {
        int vocabSize;
        int embeddingDim;
        int numLayers;
        int numHeads;
        int numKVHeads;
        int headDim;
        int hiddenDim;     // FFN intermediate size
        int maxSeqLen;
        float rmsNormEps;
        float ropeThetaBase;
        int ropeFreqScale;
        std::string architecture;  // "llama", "mistral", "phi", etc.
    };
    NativeResult parseModelParams();

    // Tensor lookup from mapped file
    struct TensorRef {
        const uint8_t* data;
        size_t         dataSize;
        int            type;      // ggml_type
        int64_t        shape[4];
        int            nDims;
    };
    NativeResult findTensor(const char* name, TensorRef& ref) const;

    // State
    std::atomic<bool>   m_ready;
    Config              m_config;
    CPUFeatures         m_cpuFeatures;
    SIMDDispatchTable   m_dispatch;
    MemoryArena         m_arena;
    MappedModel         m_model;
    NativeThreadPool    m_threadPool;
    NativePerfCounters  m_counters;
    KVCache             m_kvCache;
    ModelParams         m_params;

    // Scratch buffers (allocated from arena)
    float* m_scratchA;
    float* m_scratchB;
    float* m_scratchAttn;
    float* m_scratchLogits;
};

} // namespace Native
} // namespace RawrXD
