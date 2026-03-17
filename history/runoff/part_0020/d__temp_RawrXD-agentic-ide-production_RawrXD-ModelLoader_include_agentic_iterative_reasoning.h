#pragma once

#include <QString>
#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <vector>

class AgenticLoopState;
class AgenticEngine;
class InferenceEngine;

/**
 * @class AgenticIterativeReasoning
 * @brief Full iterative reasoning loop with reflection, adjustment, and learning
 * 
 * Complete reasoning loop lifecycle:
 * 1. ANALYSIS: Understand the problem and gather context
 * 2. PLANNING: Generate multiple strategies and select best
 * 3. EXECUTION: Execute chosen action with monitoring
 * 4. VERIFICATION: Verify results match expectations
 * 5. REFLECTION: Analyze what worked and what didn't
 * 6. ADJUSTMENT: Update strategy based on reflection
 * 7. LOOP: Repeat until goal achieved or max iterations exceeded
 * 
 * Features:
 * - Multi-turn reasoning with memory
 * - Strategy generation and evaluation
 * - Error recovery with backtracking
 * - Learning from past decisions
 * - Explainable reasoning traces
 */
class AgenticIterativeReasoning : public QObject
{
    Q_OBJECT

public:
    struct IterationResult {
        bool success;
        QString result;
        QString reasoning;
        QJsonArray decisionTrace;
        float confidence;
        int iterationCount;
        int totalTime; // milliseconds
    };

    struct Strategy {
        QString name;
        QString description;
        float estimatedSuccessProbability;
        QStringList requiredTools;
        QJsonObject parameters;
        int complexity; // 1-10
    };

public:
    explicit AgenticIterativeReasoning(QObject* parent = nullptr);
    ~AgenticIterativeReasoning();

    void initialize(AgenticEngine* engine, AgenticLoopState* state, InferenceEngine* inference);

    // Main entry point for iterative reasoning
    IterationResult reason(
        const QString& goal,
        int maxIterations = 10,
        int timeoutSeconds = 300
    );

    // Phase implementations - callable for testing
    QString analyzeGoal(const QString& goal);
    std::vector<Strategy> generateStrategies(const QString& goal, const QString& context);
    Strategy selectStrategy(const std::vector<Strategy>& strategies);
    
    QJsonObject planExecution(const QString& goal, const Strategy& strategy);
    QJsonObject executeAction(const QJsonObject& plan);
    
    bool verifyResult(const QString& expectedOutcome, const QJsonObject& actualResult);
    QString reflectOnIteration(const QJsonObject& iterationData);
    QJsonObject adjustStrategy(const QString& reflection);

    // Error handling within reasoning loop
    bool handleReasoningError(const QString& error, int& retryCount);
    QString generateBacktrackingStrategy(const QString& lastFailure);

    // Get information about reasoning process
    QJsonArray getReasoningTrace() const { return m_reasoningTrace; }
    QString getReasoningExplanation() const;
    QJsonObject getMetrics() const;

    // Configuration
    void setMaxIterations(int max) { m_maxIterations = max; }
    void setTimeoutSeconds(int seconds) { m_timeoutSeconds = seconds; }
    void setReflectionEnabled(bool enabled) { m_enableReflection = enabled; }
    void setVerboseLogging(bool enabled) { m_verboseLogging = enabled; }

signals:
    void iterationStarted(int iterationNumber);
    void phaseCompleted(const QString& phaseName, const QString& result);
    void strategySelected(const QString& strategyName);
    void executionStarted(const QString& action);
    void verificationResult(bool passed, const QString& details);
    void reflectionGenerated(const QString& reflection);
    void adjustmentApplied(const QString& newStrategy);
    void iterationCompleted(int iterationNumber, bool success);
    void reasoningCompleted(const QString& finalResult);
    void errorOccurred(const QString& error, int iterationNumber);

private:
    // Phase handlers
    QString executeAnalysisPhase(const QString& goal);
    std::vector<Strategy> executePlanningPhase(const QString& goal, const QString& analysis);
    QJsonObject executeExecutionPhase(const Strategy& strategy, const QString& goal);
    bool executeVerificationPhase(const QString& goal, const QJsonObject& result);
    QString executeReflectionPhase(
        int iterationNumber,
        const QString& goal,
        const QJsonObject& iterationData
    );
    
    // Helper methods
    Strategy pickBestStrategy(const std::vector<Strategy>& strategies);
    std::vector<QString> generateAlternativeApproaches(const QString& goal);
    QJsonObject buildIterationContext(int iteration, const QString& goal);
    bool hasConverged(const std::vector<QString>& recentResults);
    bool shouldRetry(const QString& error) const;

    // Model-based reasoning
    QString callModelForReasoning(const QString& prompt, const QString& context = "");
    QJsonArray extractStructuredResponse(const QString& modelResponse);
    
    // Metrics collection
    void recordIterationMetrics(
        int iterationNumber,
        const QString& phase,
        int durationMs
    );

    // Logging
    void log(const QString& message, const QString& level = "INFO");

    // Pointers to connected engines
    AgenticEngine* m_engine = nullptr;
    AgenticLoopState* m_state = nullptr;
    InferenceEngine* m_inferenceEngine = nullptr;

    // Configuration
    int m_maxIterations = 10;
    int m_timeoutSeconds = 300;
    bool m_enableReflection = true;
    bool m_verboseLogging = false;

    // Runtime state
    QDateTime m_loopStartTime;
    int m_currentIteration = 0;
    QStringList m_recentResults;
    std::vector<Strategy> m_failedStrategies;
    QString m_currentGoal;

    // Trace and metrics
    QJsonArray m_reasoningTrace;
    QJsonObject m_metrics;
    std::vector<std::pair<QString, int>> m_phaseDurations;
};
