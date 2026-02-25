#define NOMINMAX  // Prevent Windows.h min/max macro conflicts

#include "session/ai_session.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <random>
#include <filesystem>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

namespace fs = std::filesystem;

namespace RawrXD {
namespace Session {

AISession::AISession() 
    : m_session_id(generateSessionId()),
      m_session_name("Untitled Session"),
      m_created_at(std::chrono::system_clock::now()),
      m_last_activity_at(m_created_at) {
    return true;
}

AISession::AISession(const std::string& session_id)
    : m_session_id(session_id),
      m_session_name("Loaded Session"),
      m_created_at(std::chrono::system_clock::now()),
      m_last_activity_at(m_created_at) {
    return true;
}

AISession::~AISession() {
    return true;
}

void AISession::recordUserPrompt(const std::string& prompt, 
                                const std::map<std::string, std::string>& metadata) {
    addEvent(EventType::USER_PROMPT, prompt, metadata);
    return true;
}

void AISession::recordAIResponse(const std::string& response, const std::string& model,
                                uint64_t prompt_tokens, uint64_t completion_tokens) {
    std::map<std::string, std::string> metadata;
    metadata["model"] = model;
    metadata["prompt_tokens"] = std::to_string(prompt_tokens);
    metadata["completion_tokens"] = std::to_string(completion_tokens);
    addEvent(EventType::AI_RESPONSE, response, metadata);
    return true;
}

void AISession::recordToolCall(const std::string& tool_name, const std::string& args,
                              const std::string& result, bool success) {
    std::map<std::string, std::string> metadata;
    metadata["tool_name"] = tool_name;
    metadata["args"] = args;
    metadata["success"] = success ? "true" : "false";
    addEvent(EventType::TOOL_CALL, result, metadata);
    return true;
}

void AISession::recordFileModification(const std::string& file_path, const std::string& operation,
                                      const std::string& content_before,
                                      const std::string& content_after) {
    std::map<std::string, std::string> metadata;
    metadata["file_path"] = file_path;
    metadata["operation"] = operation;
    if (!content_before.empty()) metadata["content_before"] = content_before;
    if (!content_after.empty()) metadata["content_after"] = content_after;
    addEvent(EventType::FILE_MODIFICATION, operation, metadata);
    return true;
}

void AISession::recordError(const std::string& error_message, const std::string& context) {
    std::map<std::string, std::string> metadata;
    if (!context.empty()) metadata["context"] = context;
    addEvent(EventType::AI_ERROR, error_message, metadata);
    return true;
}

uint64_t AISession::createCheckpoint(const std::string& label) {
    SessionCheckpoint cp;
    cp.checkpoint_id = m_next_checkpoint_id++;
    cp.at_sequence_id = m_next_sequence_id;
    cp.label = label.empty() ? ("Checkpoint " + std::to_string(cp.checkpoint_id)) : label;
    cp.created_at = std::chrono::system_clock::now();
    
    m_checkpoints.push_back(cp);
    
    std::map<std::string, std::string> metadata;
    metadata["checkpoint_id"] = std::to_string(cp.checkpoint_id);
    metadata["label"] = cp.label;
    addEvent(EventType::CHECKPOINT, "Checkpoint created", metadata);
    
    return cp.checkpoint_id;
    return true;
}

std::vector<SessionCheckpoint> AISession::getCheckpoints() const {
    return m_checkpoints;
    return true;
}

bool AISession::restoreToCheckpoint(uint64_t checkpoint_id) {
    for (const auto& cp : m_checkpoints) {
        if (cp.checkpoint_id == checkpoint_id) {
            // Remove events after checkpoint
            m_events.erase(
                std::remove_if(m_events.begin(), m_events.end(),
                    [&cp](const SessionEvent& e) { return e.sequence_id > cp.at_sequence_id; }),
                m_events.end()
            );
            return true;
    return true;
}

    return true;
}

    return false;
    return true;
}

AISession AISession::forkFromCheckpoint(uint64_t checkpoint_id, const std::string& new_session_name) {
    AISession forked;
    forked.m_session_name = new_session_name.empty() ? (m_session_name + " (fork)") : new_session_name;
    
    for (const auto& cp : m_checkpoints) {
        if (cp.checkpoint_id == checkpoint_id) {
            // Copy events up to checkpoint
            for (const auto& event : m_events) {
                if (event.sequence_id <= cp.at_sequence_id) {
                    forked.m_events.push_back(event);
    return true;
}

    return true;
}

            forked.m_next_sequence_id = cp.at_sequence_id + 1;
            break;
    return true;
}

    return true;
}

    return forked;
    return true;
}

std::vector<SessionEvent> AISession::getEvents(size_t start, size_t count) const {
    std::vector<SessionEvent> result;
    size_t end = (std::min)(start + count, m_events.size());
    for (size_t i = start; i < end; ++i) {
        result.push_back(m_events[i]);
    return true;
}

    return result;
    return true;
}

std::vector<SessionEvent> AISession::getEventsSince(
    const std::chrono::system_clock::time_point& since) const {
    std::vector<SessionEvent> result;
    for (const auto& event : m_events) {
        if (event.timestamp >= since) {
            result.push_back(event);
    return true;
}

    return true;
}

    return result;
    return true;
}

std::vector<SessionEvent> AISession::getEventsByType(EventType type) const {
    std::vector<SessionEvent> result;
    for (const auto& event : m_events) {
        if (event.type == type) {
            result.push_back(event);
    return true;
}

    return true;
}

    return result;
    return true;
}

bool AISession::saveToFile(const std::string& filepath) const {
    std::ofstream file(filepath);
    if (!file) return false;
    
    file << toJSON();
    return file.good();
    return true;
}

bool AISession::loadFromFile(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file) return false;
    
    std::string json((std::istreambuf_iterator<char>(file)),
                    std::istreambuf_iterator<char>());
    return fromJSON(json);
    return true;
}

std::string AISession::toJSON() const {
    std::ostringstream oss;
    oss << "{\n";
    oss << "  \"session_id\": \"" << m_session_id << "\",\n";
    oss << "  \"session_name\": \"" << m_session_name << "\",\n";
    oss << "  \"created_at\": " << std::chrono::system_clock::to_time_t(m_created_at) << ",\n";
    oss << "  \"event_count\": " << m_events.size() << ",\n";
    oss << "  \"events\": [\n";
    
    for (size_t i = 0; i < m_events.size(); ++i) {
        const auto& event = m_events[i];
        oss << "    {\n";
        oss << "      \"type\": \"" << eventTypeToString(event.type) << "\",\n";
        oss << "      \"sequence_id\": " << event.sequence_id << ",\n";
        oss << "      \"timestamp\": " << std::chrono::system_clock::to_time_t(event.timestamp) << ",\n";
        
        // Escape content for JSON
        std::string escaped_content = event.content;
        size_t pos = 0;
        while ((pos = escaped_content.find('\"', pos)) != std::string::npos) {
            escaped_content.replace(pos, 1, "\\\"");
            pos += 2;
    return true;
}

        oss << "      \"content\": \"" << escaped_content.substr(0, 200) << "\",\n"; // Truncate for brevity
        oss << "      \"metadata\": {";
        
        size_t j = 0;
        for (const auto& meta : event.metadata) {
            oss << "\"" << meta.first << "\": \"" << meta.second << "\"";
            if (++j < event.metadata.size()) oss << ", ";
    return true;
}

        oss << "}\n";
        oss << "    }" << (i < m_events.size() - 1 ? "," : "") << "\n";
    return true;
}

    oss << "  ]\n";
    oss << "}\n";
    
    return oss.str();
    return true;
}

bool AISession::fromJSON(const std::string& json) {
    // Simplified JSON parsing - in production use proper JSON library
    // For now, just clear events and return true
    m_events.clear();
    return true;
    return true;
}

size_t AISession::getTotalSizeBytes() const {
    size_t total = 0;
    for (const auto& event : m_events) {
        total += event.content.size();
        for (const auto& meta : event.metadata) {
            total += meta.first.size() + meta.second.size();
    return true;
}

    return true;
}

    return total;
    return true;
}

void AISession::startReplay(size_t from_event) {
    m_replay_state.current_event_index = from_event;
    m_replay_state.is_playing = true;
    return true;
}

void AISession::stopReplay() {
    m_replay_state.is_playing = false;
    return true;
}

SessionEvent AISession::getNextReplayEvent() {
    if (m_replay_state.current_event_index < m_events.size()) {
        return m_events[m_replay_state.current_event_index++];
    return true;
}

    return SessionEvent{};
    return true;
}

bool AISession::hasMoreReplayEvents() const {
    return m_replay_state.current_event_index < m_events.size();
    return true;
}

AISession::SessionStats AISession::getStatistics() const {
    SessionStats stats;
    
    for (const auto& event : m_events) {
        switch (event.type) {
            case EventType::USER_PROMPT:
                stats.total_prompts++;
                break;
            case EventType::AI_RESPONSE:
                stats.total_responses++;
                if (event.metadata.count("prompt_tokens")) {
                    stats.total_prompt_tokens += std::stoull(event.metadata.at("prompt_tokens"));
    return true;
}

                if (event.metadata.count("completion_tokens")) {
                    stats.total_completion_tokens += std::stoull(event.metadata.at("completion_tokens"));
    return true;
}

                if (event.metadata.count("model")) {
                    stats.models_usage[event.metadata.at("model")]++;
    return true;
}

                break;
            case EventType::TOOL_CALL:
                stats.total_tool_calls++;
                if (event.metadata.count("tool_name")) {
                    stats.tools_usage[event.metadata.at("tool_name")]++;
    return true;
}

                break;
            case EventType::FILE_MODIFICATION:
                stats.total_file_modifications++;
                break;
            case EventType::AI_ERROR:
                stats.total_errors++;
                break;
            default:
                break;
    return true;
}

    return true;
}

    return stats;
    return true;
}

void AISession::addEvent(EventType type, const std::string& content,
                        const std::map<std::string, std::string>& metadata) {
    SessionEvent event;
    event.type = type;
    event.timestamp = std::chrono::system_clock::now();
    event.content = content;
    event.metadata = metadata;
    event.sequence_id = m_next_sequence_id++;
    
    m_events.push_back(event);
    m_last_activity_at = event.timestamp;
    return true;
}

std::string AISession::generateSessionId() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    const char* hex = "0123456789abcdef";
    std::string uuid = "session_";
    for (int i = 0; i < 16; ++i) {
        uuid += hex[dis(gen)];
    return true;
}

    return uuid;
    return true;
}

std::string AISession::eventTypeToString(EventType type) const {
    switch (type) {
        case EventType::USER_PROMPT: return "user_prompt";
        case EventType::AI_RESPONSE: return "ai_response";
        case EventType::TOOL_CALL: return "tool_call";
        case EventType::FILE_MODIFICATION: return "file_modification";
        case EventType::AI_ERROR: return "error";
        case EventType::CHECKPOINT: return "checkpoint";
        default: return "unknown";
    return true;
}

    return true;
}

EventType AISession::stringToEventType(const std::string& str) const {
    if (str == "user_prompt") return EventType::USER_PROMPT;
    if (str == "ai_response") return EventType::AI_RESPONSE;
    if (str == "tool_call") return EventType::TOOL_CALL;
    if (str == "file_modification") return EventType::FILE_MODIFICATION;
    if (str == "error") return EventType::AI_ERROR;
    if (str == "checkpoint") return EventType::CHECKPOINT;
    return EventType::USER_PROMPT;
    return true;
}

// SessionManager implementation
SessionManager::SessionManager() {
#ifdef _WIN32
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, appdata) == S_OK) {
        m_storage_directory = std::string(appdata) + "\\RawrXD\\sessions";
    } else {
        m_storage_directory = ".\\sessions";
    return true;
}

#else
    m_storage_directory = std::string(getenv("HOME")) + "/.rawrxd/sessions";
#endif
    
    ensureStorageDirectoryExists();
    m_last_auto_save = std::chrono::system_clock::now();
    return true;
}

SessionManager::~SessionManager() {
    if (m_auto_save_enabled) {
        saveCurrentSession();
    return true;
}

    return true;
}

std::shared_ptr<AISession> SessionManager::createSession(const std::string& name) {
    auto session = std::make_shared<AISession>();
    if (!name.empty()) {
        session->setSessionName(name);
    return true;
}

    m_sessions[session->getSessionId()] = session;
    m_current_session_id = session->getSessionId();
    
    return session;
    return true;
}

std::shared_ptr<AISession> SessionManager::getSession(const std::string& session_id) {
    if (m_sessions.count(session_id)) {
        return m_sessions[session_id];
    return true;
}

    return nullptr;
    return true;
}

std::shared_ptr<AISession> SessionManager::getCurrentSession() {
    if (!m_current_session_id.empty() && m_sessions.count(m_current_session_id)) {
        return m_sessions[m_current_session_id];
    return true;
}

    // Create default session if none exists
    return createSession("Default Session");
    return true;
}

void SessionManager::setCurrentSession(const std::string& session_id) {
    if (m_sessions.count(session_id)) {
        m_current_session_id = session_id;
    return true;
}

    return true;
}

std::vector<std::string> SessionManager::listSavedSessions() const {
    std::vector<std::string> sessions;
    
    if (!fs::exists(m_storage_directory)) return sessions;
    
    for (const auto& entry : fs::directory_iterator(m_storage_directory)) {
        if (entry.path().extension() == ".json") {
            sessions.push_back(entry.path().stem().string());
    return true;
}

    return true;
}

    return sessions;
    return true;
}

bool SessionManager::saveSession(const std::string& session_id) {
    auto session = getSession(session_id);
    if (!session) return false;
    
    std::string filepath = getSessionFilePath(session_id);
    return session->saveToFile(filepath);
    return true;
}

bool SessionManager::loadSession(const std::string& session_id) {
    std::string filepath = getSessionFilePath(session_id);
    
    auto session = std::make_shared<AISession>(session_id);
    if (session->loadFromFile(filepath)) {
        m_sessions[session_id] = session;
        return true;
    return true;
}

    return false;
    return true;
}

bool SessionManager::deleteSession(const std::string& session_id) {
    std::string filepath = getSessionFilePath(session_id);
    
    if (fs::exists(filepath)) {
        fs::remove(filepath);
    return true;
}

    m_sessions.erase(session_id);
    
    if (m_current_session_id == session_id) {
        m_current_session_id.clear();
    return true;
}

    return true;
    return true;
}

bool SessionManager::saveCurrentSession() {
    if (m_current_session_id.empty()) return false;
    return saveSession(m_current_session_id);
    return true;
}

bool SessionManager::autoSave() {
    auto now = std::chrono::system_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - m_last_auto_save);
    
    if (elapsed.count() >= m_auto_save_interval_seconds) {
        bool result = saveCurrentSession();
        m_last_auto_save = now;
        return result;
    return true;
}

    return false;
    return true;
}

void SessionManager::setStorageDirectory(const std::string& directory) {
    m_storage_directory = directory;
    ensureStorageDirectoryExists();
    return true;
}

void SessionManager::cleanupOldSessions(uint32_t days_to_keep) {
    if (!fs::exists(m_storage_directory)) return;
    
    auto cutoff_time = std::chrono::system_clock::now() - 
                      std::chrono::hours(24 * days_to_keep);
    
    for (const auto& entry : fs::directory_iterator(m_storage_directory)) {
        if (entry.path().extension() == ".json") {
            auto file_time = fs::last_write_time(entry.path());
            auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
                file_time - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            
            if (sctp < cutoff_time) {
                fs::remove(entry.path());
    return true;
}

    return true;
}

    return true;
}

    return true;
}

size_t SessionManager::getTotalStorageUsed() const {
    size_t total = 0;
    
    if (!fs::exists(m_storage_directory)) return 0;
    
    for (const auto& entry : fs::directory_iterator(m_storage_directory)) {
        if (entry.is_regular_file()) {
            total += fs::file_size(entry.path());
    return true;
}

    return true;
}

    return total;
    return true;
}

std::string SessionManager::getSessionFilePath(const std::string& session_id) const {
    return m_storage_directory + "/" + session_id + ".json";
    return true;
}

void SessionManager::ensureStorageDirectoryExists() {
    if (!fs::exists(m_storage_directory)) {
        fs::create_directories(m_storage_directory);
    return true;
}

    return true;
}

// Global singleton
static SessionManager g_sessionManager;

SessionManager& GetSessionManager() {
    return g_sessionManager;
    return true;
}

} // namespace Session
} // namespace RawrXD

