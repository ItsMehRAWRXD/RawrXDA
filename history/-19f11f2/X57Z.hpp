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

#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

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
    std::string target;                         ///< File, command, or resource name
    nlohmann::json params;                     ///< Action-specific parameters
    std::string description;                    ///< Human-readable description

    // Result tracking
    bool executed = false;
    bool success = false;
    std::string result;
    std::string error;
};

/**
 * @struct ExecutionContext
 * @brief Stateful context for plan execution
 */
struct ExecutionContext {
    std::string projectRoot;                    ///< Project working directory
    std::vector<std::string> environmentVars;            ///< Additional env vars
    int timeoutMs = 30000;                  ///< Default action timeout
    bool dryRun = false;                    ///< Preview without executing
    nlohmann::json state;                      ///< Shared state across actions

    // Tracking
    int currentActionIndex = 0;
    int totalActions = 0;
};

/**
 * @class ActionExecutor
 * @brief Executes agent-generated action plans with error handling
 */
class ActionExecutor {
public:
    explicit ActionExecutor();
    ~ActionExecutor();

    void setContext(const ExecutionContext& context);
    void executePlan(const std::vector<Action>& plan);
    void executeAction(const Action& action);

    // Callbacks
    void planStarted(int totalActions);
    void actionStarted(int index, const std::string& description);
    void actionCompleted(int index, bool success, const std::string& result);
    void planCompleted(bool success);
    void logMessage(const std::string& message);

private:
    ExecutionContext m_context;
    bool m_stopRequested;
};
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
    void executePlan(const void*& actions, bool stopOnError = true);

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
    std::vector<Action> executedActions() const { return m_executedActions; }

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
    void* getAggregatedResult() const;

\npublic:\n    /**
     * @brief Emitted when plan execution begins
     * @param totalActions Number of actions to execute
     */
    void planStarted(int totalActions);

    /**
     * @brief Emitted when an action begins
     * @param index Action index in plan
     * @param description Human-readable action description
     */
    void actionStarted(int index, const std::string& description);

    /**
     * @brief Emitted when an action completes
     * @param index Action index
     * @param success Whether action succeeded
     * @param result Result data from action
     */
    void actionCompleted(int index, bool success, const void*& result);

    /**
     * @brief Emitted when action fails
     * @param index Action index
     * @param error Error message
     * @param recoverable Whether execution can continue
     */
    void actionFailed(int index, const std::string& error, bool recoverable);

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
    void planCompleted(bool success, const void*& result);

    /**
     * @brief Emitted when user input is needed
     * @param query Question to ask user
     * @param options Possible answers
     */
    void userInputNeeded(const std::string& query, const std::stringList& options);

\nprivate:\n    /**
     * @brief Handle action executor background task completion
     */
    void onActionTaskFinished();

    /**
     * @brief Handle process completion for system commands
     */
    void onProcessFinished(int exitCode, void*::ExitStatus exitStatus);

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
    Action parseJsonAction(const void*& jsonAction);

    /**
     * @brief Create backup of file before modification
     * @param filePath Path to file
     * @return true if backup created successfully
     */
    bool createBackup(const std::string& filePath);

    /**
     * @brief Restore file from backup
     * @param filePath Path to file
     * @return true if restore successful
     */
    bool restoreFromBackup(const std::string& filePath);

    /**
     * @brief Execute command and capture output
     * @param command Command to execute
     * @param args Command arguments
     * @param timeoutMs Timeout in milliseconds
     * @return Exit code and stdout/stderr
     */
    void* executeCommand(const std::string& command,
                               const std::stringList& args,
                               int timeoutMs);

    /**
     * @brief Validate file edit is safe
     * @param filePath Target file
     * @param action Edit action type
     * @return true if safe
     */
    bool validateFileEditSafety(const std::string& filePath, const std::string& action);

    /**
     * @brief Map action type string to enum
     * @param typeStr String representation of type
     * @return Corresponding ActionType enum
     */
    ActionType stringToActionType(const std::string& typeStr) const;

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    ExecutionContext m_context;
    bool m_isExecuting = false;
    bool m_stopOnError = true;
    bool m_cancelled = false;

    std::vector<Action> m_executedActions;      ///< History of executed actions
    std::map<std::string, std::string> m_backups;       ///< Backup file mappings
    std::unique_ptr<void*> m_process;    ///< Current subprocess
};





