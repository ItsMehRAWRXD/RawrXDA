#pragma once

#include <string>
#include <vector>
#include <deque>
#include <map>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include <nlohmann/json.hpp>

using TimePoint = std::chrono::system_clock::time_point;

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

class AgenticLoopState {
public:
    // ===== Inner Types =====

    struct Iteration {
        int iterationNumber = 0;
        TimePoint startTime;
        TimePoint endTime;
        ReasoningPhase currentPhase = ReasoningPhase::Analysis;
        IterationStatus status = IterationStatus::NotStarted;
        std::string goal;
        std::string goalStatement;
        nlohmann::json contextSnapshot;
        float successScore = 0.0f;
        int errorCount = 0;
        std::string outcome;
        std::string reasoning;
        std::string error;
        std::string resultSummary;
        std::vector<struct Decision> decisions;
        std::vector<std::string> appliedStrategies;
    };

    struct Decision {
        TimePoint timestamp;
        ReasoningPhase phase = ReasoningPhase::Analysis;
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
        ReasoningPhase phase = ReasoningPhase::Analysis;
        int recoveryAttempt = 0;
        bool recoverySucceeded = false;
        std::string recoveryStrategy;
    };

    // ===== Constructor / Destructor =====

    AgenticLoopState();
    ~AgenticLoopState();

    // ===== Iteration Management =====

    void startIteration(const std::string& goal);
    void endIteration(IterationStatus status, const std::string& result);
    Iteration* getCurrentIteration();

    // ===== Phase Management =====

    void setCurrentPhase(ReasoningPhase phase);
    float getPhaseProgress() const;
    std::vector<std::string> getAllPhaseTransitions() const;

    // ===== Decision Tracking =====

    void recordDecision(const std::string& description,
                        const nlohmann::json& reasoning,
                        float confidence);
    void recordDecisionOutcome(int decisionIndex,
                               const nlohmann::json& outcome,
                               bool success);
    std::vector<Decision> getDecisionHistory(int limit = -1) const;
    Decision* getCurrentDecision();
    float getAverageDecisionConfidence() const;
    float getDecisionSuccessRate() const;

    // ===== Error Tracking =====

    void recordError(const std::string& errorType,
                     const std::string& message,
                     const std::string& stackTrace);
    void recordErrorRecovery(int errorIndex,
                             const std::string& strategy,
                             bool succeeded);
    const std::deque<ErrorRecord>& getErrorHistory(size_t limit = 0) const;
    float getErrorRate() const;
    std::string generateErrorAnalysis() const;

    // ===== State Snapshots =====

    nlohmann::json takeSnapshot();
    bool restoreFromSnapshot(const nlohmann::json& snapshot);

    // ===== Memory Management =====

    void addToMemory(const std::string& key, const nlohmann::json& value);
    nlohmann::json getFromMemory(const std::string& key);
    void removeFromMemory(const std::string& key);
    nlohmann::json getAllMemory() const;
    void clearMemoryExcept(const std::vector<std::string>& keysToKeep);

    // ===== Context Window =====

    void setContextWindowSize(int size);
    nlohmann::json getContextWindow() const;
    std::string formatContextForModel() const;

    // ===== Goal and Progress =====

    void updateProgress(int current, int total);
    float getProgressPercentage() const;
    nlohmann::json getProgressInfo() const;

    // ===== Constraints =====

    void addConstraint(const std::string& key, const std::string& constraint);
    void removeConstraint(const std::string& key);
    bool validateAgainstConstraints(const nlohmann::json& action) const;

    // ===== Strategy Tracking =====

    void recordAppliedStrategy(const std::string& strategy);
    void setSuggestedStrategies(const std::vector<std::string>& strategies);

    // ===== Metrics =====

    nlohmann::json getMetrics() const;
    int getCompletedIterations() const;
    int getFailedIterations() const;
    float getOverallSuccessRate() const;
    std::string getStateAsSummary() const;

    // ===== Serialization =====

    std::string serializeState() const;
    bool deserializeState(const std::string& jsonStr);

    // ===== Debugging =====

    std::string generateDebugReport() const;

    // ===== Time Helpers =====

    std::string timePointToISO(const TimePoint& tp) const;
    std::string timePointToHMS(const TimePoint& tp) const;

    // ===== String Conversion Helpers =====

    std::string phaseToString(ReasoningPhase phase) const;
    ReasoningPhase stringToPhase(const std::string& str) const;
    std::string statusToString(IterationStatus status) const;
    IterationStatus stringToStatus(const std::string& str) const;

    // ===== Inline Getters =====

    ReasoningPhase getCurrentPhase() const { return m_currentPhase; }
    IterationStatus getCurrentStatus() const { return m_currentStatus; }
    int getIterationCount() const { return static_cast<int>(m_iterations.size()); }
    int getTotalIterations() const { return static_cast<int>(m_iterations.size()); }
    int getTotalErrorCount() const { return static_cast<int>(m_errorHistory.size()); }
    const std::vector<Iteration>& getIterations() const { return m_iterations; }

private:
    ReasoningPhase m_currentPhase;
    IterationStatus m_currentStatus;
    TimePoint m_stateStartTime;
    TimePoint m_lastUpdateTime;
    bool m_debugMode = false;

    std::string m_currentGoal;

    nlohmann::json m_constraints;
    nlohmann::json m_lastSnapshot;

    std::vector<Iteration> m_iterations;
    std::unordered_map<std::string, nlohmann::json> m_memory;

    // Context window
    std::deque<Iteration> m_contextWindow;
    int m_contextWindowSize = 10;

    // Progress tracking
    int m_progressCurrent = 0;
    int m_progressTotal = 0;

    // Error history
    std::deque<ErrorRecord> m_errorHistory;

    // Snapshot history
    std::vector<nlohmann::json> m_snapshotHistory;

    // Strategy tracking
    std::vector<std::string> m_appliedStrategies;
    std::vector<std::string> m_suggestedStrategies;
};
