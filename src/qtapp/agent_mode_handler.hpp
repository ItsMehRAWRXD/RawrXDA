/**
 * @file agent_mode_handler.hpp
 * @brief Agent Mode Handler - Autonomous Execution Phase
 * 
 * Agent Mode executes the approved plan autonomously:
 * 1. Takes the approved plan from Plan Mode
 * 2. Creates a manage_todo_list with all plan steps
 * 3. Executes each step using available agent tools
 * 4. Updates progress in real-time
 * 5. Handles errors and provides recovery options
 */

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <memory>

class UnifiedBackend;
class MetaPlanner;
class AgenticToolExecutor;
struct Plan;
struct PlanStep;

/**
 * @struct ExecutionStep
 * @brief Execution state for a single step
 */
struct ExecutionStep {
    int stepId;
    QString title;
    enum Status {
        Pending,
        InProgress,
        Completed,
        Failed,
        Skipped
    } status = Pending;

    QString output;                 ///< Step output/results
    QString errorMessage;           ///< Error if failed
    int executionTimeMs = 0;        ///< Execution duration
    QStringList filesModified;      ///< Files this step changed
};

/**
 * @class AgentModeHandler
 * @brief Handles autonomous execution of approved plans
 */
class AgentModeHandler : public QObject {
    Q_OBJECT

public:
    explicit AgentModeHandler(UnifiedBackend* backend, MetaPlanner* planner, QObject* parent = nullptr);
    ~AgentModeHandler();

    /**
     * @brief Start executing a plan
     * @param plan The approved plan from Plan Mode
     */
    void executeplan(const Plan& plan);

    /**
     * @brief Pause execution
     */
    void pauseExecution();

    /**
     * @brief Resume paused execution
     */
    void resumeExecution();

    /**
     * @brief Skip current step and move to next
     */
    void skipCurrentStep();

    /**
     * @brief Cancel execution and rollback changes
     */
    void cancelExecution();

    /**
     * @brief Get all execution steps with current status
     */
    const QVector<ExecutionStep>& getExecutionSteps() const { return m_executionSteps; }

    /**
     * @brief Get current step being executed
     */
    int getCurrentStepIndex() const { return m_currentStepIndex; }

    /**
     * @brief Get overall progress percentage
     */
    float getProgressPercentage() const;

    /**
     * @brief Check if execution is complete
     */
    bool isExecutionComplete() const { return m_executionComplete; }

signals:
    /// Execution started
    void executionStarted();

    /// About to execute a step
    void stepStarting(int stepIndex, const ExecutionStep& step);

    /// Step is executing (tool invocation started)
    void stepExecuting(int stepIndex, const QString& toolName);

    /// Step progress update
    void stepProgress(int stepIndex, const QString& message);

    /// Step completed successfully
    void stepCompleted(int stepIndex, const ExecutionStep& step);

    /// Step failed
    void stepFailed(int stepIndex, const QString& errorMessage);

    /// Step output received
    void stepOutput(int stepIndex, const QString& output);

    /// Execution paused
    void executionPaused();

    /// Execution resumed
    void executionResumed();

    /// Execution completed successfully
    void executionCompleted(const QVector<ExecutionStep>& steps);

    /// Execution failed
    void executionFailed(int stepIndex, const QString& errorMessage);

    /// Execution cancelled
    void executionCancelled();

    /// Overall progress update
    void progressUpdated(float percentage, const QString& message);

    /// Error occurred (non-fatal, recovery possible)
    void errorOccurred(const QString& error);

private slots:
    /// Handle tool execution completion
    void onToolExecutionCompleted(const QString& toolName, const QString& output);

    /// Handle tool execution error
    void onToolExecutionError(const QString& toolName, const QString& error);

    /// Handle timeout during step execution
    void onStepTimeout(int stepIndex);

private:
    /**
     * @brief Execute the next step in the plan
     */
    void executeNextStep();

    /**
     * @brief Execute a single step
     * @param step The plan step to execute
     */
    void executeSingleStep(const ExecutionStep& step);

    /**
     * @brief Try to recover from a step failure
     * @param stepIndex Index of failed step
     * @return True if recovery was successful
     */
    bool attemptRecovery(int stepIndex);

    /**
     * @brief Rollback all changes from executed steps
     */
    void rollbackChanges();

    /**
     * @brief Map plan to execution steps
     */
    void mapPlanToExecutionSteps(const Plan& plan);

    // Members
    UnifiedBackend* m_backend;
    MetaPlanner* m_planner;
    std::unique_ptr<AgenticToolExecutor> m_toolExecutor;

    Plan m_executionPlan;
    QVector<ExecutionStep> m_executionSteps;
    int m_currentStepIndex = -1;
    bool m_executionComplete = false;
    bool m_executionPaused = false;
    QStringList m_modifiedFiles;  ///< Track all files modified for rollback
};
