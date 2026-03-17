#pragma once

#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <vector>
#include <deque>
#include <memory>
#include <unordered_map>

/**
 * @class AgenticLoopState
 * @brief Complete state management for iterative reasoning loops with full context
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
        QDateTime timestamp;
        ReasoningPhase phase;
        QString description;
        QJsonObject reasoning;
        QJsonObject outcome;
        float confidence;
        bool success;
        int retryCount = 0;
        QString recoveryAction;
    };

    // Iteration record
    struct Iteration {
        int iterationNumber;
        QDateTime startTime;
        QDateTime endTime;
        ReasoningPhase currentPhase;
        IterationStatus status;
        std::vector<Decision> decisions;
        QString goalStatement;
        QJsonObject contextSnapshot;
        QString resultSummary;
        float successScore;
        int errorCount = 0;
        QStringList appliedStrategies;
    };

    // Error recovery record
    struct ErrorRecord {
        QDateTime timestamp;
        QString errorType;
        QString errorMessage;
        QString stackTrace;
        ReasoningPhase phase;
        QString recoveryStrategy;
        bool recoverySucceeded;
        int recoveryAttempt;
        QJsonObject context;
    };

public:
    AgenticLoopState();
    ~AgenticLoopState();

    // ===== ITERATION MANAGEMENT =====
    void startIteration(const QString& goal);
    void endIteration(IterationStatus status, const QString& result);
    Iteration* getCurrentIteration();
    const std::vector<Iteration>& getIterationHistory() const { return m_iterations; }

    // ===== PHASE MANAGEMENT =====
    void setCurrentPhase(ReasoningPhase phase);
    ReasoningPhase getCurrentPhase() const { return m_currentPhase; }
    float getPhaseProgress() const;
    QStringList getAllPhaseTransitions() const;

    // ===== DECISION TRACKING =====
    void recordDecision(
        const QString& description,
        const QJsonObject& reasoning,
        float confidence
    );
    void recordDecisionOutcome(
        int decisionIndex,
        const QJsonObject& outcome,
        bool success
    );
    std::vector<Decision> getDecisionHistory(int limit = -1) const;
    Decision* getCurrentDecision();
    float getAverageDecisionConfidence() const;
    float getDecisionSuccessRate() const;

    // ===== ERROR TRACKING AND RECOVERY =====
    void recordError(
        const QString& errorType,
        const QString& message,
        const QString& stackTrace = ""
    );
    void recordErrorRecovery(
        int errorIndex,
        const QString& strategy,
        bool succeeded
    );
    const std::deque<ErrorRecord>& getErrorHistory(size_t limit = 50) const;
    int getTotalErrorCount() const { return m_errorHistory.size(); }
    float getErrorRate() const;
    QString generateErrorAnalysis() const;

    // ===== STATE SNAPSHOTS (CHECKPOINTING) =====
    QJsonObject takeSnapshot();
    bool restoreFromSnapshot(const QJsonObject& snapshot);
    QJsonObject getLastSnapshot() const { return m_lastSnapshot; }

    // ===== MEMORY AND CONTEXT WINDOW =====
    void addToMemory(const QString& key, const QVariant& value);
    QVariant getFromMemory(const QString& key);
    void removeFromMemory(const QString& key);
    QJsonObject getAllMemory() const;
    void clearMemoryExcept(const QStringList& keysToKeep);
    
    // Context window management - keep last N iterations in context
    void setContextWindowSize(int size);
    QJsonArray getContextWindow() const;
    QString formatContextForModel() const;

    // ===== GOAL AND PROGRESS TRACKING =====
    void setGoal(const QString& goal) { m_currentGoal = goal; }
    QString getGoal() const { return m_currentGoal; }
    void updateProgress(int current, int total);
    float getProgressPercentage() const;
    QJsonObject getProgressInfo() const;

    // ===== CONSTRAINT MANAGEMENT =====
    void addConstraint(const QString& key, const QString& constraint);
    void removeConstraint(const QString& key);
    QJsonObject getAllConstraints() const { return m_constraints; }
    bool validateAgainstConstraints(const QJsonObject& action) const;

    // ===== STRATEGY TRACKING =====
    void recordAppliedStrategy(const QString& strategy);
    QStringList getAppliedStrategies() const { return m_appliedStrategies; }
    QStringList getSuggestedStrategies() const { return m_suggestedStrategies; }
    void setSuggestedStrategies(const QStringList& strategies);

    // ===== OVERALL STATE METRICS =====
    QJsonObject getMetrics() const;
    int getTotalIterations() const { return m_iterations.size(); }
    int getCompletedIterations() const;
    int getFailedIterations() const;
    float getOverallSuccessRate() const;
    QString getStateAsSummary() const;

    // ===== SERIALIZATION =====
    QString serializeState() const;
    bool deserializeState(const QString& jsonStr);

    // ===== DEBUGGING =====
    QString generateDebugReport() const;
    void setDebugMode(bool enabled) { m_debugMode = enabled; }
    bool isDebugMode() const { return m_debugMode; }

private:
    // State data
    std::vector<Iteration> m_iterations;
    std::deque<ErrorRecord> m_errorHistory;
    std::deque<Iteration> m_contextWindow;
    
    ReasoningPhase m_currentPhase;
    IterationStatus m_currentStatus;
    QString m_currentGoal;
    
    QDateTime m_stateStartTime;
    QDateTime m_lastUpdateTime;
    
    int m_progressCurrent = 0;
    int m_progressTotal = 0;
    int m_contextWindowSize = 5;
    
    // Memory store
    std::unordered_map<std::string, QVariant> m_memory;
    QJsonObject m_constraints;
    
    // Strategy tracking
    QStringList m_appliedStrategies;
    QStringList m_suggestedStrategies;
    
    // Snapshots for recovery
    QJsonObject m_lastSnapshot;
    std::vector<QJsonObject> m_snapshotHistory;
    
    // Debug
    bool m_debugMode = false;

    // Helper methods
    QString phaseToString(ReasoningPhase phase) const;
    ReasoningPhase stringToPhase(const QString& str) const;
    QString statusToString(IterationStatus status) const;
    IterationStatus stringToStatus(const QString& str) const;
};
