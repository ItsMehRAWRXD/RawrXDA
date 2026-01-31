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
    std::string perceivedContext;        // Current state of codebase
    std::string perceivedFiles;          // Files available for modification
    std::string perceivedErrors;         // Any compilation/test errors
    
    // Decision phase
    std::string inferencePrompt;         // Prompt sent to model
    std::string modelDecision;           // Raw output from inference
    std::string parsedAction;            // Parsed action from decision
    std::string actionType;              // Type: refactor, create, fix, test, etc.
    
    // Action phase
    std::stringList toolsExecuted;       // Tools called in this iteration
    std::map<std::string, std::string> toolResults;  // Tool name → result
    bool actionSucceeded = false;
    std::string actionError;
    
    // Feedback phase
    std::string feedbackFromTools;       // Return value from tool execution
    std::string feedbackFromInference;   // Model's evaluation of action
    double confidenceScore = 0.0;    // How confident the model is in next step
};

/**
 * @struct ExecutionLog
 * @brief Complete log entry for one loop iteration
 */
struct ExecutionLog {
    int iteration;
    int64_t timestamp;
    double cycleTimeMs = 0.0;
    
    // Phase logs
    std::string perceptionSummary;
    std::string decisionReasoning;
    std::string actionDescription;
    std::string feedbackSummary;
    
    // Outcomes
    bool success;
    std::string errorMessage;
    int toolsUsed = 0;
    std::stringList filesModified;
};

/**
 * @class BoundedAutonomousExecutor
 * @brief Safe, bounded autonomous loop with human control
 */
class BoundedAutonomousExecutor  {

public:
    explicit BoundedAutonomousExecutor(
        InferenceEngine* inferenceEngine,
        AgenticToolExecutor* toolExecutor,
        MultiTabEditor* editor,
        TerminalPool* terminals
    );
    
    ~BoundedAutonomousExecutor() = default;
    
    // ========== CONTROL METHODS ==========
    
    /**
     * @brief Start the autonomous loop with an initial task
     * @param initialPrompt The task description to execute autonomously
     * @param maxIterations How many iterations before automatic stop (1-50)
     */
    void startAutonomousLoop(const std::string& initialPrompt, int maxIterations = 10);
    
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
    const std::vector<ExecutionLog>& executionLogs() const { return m_logs; }
    
    // ========== FEEDBACK QUERIES ==========
    
    /**
     * @brief Get human-readable summary of what the loop did
     */
    std::string executionSummary() const;
    
    /**
     * @brief Get detailed log for iteration N
     */
    ExecutionLog iterationLog(int iteration) const;
    \npublic:\n    // Lifecycle signals
    void loopStarted(const std::string& initialTask);
    void loopFinished();
    void loopStopped();  // User clicked stop before completion
    void loopError(const std::string& error);
    
    // Per-iteration signals
    void iterationStarted(int iteration);
    void iterationCompleted(int iteration);
    void iterationFailed(int iteration, const std::string& error);
    
    // Phase signals (for UI progress display)
    void perceptionPhaseStarted(int iteration);
    void perceptionPhaseComplete(int iteration, const std::string& context);
    
    void decisionPhaseStarted(int iteration);
    void decisionPhaseComplete(int iteration, const std::string& decision);
    
    void actionPhaseStarted(int iteration, const std::string& action);
    void actionPhaseComplete(int iteration, const std::string& result);
    void actionPhaseFailed(int iteration, const std::string& error);
    
    void feedbackPhaseStarted(int iteration);
    void feedbackPhaseComplete(int iteration, const std::string& feedback);
    
    // Progress signals
    void progressUpdated(int iteration, int maxIterations, const std::string& status);
    void outputLogged(const std::string& logEntry);  // Real-time log output
    
    // State change signals
    void shutdownRequested();
    void humanOverrideTriggered();
\nprivate:\n    // Main loop driver
    void runAutonomousLoop();
    
    // Phase handlers
    void executePerceptionPhase();
    void executeDecisionPhase();
    void executeActionPhase();
    void executeFeedbackPhase();
    
    // Tool execution handling
    void onToolExecutionComplete(const std::string& toolName, const std::string& result);
    void onToolExecutionError(const std::string& toolName, const std::string& error);
    
    // Async inference callbacks
    void onInferenceStreamToken(int64_t reqId, const std::string& token);
    void onInferenceStreamFinished(int64_t reqId);
    void onInferenceError(int64_t reqId, const std::string& error);

private:
    // Core loop infrastructure
    InferenceEngine* m_inferenceEngine;
    AgenticToolExecutor* m_toolExecutor;
    MultiTabEditor* m_editor;
    TerminalPool* m_terminals;
    
    LoopState m_state;
    std::vector<ExecutionLog> m_logs;
    // Timer m_loopTimer = nullptr;
    
    int64_t m_cycleStartTime = 0;
    std::string m_currentTask;
    
    // Async inference state
    int64_t m_decisionRequestId = 0;
    std::string m_accumulatedResponse;
    bool m_decisionPhaseWaiting = false;
    
    // Thread safety
    mutable std::mutex m_stateMutex;
    
    // ========== PHASE IMPLEMENTATIONS ==========
    
    /**
     * Perception: Gather current state of files, errors, context
     */
    std::string perceiveCurrentState();
    std::string perceiveFileContext();
    std::string perceiveErrorState();
    
    /**
     * Decision: Send state + history to inference engine, parse response
     */
    void makeDecision(const std::string& perceptionContext);
    std::string parseInferenceResponse(const std::string& response);
    std::string extractActionType(const std::string& response);
    std::stringList extractActionDetails(const std::string& response);
    
    /**
     * Action: Execute the tool(s) decided in decision phase
     */
    bool executeAction(const std::string& actionType, const std::string& actionDetails);
    bool executeRefactorAction(const std::string& details);
    bool executeCreateAction(const std::string& details);
    bool executeFixAction(const std::string& details);
    bool executeTestAction(const std::string& details);
    bool executeAnalysisAction(const std::string& details);
    
    /**
     * Feedback: Collect results, evaluate success, feed back for next iteration
     */
    std::string collectFeedback();
    double evaluateConfidence();
    bool shouldContinueLoop();
    
    // ========== LOGGING ==========
    
    void logPhase(const std::string& phase, const std::string& details);
    void logIteration();
    void logError(const std::string& error);
    
    /**
     * @brief Structured log entry with timestamp and context
     */
    void structuredLog(const std::string& level, const std::string& category, const std::string& message);
};



