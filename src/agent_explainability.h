#pragma once
// ============================================================================
// agent_explainability.h — Decision Attribution & Causal Trace (Phase 8A)
// ============================================================================
// Read-only explainability layer.  Builds causal traces from existing data:
//   - AgentHistoryRecorder  (Phase 5 events)
//   - PolicyEngine          (Phase 7 policies, heuristics, eval results)
//
// Zero behavioral changes.  No retries, no new policies, no heuristics.
// Pure read → transform → present.
//
// Outputs:
//   1. DecisionTrace     — why an agent / chain / swarm produced its result
//   2. FailureAttribution — why a failure was detected & how correction applied
//   3. PolicyAttribution  — which policies fired and what they changed
//   4. SessionExplanation — full session-level causal narrative
//   5. ExplainabilitySnapshot — JSON export for audits / bug reports / demos
// ============================================================================

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>

// Forward declarations — no new dependencies, only existing engines
class AgentHistoryRecorder;
class PolicyEngine;

// ============================================================================
// DecisionNode — a single step in the causal graph
// ============================================================================
struct DecisionNode {
    int64_t     eventId    = 0;         // References AgentEvent::id
    std::string eventType;              // e.g. "agent_spawn", "chain_step"
    std::string agentId;
    std::string description;
    int64_t     timestampMs = 0;
    int         durationMs  = 0;
    bool        success     = true;
    std::string errorMessage;

    // Causal links
    int64_t     parentEventId = 0;      // What triggered this node
    std::string trigger;                // Human-readable cause

    // Policy attribution (if any policy evaluated for this node)
    std::string policyId;               // Which policy matched (empty = none)
    std::string policyName;
    std::string policyEffect;           // What the policy changed (human-readable)

    std::string toJSON() const;
};

// ============================================================================
// DecisionTrace — ordered causal chain for one agent / chain / swarm
// ============================================================================
struct DecisionTrace {
    std::string traceId;                // Generated UUID
    std::string rootAgentId;            // The top-level agent/chain/swarm
    std::string rootEventType;          // "agent_spawn", "chain_start", "swarm_start"
    std::string sessionId;

    std::vector<DecisionNode> nodes;    // Ordered causally (parent before child)

    // Summary
    bool        overallSuccess = true;
    int         totalDurationMs = 0;
    int         nodeCount      = 0;
    int         failureCount   = 0;
    int         policyFireCount = 0;

    std::string summary;                // One-sentence human-readable explanation
    std::string toJSON() const;
};

// ============================================================================
// FailureAttribution — explains a specific failure and correction path
// ============================================================================
struct FailureAttribution {
    int64_t     failureEventId = 0;     // The event that failed
    std::string agentId;
    std::string failureType;            // e.g. "agent_fail", "chain_fail"
    std::string errorMessage;
    int64_t     timestampMs = 0;

    // What happened next
    bool        wasRetried   = false;
    int64_t     retryEventId = 0;       // Event ID of the retry (if any)
    bool        retrySucceeded = false;
    std::string correctionStrategy;     // "retry", "policy_redirect", "none"

    // Policy context (if a policy influenced the correction)
    std::string policyId;
    std::string policyName;
    std::string policyRationale;

    // Heuristic context (what stats informed the policy)
    float       historicalSuccessRate = -1.0f;  // -1 = no data
    int         historicalOccurrences = 0;

    std::string toJSON() const;
};

// ============================================================================
// PolicyAttribution — explains why a policy fired for a specific operation
// ============================================================================
struct PolicyAttribution {
    std::string policyId;
    std::string policyName;
    std::string policyDescription;
    int         policyPriority = 50;

    // Trigger match details
    std::string triggerEventType;       // What event type matched
    std::string triggerPattern;         // What pattern matched
    float       triggerFailureRate = 0; // Observed failure rate at fire time

    // Effect
    std::string effectDescription;      // Human-readable: "Reduced parallelism by 2"
    bool        redirectedSwarmToChain = false;
    int         timeoutOverrideMs = -1;
    int         maxRetriesOverride = -1;
    bool        addedValidation = false;

    // Confidence
    float       estimatedImprovement = 0.0f;
    int         policyAppliedCount = 0; // Total times this policy has fired

    std::string toJSON() const;
};

// ============================================================================
// SessionExplanation — full session-level narrative
// ============================================================================
struct SessionExplanation {
    std::string sessionId;
    int64_t     startTimestampMs = 0;
    int64_t     endTimestampMs   = 0;
    int         totalEvents      = 0;

    // Aggregated stats
    int         agentSpawns     = 0;
    int         chainExecutions = 0;
    int         swarmExecutions = 0;
    int         failures        = 0;
    int         retries         = 0;
    int         policyFirings   = 0;

    // Top-level traces
    std::vector<DecisionTrace>      traces;
    std::vector<FailureAttribution> failureAttributions;
    std::vector<PolicyAttribution>  policyAttributions;

    // Narrative
    std::string narrative;              // Multi-sentence human-readable summary

    std::string toJSON() const;
};

// ============================================================================
// ExplainabilitySnapshot — portable JSON export for audits / bug reports
// ============================================================================
struct ExplainabilitySnapshot {
    std::string version = "1.0";
    std::string generatedAt;            // ISO-8601 timestamp
    std::string engineVersion;          // "7.3.0"

    SessionExplanation session;

    // Raw data included for reproducibility
    std::string rawEventsJSON;          // All events as JSON array
    std::string rawPoliciesJSON;        // All policies as JSON array
    std::string rawHeuristicsJSON;      // All heuristics as JSON

    std::string toJSON() const;
};

// ============================================================================
// Pluggable log callback
// ============================================================================
using ExplainLogCallback = std::function<void(int level, const std::string& msg)>;

// ============================================================================
// ExplainabilityEngine — pure read-only analysis engine
// ============================================================================
class ExplainabilityEngine {
public:
    ExplainabilityEngine();
    ~ExplainabilityEngine();

    // ---- Dependency injection (read-only access) ----
    void setHistoryRecorder(AgentHistoryRecorder* rec) { m_historyRecorder = rec; }
    void setPolicyEngine(PolicyEngine* engine)          { m_policyEngine = engine; }
    void setLogCallback(ExplainLogCallback cb)          { m_logCb = cb; }

    // ---- Decision Traces ----

    /// Build a causal trace for a specific agent (follows parent/child events).
    DecisionTrace traceAgent(const std::string& agentId) const;

    /// Build a causal trace for a chain (from chain_start to chain_complete).
    DecisionTrace traceChain(const std::string& parentId) const;

    /// Build a causal trace for a swarm (from swarm_start to swarm_complete).
    DecisionTrace traceSwarm(const std::string& parentId) const;

    /// Build a trace for any root event, auto-detecting type.
    DecisionTrace traceAuto(const std::string& id) const;

    // ---- Failure Attribution ----

    /// Explain all failures in the current session.
    std::vector<FailureAttribution> explainFailures() const;

    /// Explain a specific failure event.
    FailureAttribution explainFailure(int64_t eventId) const;

    // ---- Policy Attribution ----

    /// Explain all policy firings in the current session.
    std::vector<PolicyAttribution> explainPolicies() const;

    /// Explain a specific policy's impact.
    PolicyAttribution explainPolicy(const std::string& policyId) const;

    // ---- Session-Level ----

    /// Build a full explanation for the current session.
    SessionExplanation explainSession() const;

    /// Build a full explanation for a specific session ID.
    SessionExplanation explainSession(const std::string& sessionId) const;

    // ---- Export ----

    /// Generate a portable snapshot (JSON) for audit/bug report/demo.
    ExplainabilitySnapshot generateSnapshot() const;

    /// Export snapshot to file. Returns true on success.
    bool exportSnapshot(const std::string& filePath) const;

    // ---- Summaries (for CLI / quick display) ----

    /// One-line summary for an agent decision.
    std::string summarizeAgent(const std::string& agentId) const;

    /// Quick failure summary for display.
    std::string summarizeFailures() const;

    /// Quick policy impact summary.
    std::string summarizePolicies() const;

private:
    void logInfo(const std::string& msg) const  { if (m_logCb) m_logCb(1, msg); }
    void logError(const std::string& msg) const { if (m_logCb) m_logCb(3, msg); }
    void logDebug(const std::string& msg) const { if (m_logCb) m_logCb(0, msg); }

    std::string generateUUID() const;
    int64_t nowMs() const;
    std::string escapeJson(const std::string& s) const;
    std::string nowISO8601() const;

    // Internal builders
    DecisionNode buildNode(const struct AgentEvent& event) const;
    void attachPolicyAttribution(DecisionNode& node) const;
    std::string buildNarrative(const SessionExplanation& session) const;
    FailureAttribution buildFailureAttribution(const struct AgentEvent& failEvent) const;
    PolicyAttribution buildPolicyAttribution(const struct AgentPolicy& policy) const;

    // Dependencies (read-only)
    AgentHistoryRecorder* m_historyRecorder = nullptr;
    PolicyEngine*         m_policyEngine    = nullptr;
    ExplainLogCallback    m_logCb;
};
