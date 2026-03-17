// ============================================================================
// arm64_gpu_accelerator.h — ARM64 Windows / Qualcomm Snapdragon X Elite
// ============================================================================
// Phase 29B: Toggleable Adreno GPU acceleration for ARM64 Windows devices.
// Targets Qualcomm Snapdragon X Elite (Oryon CPU + Adreno X1-85 GPU).
//
// Mirrors AMDGPUAccelerator pattern:
//   - Singleton, PatchResult-style results, no exceptions
//   - Dynamic runtime loading (DX12, Vulkan, OpenCL for Adreno)
//   - Function-pointer callbacks (no std::function in hot path)
//   - DXGI vendor-ID 0x5143 (Qualcomm) detection
//
// ARM64-specific:
//   - SVE / SVE2 NEON extensions for CPU fallback math
//   - Adreno GPU via DX12 feature level 12.1 or Vulkan 1.3
//   - Qualcomm AI Engine / Hexagon NPU offload for quantized inference
//   - Power-aware scheduling (big.LITTLE-style Oryon cores)
//
// Backend priority: DX12 Compute → Vulkan Compute → OpenCL → CPU (SVE2)
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef ARM64_GPU_ACCELERATOR_H
#define ARM64_GPU_ACCELERATOR_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <atomic>
#include <mutex>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// ============================================================================
// ARM64 GPU Backend Selection
// ============================================================================

enum class ARM64GPUBackend : uint8_t {
    None        = 0,   // CPU only (SVE2/NEON)
    DX12Compute = 1,   // DirectX 12 Compute Shaders (Adreno driver)
    Vulkan      = 2,   // Vulkan 1.3 Compute (Adreno Vulkan driver)
    OpenCL      = 3,   // OpenCL (Adreno OpenCL driver)
    HexagonNPU  = 4,   // Qualcomm Hexagon DSP / AI Engine NPU
    Auto        = 255
};

// ============================================================================
// ARM64 SoC Classification
// ============================================================================

enum class ARM64SoCType : uint8_t {
    Unknown              = 0,
    SnapdragonXElite     = 1,   // X1E-80-100 (Oryon 12-core + Adreno X1-85)
    SnapdragonXPlus      = 2,   // X1P-64-100 (Oryon 10-core + Adreno X1-45)
    SnapdragonXPlus8Core = 3,   // X1P-42-100 (Oryon 8-core + Adreno X1-25)
    Snapdragon8cxGen3    = 4,   // SC8280XP (legacy WoA)
    Ampere               = 10,  // Ampere Altra (server ARM64, no GPU)
    AppleM               = 20,  // Apple Silicon (macOS → not targeted, reference only)
    NvidiaGrace          = 30,  // NVIDIA Grace (server ARM64 + GH200 GPU)
    GenericARM64         = 99   // Unknown ARM64 platform with DX12 GPU
};

// ============================================================================
// ARM64 Feature Flags
// ============================================================================

enum class ARM64FeatureFlag : uint32_t {
    // CPU features
    NEON            = 0x00000001,  // ARM NEON SIMD (mandatory on AArch64)
    SVE             = 0x00000002,  // Scalable Vector Extension
    SVE2            = 0x00000004,  // SVE2 (Snapdragon X Elite supports 128-bit SVE2)
    DotProd         = 0x00000008,  // SDOT/UDOT instructions (INT8 dot product)
    FP16Arith       = 0x00000010,  // FP16 arithmetic (FMLAL, FMLSL)
    BF16            = 0x00000020,  // BFloat16 (BFMMLA, BFMLAL)
    I8MM            = 0x00000040,  // INT8 matrix multiply (SMMLA, UMMLA, USMMLA)
    SME             = 0x00000080,  // Scalable Matrix Extension (future)
    LSE             = 0x00000100,  // Large System Extensions (atomics)
    CRC32           = 0x00000200,  // CRC32 hardware instruction
    AES             = 0x00000400,  // AES hardware acceleration
    SHA256          = 0x00000800,  // SHA-256 hardware acceleration

    // GPU features (Adreno X1-85)
    AdrenoGPU       = 0x00010000,  // Adreno GPU present
    AdrenoFP16      = 0x00020000,  // Adreno FP16 packed compute
    AdrenoBF16      = 0x00040000,  // Adreno BF16 support
    AdrenoINT8      = 0x00080000,  // Adreno INT8 acceleration
    AdrenoINT4      = 0x00100000,  // Adreno INT4 sub-byte quantized compute
    AdrenoTensorCore= 0x00200000,  // Adreno tensor processing unit
    AdrenoRayTrace  = 0x00400000,  // Adreno hardware ray tracing

    // NPU features (Hexagon)
    HexagonNPU      = 0x01000000,  // Qualcomm Hexagon NPU present
    HexagonINT8     = 0x02000000,  // Hexagon INT8 inference
    HexagonINT4     = 0x04000000,  // Hexagon INT4 inference
    HexagonFP16     = 0x08000000,  // Hexagon FP16 inference
    NPU45TOPS       = 0x10000000,  // ≥45 TOPS NPU (Snapdragon X Elite)
};

// ============================================================================
// ARM64 Acceleration Scope (mirrors AMD pattern)
// ============================================================================

enum class ARM64AccelScope : uint8_t {
    Inference        = 0x01,
    Quantization     = 0x02,
    ModelSurgery     = 0x04,
    SwarmCompute     = 0x08,
    KVCache          = 0x10,
    Embedding        = 0x20,
    NPUOffload       = 0x40,  // Hexagon NPU for quantized layers
    All              = 0xFF
};

// ============================================================================
// ARM64 GPU Memory Pool
// ============================================================================

struct ARM64GPUMemoryPool {
    uint64_t totalBytes;
    uint64_t usedBytes;
    uint64_t peakBytes;
    uint64_t allocCount;
    uint64_t freeCount;
    bool unified;           // Adreno uses unified memory architecture
    bool isLPDDR;           // LPDDR5x (Snapdragon X Elite uses 8533MHz LPDDR5x)

    ARM64GPUMemoryPool()
        : totalBytes(0), usedBytes(0), peakBytes(0)
        , allocCount(0), freeCount(0)
        , unified(true), isLPDDR(true)
    {}

    uint64_t availableBytes() const { return totalBytes - usedBytes; }
    double usagePercent() const { return totalBytes > 0 ? (100.0 * usedBytes / totalBytes) : 0; }
};

// ============================================================================
// ARM64 GPU Buffer Handle
// ============================================================================

struct ARM64GPUBuffer {
    void* devicePtr;
    void* hostPtr;
    uint64_t sizeBytes;
    uint32_t bufferId;
    bool mapped;
    bool unified;           // Unified memory — same physical address on CPU/GPU

    ARM64GPUBuffer()
        : devicePtr(nullptr), hostPtr(nullptr), sizeBytes(0)
        , bufferId(0), mapped(false), unified(true)
    {}
};

// ============================================================================
// ARM64 Acceleration Result (PatchResult-compatible)
// ============================================================================

struct ARM64AccelResult {
    bool success;
    const char* detail;
    int errorCode;
    double elapsedMs;
    double throughputGFLOPS;

    static ARM64AccelResult ok(const char* msg) {
        ARM64AccelResult r;
        r.success = true; r.detail = msg; r.errorCode = 0;
        r.elapsedMs = 0; r.throughputGFLOPS = 0;
        return r;
    }
    static ARM64AccelResult error(const char* msg, int code = -1) {
        ARM64AccelResult r;
        r.success = false; r.detail = msg; r.errorCode = code;
        r.elapsedMs = 0; r.throughputGFLOPS = 0;
        return r;
    }
};

// ============================================================================
// ARM64 Accelerator Statistics
// ============================================================================

struct ARM64AccelStats {
    std::atomic<uint64_t> gpuDispatches{0};
    std::atomic<uint64_t> npuDispatches{0};   // Hexagon NPU dispatches
    std::atomic<uint64_t> cpuFallbacks{0};
    std::atomic<uint64_t> gpuAllocBytes{0};
    std::atomic<uint64_t> gpuFreeBytes{0};
    std::atomic<uint64_t> gpuCopyH2D{0};
    std::atomic<uint64_t> gpuCopyD2H{0};
    std::atomic<uint64_t> gpuComputeMs{0};
    std::atomic<uint64_t> npuComputeMs{0};
    std::atomic<uint64_t> neonFallbacks{0};   // NEON/SVE2 CPU path
    std::atomic<uint64_t> toggleOnCount{0};
    std::atomic<uint64_t> toggleOffCount{0};
    double peakTFLOPS{0};
    double peakNPU_TOPS{0};                    // NPU TOPS throughput
    double thermalThrottlePercent{0};          // Thermal headroom tracking
};

// ============================================================================
// Callback types (no std::function in hot paths)
// ============================================================================

typedef void (*ARM64GPUToggleCallback)(bool enabled, ARM64GPUBackend backend, void* userData);
typedef void (*ARM64GPUErrorCallback)(const char* msg, int code, void* userData);
typedef void (*ARM64GPUMemoryCallback)(uint64_t used, uint64_t total, void* userData);

// ============================================================================
// ARM64GPUAccelerator — Singleton Master Toggle
// ============================================================================

class ARM64GPUAccelerator {
public:
    static ARM64GPUAccelerator& instance();

    // ===== Lifecycle =====
    ARM64AccelResult initialize(ARM64GPUBackend preferredBackend = ARM64GPUBackend::Auto);
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ===== MASTER TOGGLE =====
    ARM64AccelResult enableGPU();
    ARM64AccelResult disableGPU();
    bool isGPUEnabled() const { return m_gpuEnabled.load(std::memory_order_acquire); }
    ARM64AccelResult toggleGPU();

    // ===== NPU Toggle =====
    ARM64AccelResult enableNPU();
    ARM64AccelResult disableNPU();
    bool isNPUEnabled() const { return m_npuEnabled.load(std::memory_order_acquire); }

    // ===== Scope Toggles =====
    ARM64AccelResult enableScope(ARM64AccelScope scope);
    ARM64AccelResult disableScope(ARM64AccelScope scope);
    bool isScopeEnabled(ARM64AccelScope scope) const;
    uint8_t getEnabledScopes() const { return m_enabledScopes.load(std::memory_order_acquire); }

    // ===== Platform Info =====
    ARM64GPUBackend getActiveBackend() const { return m_activeBackend; }
    const char* getBackendName() const;
    ARM64SoCType getSoCType() const { return m_socType; }
    uint32_t getARM64FeatureFlags() const { return m_arm64Features; }
    bool hasFeature(ARM64FeatureFlag feature) const;
    std::string getGPUName() const { return m_gpuName; }
    std::string getSoCName() const { return m_socName; }
    uint32_t getVendorId() const { return m_vendorId; }
    uint32_t getDeviceId() const { return m_deviceId; }
    uint32_t getCPUCoreCount() const { return m_cpuCoreCount; }
    uint32_t getGPUShaderCores() const { return m_gpuShaderCores; }
    uint32_t getNPUCores() const { return m_npuCores; }
    uint64_t getSystemRAMBytes() const { return m_systemRAMBytes; }

    // ===== Memory Management =====
    ARM64AccelResult allocGPU(uint64_t sizeBytes, ARM64GPUBuffer& outBuffer);
    ARM64AccelResult freeGPU(ARM64GPUBuffer& buffer);
    ARM64AccelResult copyToGPU(ARM64GPUBuffer& dst, const void* hostSrc, uint64_t bytes);
    ARM64AccelResult copyFromGPU(void* hostDst, const ARM64GPUBuffer& src, uint64_t bytes);
    ARM64AccelResult mapBuffer(ARM64GPUBuffer& buffer);
    ARM64AccelResult unmapBuffer(ARM64GPUBuffer& buffer);
    const ARM64GPUMemoryPool& getMemoryPool() const { return m_memPool; }

    // ===== Compute Dispatch =====
    ARM64AccelResult dispatchMatMul(const ARM64GPUBuffer& A, const ARM64GPUBuffer& B,
                                     ARM64GPUBuffer& C,
                                     uint32_t M, uint32_t N, uint32_t K, bool fp16 = true);
    ARM64AccelResult dispatchQuantize(const ARM64GPUBuffer& input, ARM64GPUBuffer& output,
                                       uint32_t elements, uint8_t quantType);
    ARM64AccelResult dispatchDequantize(const ARM64GPUBuffer& input, ARM64GPUBuffer& output,
                                         uint32_t elements, uint8_t quantType);
    ARM64AccelResult dispatchAttention(const ARM64GPUBuffer& Q, const ARM64GPUBuffer& K,
                                        const ARM64GPUBuffer& V, ARM64GPUBuffer& output,
                                        uint32_t heads, uint32_t seqLen, uint32_t headDim);
    ARM64AccelResult dispatchRMSNorm(const ARM64GPUBuffer& input, const ARM64GPUBuffer& weight,
                                      ARM64GPUBuffer& output, uint32_t size, float eps = 1e-6f);
    ARM64AccelResult dispatchSoftmax(const ARM64GPUBuffer& input, ARM64GPUBuffer& output,
                                      uint32_t rows, uint32_t cols);
    ARM64AccelResult dispatchRoPE(ARM64GPUBuffer& qk, uint32_t seqLen, uint32_t headDim,
                                   uint32_t posOffset, float theta = 10000.0f);
    ARM64AccelResult dispatchGeneric(const char* kernelName, const ARM64GPUBuffer* buffers,
                                      uint32_t bufferCount, uint32_t groupX,
                                      uint32_t groupY = 1, uint32_t groupZ = 1);

    // ===== NPU Dispatch (Hexagon DSP) =====
    ARM64AccelResult dispatchNPUInference(const ARM64GPUBuffer& weights,
                                           const ARM64GPUBuffer& input,
                                           ARM64GPUBuffer& output,
                                           uint32_t batchSize, uint8_t quantType);

    // ===== Synchronization =====
    ARM64AccelResult syncGPU();
    ARM64AccelResult flushGPU();

    // ===== Integration Hooks =====
    bool shouldUseGPU(ARM64AccelScope scope) const;
    bool shouldUseGPU(ARM64AccelScope scope, uint64_t dataBytes) const;
    bool shouldUseNPU(uint64_t dataBytes, uint8_t quantType) const;

    // ===== Power Management =====
    ARM64AccelResult setPowerProfile(uint8_t profile); // 0=balanced, 1=perf, 2=battery
    double getThermalHeadroom() const;                 // 0.0 = throttled, 1.0 = cool

    // ===== Callbacks =====
    void setToggleCallback(ARM64GPUToggleCallback cb, void* userData);
    void setErrorCallback(ARM64GPUErrorCallback cb, void* userData);
    void setMemoryCallback(ARM64GPUMemoryCallback cb, void* userData);

    // ===== Stats & JSON =====
    const ARM64AccelStats& getStats() const { return m_stats; }
    void resetStats();
    std::string toJson() const;
    std::string memoryToJson() const;
    std::string featuresToJson() const;

private:
    ARM64GPUAccelerator();
    ~ARM64GPUAccelerator();
    ARM64GPUAccelerator(const ARM64GPUAccelerator&) = delete;
    ARM64GPUAccelerator& operator=(const ARM64GPUAccelerator&) = delete;

    // ===== Detection & Init =====
    ARM64AccelResult detectPlatform();
    ARM64AccelResult initDX12();
    ARM64AccelResult initVulkan();
    ARM64AccelResult initOpenCL();
    ARM64AccelResult initHexagonNPU();
    ARM64AccelResult probeCPUFeatures();
    ARM64AccelResult probeGPUFeatures();
    ARM64SoCType classifySoC(uint32_t vendorId, uint32_t deviceId);

    // ===== Internal =====
    uint32_t m_nextBufferId;

    // ===== State =====
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_gpuEnabled{false};
    std::atomic<bool> m_npuEnabled{false};
    std::atomic<uint8_t> m_enabledScopes{0};
    mutable std::mutex m_mutex;

    ARM64GPUBackend m_activeBackend;
    ARM64SoCType m_socType;
    std::string m_gpuName;
    std::string m_socName;
    uint32_t m_vendorId;
    uint32_t m_deviceId;
    uint32_t m_cpuCoreCount;
    uint32_t m_gpuShaderCores;
    uint32_t m_npuCores;
    uint64_t m_systemRAMBytes;
    uint32_t m_arm64Features;    // Bitfield of ARM64FeatureFlag

    ARM64GPUMemoryPool m_memPool;
    std::vector<ARM64GPUBuffer> m_allocatedBuffers;

    // Backend handles
    void* m_dx12Device;
    void* m_dx12Queue;
    void* m_vulkanInstance;
    void* m_vulkanDevice;
    void* m_vulkanQueue;
    void* m_openclContext;
    void* m_openclQueue;
    void* m_hexagonHandle;      // Hexagon NPU runtime handle

    uint64_t m_gpuMinBytes;
    uint8_t m_powerProfile;     // 0=balanced, 1=perf, 2=battery

    // Callbacks
    ARM64GPUToggleCallback m_toggleCb;
    void* m_toggleData;
    ARM64GPUErrorCallback m_errorCb;
    void* m_errorData;
    ARM64GPUMemoryCallback m_memoryCb;
    void* m_memoryData;

    ARM64AccelStats m_stats;
};

#endif // ARM64_GPU_ACCELERATOR_H
