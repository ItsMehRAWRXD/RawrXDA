#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <map>

namespace RawrXD {
namespace Session {

// Event types in the session
enum class EventType {
    USER_PROMPT,
    AI_RESPONSE,
    TOOL_CALL,
    FILE_MODIFICATION,
    ERROR,
    CHECKPOINT
};

// Single event in the conversation
struct SessionEvent {
    EventType type;
    std::chrono::system_clock::time_point timestamp;
    std::string content;
    std::map<std::string, std::string> metadata; // model, tool_name, file_path, etc.
    uint64_t sequence_id = 0;
};

// Checkpoint for session branching
struct SessionCheckpoint {
    uint64_t checkpoint_id;
    uint64_t at_sequence_id;
    std::string label;
    std::chrono::system_clock::time_point created_at;
};

// Complete AI session
class AISession {
public:
    AISession();
    explicit AISession(const std::string& session_id);
    ~AISession();

    // Session management
    std::string getSessionId() const { return m_session_id; }
    void setSessionName(const std::string& name) { m_session_name = name; }
    std::string getSessionName() const { return m_session_name; }
    
    // Recording events
    void recordUserPrompt(const std::string& prompt, const std::map<std::string, std::string>& metadata = {});
    void recordAIResponse(const std::string& response, const std::string& model, 
                         uint64_t prompt_tokens, uint64_t completion_tokens);
    void recordToolCall(const std::string& tool_name, const std::string& args, 
                       const std::string& result, bool success);
    void recordFileModification(const std::string& file_path, const std::string& operation,
                               const std::string& content_before = "", 
                               const std::string& content_after = "");
    void recordError(const std::string& error_message, const std::string& context = "");
    
    // Checkpoints
    uint64_t createCheckpoint(const std::string& label = "");
    std::vector<SessionCheckpoint> getCheckpoints() const;
    bool restoreToCheckpoint(uint64_t checkpoint_id);
    AISession forkFromCheckpoint(uint64_t checkpoint_id, const std::string& new_session_name = "");
    
    // Query
    std::vector<SessionEvent> getEvents(size_t start = 0, size_t count = 100) const;
    std::vector<SessionEvent> getEventsSince(const std::chrono::system_clock::time_point& since) const;
    std::vector<SessionEvent> getEventsByType(EventType type) const;
    size_t getEventCount() const { return m_events.size(); }
    
    // Persistence
    bool saveToFile(const std::string& filepath) const;
    bool loadFromFile(const std::string& filepath);
    std::string toJSON() const;
    bool fromJSON(const std::string& json);
    
    // Session info
    std::chrono::system_clock::time_point getCreatedAt() const { return m_created_at; }
    std::chrono::system_clock::time_point getLastActivityAt() const { return m_last_activity_at; }
    size_t getTotalSizeBytes() const;
    
    // Replay mode
    struct ReplayState {
        size_t current_event_index = 0;
        bool is_playing = false;
        std::chrono::milliseconds playback_speed{1000}; // ms between events
    };
    
    void startReplay(size_t from_event = 0);
    void stopReplay();
    SessionEvent getNextReplayEvent();
    bool hasMoreReplayEvents() const;
    ReplayState getReplayState() const { return m_replay_state; }
    
    // Statistics
    struct SessionStats {
        size_t total_prompts = 0;
        size_t total_responses = 0;
        size_t total_tool_calls = 0;
        size_t total_file_modifications = 0;
        size_t total_errors = 0;
        uint64_t total_prompt_tokens = 0;
        uint64_t total_completion_tokens = 0;
        std::map<std::string, size_t> tools_usage; // tool_name -> count
        std::map<std::string, size_t> models_usage; // model -> count
    };
    
    SessionStats getStatistics() const;

private:
    std::string m_session_id;
    std::string m_session_name;
    std::chrono::system_clock::time_point m_created_at;
    std::chrono::system_clock::time_point m_last_activity_at;
    
    std::vector<SessionEvent> m_events;
    std::vector<SessionCheckpoint> m_checkpoints;
    uint64_t m_next_sequence_id = 0;
    uint64_t m_next_checkpoint_id = 0;
    
    ReplayState m_replay_state;
    
    // Helper methods
    void addEvent(EventType type, const std::string& content, 
                 const std::map<std::string, std::string>& metadata);
    std::string generateSessionId() const;
    std::string eventTypeToString(EventType type) const;
    EventType stringToEventType(const std::string& str) const;
};

// Session manager for handling multiple sessions
class SessionManager {
public:
    SessionManager();
    ~SessionManager();
    
    // Session lifecycle
    std::shared_ptr<AISession> createSession(const std::string& name = "");
    std::shared_ptr<AISession> getSession(const std::string& session_id);
    std::shared_ptr<AISession> getCurrentSession();
    void setCurrentSession(const std::string& session_id);
    
    // Persistence
    std::vector<std::string> listSavedSessions() const;
    bool saveSession(const std::string& session_id);
    bool loadSession(const std::string& session_id);
    bool deleteSession(const std::string& session_id);
    bool saveCurrentSession();
    bool autoSave(); // Auto-save current session
    
    // Settings
    void setStorageDirectory(const std::string& directory);
    std::string getStorageDirectory() const { return m_storage_directory; }
    void setMaxSessionSizeMB(size_t mb) { m_max_session_size_bytes = mb * 1024 * 1024; }
    void setAutoSaveEnabled(bool enabled) { m_auto_save_enabled = enabled; }
    void setAutoSaveIntervalSeconds(uint32_t seconds) { m_auto_save_interval_seconds = seconds; }
    
    // Cleanup
    void cleanupOldSessions(uint32_t days_to_keep = 30);
    size_t getTotalStorageUsed() const;

private:
    std::map<std::string, std::shared_ptr<AISession>> m_sessions;
    std::string m_current_session_id;
    std::string m_storage_directory;
    size_t m_max_session_size_bytes = 50 * 1024 * 1024; // 50MB default
    bool m_auto_save_enabled = true;
    uint32_t m_auto_save_interval_seconds = 300; // 5 minutes
    std::chrono::system_clock::time_point m_last_auto_save;
    
    std::string getSessionFilePath(const std::string& session_id) const;
    void ensureStorageDirectoryExists();
};

// Global singleton
SessionManager& GetSessionManager();

} // namespace Session
} // namespace RawrXD
