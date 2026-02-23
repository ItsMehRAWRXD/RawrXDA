// ============================================================================
// intel_gpu_accelerator.h — Intel Arc / Meteor Lake GPU Acceleration
// ============================================================================
// Phase 29A: Toggleable Intel GPU acceleration for Arc (Alchemist, Battlemage)
// and Meteor Lake (integrated Xe-LPG).  Mirrors AMDGPUAccelerator pattern:
//   - Singleton, PatchResult-style results, no exceptions
//   - Dynamic runtime loading (Level Zero, oneAPI, DX12, Vulkan)
//   - Function-pointer callbacks (no std::function in hot path)
//   - DXGI vendor-ID 0x8086 detection
//
// Backend priority: Level Zero → DX12 Compute → Vulkan Compute → OpenCL
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef INTEL_GPU_ACCELERATOR_H
#define INTEL_GPU_ACCELERATOR_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

// ============================================================================
// Intel GPU Backend Selection
// ============================================================================

enum class IntelGPUBackend : uint8_t {
    None        = 0,   // CPU only
    LevelZero   = 1,   // oneAPI Level Zero (native Intel path)
    DX12Compute = 2,   // DirectX 12 Compute Shaders
    Vulkan      = 3,   // Vulkan Compute Shaders
    OpenCL      = 4,   // OpenCL (legacy fallback)
    Auto        = 255  // Auto-detect best backend
};

// ============================================================================
// Intel GPU Architecture Classification
// ============================================================================

enum class IntelGPUArch : uint8_t {
    Unknown        = 0,
    Xe_LP          = 1,   // Xe-LP (Tiger Lake / DG1 / Rocket Lake iGPU)
    Xe_HPG         = 2,   // Xe-HPG (Alchemist — Arc A770/A750/A580/A380)
    Xe_HPC         = 3,   // Xe-HPC (Ponte Vecchio — data center)
    Xe_LPG         = 4,   // Xe-LPG (Meteor Lake integrated GPU)
    Xe2_HPG        = 5,   // Xe2-HPG (Battlemage — Arc B580/B570)
    Xe2_LPG        = 6,   // Xe2-LPG (Lunar Lake integrated GPU)
    Xe3_HPG        = 7,   // Xe3-HPG (Celestial — next-gen discrete)
};

// ============================================================================
// Intel GPU Feature Flags
// ============================================================================

enum class IntelFeatureFlag : uint32_t {
    XMX             = 0x0001,  // Xe Matrix Extensions (INT8/BF16/FP16 systolic)
    DP4a            = 0x0002,  // INT8 DP4A dot product acceleration
    DPAS            = 0x0004,  // Dot Product Accumulate Systolic
    BF16            = 0x0008,  // BF16 native support
    FP16            = 0x0010,  // FP16 native support
    FP64            = 0x0020,  // FP64 support (Xe-HPC)
    TF32            = 0x0040,  // TF32 support (Xe-HPC)
    INT2            = 0x0080,  // INT2 sub-byte quantization (Xe2+)
    AsyncCopy       = 0x0100,  // Async DMA copy engine
    CoopMatrix      = 0x0200,  // VK_KHR_cooperative_matrix / joint_matrix
    SubgroupShuffle = 0x0400,  // Subgroup shuffle / ballot
    LSC             = 0x0800,  // Load/Store Cache (Xe-HPG+) — replaces SLM direct
    EUFusion        = 0x1000,  // EU Fusion (two EUs share resources)
    L1CacheControl  = 0x2000,  // Explicit L1 cache control hints
    URB             = 0x4000,  // Unified Return Buffer (shared EU↔SLM)
    RayTracing      = 0x8000,  // Hardware ray tracing units (Xe-HPG+)
};

// ============================================================================
// Intel GPU Acceleration Scope (mirrors AMD pattern)
// ============================================================================

enum class IntelAccelScope : uint8_t {
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
// Intel GPU Memory Pool
// ============================================================================

struct IntelGPUMemoryPool {
    uint64_t totalBytes;
    uint64_t usedBytes;
    uint64_t peakBytes;
    uint64_t allocCount;
    uint64_t freeCount;
    bool unified;           // Unified memory (iGPU or ResizableBAR)
    bool sharedSystemMem;   // GPU can access system RAM directly

    IntelGPUMemoryPool()
        : totalBytes(0), usedBytes(0), peakBytes(0)
        , allocCount(0), freeCount(0)
        , unified(false), sharedSystemMem(false)
    {}

    uint64_t availableBytes() const { return totalBytes - usedBytes; }
    double usagePercent() const { return totalBytes > 0 ? (100.0 * usedBytes / totalBytes) : 0; }
};

// ============================================================================
// Intel GPU Buffer Handle
// ============================================================================

struct IntelGPUBuffer {
    void* devicePtr;
    void* hostPtr;
    uint64_t sizeBytes;
    uint32_t bufferId;
    bool mapped;
    bool usm;               // Unified Shared Memory (Level Zero / oneAPI)

    IntelGPUBuffer()
        : devicePtr(nullptr), hostPtr(nullptr), sizeBytes(0)
        , bufferId(0), mapped(false), usm(false)
    {}
};

// ============================================================================
// Intel Acceleration Result (PatchResult-compatible)
// ============================================================================

struct IntelAccelResult {
    bool success;
    const char* detail;
    int errorCode;
    double elapsedMs;
    double throughputGFLOPS;

    static IntelAccelResult ok(const char* msg) {
        IntelAccelResult r;
        r.success = true; r.detail = msg; r.errorCode = 0;
        r.elapsedMs = 0; r.throughputGFLOPS = 0;
        return r;
    }
    static IntelAccelResult error(const char* msg, int code = -1) {
        IntelAccelResult r;
        r.success = false; r.detail = msg; r.errorCode = code;
        r.elapsedMs = 0; r.throughputGFLOPS = 0;
        return r;
    }
};

// ============================================================================
// Intel GPU Accelerator Statistics
// ============================================================================

struct IntelAccelStats {
    std::atomic<uint64_t> gpuDispatches{0};
    std::atomic<uint64_t> cpuFallbacks{0};
    std::atomic<uint64_t> gpuAllocBytes{0};
    std::atomic<uint64_t> gpuFreeBytes{0};
    std::atomic<uint64_t> gpuCopyH2D{0};
    std::atomic<uint64_t> gpuCopyD2H{0};
    std::atomic<uint64_t> gpuComputeMs{0};
    std::atomic<uint64_t> gpuWaitMs{0};
    std::atomic<uint64_t> xmxDispatches{0};   // XMX/DPAS dispatches
    std::atomic<uint64_t> euScalarFallbacks{0}; // Non-systolic fallbacks
    std::atomic<uint64_t> toggleOnCount{0};
    std::atomic<uint64_t> toggleOffCount{0};
    double peakTFLOPS{0};
    double avgEUOccupancy{0};
};

// ============================================================================
// Callback types (no std::function in hot paths)
// ============================================================================

typedef void (*IntelGPUToggleCallback)(bool enabled, IntelGPUBackend backend, void* userData);
typedef void (*IntelGPUErrorCallback)(const char* msg, int code, void* userData);
typedef void (*IntelGPUMemoryCallback)(uint64_t used, uint64_t total, void* userData);

// ============================================================================
// IntelGPUAccelerator — Singleton Master Toggle
// ============================================================================

class IntelGPUAccelerator {
public:
    static IntelGPUAccelerator& instance();

    // ===== Lifecycle =====
    IntelAccelResult initialize(IntelGPUBackend preferredBackend = IntelGPUBackend::Auto);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ===== MASTER TOGGLE =====
    IntelAccelResult enableGPU();
    IntelAccelResult disableGPU();
    bool isGPUEnabled() const { return m_gpuEnabled.load(std::memory_order_acquire); }
    IntelAccelResult toggleGPU();

    // ===== Scope Toggles (per-subsystem) =====
    IntelAccelResult enableScope(IntelAccelScope scope);
    IntelAccelResult disableScope(IntelAccelScope scope);
    bool isScopeEnabled(IntelAccelScope scope) const;
    uint8_t getEnabledScopes() const { return m_enabledScopes.load(std::memory_order_acquire); }

    // ===== Backend Info =====
    IntelGPUBackend getActiveBackend() const { return m_activeBackend; }
    const char* getBackendName() const;
    IntelGPUArch getArchitecture() const { return m_arch; }
    uint32_t getIntelFeatureFlags() const { return m_intelFeatures; }
    bool hasFeature(IntelFeatureFlag feature) const;
    std::string getGPUName() const { return m_gpuName; }
    uint32_t getVendorId() const { return m_vendorId; }
    uint32_t getDeviceId() const { return m_deviceId; }
    uint32_t getEUCount() const { return m_euCount; }
    uint32_t getSliceCount() const { return m_sliceCount; }
    uint32_t getSubsliceCount() const { return m_subsliceCount; }
    uint64_t getVRAMBytes() const { return m_vramBytes; }

    // ===== Memory Management =====
    IntelAccelResult allocGPU(uint64_t sizeBytes, IntelGPUBuffer& outBuffer);
    IntelAccelResult freeGPU(IntelGPUBuffer& buffer);
    IntelAccelResult copyToGPU(IntelGPUBuffer& dst, const void* hostSrc, uint64_t bytes);
    IntelAccelResult copyFromGPU(void* hostDst, const IntelGPUBuffer& src, uint64_t bytes);
    IntelAccelResult allocUSM(uint64_t sizeBytes, IntelGPUBuffer& outBuffer); // Unified Shared Memory
    IntelAccelResult mapBuffer(IntelGPUBuffer& buffer);
    IntelAccelResult unmapBuffer(IntelGPUBuffer& buffer);
    const IntelGPUMemoryPool& getMemoryPool() const { return m_memPool; }

    // ===== Compute Dispatch =====
    IntelAccelResult dispatchMatMul(const IntelGPUBuffer& A, const IntelGPUBuffer& B,
                                     IntelGPUBuffer& C,
                                     uint32_t M, uint32_t N, uint32_t K, bool fp16 = true);
    IntelAccelResult dispatchXMXMatMul(const IntelGPUBuffer& A, const IntelGPUBuffer& B,
                                        IntelGPUBuffer& C,
                                        uint32_t M, uint32_t N, uint32_t K);
    IntelAccelResult dispatchQuantize(const IntelGPUBuffer& input, IntelGPUBuffer& output,
                                       uint32_t elements, uint8_t quantType);
    IntelAccelResult dispatchDequantize(const IntelGPUBuffer& input, IntelGPUBuffer& output,
                                         uint32_t elements, uint8_t quantType);
    IntelAccelResult dispatchAttention(const IntelGPUBuffer& Q, const IntelGPUBuffer& K,
                                        const IntelGPUBuffer& V, IntelGPUBuffer& output,
                                        uint32_t heads, uint32_t seqLen, uint32_t headDim);
    IntelAccelResult dispatchRMSNorm(const IntelGPUBuffer& input, const IntelGPUBuffer& weight,
                                      IntelGPUBuffer& output, uint32_t size, float eps = 1e-6f);
    IntelAccelResult dispatchSoftmax(const IntelGPUBuffer& input, IntelGPUBuffer& output,
                                      uint32_t rows, uint32_t cols);
    IntelAccelResult dispatchRoPE(IntelGPUBuffer& qk, uint32_t seqLen, uint32_t headDim,
                                   uint32_t posOffset, float theta = 10000.0f);
    IntelAccelResult dispatchGeneric(const char* kernelName, const IntelGPUBuffer* buffers,
                                      uint32_t bufferCount, uint32_t groupX,
                                      uint32_t groupY = 1, uint32_t groupZ = 1);

    // ===== Synchronization =====
    IntelAccelResult syncGPU();
    IntelAccelResult flushGPU();

    // ===== Integration Hooks =====
    bool shouldUseGPU(IntelAccelScope scope) const;
    bool shouldUseGPU(IntelAccelScope scope, uint64_t dataBytes) const;

    // ===== Callbacks =====
    void setToggleCallback(IntelGPUToggleCallback cb, void* userData);
    void setErrorCallback(IntelGPUErrorCallback cb, void* userData);
    void setMemoryCallback(IntelGPUMemoryCallback cb, void* userData);

    // ===== Stats & JSON =====
    const IntelAccelStats& getStats() const { return m_stats; }
    void resetStats();
    std::string toJson() const;
    std::string memoryToJson() const;
    std::string featuresToJson() const;

private:
    IntelGPUAccelerator();
    ~IntelGPUAccelerator();
    IntelGPUAccelerator(const IntelGPUAccelerator&) = delete;
    IntelGPUAccelerator& operator=(const IntelGPUAccelerator&) = delete;

    // ===== Backend Initialization =====
    IntelAccelResult initLevelZero();
    IntelAccelResult initDX12();
    IntelAccelResult initVulkan();
    IntelAccelResult initOpenCL();
    IntelAccelResult probeIntelFeatures();
    IntelGPUArch classifyArchitecture(uint32_t deviceId) const;

    // ===== Internal =====
    uint32_t m_nextBufferId;

    // ===== State =====
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_gpuEnabled{false};
    std::atomic<uint8_t> m_enabledScopes{0};
    mutable std::mutex m_mutex;

    IntelGPUBackend m_activeBackend;
    IntelGPUArch m_arch;
    std::string m_gpuName;
    uint32_t m_vendorId;
    uint32_t m_deviceId;
    uint32_t m_euCount;          // Execution Units
    uint32_t m_sliceCount;       // GPU slices
    uint32_t m_subsliceCount;    // Subslices per slice
    uint64_t m_vramBytes;
    uint32_t m_intelFeatures;    // Bitfield of IntelFeatureFlag

    IntelGPUMemoryPool m_memPool;
    std::vector<IntelGPUBuffer> m_allocatedBuffers;

    // Backend handles (opaque pointers loaded dynamically)
    void* m_zeDriver;           // Level Zero ze_driver_handle_t
    void* m_zeDevice;           // Level Zero ze_device_handle_t
    void* m_zeContext;          // Level Zero ze_context_handle_t
    void* m_zeCmdQueue;         // Level Zero ze_command_queue_handle_t
    void* m_zeCmdList;          // Level Zero ze_command_list_handle_t
    void* m_dx12Device;
    void* m_dx12Queue;
    void* m_vulkanInstance;
    void* m_vulkanDevice;
    void* m_vulkanQueue;
    void* m_openclContext;
    void* m_openclQueue;

    // Size threshold: only use GPU if data exceeds this
    uint64_t m_gpuMinBytes;

    // Callbacks
    IntelGPUToggleCallback m_toggleCb;
    void* m_toggleData;
    IntelGPUErrorCallback m_errorCb;
    void* m_errorData;
    IntelGPUMemoryCallback m_memoryCb;
    void* m_memoryData;

    IntelAccelStats m_stats;
};

#endif // INTEL_GPU_ACCELERATOR_H
