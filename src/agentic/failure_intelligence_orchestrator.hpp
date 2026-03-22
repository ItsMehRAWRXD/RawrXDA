// failure_intelligence_orchestrator.hpp
// Real-time failure detection, root cause analysis, and autonomous recovery orchestration
// Complements AgenticPlanningOrchestrator with execution monitoring and adaptive recovery

#pragma once


#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

namespace Agentic {

// ============================================================================
// Failure Classification & Severity
// ============================================================================

enum class FailureCategory : uint8_t {
    Transient = 1,          // Temporary network/resource issue (retry likely helps)
    Logic = 2,              // Code error / incorrect logic (needs code change)
    Timeout = 3,            // Exceeded time/resource limits (optimize or retry with more resources)
    Dependency = 4,         // Missing tool/library/service (resolve external dependency)
    Permission = 5,         // Access denied / security policy (elevation needed)
    Configuration = 6,      // Config/state mismatch (reset state/reconfigure)
    Environmental = 7,      // System state issue (cleanup, restart, or escalate)
    Fatal = 8               // Unrecoverable error (escalate to human)
};

enum class SeverityLevel : uint8_t {
    Info = 1,
    Warning = 2,
    Error = 3,
    Critical = 4,
    Fatal = 5
};

enum class RecoveryStrategy : uint8_t {
    Retry = 1,              // Retry same action (with optional backoff)
    Fallback = 2,           // Use alternative implementation/tool
    Compensate = 3,         // Execute compensating action (rollback+alternative)
    Escalate = 4,           // Require human intervention
    Abort = 5,              // Terminate step/plan
    ReplanAndRecontinue = 6 // Regenerate plan from current state
};

enum class RecoveryStatus : uint8_t {
    Pending = 1,
    InProgress = 2,
    Succeeded = 3,
    Failed = 4,
    Skipped = 5,
    Escalated = 6
};

// ============================================================================
// Failure Signal & Analysis
// ============================================================================

struct FailureSignal {
    std::string signal_id;
    std::string source_component;           // "executor", "tool", "subprocess", "network", etc.
    std::string step_id;                    // From ExecutionPlan::PlanStep
    std::string action_descriptor;          // Command or action that failed
    
    int exit_code = 0;                      // Process exit code (if applicable)
    uint64_t duration_ms = 0;               // How long before failure
    uint64_t timeout_ms = 0;                // Expected timeout (0 if unlimited)
    
    std::string error_message;              // Raw error text
    std::string stdout_output;              // Captured stdout (may be very long)
    std::string stderr_output;              // Captured stderr
    
    FailureCategory category = FailureCategory::Logic;
    SeverityLevel severity = SeverityLevel::Error;
    
    float confidence_score = 0.8f;          // Planner's confidence in categorization
    std::chrono::system_clock::time_point detected_at;
    
    nlohmann::json toJson() const;
};

struct RootCauseAnalysis {
    std::string analysis_id;
    std::string failure_signal_id;
    FailureCategory primary_category;
    SeverityLevel assessed_severity;
    
    std::vector<std::string> identified_patterns;    // Regex patterns / keywords matched
    std::vector<std::string> probable_causes;        // Human-readable cause hypotheses
    std::string root_cause_description;              // Most likely root cause
    
    int pattern_match_count = 0;
    float analysis_confidence = 0.8f;                // How confident is the analysis?
    std::vector<RecoveryStrategy> recommended_recoveries;  // Ranked by likelihood
    
    std::chrono::system_clock::time_point analyzed_at;
    nlohmann::json toJson() const;
};

struct RecoveryPlan {
    std::string recovery_id;
    std::string failure_signal_id;
    std::string root_cause_analysis_id;
    
    RecoveryStrategy strategy;
    std::string strategy_description;
    std::vector<std::string> recovery_steps;       // Ordered sequence of actions
    std::map<std::string, std::string> recovery_params; // Strategy-specific config
    
    int max_retry_attempts = 3;
    int current_attempt = 0;
    uint64_t backoff_delay_ms = 100;               // Exponential backoff
    
    RecoveryStatus status = RecoveryStatus::Pending;
    std::string status_reason;
    
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point executed_at;
    std::chrono::system_clock::time_point completed_at;
    
    nlohmann::json toJson() const;
};

// ============================================================================
// Failure Intelligence Engine
// ============================================================================

class FailureIntelligenceOrchestrator {
public:
    using FailureDetectedCallback = std::function<void(const FailureSignal&)>;
    using RecoveryInitiatedCallback = std::function<void(const RecoveryPlan&)>;
    using RecoveryCompletedCallback = std::function<void(const RecoveryPlan&, bool success)>;
    using AnalysisLogFn = std::function<void(const std::string& log_entry)>;
    
    // Recovery executor: implements the actual recovery action
    using RecoveryExecutorFn = std::function<bool(const RecoveryPlan&, std::string& output)>;

    FailureIntelligenceOrchestrator();
    ~FailureIntelligenceOrchestrator();

    // ── Failure Reporting ──────────────────────────────────────────────────
    /// Report a failure signal (typically called by execution environment)
    void reportFailure(const FailureSignal& signal);
    
    /// Analyze a failure and generate root cause analysis
    RootCauseAnalysis* analyzeFailure(const FailureSignal& signal);
    
    /// Generate recovery plan for a failure
    RecoveryPlan* generateRecoveryPlan(
        const FailureSignal& signal,
        const RootCauseAnalysis* analysis = nullptr);
    
    /// Execute a recovery plan
    bool executeRecovery(RecoveryPlan* plan, std::string& output);
    
    /// Autonomous recovery: detect → analyze → recover (all steps)
    bool autonomousRecover(const FailureSignal& signal, std::string& output);

    // ── Pattern Recognition ────────────────────────────────────────────────
    /// Train on historical failures (updates pattern database)
    void learnFromFailure(const FailureSignal& signal, FailureCategory actual_category);
    
    /// Get statistics on failure patterns
    struct FailureStats {
        std::map<FailureCategory, int> category_counts;
        std::map<RecoveryStrategy, int> recovery_success_counts;
        int total_failures_seen = 0;
        int total_recoveries_attempted = 0;
        int total_recoveries_succeeded = 0;
        float overall_recovery_success_rate = 0.0f;
    };
    FailureStats getFailureStatistics() const;

    // ── Configuration & Callbacks ──────────────────────────────────────────
    void setFailureDetectedCallback(FailureDetectedCallback fn)       { m_failureCallback = fn; }
    void setRecoveryInitiatedCallback(RecoveryInitiatedCallback fn)   { m_recoveryStartCallback = fn; }
    void setRecoveryCompletedCallback(RecoveryCompletedCallback fn)   { m_recoveryCompleteCallback = fn; }
    void setAnalysisLogFn(AnalysisLogFn fn)                           { m_analyzeLogFn = fn; }
    void setRecoveryExecutor(RecoveryExecutorFn fn)                   { m_recoveryExecutorFn = fn; }
    
    // Policy tuning
    void setAutoRetryThreshold(SeverityLevel max_severity)            { m_autoRetryThreshold = max_severity; }
    void setMaxRecoveryAttempts(int max_attempts)                     { m_maxRecoveryAttempts = max_attempts; }
    void setEscalationThreshold(FailureCategory category)             { m_escalationCategory = category; }

    // ── State Access ───────────────────────────────────────────────────────
    FailureSignal* getFailure(const std::string& signal_id);
    RootCauseAnalysis* getAnalysis(const std::string& analysis_id);
    RecoveryPlan* getRecoveryPlan(const std::string& plan_id);
    
    std::vector<FailureSignal*> getRecentFailures(int count) const;
    std::vector<RecoveryPlan*> getPendingRecoveries() const;
    std::vector<RecoveryPlan*> getFailedRecoveries() const;

    // ── JSON Export for Monitoring ─────────────────────────────────────────
    nlohmann::json getFailureQueueJson() const;
    nlohmann::json getRecoveryQueueJson() const;
    nlohmann::json getStatisticsJson() const;
    nlohmann::json getSystemHealthJson() const;

private:
    struct FailurePattern {
        std::string regex_pattern;          // e.g., "connection refused", "timeout", "permission denied"
        FailureCategory suggested_category;
        float confidence;
        int match_count = 0;
    };

    // ── Internal Analysis ──────────────────────────────────────────────────
    FailureCategory classifyFailure(const FailureSignal& signal) const;
    std::vector<FailurePattern> matchPatterns(const FailureSignal& signal) const;
    RecoveryStrategy selectBestRecovery(const RootCauseAnalysis* analysis) const;
    
    // ── Pattern Database ───────────────────────────────────────────────────
    void initializePatterns();
    std::vector<FailurePattern> m_failurePatterns;

    // ── State ──────────────────────────────────────────────────────────────
    mutable std::mutex m_mutex;
    std::map<std::string, std::unique_ptr<FailureSignal>> m_failureHistory;
    std::map<std::string, std::unique_ptr<RootCauseAnalysis>> m_analyses;
    std::map<std::string, std::unique_ptr<RecoveryPlan>> m_recoveryPlans;
    std::vector<RecoveryPlan*> m_pendingRecoveries;
    
    int m_failureCounter = 0;
    int m_analysisCounter = 0;
    int m_recoveryCounter = 0;

    // ── Statistics ────────────────────────────────────────────────────────
    FailureStats m_stats;

    // ── Configuration ─────────────────────────────────────────────────────
    SeverityLevel m_autoRetryThreshold = SeverityLevel::Error;
    int m_maxRecoveryAttempts = 3;
    FailureCategory m_escalationCategory = FailureCategory::Fatal;

    // ── Callbacks ──────────────────────────────────────────────────────────
    FailureDetectedCallback m_failureCallback;
    RecoveryInitiatedCallback m_recoveryStartCallback;
    RecoveryCompletedCallback m_recoveryCompleteCallback;
    AnalysisLogFn m_analyzeLogFn;
    RecoveryExecutorFn m_recoveryExecutorFn;
};

} // namespace Agentic
