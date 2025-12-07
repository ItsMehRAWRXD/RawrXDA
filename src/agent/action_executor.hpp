/**
 * @file action_executor.hpp
 * @brief Execution engine for structured action plans
 *
 * Executes individual actions from agent-generated plans with:
 * - Error recovery and rollback
 * - Progress tracking and observability
 * - Thread-safe operation
 *
 * @author RawrXD Agent Team
 * @version 1.0.0
 */

#pragma once

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <QJsonArray>
#include <QProcess>
#include <memory>

/**
 * @enum ActionType
 * @brief Categories of actions the executor can perform
 */
enum class ActionType {
    FileEdit,           ///< Modify, create, or delete files
    SearchFiles,        ///< Find files matching patterns
    RunBuild,           ///< Execute build system (CMake, MSBuild)
    ExecuteTests,       ///< Run test suite
    CommitGit,          ///< Git operations (commit, push)
    InvokeCommand,      ///< Execute arbitrary command
    QueryUser,          ///< Pause and ask user for input
    RecursiveAgent,     ///< Invoke agent recursively
    Unknown             ///< Unknown action type
};

/**
 * @struct Action
 * @brief Parsed action from plan
 */
struct Action {
    ActionType type = ActionType::Unknown;
    QString target;                         ///< File, command, or resource name
    QJsonObject params;                     ///< Action-specific parameters
    QString description;                    ///< Human-readable description

    // Result tracking
    bool executed = false;
    bool success = false;
    QString result;
    QString error;
};

/**
 * @struct ExecutionContext
 * @brief Stateful context for plan execution
 */
struct ExecutionContext {
    QString projectRoot;                    ///< Project working directory
    QStringList environmentVars;            ///< Additional env vars
    int timeoutMs = 30000;                  ///< Default action timeout
    bool dryRun = false;                    ///< Preview without executing
    QJsonObject state;                      ///< Shared state across actions

    // Tracking
    int currentActionIndex = 0;
    int totalActions = 0;
};

/**
 * @class ActionExecutor
 * @brief Executes agent-generated action plans with error handling
 *
 * Responsibilities:
 * - Parse JSON actions from agent plan
 * - Execute each action with appropriate handler
 * - Collect results and aggregate state
 * - Handle errors with recovery strategies
 * - Track progress for UI updates
 * - Provide rollback on failure
 *
 * @note Thread-safe via Qt signal/slot mechanism
 * @note All blocking operations run on background threads
 *
 * @example
 * @code
 * ActionExecutor executor;
 * executor.setContext(ctx);
 *
 * connect(&executor, &ActionExecutor::actionCompleted,
 *         this, &MyClass::onActionDone);
 *
 * executor.executePlan(planArray);
 * @endcode
 */
class ActionExecutor : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param parent Qt parent object
     */
    explicit ActionExecutor(QObject* parent = nullptr);

    /**
     * @brief Destructor
     */
    ~ActionExecutor() override;

    /**
     * @brief Set execution context (project root, env, timeout)
     * @param context Execution configuration
     */
    void setContext(const ExecutionContext& context);

    /**
     * @brief Get current execution context
     * @return Current context
     */
    ExecutionContext context() const { return m_context; }

    /**
     * @brief Execute single action (synchronous)
     * @param action The action to execute
     * @return true if successful
     */
    bool executeAction(Action& action);

    /**
     * @brief Execute complete plan (asynchronous)
     * @param actions Array of actions to execute
     * @param stopOnError If true, stop at first failure; if false, continue
     *
     * Emits actionStarted/actionCompleted for each action.
     * Emits planCompleted at end with overall result.
     */
    void executePlan(const QJsonArray& actions, bool stopOnError = true);

    /**
     * @brief Cancel executing plan
     */
    void cancelExecution();

    /**
     * @brief Check if execution is in progress
     * @return true if plan is being executed
     */
    bool isExecuting() const { return m_isExecuting; }

    /**
     * @brief Get all executed actions
     * @return Vector of completed actions with results
     */
    QVector<Action> executedActions() const { return m_executedActions; }

    /**
     * @brief Rollback previous action (if supported)
     * @param actionIndex Index of action to rollback
     * @return true if rollback succeeded
     */
    bool rollbackAction(int actionIndex);

    /**
     * @brief Get aggregated result from plan
     * @return JSON object with results from all actions
     */
    QJsonObject getAggregatedResult() const;

signals:
    /**
     * @brief Emitted when plan execution begins
     * @param totalActions Number of actions to execute
     */
    void planStarted(int totalActions);

    /**
     * @brief Emitted when an action begins
     * @param index Action index in plan
     * @param description Human-readable action description
     */
    void actionStarted(int index, const QString& description);

    /**
     * @brief Emitted when an action completes
     * @param index Action index
     * @param success Whether action succeeded
     * @param result Result data from action
     */
    void actionCompleted(int index, bool success, const QJsonObject& result);

    /**
     * @brief Emitted when action fails
     * @param index Action index
     * @param error Error message
     * @param recoverable Whether execution can continue
     */
    void actionFailed(int index, const QString& error, bool recoverable);

    /**
     * @brief Emitted for progress updates
     * @param current Current action index
     * @param total Total actions
     */
    void progressUpdated(int current, int total);

    /**
     * @brief Emitted when entire plan completes
     * @param success Overall success (all actions passed)
     * @param result Aggregated results
     */
    void planCompleted(bool success, const QJsonObject& result);

    /**
     * @brief Emitted when user input is needed
     * @param query Question to ask user
     * @param options Possible answers
     */
    void userInputNeeded(const QString& query, const QStringList& options);

private slots:
    /**
     * @brief Handle action executor background task completion
     */
    void onActionTaskFinished();

    /**
     * @brief Handle process completion for system commands
     */
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    // ─────────────────────────────────────────────────────────────────────
    // Action Handlers
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Execute file edit action
     * @param action Action containing edit parameters
     * @return true if successful
     */
    bool handleFileEdit(Action& action);

    /**
     * @brief Execute file search action
     * @param action Action containing search parameters
     * @return true if successful
     */
    bool handleSearchFiles(Action& action);

    /**
     * @brief Execute build action
     * @param action Action containing build parameters
     * @return true if successful
     */
    bool handleRunBuild(Action& action);

    /**
     * @brief Execute test action
     * @param action Action containing test parameters
     * @return true if successful
     */
    bool handleExecuteTests(Action& action);

    /**
     * @brief Execute git operation
     * @param action Action containing git parameters
     * @return true if successful
     */
    bool handleCommitGit(Action& action);

    /**
     * @brief Execute arbitrary command
     * @param action Action containing command
     * @return true if successful
     */
    bool handleInvokeCommand(Action& action);

    /**
     * @brief Handle recursive agent invocation
     * @param action Action for agent recursion
     * @return true if successful
     */
    bool handleRecursiveAgent(Action& action);

    /**
     * @brief Prompt user for input
     * @param action Action containing query
     * @return true if user approved
     */
    bool handleQueryUser(Action& action);

    // ─────────────────────────────────────────────────────────────────────
    // Utility Methods
    // ─────────────────────────────────────────────────────────────────────

    /**
     * @brief Parse JSON action object into Action struct
     * @param jsonAction JSON representation of action
     * @return Parsed Action
     */
    Action parseJsonAction(const QJsonObject& jsonAction);

    /**
     * @brief Create backup of file before modification
     * @param filePath Path to file
     * @return true if backup created successfully
     */
    bool createBackup(const QString& filePath);

    /**
     * @brief Restore file from backup
     * @param filePath Path to file
     * @return true if restore successful
     */
    bool restoreFromBackup(const QString& filePath);

    /**
     * @brief Execute command and capture output
     * @param command Command to execute
     * @param args Command arguments
     * @param timeoutMs Timeout in milliseconds
     * @return Exit code and stdout/stderr
     */
    QJsonObject executeCommand(const QString& command,
                               const QStringList& args,
                               int timeoutMs);

    /**
     * @brief Validate file edit is safe
     * @param filePath Target file
     * @param action Edit action type
     * @return true if safe
     */
    bool validateFileEditSafety(const QString& filePath, const QString& action);

    /**
     * @brief Map action type string to enum
     * @param typeStr String representation of type
     * @return Corresponding ActionType enum
     */
    ActionType stringToActionType(const QString& typeStr) const;

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    ExecutionContext m_context;
    bool m_isExecuting = false;
    bool m_stopOnError = true;
    bool m_cancelled = false;

    QVector<Action> m_executedActions;      ///< History of executed actions
    QMap<QString, QString> m_backups;       ///< Backup file mappings
    std::unique_ptr<QProcess> m_process;    ///< Current subprocess
};
