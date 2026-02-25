// ============================================================================
// gpu_kernel_autotuner.h — GPU Kernel Auto-Tuner for 120B-800B Models
// ============================================================================
// Dynamically tunes GPU dispatch parameters (workgroup size, wave occupancy,
// memory tiling, cache strategies) for massive model inference on AMD/ATI GPUs.
// Integrates with GPUBackendBridge (DX12), Vulkan, and ROCm/OpenCL backends.
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef GPU_KERNEL_AUTOTUNER_H
#define GPU_KERNEL_AUTOTUNER_H

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
// GPU Compute Enums
// ============================================================================

enum class GPUVendorClass : uint8_t {
    Unknown      = 0,
    AMD_RDNA1    = 1,
    AMD_RDNA2    = 2,
    AMD_RDNA3    = 3,
    AMD_CDNA1    = 4,
    AMD_CDNA2    = 5,
    AMD_GCN5     = 6,
    NVIDIA_Ampere = 10,
    NVIDIA_Ada    = 11,
    Intel_Xe      = 20,
    Intel_Xe_HPG  = 21,  // Arc A-series (Alchemist, Battlemage)
    Intel_Xe_LPG  = 22,  // Meteor Lake / Lunar Lake iGPU
    Intel_Xe_HPC  = 23,  // Ponte Vecchio / Data Center
    Qualcomm_Adreno = 30, // Snapdragon X Elite / X Plus
    ARM64_Generic   = 31, // Generic ARM64 GPU (Ampere Altra, etc.)
    Cerebras_WSE2   = 40, // Cerebras Wafer-Scale Engine 2 (850k cores)
    Cerebras_WSE3   = 41, // Cerebras Wafer-Scale Engine 3 (900k cores)
    CPU_Fallback  = 99
};

enum class KernelType : uint8_t {
    MatMul_FP16       = 0,
    MatMul_INT8       = 1,
    MatMul_INT4       = 2,
    Attention_FlashV2 = 3,
    Attention_Paged   = 4,
    Softmax           = 5,
    RMSNorm           = 6,
    RoPE              = 7,
    GELU              = 8,
    SiLU              = 9,
    Quantize_Q4K      = 10,
    Dequantize_Q4K    = 11,
    Quantize_Q8K      = 12,
    Dequantize_Q8K    = 13,
    KVCacheUpdate     = 14,
    TokenEmbed        = 15,
    AllReduce         = 16,
    Custom            = 63
};

enum class TuneStrategy : uint8_t {
    Exhaustive    = 0,   // Try all combos (slow but optimal)
    Heuristic     = 1,   // Vendor-specific heuristics first
    AdaptiveScan  = 2,   // Start with heuristic, explore neighbors
    CacheLookup   = 3,   // Only use cached results
    ModelSpecific = 4    // Use pre-tuned configs for known models
};

enum class MemoryTilingMode : uint8_t {
    None     = 0,
    Linear   = 1,
    Tiled2D  = 2,
    Tiled3D  = 3,
    Swizzled = 4,
    AMDDelta = 5   // AMD-specific delta color compression
};

// ============================================================================
// Dispatch Configuration
// ============================================================================

struct DispatchConfig {
    uint32_t workgroupSizeX;
    uint32_t workgroupSizeY;
    uint32_t workgroupSizeZ;
    uint32_t tilesM;          // Matrix rows per tile
    uint32_t tilesN;          // Matrix cols per tile
    uint32_t tilesK;          // Reduction dimension per tile
    uint32_t wavefrontSize;   // 32 (NVIDIA) or 64 (AMD)
    uint32_t occupancyTarget; // Target wave occupancy percentage
    uint32_t sharedMemBytes;  // Shared/LDS memory per workgroup
    MemoryTilingMode tilingMode;
    bool useAsyncCopy;        // Async memory copy (AMD DMA engine)
    bool useCooperativeGroups;
    bool prefetchToLDS;       // AMD LDS prefetch
    bool usePackedMath;       // FP16x2 packed ops

    DispatchConfig()
        : workgroupSizeX(256), workgroupSizeY(1), workgroupSizeZ(1)
        , tilesM(128), tilesN(128), tilesK(32)
        , wavefrontSize(64), occupancyTarget(75), sharedMemBytes(32768)
        , tilingMode(MemoryTilingMode::Tiled2D)
        , useAsyncCopy(false), useCooperativeGroups(false)
        , prefetchToLDS(true), usePackedMath(true)
    {}

    std::string toKey() const;
};

// ============================================================================
// Benchmark Result
// ============================================================================

struct KernelBenchmark {
    DispatchConfig config;
    double elapsedMs;
    double throughputGFLOPS;
    double memBandwidthGBs;
    uint32_t wavesPerCU;
    uint32_t ldsUsageBytes;
    uint32_t vgprCount;
    uint32_t sgprCount;
    bool valid;

    KernelBenchmark()
        : elapsedMs(0), throughputGFLOPS(0), memBandwidthGBs(0)
        , wavesPerCU(0), ldsUsageBytes(0), vgprCount(0), sgprCount(0)
        , valid(false)
    {}
};

// ============================================================================
// GPU Hardware Profile
// ============================================================================

struct GPUHardwareProfile {
    GPUVendorClass vendorClass;
    std::string deviceName;
    uint32_t vendorId;
    uint32_t deviceId;
    uint32_t computeUnits;        // CUs for AMD, SMs for NVIDIA
    uint32_t wavefrontSize;       // 64 for AMD, 32 for NVIDIA
    uint32_t maxWavesPerCU;
    uint32_t ldsPerCU;            // Local Data Share bytes per CU
    uint32_t vgprsPerCU;
    uint32_t sgprsPerCU;
    uint64_t vramBytes;
    uint64_t vramBandwidthBps;    // Bytes per second
    uint32_t clockMHz;
    uint32_t memClockMHz;
    uint32_t memBusWidth;         // bits
    bool hasMatrixCores;          // WMMA for AMD, Tensor Cores for NVIDIA
    bool hasDPAcceleration;       // Packed FP16/BF16/INT8 dot product
    bool hasAsyncCompute;
    bool hasInfinityCache;        // RDNA2/3 Infinity Cache

    GPUHardwareProfile()
        : vendorClass(GPUVendorClass::Unknown), vendorId(0), deviceId(0)
        , computeUnits(0), wavefrontSize(64), maxWavesPerCU(32)
        , ldsPerCU(65536), vgprsPerCU(65536), sgprsPerCU(16384)
        , vramBytes(0), vramBandwidthBps(0), clockMHz(0)
        , memClockMHz(0), memBusWidth(256)
        , hasMatrixCores(false), hasDPAcceleration(false)
        , hasAsyncCompute(false), hasInfinityCache(false)
    {}

    double peakTFLOPS_FP16() const;
    double peakTFLOPS_INT8() const;
};

// ============================================================================
// Tune Cache Entry
// ============================================================================

struct TuneCacheEntry {
    KernelType kernelType;
    uint32_t matrixM;
    uint32_t matrixN;
    uint32_t matrixK;
    DispatchConfig bestConfig;
    double bestThroughput;
    uint32_t samplesTested;
    uint64_t timestamp;
};

// ============================================================================
// Autotuner Result
// ============================================================================

struct AutotuneResult {
    bool success;
    const char* detail;
    int errorCode;
    DispatchConfig bestConfig;
    double bestThroughput;
    uint32_t configsTested;

    static AutotuneResult ok(const char* msg) {
        AutotuneResult r; r.success = true; r.detail = msg; r.errorCode = 0;
        r.bestThroughput = 0; r.configsTested = 0; return r;
    }
    static AutotuneResult error(const char* msg, int code = -1) {
        AutotuneResult r; r.success = false; r.detail = msg; r.errorCode = code;
        r.bestThroughput = 0; r.configsTested = 0; return r;
    }
};

// ============================================================================
// Statistics
// ============================================================================

struct AutotunerStats {
    std::atomic<uint64_t> totalTuneRuns{0};
    std::atomic<uint64_t> cacheHits{0};
    std::atomic<uint64_t> cacheMisses{0};
    std::atomic<uint64_t> benchmarkRuns{0};
    std::atomic<uint64_t> configsExplored{0};
    std::atomic<uint64_t> improvementsFound{0};
    std::atomic<uint64_t> tuneTimeMs{0};
    double bestOverallTFLOPS{0};
};

// ============================================================================
// GPUKernelAutoTuner — Singleton
// ============================================================================

class GPUKernelAutoTuner {
public:
    static GPUKernelAutoTuner& instance();

    // ----- Lifecycle -----
    AutotuneResult initialize();
    void shutdown();
    bool isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ----- GPU Discovery -----
    AutotuneResult detectGPU();
    const GPUHardwareProfile& getGPUProfile() const { return m_gpu; }
    GPUVendorClass classifyGPU(uint32_t vendorId, uint32_t deviceId) const;

    // ----- Tuning -----
    AutotuneResult tuneKernel(KernelType type, uint32_t M, uint32_t N, uint32_t K,
                               TuneStrategy strategy = TuneStrategy::AdaptiveScan);

    AutotuneResult tuneForModelLayer(const char* layerName, uint32_t hiddenDim,
                                      uint32_t headCount, uint32_t kvHeadCount);

    AutotuneResult tuneAllKernels(TuneStrategy strategy = TuneStrategy::Heuristic);

    // ----- Config Lookup -----
    DispatchConfig getConfig(KernelType type, uint32_t M, uint32_t N, uint32_t K) const;
    DispatchConfig getDefaultConfig(KernelType type) const;

    // ----- AMD-Specific Tuning -----
    DispatchConfig tuneAMD_RDNA(KernelType type, uint32_t M, uint32_t N, uint32_t K) const;
    DispatchConfig tuneAMD_CDNA(KernelType type, uint32_t M, uint32_t N, uint32_t K) const;
    uint32_t computeOptimalLDSUsage(uint32_t tilesM, uint32_t tilesN, uint32_t elemSize) const;
    uint32_t computeOptimalOccupancy(uint32_t vgprCount, uint32_t ldsBytes) const;

    // ----- Cache Management -----
    bool loadTuneCache(const char* path);
    bool saveTuneCache(const char* path) const;
    void clearTuneCache();
    uint32_t getCacheSize() const;

    // ----- Statistics & Diagnostics -----
    const AutotunerStats& getStats() const { return m_stats; }
    void resetStats();
    std::string toJson() const;
    std::string gpuProfileToJson() const;
    std::string tuneCacheToJson() const;

private:
    GPUKernelAutoTuner();
    ~GPUKernelAutoTuner();
    GPUKernelAutoTuner(const GPUKernelAutoTuner&) = delete;
    GPUKernelAutoTuner& operator=(const GPUKernelAutoTuner&) = delete;

    // ----- Benchmarking -----
    KernelBenchmark benchmarkConfig(KernelType type, uint32_t M, uint32_t N, uint32_t K,
                                     const DispatchConfig& config);
    std::vector<DispatchConfig> generateCandidates(KernelType type, uint32_t M, uint32_t N, uint32_t K,
                                                    TuneStrategy strategy) const;
    std::vector<DispatchConfig> generateNeighbors(const DispatchConfig& center) const;

    // ----- Cache Key -----
    std::string makeCacheKey(KernelType type, uint32_t M, uint32_t N, uint32_t K) const;

    // ----- Members -----
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;

    GPUHardwareProfile m_gpu;
    std::map<std::string, TuneCacheEntry> m_tuneCache;
    AutotunerStats m_stats;
};

#endif // GPU_KERNEL_AUTOTUNER_H
