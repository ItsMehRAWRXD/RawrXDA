#pragma once
// AgenticLoopState — Full iterative reasoning state machine
// Synced to agentic_loop_state.cpp (701 lines)

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include "../src/nlohmann/json.hpp"

// ===== TimePoint alias =====
using TimePoint = std::chrono::system_clock::time_point;

// ===== Enums =====

enum class ReasoningPhase {
    Analysis,
    Planning,
    Execution,
    Verification,
    Reflection,
    Adjustment
};

enum class IterationStatus {
    NotStarted,
    InProgress,
    Completed,
    Failed,
    Recovering,
    MaxAttemptsReached
};

// ===== AgenticLoopState =====

class AgenticLoopState {
public:
    // ----- Inner types -----

    struct Decision {
        TimePoint timestamp;
        ReasoningPhase phase;
        std::string description;
        nlohmann::json reasoning;
        float confidence = 0.0f;
        bool success = false;
        int retryCount = 0;
        nlohmann::json outcome;
    };

    struct ErrorRecord {
        TimePoint timestamp;
        std::string errorType;
        std::string errorMessage;
        std::string stackTrace;
        ReasoningPhase phase;
        int recoveryAttempt = 0;
        bool recoverySucceeded = false;
        std::string recoveryStrategy;
    };

    struct Iteration {
        int iterationNumber = 0;
        TimePoint startTime;
        TimePoint endTime;
        ReasoningPhase currentPhase = ReasoningPhase::Analysis;
        IterationStatus status = IterationStatus::NotStarted;
        std::string goalStatement;
        nlohmann::json contextSnapshot;
        float successScore = 0.0f;
        int errorCount = 0;
        std::string resultSummary;
        std::vector<Decision> decisions;
        std::vector<std::string> appliedStrategies;

        // Legacy compat aliases
        std::string goal;
        std::string outcome;
        std::string reasoning;
        std::string error;
    };

    // ----- Construction / Destruction -----
    AgenticLoopState();
    ~AgenticLoopState();

    // ----- Iteration management -----
    void startIteration(const std::string& goal);
    void endIteration(IterationStatus status, const std::string& result);
    Iteration* getCurrentIteration();

    // ----- Phase management -----
    void setCurrentPhase(ReasoningPhase phase);
    float getPhaseProgress() const;
    std::vector<std::string> getAllPhaseTransitions() const;

    // ----- Decision tracking -----
    void recordDecision(const std::string& description, const nlohmann::json& reasoning, float confidence);
    void recordDecisionOutcome(int decisionIndex, const nlohmann::json& outcome, bool success);
    std::vector<Decision> getDecisionHistory(int limit = 0) const;
    Decision* getCurrentDecision();
    float getAverageDecisionConfidence() const;
    float getDecisionSuccessRate() const;

    // ----- Error tracking -----
    void recordError(const std::string& errorType, const std::string& message, const std::string& stackTrace = "");
    void recordErrorRecovery(int errorIndex, const std::string& strategy, bool succeeded);
    const std::deque<ErrorRecord>& getErrorHistory(size_t limit = 0) const;
    float getErrorRate() const;
    std::string generateErrorAnalysis() const;

    // ----- Snapshots -----
    nlohmann::json takeSnapshot();
    bool restoreFromSnapshot(const nlohmann::json& snapshot);
    nlohmann::json getLastSnapshot() const { return m_lastSnapshot; }

    // ----- Memory management (json-based) -----
    void addToMemory(const std::string& key, const nlohmann::json& value);
    nlohmann::json getFromMemory(const std::string& key);
    void removeFromMemory(const std::string& key);
    nlohmann::json getAllMemory() const;
    void clearMemoryExcept(const std::vector<std::string>& keysToKeep);

    // Legacy string-based aliases (inline delegation)
    void addMemory(const std::string& key, const std::string& value) { addToMemory(key, nlohmann::json(value)); }
    std::string getMemory(const std::string& key) const {
        auto it = m_memory.find(key);
        if (it != m_memory.end() && it->second.is_string()) return it->second.get<std::string>();
        return {};
    }
    void clearMemory() { m_memory.clear(); }

    // ----- Context window -----
    void setContextWindowSize(int size);
    nlohmann::json getContextWindow() const;
    std::string formatContextForModel() const;

    // ----- Goal / Progress -----
    void updateProgress(int current, int total);
    float getProgressPercentage() const;
    nlohmann::json getProgressInfo() const;

    // ----- Constraints -----
    void addConstraint(const std::string& key, const std::string& constraint);
    void removeConstraint(const std::string& key);
    bool validateAgainstConstraints(const nlohmann::json& action) const;

    // ----- Strategy tracking -----
    void recordAppliedStrategy(const std::string& strategy);
    void setSuggestedStrategies(const std::vector<std::string>& strategies);

    // ----- Metrics / Stats -----
    nlohmann::json getMetrics() const;
    int getCompletedIterations() const;
    int getFailedIterations() const;
    float getOverallSuccessRate() const;
    int getTotalIterations() const { return static_cast<int>(m_iterations.size()); }
    int getTotalErrorCount() const { return static_cast<int>(m_errorHistory.size()); }

    // ----- Serialization -----
    std::string getStateAsSummary() const;
    std::string serializeState() const;
    bool deserializeState(const std::string& jsonStr);

    // ----- Debugging -----
    std::string generateDebugReport() const;

    // ----- Simple getters -----
    ReasoningPhase getCurrentPhase() const { return m_currentPhase; }
    IterationStatus getCurrentStatus() const { return m_currentStatus; }
    int getIterationCount() const { return static_cast<int>(m_iterations.size()); }
    const std::vector<Iteration>& getIterations() const { return m_iterations; }

private:
    // ----- Helpers -----
    std::string timePointToISO(const TimePoint& tp) const;
    std::string timePointToHMS(const TimePoint& tp) const;
    std::string phaseToString(ReasoningPhase phase) const;
    std::string statusToString(IterationStatus status) const;
    ReasoningPhase stringToPhase(const std::string& str) const;
    IterationStatus stringToStatus(const std::string& str) const;

    // ----- State -----
    ReasoningPhase m_currentPhase;
    IterationStatus m_currentStatus;
    TimePoint m_stateStartTime;
    TimePoint m_lastUpdateTime;

    // ----- Core data -----
    std::vector<Iteration> m_iterations;
    std::unordered_map<std::string, nlohmann::json> m_memory;
    std::deque<Iteration> m_contextWindow;
    int m_contextWindowSize = 10;
    std::deque<ErrorRecord> m_errorHistory;

    // ----- Progress -----
    int m_progressCurrent = 0;
    int m_progressTotal = 0;
    std::string m_currentGoal;

    // ----- Strategy / Constraints -----
    std::vector<std::string> m_appliedStrategies;
    std::vector<std::string> m_suggestedStrategies;
    nlohmann::json m_constraints;

    // ----- Snapshots -----
    nlohmann::json m_lastSnapshot;
    std::vector<nlohmann::json> m_snapshotHistory;

    // ----- Debug -----
    bool m_debugMode = false;
};
