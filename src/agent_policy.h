#pragma once
// ============================================================================
// agent_policy.h — Adaptive Intelligence & Policy Layer (Phase 7)
// ============================================================================
// Explicit, versioned, auditable, human-editable policies — "iptables for
// agent behavior."  Every policy is opt-in: the user MUST accept suggestions
// before they take effect.
//
// Components:
//   1. PolicyEngine       — core evaluation + persistence
//   2. Heuristics         — history-derived success rates & patterns
//   3. Adaptive Planning  — apply policies to plan generation
//   4. Suggestions        — generated from heuristics, user must approve
//   5. Export / Import    — team-wide standards via JSON files
//
// Design principles:
//   - No ML / RL / fine-tuning / opaque scoring
//   - PatchResult-style structured results (no exceptions)
//   - Mutex-locked, thread-safe
//   - JSONL persistence (append-only policy log, separate from events)
// ============================================================================

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <chrono>
#include <unordered_map>

// Forward declarations
class AgentHistoryRecorder;
class SubAgentManager;

// ============================================================================
// PolicyTrigger — when does a policy fire?
// ============================================================================
struct PolicyTrigger {
    std::string eventType;          // e.g. "agent_fail", "swarm_complete" (empty = any)
    std::string failureReason;      // substring match on errorMessage (empty = any)
    std::string taskPattern;        // substring match on description/prompt (empty = any)
    std::string toolName;           // match on specific tool (empty = any)
    float       failureRateAbove = 0.0f;  // trigger when observed rate exceeds this
    int         minOccurrences   = 0;     // must have seen N events matching trigger

    std::string toJSON() const;
    static PolicyTrigger fromJSON(const std::string& json);
};

// ============================================================================
// PolicyAction — what does the policy do when it fires?
// ============================================================================
struct PolicyAction {
    // Retry tuning
    int  maxRetries         = -1;       // -1 = don't override; 0+ = override
    int  retryDelayMs       = -1;       // -1 = don't override
    
    // Execution strategy tuning
    bool preferChainOverSwarm = false;  // true = suggest sequential over parallel
    int  reduceParallelism    = 0;      // reduce swarm maxParallel by this many
    int  timeoutOverrideMs    = -1;     // -1 = don't override
    
    // Confidence / quality
    float confidenceThreshold = -1.0f;  // -1 = don't override
    bool  addValidationStep   = false;  // inject a validation sub-agent
    std::string validationPrompt;       // custom validation prompt
    
    // Custom transform (string key for registered transforms)
    std::string customAction;           // named transform (e.g. "add_system_prefix")

    std::string toJSON() const;
    static PolicyAction fromJSON(const std::string& json);
};

// ============================================================================
// AgentPolicy — a single explicit, versioned rule
// ============================================================================
struct AgentPolicy {
    // Identity
    std::string id;                 // UUID
    std::string name;               // Human-readable
    std::string description;        // Why this policy exists
    int         version = 1;
    
    // When
    PolicyTrigger trigger;
    
    // What
    PolicyAction action;
    
    // Control
    bool        enabled            = true;
    bool        requiresUserApproval = true;  // must user accept before apply?
    int         priority           = 50;      // 0 = highest, 100 = lowest
    
    // Audit trail
    int64_t     createdAt  = 0;     // epoch ms
    int64_t     modifiedAt = 0;     // epoch ms
    std::string createdBy;          // "system", "user", "import"
    int         appliedCount = 0;   // how many times this policy has been applied
    
    // Serialize / deserialize
    std::string toJSON() const;
    static AgentPolicy fromJSON(const std::string& json);
};

// ============================================================================
// PolicySuggestion — generated from history analysis, user must approve
// ============================================================================
struct PolicySuggestion {
    std::string id;                 // UUID
    AgentPolicy proposedPolicy;     // The actual policy being suggested
    
    // Rationale
    std::string rationale;          // Human-readable explanation
    float       estimatedImprovement; // 0.0–1.0: estimated success rate boost
    int         supportingEvents;   // how many history events support this
    
    // Affected operations
    std::vector<std::string> affectedEventTypes;
    std::vector<std::string> affectedAgentIds;
    
    // State
    enum class State { Pending, Accepted, Rejected, Expired };
    State       state = State::Pending;
    int64_t     generatedAt = 0;    // epoch ms
    int64_t     decidedAt   = 0;    // epoch ms
    
    std::string stateString() const {
        switch (state) {
            case State::Pending:  return "pending";
            case State::Accepted: return "accepted";
            case State::Rejected: return "rejected";
            case State::Expired:  return "expired";
        }
        return "unknown";
    }

    std::string toJSON() const;
    static PolicySuggestion fromJSON(const std::string& json);
};

// ============================================================================
// PolicyHeuristic — computed stats for a specific event type / tool / pattern
// ============================================================================
struct PolicyHeuristic {
    std::string key;                // e.g. "agent_spawn", "tool:file_write", "chain"
    int         totalEvents  = 0;
    int         successCount = 0;
    int         failCount    = 0;
    float       successRate  = 0.0f;
    float       avgDurationMs = 0.0f;
    float       p95DurationMs = 0.0f;
    
    // Failure pattern analysis
    std::vector<std::string> topFailureReasons;     // most common error substrings
    std::unordered_map<std::string, int> failureReasonCounts;
    
    std::string toJSON() const;
};

// ============================================================================
// PolicyEvalResult — result of evaluating policies for an operation
// ============================================================================
struct PolicyEvalResult {
    bool        hasMatch = false;
    std::vector<const AgentPolicy*> matchedPolicies;  // sorted by priority
    
    // Merged action from all matched policies (highest priority wins per field)
    PolicyAction mergedAction;
    
    // Did any matched policy require user approval?
    bool        needsUserApproval = false;
    std::string summary;            // human-readable explanation
};

// ============================================================================
// Pluggable log callback
// ============================================================================
using PolicyLogCallback = std::function<void(int level, const std::string& msg)>;

// ============================================================================
// PolicyEngine — core evaluation, persistence, heuristics, suggestions
// ============================================================================
class PolicyEngine {
public:
    explicit PolicyEngine(const std::string& storageDir = "");
    ~PolicyEngine();

    // ---- Configuration ----
    void setLogCallback(PolicyLogCallback cb)        { m_logCb = cb; }
    void setHistoryRecorder(AgentHistoryRecorder* rec) { m_historyRecorder = rec; }

    // ---- Policy CRUD ----
    
    /// Add a new policy. Returns the policy ID.
    std::string addPolicy(const AgentPolicy& policy);
    
    /// Update an existing policy by ID. Returns true on success.
    bool updatePolicy(const std::string& policyId, const AgentPolicy& updated);
    
    /// Remove a policy by ID. Returns true if found and removed.
    bool removePolicy(const std::string& policyId);
    
    /// Enable / disable a policy.
    bool setEnabled(const std::string& policyId, bool enabled);
    
    /// Get a policy by ID (nullptr if not found).
    const AgentPolicy* getPolicy(const std::string& policyId) const;
    
    /// Get all policies, optionally filtered to enabled-only.
    std::vector<AgentPolicy> getAllPolicies(bool enabledOnly = false) const;
    
    /// Get total policy count.
    size_t policyCount() const;

    // ---- Evaluation ----
    
    /// Evaluate all active policies against an operation context.
    /// Returns matched policies with merged action.
    PolicyEvalResult evaluate(const std::string& eventType,
                              const std::string& description,
                              const std::string& toolName = "",
                              const std::string& errorMessage = "");

    // ---- Heuristics (History-Derived) ----
    
    /// Recompute all heuristics from event history.
    void computeHeuristics();
    
    /// Get heuristic for a specific key.
    const PolicyHeuristic* getHeuristic(const std::string& key) const;
    
    /// Get all computed heuristics.
    std::vector<PolicyHeuristic> getAllHeuristics() const;
    
    /// Get heuristics summary as JSON string.
    std::string heuristicsSummaryJSON() const;

    // ---- Suggestions ----
    
    /// Generate policy suggestions from heuristics analysis.
    /// Does NOT auto-apply — user must accept.
    std::vector<PolicySuggestion> generateSuggestions();
    
    /// Accept a suggestion (converts to active policy).
    bool acceptSuggestion(const std::string& suggestionId);
    
    /// Reject a suggestion.
    bool rejectSuggestion(const std::string& suggestionId);
    
    /// Get all pending suggestions.
    std::vector<PolicySuggestion> getPendingSuggestions() const;
    
    /// Get all suggestions (any state).
    std::vector<PolicySuggestion> getAllSuggestions() const;

    // ---- Export / Import ----
    
    /// Export all enabled policies to a portable JSON string.
    std::string exportPolicies() const;
    
    /// Import policies from a JSON string. Returns count imported.
    int importPolicies(const std::string& json);
    
    /// Export to a file. Returns true on success.
    bool exportToFile(const std::string& filePath) const;
    
    /// Import from a file. Returns count imported.
    int importFromFile(const std::string& filePath);

    // ---- Persistence ----
    
    /// Save all policies to disk (JSON file in storageDir).
    void save();
    
    /// Load policies from disk.
    void load();
    
    /// Get storage directory path.
    std::string storageDir() const { return m_storageDir; }

    // ---- Statistics ----
    std::string getStatsSummary() const;

private:
    void logInfo(const std::string& msg) const  { if (m_logCb) m_logCb(1, msg); }
    void logError(const std::string& msg) const { if (m_logCb) m_logCb(3, msg); }
    void logDebug(const std::string& msg) const { if (m_logCb) m_logCb(0, msg); }

    std::string generateUUID() const;
    int64_t nowMs() const;
    std::string escapeJson(const std::string& s) const;
    std::string unescapeJson(const std::string& s) const;
    std::string getPolicyFilePath() const;
    std::string getSuggestionsFilePath() const;
    
    bool matchesTrigger(const PolicyTrigger& trigger,
                        const std::string& eventType,
                        const std::string& description,
                        const std::string& toolName,
                        const std::string& errorMessage);

    PolicyAction mergePolicyActions(const std::vector<const AgentPolicy*>& policies) const;

    // Storage
    std::string                  m_storageDir;
    mutable std::mutex           m_mutex;
    std::vector<AgentPolicy>     m_policies;
    std::vector<PolicySuggestion> m_suggestions;
    
    // Heuristics cache
    std::vector<PolicyHeuristic> m_heuristics;
    mutable std::mutex           m_heuristicsMutex;
    
    // External dependencies
    AgentHistoryRecorder*        m_historyRecorder = nullptr;
    PolicyLogCallback            m_logCb;
    
    // Counters
    std::atomic<int>             m_evalCount{0};
    std::atomic<int>             m_matchCount{0};
};
