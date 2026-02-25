// ============================================================================
// gpu_kernel_autotuner.cpp — GPU Kernel Auto-Tuner Implementation
// ============================================================================
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "gpu_kernel_autotuner.h"

#include "logging/logger.h"
static Logger s_logger("gpu_kernel_autotuner");

#include <iostream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <cstring>
#include <cmath>
#include <fstream>

// ============================================================================
// DispatchConfig
// ============================================================================

std::string DispatchConfig::toKey() const {
    std::ostringstream oss;
    oss << workgroupSizeX << "x" << workgroupSizeY << "x" << workgroupSizeZ
        << "_t" << tilesM << "x" << tilesN << "x" << tilesK
        << "_w" << wavefrontSize << "_o" << occupancyTarget
        << "_s" << sharedMemBytes << "_m" << static_cast<int>(tilingMode);
    return oss.str();
}

// ============================================================================
// GPUHardwareProfile
// ============================================================================

double GPUHardwareProfile::peakTFLOPS_FP16() const {
    // FP16 ops per clock per CU: wavefrontSize * 2 (packed FP16)
    double opsPerClock = (double)computeUnits * wavefrontSize * 2.0;
    return (opsPerClock * clockMHz * 1e-6) / 1e12;
}

double GPUHardwareProfile::peakTFLOPS_INT8() const {
    // INT8 typically 2x FP16 with DP acceleration
    return peakTFLOPS_FP16() * (hasDPAcceleration ? 2.0 : 1.0);
}

// ============================================================================
// Singleton
// ============================================================================

GPUKernelAutoTuner& GPUKernelAutoTuner::instance() {
    static GPUKernelAutoTuner s_instance;
    return s_instance;
}

GPUKernelAutoTuner::GPUKernelAutoTuner() {}
GPUKernelAutoTuner::~GPUKernelAutoTuner() { shutdown(); }

// ============================================================================
// Lifecycle
// ============================================================================

AutotuneResult GPUKernelAutoTuner::initialize() {
    if (m_initialized.load()) return AutotuneResult::ok("Already initialized");

    auto r = detectGPU();
    if (!r.success) return r;

    // Load cached tuning data
    loadTuneCache("rawrxd_tune_cache.json");

    m_initialized.store(true, std::memory_order_release);

    s_logger.info("[AUTOTUNER] Initialized for ");

    return AutotuneResult::ok("GPU kernel auto-tuner initialized");
}

void GPUKernelAutoTuner::shutdown() {
    if (!m_initialized.load()) return;
    saveTuneCache("rawrxd_tune_cache.json");
    m_initialized.store(false);
    s_logger.info("[AUTOTUNER] Shutdown.\n");
}

// ============================================================================
// GPU Detection
// ============================================================================

AutotuneResult GPUKernelAutoTuner::detectGPU() {
    // Use DXGI to enumerate adapters
    // Fallback: parse registry or use Vulkan instance query

    // DXGI adapter enumeration via COM (IDXGIFactory1)

    // DXGI factory approach (simplified — in production would use IDXGIFactory6)
    HMODULE hDXGI = LoadLibraryA("dxgi.dll");
    if (!hDXGI) {
        // Fallback: CPU only
        m_gpu.vendorClass = GPUVendorClass::CPU_Fallback;
        m_gpu.deviceName = "CPU Fallback";
        m_gpu.wavefrontSize = 1;
        m_gpu.computeUnits = 1;
        return AutotuneResult::ok("No GPU detected, CPU fallback");
    }

    typedef HRESULT(WINAPI* PFN_CreateDXGIFactory1)(REFIID, void**);
    auto fnCreate = (PFN_CreateDXGIFactory1)GetProcAddress(hDXGI, "CreateDXGIFactory1");
    if (!fnCreate) {
        FreeLibrary(hDXGI);
        m_gpu.vendorClass = GPUVendorClass::CPU_Fallback;
        m_gpu.deviceName = "CPU Fallback (DXGI unavailable)";
        return AutotuneResult::ok("DXGI not available, CPU fallback");
    }

    // We'll use the COM interface dynamically
    // IID_IDXGIFactory1 = {770aae78-f26f-4dba-a829-253c83d1b387}
    static const GUID IID_IDXGIFactory1_local =
        { 0x770aae78, 0xf26f, 0x4dba, { 0xa8, 0x29, 0x25, 0x3c, 0x83, 0xd1, 0xb3, 0x87 } };

    void* pFactory = nullptr;
    HRESULT hr = fnCreate(IID_IDXGIFactory1_local, &pFactory);
    if (FAILED(hr) || !pFactory) {
        FreeLibrary(hDXGI);
        m_gpu.vendorClass = GPUVendorClass::CPU_Fallback;
        m_gpu.deviceName = "CPU Fallback";
        return AutotuneResult::ok("DXGI factory creation failed, CPU fallback");
    }

    // Use vtable to call EnumAdapters (method index 7 in IDXGIFactory1)
    // IDXGIFactory1 vtable:
    //  0: QueryInterface, 1: AddRef, 2: Release,
    //  3: SetPrivateData, 4: SetPrivateDataInterface, 5: GetPrivateData,
    //  6: GetParent, 7: EnumAdapters, 8: MakeWindowAssociation,
    //  9: GetWindowAssociation, 10: CreateSwapChain, 11: CreateSoftwareAdapter,
    //  12: EnumAdapters1, 13: IsCurrent

    struct IDXGIFactory1_vtbl {
        void* methods[14];
    };
    struct IDXGIFactory1_obj {
        IDXGIFactory1_vtbl* vtbl;
    };

    auto* factoryObj = static_cast<IDXGIFactory1_obj*>(pFactory);

    // Use EnumAdapters1 (index 12) to get DXGI_ADAPTER_DESC1
    typedef HRESULT(WINAPI* PFN_EnumAdapters1)(void*, UINT, void**);
    auto fnEnumAdapters1 = (PFN_EnumAdapters1)(factoryObj->vtbl->methods[12]);

    void* pAdapter = nullptr;
    hr = fnEnumAdapters1(pFactory, 0, &pAdapter);
    if (SUCCEEDED(hr) && pAdapter) {
        // IDXGIAdapter1 vtable: GetDesc1 is at index 10
        struct IDXGIAdapter1_vtbl { void* methods[11]; };
        struct IDXGIAdapter1_obj { IDXGIAdapter1_vtbl* vtbl; };

        struct DXGI_ADAPTER_DESC1 {
            WCHAR Description[128];
            UINT VendorId;
            UINT DeviceId;
            UINT SubSysId;
            UINT Revision;
            SIZE_T DedicatedVideoMemory;
            SIZE_T DedicatedSystemMemory;
            SIZE_T SharedSystemMemory;
            LUID AdapterLuid;
            UINT Flags;
        };

        auto* adapterObj = static_cast<IDXGIAdapter1_obj*>(pAdapter);
        typedef HRESULT(WINAPI* PFN_GetDesc1)(void*, DXGI_ADAPTER_DESC1*);
        auto fnGetDesc1 = (PFN_GetDesc1)(adapterObj->vtbl->methods[10]);

        DXGI_ADAPTER_DESC1 desc;
        memset(&desc, 0, sizeof(desc));
        hr = fnGetDesc1(pAdapter, &desc);
        if (SUCCEEDED(hr)) {
            m_gpu.vendorId = desc.VendorId;
            m_gpu.deviceId = desc.DeviceId;
            m_gpu.vramBytes = desc.DedicatedVideoMemory;

            // Convert wide description
            char nameBuf[256];
            WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
            m_gpu.deviceName = nameBuf;

            m_gpu.vendorClass = classifyGPU(desc.VendorId, desc.DeviceId);

            // Set vendor-specific defaults
            if (desc.VendorId == 0x1002) { // AMD
                m_gpu.wavefrontSize = 64;
                m_gpu.hasAsyncCompute = true;
                m_gpu.hasDPAcceleration = true;

                // Estimate CUs from VRAM (rough heuristic)
                if (m_gpu.vramBytes >= 16ULL * 1024 * 1024 * 1024) {
                    m_gpu.computeUnits = 60; // RX 7900 XTX class
                    m_gpu.hasMatrixCores = true; // WMMA
                    m_gpu.hasInfinityCache = true;
                    m_gpu.clockMHz = 2500;
                    m_gpu.memBusWidth = 384;
                } else if (m_gpu.vramBytes >= 8ULL * 1024 * 1024 * 1024) {
                    m_gpu.computeUnits = 36;
                    m_gpu.hasMatrixCores = true;
                    m_gpu.hasInfinityCache = true;
                    m_gpu.clockMHz = 2400;
                    m_gpu.memBusWidth = 256;
                } else {
                    m_gpu.computeUnits = 20;
                    m_gpu.clockMHz = 2000;
                    m_gpu.memBusWidth = 128;
                }
                m_gpu.ldsPerCU = 65536; // 64KB LDS per CU
                m_gpu.vgprsPerCU = 65536;
                m_gpu.sgprsPerCU = 16384;
                m_gpu.maxWavesPerCU = 32;
            } else if (desc.VendorId == 0x10DE) { // NVIDIA
                m_gpu.wavefrontSize = 32;
                m_gpu.hasAsyncCompute = true;
                m_gpu.computeUnits = 40; // Rough estimate
                m_gpu.clockMHz = 2100;
                m_gpu.ldsPerCU = 49152; // 48KB shared per SM
                m_gpu.maxWavesPerCU = 48;
            } else {
                m_gpu.wavefrontSize = 32;
                m_gpu.computeUnits = 8;
            }
        }

        // Release adapter
        typedef ULONG(WINAPI* PFN_Release)(void*);
        auto fnRelease = (PFN_Release)(adapterObj->vtbl->methods[2]);
        fnRelease(pAdapter);
    }

    // Release factory
    typedef ULONG(WINAPI* PFN_Release)(void*);
    auto fnFactoryRelease = (PFN_Release)(factoryObj->vtbl->methods[2]);
    fnFactoryRelease(pFactory);

    FreeLibrary(hDXGI);

    s_logger.info("[AUTOTUNER] Detected GPU: ");

    return AutotuneResult::ok("GPU detected");
}

GPUVendorClass GPUKernelAutoTuner::classifyGPU(uint32_t vendorId, uint32_t deviceId) const {
    if (vendorId == 0x1002) { // AMD
        // RDNA3: Navi 3x (device IDs 0x744C-0x7480 range)
        if (deviceId >= 0x744C && deviceId <= 0x7500) return GPUVendorClass::AMD_RDNA3;
        // RDNA2: Navi 2x (device IDs 0x73BF-0x73FF range)
        if (deviceId >= 0x73A0 && deviceId <= 0x7430) return GPUVendorClass::AMD_RDNA2;
        // RDNA1: Navi 1x (device IDs 0x7310-0x7360 range)
        if (deviceId >= 0x7310 && deviceId <= 0x7370) return GPUVendorClass::AMD_RDNA1;
        // CDNA2: MI200 series
        if (deviceId >= 0x7400 && deviceId <= 0x7440) return GPUVendorClass::AMD_CDNA2;
        // CDNA1: MI100
        if (deviceId >= 0x7380 && deviceId <= 0x73A0) return GPUVendorClass::AMD_CDNA1;
        // Default AMD to GCN5
        return GPUVendorClass::AMD_GCN5;
    }
    if (vendorId == 0x10DE) { // NVIDIA
        if (deviceId >= 0x2600) return GPUVendorClass::NVIDIA_Ada;
        return GPUVendorClass::NVIDIA_Ampere;
    }
    if (vendorId == 0x8086) return GPUVendorClass::Intel_Xe;
    return GPUVendorClass::Unknown;
}

// ============================================================================
// Kernel Tuning
// ============================================================================

AutotuneResult GPUKernelAutoTuner::tuneKernel(KernelType type, uint32_t M, uint32_t N, uint32_t K,
                                                TuneStrategy strategy) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stats.totalTuneRuns.fetch_add(1, std::memory_order_relaxed);

    auto start = std::chrono::steady_clock::now();

    // Check cache first
    std::string key = makeCacheKey(type, M, N, K);
    if (strategy == TuneStrategy::CacheLookup || m_tuneCache.count(key)) {
        m_stats.cacheHits.fetch_add(1, std::memory_order_relaxed);
        if (m_tuneCache.count(key)) {
            AutotuneResult r = AutotuneResult::ok("Cache hit");
            r.bestConfig = m_tuneCache[key].bestConfig;
            r.bestThroughput = m_tuneCache[key].bestThroughput;
            r.configsTested = 0;
            return r;
        }
    }
    m_stats.cacheMisses.fetch_add(1, std::memory_order_relaxed);

    // Generate candidate configs
    std::vector<DispatchConfig> candidates = generateCandidates(type, M, N, K, strategy);

    DispatchConfig bestConfig;
    double bestThroughput = 0;
    uint32_t tested = 0;

    for (const auto& config : candidates) {
        KernelBenchmark result = benchmarkConfig(type, M, N, K, config);
        m_stats.benchmarkRuns.fetch_add(1, std::memory_order_relaxed);
        m_stats.configsExplored.fetch_add(1, std::memory_order_relaxed);
        tested++;

        if (result.valid && result.throughputGFLOPS > bestThroughput) {
            bestThroughput = result.throughputGFLOPS;
            bestConfig = config;
            m_stats.improvementsFound.fetch_add(1, std::memory_order_relaxed);
        }
    }

    // Adaptive scan: explore neighbors of best config
    if (strategy == TuneStrategy::AdaptiveScan && bestThroughput > 0) {
        std::vector<DispatchConfig> neighbors = generateNeighbors(bestConfig);
        for (const auto& config : neighbors) {
            KernelBenchmark result = benchmarkConfig(type, M, N, K, config);
            m_stats.benchmarkRuns.fetch_add(1, std::memory_order_relaxed);
            m_stats.configsExplored.fetch_add(1, std::memory_order_relaxed);
            tested++;

            if (result.valid && result.throughputGFLOPS > bestThroughput) {
                bestThroughput = result.throughputGFLOPS;
                bestConfig = config;
                m_stats.improvementsFound.fetch_add(1, std::memory_order_relaxed);
            }
        }
    }

    // Cache result
    TuneCacheEntry entry;
    entry.kernelType = type;
    entry.matrixM = M;
    entry.matrixN = N;
    entry.matrixK = K;
    entry.bestConfig = bestConfig;
    entry.bestThroughput = bestThroughput;
    entry.samplesTested = tested;
    entry.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    m_tuneCache[key] = entry;

    auto elapsed = std::chrono::steady_clock::now() - start;
    m_stats.tuneTimeMs.fetch_add(
        std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count(),
        std::memory_order_relaxed);

    if (bestThroughput > m_stats.bestOverallTFLOPS) {
        m_stats.bestOverallTFLOPS = bestThroughput / 1000.0; // GFLOPS → TFLOPS
    }

    AutotuneResult r = AutotuneResult::ok("Tuning complete");
    r.bestConfig = bestConfig;
    r.bestThroughput = bestThroughput;
    r.configsTested = tested;
    return r;
}

AutotuneResult GPUKernelAutoTuner::tuneForModelLayer(const char* layerName,
                                                       uint32_t hiddenDim,
                                                       uint32_t headCount,
                                                       uint32_t kvHeadCount) {
    // Compute matrix dimensions from model architecture
    uint32_t headDim = hiddenDim / headCount;
    uint32_t kvDim = headDim * kvHeadCount;

    // Tune QKV projection: [batch, hiddenDim] x [hiddenDim, 3*hiddenDim]
    auto r1 = tuneKernel(KernelType::MatMul_FP16, 1, 3 * hiddenDim, hiddenDim, TuneStrategy::Heuristic);

    // Tune attention: [batch*heads, seqlen, headDim] x [batch*heads, headDim, seqlen]
    auto r2 = tuneKernel(KernelType::Attention_FlashV2, headCount, headDim, headDim, TuneStrategy::Heuristic);

    // Tune FFN: typically 4*hiddenDim intermediate
    auto r3 = tuneKernel(KernelType::MatMul_FP16, 1, 4 * hiddenDim, hiddenDim, TuneStrategy::Heuristic);

    // Tune RMSNorm
    auto r4 = tuneKernel(KernelType::RMSNorm, 1, hiddenDim, 1, TuneStrategy::Heuristic);

    s_logger.info("[AUTOTUNER] Tuned layer '");

    return AutotuneResult::ok("Model layer tuning complete");
}

AutotuneResult GPUKernelAutoTuner::tuneAllKernels(TuneStrategy strategy) {
    // Common matrix sizes for LLM inference
    const uint32_t sizes[][3] = {
        {1, 4096, 4096},     // Small hidden
        {1, 5120, 5120},     // 13B-class
        {1, 8192, 8192},     // 70B-class
        {1, 12288, 12288},   // 120B-class
        {1, 16384, 16384},   // 400B-class
        {1, 32768, 32768},   // 800B-class (extreme)
        {32, 4096, 4096},    // Batched small
        {32, 8192, 8192},    // Batched large
    };

    uint32_t total = 0;
    for (const auto& [M, N, K] : sizes) {
        tuneKernel(KernelType::MatMul_FP16, M, N, K, strategy);
        tuneKernel(KernelType::MatMul_INT8, M, N, K, strategy);
        tuneKernel(KernelType::MatMul_INT4, M, N, K, strategy);
        total += 3;
    }

    // Also tune non-matmul kernels for common hidden sizes
    const uint32_t hiddenSizes[] = { 4096, 5120, 8192, 12288, 16384, 32768 };
    for (uint32_t h : hiddenSizes) {
        tuneKernel(KernelType::RMSNorm, 1, h, 1, strategy);
        tuneKernel(KernelType::RoPE, 1, h, 1, strategy);
        tuneKernel(KernelType::Softmax, 1, h, 1, strategy);
        tuneKernel(KernelType::GELU, 1, h, 1, strategy);
        tuneKernel(KernelType::SiLU, 1, h, 1, strategy);
        total += 5;
    }

    s_logger.info("[AUTOTUNER] Bulk tuning complete: ");
    AutotuneResult r = AutotuneResult::ok("All kernels tuned");
    r.configsTested = total;
    return r;
}

// ============================================================================
// Config Lookup
// ============================================================================

DispatchConfig GPUKernelAutoTuner::getConfig(KernelType type, uint32_t M, uint32_t N, uint32_t K) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = makeCacheKey(type, M, N, K);
    auto it = m_tuneCache.find(key);
    if (it != m_tuneCache.end()) return it->second.bestConfig;
    return getDefaultConfig(type);
}

DispatchConfig GPUKernelAutoTuner::getDefaultConfig(KernelType type) const {
    DispatchConfig cfg;

    // Apply AMD-specific defaults if AMD GPU detected
    bool isAMD = (m_gpu.vendorId == 0x1002);
    cfg.wavefrontSize = isAMD ? 64 : 32;

    switch (type) {
    case KernelType::MatMul_FP16:
    case KernelType::MatMul_INT8:
    case KernelType::MatMul_INT4:
        cfg.workgroupSizeX = isAMD ? 256 : 128;
        cfg.workgroupSizeY = 1;
        cfg.tilesM = isAMD ? 128 : 64;
        cfg.tilesN = isAMD ? 128 : 64;
        cfg.tilesK = isAMD ? 32 : 16;
        cfg.sharedMemBytes = isAMD ? 65536 : 49152;
        cfg.tilingMode = isAMD ? MemoryTilingMode::AMDDelta : MemoryTilingMode::Tiled2D;
        cfg.prefetchToLDS = isAMD;
        cfg.usePackedMath = true;
        break;

    case KernelType::Attention_FlashV2:
    case KernelType::Attention_Paged:
        cfg.workgroupSizeX = isAMD ? 256 : 128;
        cfg.tilesM = 64;
        cfg.tilesN = 64;
        cfg.tilesK = 64;
        cfg.sharedMemBytes = isAMD ? 65536 : 49152;
        cfg.useAsyncCopy = isAMD; // AMD DMA engine
        break;

    case KernelType::RMSNorm:
    case KernelType::Softmax:
        cfg.workgroupSizeX = isAMD ? 256 : 256;
        cfg.tilesM = 1;
        cfg.tilesN = 1;
        cfg.sharedMemBytes = 8192;
        break;

    case KernelType::RoPE:
    case KernelType::GELU:
    case KernelType::SiLU:
        cfg.workgroupSizeX = isAMD ? 256 : 128;
        cfg.sharedMemBytes = 4096;
        break;

    case KernelType::Quantize_Q4K:
    case KernelType::Dequantize_Q4K:
    case KernelType::Quantize_Q8K:
    case KernelType::Dequantize_Q8K:
        cfg.workgroupSizeX = 256;
        cfg.sharedMemBytes = 16384;
        cfg.usePackedMath = true;
        break;

    case KernelType::KVCacheUpdate:
        cfg.workgroupSizeX = 64;
        cfg.useAsyncCopy = true;
        break;

    case KernelType::TokenEmbed:
        cfg.workgroupSizeX = 256;
        break;

    case KernelType::AllReduce:
        cfg.workgroupSizeX = isAMD ? 256 : 128;
        cfg.useCooperativeGroups = true;
        break;

    default:
        break;
    }

    return cfg;
}

// ============================================================================
// AMD-Specific Tuning
// ============================================================================

DispatchConfig GPUKernelAutoTuner::tuneAMD_RDNA(KernelType type, uint32_t M, uint32_t N, uint32_t K) const {
    DispatchConfig cfg;
    cfg.wavefrontSize = 64; // RDNA can do wave32 or wave64, prefer 64 for matmul
    cfg.prefetchToLDS = true;
    cfg.tilingMode = MemoryTilingMode::AMDDelta;
    cfg.useAsyncCopy = true; // RDNA2/3 support async copy engine

    // RDNA3 has WMMA (Wave Matrix Multiply Accumulate) instructions
    bool hasWMMA = (m_gpu.vendorClass == GPUVendorClass::AMD_RDNA3);

    switch (type) {
    case KernelType::MatMul_FP16:
        if (hasWMMA) {
            // WMMA tiles: 16x16x16 for FP16
            cfg.workgroupSizeX = 256;
            cfg.tilesM = 128;
            cfg.tilesN = 128;
            cfg.tilesK = 16;
            cfg.sharedMemBytes = 65536;
        } else {
            // VALU dot product path
            cfg.workgroupSizeX = 256;
            cfg.tilesM = 64;
            cfg.tilesN = 64;
            cfg.tilesK = 32;
            cfg.sharedMemBytes = 32768;
        }
        break;

    case KernelType::MatMul_INT4:
        // INT4 dequant+matmul fused kernel
        cfg.workgroupSizeX = 256;
        cfg.tilesM = 64;
        cfg.tilesN = 128; // Wider N for better memory coalescing
        cfg.tilesK = 64;
        cfg.sharedMemBytes = 32768;
        cfg.usePackedMath = true;
        break;

    case KernelType::Attention_FlashV2:
        cfg.workgroupSizeX = 256;
        cfg.tilesM = 64;
        cfg.tilesN = 64;
        cfg.tilesK = 128; // Larger K tile for attention
        cfg.sharedMemBytes = 65536;
        cfg.useAsyncCopy = true;
        break;

    default:
        cfg = getDefaultConfig(type);
        break;
    }

    // Compute occupancy
    cfg.occupancyTarget = computeOptimalOccupancy(
        computeOptimalLDSUsage(cfg.tilesM, cfg.tilesN, 2), // 2 bytes for FP16
        cfg.sharedMemBytes
    );

    return cfg;
}

DispatchConfig GPUKernelAutoTuner::tuneAMD_CDNA(KernelType type, uint32_t M, uint32_t N, uint32_t K) const {
    DispatchConfig cfg;
    cfg.wavefrontSize = 64;
    cfg.prefetchToLDS = true;
    cfg.tilingMode = MemoryTilingMode::Tiled2D;
    cfg.useAsyncCopy = true;

    // CDNA has MFMA (Matrix Fused Multiply Add) instructions
    switch (type) {
    case KernelType::MatMul_FP16:
        // MFMA tiles: 32x32x8 for FP16
        cfg.workgroupSizeX = 256;
        cfg.tilesM = 128;
        cfg.tilesN = 128;
        cfg.tilesK = 8;
        cfg.sharedMemBytes = 65536;
        cfg.usePackedMath = true;
        break;

    case KernelType::MatMul_INT8:
        // MFMA INT8: 32x32x16
        cfg.workgroupSizeX = 256;
        cfg.tilesM = 128;
        cfg.tilesN = 128;
        cfg.tilesK = 16;
        cfg.sharedMemBytes = 65536;
        break;

    default:
        cfg = getDefaultConfig(type);
        break;
    }

    return cfg;
}

uint32_t GPUKernelAutoTuner::computeOptimalLDSUsage(uint32_t tilesM, uint32_t tilesN, uint32_t elemSize) const {
    // LDS needs to hold tiles for A and B
    // A tile: tilesM * tilesK * elemSize
    // B tile: tilesK * tilesN * elemSize
    // Using tilesK = 32 as typical
    uint32_t tilesK = 32;
    uint32_t aTileSize = tilesM * tilesK * elemSize;
    uint32_t bTileSize = tilesK * tilesN * elemSize;
    uint32_t total = aTileSize + bTileSize;

    // Round up to 256 bytes (AMD LDS bank alignment)
    total = (total + 255) & ~255u;

    // Cap at max LDS per CU
    if (total > m_gpu.ldsPerCU) total = m_gpu.ldsPerCU;

    return total;
}

uint32_t GPUKernelAutoTuner::computeOptimalOccupancy(uint32_t vgprCount, uint32_t ldsBytes) const {
    // AMD CU: 64KB LDS shared among workgroups, 256 VGPRs per wave
    // Occupancy = min(maxWaves, floor(totalLDS / ldsPerWG), floor(totalVGPR / vgprPerWave))

    uint32_t maxByLDS = (ldsBytes > 0) ? (m_gpu.ldsPerCU / ldsBytes) : m_gpu.maxWavesPerCU;
    uint32_t maxByVGPR = (vgprCount > 0) ? (m_gpu.vgprsPerCU / vgprCount) : m_gpu.maxWavesPerCU;

    uint32_t occupancy = std::min({m_gpu.maxWavesPerCU, maxByLDS, maxByVGPR});

    // Express as percentage
    return (occupancy * 100) / m_gpu.maxWavesPerCU;
}

// ============================================================================
// Benchmarking — Analytical Performance Model
// ============================================================================

KernelBenchmark GPUKernelAutoTuner::benchmarkConfig(KernelType type, uint32_t M, uint32_t N, uint32_t K,
                                                      const DispatchConfig& config) {
    KernelBenchmark result;
    result.config = config;

    // In production: dispatch actual compute shader with the given config
    // and measure wall-clock time via GPU timestamp queries.
    // Here we use an analytical model based on hardware profile.

    double ops = 2.0 * M * N * K; // FLOPs for matmul
    if (type == KernelType::RMSNorm || type == KernelType::Softmax ||
        type == KernelType::GELU || type == KernelType::SiLU ||
        type == KernelType::RoPE) {
        ops = 2.0 * N; // Element-wise
    }

    // Compute theoretical throughput given config parameters
    double wavesNeeded = std::ceil((double)(M * N) / (config.tilesM * config.tilesN));
    double wavesPerDispatch = (double)m_gpu.computeUnits * (config.occupancyTarget / 100.0) * m_gpu.maxWavesPerCU;

    double dispatches = std::ceil(wavesNeeded / wavesPerDispatch);
    double clocksPerDispatch = (double)config.tilesK; // Simplified: K reduction iterations

    double totalClocks = dispatches * clocksPerDispatch;
    double timeS = totalClocks / (m_gpu.clockMHz * 1e6);

    result.elapsedMs = timeS * 1000.0;
    result.throughputGFLOPS = (result.elapsedMs > 0) ? (ops / (result.elapsedMs * 1e6)) : 0;

    // Memory bandwidth estimation
    double memBytes = (double)(M * K + K * N + M * N) * 2; // FP16
    result.memBandwidthGBs = (result.elapsedMs > 0) ? (memBytes / (result.elapsedMs * 1e6)) : 0;

    // Occupancy estimation
    result.ldsUsageBytes = config.sharedMemBytes;
    result.vgprCount = (config.tilesM * config.tilesN) / config.wavefrontSize + 16; // Rough estimate
    result.wavesPerCU = computeOptimalOccupancy(result.vgprCount, result.ldsUsageBytes);

    // Validity: config must not exceed hardware limits
    result.valid = true;
    if (config.sharedMemBytes > m_gpu.ldsPerCU) result.valid = false;
    if (config.workgroupSizeX * config.workgroupSizeY * config.workgroupSizeZ > 1024) result.valid = false;
    if (config.workgroupSizeX % config.wavefrontSize != 0) result.valid = false;

    return result;
}

std::vector<DispatchConfig> GPUKernelAutoTuner::generateCandidates(KernelType type,
                                                                     uint32_t M, uint32_t N, uint32_t K,
                                                                     TuneStrategy strategy) const {
    std::vector<DispatchConfig> candidates;

    bool isAMD = (m_gpu.vendorId == 0x1002);

    if (strategy == TuneStrategy::Heuristic || strategy == TuneStrategy::AdaptiveScan) {
        // Start with vendor-specific heuristic
        if (isAMD) {
            if (m_gpu.vendorClass == GPUVendorClass::AMD_CDNA1 ||
                m_gpu.vendorClass == GPUVendorClass::AMD_CDNA2) {
                candidates.push_back(tuneAMD_CDNA(type, M, N, K));
            } else {
                candidates.push_back(tuneAMD_RDNA(type, M, N, K));
            }
        }
        candidates.push_back(getDefaultConfig(type));
    }

    if (strategy == TuneStrategy::Exhaustive || strategy == TuneStrategy::AdaptiveScan) {
        // Generate grid of workgroup sizes
        const uint32_t wgSizes[] = { 64, 128, 256, 512 };
        const uint32_t tileSizes[] = { 32, 64, 128, 256 };
        const uint32_t kTiles[] = { 8, 16, 32, 64 };

        for (uint32_t wg : wgSizes) {
            // Skip sizes smaller than wavefront
            if (wg < m_gpu.wavefrontSize) continue;

            for (uint32_t tM : tileSizes) {
                for (uint32_t tN : tileSizes) {
                    for (uint32_t tK : kTiles) {
                        DispatchConfig cfg;
                        cfg.workgroupSizeX = wg;
                        cfg.workgroupSizeY = 1;
                        cfg.workgroupSizeZ = 1;
                        cfg.wavefrontSize = m_gpu.wavefrontSize;
                        cfg.tilesM = tM;
                        cfg.tilesN = tN;
                        cfg.tilesK = tK;
                        cfg.sharedMemBytes = computeOptimalLDSUsage(tM, tN, 2);
                        cfg.tilingMode = isAMD ? MemoryTilingMode::AMDDelta : MemoryTilingMode::Tiled2D;
                        cfg.prefetchToLDS = isAMD;
                        cfg.usePackedMath = true;
                        cfg.occupancyTarget = computeOptimalOccupancy(
                            (tM * tN) / m_gpu.wavefrontSize + 16,
                            cfg.sharedMemBytes);
                        candidates.push_back(cfg);
                    }
                }
            }

            // For non-exhaustive, only try a subset
            if (strategy != TuneStrategy::Exhaustive) break;
        }
    }

    return candidates;
}

std::vector<DispatchConfig> GPUKernelAutoTuner::generateNeighbors(const DispatchConfig& center) const {
    std::vector<DispatchConfig> neighbors;

    // Vary workgroup size
    for (int delta : {-64, 64}) {
        int newWG = (int)center.workgroupSizeX + delta;
        if (newWG >= (int)m_gpu.wavefrontSize && newWG <= 1024) {
            DispatchConfig cfg = center;
            cfg.workgroupSizeX = (uint32_t)newWG;
            neighbors.push_back(cfg);
        }
    }

    // Vary tile M
    for (int delta : {-32, 32}) {
        int newTM = (int)center.tilesM + delta;
        if (newTM >= 16 && newTM <= 512) {
            DispatchConfig cfg = center;
            cfg.tilesM = (uint32_t)newTM;
            neighbors.push_back(cfg);
        }
    }

    // Vary tile N
    for (int delta : {-32, 32}) {
        int newTN = (int)center.tilesN + delta;
        if (newTN >= 16 && newTN <= 512) {
            DispatchConfig cfg = center;
            cfg.tilesN = (uint32_t)newTN;
            neighbors.push_back(cfg);
        }
    }

    // Vary tile K
    for (int delta : {-8, 8}) {
        int newTK = (int)center.tilesK + delta;
        if (newTK >= 4 && newTK <= 128) {
            DispatchConfig cfg = center;
            cfg.tilesK = (uint32_t)newTK;
            neighbors.push_back(cfg);
        }
    }

    // Vary LDS size
    for (int delta : {-8192, 8192}) {
        int newLDS = (int)center.sharedMemBytes + delta;
        if (newLDS >= 4096 && newLDS <= (int)m_gpu.ldsPerCU) {
            DispatchConfig cfg = center;
            cfg.sharedMemBytes = (uint32_t)newLDS;
            neighbors.push_back(cfg);
        }
    }

    // Toggle flags
    {
        DispatchConfig cfg = center;
        cfg.useAsyncCopy = !center.useAsyncCopy;
        neighbors.push_back(cfg);
    }
    {
        DispatchConfig cfg = center;
        cfg.prefetchToLDS = !center.prefetchToLDS;
        neighbors.push_back(cfg);
    }

    return neighbors;
}

// ============================================================================
// Cache Management
// ============================================================================

std::string GPUKernelAutoTuner::makeCacheKey(KernelType type, uint32_t M, uint32_t N, uint32_t K) const {
    std::ostringstream oss;
    oss << static_cast<int>(type) << "_" << M << "_" << N << "_" << K
        << "_v" << m_gpu.vendorId << "_d" << m_gpu.deviceId;
    return oss.str();
}

bool GPUKernelAutoTuner::loadTuneCache(const char* path) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    // Simple line-based format: key|wgX|wgY|wgZ|tM|tN|tK|wave|occ|shmem|throughput
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;

        // Parse pipe-delimited
        std::istringstream iss(line);
        std::string key;
        TuneCacheEntry entry;
        char delim;

        if (std::getline(iss, key, '|')) {
            iss >> entry.bestConfig.workgroupSizeX >> delim
                >> entry.bestConfig.workgroupSizeY >> delim
                >> entry.bestConfig.workgroupSizeZ >> delim
                >> entry.bestConfig.tilesM >> delim
                >> entry.bestConfig.tilesN >> delim
                >> entry.bestConfig.tilesK >> delim
                >> entry.bestConfig.wavefrontSize >> delim
                >> entry.bestConfig.occupancyTarget >> delim
                >> entry.bestConfig.sharedMemBytes >> delim
                >> entry.bestThroughput;

            m_tuneCache[key] = entry;
        }
    }

    s_logger.info("[AUTOTUNER] Loaded ");
    return true;
}

bool GPUKernelAutoTuner::saveTuneCache(const char* path) const {
    std::ofstream f(path);
    if (!f.is_open()) return false;

    f << "# RawrXD GPU Kernel AutoTuner Cache\n";
    f << "# Format: key|wgX|wgY|wgZ|tM|tN|tK|wave|occ|shmem|throughput\n";

    for (const auto& [key, entry] : m_tuneCache) {
        const auto& c = entry.bestConfig;
        f << key << "|"
          << c.workgroupSizeX << "|" << c.workgroupSizeY << "|" << c.workgroupSizeZ << "|"
          << c.tilesM << "|" << c.tilesN << "|" << c.tilesK << "|"
          << c.wavefrontSize << "|" << c.occupancyTarget << "|" << c.sharedMemBytes << "|"
          << entry.bestThroughput << "\n";
    }

    s_logger.info("[AUTOTUNER] Saved ");
    return true;
}

void GPUKernelAutoTuner::clearTuneCache() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tuneCache.clear();
}

uint32_t GPUKernelAutoTuner::getCacheSize() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (uint32_t)m_tuneCache.size();
}

// ============================================================================
// Stats & JSON
// ============================================================================

void GPUKernelAutoTuner::resetStats() {
    m_stats.totalTuneRuns.store(0);
    m_stats.cacheHits.store(0);
    m_stats.cacheMisses.store(0);
    m_stats.benchmarkRuns.store(0);
    m_stats.configsExplored.store(0);
    m_stats.improvementsFound.store(0);
    m_stats.tuneTimeMs.store(0);
    m_stats.bestOverallTFLOPS = 0;
}

std::string GPUKernelAutoTuner::toJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "{\"initialized\":" << (m_initialized.load() ? "true" : "false")
        << ",\"gpu\":" << gpuProfileToJson()
        << ",\"cacheSize\":" << m_tuneCache.size()
        << ",\"stats\":{\"tuneRuns\":" << m_stats.totalTuneRuns.load()
        << ",\"cacheHits\":" << m_stats.cacheHits.load()
        << ",\"cacheMisses\":" << m_stats.cacheMisses.load()
        << ",\"benchmarks\":" << m_stats.benchmarkRuns.load()
        << ",\"configsExplored\":" << m_stats.configsExplored.load()
        << ",\"improvements\":" << m_stats.improvementsFound.load()
        << ",\"tuneTimeMs\":" << m_stats.tuneTimeMs.load()
        << ",\"bestTFLOPS\":" << m_stats.bestOverallTFLOPS
        << "}}";
    return oss.str();
}

std::string GPUKernelAutoTuner::gpuProfileToJson() const {
    std::ostringstream oss;
    oss << "{\"device\":\"" << m_gpu.deviceName << "\""
        << ",\"vendorId\":\"0x" << std::hex << m_gpu.vendorId << std::dec << "\""
        << ",\"deviceId\":\"0x" << std::hex << m_gpu.deviceId << std::dec << "\""
        << ",\"class\":" << static_cast<int>(m_gpu.vendorClass)
        << ",\"computeUnits\":" << m_gpu.computeUnits
        << ",\"wavefront\":" << m_gpu.wavefrontSize
        << ",\"vramMB\":" << (m_gpu.vramBytes / (1024*1024))
        << ",\"clockMHz\":" << m_gpu.clockMHz
        << ",\"peakFP16_TFLOPS\":" << m_gpu.peakTFLOPS_FP16()
        << ",\"matrixCores\":" << (m_gpu.hasMatrixCores ? "true" : "false")
        << ",\"asyncCompute\":" << (m_gpu.hasAsyncCompute ? "true" : "false")
        << ",\"infinityCache\":" << (m_gpu.hasInfinityCache ? "true" : "false")
        << "}";
    return oss.str();
}

std::string GPUKernelAutoTuner::tuneCacheToJson() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::ostringstream oss;
    oss << "[";
    bool first = true;
    for (const auto& [key, entry] : m_tuneCache) {
        if (!first) oss << ",";
        first = false;
        oss << "{\"key\":\"" << key << "\""
            << ",\"kernel\":" << static_cast<int>(entry.kernelType)
            << ",\"M\":" << entry.matrixM
            << ",\"N\":" << entry.matrixN
            << ",\"K\":" << entry.matrixK
            << ",\"throughput\":" << entry.bestThroughput
            << ",\"tested\":" << entry.samplesTested
            << ",\"wg\":" << entry.bestConfig.workgroupSizeX
            << ",\"tileM\":" << entry.bestConfig.tilesM
            << ",\"tileN\":" << entry.bestConfig.tilesN
            << ",\"tileK\":" << entry.bestConfig.tilesK
            << "}";
    }
    oss << "]";
    return oss.str();
}
