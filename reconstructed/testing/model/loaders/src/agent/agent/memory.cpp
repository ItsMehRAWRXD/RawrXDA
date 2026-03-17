// ============================================================================
// File: src/agent/agent_memory.cpp
// Purpose: Agent Memory System implementation
// Converted from Qt/SQLite to pure C++17 with file-based storage
// ============================================================================
#include "agent_memory.hpp"
#include "../common/logger.hpp"
#include <algorithm>
#include <numeric>

AgentMemory::AgentMemory()
{
}

AgentMemory::~AgentMemory()
{
    saveData();
}

bool AgentMemory::initialize(const std::string& databasePath)
{
    std::lock_guard<std::mutex> locker(m_mutex);
    m_databasePath = databasePath;

    auto parent = FileUtils::parentPath(databasePath);
    if (!parent.empty() && !FileUtils::exists(parent)) {
        if (!FileUtils::createDirectories(parent)) {
            logWarning() << "Failed to create directory for database:" << parent;
            return false;
        }
    }

    if (FileUtils::exists(databasePath)) {
        if (!loadData()) {
            logWarning() << "Failed to load existing data, starting fresh";
        }
    }

    logDebug() << "AgentMemory initialized successfully with database:" << databasePath;
    return true;
}

void AgentMemory::recordExecution(const std::string& wish, const std::string& plan, bool success,
                                 int executionTime, const std::string& errorMessage)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    ExecutionRecord record;
    record.id = m_nextExecutionId++;
    record.wish = wish;
    record.plan = plan;
    record.success = success;
    record.executionTime = executionTime;
    record.errorMessage = errorMessage;
    record.timestamp = TimeUtils::now();

    m_executionHistory.push_back(record);
    saveData();

    onExecutionRecorded.emit(record);
}

std::vector<ExecutionRecord> AgentMemory::getExecutionHistory(int limit) const
{
    std::lock_guard<std::mutex> locker(m_mutex);

    if (static_cast<int>(m_executionHistory.size()) <= limit) {
        return m_executionHistory;
    }

    return std::vector<ExecutionRecord>(
        m_executionHistory.end() - limit, m_executionHistory.end());
}

void AgentMemory::recordPattern(const std::string& patternType, const std::string& patternData,
                               const std::string& projectContext, bool success)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    // Check if pattern exists
    auto it = std::find_if(m_patterns.begin(), m_patterns.end(),
        [&](const LearnedPattern& p) {
            return p.patternType == patternType && p.patternData == patternData &&
                   p.projectContext == projectContext;
        });

    if (it != m_patterns.end()) {
        // Update existing pattern
        // Calculate new success rate with weighted average
        double currentRate = it->successRate;
        // Estimate usage count from the data
        int usageCount = static_cast<int>(1.0 / (1.0 - currentRate + 0.01));
        double newRate = (currentRate * usageCount + (success ? 1.0 : 0.0)) / (usageCount + 1);
        it->successRate = newRate;
        it->lastUsed = TimeUtils::now();

        onPatternLearned.emit(*it);
    } else {
        // New pattern
        LearnedPattern pattern;
        pattern.id = m_nextPatternId++;
        pattern.patternType = patternType;
        pattern.patternData = patternData;
        pattern.projectContext = projectContext;
        pattern.successRate = success ? 1.0 : 0.0;
        pattern.lastUsed = TimeUtils::now();

        m_patterns.push_back(pattern);
        onPatternLearned.emit(pattern);
    }

    saveData();
}

std::vector<LearnedPattern> AgentMemory::getLearnedPatterns(const std::string& patternType) const
{
    std::lock_guard<std::mutex> locker(m_mutex);

    if (patternType.empty()) {
        return m_patterns;
    }

    std::vector<LearnedPattern> filtered;
    std::copy_if(m_patterns.begin(), m_patterns.end(), std::back_inserter(filtered),
        [&](const LearnedPattern& p) { return p.patternType == patternType; });
    return filtered;
}

void AgentMemory::updateUserPreference(const std::string& preferenceType, const std::string& preferenceValue,
                                      double confidence)
{
    std::lock_guard<std::mutex> locker(m_mutex);

    auto it = std::find_if(m_preferences.begin(), m_preferences.end(),
        [&](const UserPreference& p) { return p.preferenceType == preferenceType; });

    if (it != m_preferences.end()) {
        double newConfidence = (it->confidenceLevel + confidence) / 2.0;
        it->preferenceValue = preferenceValue;
        it->confidenceLevel = newConfidence;
        it->lastUpdated = TimeUtils::now();
        onPreferenceUpdated.emit(*it);
    } else {
        UserPreference pref;
        pref.id = m_nextPreferenceId++;
        pref.preferenceType = preferenceType;
        pref.preferenceValue = preferenceValue;
        pref.confidenceLevel = confidence;
        pref.lastUpdated = TimeUtils::now();
        m_preferences.push_back(pref);
        onPreferenceUpdated.emit(pref);
    }

    saveData();
}

std::vector<UserPreference> AgentMemory::getUserPreferences(const std::string& preferenceType) const
{
    std::lock_guard<std::mutex> locker(m_mutex);

    if (preferenceType.empty()) {
        return m_preferences;
    }

    std::vector<UserPreference> filtered;
    std::copy_if(m_preferences.begin(), m_preferences.end(), std::back_inserter(filtered),
        [&](const UserPreference& p) { return p.preferenceType == preferenceType; });
    return filtered;
}

double AgentMemory::getPatternSuccessRate(const std::string& patternType) const
{
    std::lock_guard<std::mutex> locker(m_mutex);

    int count = 0;
    double totalRate = 0.0;
    for (const auto& p : m_patterns) {
        if (p.patternType == patternType) {
            totalRate += p.successRate;
            ++count;
        }
    }

    return count > 0 ? totalRate / count : 0.0;
}

double AgentMemory::getAverageExecutionTime() const
{
    std::lock_guard<std::mutex> locker(m_mutex);

    int count = 0;
    double totalTime = 0.0;
    for (const auto& r : m_executionHistory) {
        if (r.success) {
            totalTime += r.executionTime;
            ++count;
        }
    }

    return count > 0 ? totalTime / count : 0.0;
}

double AgentMemory::getFailureRate() const
{
    std::lock_guard<std::mutex> locker(m_mutex);

    if (m_executionHistory.empty()) return 0.0;

    int failures = 0;
    for (const auto& r : m_executionHistory) {
        if (!r.success) ++failures;
    }

    return static_cast<double>(failures) / m_executionHistory.size();
}

bool AgentMemory::saveData() const
{
    if (m_databasePath.empty()) return false;

    JsonObject root;

    // Save execution history
    JsonArray execArr;
    for (const auto& r : m_executionHistory) {
        JsonObject obj;
        obj["id"] = JsonValue(r.id);
        obj["wish"] = JsonValue(r.wish);
        obj["plan"] = JsonValue(r.plan);
        obj["success"] = JsonValue(r.success);
        obj["executionTime"] = JsonValue(r.executionTime);
        obj["errorMessage"] = JsonValue(r.errorMessage);
        obj["timestamp"] = JsonValue(TimeUtils::toISOString(r.timestamp));
        execArr.push_back(JsonValue(obj));
    }
    root["executionHistory"] = JsonValue(execArr);

    // Save patterns
    JsonArray patArr;
    for (const auto& p : m_patterns) {
        JsonObject obj;
        obj["id"] = JsonValue(p.id);
        obj["patternType"] = JsonValue(p.patternType);
        obj["patternData"] = JsonValue(p.patternData);
        obj["projectContext"] = JsonValue(p.projectContext);
        obj["successRate"] = JsonValue(p.successRate);
        obj["lastUsed"] = JsonValue(TimeUtils::toISOString(p.lastUsed));
        patArr.push_back(JsonValue(obj));
    }
    root["patterns"] = JsonValue(patArr);

    // Save preferences
    JsonArray prefArr;
    for (const auto& p : m_preferences) {
        JsonObject obj;
        obj["id"] = JsonValue(p.id);
        obj["preferenceType"] = JsonValue(p.preferenceType);
        obj["preferenceValue"] = JsonValue(p.preferenceValue);
        obj["confidenceLevel"] = JsonValue(p.confidenceLevel);
        obj["lastUpdated"] = JsonValue(TimeUtils::toISOString(p.lastUpdated));
        prefArr.push_back(JsonValue(obj));
    }
    root["preferences"] = JsonValue(prefArr);

    return FileUtils::writeFile(m_databasePath, JsonSerializer::serialize(JsonValue(root)));
}

bool AgentMemory::loadData()
{
    std::string content = FileUtils::readFile(m_databasePath);
    if (content.empty()) return false;

    JsonValue root = JsonSerializer::parse(content);
    if (!root.isObject()) return false;

    // Load execution history
    if (root.contains("executionHistory")) {
        for (const auto& item : root["executionHistory"].toArray()) {
            ExecutionRecord r;
            r.id = item["id"].toInt();
            r.wish = item["wish"].toString();
            r.plan = item["plan"].toString();
            r.success = item["success"].toBool();
            r.executionTime = item["executionTime"].toInt();
            r.errorMessage = item["errorMessage"].toString();
            r.timestamp = TimeUtils::now(); // Simplified
            m_executionHistory.push_back(r);
            if (r.id >= m_nextExecutionId) m_nextExecutionId = r.id + 1;
        }
    }

    // Load patterns
    if (root.contains("patterns")) {
        for (const auto& item : root["patterns"].toArray()) {
            LearnedPattern p;
            p.id = item["id"].toInt();
            p.patternType = item["patternType"].toString();
            p.patternData = item["patternData"].toString();
            p.projectContext = item["projectContext"].toString();
            p.successRate = item["successRate"].toDouble();
            p.lastUsed = TimeUtils::now();
            m_patterns.push_back(p);
            if (p.id >= m_nextPatternId) m_nextPatternId = p.id + 1;
        }
    }

    // Load preferences
    if (root.contains("preferences")) {
        for (const auto& item : root["preferences"].toArray()) {
            UserPreference p;
            p.id = item["id"].toInt();
            p.preferenceType = item["preferenceType"].toString();
            p.preferenceValue = item["preferenceValue"].toString();
            p.confidenceLevel = item["confidenceLevel"].toDouble();
            p.lastUpdated = TimeUtils::now();
            m_preferences.push_back(p);
            if (p.id >= m_nextPreferenceId) m_nextPreferenceId = p.id + 1;
        }
    }

    return true;
}
