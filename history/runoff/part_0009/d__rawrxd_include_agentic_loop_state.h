#pragma once

#include <string>
#include <vector>
#include <deque>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <nlohmann/json.hpp>

/**
 * @class AgenticLoopState
 * @brief Complete state management for iterative reasoning loops with full context (Qt-free)
 * 
 * Manages:
 * - Reasoning iterations and history
 * - Agent state (current goal, progress, constraints)
 * - Execution context and environment
 * - Error tracking and recovery state
 * - Decision history for reflection
 * - Memory management with context windows
 * - Checkpoints for resumption
 */
class AgenticLoopState
{
public:
    using TimePoint = std::chrono::system_clock::time_point;

    // Iteration result status
    enum class IterationStatus {
        NotStarted,
        InProgress,
        Completed,
        Failed,
        Recovering,
        MaxAttemptsReached
    };

    // Reasoning phase
    enum class ReasoningPhase {
        Analysis,
        Planning,
        Execution,
        Verification,
        Reflection,
        Adjustment
    };

    // Agent decision record
    struct Decision {
        TimePoint timestamp;
        ReasoningPhase phase;
        std::string description;
        nlohmann::json reasoning;
        nlohmann::json outcome;
        float confidence;
        bool success;
        int retryCount = 0;
        std::string recoveryAction;
    };

    // Iteration record
    struct Iteration {
        int iterationNumber;
        TimePoint startTime;
        TimePoint endTime;
        ReasoningPhase currentPhase;
        IterationStatus status;
        std::vector<Decision> decisions;
        std::string goalStatement;
        nlohmann::json contextSnapshot;
        std::string resultSummary;
        float successScore;
        int errorCount = 0;
        std::vector<std::string> appliedStrategies;
    };

    // Error recovery record
    struct ErrorRecord {
        TimePoint timestamp;
        std::string errorType;
        std::string errorMessage;
        std::string stackTrace;
        ReasoningPhase phase;
        std::string recoveryStrategy;
        bool recoverySucceeded;
        int recoveryAttempt;
        nlohmann::json context;
    };

public:
    AgenticLoopState();
    ~AgenticLoopState();

    // ===== ITERATION MANAGEMENT =====
    void startIteration(const std::string& goal);
    void endIteration(IterationStatus status, const std::string& result);
    Iteration* getCurrentIteration();
    const std::vector<Iteration>& getIterationHistory() const { return m_iterations; }

    // ===== PHASE MANAGEMENT =====
    void setCurrentPhase(ReasoningPhase phase);
    ReasoningPhase getCurrentPhase() const { return m_currentPhase; }
    float getPhaseProgress() const;
    std::vector<std::string> getAllPhaseTransitions() const;

    // ===== DECISION TRACKING =====
    void recordDecision(
        const std::string& description,
        const nlohmann::json& reasoning,
        float confidence
    );
    void recordDecisionOutcome(
        int decisionIndex,
        const nlohmann::json& outcome,
        bool success
    );
    std::vector<Decision> getDecisionHistory(int limit = -1) const;
    Decision* getCurrentDecision();
    float getAverageDecisionConfidence() const;
    float getDecisionSuccessRate() const;

    // ===== ERROR TRACKING AND RECOVERY =====
    void recordError(
        const std::string& errorType,
        const std::string& message,
        const std::string& stackTrace = ""
    );
    void recordErrorRecovery(
        int errorIndex,
        const std::string& strategy,
        bool succeeded
    );
    const std::deque<ErrorRecord>& getErrorHistory(size_t limit = 50) const;
    int getTotalErrorCount() const { return static_cast<int>(m_errorHistory.size()); }
    float getErrorRate() const;
    std::string generateErrorAnalysis() const;

    // ===== STATE SNAPSHOTS (CHECKPOINTING) =====
    nlohmann::json takeSnapshot();
    bool restoreFromSnapshot(const nlohmann::json& snapshot);
    nlohmann::json getLastSnapshot() const { return m_lastSnapshot; }

    // ===== MEMORY AND CONTEXT WINDOW =====
    void addToMemory(const std::string& key, const nlohmann::json& value);
    nlohmann::json getFromMemory(const std::string& key);
    void removeFromMemory(const std::string& key);
    nlohmann::json getAllMemory() const;
    void clearMemoryExcept(const std::vector<std::string>& keysToKeep);
    
    // Context window management - keep last N iterations in context
    void setContextWindowSize(int size);
    nlohmann::json getContextWindow() const;
    std::string formatContextForModel() const;

    // ===== GOAL AND PROGRESS TRACKING =====
    void setGoal(const std::string& goal) { m_currentGoal = goal; }
    std::string getGoal() const { return m_currentGoal; }
    void updateProgress(int current, int total);
    float getProgressPercentage() const;
    nlohmann::json getProgressInfo() const;

    // ===== CONSTRAINT MANAGEMENT =====
    void addConstraint(const std::string& key, const std::string& constraint);
    void removeConstraint(const std::string& key);
    nlohmann::json getAllConstraints() const { return m_constraints; }
    bool validateAgainstConstraints(const nlohmann::json& action) const;

    // ===== STRATEGY TRACKING =====
    void recordAppliedStrategy(const std::string& strategy);
    std::vector<std::string> getAppliedStrategies() const { return m_appliedStrategies; }
    std::vector<std::string> getSuggestedStrategies() const { return m_suggestedStrategies; }
    void setSuggestedStrategies(const std::vector<std::string>& strategies);

    // ===== OVERALL STATE METRICS =====
    nlohmann::json getMetrics() const;
    int getTotalIterations() const { return static_cast<int>(m_iterations.size()); }
    int getCompletedIterations() const;
    int getFailedIterations() const;
    float getOverallSuccessRate() const;
    std::string getStateAsSummary() const;

    // ===== SERIALIZATION =====
    std::string serializeState() const;
    bool deserializeState(const std::string& jsonStr);

    // ===== DEBUGGING =====
    std::string generateDebugReport() const;
    void setDebugMode(bool enabled) { m_debugMode = enabled; }
    bool isDebugMode() const { return m_debugMode; }

private:
    // Helper: ISO timestamp from time_point
    static std::string timePointToISO(const TimePoint& tp);
    // Helper: HH:MM:SS from time_point
    static std::string timePointToHMS(const TimePoint& tp);

    // State data
    std::vector<Iteration> m_iterations;
    std::deque<ErrorRecord> m_errorHistory;
    std::deque<Iteration> m_contextWindow;
    
    ReasoningPhase m_currentPhase;
    IterationStatus m_currentStatus;
    std::string m_currentGoal;
    
    TimePoint m_stateStartTime;
    TimePoint m_lastUpdateTime;
    
    int m_progressCurrent = 0;
    int m_progressTotal = 0;
    int m_contextWindowSize = 5;
    
    // Memory store
    std::unordered_map<std::string, nlohmann::json> m_memory;
    nlohmann::json m_constraints;
    
    // Strategy tracking
    std::vector<std::string> m_appliedStrategies;
    std::vector<std::string> m_suggestedStrategies;
    
    // Snapshots for recovery
    nlohmann::json m_lastSnapshot;
    std::vector<nlohmann::json> m_snapshotHistory;
    
    // Debug
    bool m_debugMode = false;

    // Helper methods
    std::string phaseToString(ReasoningPhase phase) const;
    ReasoningPhase stringToPhase(const std::string& str) const;
    std::string statusToString(IterationStatus status) const;
    IterationStatus stringToStatus(const std::string& str) const;
};
