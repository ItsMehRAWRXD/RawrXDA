// ============================================================================
// RawrXD 10x Dual Engine System
// Each engine = 2 CLI features, fused via Beaconism backend
// Zero external dependencies — pure Win32 + STL
// ============================================================================
#pragma once
#ifndef RAWRXD_DUAL_ENGINE_SYSTEM_H
#define RAWRXD_DUAL_ENGINE_SYSTEM_H

#include <cstdint>
#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <functional>
#include <array>
#include <chrono>
#include <thread>

// Forward declarations
struct AppState;

extern "C" {
    __int64 DualEngine_InitAll();
    __int64 DualEngine_ShutdownAll();
    __int64 DualEngine_ExecuteOnAll(uint32_t featureA, const char* args);
    __int64 DualEngine_AllHealthy();
    __int64 DualEngine_SelfDiagnoseAll();
    __int64 DualEngine_DispatchCLI(const char* flag, const char* args);
}

namespace RawrXD {

// ============================================================================
// Result Pattern (No Exceptions)
// ============================================================================
struct EngineResult {
    bool success;
    const char* detail;
    int errorCode;

    static EngineResult ok(const char* msg = "OK") { return {true, msg, 0}; }
    static EngineResult error(const char* msg, int code = -1) { return {false, msg, code}; }
};

// ============================================================================
// Engine Identity
// ============================================================================
enum class EngineId : uint32_t {
    // 10 Dual Engines (each has 2 CLI features = 20 features total)
    InferenceOptimizer   = 0,   // CLI: --infer-optimize, --infer-benchmark
    MemoryCompactor      = 1,   // CLI: --mem-compact, --mem-defrag
    ThermalRegulator     = 2,   // CLI: --thermal-regulate, --thermal-profile
    FrequencyScaler      = 3,   // CLI: --freq-scale, --freq-lock
    StorageAccelerator   = 4,   // CLI: --storage-accel, --storage-cache
    NetworkOptimizer     = 5,   // CLI: --net-optimize, --net-compress
    PowerGovernor        = 6,   // CLI: --power-govern, --power-profile
    LatencyReducer       = 7,   // CLI: --latency-reduce, --latency-predict
    ThroughputMaximizer  = 8,   // CLI: --throughput-max, --throughput-sched
    QuantumFusion        = 9,   // CLI: --quantum-fuse, --quantum-entangle
    COUNT = 10
};

const char* EngineIdToString(EngineId id);

// ============================================================================
// CLI Feature Pair (each engine has exactly 2)
// ============================================================================
struct CLIFeaturePair {
    const char* flag_a;
    const char* description_a;
    const char* flag_b;
    const char* description_b;
};

const CLIFeaturePair& GetCLIFeatures(EngineId id);

// ============================================================================
// Engine Telemetry Snapshot
// ============================================================================
struct EngineTelemetry {
    uint64_t invocations;
    uint64_t totalCyclesCost;
    double   avgLatencyMs;
    double   peakLatencyMs;
    double   throughputOpsPerSec;
    float    efficiencyRatio;       // 0.0 - 1.0
    float    thermalContribution;   // Degrees C added
    uint32_t faultCount;
    std::chrono::steady_clock::time_point lastInvocation;
};

// ============================================================================
// Single Dual Engine Interface
// ============================================================================
class IDualEngine {
public:
    virtual ~IDualEngine() = default;

    virtual EngineId         getId() const = 0;
    virtual const char*      getName() const = 0;
    virtual EngineResult     initialize() = 0;
    virtual EngineResult     shutdown() = 0;
    virtual EngineResult     executeFeatureA(const std::string& args) = 0;
    virtual EngineResult     executeFeatureB(const std::string& args) = 0;
    virtual EngineTelemetry  getTelemetry() const = 0;
    virtual bool             isHealthy() const = 0;
    virtual EngineResult     selfDiagnose() = 0;
};

// ============================================================================
// Concrete Engine Implementations (forward decl)
// ============================================================================
class InferenceOptimizerEngine;
class MemoryCompactorEngine;
class ThermalRegulatorEngine;
class FrequencyScalerEngine;
class StorageAcceleratorEngine;
class NetworkOptimizerEngine;
class PowerGovernorEngine;
class LatencyReducerEngine;
class ThroughputMaximizerEngine;
class QuantumFusionEngine;

// ============================================================================
// 10x Dual Engine Coordinator
// ============================================================================
class DualEngineCoordinator {
public:
    static DualEngineCoordinator& Instance();

    EngineResult initializeAll();
    EngineResult shutdownAll();

    // Individual engine access
    IDualEngine* getEngine(EngineId id);
    const IDualEngine* getEngine(EngineId id) const;

    // Batch operations
    EngineResult executeOnAll(bool featureA, const std::string& args);
    std::vector<EngineTelemetry> getAllTelemetry() const;

    // Health monitoring
    bool allHealthy() const;
    std::vector<EngineId> getUnhealthyEngines() const;
    EngineResult selfDiagnoseAll();

    // CLI dispatch
    EngineResult dispatchCLI(const std::string& flag, const std::string& args);

private:
    DualEngineCoordinator();
    ~DualEngineCoordinator();
    DualEngineCoordinator(const DualEngineCoordinator&) = delete;
    DualEngineCoordinator& operator=(const DualEngineCoordinator&) = delete;

    std::array<IDualEngine*, static_cast<size_t>(EngineId::COUNT)> engines_{};
    mutable std::mutex mutex_;
    bool initialized_ = false;
};

} // namespace RawrXD
#endif // RAWRXD_DUAL_ENGINE_SYSTEM_H
