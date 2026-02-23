// ============================================================================
// dual_engine_inference.h — Phase 1: 800B Dual-Engine Inference Manager
// ============================================================================
// Public header for the DualEngineManager singleton — an Enterprise-tier
// multi-shard inference orchestrator for 800B+ parameter models.
//
// Architecture:
//   DualEngineManager::Instance()
//    ├─ License Gate (EnterpriseLicenseV2::gate → DualEngine800B)
//    ├─ Shard Discovery (multi-file GGUF detection)
//    ├─ Memory Budget (tier-aware allocation limits)
//    └─ Inference Dispatch (round-robin or pipeline parallel)
//
// Usage:
//   #include "dual_engine_inference.h"
//
//   auto& engine = RawrXD::DualEngineManager::Instance();
//   if (engine.initialize()) {
//       engine.loadModel("path/to/800B-model");
//       std::string result = engine.infer("Hello world");
//   }
//
// Enterprise Feature Gate:
//   FeatureID::DualEngine800B  (ID 27, requires Enterprise tier)
//
// PATTERN:   No exceptions. Returns bool/string.
// THREADING: Singleton with std::mutex. Thread-safe.
// RULE:      NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#pragma once

#include "enterprise_license.h"
#include "license_enforcement.h"

#include <string>
#include <vector>
#include <mutex>
#include <cstdint>

namespace RawrXD {

// ============================================================================
// Engine State — lifecycle tracking for dual-engine subsystem
// ============================================================================
enum class EngineState : uint32_t {
    Uninitialized   = 0,
    Initializing    = 1,
    Ready           = 2,
    Loading         = 3,
    Loaded          = 4,
    Inferring       = 5,
    Error           = 6,
    ShuttingDown    = 7
};

// ============================================================================
// Engine Status — snapshot of current engine state
// ============================================================================
struct EngineStatus {
    EngineState     state;
    bool            initialized;
    bool            modelLoaded;
    size_t          shardCount;
    uint64_t        totalModelBytes;
    uint64_t        inferenceCount;
    uint64_t        totalTokensProcessed;
    const char*     lastError;
};

// ============================================================================
// DualEngineConfig — configuration for 800B dual-engine initialization
// ============================================================================
struct DualEngineConfig {
    uint32_t        maxShards           = 16;
    uint32_t        threadPoolSize      = 4;
    uint64_t        memoryBudgetBytes   = 0;    // 0 = use tier limit
    bool            enablePipelineParallel = false;
    bool            enableTensorParallel   = false;
    bool            enableStreamingInfer   = true;
    const char*     backend             = "cpu";    // "cpu", "cuda", "hip"
};

// ============================================================================
// DualEngineManager — Enterprise 800B inference orchestrator
// ============================================================================
class DualEngineManager {
public:
    static DualEngineManager& Instance();

    // Non-copyable
    DualEngineManager(const DualEngineManager&) = delete;
    DualEngineManager& operator=(const DualEngineManager&) = delete;

    // ── License Gate ──
    bool checkLicenseGate();

    // ── Lifecycle ──
    bool initialize();
    bool initialize(const DualEngineConfig& config);
    void shutdown();

    // ── Model Loading ──
    bool loadModel(const std::string& modelPath);
    void unloadModel();

    // ── Inference ──
    std::string infer(const std::string& prompt);

    // ── Status Queries ──
    bool isInitialized() const;
    bool isModelLoaded() const;
    size_t shardCount() const;
    uint64_t totalModelBytes() const;
    uint64_t inferenceCount() const;
    const std::string& lastError() const;
    EngineStatus getStatus() const;
    EngineState currentState() const;

private:
    DualEngineManager() = default;
    ~DualEngineManager() = default;

    std::mutex              m_mutex;
    bool                    m_initialized = false;
    bool                    m_modelLoaded = false;
    EngineState             m_state = EngineState::Uninitialized;
    DualEngineConfig        m_config;
    std::vector<std::string> m_shardPaths;
    uint64_t                m_totalModelBytes = 0;
    uint64_t                m_inferenceCount = 0;
    uint64_t                m_totalTokensProcessed = 0;
    std::string             m_lastError;
};

} // namespace RawrXD
