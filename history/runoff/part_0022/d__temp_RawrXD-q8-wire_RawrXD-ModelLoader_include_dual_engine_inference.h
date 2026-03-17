// dual_engine_inference.h - 800B Dual-Engine Inference System
// Phase 1: Stub + Interface for dual-engine model loading
// License gate is TOGGLEABLE: when disabled, dual-engine is fully usable
// without any Enterprise license. When enabled, auto-bridges to key
// creation for seamless Enterprise beacon activation.
// No exceptions. PatchResult-style returns.

#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <cstdint>
#include <functional>
#include "enterprise_license.h"

// ============================================================================
// Dual-Engine Configuration
// ============================================================================
struct DualEngineConfig {
    std::string primaryModelPath;    // First engine model (e.g., first 400B shard)
    std::string secondaryModelPath;  // Second engine model (e.g., second 400B shard)
    
    uint32_t primaryGPUIndex = 0;    // GPU for primary engine
    uint32_t secondaryGPUIndex = 1;  // GPU for secondary engine
    
    bool enableTensorParallelism = true;
    bool enablePipelineParallelism = false;
    uint32_t tensorParallelDegree = 2;
    
    size_t primaryVRAMBudgetMB = 0;   // 0 = auto
    size_t secondaryVRAMBudgetMB = 0;
    
    uint32_t maxBatchSize = 1;
    uint32_t maxSequenceLength = 4096;
    
    float primaryTemperature = 0.7f;
    float secondaryTemperature = 0.7f;
    
    bool syncInference = true;        // Synchronize between engines
    uint32_t syncIntervalLayers = 8;  // Sync every N transformer layers

    // ── License Gate Toggle ──
    // When false (default): dual-engine is fully usable without Enterprise license
    // When true: auto-bridges to EnterpriseLicenseManager::createLicenseKey()
    //            to beacon the system with a valid Enterprise key, then proceeds
    bool requireEnterpriseLicense = false;

    // Auto-beacon config (used when requireEnterpriseLicense == true)
    std::string licensee = "RawrXD-DualEngine-AutoBeacon";
    std::string licenseEmail = "auto@rawrxd.local";
    uint64_t licenseValidDays = 365;
    std::string licenseOutputPath;   // Empty = in-memory only, no disk write
};

// ============================================================================
// Engine Status
// ============================================================================
enum class EngineState : uint8_t {
    Uninitialized = 0,
    Loading,
    Ready,
    Running,
    Paused,
    Error,
    Shutdown
};

inline const char* EngineStateToString(EngineState s) {
    switch (s) {
        case EngineState::Uninitialized: return "Uninitialized";
        case EngineState::Loading:       return "Loading";
        case EngineState::Ready:         return "Ready";
        case EngineState::Running:       return "Running";
        case EngineState::Paused:        return "Paused";
        case EngineState::Error:         return "Error";
        case EngineState::Shutdown:      return "Shutdown";
        default:                         return "Unknown";
    }
}

struct EngineStatus {
    EngineState state = EngineState::Uninitialized;
    std::string modelName;
    size_t loadedParametersMB = 0;
    size_t vramUsedMB = 0;
    float tokensPerSecond = 0.0f;
    uint64_t totalTokensGenerated = 0;
    uint32_t gpuIndex = 0;
    float gpuUtilization = 0.0f;
    float gpuTemperature = 0.0f;
};

// ============================================================================
// Dual-Engine Inference Manager
// ============================================================================
class DualEngineInferenceManager {
public:
    static DualEngineInferenceManager& getInstance();

    // Lifecycle
    LicenseResult initialize(const DualEngineConfig& config);
    LicenseResult shutdown();

    // Engine control
    LicenseResult loadPrimaryModel(const std::string& modelPath);
    LicenseResult loadSecondaryModel(const std::string& modelPath);
    LicenseResult unloadAll();

    // Inference
    struct InferenceRequest {
        std::string prompt;
        uint32_t maxTokens = 512;
        float temperature = 0.7f;
        float topP = 0.9f;
        bool stream = true;
    };

    struct InferenceResponse {
        bool success = false;
        std::string output;
        uint32_t tokensGenerated = 0;
        float latencyMs = 0.0f;
        float tokensPerSecond = 0.0f;
        std::string errorDetail;
    };

    LicenseResult runInference(const InferenceRequest& request, InferenceResponse& response);

    // License gate control
    LicenseResult setEnterpriseGateEnabled(bool enabled);
    bool isEnterpriseGateEnabled() const;
    LicenseResult beaconEnterpriseLicense();  // Manually trigger key creation bridge

    // Status
    EngineStatus getPrimaryStatus() const;
    EngineStatus getSecondaryStatus() const;
    bool isDualEngineActive() const;
    bool is800BModelLoaded() const;
    bool isLicenseBeaconed() const;           // True if auto-beacon succeeded
    std::string getStatusSummary() const;

    // Callbacks
    using TokenCallback = std::function<void(const std::string& token)>;
    void setTokenCallback(TokenCallback cb);
    using StatusCallback = std::function<void(EngineState primary, EngineState secondary)>;
    void setStatusCallback(StatusCallback cb);

    ~DualEngineInferenceManager();
    DualEngineInferenceManager(const DualEngineInferenceManager&) = delete;
    DualEngineInferenceManager& operator=(const DualEngineInferenceManager&) = delete;

private:
    DualEngineInferenceManager();

    // Internal: check license gate — returns ok() if gate disabled or license valid
    LicenseResult checkLicenseGate(EnterpriseFeature feature);

    DualEngineConfig m_config;
    EngineStatus m_primaryStatus;
    EngineStatus m_secondaryStatus;
    mutable std::mutex m_mutex;
    std::atomic<bool> m_initialized{false};
    std::atomic<bool> m_enterpriseGateEnabled{false};
    std::atomic<bool> m_licenseBeaconed{false};
    TokenCallback m_tokenCb;
    StatusCallback m_statusCb;
};
