// ============================================================================
// accelerator_router.h — Phase 30: Unified Multi-Backend Accelerator Router
// ============================================================================
// Automatic hardware detection and intelligent routing of compute workloads
// across all available accelerator backends:
//
//   Priority (local):  AMD XDNA  →  Intel Xe  →  ARM64 Adreno/NPU  →  CPU
//   Priority (remote): Cerebras WSE-2/3 (network-attached, high-batch only)
//
// Design principles:
//   - Singleton (matches all Phase 25 accelerators)
//   - PatchResult-style structured returns (no exceptions)
//   - Function-pointer callbacks (no std::function in hot path)
//   - Zero compile-time dependency on vendor SDKs
//   - Thermal-aware routing with automatic fallback cascade
//   - Scope-based dispatch: different workloads prefer different backends
//   - Deterministic: given same hardware + config, always routes the same way
//
// The router does NOT own the backend singletons. It references them.
// Each backend manages its own lifecycle. The router merely queries and routes.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#ifndef ACCELERATOR_ROUTER_H
#define ACCELERATOR_ROUTER_H

#include <cstdint>
#include <cstddef>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// Forward-declare accelerator classes (no header coupling)
class AMDGPUAccelerator;
class IntelGPUAccelerator;
class ARM64GPUAccelerator;
class CerebrasWSEAccelerator;

// ============================================================================
// Backend Type Enum (Unified Across All Accelerators)
// ============================================================================

enum class RouterBackendType : uint8_t {
    None            = 0,
    AMD_XDNA        = 1,   // Phase 25: AMD RDNA/CDNA (DX12, Vulkan, ROCm)
    Intel_Xe        = 2,   // Phase 29A: Intel Arc/Meteor Lake (Level Zero, DX12)
    ARM64_Adreno    = 3,   // Phase 29B: Qualcomm Adreno GPU (DX12, Vulkan)
    ARM64_NPU       = 4,   // Phase 29B: Qualcomm Hexagon NPU (QNN/SNPE)
    Cerebras_WSE    = 5,   // Phase 29C: Cerebras WSE-2/3 (Network, gRPC/TCP)
    CPU_Fallback    = 6,   // AVX-512 / NEON / SVE2 (always available)
    Auto            = 255
};

// ============================================================================
// Dispatch Priority Hint
// ============================================================================

enum class DispatchPriority : uint8_t {
    Realtime     = 0,   // UI-blocking: sub-5ms target (ghost text, hover)
    Interactive  = 1,   // User-facing: sub-100ms target (completions, search)
    Batch        = 2,   // Background: no latency constraint (quantization, training)
    Streaming    = 3    // Long-running: token generation, weight streaming
};

// ============================================================================
// Dispatch Scope (which subsystem is requesting compute)
// ============================================================================

enum class DispatchScope : uint8_t {
    Inference        = 0x01,   // Model inference (matmul, attention)
    Quantization     = 0x02,   // Quantize / dequantize
    ModelSurgery     = 0x04,   // Hotpatch byte-level ops
    SwarmCompute     = 0x08,   // Distributed swarm GPU offload
    KVCache          = 0x10,   // KV cache management
    Embedding        = 0x20,   // Token embedding lookup
    SymbolResolution = 0x40,   // PDB symbol parallel search
    All              = 0xFF
};

// ============================================================================
// Inference Task — Submitted to the router for dispatch
// ============================================================================

struct RouterInferenceTask {
    const void*       inputData;
    uint64_t          inputSizeBytes;
    void*             outputData;
    uint64_t          outputSizeBytes;
    const char*       kernelName;       // e.g. "matmul_q4_0", "flash_attn_v2"
    DispatchPriority  priority;
    DispatchScope     scope;
    RouterBackendType preferredBackend; // Hint, router may override
    uint32_t          timeoutMs;       // 0 = no timeout
    uint32_t          batchSize;       // For Cerebras batch optimization
    uint8_t           quantType;       // 0=FP32, 1=FP16, 2=BF16, 3=INT8, 4=INT4
};

// ============================================================================
// Router Result — Returned from dispatch
// ============================================================================

struct RouterResult {
    bool              success;
    const char*       detail;
    int               errorCode;
    RouterBackendType executedOn;       // Which backend actually ran the task
    RouterBackendType attemptedFirst;   // Which backend was tried first
    double            elapsedMs;        // Wall-clock time
    double            throughputGFLOPS; // If applicable
    double            networkLatencyMs; // For Cerebras remote execution
    uint32_t          deviceTempC;      // Thermal reading at dispatch time
    bool              wasFallback;      // True if cascaded to lower-tier backend

    static RouterResult ok(const char* msg, RouterBackendType backend) {
        RouterResult r;
        r.success = true; r.detail = msg; r.errorCode = 0;
        r.executedOn = backend; r.attemptedFirst = backend;
        r.elapsedMs = 0; r.throughputGFLOPS = 0; r.networkLatencyMs = 0;
        r.deviceTempC = 0; r.wasFallback = false;
        return r;
    }
    static RouterResult error(const char* msg, int code = -1) {
        RouterResult r;
        r.success = false; r.detail = msg; r.errorCode = code;
        r.executedOn = RouterBackendType::None;
        r.attemptedFirst = RouterBackendType::None;
        r.elapsedMs = 0; r.throughputGFLOPS = 0; r.networkLatencyMs = 0;
        r.deviceTempC = 0; r.wasFallback = false;
        return r;
    }
    static RouterResult fallback(const char* msg, RouterBackendType tried, RouterBackendType actual) {
        RouterResult r;
        r.success = true; r.detail = msg; r.errorCode = 0;
        r.executedOn = actual; r.attemptedFirst = tried;
        r.elapsedMs = 0; r.throughputGFLOPS = 0; r.networkLatencyMs = 0;
        r.deviceTempC = 0; r.wasFallback = true;
        return r;
    }
};

// ============================================================================
// Per-Backend State (internal tracking)
// ============================================================================

struct BackendState {
    RouterBackendType type;
    bool              available;       // Hardware detected and init succeeded
    bool              enabled;         // User toggle (can be force-disabled)
    uint32_t          thermalLimitC;   // Max temperature before cascade
    uint32_t          currentTempC;    // Last polled temperature
    uint64_t          totalDispatches; // Lifetime dispatch count
    uint64_t          totalFallbacks;  // Times we fell through to this backend
    uint64_t          totalBytesProcessed;
    uint64_t          totalComputeMs;
    double            avgLatencyMs;    // Rolling average
    const char*       backendName;     // Human-readable name

    BackendState()
        : type(RouterBackendType::None), available(false), enabled(true)
        , thermalLimitC(90), currentTempC(0)
        , totalDispatches(0), totalFallbacks(0)
        , totalBytesProcessed(0), totalComputeMs(0)
        , avgLatencyMs(0), backendName("Unknown")
    {}
};

// ============================================================================
// Router Statistics
// ============================================================================

struct RouterStats {
    std::atomic<uint64_t> totalSubmissions{0};
    std::atomic<uint64_t> totalSuccesses{0};
    std::atomic<uint64_t> totalFailures{0};
    std::atomic<uint64_t> totalFallbacks{0};       // Primary backend unavailable
    std::atomic<uint64_t> thermalThrottleEvents{0}; // Backend too hot
    std::atomic<uint64_t> totalCPUFallbacks{0};     // All GPUs failed
    std::atomic<uint64_t> cerebrasDispatches{0};    // Remote execution count
    std::atomic<uint64_t> totalComputeMs{0};
    std::atomic<uint64_t> totalNetworkMs{0};        // Cerebras network time
    double peakLocalTFLOPS;
    double peakRemoteTFLOPS;
};

// ============================================================================
// Callback Types (function pointers only)
// ============================================================================

// Called when active backend changes (thermal cascade, forced switch)
typedef void (*RouterBackendChangeCallback)(RouterBackendType oldBackend,
                                            RouterBackendType newBackend,
                                            const char* reason,
                                            void* userData);

// Called on dispatch failure (before fallback attempt)
typedef void (*RouterFailureCallback)(RouterBackendType failedBackend,
                                       const char* errorMsg,
                                       int errorCode,
                                       void* userData);

// Called when thermal threshold is crossed
typedef void (*RouterThermalCallback)(RouterBackendType backend,
                                       uint32_t tempC,
                                       uint32_t limitC,
                                       void* userData);

// ============================================================================
// AcceleratorRouter — Singleton Unified Dispatch
// ============================================================================

class AcceleratorRouter {
public:
    static AcceleratorRouter& instance();

    // ===== Lifecycle =====
    RouterResult initialize();
    void         shutdown();
    bool         isInitialized() const { return m_initialized.load(std::memory_order_acquire); }

    // ===== Core Dispatch =====
    // Submit a task — router picks the best backend automatically
    RouterResult submitInference(const RouterInferenceTask& task);

    // Submit with explicit backend (bypasses auto-selection)
    RouterResult submitTo(RouterBackendType backend, const RouterInferenceTask& task);

    // ===== Backend Management =====
    RouterBackendType getActiveBackend() const { return m_activeBackend.load(std::memory_order_acquire); }
    RouterResult      forceBackend(RouterBackendType type);
    RouterResult      enableBackend(RouterBackendType type);
    RouterResult      disableBackend(RouterBackendType type);
    bool              isBackendAvailable(RouterBackendType type) const;
    bool              isBackendEnabled(RouterBackendType type) const;
    const char*       getBackendName(RouterBackendType type) const;

    // ===== Discovery =====
    uint32_t getAvailableBackendCount() const;
    void     getAvailableBackends(RouterBackendType* outTypes, uint32_t maxCount, uint32_t& outCount) const;

    // ===== Thermal Management =====
    RouterResult setThermalLimit(RouterBackendType type, uint32_t maxTempC);
    uint32_t     getThermalLimit(RouterBackendType type) const;
    RouterResult pollThermals(); // Update all backend temperatures

    // ===== Integration Hooks =====
    // Called by subsystems to check if a particular scope should use GPU
    bool shouldAccelerate(DispatchScope scope) const;
    bool shouldAccelerate(DispatchScope scope, uint64_t dataBytes) const;
    // Get the recommended backend for a scope + data size combo
    RouterBackendType recommendBackend(DispatchScope scope, uint64_t dataBytes,
                                        DispatchPriority priority) const;

    // ===== Callbacks =====
    void setBackendChangeCallback(RouterBackendChangeCallback cb, void* userData);
    void setFailureCallback(RouterFailureCallback cb, void* userData);
    void setThermalCallback(RouterThermalCallback cb, void* userData);

    // ===== Stats & JSON =====
    const RouterStats& getStats() const { return m_stats; }
    void               resetStats();
    std::string        toJson() const;
    std::string        backendsToJson() const;
    std::string        thermalToJson() const;

private:
    AcceleratorRouter();
    ~AcceleratorRouter();
    AcceleratorRouter(const AcceleratorRouter&) = delete;
    AcceleratorRouter& operator=(const AcceleratorRouter&) = delete;

    // ===== Internal Dispatch =====
    RouterResult dispatchToAMD(const RouterInferenceTask& task);
    RouterResult dispatchToIntel(const RouterInferenceTask& task);
    RouterResult dispatchToARM64GPU(const RouterInferenceTask& task);
    RouterResult dispatchToARM64NPU(const RouterInferenceTask& task);
    RouterResult dispatchToCerebras(const RouterInferenceTask& task);
    RouterResult dispatchToCPU(const RouterInferenceTask& task);

    // ===== Backend Selection Logic =====
    RouterBackendType autoSelectBackend(const RouterInferenceTask& task) const;
    bool              checkThermal(RouterBackendType type) const;
    RouterBackendType cascadeFallback(RouterBackendType failed) const;

    // ===== Backend Probing =====
    void probeAllBackends();
    void probeAMD();
    void probeIntel();
    void probeARM64();
    void probeCerebras();

    // ===== State =====
    std::atomic<bool>              m_initialized{false};
    std::atomic<RouterBackendType> m_activeBackend{RouterBackendType::None};
    std::atomic<RouterBackendType> m_forcedBackend{RouterBackendType::Auto}; // Auto = no force
    mutable std::mutex             m_mutex;

    // Backend states (indexed by RouterBackendType cast to int)
    static constexpr int MAX_BACKENDS = 7;
    BackendState m_backends[MAX_BACKENDS];

    // Minimum data sizes for GPU acceleration (avoid dispatch overhead)
    uint64_t m_localGPUMinBytes;   // AMD/Intel/ARM64: default 4KB
    uint64_t m_remoteMinBytes;     // Cerebras: default 1MB (justify network RTT)
    uint64_t m_npuMinBytes;        // Hexagon NPU: default 4KB

    // Callbacks
    RouterBackendChangeCallback m_backendChangeCb;
    void* m_backendChangeData;
    RouterFailureCallback m_failureCb;
    void* m_failureData;
    RouterThermalCallback m_thermalCb;
    void* m_thermalData;

    // Stats
    RouterStats m_stats;
};

// ============================================================================
// C Bridge (for MASM integration / external callers)
// ============================================================================

extern "C" {
    void*        AccelRouter_Create();
    int          AccelRouter_Init(void* handle);
    void         AccelRouter_Shutdown(void* handle);
    int          AccelRouter_Submit(void* handle, const RouterInferenceTask* task,
                                    RouterResult* result);
    uint32_t     AccelRouter_GetActiveBackend(void* handle);
    void         AccelRouter_ForceBackend(void* handle, uint32_t backendType);
    int          AccelRouter_IsBackendAvailable(void* handle, uint32_t backendType);
    void         AccelRouter_GetStatsJson(void* handle, char* outJson, uint32_t maxLen);
}

#endif // ACCELERATOR_ROUTER_H
