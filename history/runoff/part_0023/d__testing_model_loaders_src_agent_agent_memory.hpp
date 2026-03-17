// ============================================================================
// File: src/agent/agent_memory.hpp
// Purpose: Agent Memory System for Persistent Learning
// Converted from Qt to pure C++17 (SQLite replaced with file-based storage)
// ============================================================================
#pragma once

#include "../common/json_types.hpp"
#include "../common/callback_system.hpp"
#include "../common/time_utils.hpp"
#include "../common/string_utils.hpp"
#include "../common/file_utils.hpp"
#include <string>
#include <vector>
#include <mutex>
#include <memory>

struct ExecutionRecord {
    int id;
    std::string wish;
    std::string plan;
    bool success;
    int executionTime;
    std::string errorMessage;
    TimePoint timestamp;
};

struct LearnedPattern {
    int id;
    std::string patternType;
    std::string patternData;
    std::string projectContext;
    double successRate;
    TimePoint lastUsed;
};

struct UserPreference {
    int id;
    std::string preferenceType;
    std::string preferenceValue;
    double confidenceLevel;
    TimePoint lastUpdated;
};

class AgentMemory {
public:
    AgentMemory();
    ~AgentMemory();

    bool initialize(const std::string& databasePath);

    void recordExecution(const std::string& wish, const std::string& plan, bool success,
                        int executionTime, const std::string& errorMessage = "");
    std::vector<ExecutionRecord> getExecutionHistory(int limit = 100) const;

    void recordPattern(const std::string& patternType, const std::string& patternData,
                      const std::string& projectContext, bool success);
    std::vector<LearnedPattern> getLearnedPatterns(const std::string& patternType = "") const;

    void updateUserPreference(const std::string& preferenceType, const std::string& preferenceValue,
                             double confidence);
    std::vector<UserPreference> getUserPreferences(const std::string& preferenceType = "") const;

    double getPatternSuccessRate(const std::string& patternType) const;
    double getAverageExecutionTime() const;
    double getFailureRate() const;

    // Callbacks (replacing Qt signals)
    CallbackList<const ExecutionRecord&> onExecutionRecorded;
    CallbackList<const LearnedPattern&> onPatternLearned;
    CallbackList<const UserPreference&> onPreferenceUpdated;

private:
    bool saveData() const;
    bool loadData();

    std::vector<ExecutionRecord> m_executionHistory;
    std::vector<LearnedPattern> m_patterns;
    std::vector<UserPreference> m_preferences;
    std::string m_databasePath;
    mutable std::mutex m_mutex;
    int m_nextExecutionId = 1;
    int m_nextPatternId = 1;
    int m_nextPreferenceId = 1;
};
