// ============================================================================
// deterministic_replay.cpp — Phase 10C: Deterministic Replay Journal
// ============================================================================
//
// Full implementation of the ring-buffered action journal with disk-backed
// persistence. Records every agentic action for audit, replay, debugging.
//
// Pattern:  Structured results, no exceptions
// Threading: All public methods are mutex-guarded
// Rule:     NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================

#include "deterministic_replay.h"
#include <sstream>
#include <iomanip>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <ctime>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>

// SCAFFOLD_268: Deterministic replay for audit

#endif

// ============================================================================
// SINGLETON
// ============================================================================

ReplayJournal& ReplayJournal::instance() {
    static ReplayJournal inst;
    return inst;
}

ReplayJournal::ReplayJournal()
    : m_initialized(false),
      m_state(ReplayState::Idle),
      m_maxMemoryRecords(DEFAULT_MAX_MEMORY_RECORDS),
      m_autoFlush(true),
      m_autoFlushThreshold(DEFAULT_FLUSH_THRESHOLD),
      m_nextSequenceId(1),
      m_activeSessionId(0),
      m_nextSessionId(1),
      m_playbackIndex(0),
      m_recordsFlushed(0) {}

ReplayJournal::~ReplayJournal() {
    shutdown();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool ReplayJournal::init(const std::string& journalDir) {
    if (m_initialized.load()) return true;

    std::lock_guard<std::mutex> lock(m_mutex);

    m_journalDir = journalDir.empty() ? "." : journalDir;

    // Create journal directory if needed (best effort)
    CreateDirectoryA(m_journalDir.c_str(), nullptr);

    m_ringBuffer.clear();
    m_sessions.clear();
    m_playbackBuffer.clear();
    m_playbackIndex = 0;
    m_nextSequenceId.store(1);
    m_activeSessionId.store(0);
    m_nextSessionId = 1;
    m_recordsFlushed = 0;

    m_initialized.store(true);
    return true;
}

void ReplayJournal::shutdown() {
    if (!m_initialized.load()) return;

    // End active session if any
    if (m_activeSessionId.load() > 0) {
        endSession();
    }

    // Flush remaining records
    flushToDisk();

    std::lock_guard<std::mutex> lock(m_mutex);
    m_ringBuffer.clear();
    m_sessions.clear();
    m_playbackBuffer.clear();
    m_initialized.store(false);
}

// ============================================================================
// RECORDING CONTROL
// ============================================================================

uint64_t ReplayJournal::startSession(const std::string& label) {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t sessionId = m_nextSessionId++;
    m_activeSessionId.store(sessionId);

    SessionSnapshot snap;
    snap.sessionId = sessionId;
    snap.actionCount = 0;
    snap.sessionLabel = label.empty() ? ("Session-" + std::to_string(sessionId)) : label;
    snap.totalDurationMs = 0.0;
    snap.firstSequenceId = m_nextSequenceId.load();
    snap.lastSequenceId = 0;
    snap.agentQueries = 0;
    snap.commandsRun = 0;
    snap.filesModified = 0;
    snap.safetyDenials = 0;
    snap.governorTimeouts = 0;
    snap.errors = 0;

    // ISO 8601 timestamp
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    struct tm lt;
    localtime_s(&lt, &tt);
    char timeBuf[64];
    strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", &lt);
    snap.startTime = timeBuf;

    m_sessions.push_back(snap);
    m_state.store(ReplayState::Recording);

    return sessionId;
}

void ReplayJournal::endSession() {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t sid = m_activeSessionId.load();
    if (sid == 0) return;

    // Update session snapshot
    for (auto& s : m_sessions) {
        if (s.sessionId == sid) {
            s.lastSequenceId = m_nextSequenceId.load() - 1;

            auto now = std::chrono::system_clock::now();
            auto tt = std::chrono::system_clock::to_time_t(now);
            struct tm lt;
            localtime_s(&lt, &tt);
            char timeBuf[64];
            strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%dT%H:%M:%S", &lt);
            s.endTime = timeBuf;

            break;
        }
    }

    m_activeSessionId.store(0);
    m_state.store(ReplayState::Idle);
}

uint64_t ReplayJournal::getActiveSessionId() const {
    return m_activeSessionId.load();
}

void ReplayJournal::startRecording() {
    m_state.store(ReplayState::Recording);
}

void ReplayJournal::pauseRecording() {
    if (m_state.load() == ReplayState::Recording) {
        m_state.store(ReplayState::Paused);
    }
}

void ReplayJournal::stopRecording() {
    m_state.store(ReplayState::Idle);
}

bool ReplayJournal::isRecording() const {
    return m_state.load() == ReplayState::Recording;
}

// ============================================================================
// ACTION RECORDING
// ============================================================================

uint64_t ReplayJournal::record(const ActionRecord& action) {
    if (m_state.load() != ReplayState::Recording) return 0;

    std::lock_guard<std::mutex> lock(m_mutex);

    ActionRecord rec = action;
    rec.sequenceId = m_nextSequenceId.fetch_add(1);
    rec.sessionId = m_activeSessionId.load();
    rec.timestamp = std::chrono::steady_clock::now();

    // Update session stats
    for (auto& s : m_sessions) {
        if (s.sessionId == rec.sessionId) {
            s.actionCount++;
            s.lastSequenceId = rec.sequenceId;

            switch (rec.type) {
                case ReplayActionType::AgentQuery:       s.agentQueries++; break;
                case ReplayActionType::CommandExecution:  s.commandsRun++; break;
                case ReplayActionType::FileWrite:
                case ReplayActionType::FileCreate:
                case ReplayActionType::FileDelete:       s.filesModified++; break;
                case ReplayActionType::SafetyDenial:     s.safetyDenials++; break;
                case ReplayActionType::GovernorTimeout:  s.governorTimeouts++; break;
                default: break;
            }

            if (rec.exitCode != 0 &&
                (rec.type == ReplayActionType::CommandExecution ||
                 rec.type == ReplayActionType::BuildComplete)) {
                s.errors++;
            }
            break;
        }
    }

    uint64_t seqId = rec.sequenceId;
    pushRecord(std::move(rec));
    autoFlushIfNeeded();

    return seqId;
}

uint64_t ReplayJournal::recordAction(
    ReplayActionType type,
    const std::string& category,
    const std::string& action,
    const std::string& input,
    const std::string& output,
    int exitCode,
    float confidence,
    double durationMs,
    const std::string& metadata)
{
    ActionRecord rec;
    rec.type = type;
    rec.category = category;
    rec.action = action;
    rec.input = input;
    rec.output = output;
    rec.exitCode = exitCode;
    rec.confidence = confidence;
    rec.durationMs = durationMs;
    rec.metadata = metadata;
    rec.agentId = "primary";

    return record(rec);
}

uint64_t ReplayJournal::recordAgentQuery(const std::string& query, const std::string& agentId) {
    ActionRecord rec;
    rec.type = ReplayActionType::AgentQuery;
    rec.category = "agent";
    rec.action = "query";
    rec.input = query;
    rec.agentId = agentId;
    return record(rec);
}

uint64_t ReplayJournal::recordAgentResponse(const std::string& response, float confidence, double durationMs) {
    ActionRecord rec;
    rec.type = ReplayActionType::AgentResponse;
    rec.category = "agent";
    rec.action = "response";
    rec.output = response;
    rec.confidence = confidence;
    rec.durationMs = durationMs;
    rec.agentId = "primary";
    return record(rec);
}

uint64_t ReplayJournal::recordToolCall(const std::string& tool, const std::string& args, const std::string& result) {
    ActionRecord rec;
    rec.type = ReplayActionType::AgentToolCall;
    rec.category = "agent";
    rec.action = "tool:" + tool;
    rec.input = args;
    rec.output = result;
    rec.agentId = "primary";
    return record(rec);
}

uint64_t ReplayJournal::recordCommand(const std::string& command, const std::string& output, int exitCode, double durationMs) {
    ActionRecord rec;
    rec.type = ReplayActionType::CommandExecution;
    rec.category = "system";
    rec.action = "command";
    rec.input = command;
    rec.output = output;
    rec.exitCode = exitCode;
    rec.durationMs = durationMs;
    return record(rec);
}

uint64_t ReplayJournal::recordFileOp(ReplayActionType type, const std::string& path, const std::string& detail) {
    ActionRecord rec;
    rec.type = type;
    rec.category = "filesystem";
    rec.action = replayActionTypeToString(type);
    rec.input = path;
    rec.output = detail;
    return record(rec);
}

uint64_t ReplayJournal::recordSafetyEvent(ReplayActionType type, const std::string& description, const std::string& metadata) {
    ActionRecord rec;
    rec.type = type;
    rec.category = "safety";
    rec.action = replayActionTypeToString(type);
    rec.input = description;
    rec.metadata = metadata;
    return record(rec);
}

uint64_t ReplayJournal::recordGovernorEvent(ReplayActionType type, const std::string& description, uint64_t taskId) {
    ActionRecord rec;
    rec.type = type;
    rec.category = "governor";
    rec.action = replayActionTypeToString(type);
    rec.input = description;
    rec.metadata = "{\"taskId\":" + std::to_string(taskId) + "}";
    return record(rec);
}

uint64_t ReplayJournal::recordCheckpoint(const std::string& label) {
    ActionRecord rec;
    rec.type = ReplayActionType::Checkpoint;
    rec.category = "meta";
    rec.action = "checkpoint";
    rec.input = label;
    return record(rec);
}

uint64_t ReplayJournal::recordMarker(const std::string& label) {
    ActionRecord rec;
    rec.type = ReplayActionType::Marker;
    rec.category = "meta";
    rec.action = "marker";
    rec.input = label;
    return record(rec);
}

// ============================================================================
// QUERYING
// ============================================================================

bool ReplayJournal::getRecord(uint64_t sequenceId, ActionRecord& outRecord) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (const auto& rec : m_ringBuffer) {
        if (rec.sequenceId == sequenceId) {
            outRecord = rec;
            return true;
        }
    }
    return false;
}

std::vector<ActionRecord> ReplayJournal::getRecords(uint64_t fromSeq, uint64_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ActionRecord> result;

    for (const auto& rec : m_ringBuffer) {
        if (rec.sequenceId >= fromSeq) {
            result.push_back(rec);
            if (result.size() >= count) break;
        }
    }
    return result;
}

std::vector<ActionRecord> ReplayJournal::getLastN(uint64_t count) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ActionRecord> result;

    size_t total = m_ringBuffer.size();
    size_t start = (total > count) ? (total - (size_t)count) : 0;

    for (size_t i = start; i < total; i++) {
        result.push_back(m_ringBuffer[i]);
    }
    return result;
}

std::vector<ActionRecord> ReplayJournal::filter(const ReplayFilter& f) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ActionRecord> result;

    for (const auto& rec : m_ringBuffer) {
        if (rec.sequenceId < f.afterSequenceId) continue;
        if (rec.sequenceId > f.beforeSequenceId) continue;
        if (f.filterByType && rec.type != f.filterType) continue;
        if (!f.filterCategory.empty() && rec.category != f.filterCategory) continue;
        if (!f.filterAgentId.empty() && rec.agentId != f.filterAgentId) continue;
        if (f.filterSessionId > 0 && rec.sessionId != f.filterSessionId) continue;
        if (rec.durationMs < f.minDurationMs) continue;
        if (rec.confidence < f.minConfidence) continue;

        result.push_back(rec);
    }
    return result;
}

std::vector<ActionRecord> ReplayJournal::getSessionRecords(uint64_t sessionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ActionRecord> result;

    for (const auto& rec : m_ringBuffer) {
        if (rec.sessionId == sessionId) {
            result.push_back(rec);
        }
    }
    return result;
}

// ============================================================================
// SESSION MANAGEMENT
// ============================================================================

SessionSnapshot ReplayJournal::getSessionSnapshot(uint64_t sessionId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& s : m_sessions) {
        if (s.sessionId == sessionId) return s;
    }
    return SessionSnapshot();
}

std::vector<SessionSnapshot> ReplayJournal::getAllSessions() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessions;
}

uint64_t ReplayJournal::getSessionCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_sessions.size();
}

// ============================================================================
// PLAYBACK
// ============================================================================

bool ReplayJournal::startPlayback(uint64_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    m_playbackBuffer.clear();
    m_playbackIndex = 0;

    for (const auto& rec : m_ringBuffer) {
        if (rec.sessionId == sessionId) {
            m_playbackBuffer.push_back(rec);
        }
    }

    if (m_playbackBuffer.empty()) return false;

    m_state.store(ReplayState::Playing);
    return true;
}

bool ReplayJournal::stepForward() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state.load() != ReplayState::Playing) return false;
    if (m_playbackIndex >= m_playbackBuffer.size()) return false;

    const auto& rec = m_playbackBuffer[m_playbackIndex];
    if (m_playbackCallback) {
        m_playbackCallback(rec);
    }

    m_playbackIndex++;
    return (m_playbackIndex < m_playbackBuffer.size());
}

bool ReplayJournal::stepBackward() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_state.load() != ReplayState::Playing) return false;
    if (m_playbackIndex == 0) return false;

    m_state.store(ReplayState::Rewinding);
    m_playbackIndex--;

    const auto& rec = m_playbackBuffer[m_playbackIndex];
    if (m_playbackCallback) {
        m_playbackCallback(rec);
    }

    m_state.store(ReplayState::Playing);
    return true;
}

bool ReplayJournal::seekTo(uint64_t sequenceId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (size_t i = 0; i < m_playbackBuffer.size(); i++) {
        if (m_playbackBuffer[i].sequenceId == sequenceId) {
            m_playbackIndex = i;
            return true;
        }
    }
    return false;
}

bool ReplayJournal::isPlaying() const {
    ReplayState s = m_state.load();
    return (s == ReplayState::Playing || s == ReplayState::Rewinding);
}

void ReplayJournal::stopPlayback() {
    m_state.store(ReplayState::Idle);
    std::lock_guard<std::mutex> lock(m_mutex);
    m_playbackBuffer.clear();
    m_playbackIndex = 0;
}

ActionRecord ReplayJournal::getCurrentPlaybackRecord() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_playbackIndex < m_playbackBuffer.size()) {
        return m_playbackBuffer[m_playbackIndex];
    }
    return ActionRecord();
}

uint64_t ReplayJournal::getPlaybackPosition() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_playbackIndex;
}

void ReplayJournal::setPlaybackCallback(std::function<void(const ActionRecord&)> cb) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_playbackCallback = cb;
}

// ============================================================================
// DISK PERSISTENCE
// ============================================================================

bool ReplayJournal::flushToDisk() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_journalDir.empty()) return false;

    std::string filePath = m_journalDir + "\\replay_journal.log";
    std::ofstream ofs(filePath, std::ios::app);
    if (!ofs.is_open()) return false;

    for (const auto& rec : m_ringBuffer) {
        ofs << serializeRecord(rec) << "\n";
    }

    m_recordsFlushed += m_ringBuffer.size();
    ofs.close();
    return true;
}

bool ReplayJournal::loadFromDisk(uint64_t sessionId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string filePath = m_journalDir + "\\replay_journal.log";
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) return false;

    std::string line;
    while (std::getline(ifs, line)) {
        ActionRecord rec;
        if (deserializeRecord(line, rec)) {
            if (sessionId == 0 || rec.sessionId == sessionId) {
                m_ringBuffer.push_back(std::move(rec));
            }
        }
    }
    return true;
}

bool ReplayJournal::exportSession(uint64_t sessionId, const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ofstream ofs(filePath);
    if (!ofs.is_open()) return false;

    // Write session header
    for (const auto& s : m_sessions) {
        if (s.sessionId == sessionId) {
            ofs << "# RawrXD Replay Session: " << s.sessionLabel << "\n";
            ofs << "# Started: " << s.startTime << "\n";
            ofs << "# Ended: " << s.endTime << "\n";
            ofs << "# Actions: " << s.actionCount << "\n";
            ofs << "# ---\n";
            break;
        }
    }

    uint64_t count = 0;
    for (const auto& rec : m_ringBuffer) {
        if (rec.sessionId == sessionId) {
            ofs << serializeRecord(rec) << "\n";
            count++;
        }
    }

    ofs.close();
    return (count > 0);
}

bool ReplayJournal::importSession(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::ifstream ifs(filePath);
    if (!ifs.is_open()) return false;

    std::string line;
    uint64_t imported = 0;
    while (std::getline(ifs, line)) {
        if (line.empty() || line[0] == '#') continue;
        ActionRecord rec;
        if (deserializeRecord(line, rec)) {
            m_ringBuffer.push_back(std::move(rec));
            imported++;
        }
    }
    return (imported > 0);
}

std::string ReplayJournal::getJournalDirectory() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_journalDir;
}

// ============================================================================
// STATS & REPORTING
// ============================================================================

ReplayJournalStats ReplayJournal::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    ReplayJournalStats stats;
    stats.totalRecords = m_nextSequenceId.load() - 1;
    stats.recordsInMemory = m_ringBuffer.size();
    stats.recordsFlushedToDisk = m_recordsFlushed;
    stats.totalSessions = m_sessions.size();
    stats.activeSessionId = m_activeSessionId.load();
    stats.currentSequenceId = m_nextSequenceId.load();
    stats.memoryUsageBytes = m_ringBuffer.size() * sizeof(ActionRecord);
    stats.state = m_state.load();

    if (!m_ringBuffer.empty()) {
        auto now = std::chrono::steady_clock::now();
        stats.oldestRecordAgeMs = std::chrono::duration<double, std::milli>(
            now - m_ringBuffer.front().timestamp).count();
        stats.newestRecordAgeMs = std::chrono::duration<double, std::milli>(
            now - m_ringBuffer.back().timestamp).count();
    }

    return stats;
}

std::string ReplayJournal::getStatusString() const {
    auto s = getStats();

    std::ostringstream oss;
    oss << "════════════════════════════════════════════\n"
        << "  Deterministic Replay Journal (Phase 10C)\n"
        << "════════════════════════════════════════════\n"
        << "  State:              " << replayStateToString(s.state) << "\n"
        << "  Active Session:     " << (s.activeSessionId > 0 ? std::to_string(s.activeSessionId) : "none") << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Total Records:      " << s.totalRecords << "\n"
        << "  In Memory:          " << s.recordsInMemory << "\n"
        << "  Flushed to Disk:    " << s.recordsFlushedToDisk << "\n"
        << "  Max Memory Records: " << m_maxMemoryRecords << "\n"
        << "  ─────────────────────────────────────────\n"
        << "  Total Sessions:     " << s.totalSessions << "\n"
        << "  Current Seq ID:     " << s.currentSequenceId << "\n"
        << "  Memory Usage:       " << (s.memoryUsageBytes / 1024) << " KB\n"
        << "  ─────────────────────────────────────────\n"
        << "  Oldest Record Age:  " << std::fixed << std::setprecision(0) << s.oldestRecordAgeMs << " ms\n"
        << "  Newest Record Age:  " << s.newestRecordAgeMs << " ms\n"
        << "  Journal Directory:  " << m_journalDir << "\n"
        << "  Auto-Flush:         " << (m_autoFlush ? "ON" : "OFF") << "\n"
        << "════════════════════════════════════════════";
    return oss.str();
}

// ============================================================================
// CONFIGURATION
// ============================================================================

void ReplayJournal::setMaxMemoryRecords(size_t max) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxMemoryRecords = max;
}

void ReplayJournal::setAutoFlush(bool enabled) {
    m_autoFlush = enabled;
}

void ReplayJournal::setAutoFlushThreshold(size_t records) {
    m_autoFlushThreshold = records;
}

// ============================================================================
// PRIVATE — Ring buffer management
// ============================================================================

void ReplayJournal::pushRecord(ActionRecord&& record) {
    // Already under lock
    m_ringBuffer.push_back(std::move(record));
    while (m_ringBuffer.size() > m_maxMemoryRecords) {
        evictOldest();
    }
}

void ReplayJournal::evictOldest() {
    // Already under lock
    if (!m_ringBuffer.empty()) {
        m_ringBuffer.pop_front();
    }
}

void ReplayJournal::autoFlushIfNeeded() {
    // Already under lock
    if (m_autoFlush && m_ringBuffer.size() >= m_autoFlushThreshold) {
        // Write oldest records to disk and remove from buffer
        std::string filePath = m_journalDir + "\\replay_journal.log";
        std::ofstream ofs(filePath, std::ios::app);
        if (ofs.is_open()) {
            size_t toFlush = m_ringBuffer.size() / 2; // Flush half
            for (size_t i = 0; i < toFlush; i++) {
                ofs << serializeRecord(m_ringBuffer.front()) << "\n";
                m_ringBuffer.pop_front();
                m_recordsFlushed++;
            }
        }
    }
}

// ============================================================================
// PRIVATE — Disk I/O Serialization
// ============================================================================

std::string ReplayJournal::serializeRecord(const ActionRecord& rec) const {
    // Tab-separated values for simplicity and speed
    // Fields: seqId | sessionId | type | category | action | input | output | exitCode |
    //         confidence | durationMs | parentId | causedBy | agentId | metadata
    std::ostringstream oss;

    // Escape tabs and newlines in string fields
    auto escape = [](const std::string& s) -> std::string {
        std::string result;
        result.reserve(s.length());
        for (char c : s) {
            if (c == '\t')      result += "\\t";
            else if (c == '\n') result += "\\n";
            else if (c == '\r') result += "\\r";
            else if (c == '\\') result += "\\\\";
            else result += c;
        }
        return result;
    };

    oss << rec.sequenceId << "\t"
        << rec.sessionId << "\t"
        << (int)rec.type << "\t"
        << escape(rec.category) << "\t"
        << escape(rec.action) << "\t"
        << escape(rec.input) << "\t"
        << escape(rec.output) << "\t"
        << rec.exitCode << "\t"
        << std::fixed << std::setprecision(3) << rec.confidence << "\t"
        << std::fixed << std::setprecision(1) << rec.durationMs << "\t"
        << rec.parentId << "\t"
        << rec.causedBy << "\t"
        << escape(rec.agentId) << "\t"
        << escape(rec.metadata);

    return oss.str();
}

bool ReplayJournal::deserializeRecord(const std::string& line, ActionRecord& out) const {
    if (line.empty() || line[0] == '#') return false;

    // Split by tabs
    std::vector<std::string> fields;
    std::string current;
    for (size_t i = 0; i < line.length(); i++) {
        if (line[i] == '\t') {
            fields.push_back(current);
            current.clear();
        } else if (line[i] == '\\' && i + 1 < line.length()) {
            char next = line[i + 1];
            if (next == 't')       { current += '\t'; i++; }
            else if (next == 'n')  { current += '\n'; i++; }
            else if (next == 'r')  { current += '\r'; i++; }
            else if (next == '\\') { current += '\\'; i++; }
            else current += line[i];
        } else {
            current += line[i];
        }
    }
    fields.push_back(current);

    if (fields.size() < 14) return false;

    out.sequenceId  = std::stoull(fields[0]);
    out.sessionId   = std::stoull(fields[1]);
    out.type        = (ReplayActionType)std::stoi(fields[2]);
    out.category    = fields[3];
    out.action      = fields[4];
    out.input       = fields[5];
    out.output      = fields[6];
    out.exitCode    = std::stoi(fields[7]);
    out.confidence  = std::stof(fields[8]);
    out.durationMs  = std::stod(fields[9]);
    out.parentId    = std::stoull(fields[10]);
    out.causedBy    = std::stoull(fields[11]);
    out.agentId     = fields[12];
    out.metadata    = fields[13];

    return true;
}

bool ReplayJournal::writeRecordToDisk(const ActionRecord& record) {
    // Already under lock
    std::string filePath = m_journalDir + "\\replay_journal.log";
    std::ofstream ofs(filePath, std::ios::app);
    if (!ofs.is_open()) return false;
    ofs << serializeRecord(record) << "\n";
    m_recordsFlushed++;
    return true;
}

bool ReplayJournal::readRecordsFromDisk(uint64_t sessionId, std::vector<ActionRecord>& out) {
    // Already under lock
    std::string filePath = m_journalDir + "\\replay_journal.log";
    std::ifstream ifs(filePath);
    if (!ifs.is_open()) return false;

    std::string line;
    while (std::getline(ifs, line)) {
        ActionRecord rec;
        if (deserializeRecord(line, rec)) {
            if (sessionId == 0 || rec.sessionId == sessionId) {
                out.push_back(std::move(rec));
            }
        }
    }
    return true;
}
