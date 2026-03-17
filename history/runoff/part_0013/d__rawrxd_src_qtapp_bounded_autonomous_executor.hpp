/**
 * @file bounded_autonomous_executor.hpp
 * @brief Bounded Autonomous Executor with Safety Controls
 * 
 * Implements a controlled perception→decision→action→feedback loop that:
 * - Runs up to MAX_ITERATIONS before stopping
 * - Responds to explicit shutdown signal
 * - Provides human override button to stop mid-execution
 * - Logs every cycle phase for audit and debugging
 * - Feeds action results back into next perception cycle
 * - Tracks state, decisions, and tool execution results
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <QTimer>
#include <memory>
#include <atomic>

class InferenceEngine;
class AgenticToolExecutor;
class MultiTabEditor;
class TerminalPool;

/**
 * @struct LoopState
 * @brief Current state of the autonomous execution loop
 */
struct LoopState {
    // Cycle tracking
    int currentIteration = 0;
    int maxIterations = 10;  // Safety bound: max 10 iterations
    bool isRunning = false;
    bool shutdownRequested = false;
    bool humanOverride = false;
    
    // Perception phase
    QString perceivedContext;        // Current state of codebase
    QString perceivedFiles;          // Files available for modification
    QString perceivedErrors;         // Any compilation/test errors
    
    // Decision phase
    QString inferencePrompt;         // Prompt sent to model
    QString modelDecision;           // Raw output from inference
    QString parsedAction;            // Parsed action from decision
    QString actionType;              // Type: refactor, create, fix, test, etc.
    
    // Action phase
    QStringList toolsExecuted;       // Tools called in this iteration
    QMap<QString, QString> toolResults;  // Tool name → result
    bool actionSucceeded = false;
    QString actionError;
    
    // Feedback phase
    QString feedbackFromTools;       // Return value from tool execution
    QString feedbackFromInference;   // Model's evaluation of action
    double confidenceScore = 0.0;    // How confident the model is in next step
};

/**
 * @struct ExecutionLog
 * @brief Complete log entry for one loop iteration
 */
struct ExecutionLog {
    int iteration;
    qint64 timestamp;
    double cycleTimeMs = 0.0;
    
    // Phase logs
    QString perceptionSummary;
    QString decisionReasoning;
    QString actionDescription;
    QString feedbackSummary;
    
    // Outcomes
    bool success;
    QString errorMessage;
    int toolsUsed = 0;
    QStringList filesModified;
};

/**
 * @class BoundedAutonomousExecutor
 * @brief Safe, bounded autonomous loop with human control
 */
class BoundedAutonomousExecutor : public QObject {
    Q_OBJECT

public:
    explicit BoundedAutonomousExecutor(
        InferenceEngine* inferenceEngine,
        AgenticToolExecutor* toolExecutor,
        MultiTabEditor* editor,
        TerminalPool* terminals,
        QObject* parent = nullptr
    );
    
    ~BoundedAutonomousExecutor() = default;
    
    // ========== CONTROL METHODS ==========
    
    /**
     * @brief Start the autonomous loop with an initial task
     * @param initialPrompt The task description to execute autonomously
     * @param maxIterations How many iterations before automatic stop (1-50)
     */
    void startAutonomousLoop(const QString& initialPrompt, int maxIterations = 10);
    
    /**
     * @brief Immediately stop the loop (human override)
     * Called when user clicks "STOP" button
     */
    void requestShutdown();
    
    /**
     * @brief Force emergency stop with cleanup
     */
    void emergencyStop();
    
    // ========== STATE QUERIES ==========
    
    bool isRunning() const { return m_state.isRunning; }
    int currentIteration() const { return m_state.currentIteration; }
    int maxIterations() const { return m_state.maxIterations; }
    const LoopState& currentState() const { return m_state; }
    const QVector<ExecutionLog>& executionLogs() const { return m_logs; }
    
    // ========== FEEDBACK QUERIES ==========
    
    /**
     * @brief Get human-readable summary of what the loop did
     */
    QString executionSummary() const;
    
    /**
     * @brief Get detailed log for iteration N
     */
    ExecutionLog iterationLog(int iteration) const;
    
signals:
    // Lifecycle signals
    void loopStarted(const QString& initialTask);
    void loopFinished();
    void loopStopped();  // User clicked stop before completion
    void loopError(const QString& error);
    
    // Per-iteration signals
    void iterationStarted(int iteration);
    void iterationCompleted(int iteration);
    void iterationFailed(int iteration, const QString& error);
    
    // Phase signals (for UI progress display)
    void perceptionPhaseStarted(int iteration);
    void perceptionPhaseComplete(int iteration, const QString& context);
    
    void decisionPhaseStarted(int iteration);
    void decisionPhaseComplete(int iteration, const QString& decision);
    
    void actionPhaseStarted(int iteration, const QString& action);
    void actionPhaseComplete(int iteration, const QString& result);
    void actionPhaseFailed(int iteration, const QString& error);
    
    void feedbackPhaseStarted(int iteration);
    void feedbackPhaseComplete(int iteration, const QString& feedback);
    
    // Progress signals
    void progressUpdated(int iteration, int maxIterations, const QString& status);
    void outputLogged(const QString& logEntry);  // Real-time log output
    
    // State change signals
    void shutdownRequested();
    void humanOverrideTriggered();

private slots:
    // Main loop driver
    void runAutonomousLoop();
    
    // Phase handlers
    void executePerceptionPhase();
    void executeDecisionPhase();
    void executeActionPhase();
    void executeFeedbackPhase();
    
    // Tool execution handling
    void onToolExecutionComplete(const QString& toolName, const QString& result);
    void onToolExecutionError(const QString& toolName, const QString& error);
    
    // Async inference callbacks
    void onInferenceStreamToken(qint64 reqId, const QString& token);
    void onInferenceStreamFinished(qint64 reqId);
    void onInferenceError(qint64 reqId, const QString& error);

private:
    // Core loop infrastructure
    InferenceEngine* m_inferenceEngine;
    AgenticToolExecutor* m_toolExecutor;
    MultiTabEditor* m_editor;
    TerminalPool* m_terminals;
    
    LoopState m_state;
    QVector<ExecutionLog> m_logs;
    QTimer* m_loopTimer = nullptr;
    
    qint64 m_cycleStartTime = 0;
    QString m_currentTask;
    
    // Async inference state
    qint64 m_decisionRequestId = 0;
    QString m_accumulatedResponse;
    bool m_decisionPhaseWaiting = false;
    
    // Thread safety
    mutable QMutex m_stateMutex;
    
    // ========== PHASE IMPLEMENTATIONS ==========
    
    /**
     * Perception: Gather current state of files, errors, context
     */
    QString perceiveCurrentState();
    QString perceiveFileContext();
    QString perceiveErrorState();
    
    /**
     * Decision: Send state + history to inference engine, parse response
     */
    void makeDecision(const QString& perceptionContext);
    QString parseInferenceResponse(const QString& response);
    QString extractActionType(const QString& response);
    QStringList extractActionDetails(const QString& response);
    
    /**
     * Action: Execute the tool(s) decided in decision phase
     */
    bool executeAction(const QString& actionType, const QString& actionDetails);
    bool executeRefactorAction(const QString& details);
    bool executeCreateAction(const QString& details);
    bool executeFixAction(const QString& details);
    bool executeTestAction(const QString& details);
    bool executeAnalysisAction(const QString& details);
    
    /**
     * Feedback: Collect results, evaluate success, feed back for next iteration
     */
    QString collectFeedback();
    double evaluateConfidence();
    bool shouldContinueLoop();
    
    // ========== LOGGING ==========
    
    void logPhase(const QString& phase, const QString& details);
    void logIteration();
    void logError(const QString& error);
    
    /**
     * @brief Structured log entry with timestamp and context
     */
    void structuredLog(const QString& level, const QString& category, const QString& message);
};
