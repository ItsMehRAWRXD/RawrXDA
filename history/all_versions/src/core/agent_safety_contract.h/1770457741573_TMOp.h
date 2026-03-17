// ============================================================================
// agent_safety_contract.h — Phase 10B: Agent Safety Contracts
// ============================================================================
//
// Enforces intent budgets, risk tiers, action classification, and rollback
// guarantees for every agentic operation. This is the trust boundary between
// "what the agent wants to do" and "what the system will allow."
//
// Pattern:  Structured results (PatchResult-style), no exceptions
// Threading: All methods are thread-safe (mutex-guarded)
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#ifndef RAWRXD_AGENT_SAFETY_CONTRACT_H
#define RAWRXD_AGENT_SAFETY_CONTRACT_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>

// ============================================================================
// ENUMS
// ============================================================================

enum class SafetyRiskTier {
    None     = 0,  // Read-only, informational
    Low      = 1,  // Reversible, no side effects (e.g., search, completion)
    Medium   = 2,  // File edits, config changes — reversible with rollback
    High     = 3,  // Process execution, network calls — partially reversible
    Critical = 4   // System modification, model changes — irreversible
};

enum class ActionClass {
    // Read-Only (None/Low risk)
    ReadFile       = 0,
    SearchCode     = 1,
    QueryModel     = 2,
    ListDirectory  = 3,
    InspectState   = 4,

    // Mutating (Medium risk)
    WriteFile      = 10,
    EditFile       = 11,
    CreateFile     = 12,
    DeleteFile     = 13,
    RenameFile     = 14,

    // Execution (High risk)
    RunCommand     = 20,
    RunBuild       = 21,
    RunTest        = 22,
    InstallPackage = 23,
    NetworkRequest = 24,

    // Critical (Critical risk)
    ModifyModel    = 30,
    SystemConfig   = 31,
    ProcessKill    = 32,
    PatchMemory    = 33,
    ServerRestart  = 34,

    Unknown        = 99
};

enum class ContractVerdict {
    Allowed    = 0,  // Action is permitted
    Denied     = 1,  // Action blocked by policy
    Escalated  = 2,  // Requires user confirmation
    Budgeted   = 3,  // Allowed but budget decremented
    Rollback   = 4   // Action allowed but rollback was triggered
};

enum class ViolationType {
    BudgetExceeded     = 0,
    RiskTierBlocked    = 1,
    ActionClassBlocked = 2,
    RateLimitExceeded  = 3,
    ConfidenceTooLow   = 4,
    ManualDeny         = 5,
    SafetyTripwire     = 6,
    SequenceViolation  = 7
};

// ============================================================================
// STRUCTS
// ============================================================================

struct IntentBudget {
    // Maximum operations allowed per category in a session
    int maxFileWrites      = 50;
    int maxFileDeletes     = 10;
    int maxCommandRuns     = 30;
    int maxBuildRuns       = 20;
    int maxNetworkRequests = 100;
    int maxModelModifications = 5;
    int maxProcessKills    = 10;
    int maxTotalActions    = 500;

    // Current consumption
    int usedFileWrites      = 0;
    int usedFileDeletes     = 0;
    int usedCommandRuns     = 0;
    int usedBuildRuns       = 0;
    int usedNetworkRequests = 0;
    int usedModelModifications = 0;
    int usedProcessKills    = 0;
    int usedTotalActions    = 0;

    // Rate limiting
    int maxActionsPerMinute = 60;
    int actionsThisMinute   = 0;
    std::chrono::steady_clock::time_point minuteStart;

    IntentBudget() : minuteStart(std::chrono::steady_clock::now()) {}
};

struct SafetyContractRule {
    ActionClass actionClass;
    SafetyRiskTier maxAllowedRisk;
    bool requireConfirmation;
    bool allowRollback;
    std::string description;
};

struct ContractCheckResult {
    ContractVerdict verdict;
    ViolationType violationType;
    std::string reason;
    std::string suggestion;
    int remainingBudget;
    float riskScore;

    static ContractCheckResult allowed(const std::string& msg, int remaining = -1) {
        ContractCheckResult r;
        r.verdict = ContractVerdict::Allowed;
        r.reason = msg;
        r.remainingBudget = remaining;
        r.riskScore = 0.0f;
        r.violationType = ViolationType::ManualDeny; // unused
        return r;
    }

    static ContractCheckResult denied(ViolationType vt, const std::string& msg, const std::string& suggestion = "") {
        ContractCheckResult r;
        r.verdict = ContractVerdict::Denied;
        r.violationType = vt;
        r.reason = msg;
        r.suggestion = suggestion;
        r.remainingBudget = 0;
        r.riskScore = 1.0f;
        return r;
    }

    static ContractCheckResult escalated(const std::string& msg) {
        ContractCheckResult r;
        r.verdict = ContractVerdict::Escalated;
        r.reason = msg;
        r.remainingBudget = -1;
        r.riskScore = 0.5f;
        r.violationType = ViolationType::ManualDeny;
        return r;
    }

    static ContractCheckResult budgeted(const std::string& msg, int remaining) {
        ContractCheckResult r;
        r.verdict = ContractVerdict::Budgeted;
        r.reason = msg;
        r.remainingBudget = remaining;
        r.riskScore = 0.2f;
        r.violationType = ViolationType::BudgetExceeded;
        return r;
    }
};

struct SafetyViolationRecord {
    uint64_t id;
    ViolationType type;
    ActionClass attemptedAction;
    SafetyRiskTier riskTier;
    std::string description;
    std::string agentId;
    std::chrono::steady_clock::time_point timestamp;
    bool wasEscalated;
    bool userApproved;
};

struct RollbackEntry {
    uint64_t actionId;
    ActionClass actionClass;
    std::string description;
    std::string targetPath;        // File path or resource identifier
    std::string beforeState;       // Serialized pre-state (file content, config, etc.)
    std::function<void()> undoFn;  // Custom rollback function
    std::chrono::steady_clock::time_point timestamp;
    bool executed;
};

struct SafetyStats {
    uint64_t totalChecks       = 0;
    uint64_t totalAllowed      = 0;
    uint64_t totalDenied       = 0;
    uint64_t totalEscalated    = 0;
    uint64_t totalRollbacks    = 0;
    uint64_t totalViolations   = 0;
    uint64_t budgetExceeded    = 0;
    uint64_t riskBlocked       = 0;
    uint64_t rateLimited       = 0;
    SafetyRiskTier maxRiskSeen = SafetyRiskTier::None;
};

// ============================================================================
// AGENT SAFETY CONTRACT — The Enforcer
// ============================================================================

class AgentSafetyContract {
public:
    static AgentSafetyContract& instance();

    // ── Lifecycle ──────────────────────────────────────────────────────
    bool init();
    void shutdown();
    void reset();  // Reset budgets and violations

    // ── Contract Checks ────────────────────────────────────────────────
    ContractCheckResult checkAction(
        ActionClass action,
        SafetyRiskTier risk,
        const std::string& description = "",
        float confidence = 1.0f);

    ContractCheckResult checkAndConsume(
        ActionClass action,
        SafetyRiskTier risk,
        const std::string& description = "",
        float confidence = 1.0f);

    // ── Budget Management ──────────────────────────────────────────────
    IntentBudget getBudget() const;
    void setBudget(const IntentBudget& budget);
    void resetBudget();
    int getRemainingBudget(ActionClass action) const;
    bool isBudgetExhausted(ActionClass action) const;

    // ── Risk Tier Policy ───────────────────────────────────────────────
    void setMaxAllowedRisk(SafetyRiskTier maxRisk);
    SafetyRiskTier getMaxAllowedRisk() const;
    void blockActionClass(ActionClass action);
    void unblockActionClass(ActionClass action);
    bool isActionBlocked(ActionClass action) const;

    // ── Rules ──────────────────────────────────────────────────────────
    void addRule(const SafetyContractRule& rule);
    void removeRule(ActionClass action);
    std::vector<SafetyContractRule> getRules() const;

    // ── Rollback ───────────────────────────────────────────────────────
    uint64_t registerRollback(
        ActionClass action,
        const std::string& description,
        const std::string& targetPath,
        const std::string& beforeState,
        std::function<void()> undoFn = nullptr);

    bool executeRollback(uint64_t actionId);
    bool rollbackLast();
    bool rollbackAll();
    std::vector<RollbackEntry> getRollbackHistory() const;

    // ── Violations ─────────────────────────────────────────────────────
    std::vector<SafetyViolationRecord> getViolations() const;
    void clearViolations();

    // ── Escalation ─────────────────────────────────────────────────────
    void setEscalationCallback(std::function<bool(const std::string&)> cb);
    void setAutoApproveEscalations(bool autoApprove);

    // ── Stats & Reporting ──────────────────────────────────────────────
    SafetyStats getStats() const;
    std::string getStatusString() const;

    // ── Serialization (JSON-compatible) ────────────────────────────────
    std::string serializeBudget() const;
    bool deserializeBudget(const std::string& json);

private:
    AgentSafetyContract();
    ~AgentSafetyContract();

    // Helpers
    SafetyRiskTier getActionDefaultRisk(ActionClass action) const;
    int* getBudgetCounterForAction(ActionClass action);
    const int* getBudgetLimitForAction(ActionClass action) const;
    bool checkRateLimit();
    void recordViolation(ViolationType type, ActionClass action,
                         SafetyRiskTier risk, const std::string& desc);

    mutable std::mutex m_mutex;
    std::atomic<bool> m_initialized;

    IntentBudget m_budget;
    SafetyRiskTier m_maxAllowedRisk;
    SafetyStats m_stats;

    std::vector<SafetyContractRule> m_rules;
    std::vector<SafetyViolationRecord> m_violations;
    std::vector<RollbackEntry> m_rollbackStack;
    std::unordered_map<int, bool> m_blockedActions; // ActionClass int -> blocked

    std::function<bool(const std::string&)> m_escalationCallback;
    bool m_autoApproveEscalations;

    uint64_t m_nextViolationId;
    uint64_t m_nextRollbackId;

    static constexpr int MAX_VIOLATIONS = 10000;
    static constexpr int MAX_ROLLBACK_ENTRIES = 500;
    static constexpr float MIN_CONFIDENCE_THRESHOLD = 0.3f;
};

// ============================================================================
// UTILITY — Risk tier to string
// ============================================================================

inline const char* safetyRiskTierToString(SafetyRiskTier tier) {
    switch (tier) {
        case SafetyRiskTier::None:     return "None";
        case SafetyRiskTier::Low:      return "Low";
        case SafetyRiskTier::Medium:   return "Medium";
        case SafetyRiskTier::High:     return "High";
        case SafetyRiskTier::Critical: return "Critical";
        default:                       return "Unknown";
    }
}

inline const char* actionClassToString(ActionClass ac) {
    switch (ac) {
        case ActionClass::ReadFile:       return "ReadFile";
        case ActionClass::SearchCode:     return "SearchCode";
        case ActionClass::QueryModel:     return "QueryModel";
        case ActionClass::ListDirectory:  return "ListDirectory";
        case ActionClass::InspectState:   return "InspectState";
        case ActionClass::WriteFile:      return "WriteFile";
        case ActionClass::EditFile:       return "EditFile";
        case ActionClass::CreateFile:     return "CreateFile";
        case ActionClass::DeleteFile:     return "DeleteFile";
        case ActionClass::RenameFile:     return "RenameFile";
        case ActionClass::RunCommand:     return "RunCommand";
        case ActionClass::RunBuild:       return "RunBuild";
        case ActionClass::RunTest:        return "RunTest";
        case ActionClass::InstallPackage: return "InstallPackage";
        case ActionClass::NetworkRequest: return "NetworkRequest";
        case ActionClass::ModifyModel:    return "ModifyModel";
        case ActionClass::SystemConfig:   return "SystemConfig";
        case ActionClass::ProcessKill:    return "ProcessKill";
        case ActionClass::PatchMemory:    return "PatchMemory";
        case ActionClass::ServerRestart:  return "ServerRestart";
        case ActionClass::Unknown:        return "Unknown";
        default:                          return "???";
    }
}

inline const char* contractVerdictToString(ContractVerdict v) {
    switch (v) {
        case ContractVerdict::Allowed:   return "Allowed";
        case ContractVerdict::Denied:    return "Denied";
        case ContractVerdict::Escalated: return "Escalated";
        case ContractVerdict::Budgeted:  return "Budgeted";
        case ContractVerdict::Rollback:  return "Rollback";
        default:                         return "Unknown";
    }
}

inline const char* violationTypeToString(ViolationType vt) {
    switch (vt) {
        case ViolationType::BudgetExceeded:     return "BudgetExceeded";
        case ViolationType::RiskTierBlocked:    return "RiskTierBlocked";
        case ViolationType::ActionClassBlocked: return "ActionClassBlocked";
        case ViolationType::RateLimitExceeded:  return "RateLimitExceeded";
        case ViolationType::ConfidenceTooLow:   return "ConfidenceTooLow";
        case ViolationType::ManualDeny:         return "ManualDeny";
        case ViolationType::SafetyTripwire:     return "SafetyTripwire";
        case ViolationType::SequenceViolation:  return "SequenceViolation";
        default:                                return "Unknown";
    }
}

#endif // RAWRXD_AGENT_SAFETY_CONTRACT_H
