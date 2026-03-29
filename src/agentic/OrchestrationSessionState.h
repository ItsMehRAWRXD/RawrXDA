// ============================================================================
// OrchestrationSessionState.h
// Shared Per-Session Orchestration State for All Endpoints
// ============================================================================
// Provides:
// - Intent classification & history (per-session tracking)
// - Speculative tool execution results cache
// - Confidence thresholds & rollback gating
// - Synthesis signal (compact representation of tool results)
// - Metadata telemetry (execution count, success rate)
//
// Used by:
// - Win32IDE::ExecuteAgentCommand (native UI)
// - Win32IDE_LocalServer endpoints (/api/*)
// - complete_server endpoints (/v1/*)
//
// Thread-safe via atomic flags and intent for single-threaded model dispatch.
// ============================================================================

#pragma once

#include <atomic>
#include <chrono>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace RawrXD::Orchestration
{

struct ToolExecutionResult
{
    std::string tool_name;
    std::string args;
    std::string result;
    std::string error;
    bool success = false;
    int64_t duration_ms = 0;
    int retry_count = 0;
};

struct IntentClassification
{
    std::string intent;        // 'search', 'code_gen', 'debug', 'plan', 'refactor', etc.
    float confidence = 0.0f;   // 0.0 - 1.0
    std::vector<std::string> suggested_tools;
    std::string reasoning;     // Why this intent was chosen
};

// Per-session orchestration state (lives for the duration of a chat session or HTTP connection)
class OrchestrationSessionState
{
public:
    static OrchestrationSessionState& instance();

    // ---- Intent Classification ----
    void setCurrentIntent(const IntentClassification& intent);
    IntentClassification getCurrentIntent() const;
    std::vector<IntentClassification> getIntentHistory() const;
    void clearIntentHistory();

    // ---- Tool Execution Results ----
    void recordToolExecution(const ToolExecutionResult& result);
    std::vector<ToolExecutionResult> getRecentToolResults(size_t max_count = 5) const;
    std::string getSynthesisSignal() const;  // Compact representation of recent tool results

    // ---- Orchestration Telemetry ----
    struct OrchestrationMetrics
    {
        std::atomic<uint64_t> orchestrations_executed{0};
        std::atomic<uint64_t> tool_executions_succeeded{0};
        std::atomic<uint64_t> tool_executions_failed{0};
        std::atomic<uint64_t> fallback_count{0};
        float average_confidence = 0.0f;
        std::chrono::steady_clock::time_point session_start;
    };

    OrchestrationMetrics& getMetrics();
    void recordOrchestrationPass(bool success);

    // ---- Confidence & Gating ----
    bool shouldExecuteTools(float model_confidence_threshold = 0.75f) const;
    void setConfidenceThreshold(float threshold);
    float getConfidenceThreshold() const;

    // ---- Session Lifecycle ----
    void reset();
    bool isActive() const { return m_active; }
    void setActive(bool active) { m_active.store(active); }
    std::chrono::steady_clock::duration sessionDuration() const;

private:
    OrchestrationSessionState();
    ~OrchestrationSessionState() = default;

    mutable std::mutex m_mutex;
    std::vector<IntentClassification> m_intent_history;
    std::vector<ToolExecutionResult> m_tool_results;
    OrchestrationMetrics m_metrics;
    float m_confidence_threshold = 0.75f;
    std::atomic<bool> m_active{true};

    // Cached synthesis signal (updated on each tool execution)
    mutable std::string m_synthesis_signal_cache;
    mutable std::chrono::steady_clock::time_point m_last_synthesis_update;
};

}  // namespace RawrXD::Orchestration
