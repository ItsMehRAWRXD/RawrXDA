// ============================================================================
// agentic_autonomous_config.h — Agentic Autonomous Operations & Model Selection
// ============================================================================
//
// Central configuration for:
//   - Agentic Operation Mode: Agent | Plan | Debug | Ask (Cursor-style: select mode)
//   - Model Selection: Auto | MAX | Use Multiple Models (1x–99x)
//   - Global parallel cap: 1–99 models in parallel
//   - Cycle Agent Counter: 1x–99x (repetition multiplier for agent cycles)
//   - Quality/Speed: Auto balance | MAX MODE | Use Multiple Models (1x–99x) | Cycle Agent Count (1x–99x)
//   - Terminal/PWSH time limit: tie-in for configurable/random/auto-adjusted per run (see IDEConfig)
//   - Production readiness audit: estimate iteration redo count, top-N difficulty, no token/time/complexity constraints when requested
//
// Used by: main REPL, complete_server API, SubAgentManager (swarm/chain), Win32 IDE, Audit Dashboard.
// ============================================================================

#pragma once

#include <cstdint>
#include <string>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace RawrXD {

// ============================================================================
// Agentic Autonomous Operation Mode
// ============================================================================
enum class AgenticOperationMode : uint8_t {
    Agent  = 0,   // Full agent: plan + act + tools
    Plan   = 1,   // Planning only: decompose and suggest steps
    Debug  = 2,   // Debug focus: analyze failures, suggest fixes
    Ask    = 3,   // Q&A: answer questions, no tool execution
};

inline const char* to_string(AgenticOperationMode m) {
    switch (m) {
        case AgenticOperationMode::Agent: return "Agent";
        case AgenticOperationMode::Plan:  return "Plan";
        case AgenticOperationMode::Debug: return "Debug";
        case AgenticOperationMode::Ask:   return "Ask";
    }
    return "Agent";
}

inline AgenticOperationMode agentic_operation_mode_from_string(const std::string& s) {
    if (s == "Plan" || s == "plan") return AgenticOperationMode::Plan;
    if (s == "Debug" || s == "debug") return AgenticOperationMode::Debug;
    if (s == "Ask" || s == "ask") return AgenticOperationMode::Ask;
    return AgenticOperationMode::Agent;
}

// ============================================================================
// Model Selection Mode
// ============================================================================
enum class ModelSelectionMode : uint8_t {
    Auto              = 0,   // Single model, auto-selected
    MAX               = 1,   // MAX mode: best capability single model
    UseMultipleModels = 2,   // Multiple models; per-model count 1–99, total cap 99
};

inline const char* to_string(ModelSelectionMode m) {
    switch (m) {
        case ModelSelectionMode::Auto:              return "Auto";
        case ModelSelectionMode::MAX:               return "MAX";
        case ModelSelectionMode::UseMultipleModels: return "UseMultipleModels";
    }
    return "Auto";
}

inline ModelSelectionMode model_selection_mode_from_string(const std::string& s) {
    if (s == "MAX" || s == "max") return ModelSelectionMode::MAX;
    if (s == "UseMultipleModels" || s == "multiple" || s == "multi") return ModelSelectionMode::UseMultipleModels;
    return ModelSelectionMode::Auto;
}

// ============================================================================
// Constants — Production: 1x–99x (quantum-grade limits)
// ============================================================================
constexpr int kMaxModelsInParallel = 99;
constexpr int kMinPerModelInstances = 1;
constexpr int kMaxPerModelInstances = 99;
constexpr int kMinCycleAgentCounter = 1;
constexpr int kMaxCycleAgentCounter = 99;

// ============================================================================
// Quality/Speed Balance Mode — Auto-balance or explicit bias
// ============================================================================
enum class QualitySpeedBalance : uint8_t {
    Auto          = 0,   // Balance quality and speed automatically per task
    QualityBias   = 1,   // Favor quality (deeper reasoning, more iterations)
    SpeedBias     = 2,   // Favor speed (faster response, lighter passes)
    MAX_MODE      = 3,   // Maximum quality (no compromise)
};

inline const char* to_string(QualitySpeedBalance q) {
    switch (q) {
        case QualitySpeedBalance::Auto:        return "Auto";
        case QualitySpeedBalance::QualityBias: return "QualityBias";
        case QualitySpeedBalance::SpeedBias:   return "SpeedBias";
        case QualitySpeedBalance::MAX_MODE:    return "MAX_MODE";
    }
    return "Auto";
}

inline QualitySpeedBalance quality_speed_balance_from_string(const std::string& s) {
    if (s == "QualityBias" || s == "quality") return QualitySpeedBalance::QualityBias;
    if (s == "SpeedBias"   || s == "speed")   return QualitySpeedBalance::SpeedBias;
    if (s == "MAX_MODE"    || s == "max")     return QualitySpeedBalance::MAX_MODE;
    return QualitySpeedBalance::Auto;
}

// ============================================================================
// AgenticAutonomousConfig — Singleton
// ============================================================================
class AgenticAutonomousConfig {
public:
    static AgenticAutonomousConfig& instance();

    // ---- Agentic Operation Mode (Agent / Plan / Debug / Ask) ----
    AgenticOperationMode getOperationMode() const;
    void setOperationMode(AgenticOperationMode mode);
    bool setOperationModeFromString(const std::string& s);

    // ---- Model Selection (Auto / MAX / Use Multiple Models) ----
    ModelSelectionMode getModelSelectionMode() const;
    void setModelSelectionMode(ModelSelectionMode mode);
    bool setModelSelectionModeFromString(const std::string& s);

    // Per-model instance count (1–99) when UseMultipleModels is on
    int getPerModelInstanceCount() const;
    void setPerModelInstanceCount(int count);  // clamped to [1, 99]

    // Global cap for models in parallel (1–99)
    int getMaxModelsInParallel() const;
    void setMaxModelsInParallel(int cap);     // clamped to [1, 99]

    // Cycle Agent Counter (1x–99x): repetition multiplier for agent cycles
    int getCycleAgentCounter() const;
    void setCycleAgentCounter(int count);      // clamped to [1, 99]

    // Quality/Speed Balance: Auto | QualityBias | SpeedBias | MAX_MODE
    QualitySpeedBalance getQualitySpeedBalance() const;
    void setQualitySpeedBalance(QualitySpeedBalance q);
    bool setQualitySpeedBalanceFromString(const std::string& s);

    // Estimate iterations for "write what you want" — audit-driven redo count (1–99).
    // When noTokenOrTimeConstraints is true, uses MAX_MODE-style depth without reducing by constraints.
    int estimateIterationRedoCount(const std::string& taskDescription, int complexityHint = 0,
                                   bool noTokenOrTimeConstraints = false) const;

    // Recommended terminal/PWSH requirement hint for IDEConfig.getTerminalTimeoutMs(isAgentic, hint).
    // Returns "default" | "agentic" | "audit" | "quick" so caller can resolve timeout per run.
    std::string getRecommendedTerminalRequirementHint() const;

    // Production readiness audit: estimated number of model round-trips to audit codebase and
    // execute top N most difficult tasks (no simplification of automation/agenticness/logic).
    // codebaseHint: e.g. "full", "ship", "src" for scope; topNDifficult: e.g. 20.
    // Fills estimatedIterationRedos (total redo/round-trip count) and optionally taskCategoryCount.
    void estimateProductionAuditIterations(const std::string& codebaseHint, int topNDifficult,
                                           int* estimatedIterationRedos,
                                           int* taskCategoryCount = nullptr) const;

    // Per-model override: modelId -> instance count (1–99). Empty = use default.
    int getInstanceCountForModel(const std::string& modelId) const;
    void setInstanceCountForModel(const std::string& modelId, int count);
    void clearModelInstanceOverrides();

    // Effective max parallel for swarms: min(requested, getMaxModelsInParallel())
    int effectiveMaxParallel(int requestedMaxParallel) const;

    // Serialization (for REPL/API display and optional persistence)
    std::string toJson() const;
    std::string toDisplayString() const;
    // Load from JSON (keys: operationMode, modelSelectionMode, qualitySpeedBalance,
    // perModelInstances, maxModelsInParallel, cycleAgentCounter, modelInstanceOverrides). Returns true if parsed.
    bool fromJson(const std::string& json);

private:
    AgenticAutonomousConfig();
    mutable std::mutex m_mutex;
    AgenticOperationMode m_operationMode{AgenticOperationMode::Agent};
    ModelSelectionMode m_modelSelectionMode{ModelSelectionMode::Auto};
    QualitySpeedBalance m_qualitySpeedBalance{QualitySpeedBalance::Auto};
    int m_perModelInstanceCount{1};
    int m_maxModelsInParallel{kMaxModelsInParallel};
    int m_cycleAgentCounter{1};
    std::unordered_map<std::string, int> m_modelInstanceOverrides;
};

} // namespace RawrXD
