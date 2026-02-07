// ============================================================================
// deterministic_replay.h — Phase 10C: Deterministic Replay Journal
// ============================================================================
//
// Ring-buffered action journal with disk-backed persistence for deterministic
// replay of agent sessions. Every action taken by the agent is recorded with
// full context for audit, debugging, and reproducibility.
//
// Architecture:
//   - ActionRecord: single atomic unit of agent behavior
//   - ReplayJournal: ring buffer (memory) + optional disk flush
//   - SessionSnapshot: full state capture at a point in time
//   - ReplayEngine: playback of recorded sessions
//
// Pattern:  Structured results, no exceptions
// Threading: All methods are thread-safe
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#ifndef RAWRXD_DETERMINISTIC_REPLAY_H
#define RAWRXD_DETERMINISTIC_REPLAY_H

#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <deque>

// ============================================================================
// ENUMS
// ============================================================================

enum class ReplayActionType {
    // Agent actions
    AgentQuery         = 0,
    AgentResponse      = 1,
    AgentToolCall      = 2,
    AgentToolResult    = 3,
    AgentPlanStep      = 4,
    AgentDecision      = 5,

    // System actions
    CommandExecution   = 10,
    FileRead           = 11,
    FileWrite          = 12,
    FileCreate         = 13,
    FileDelete         = 14,
    BuildStart         = 15,
    BuildComplete      = 16,

    // Model actions
    ModelInference     = 20,
    ModelSwitch        = 21,
    ModelHotpatch      = 22,

    // Safety actions
    SafetyCheck        = 30,
    SafetyDenial       = 31,
    SafetyEscalation   = 32,
    SafetyRollback     = 33,

    // Governor actions
    GovernorSubmit     = 40,
    GovernorTimeout    = 41,
    GovernorKill       = 42,
    GovernorComplete   = 43,

    // UI actions
    UserInput          = 50,
    UserConfirmation   = 51,
    MenuCommand        = 52,

    // Meta
    SessionStart       = 90,
    SessionEnd         = 91,
    Checkpoint         = 92,
    Marker             = 93
};

enum class ReplayState {
    Idle       = 0,
    Recording  = 1,
    Paused     = 2,
    Playing    = 3,
    Rewinding  = 4
};

// ============================================================================
// STRUCTS
// ============================================================================

struct ActionRecord {
    uint64_t sequenceId;            // Global monotonic sequence number
    uint64_t sessionId;             // Session this belongs to
    ReplayActionType type;
    std::string category;           // Grouping label (e.g., "agent", "build", "safety")
    std::string action;             // Human-readable action description
    std::string input;              // Input/arguments
    std::string output;             // Output/result
    std::string metadata;           // JSON-encoded extra data
    int exitCode;                   // For commands/builds
    float confidence;               // Agent confidence at time of action
    double durationMs;              // How long the action took
    std::chrono::steady_clock::time_point timestamp;

    // Linkage
    uint64_t parentId;              // Parent action (for nested operations)
    uint64_t causedBy;              // What triggered this action
    std::string agentId;            // Which agent performed this

    ActionRecord()
        : sequenceId(0), sessionId(0), type(ReplayActionType::Marker),
          exitCode(0), confidence(1.0f), durationMs(0.0),
          parentId(0), causedBy(0) {}
};

struct SessionSnapshot {
    uint64_t sessionId;
    uint64_t actionCount;
    std::string sessionLabel;
    std::string startTime;          // ISO 8601
    std::string endTime;
    double totalDurationMs;
    uint64_t firstSequenceId;
    uint64_t lastSequenceId;

    // Summary stats
    uint64_t agentQueries;
    uint64_t commandsRun;
    uint64_t filesModified;
    uint64_t safetyDenials;
    uint64_t governorTimeouts;
    uint64_t errors;
};

struct ReplayFilter {
    ReplayActionType filterType;
    bool filterByType;
    std::string filterCategory;
    std::string filterAgentId;
    uint64_t filterSessionId;
    uint64_t afterSequenceId;
    uint64_t beforeSequenceId;
    double minDurationMs;
    float minConfidence;

    ReplayFilter()
        : filterType(ReplayActionType::Marker), filterByType(false),
          filterSessionId(0), afterSequenceId(0), beforeSequenceId(UINT64_MAX),
          minDurationMs(0.0), minConfidence(0.0f) {}
};

struct ReplayJournalStats {
    uint64_t totalRecords          = 0;
    uint64_t recordsInMemory       = 0;
    uint64_t recordsFlushedToDisk  = 0;
    uint64_t totalSessions         = 0;
    uint64_t activeSessionId       = 0;
    uint64_t currentSequenceId     = 0;
    size_t   memoryUsageBytes      = 0;
    size_t   diskUsageBytes        = 0;
    double   oldestRecordAgeMs     = 0.0;
    double   newestRecordAgeMs     = 0.0;
    ReplayState state              = ReplayState::Idle;
};

// ============================================================================
// REPLAY JOURNAL — Ring-buffered action recorder
// ============================================================================

class ReplayJournal {
public:
    static ReplayJournal& instance();

    // ── Lifecycle ──────────────────────────────────────────────────────
    bool init(const std::string& journalDir = "");
    void shutdown();

    // ── Recording ──────────────────────────────────────────────────────
    uint64_t startSession(const std::string& label = "");
    void endSession();
    uint64_t getActiveSessionId() const;

    void startRecording();
    void pauseRecording();
    void stopRecording();
    bool isRecording() const;

    // ── Action Recording ───────────────────────────────────────────────
    uint64_t record(const ActionRecord& action);
    uint64_t recordAction(
        ReplayActionType type,
        const std::string& category,
        const std::string& action,
        const std::string& input = "",
        const std::string& output = "",
        int exitCode = 0,
        float confidence = 1.0f,
        double durationMs = 0.0,
        const std::string& metadata = "");

    // Convenience recorders
    uint64_t recordAgentQuery(const std::string& query, const std::string& agentId = "primary");
    uint64_t recordAgentResponse(const std::string& response, float confidence, double durationMs);
    uint64_t recordToolCall(const std::string& tool, const std::string& args, const std::string& result);
    uint64_t recordCommand(const std::string& command, const std::string& output, int exitCode, double durationMs);
    uint64_t recordFileOp(ReplayActionType type, const std::string& path, const std::string& detail = "");
    uint64_t recordSafetyEvent(ReplayActionType type, const std::string& description, const std::string& metadata = "");
    uint64_t recordGovernorEvent(ReplayActionType type, const std::string& description, uint64_t taskId = 0);
    uint64_t recordCheckpoint(const std::string& label);
    uint64_t recordMarker(const std::string& label);

    // ── Querying ───────────────────────────────────────────────────────
    bool getRecord(uint64_t sequenceId, ActionRecord& outRecord) const;
    std::vector<ActionRecord> getRecords(uint64_t fromSeq, uint64_t count) const;
    std::vector<ActionRecord> getLastN(uint64_t count) const;
    std::vector<ActionRecord> filter(const ReplayFilter& filter) const;
    std::vector<ActionRecord> getSessionRecords(uint64_t sessionId) const;

    // ── Session Management ─────────────────────────────────────────────
    SessionSnapshot getSessionSnapshot(uint64_t sessionId) const;
    std::vector<SessionSnapshot> getAllSessions() const;
    uint64_t getSessionCount() const;

    // ── Playback ───────────────────────────────────────────────────────
    bool startPlayback(uint64_t sessionId);
    bool stepForward();
    bool stepBackward();
    bool seekTo(uint64_t sequenceId);
    bool isPlaying() const;
    void stopPlayback();
    ActionRecord getCurrentPlaybackRecord() const;
    uint64_t getPlaybackPosition() const;

    // Set a callback for playback step events
    void setPlaybackCallback(std::function<void(const ActionRecord&)> cb);

    // ── Disk Persistence ───────────────────────────────────────────────
    bool flushToDisk();
    bool loadFromDisk(uint64_t sessionId);
    bool exportSession(uint64_t sessionId, const std::string& filePath);
    bool importSession(const std::string& filePath);
    std::string getJournalDirectory() const;

    // ── Stats & Reporting ──────────────────────────────────────────────
    ReplayJournalStats getStats() const;
    std::string getStatusString() const;

    // ── Configuration ──────────────────────────────────────────────────
    void setMaxMemoryRecords(size_t max);
    void setAutoFlush(bool enabled);
    void setAutoFlushThreshold(size_t records);

private:
    ReplayJournal();
    ~ReplayJournal();

    // Ring buffer management
    void pushRecord(ActionRecord&& record);
    void evictOldest();
    void autoFlushIfNeeded();

    // Disk I/O
    bool writeRecordToDisk(const ActionRecord& record);
    bool readRecordsFromDisk(uint64_t sessionId, std::vector<ActionRecord>& out);
    std::string serializeRecord(const ActionRecord& record) const;
    bool deserializeRecord(const std::string& line, ActionRecord& out) const;

    mutable std::mutex m_mutex;
    std::atomic<bool> m_initialized;
    std::atomic<ReplayState> m_state;

    // Ring buffer
    std::deque<ActionRecord> m_ringBuffer;
    size_t m_maxMemoryRecords;
    bool m_autoFlush;
    size_t m_autoFlushThreshold;

    // Sequence tracking
    std::atomic<uint64_t> m_nextSequenceId;
    std::atomic<uint64_t> m_activeSessionId;
    uint64_t m_nextSessionId;

    // Session index
    std::vector<SessionSnapshot> m_sessions;

    // Playback state
    size_t m_playbackIndex;
    std::vector<ActionRecord> m_playbackBuffer;
    std::function<void(const ActionRecord&)> m_playbackCallback;

    // Disk
    std::string m_journalDir;
    uint64_t m_recordsFlushed;

    static constexpr size_t DEFAULT_MAX_MEMORY_RECORDS = 50000;
    static constexpr size_t DEFAULT_FLUSH_THRESHOLD = 10000;
};

// ============================================================================
// UTILITY — Action type to string
// ============================================================================

inline const char* replayActionTypeToString(ReplayActionType t) {
    switch (t) {
        case ReplayActionType::AgentQuery:       return "AgentQuery";
        case ReplayActionType::AgentResponse:    return "AgentResponse";
        case ReplayActionType::AgentToolCall:    return "AgentToolCall";
        case ReplayActionType::AgentToolResult:  return "AgentToolResult";
        case ReplayActionType::AgentPlanStep:    return "AgentPlanStep";
        case ReplayActionType::AgentDecision:    return "AgentDecision";
        case ReplayActionType::CommandExecution: return "CommandExecution";
        case ReplayActionType::FileRead:         return "FileRead";
        case ReplayActionType::FileWrite:        return "FileWrite";
        case ReplayActionType::FileCreate:       return "FileCreate";
        case ReplayActionType::FileDelete:       return "FileDelete";
        case ReplayActionType::BuildStart:       return "BuildStart";
        case ReplayActionType::BuildComplete:    return "BuildComplete";
        case ReplayActionType::ModelInference:   return "ModelInference";
        case ReplayActionType::ModelSwitch:      return "ModelSwitch";
        case ReplayActionType::ModelHotpatch:    return "ModelHotpatch";
        case ReplayActionType::SafetyCheck:      return "SafetyCheck";
        case ReplayActionType::SafetyDenial:     return "SafetyDenial";
        case ReplayActionType::SafetyEscalation: return "SafetyEscalation";
        case ReplayActionType::SafetyRollback:   return "SafetyRollback";
        case ReplayActionType::GovernorSubmit:   return "GovernorSubmit";
        case ReplayActionType::GovernorTimeout:  return "GovernorTimeout";
        case ReplayActionType::GovernorKill:     return "GovernorKill";
        case ReplayActionType::GovernorComplete: return "GovernorComplete";
        case ReplayActionType::UserInput:        return "UserInput";
        case ReplayActionType::UserConfirmation: return "UserConfirmation";
        case ReplayActionType::MenuCommand:      return "MenuCommand";
        case ReplayActionType::SessionStart:     return "SessionStart";
        case ReplayActionType::SessionEnd:       return "SessionEnd";
        case ReplayActionType::Checkpoint:       return "Checkpoint";
        case ReplayActionType::Marker:           return "Marker";
        default:                                 return "Unknown";
    }
}

inline const char* replayStateToString(ReplayState s) {
    switch (s) {
        case ReplayState::Idle:      return "Idle";
        case ReplayState::Recording: return "Recording";
        case ReplayState::Paused:    return "Paused";
        case ReplayState::Playing:   return "Playing";
        case ReplayState::Rewinding: return "Rewinding";
        default:                     return "Unknown";
    }
}

#endif // RAWRXD_DETERMINISTIC_REPLAY_H
