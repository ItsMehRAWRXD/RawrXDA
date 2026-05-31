#pragma once
// ============================================================================
// agent_history.h — Portable Agent Event History, Timeline & Replay
// ============================================================================
// Append-only event log for all agentic operations.
// Platform-independent. Used by Win32IDE, CLI, React/HTTP, and console.
//
// Event types:
//   - agent_spawn / agent_complete / agent_fail / agent_cancel
//   - tool_invoke / tool_result
//   - chain_start / chain_step / chain_complete / chain_fail
//   - swarm_start / swarm_task / swarm_merge / swarm_complete
//   - chat_request / chat_response
//   - todo_update
//
// Persistence: JSONL (one JSON object per line), append-only.
// Replay: re-executes a chain or swarm from recorded state.
// ============================================================================

#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <chrono>
#include <fstream>
#include <sstream>
#include <atomic>
#include <unordered_map>

// Forward declarations
class SubAgentManager;

// ============================================================================
// AgentEvent — a single recorded event in the agent timeline
// ============================================================================
struct AgentEvent {
    // Event identity
    int64_t     id;                 // Auto-incrementing sequence number
    std::string eventType;          // e.g. "agent_spawn", "chain_step", "swarm_merge"
    std::string sessionId;          // Session (process run) identifier
    
    // Timestamps
    int64_t     timestampMs;        // Epoch milliseconds (UTC)
    int         durationMs;         // Duration of the operation (0 if instantaneous)
    
    // Agent context
    std::string agentId;            // SubAgent ID (if applicable)
    std::string parentId;           // Parent agent/session
    
    // Payload
    std::string description;        // Human-readable summary
    std::string input;              // Input prompt / data (truncated for storage)
    std::string output;             // Result / output (truncated for storage)
    std::string metadata;           // Extra JSON blob (tool name, strategy, step index, etc.)
    
    // Outcome
    bool        success;            // true = completed, false = failed/cancelled
    std::string errorMessage;       // Non-empty if success == false

    // Serialize to a single JSON line (no newlines in output)
    std::string toJSON() const;
    
    // Deserialize from a JSON line
    static AgentEvent fromJSON(const std::string& json);
};

// ============================================================================
// HistoryQuery — filter criteria for querying the event log
// ============================================================================
struct HistoryQuery {
    std::string sessionId;          // Filter by session (empty = all)
    std::string agentId;            // Filter by agent (empty = all)
    std::string eventType;          // Filter by type (empty = all)
    std::string parentId;           // Filter by parent (empty = all)
    int64_t     afterTimestamp = 0;  // Only events after this timestamp
    int64_t     beforeTimestamp = 0; // Only events before this timestamp (0 = no limit)
    int         limit = 100;        // Max results
    int         offset = 0;         // Skip first N results
    bool        successOnly = false;// Only successful events
};

// ============================================================================
// ReplayRequest — specifies what to replay
// ============================================================================
struct ReplayRequest {
    std::string originalAgentId;    // Replay a specific agent run
    std::string originalSessionId;  // Or replay all events from a session
    std::string eventType;          // "chain" or "swarm" (determines replay strategy)
    bool        dryRun = false;     // If true, return plan without executing
};

// ============================================================================
// ReplayResult — outcome of a replay operation
// ============================================================================
struct ReplayResult {
    bool        success;
    std::string result;
    std::string originalResult;
    int         eventsReplayed;
    int         durationMs;
    std::vector<AgentEvent> replayEvents;  // New events generated during replay
};

// ============================================================================
// Pluggable log callback (same pattern as subagent_core)
// ============================================================================
using HistoryLogCallback = std::function<void(int level, const std::string& msg)>;

// ============================================================================
// AgentHistoryRecorder — records, queries, and replays agent events
// ============================================================================
class AgentHistoryRecorder {
public:
    explicit AgentHistoryRecorder(const std::string& storageDir = "");
    ~AgentHistoryRecorder();

    // ---- Configuration ----
    void setLogCallback(HistoryLogCallback cb) { m_logCb = cb; }
    void setSessionId(const std::string& id)   { m_sessionId = id; }
    std::string sessionId() const              { return m_sessionId; }
    void setMaxEventPayloadBytes(size_t bytes) { m_maxPayloadBytes = bytes; }
    void setRetentionDays(int days)            { m_retentionDays = days; }

    // ---- Recording ----
    
    /// Record an event. Thread-safe. Returns the event ID.
    int64_t record(const std::string& eventType,
                   const std::string& agentId,
                   const std::string& parentId,
                   const std::string& description,
                   const std::string& input,
                   const std::string& output,
                   bool success,
                   int durationMs = 0,
                   const std::string& errorMessage = "",
                   const std::string& metadata = "");

    // Convenience recorders for common events
    int64_t recordAgentSpawn(const std::string& agentId, const std::string& parentId,
                              const std::string& description, const std::string& prompt);
    int64_t recordAgentComplete(const std::string& agentId, const std::string& result,
                                 int durationMs);
    int64_t recordAgentFail(const std::string& agentId, const std::string& error,
                             int durationMs);
    int64_t recordAgentCancel(const std::string& agentId);
    int64_t recordToolInvoke(const std::string& agentId, const std::string& toolName,
                              const std::string& input);
    int64_t recordToolResult(const std::string& agentId, const std::string& toolName,
                              const std::string& result, bool success);
    int64_t recordChainStart(const std::string& parentId, int stepCount);
    int64_t recordChainStep(const std::string& parentId, int stepIndex,
                             const std::string& agentId, const std::string& result,
                             bool success, int durationMs);
    int64_t recordChainComplete(const std::string& parentId, const std::string& result,
                                 int stepCount, int totalDurationMs);
    int64_t recordSwarmStart(const std::string& parentId, int taskCount,
                              const std::string& strategy);
    int64_t recordSwarmTask(const std::string& parentId, const std::string& taskId,
                             const std::string& agentId, const std::string& result,
                             bool success, int durationMs);
    int64_t recordSwarmComplete(const std::string& parentId, const std::string& mergedResult,
                                 int taskCount, const std::string& strategy, int totalDurationMs);
    int64_t recordChatRequest(const std::string& message);
    int64_t recordChatResponse(const std::string& response, int durationMs);
    int64_t recordTodoUpdate(int todoId, const std::string& title, const std::string& status);

    // ---- Querying ----
    
    /// Query events matching filter criteria
    std::vector<AgentEvent> query(const HistoryQuery& q) const;
    
    /// Get all events for a specific agent
    std::vector<AgentEvent> getAgentTimeline(const std::string& agentId) const;
    
    /// Get all events for a specific session
    std::vector<AgentEvent> getSessionTimeline(const std::string& sessionId) const;
    
    /// Get all events of a specific type
    std::vector<AgentEvent> getEventsByType(const std::string& eventType, int limit = 100) const;

    /// Get all chain events for a given parent
    std::vector<AgentEvent> getChainEvents(const std::string& parentId) const;
    
    /// Get all swarm events for a given parent
    std::vector<AgentEvent> getSwarmEvents(const std::string& parentId) const;
    
    /// Get the full in-memory event log
    std::vector<AgentEvent> allEvents() const;
    
    /// Get total event count
    size_t eventCount() const;

    /// Serialize all events to JSON array string
    std::string toJSON(const std::vector<AgentEvent>& events) const;
    
    /// Get summary statistics
    std::string getStatsSummary() const;

    // ---- Replay ----
    
    /// Replay a previously recorded chain or swarm.
    /// Requires a SubAgentManager to execute.
    ReplayResult replay(const ReplayRequest& req, SubAgentManager* mgr);

    // ---- Maintenance ----
    
    /// Flush in-memory events to disk
    void flush();
    
    /// Load events from disk into memory
    void loadFromDisk();
    
    /// Purge events older than retention period
    void purgeExpired();
    
    /// Clear all in-memory events
    void clear();

private:
    void logInfo(const std::string& msg) const  { if (m_logCb) m_logCb(1, msg); }
    void logError(const std::string& msg) const { if (m_logCb) m_logCb(3, msg); }
    void logDebug(const std::string& msg) const { if (m_logCb) m_logCb(0, msg); }
    
    std::string truncate(const std::string& s, size_t maxLen) const;
    std::string escapeJsonValue(const std::string& s) const;
    std::string unescapeJsonValue(const std::string& s) const;
    int64_t nowMs() const;
    std::string generateSessionId() const;
    std::string getLogFilePath() const;

    // Storage
    std::string             m_storageDir;
    std::string             m_sessionId;
    mutable std::mutex      m_mutex;
    std::vector<AgentEvent> m_events;
    std::atomic<int64_t>    m_nextId{1};
    
    // Config
    size_t  m_maxPayloadBytes = 4096;   // Truncate input/output beyond this
    int     m_retentionDays   = 30;     // Purge events older than this
    
    // Logging
    HistoryLogCallback m_logCb;
};
