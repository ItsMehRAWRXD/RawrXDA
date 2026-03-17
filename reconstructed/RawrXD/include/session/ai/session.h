#pragma once

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace RawrXD {
namespace Session {

enum class EventType {
    USER_PROMPT,
    AI_RESPONSE,
    TOOL_CALL,
    FILE_MODIFICATION,
    AI_ERROR,
    CHECKPOINT
};

struct SessionEvent {
    EventType type{EventType::USER_PROMPT};
    std::chrono::system_clock::time_point timestamp{};
    std::string content;
    std::map<std::string, std::string> metadata;
    uint64_t sequence_id{0};
};

struct SessionCheckpoint {
    uint64_t checkpoint_id{0};
    uint64_t at_sequence_id{0};
    std::string label;
    std::chrono::system_clock::time_point created_at{};
};

class AISession {
public:
    struct ReplayState {
        size_t current_event_index{0};
        bool is_playing{false};
    };

    struct SessionStats {
        uint64_t total_prompts{0};
        uint64_t total_responses{0};
        uint64_t total_tool_calls{0};
        uint64_t total_file_modifications{0};
        uint64_t total_errors{0};
        uint64_t total_prompt_tokens{0};
        uint64_t total_completion_tokens{0};
        std::map<std::string, uint64_t> models_usage;
        std::map<std::string, uint64_t> tools_usage;
    };

    AISession();
    explicit AISession(const std::string& session_id);
    ~AISession();

    void recordUserPrompt(const std::string& prompt, const std::map<std::string, std::string>& metadata = {});
    void recordAIResponse(const std::string& response, const std::string& model, uint64_t prompt_tokens, uint64_t completion_tokens);
    void recordToolCall(const std::string& tool_name, const std::string& args, const std::string& result, bool success);
    void recordFileModification(const std::string& file_path, const std::string& operation, const std::string& content_before, const std::string& content_after);
    void recordError(const std::string& error_message, const std::string& context = "");

    uint64_t createCheckpoint(const std::string& label = "");
    std::vector<SessionCheckpoint> getCheckpoints() const;
    bool restoreToCheckpoint(uint64_t checkpoint_id);
    AISession forkFromCheckpoint(uint64_t checkpoint_id, const std::string& new_session_name = "");

    std::vector<SessionEvent> getEvents(size_t start = 0, size_t count = 100) const;
    std::vector<SessionEvent> getEventsSince(const std::chrono::system_clock::time_point& since) const;
    std::vector<SessionEvent> getEventsByType(EventType type) const;

    bool saveToFile(const std::string& filepath) const;
    bool loadFromFile(const std::string& filepath);

    std::string toJSON() const;
    bool fromJSON(const std::string& json);

    size_t getTotalSizeBytes() const;

    void startReplay(size_t from_event = 0);
    void stopReplay();
    SessionEvent getNextReplayEvent();
    bool hasMoreReplayEvents() const;

    SessionStats getStatistics() const;

    const std::string& getSessionId() const { return m_session_id; }
    const std::string& getSessionName() const { return m_session_name; }
    void setSessionName(const std::string& name) { m_session_name = name; }

private:
    void addEvent(EventType type, const std::string& content, const std::map<std::string, std::string>& metadata);
    std::string generateSessionId() const;
    std::string eventTypeToString(EventType type) const;
    EventType stringToEventType(const std::string& str) const;

    std::string m_session_id;
    std::string m_session_name;
    std::chrono::system_clock::time_point m_created_at{};
    std::chrono::system_clock::time_point m_last_activity_at{};

    std::vector<SessionEvent> m_events;
    std::vector<SessionCheckpoint> m_checkpoints;
    uint64_t m_next_sequence_id{1};
    uint64_t m_next_checkpoint_id{1};

    ReplayState m_replay_state;
};

class SessionManager {
public:
    SessionManager();
    ~SessionManager();

    std::shared_ptr<AISession> createSession(const std::string& name = "");
    std::shared_ptr<AISession> getSession(const std::string& session_id);
    std::shared_ptr<AISession> getCurrentSession();
    void setCurrentSession(const std::string& session_id);

    std::vector<std::string> listSavedSessions() const;

    bool saveSession(const std::string& session_id);
    bool loadSession(const std::string& session_id);
    bool deleteSession(const std::string& session_id);
    bool saveCurrentSession();

    bool autoSave();
    void setStorageDirectory(const std::string& directory);
    void cleanupOldSessions(uint32_t days_to_keep = 30);
    size_t getTotalStorageUsed() const;

private:
    std::string getSessionFilePath(const std::string& session_id) const;
    void ensureStorageDirectoryExists();

    std::map<std::string, std::shared_ptr<AISession>> m_sessions;
    std::string m_current_session_id;

    std::string m_storage_directory;
    bool m_auto_save_enabled{true};
    uint32_t m_auto_save_interval_seconds{300};
    std::chrono::system_clock::time_point m_last_auto_save{};
};

SessionManager& GetSessionManager();

} // namespace Session
} // namespace RawrXD
