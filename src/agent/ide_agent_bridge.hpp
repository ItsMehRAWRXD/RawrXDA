/**
 * @file ide_agent_bridge.hpp
 * @brief Plugin interface connecting IDE UI to agent execution pipeline
 *
 * Orchestrates full wish→plan→execute→result flow with progress tracking
 * and observability.
 *
 * @author RawrXD Agent Team
 * @version 1.0.0
 */

#pragma once

#include "model_invoker.hpp"
#include "action_executor.hpp"

#include <QObject>
#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QFuture>
#include <memory>

/**
 * @struct ExecutionPlan
 * @brief High-level execution plan with metadata
 */
struct ExecutionPlan {
    QString wish;                           ///< Original user wish
    QJsonArray actions;                     ///< Parsed actions
    QString reasoning;                      ///< Agent's reasoning
    int estimatedTimeMs = 0;                ///< Estimated execution time
    QString status;                         ///< Current status
};

/**
 * @class IDEAgentBridge
 * @brief Main plugin interface for IDE integration
 *
 * Responsibilities:
 * - Accept user wishes from IDE UI
 * - Orchestrate ModelInvoker (wish→plan)
 * - Orchestrate ActionExecutor (plan→results)
 * - Provide real-time progress updates
 * - Handle user approvals/cancellations
 * - Aggregate and present results
 *
 * @note Thread-safe via Qt signal/slot mechanism
 * @note All long-running operations are asynchronous
 *
 * Signal/Slot Connections:
 * ```cpp
 * connect(&bridge, &IDEAgentBridge::agentThinkingStarted,
 *         statusBar, &StatusBar::showMessage);
 * connect(&bridge, &IDEAgentBridge::agentGeneratedPlan,
 *         planPreview, &PlanPreviewWidget::displayPlan);
 * connect(&bridge, &IDEAgentBridge::agentExecutionProgress,
 *         progressBar, &ProgressBar::updateProgress);
 * ```
 *
 * @example
 * @code
 * IDEAgentBridge bridge;
 * bridge.initialize("http://localhost:11434", "mistral");
 *
 * connect(&bridge, &IDEAgentBridge::agentCompleted,
 *         this, &MyWindow::onAgentCompleted);
 *
 * bridge.executeWish("Add Q8_K quantization kernel");
 * @endcode
 */
class IDEAgentBridge : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Qt parent object
     */
    explicit IDEAgentBridge(QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~IDEAgentBridge() override;

    /**
     * @brief Initialize bridge with LLM backend configuration
     * @param endpoint LLM service URL (e.g., "http://localhost:11434")
     * @param backend Backend type ("ollama", "claude", "openai")
     * @param apiKey Optional API key for cloud services
     *
     * @note Must be called before executeWish()
     */
    void initialize(const QString& endpoint,
                   const QString& backend = "ollama",
                   const QString& apiKey = QString());

    /**
     * @brief Set project root directory for actions
     * @param root Absolute path to project root
     */
    void setProjectRoot(const QString& root);

    /**
     * @brief Get current project root
     * @return Project root path
     */
    QString projectRoot() const { return m_projectRoot; }

    /**
     * @brief Execute wish (full async pipeline)
     * @param wish Natural language request
     * @param requireApproval If true, emit planApprovalNeeded before execution
     *
     * Emits signals in sequence:
     * 1. agentThinkingStarted
     * 2. agentGeneratedPlan
     * 3. agentExecutionStarted (after user approval)
     * 4. agentExecutionProgress (periodic updates)
     * 5. agentCompleted or agentError
     *
     * @note Non-blocking; returns immediately
     */
    void executeWish(const QString& wish, bool requireApproval = true);

    /**
     * @brief Plan wish without executing (preview mode)
     * @param wish Natural language request
     *
     * Emits agentGeneratedPlan, skips execution.
     * Useful for showing user what agent would do before approval.
     */
    void planWish(const QString& wish);

    /**
     * @brief Approve and execute planned wish
     *
     * Called after planApprovalNeeded signal to proceed with execution.
     */
    void approvePlan();

    /**
     * @brief Reject planned wish (cancel execution)
     */
    void rejectPlan();

    /**
     * @brief Cancel ongoing execution
     */
    void cancelExecution();

    /**
     * @brief Check if any execution is in progress
     * @return true if planning or executing
     */
    bool isExecuting() const { return m_isExecuting; }

    /**
     * @brief Get current execution plan
     * @return Plan if available, null otherwise
     */
    ExecutionPlan currentPlan() const { return m_currentPlan; }

    /**
     * @brief Get execution history
     * @return List of all executed wishes and results
     */
    QJsonArray executionHistory() const { return m_executionHistory; }

    /**
     * @brief Enable/disable dry-run mode (preview without executing)
     * @param enabled If true, no actual changes are made
     */
    void setDryRunMode(bool enabled);

    /**
     * @brief Set whether to stop on first action error
     * @param stopOnError If true, halt plan on error; if false, continue
     */
    void setStopOnError(bool stopOnError);

signals:
    /**
     * @brief Emitted when agent starts thinking about wish
     * @param wish The user's request
     */
    void agentThinkingStarted(const QString& wish);

    /**
     * @brief Emitted when agent has generated a plan
     * @param plan Structured execution plan
     *
     * UI can display plan preview to user at this point.
     */
    void agentGeneratedPlan(const ExecutionPlan& plan);

    /**
     * @brief Emitted when user approval is needed before execution
     * @param plan Plan awaiting approval
     *
     * Connect external button handler to approvePlan() or rejectPlan().
     */
    void planApprovalNeeded(const ExecutionPlan& plan);

    /**
     * @brief Emitted when execution starts
     * @param totalActions Number of actions to execute
     */
    void agentExecutionStarted(int totalActions);

    /**
     * @brief Emitted when an action completes
     * @param actionIndex Index in plan
     * @param description Action description
     * @param success Whether action succeeded
     */
    void agentExecutionProgress(int actionIndex, const QString& description, bool success);

    /**
     * @brief Emitted periodically during execution for progress bar updates
     * @param current Current action index
     * @param total Total actions
     * @param elapsedMs Milliseconds elapsed
     */
    void agentProgressUpdated(int current, int total, int elapsedMs);

    /**
     * @brief Emitted when plan completes successfully
     * @param result Aggregated results from all actions
     * @param elapsedMs Total execution time
     */
    void agentCompleted(const QJsonObject& result, int elapsedMs);

    /**
     * @brief Emitted when agent or execution encounters error
     * @param error Error message
     * @param recoverable Whether user can retry
     */
    void agentError(const QString& error, bool recoverable);

    /**
     * @brief Emitted when agent needs user input to proceed
     * @param query Question to ask user
     * @param options Possible answers
     */
    void userInputRequested(const QString& query, const QStringList& options);

    /**
     * @brief Emitted when execution is cancelled by user
     */
    void executionCancelled();

private slots:
    /**
     * @brief Handle plan generation completion
     */
    void onPlanGenerated(const LLMResponse& response);

    /**
     * @brief Handle action completion
     */
    void onActionCompleted(int index, bool success, const QJsonObject& result);

    /**
     * @brief Handle action failure
     */
    void onActionFailed(int index, const QString& error, bool recoverable);

    /**
     * @brief Handle full plan completion
     */
    void onPlanCompleted(bool success, const QJsonObject& result);

    /**
     * @brief Handle user input needed from executor
     */
    void onUserInputNeeded(const QString& query, const QStringList& options);

private:
    /**
     * @brief Build context string for LLM (current IDE state)
     * @return Context description
     */
    QString buildExecutionContext() const;

    /**
     * @brief Convert LLM plan to ExecutionPlan
     * @param llmPlan Raw JSON plan from LLM
     * @return Structured ExecutionPlan
     */
    ExecutionPlan convertToExecutionPlan(const QJsonArray& llmPlan);

    /**
     * @brief Start plan execution after approval
     */
    void executeCurrentPlan();

    /**
     * @brief Record execution in history
     */
    void recordExecution(const QString& wish,
                        bool success,
                        const QJsonObject& result,
                        int elapsedMs);

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    std::unique_ptr<ModelInvoker> m_invoker;
    std::unique_ptr<ActionExecutor> m_executor;

    bool m_isExecuting = false;
    bool m_waitingForApproval = false;
    bool m_dryRun = false;

    QString m_projectRoot;
    ExecutionPlan m_currentPlan;
    QJsonArray m_executionHistory;

    int m_executionStartTime = 0;  ///< For timing measurements

    // Configuration
    bool m_requireApproval = true;
    bool m_stopOnError = true;
};
