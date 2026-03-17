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

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <nlohmann/json.hpp>
#include "agentic_engine.h"

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
    std::string target;                     ///< File, command, or resource name
    nlohmann::json params;                  ///< Action-specific parameters
    std::string description;                ///< Human-readable description

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
    std::string projectRoot;                ///< Project working directory
    std::vector<std::string> environmentVars; ///< Additional env vars
    int timeoutMs = 30000;                  ///< Default action timeout
    bool dryRun = false;                    ///< Preview without executing
    nlohmann::json state;                   ///< Shared state across actions

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
    /**
     * @brief Constructor
     */
    explicit ActionExecutor();

    /**
     * @brief Destructor
     */
    virtual ~ActionExecutor();

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
     */
    void executePlan(const nlohmann::json& actions, bool stopOnError = true);

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
    nlohmann::json getAggregatedResult() const;

    // Callbacks (replacing Qt signals)
    std::function<void(int)> onPlanStarted;
    std::function<void(int, const std::string&)> onActionStarted;
    std::function<void(int, bool, const nlohmann::json&)> onActionCompleted;
    std::function<void(int, const std::string&, bool)> onActionFailed;
    std::function<void(int, int)> onProgressUpdated;
    std::function<void(bool, const nlohmann::json&)> onPlanCompleted;
    std::function<void(const std::string&, const std::vector<std::string>&)> onUserInputNeeded;
    std::function<void(const std::string&, const std::string&)> onRecursiveTaskNeeded;

    // Setter for engine
    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }

private:
    AgenticEngine* m_agenticEngine = nullptr;
    // ─────────────────────────────────────────────────────────────────────
    // Action Handlers
    // ─────────────────────────────────────────────────────────────────────

    bool handleFileEdit(Action& action);
    bool handleSearchFiles(Action& action);
    bool handleRunBuild(Action& action);
    bool handleExecuteTests(Action& action);
    bool handleCommitGit(Action& action);
    bool handleInvokeCommand(Action& action);
    bool handleRecursiveAgent(Action& action);
    bool handleQueryUser(Action& action);

    // ─────────────────────────────────────────────────────────────────────
    // Utility Methods
    // ─────────────────────────────────────────────────────────────────────

    Action parseJsonAction(const nlohmann::json& jsonAction);
    bool createBackup(const std::string& filePath);
    bool restoreFromBackup(const std::string& filePath);
    
    nlohmann::json executeCommand(const std::string& command,
                                   const std::vector<std::string>& args,
                                   int timeoutMs);

    bool validateFileEditSafety(const std::string& filePath, const std::string& action);
    ActionType stringToActionType(const std::string& typeStr) const;

    // ─────────────────────────────────────────────────────────────────────
    // Member Variables
    // ─────────────────────────────────────────────────────────────────────

    ExecutionContext m_context;
    bool m_isExecuting = false;
    bool m_stopOnError = true;
    bool m_cancelled = false;

    std::vector<Action> m_executedActions;      ///< History of executed actions
    std::map<std::string, std::string> m_backups; ///< Backup file mappings
};

