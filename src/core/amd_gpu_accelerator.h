// ============================================================================
// amd_gpu_accelerator.h — Toggleable AMD/ATI GPU Acceleration
// ============================================================================
// Master GPU acceleration toggle that wires AMD GPU support across ALL
// subsystems: inference, hotpatching, swarm, model surgery, kernel auto-tuner.
// Supports DX12 Compute, Vulkan Compute, ROCm/HIP, and OpenCL backends.
// User's AMD ATI GPU is fully utilized when toggled ON.
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef AMD_GPU_ACCELERATOR_H
#define AMD_GPU_ACCELERATOR_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>
#include <functional>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// ============================================================================
// GPU Backend Selection
// ============================================================================

enum class GPUBackend : uint8_t {
    None        = 0,   // CPU only
    DX12Compute = 1,   // DirectX 12 Compute Shaders
    Vulkan      = 2,   // Vulkan Compute Shaders
    ROCm_HIP    = 3,   // ROCm/HIP (AMD native)
    OpenCL      = 4,   // OpenCL (cross-vendor fallback)
    Auto        = 255  // Auto-detect best backend
};

enum class AccelScope : uint8_t {
    Inference        = 0x01,  // Model inference (matmul, attention)
    Quantization     = 0x02,  // Quantize/dequantize operations
    ModelSurgery     = 0x04,  // Model hotpatching byte operations
    SwarmCompute     = 0x08,  // Distributed swarm GPU offload
    KVCache          = 0x10,  // KV cache management on GPU
    Embedding        = 0x20,  // Token embedding lookup
    AllReduce        = 0x40,  // Multi-GPU all-reduce
    All              = 0xFF
};

enum class AMDFeatureFlag : uint32_t {
    WMMA            = 0x0001,  // Wave Matrix Multiply Accumulate (RDNA3)
    MFMA            = 0x0002,  // Matrix Fused Multiply Add (CDNA)
    DPP             = 0x0004,  // Data Parallel Primitives
    PackedFP16      = 0x0008,  // FP16x2 packed math
    PackedBF16      = 0x0010,  // BF16x2 packed math
    INT8DP4         = 0x0020,  // INT8 4-element dot product
    INT4DP8         = 0x0040,  // INT4 8-element dot product
    InfinityCache   = 0x0080,  // Infinity Cache (RDNA2/3)
    AsyncCompute    = 0x0100,  // Async compute queue
    AsyncCopy       = 0x0200,  // DMA copy engine
    LDSPrefetch     = 0x0400,  // LDS software prefetch
    WavefrontSize64 = 0x0800,  // 64-wide wavefront
    WavefrontSize32 = 0x1000,  // 32-wide wavefront (RDNA wave32)
    ShaderClock     = 0x2000,  // shader_clock extension
    SubgroupShuffle = 0x4000,  // Subgroup shuffle operations
    FP64            = 0x8000   // Native FP64 support (CDNA)
};

// ============================================================================
// GPU Memory Pool
// ============================================================================

struct GPUMemoryPool {
    uint64_t totalBytes;
    uint64_t usedBytes;
    uint64_t peakBytes;
    uint64_t allocCount;
    uint64_t freeCount;
    bool unified; // Unified memory (APU or SAM)

    GPUMemoryPool()
        : totalBytes(0), usedBytes(0), peakBytes(0)
        , allocCount(0), freeCount(0), unified(false)
    {}

    uint64_t availableBytes() const { return totalBytes - usedBytes; }
    double usagePercent() const { return totalBytes > 0 ? (100.0 * usedBytes / totalBytes) : 0; }
};

// ============================================================================
// GPU Buffer Handle
// ============================================================================

struct GPUBuffer {
    void* devicePtr;        // GPU-side pointer (or mapped host ptr)
    void* hostPtr;          // Host-mapped pointer (if mapped)
    uint64_t sizeBytes;
    uint32_t bufferId;
    bool mapped;
    bool coherent;          // AMD SAM (Smart Access Memory)

    GPUBuffer()
        : devicePtr(nullptr), hostPtr(nullptr), sizeBytes(0)
        , bufferId(0), mapped(false), coherent(false)
    {}
};

// ============================================================================
// Acceleration Result
// ============================================================================

struct AccelResult {
    bool success;
    const char* detail;
    int errorCode;
    double elapsedMs;
    double throughputGFLOPS;

    static AccelResult ok(const char* msg) {
        AccelResult r; r.success = true; r.detail = msg;
        r.errorCode = 0; r.elapsedMs = 0; r.throughputGFLOPS = 0; return r;
    }
    static AccelResult error(const char* msg, int code = -1) {
        AccelResult r; r.success = false; r.detail = msg;
        r.errorCode = code; r.elapsedMs = 0; r.throughputGFLOPS = 0; return r;
    }
};

// ============================================================================
// Stats
// ============================================================================

struct AMDAccelStats {
    std::atomic<uint64_t> gpuDispatches{0};
    std::atomic<uint64_t> cpuFallbacks{0};
    std::atomic<uint64_t> gpuAllocBytes{0};
    std::atomic<uint64_t> gpuFreeBytes{0};
    std::atomic<uint64_t> gpuCopyH2D{0};   // Host to Device
    std::atomic<uint64_t> gpuCopyD2H{0};   // Device to Host
    std::atomic<uint64_t> gpuComputeMs{0};
    std::atomic<uint64_t> gpuWaitMs{0};
    std::atomic<uint64_t> toggleOnCount{0};
    std::atomic<uint64_t> toggleOffCount{0};
    double peakTFLOPS{0};
    double avgOccupancy{0};
};

// ============================================================================
// Callback types (no std::function in hot paths)
// ============================================================================

typedef void (*GPUToggleCallback)(bool enabled, GPUBackend backend, void* userData);
typedef void (*GPUErrorCallback)(const char* msg, int code, void* userData);
typedef void (*GPUMemoryCallback)(uint64_t used, uint64_t total, void* userData);

// ============================================================================
// AMDGPUAccelerator — Singleton Master Toggle
// ============================================================================

class AMDGPUAccelerator {
public:
    static AMDGPUAccelerator& instance();

    // ===== Lifecycle =====
    AccelResult initialize(GPUBackend preferredBackend = GPUBackend::Auto);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ===== MASTER TOGGLE =====
    AccelResult enableGPU();
    AccelResult disableGPU();
    bool isGPUEnabled() const { return m_gpuEnabled.load(std::memory_order_acquire); }
    AccelResult toggleGPU(); // Flip current state

    // ===== Scope Toggles (per-subsystem) =====
    AccelResult enableScope(AccelScope scope);
    AccelResult disableScope(AccelScope scope);
    bool isScopeEnabled(AccelScope scope) const;
    uint8_t getEnabledScopes() const { return m_enabledScopes.load(std::memory_order_acquire); }

    // ===== Backend Info =====
    GPUBackend getActiveBackend() const { return m_activeBackend; }
    const char* getBackendName() const;
    uint32_t getAMDFeatureFlags() const { return m_amdFeatures; }
    bool hasFeature(AMDFeatureFlag feature) const;
    std::string getGPUName() const { return m_gpuName; }
    uint32_t getVendorId() const { return m_vendorId; }
    uint32_t getDeviceId() const { return m_deviceId; }
    uint32_t getComputeUnits() const { return m_computeUnits; }
    uint64_t getVRAMBytes() const { return m_vramBytes; }

    // ===== Memory Management =====
    AccelResult allocGPU(uint64_t sizeBytes, GPUBuffer& outBuffer);
    AccelResult freeGPU(GPUBuffer& buffer);
    AccelResult copyToGPU(GPUBuffer& dst, const void* hostSrc, uint64_t bytes);
    AccelResult copyFromGPU(void* hostDst, const GPUBuffer& src, uint64_t bytes);
    AccelResult mapBuffer(GPUBuffer& buffer);
    AccelResult unmapBuffer(GPUBuffer& buffer);
    const GPUMemoryPool& getMemoryPool() const { return m_memPool; }

    // ===== Compute Dispatch =====
    AccelResult dispatchMatMul(const GPUBuffer& A, const GPUBuffer& B, GPUBuffer& C,
                                uint32_t M, uint32_t N, uint32_t K, bool fp16 = true);
    AccelResult dispatchQuantize(const GPUBuffer& input, GPUBuffer& output,
                                  uint32_t elements, uint8_t quantType);
    AccelResult dispatchDequantize(const GPUBuffer& input, GPUBuffer& output,
                                    uint32_t elements, uint8_t quantType);
    AccelResult dispatchAttention(const GPUBuffer& Q, const GPUBuffer& K,
                                   const GPUBuffer& V, GPUBuffer& output,
                                   uint32_t heads, uint32_t seqLen, uint32_t headDim);
    AccelResult dispatchRMSNorm(const GPUBuffer& input, const GPUBuffer& weight,
                                 GPUBuffer& output, uint32_t size, float eps = 1e-6f);
    AccelResult dispatchSoftmax(const GPUBuffer& input, GPUBuffer& output,
                                 uint32_t rows, uint32_t cols);
    AccelResult dispatchRoPE(GPUBuffer& qk, uint32_t seqLen, uint32_t headDim,
                              uint32_t posOffset, float theta = 10000.0f);
    AccelResult dispatchGeneric(const char* kernelName, const GPUBuffer* buffers,
                                 uint32_t bufferCount, uint32_t groupX,
                                 uint32_t groupY = 1, uint32_t groupZ = 1);

    // ===== Synchronization =====
    AccelResult syncGPU();    // Wait for all GPU work
    AccelResult flushGPU();   // Flush pending commands

    // ===== Integration Hooks =====
    // These are called by other subsystems to check if GPU path should be used
    bool shouldUseGPU(AccelScope scope) const;
    bool shouldUseGPU(AccelScope scope, uint64_t dataBytes) const; // Size threshold check

    // ===== Callbacks =====
    void setToggleCallback(GPUToggleCallback cb, void* userData);
    void setErrorCallback(GPUErrorCallback cb, void* userData);
    void setMemoryCallback(GPUMemoryCallback cb, void* userData);

    // ===== Stats & JSON =====
    const AMDAccelStats& getStats() const { return m_stats; }
    void resetStats();
    std::string toJson() const;
    std::string memoryToJson() const;
    std::string featuresToJson() const;

private:
    AMDGPUAccelerator();
    ~AMDGPUAccelerator();
    AMDGPUAccelerator(const AMDGPUAccelerator&) = delete;
    AMDGPUAccelerator& operator=(const AMDGPUAccelerator&) = delete;

    // ===== Backend Initialization =====
    AccelResult initDX12();
    AccelResult initVulkan();
    AccelResult initROCm();
    AccelResult initOpenCL();
    AccelResult probeAMDFeatures();

    // ===== Internal =====
    uint32_t m_nextBufferId;

    // ===== State =====
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_gpuEnabled{false};
    std::atomic<uint8_t> m_enabledScopes{0};
    mutable std::mutex m_mutex;

    GPUBackend m_activeBackend;
    std::string m_gpuName;
    uint32_t m_vendorId;
    uint32_t m_deviceId;
    uint32_t m_computeUnits;
    uint64_t m_vramBytes;
    uint32_t m_amdFeatures;      // Bitfield of AMDFeatureFlag

    GPUMemoryPool m_memPool;
    std::vector<GPUBuffer> m_allocatedBuffers;

    // Backend handles (opaque pointers loaded dynamically)
    void* m_dx12Device;
    void* m_dx12Queue;
    void* m_dx12Allocator;
    void* m_vulkanInstance;
    void* m_vulkanDevice;
    void* m_vulkanQueue;
    void* m_rocmHandle;
    void* m_openclContext;
    void* m_openclQueue;

    // Size threshold: only use GPU if data exceeds this (avoid PCIe overhead)
    uint64_t m_gpuMinBytes;

    // Callbacks
    GPUToggleCallback m_toggleCb;
    void* m_toggleData;
    GPUErrorCallback m_errorCb;
    void* m_errorData;
    GPUMemoryCallback m_memoryCb;
    void* m_memoryData;

    AMDAccelStats m_stats;
};

#endif // AMD_GPU_ACCELERATOR_H
