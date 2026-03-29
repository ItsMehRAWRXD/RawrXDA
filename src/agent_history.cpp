// ============================================================================
// agent_history.cpp — Agent Event History, Timeline & Replay Implementation
// ============================================================================
// Append-only JSONL persistence, in-memory query, and deterministic replay.
// ============================================================================

#include "agent_history.h"
#include "subagent_core.h"
#include <algorithm>
#include <ctime>
#include <filesystem>
#include <random>

namespace fs = std::filesystem;

// ============================================================================
// Helpers — JSON serialization (self-contained, no external deps)
// ============================================================================

std::string AgentHistoryRecorder::escapeJsonValue(const std::string& s) const {
    std::string out;
    out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"':  out += "\\\""; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    snprintf(buf, sizeof(buf), "\\u%04x", (int)(unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

std::string AgentHistoryRecorder::unescapeJsonValue(const std::string& s) const {
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            switch (s[i + 1]) {
                case '\\': out += '\\'; i++; break;
                case '"':  out += '"';  i++; break;
                case 'n':  out += '\n'; i++; break;
                case 'r':  out += '\r'; i++; break;
                case 't':  out += '\t'; i++; break;
                case 'u':  out += '?';  i += 5; break; // simplified
                default:   out += s[i]; break;
            }
        } else {
            out += s[i];
        }
    }
    return out;
}

std::string AgentHistoryRecorder::truncate(const std::string& s, size_t maxLen) const {
    if (s.size() <= maxLen) return s;
    return s.substr(0, maxLen) + "...[truncated]";
}

int64_t AgentHistoryRecorder::nowMs() const {
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count();
}

std::string AgentHistoryRecorder::generateSessionId() const {
    auto ms = nowMs();
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dis(1000, 9999);
    std::ostringstream ss;
    ss << "ses-" << ms << "-" << dis(gen);
    return ss.str();
}

std::string AgentHistoryRecorder::getLogFilePath() const {
    if (m_storageDir.empty()) return "";
    return m_storageDir + "/agent_history.jsonl";
}

// ============================================================================
// AgentEvent — JSON serialization
// ============================================================================

std::string AgentEvent::toJSON() const {
    // We use a lambda capture to access the same escapeJsonValue logic
    // Since this is a standalone struct method, we inline the escape here
    auto esc = [](const std::string& s) -> std::string {
        std::string out;
        out.reserve(s.size() + 16);
        for (char c : s) {
            switch (c) {
                case '\\': out += "\\\\"; break;
                case '"':  out += "\\\""; break;
                case '\n': out += "\\n";  break;
                case '\r': out += "\\r";  break;
                case '\t': out += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (int)(unsigned char)c);
                        out += buf;
                    } else {
                        out += c;
                    }
            }
        }
        return out;
    };

    std::ostringstream j;
    j << "{";
    j << "\"id\":" << id;
    j << ",\"eventType\":\"" << esc(eventType) << "\"";
    j << ",\"sessionId\":\"" << esc(sessionId) << "\"";
    j << ",\"timestampMs\":" << timestampMs;
    j << ",\"durationMs\":" << durationMs;
    j << ",\"agentId\":\"" << esc(agentId) << "\"";
    j << ",\"parentId\":\"" << esc(parentId) << "\"";
    j << ",\"description\":\"" << esc(description) << "\"";
    j << ",\"input\":\"" << esc(input) << "\"";
    j << ",\"output\":\"" << esc(output) << "\"";
    j << ",\"metadata\":\"" << esc(metadata) << "\"";
    j << ",\"success\":" << (success ? "true" : "false");
    j << ",\"errorMessage\":\"" << esc(errorMessage) << "\"";
    j << "}";
    return j.str();
}

// Minimal JSON parser for AgentEvent (handles our known flat schema)
static std::string extractJsonStr(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos += needle.size();
    
    std::string result;
    for (size_t i = pos; i < json.size(); i++) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            switch (json[i + 1]) {
                case '\\': result += '\\'; i++; break;
                case '"':  result += '"';  i++; break;
                case 'n':  result += '\n'; i++; break;
                case 'r':  result += '\r'; i++; break;
                case 't':  result += '\t'; i++; break;
                default:   result += json[i]; break;
            }
        } else if (json[i] == '"') {
            break;
        } else {
            result += json[i];
        }
    }
    return result;
}

static int64_t extractJsonInt(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return 0;
    pos += needle.size();
    // Skip whitespace
    while (pos < json.size() && json[pos] == ' ') pos++;
    std::string numStr;
    while (pos < json.size() && (isdigit(json[pos]) || json[pos] == '-')) {
        numStr += json[pos++];
    }
    if (numStr.empty()) return 0;
    return std::stoll(numStr);
}

static bool extractJsonBool(const std::string& json, const std::string& key) {
    std::string needle = "\"" + key + "\":";
    auto pos = json.find(needle);
    if (pos == std::string::npos) return false;
    pos += needle.size();
    while (pos < json.size() && json[pos] == ' ') pos++;
    return (pos < json.size() && json[pos] == 't');
}

AgentEvent AgentEvent::fromJSON(const std::string& json) {
    AgentEvent e;
    e.id           = extractJsonInt(json, "id");
    e.eventType    = extractJsonStr(json, "eventType");
    e.sessionId    = extractJsonStr(json, "sessionId");
    e.timestampMs  = extractJsonInt(json, "timestampMs");
    e.durationMs   = (int)extractJsonInt(json, "durationMs");
    e.agentId      = extractJsonStr(json, "agentId");
    e.parentId     = extractJsonStr(json, "parentId");
    e.description  = extractJsonStr(json, "description");
    e.input        = extractJsonStr(json, "input");
    e.output       = extractJsonStr(json, "output");
    e.metadata     = extractJsonStr(json, "metadata");
    e.success      = extractJsonBool(json, "success");
    e.errorMessage = extractJsonStr(json, "errorMessage");
    return e;
}

// ============================================================================
// AgentHistoryRecorder — constructor / destructor
// ============================================================================

AgentHistoryRecorder::AgentHistoryRecorder(const std::string& storageDir)
    : m_storageDir(storageDir)
{
    m_sessionId = generateSessionId();
    
    // Create storage directory if needed
    if (!m_storageDir.empty()) {
        try {
            fs::create_directories(m_storageDir);
        } catch (const std::exception& ex) {
            // Will log when callback is set
        }
    }
    
    // Load existing events from disk
    loadFromDisk();
}

AgentHistoryRecorder::~AgentHistoryRecorder() {
    flush();
}

// ============================================================================
// Recording — core record() method
// ============================================================================

int64_t AgentHistoryRecorder::record(
    const std::string& eventType,
    const std::string& agentId,
    const std::string& parentId,
    const std::string& description,
    const std::string& input,
    const std::string& output,
    bool success,
    int durationMs,
    const std::string& errorMessage,
    const std::string& metadata)
{
    AgentEvent e;
    e.id           = m_nextId.fetch_add(1);
    e.eventType    = eventType;
    e.sessionId    = m_sessionId;
    e.timestampMs  = nowMs();
    e.durationMs   = durationMs;
    e.agentId      = agentId;
    e.parentId     = parentId;
    e.description  = description;
    e.input        = truncate(input, m_maxPayloadBytes);
    e.output       = truncate(output, m_maxPayloadBytes);
    e.metadata     = metadata;
    e.success      = success;
    e.errorMessage = errorMessage;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events.push_back(e);
    }

    // Append to JSONL file
    std::string logFile = getLogFilePath();
    if (!logFile.empty()) {
        try {
            std::ofstream f(logFile, std::ios::app);
            if (f.is_open()) {
                f << e.toJSON() << "\n";
                f.flush();
            }
        } catch (const std::exception& ex) {
            logError("Failed to write event to JSONL: " + std::string(ex.what()));
        }
    }

    logDebug("[History] " + eventType + " id=" + std::to_string(e.id) +
             " agent=" + agentId + " " + description);

    return e.id;
}

// ============================================================================
// Convenience recorders
// ============================================================================

int64_t AgentHistoryRecorder::recordAgentSpawn(
    const std::string& agentId, const std::string& parentId,
    const std::string& description, const std::string& prompt)
{
    return record("agent_spawn", agentId, parentId, description, prompt, "", true);
}

int64_t AgentHistoryRecorder::recordAgentComplete(
    const std::string& agentId, const std::string& result, int durationMs)
{
    return record("agent_complete", agentId, "", "Agent completed", "", result, true, durationMs);
}

int64_t AgentHistoryRecorder::recordAgentFail(
    const std::string& agentId, const std::string& error, int durationMs)
{
    return record("agent_fail", agentId, "", "Agent failed", "", "", false, durationMs, error);
}

int64_t AgentHistoryRecorder::recordAgentCancel(const std::string& agentId) {
    return record("agent_cancel", agentId, "", "Agent cancelled", "", "", false);
}

int64_t AgentHistoryRecorder::recordToolInvoke(
    const std::string& agentId, const std::string& toolName, const std::string& input)
{
    std::string meta = "{\"tool\":\"" + escapeJsonValue(toolName) + "\"}";
    return record("tool_invoke", agentId, "", "Tool invocation: " + toolName, input, "", true, 0, "", meta);
}

int64_t AgentHistoryRecorder::recordToolResult(
    const std::string& agentId, const std::string& toolName,
    const std::string& result, bool success)
{
    std::string meta = "{\"tool\":\"" + escapeJsonValue(toolName) + "\"}";
    return record("tool_result", agentId, "", "Tool result: " + toolName, "", result, success, 0, "", meta);
}

int64_t AgentHistoryRecorder::recordChainStart(const std::string& parentId, int stepCount) {
    std::string meta = "{\"stepCount\":" + std::to_string(stepCount) + "}";
    return record("chain_start", "", parentId, "Chain started with " + std::to_string(stepCount) + " steps",
                   "", "", true, 0, "", meta);
}

int64_t AgentHistoryRecorder::recordChainStep(
    const std::string& parentId, int stepIndex,
    const std::string& agentId, const std::string& result,
    bool success, int durationMs)
{
    std::string meta = "{\"stepIndex\":" + std::to_string(stepIndex) + "}";
    return record("chain_step", agentId, parentId,
                   "Chain step " + std::to_string(stepIndex) + " completed",
                   "", result, success, durationMs, "", meta);
}

int64_t AgentHistoryRecorder::recordChainComplete(
    const std::string& parentId, const std::string& result,
    int stepCount, int totalDurationMs)
{
    std::string meta = "{\"stepCount\":" + std::to_string(stepCount) + "}";
    return record("chain_complete", "", parentId,
                   "Chain completed: " + std::to_string(stepCount) + " steps",
                   "", result, true, totalDurationMs, "", meta);
}

int64_t AgentHistoryRecorder::recordSwarmStart(
    const std::string& parentId, int taskCount, const std::string& strategy)
{
    std::string meta = "{\"taskCount\":" + std::to_string(taskCount) +
                       ",\"strategy\":\"" + escapeJsonValue(strategy) + "\"}";
    return record("swarm_start", "", parentId,
                   "Swarm started: " + std::to_string(taskCount) + " tasks, strategy=" + strategy,
                   "", "", true, 0, "", meta);
}

int64_t AgentHistoryRecorder::recordSwarmTask(
    const std::string& parentId, const std::string& taskId,
    const std::string& agentId, const std::string& result,
    bool success, int durationMs)
{
    std::string meta = "{\"taskId\":\"" + escapeJsonValue(taskId) + "\"}";
    return record("swarm_task", agentId, parentId,
                   "Swarm task completed: " + taskId,
                   "", result, success, durationMs, "", meta);
}

int64_t AgentHistoryRecorder::recordSwarmComplete(
    const std::string& parentId, const std::string& mergedResult,
    int taskCount, const std::string& strategy, int totalDurationMs)
{
    std::string meta = "{\"taskCount\":" + std::to_string(taskCount) +
                       ",\"strategy\":\"" + escapeJsonValue(strategy) + "\"}";
    return record("swarm_complete", "", parentId,
                   "Swarm completed: " + std::to_string(taskCount) + " tasks merged",
                   "", mergedResult, true, totalDurationMs, "", meta);
}

int64_t AgentHistoryRecorder::recordChatRequest(const std::string& message) {
    return record("chat_request", "", "", "Chat request", message, "", true);
}

int64_t AgentHistoryRecorder::recordChatResponse(const std::string& response, int durationMs) {
    return record("chat_response", "", "", "Chat response", "", response, true, durationMs);
}

int64_t AgentHistoryRecorder::recordTodoUpdate(int todoId, const std::string& title, const std::string& status) {
    std::string meta = "{\"todoId\":" + std::to_string(todoId) +
                       ",\"status\":\"" + escapeJsonValue(status) + "\"}";
    return record("todo_update", "", "", "Todo updated: " + title, "", status, true, 0, "", meta);
}

// ============================================================================
// Querying
// ============================================================================

std::vector<AgentEvent> AgentHistoryRecorder::query(const HistoryQuery& q) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::vector<AgentEvent> results;
    int skipped = 0;
    
    for (const auto& e : m_events) {
        // Apply filters
        if (!q.sessionId.empty() && e.sessionId != q.sessionId) continue;
        if (!q.agentId.empty() && e.agentId != q.agentId) continue;
        if (!q.eventType.empty() && e.eventType != q.eventType) continue;
        if (!q.parentId.empty() && e.parentId != q.parentId) continue;
        if (q.afterTimestamp > 0 && e.timestampMs <= q.afterTimestamp) continue;
        if (q.beforeTimestamp > 0 && e.timestampMs >= q.beforeTimestamp) continue;
        if (q.successOnly && !e.success) continue;
        
        // Apply offset
        if (skipped < q.offset) {
            skipped++;
            continue;
        }
        
        results.push_back(e);
        if ((int)results.size() >= q.limit) break;
    }
    
    return results;
}

std::vector<AgentEvent> AgentHistoryRecorder::getAgentTimeline(const std::string& agentId) const {
    HistoryQuery q;
    q.agentId = agentId;
    q.limit = 1000;
    return query(q);
}

std::vector<AgentEvent> AgentHistoryRecorder::getSessionTimeline(const std::string& sessionId) const {
    HistoryQuery q;
    q.sessionId = sessionId;
    q.limit = 10000;
    return query(q);
}

std::vector<AgentEvent> AgentHistoryRecorder::getEventsByType(const std::string& eventType, int limit) const {
    HistoryQuery q;
    q.eventType = eventType;
    q.limit = limit;
    return query(q);
}

std::vector<AgentEvent> AgentHistoryRecorder::getChainEvents(const std::string& parentId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AgentEvent> results;
    for (const auto& e : m_events) {
        if (e.parentId == parentId &&
            (e.eventType == "chain_start" || e.eventType == "chain_step" || e.eventType == "chain_complete" || e.eventType == "chain_fail")) {
            results.push_back(e);
        }
    }
    return results;
}

std::vector<AgentEvent> AgentHistoryRecorder::getSwarmEvents(const std::string& parentId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<AgentEvent> results;
    for (const auto& e : m_events) {
        if (e.parentId == parentId &&
            (e.eventType == "swarm_start" || e.eventType == "swarm_task" || e.eventType == "swarm_complete")) {
            results.push_back(e);
        }
    }
    return results;
}

std::vector<AgentEvent> AgentHistoryRecorder::allEvents() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_events;
}

size_t AgentHistoryRecorder::eventCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_events.size();
}

std::string AgentHistoryRecorder::toJSON(const std::vector<AgentEvent>& events) const {
    std::ostringstream ss;
    ss << "[";
    for (size_t i = 0; i < events.size(); i++) {
        if (i > 0) ss << ",";
        ss << events[i].toJSON();
    }
    ss << "]";
    return ss.str();
}

std::string AgentHistoryRecorder::getStatsSummary() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::unordered_map<std::string, int> typeCounts;
    int totalSuccess = 0, totalFail = 0;
    int64_t minTs = INT64_MAX, maxTs = 0;
    
    for (const auto& e : m_events) {
        typeCounts[e.eventType]++;
        if (e.success) totalSuccess++; else totalFail++;
        if (e.timestampMs < minTs) minTs = e.timestampMs;
        if (e.timestampMs > maxTs) maxTs = e.timestampMs;
    }
    
    std::ostringstream ss;
    ss << "{\"totalEvents\":" << m_events.size();
    ss << ",\"sessionId\":\"" << m_sessionId << "\"";
    ss << ",\"successCount\":" << totalSuccess;
    ss << ",\"failCount\":" << totalFail;
    if (!m_events.empty()) {
        ss << ",\"timeSpanMs\":" << (maxTs - minTs);
    }
    ss << ",\"eventTypes\":{";
    bool first = true;
    for (const auto& [type, count] : typeCounts) {
        if (!first) ss << ",";
        ss << "\"" << type << "\":" << count;
        first = false;
    }
    ss << "}}";
    return ss.str();
}

// ============================================================================
// Replay
// ============================================================================

ReplayResult AgentHistoryRecorder::replay(const ReplayRequest& req, SubAgentManager* mgr) {
    ReplayResult result;
    result.success = false;
    result.eventsReplayed = 0;
    result.durationMs = 0;
    
    if (!mgr) {
        result.result = "No SubAgentManager provided for replay";
        return result;
    }
    
    auto startTime = std::chrono::steady_clock::now();
    
    // Collect the original events to replay
    std::vector<AgentEvent> originalEvents;
    
    if (!req.originalAgentId.empty()) {
        originalEvents = getAgentTimeline(req.originalAgentId);
    } else if (!req.originalSessionId.empty()) {
        originalEvents = getSessionTimeline(req.originalSessionId);
    }
    
    if (originalEvents.empty()) {
        result.result = "No events found for replay target";
        return result;
    }
    
    logInfo("[Replay] Replaying " + std::to_string(originalEvents.size()) +
            " events from agent=" + req.originalAgentId +
            " session=" + req.originalSessionId);
    
    // Store original final result for comparison
    for (auto it = originalEvents.rbegin(); it != originalEvents.rend(); ++it) {
        if (!it->output.empty()) {
            result.originalResult = it->output;
            break;
        }
    }
    
    if (req.dryRun) {
        result.success = true;
        result.eventsReplayed = (int)originalEvents.size();
        result.result = "Dry run: would replay " + std::to_string(originalEvents.size()) + " events";
        result.replayEvents = originalEvents;
        return result;
    }
    
    // Determine replay strategy from events
    bool hasChain = false, hasSwarm = false;
    std::string replayInput;
    
    for (const auto& e : originalEvents) {
        if (e.eventType == "chain_start") hasChain = true;
        if (e.eventType == "swarm_start") hasSwarm = true;
        if (e.eventType == "agent_spawn" && replayInput.empty()) {
            replayInput = e.input;
        }
    }
    
    // Re-execute based on event pattern
    if (hasChain) {
        // Rebuild chain step prompts from recorded events
        std::vector<std::string> stepPrompts;
        std::string chainParentId = "replay";
        for (const auto& e : originalEvents) {
            if (e.eventType == "chain_step" || e.eventType == "agent_spawn") {
                std::string stepPrompt = e.input.empty() ? e.description : e.input;
                if (!stepPrompt.empty()) stepPrompts.push_back(stepPrompt);
            }
            if (e.eventType == "chain_start" && !e.parentId.empty()) {
                chainParentId = e.parentId;
            }
        }
        
        if (!stepPrompts.empty()) {
            record("chain_start", "", "replay", "Replay chain started",
                   "", "", true, 0, "",
                   "{\"originalAgent\":\"" + escapeJsonValue(req.originalAgentId) + "\"}");
            
            std::string chainResult = mgr->executeChain("replay", stepPrompts);
            
            result.success = true;
            result.result = chainResult;
            result.eventsReplayed = (int)stepPrompts.size();
        }
    } else if (hasSwarm) {
        // Rebuild swarm prompts from recorded events
        std::vector<std::string> swarmPrompts;
        SwarmConfig config;
        
        for (const auto& e : originalEvents) {
            if (e.eventType == "swarm_task" || e.eventType == "agent_spawn") {
                std::string taskPrompt = e.input.empty() ? e.description : e.input;
                if (!taskPrompt.empty()) swarmPrompts.push_back(taskPrompt);
            }
            if (e.eventType == "swarm_start") {
                // Extract strategy from metadata
                auto stratPos = e.metadata.find("\"strategy\":\"");
                if (stratPos != std::string::npos) {
                    auto start = stratPos + 12;
                    auto end = e.metadata.find('"', start);
                    if (end != std::string::npos) {
                        config.mergeStrategy = e.metadata.substr(start, end - start);
                    }
                }
            }
        }
        
        if (!swarmPrompts.empty()) {
            if (config.mergeStrategy.empty()) config.mergeStrategy = "concatenate";
            
            record("swarm_start", "", "replay", "Replay swarm started",
                   "", "", true, 0, "",
                   "{\"originalAgent\":\"" + escapeJsonValue(req.originalAgentId) + "\"}");
            
            std::string swarmResult = mgr->executeSwarm("replay", swarmPrompts, config);
            
            result.success = true;
            result.result = swarmResult;
            result.eventsReplayed = (int)swarmPrompts.size();
        }
    } else if (!replayInput.empty()) {
        // Simple agent re-run
        record("agent_spawn", "replay-" + req.originalAgentId, "replay",
               "Replay agent spawn", replayInput, "", true);
        
        std::string agentResult = mgr->spawnSubAgent("replay-" + req.originalAgentId, replayInput, "replay");
        
        result.success = true;
        result.result = agentResult;
        result.eventsReplayed = 1;
    } else {
        result.result = "Could not determine replay strategy from recorded events";
        return result;
    }
    
    auto endTime = std::chrono::steady_clock::now();
    result.durationMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
    
    logInfo("[Replay] Complete: " + std::to_string(result.eventsReplayed) +
            " events replayed in " + std::to_string(result.durationMs) + "ms");
    
    return result;
}

// ============================================================================
// Persistence — JSONL load / flush / purge
// ============================================================================

void AgentHistoryRecorder::flush() {
    std::string logFile = getLogFilePath();
    if (logFile.empty()) return;
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    try {
        // Rewrite the entire file (for compaction after purge)
        std::ofstream f(logFile, std::ios::trunc);
        if (f.is_open()) {
            for (const auto& e : m_events) {
                f << e.toJSON() << "\n";
            }
            f.flush();
        }
        logDebug("[History] Flushed " + std::to_string(m_events.size()) + " events to disk");
    } catch (const std::exception& ex) {
        logError("Failed to flush history: " + std::string(ex.what()));
    }
}

void AgentHistoryRecorder::loadFromDisk() {
    std::string logFile = getLogFilePath();
    if (logFile.empty()) return;
    
    if (!fs::exists(logFile)) {
        logDebug("[History] No existing log file found at " + logFile);
        return;
    }
    
    try {
        std::ifstream f(logFile);
        if (!f.is_open()) return;
        
        std::string line;
        int loaded = 0;
        int64_t maxId = 0;
        
        while (std::getline(f, line)) {
            if (line.empty()) continue;
            try {
                AgentEvent e = AgentEvent::fromJSON(line);
                if (e.id > maxId) maxId = e.id;
                
                std::lock_guard<std::mutex> lock(m_mutex);
                m_events.push_back(std::move(e));
                loaded++;
            } catch (...) {
                logError("[History] Failed to parse event line: " + line.substr(0, 80));
            }
        }
        
        // Update next ID to be beyond loaded events
        if (maxId >= m_nextId.load()) {
            m_nextId.store(maxId + 1);
        }
        
        logInfo("[History] Loaded " + std::to_string(loaded) + " events from disk");
    } catch (const std::exception& ex) {
        logError("Failed to load history: " + std::string(ex.what()));
    }
}

void AgentHistoryRecorder::purgeExpired() {
    if (m_retentionDays <= 0) return;
    
    int64_t cutoff = nowMs() - ((int64_t)m_retentionDays * 24LL * 60LL * 60LL * 1000LL);
    
    std::lock_guard<std::mutex> lock(m_mutex);
    
    size_t before = m_events.size();
    m_events.erase(
        std::remove_if(m_events.begin(), m_events.end(),
                        [cutoff](const AgentEvent& e) { return e.timestampMs < cutoff; }),
        m_events.end());
    
    size_t after = m_events.size();
    if (before != after) {
        logInfo("[History] Purged " + std::to_string(before - after) +
                " expired events (retention=" + std::to_string(m_retentionDays) + " days)");
    }
}

void AgentHistoryRecorder::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_events.clear();
    logInfo("[History] Event log cleared");
}

size_t AgentHistoryRecorder::compact(size_t keepLast) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (keepLast == 0) {
        size_t removed = m_events.size();
        m_events.clear();
        logInfo("[History] Compaction removed all events (keepLast=0)");
        return removed;
    }
    if (m_events.size() <= keepLast) {
        return 0;
    }
    size_t removed = m_events.size() - keepLast;
    m_events.erase(m_events.begin(), m_events.begin() + static_cast<std::ptrdiff_t>(removed));
    logInfo("[History] Compaction removed " + std::to_string(removed) + " events (keepLast=" +
            std::to_string(keepLast) + ")");
    return removed;
}

bool AgentHistoryRecorder::loadFromLogFile(const std::string& logFilePath) {
    if (logFilePath.empty()) return false;
    if (!fs::exists(logFilePath)) {
        logError("[History] Log file not found: " + logFilePath);
        return false;
    }

    std::ifstream f(logFilePath);
    if (!f.is_open()) {
        logError("[History] Failed to open log file: " + logFilePath);
        return false;
    }

    std::vector<AgentEvent> loaded;
    std::string line;
    int loadedCount = 0;
    int64_t maxId = 0;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        try {
            AgentEvent e = AgentEvent::fromJSON(line);
            if (e.id > maxId) maxId = e.id;
            loaded.push_back(std::move(e));
            loadedCount++;
        } catch (...) {
            logError("[History] Failed to parse event line: " + line.substr(0, 80));
        }
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_events = std::move(loaded);
        m_nextId.store(maxId + 1);
    }

    logInfo("[History] Loaded " + std::to_string(loadedCount) + " events from " + logFilePath);
    return true;
}
