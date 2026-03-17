// ============================================================================
// dual_engine_inference.h — Phase 1: 800B Dual-Engine Inference Header
// ============================================================================
// Enterprise-gated dual-engine inference system for 800B+ parameter models.
// Splits model across two engine shards (CPU + GPU, or CPU + CPU) with
// coordinated inference pipeline.
//
// Architecture:
//   DualEngineManager
//    ├─ EngineShard[0]  (primary — layers 0..N/2)
//    ├─ EngineShard[1]  (secondary — layers N/2..N)
//    ├─ TensorTransfer  (inter-shard tensor relay)
//    └─ SchedulePolicy  (round-robin, pipeline, adaptive)
//
// License: Requires Enterprise tier (FeatureID::DualEngine800B)
// PATTERN:  No exceptions. Returns DualEngineResult status codes.
// THREADING: Internal mutex. Thread-safe API.
// ============================================================================

#pragma once

#include <cstdint>
#include <cstddef>
#include <mutex>

namespace RawrXD::DualEngine {

// ============================================================================
// Engine Backend Type
// ============================================================================
enum class EngineBackend : uint32_t {
    CPU         = 0,
    CUDA        = 1,
    HIP         = 2,
    Vulkan      = 3,
    DirectML    = 4,
    COUNT
};

// ============================================================================
// Schedule Policy — how shards coordinate
// ============================================================================
enum class SchedulePolicy : uint32_t {
    RoundRobin  = 0,    // Alternate layers between shards
    Pipeline    = 1,    // Pipeline-parallel: shard 0 feeds shard 1
    Adaptive    = 2,    // Dynamic based on latency measurements
    COUNT
};

// ============================================================================
// Engine Shard — one half of the dual-engine
// ============================================================================
struct EngineShard {
    uint32_t        id;
    EngineBackend   backend;
    uint32_t        layerStart;
    uint32_t        layerEnd;
    uint64_t        memoryUsed;     // Bytes allocated
    uint64_t        memoryBudget;   // Max bytes allowed
    float           lastLatencyMs;  // Last inference latency
    bool            active;
    const char*     deviceName;
};

// ============================================================================
// Dual Engine Result — non-exception return type
// ============================================================================
struct DualEngineResult {
    bool        success;
    const char* detail;
    int         errorCode;

    static DualEngineResult ok(const char* msg = "OK") {
        return { true, msg, 0 };
    }
    static DualEngineResult error(const char* msg, int code = -1) {
        return { false, msg, code };
    }
};

// ============================================================================
// Inference Stats — per-inference telemetry
// ============================================================================
struct DualEngineStats {
    float       totalLatencyMs;
    float       shard0LatencyMs;
    float       shard1LatencyMs;
    float       transferLatencyMs;
    uint64_t    tokensGenerated;
    float       tokensPerSecond;
    uint64_t    totalMemoryUsed;
};

// ============================================================================
// Dual Engine Config
// ============================================================================
struct DualEngineConfig {
    EngineBackend   shard0Backend;
    EngineBackend   shard1Backend;
    SchedulePolicy  policy;
    uint64_t        shard0MemBudget;    // 0 = auto
    uint64_t        shard1MemBudget;    // 0 = auto
    float           splitRatio;         // 0.5 = even split
    bool            enableTelemetry;
};

/// Default config: CPU+CPU, pipeline parallel, even split
inline DualEngineConfig defaultConfig() {
    return {
        EngineBackend::CPU,
        EngineBackend::CPU,
        SchedulePolicy::Pipeline,
        0, 0, 0.5f, true
    };
}

// ============================================================================
// Dual Engine Manager — Enterprise-gated singleton
// ============================================================================
class DualEngineManager {
public:
    static DualEngineManager& Instance();

    // Non-copyable
    DualEngineManager(const DualEngineManager&) = delete;
    DualEngineManager& operator=(const DualEngineManager&) = delete;

    // ── Lifecycle ──
    DualEngineResult initialize(const DualEngineConfig& config);
    void shutdown();
    bool isInitialized() const { return m_initialized; }

    // ── Model Loading ──
    /// Load a model file for dual-engine inference.
    /// Splits layers across shards according to config.splitRatio.
    DualEngineResult loadModel(const char* ggufPath, uint64_t modelSizeGB);

    /// Unload current model and free shard memory.
    DualEngineResult unloadModel();

    // ── Inference ──
    /// Run inference on the loaded model.
    /// prompt: input text, outBuf: output buffer, outBufLen: buffer size
    /// Returns tokens generated in outTokens.
    DualEngineResult infer(const char* prompt, char* outBuf, size_t outBufLen,
                           uint32_t maxTokens, uint32_t* outTokens);

    // ── Telemetry ──
    DualEngineStats getStats() const;
    void resetStats();

    // ── Shard Access ──
    const EngineShard& getShard(uint32_t idx) const;
    SchedulePolicy getPolicy() const;

private:
    DualEngineManager() = default;
    ~DualEngineManager() = default;

    DualEngineResult checkLicenseGate();

    mutable std::mutex  m_mutex;
    bool                m_initialized = false;
    bool                m_modelLoaded = false;
    DualEngineConfig    m_config{};
    EngineShard         m_shards[2]{};
    DualEngineStats     m_stats{};
};

// ============================================================================
// Convenience: backend name
// ============================================================================
inline const char* backendName(EngineBackend b) {
    switch (b) {
        case EngineBackend::CPU:      return "CPU";
        case EngineBackend::CUDA:     return "CUDA";
        case EngineBackend::HIP:      return "HIP";
        case EngineBackend::Vulkan:   return "Vulkan";
        case EngineBackend::DirectML: return "DirectML";
        default:                      return "Unknown";
    }
}

inline const char* policyName(SchedulePolicy p) {
    switch (p) {
        case SchedulePolicy::RoundRobin: return "RoundRobin";
        case SchedulePolicy::Pipeline:   return "Pipeline";
        case SchedulePolicy::Adaptive:   return "Adaptive";
        default:                         return "Unknown";
    }
}

} // namespace RawrXD::DualEngine
