/**
 * @file action_executor.hpp
 * @brief Execution engine for structured action plans
 *
 * Executes individual actions from agent-generated plans with:
 * - Error recovery and rollback
 * - Progress tracking and observability
 * - Thread-safe operation
 *
 * Architecture: C++20, no Qt, no exceptions
 * Error model: PatchResult pattern where applicable
 *
 * @author RawrXD Agent Team
 * @version 2.0.0
 */

#pragma once

#include "simple_json.hpp"

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <cstdint>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

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
    std::string id;                         ///< Unique action ID
    std::string target;                     ///< File, command, or resource name
    JsonValue params;                       ///< Action-specific parameters (Object)
    std::string description;                ///< Human-readable description

    // Result tracking
    bool executed = false;
    bool success  = false;
    std::string result;
    std::string error;
};

/**
 * @struct ProcessResult
 * @brief Result from an external process execution
 */
struct ProcessResult {
    int exitCode = -1;
    std::string stdoutStr;
    std::string stderrStr;
    bool timedOut = false;
};

/**
 * @brief Model invoker function pointer type
 *
 * Signature: JsonValue invoke(const std::string& wish, const JsonValue& params, void* userData)
 */
typedef JsonValue (*ModelInvokeFunc)(const std::string& wish, const JsonValue& params, void* userData);

/**
 * @struct ExecutionContext
 * @brief Stateful context for plan execution
 */
struct ExecutionContext {
    std::string projectRoot;                    ///< Project working directory
    std::vector<std::string> environmentVars;   ///< Additional env vars
    int timeoutMs = 30000;                      ///< Default action timeout
    bool dryRun = false;                        ///< Preview without executing
    JsonValue state;                            ///< Shared state across actions (Object)

    // Model invocation (for recursive agent)
    ModelInvokeFunc modelInvokeFunc = nullptr;
    void* modelInvokeUserData = nullptr;

    // Tracking
    int currentActionIndex = 0;
    int totalActions = 0;
};

// =========================================================================
// Callback types (replace Qt signals)
// =========================================================================

typedef void (*PlanStartedCallback)(int totalActions, void* userData);
typedef void (*ActionStartedCallback)(int index, const std::string& description, void* userData);
typedef void (*ActionCompletedCallback)(int index, bool success, const JsonValue& result, void* userData);
typedef void (*ActionFailedCallback)(int index, const std::string& error, bool recoverable, void* userData);
typedef void (*ProgressUpdatedCallback)(int current, int total, void* userData);
typedef void (*PlanCompletedCallback)(bool success, const JsonValue& result, void* userData);
typedef void (*UserInputNeededCallback)(const std::string& query, const std::vector<std::string>& options, void* userData);

/**
 * @class ActionExecutor
 * @brief Executes agent-generated action plans with error handling
 *
 * Responsibilities:
 * - Parse JSON actions from agent plan
 * - Execute each action with appropriate handler
 * - Collect results and aggregate state
 * - Handle errors with recovery strategies
 * - Track progress via callbacks
 * - Provide rollback on failure
 *
 * @note Thread-safe via std::mutex
 * @note All blocking operations run on background threads
 *
 * @example
 * @code
 * ActionExecutor executor;
 * executor.setContext(ctx);
 *
 * executor.registerActionCompletedCallback(onActionDone, nullptr);
 * executor.executePlan(planArray);
 * @endcode
 */
class ActionExecutor {
public:
    ActionExecutor(const ActionExecutor&) = delete;
    ActionExecutor& operator=(const ActionExecutor&) = delete;

    /**
     * @brief Constructor
     */
    ActionExecutor();

    /**
     * @brief Destructor - joins background thread if active
     */
    ~ActionExecutor();

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
     * @brief Execute complete plan (asynchronous, runs on background thread)
     * @param actions JsonValue Array of actions to execute
     * @param stopOnError If true, stop at first failure; if false, continue
     *
     * Fires ActionStarted/ActionCompleted callbacks for each action.
     * Fires PlanCompleted callback at end with overall result.
     */
    void executePlan(const JsonValue& actions, bool stopOnError = true);

    /**
     * @brief Cancel executing plan
     */
    void cancelExecution();

    /**
     * @brief Check if execution is in progress
     * @return true if plan is being executed
     */
    bool isExecuting() const { return m_isExecuting.load(); }

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
     * @return JsonValue object with results from all actions
     */
    JsonValue getAggregatedResult() const;

    // -----------------------------------------------------------------
    // Callback Registration (replaces Qt signals)
    // -----------------------------------------------------------------

    void registerPlanStartedCallback(PlanStartedCallback cb, void* userData);
    void registerActionStartedCallback(ActionStartedCallback cb, void* userData);
    void registerActionCompletedCallback(ActionCompletedCallback cb, void* userData);
    void registerActionFailedCallback(ActionFailedCallback cb, void* userData);
    void registerProgressUpdatedCallback(ProgressUpdatedCallback cb, void* userData);
    void registerPlanCompletedCallback(PlanCompletedCallback cb, void* userData);
    void registerUserInputNeededCallback(UserInputNeededCallback cb, void* userData);

private:
    // -----------------------------------------------------------------
    // Action Handlers
    // -----------------------------------------------------------------

    /** @brief Execute file edit action */
    bool handleFileEdit(Action& action);

    /** @brief Execute file search action */
    bool handleSearchFiles(Action& action);

    /** @brief Execute build action */
    bool handleRunBuild(Action& action);

    /** @brief Execute test action */
    bool handleExecuteTests(Action& action);

    /** @brief Execute git operation */
    bool handleCommitGit(Action& action);

    /** @brief Execute arbitrary command */
    bool handleInvokeCommand(Action& action);

    /** @brief Handle recursive agent invocation */
    bool handleRecursiveAgent(Action& action);

    /** @brief Prompt user for input */
    bool handleQueryUser(Action& action);

    // -----------------------------------------------------------------
    // Utility Methods
    // -----------------------------------------------------------------

    /** @brief Parse JSON action object into Action struct */
    Action parseJsonAction(const JsonValue& jsonAction);

    /** @brief Create backup of file before modification */
    bool createBackup(const std::string& filePath);

    /** @brief Restore file from backup */
    bool restoreFromBackup(const std::string& filePath);

    /** @brief Execute command and capture output */
    JsonValue executeCommand(const std::string& command,
                             const std::vector<std::string>& args,
                             int timeoutMs);

    /** @brief Validate file edit is safe */
    bool validateFileEditSafety(const std::string& filePath, const std::string& action);

    /** @brief Map action type string to enum */
    ActionType stringToActionType(const std::string& typeStr) const;

    // -----------------------------------------------------------------
    // Process Execution (Windows: CreateProcessA, POSIX: fork+exec)
    // -----------------------------------------------------------------

    /** @brief Run external process with stdout/stderr capture and timeout */
    ProcessResult runProcess(const std::string& command,
                             const std::vector<std::string>& args,
                             const std::string& workingDir,
                             int timeoutMs);

    // -----------------------------------------------------------------
    // Callback Notification Helpers
    // -----------------------------------------------------------------

    void notifyPlanStarted(int totalActions);
    void notifyActionStarted(int index, const std::string& description);
    void notifyActionCompleted(int index, bool success, const JsonValue& result);
    void notifyActionFailed(int index, const std::string& error, bool recoverable);
    void notifyProgressUpdated(int current, int total);
    void notifyPlanCompleted(bool success, const JsonValue& result);
    void notifyUserInputNeeded(const std::string& query, const std::vector<std::string>& options);

    // Formerly: onActionTaskFinished / onProcessFinished (Qt slots, now regular methods)
    void onActionTaskFinished();
    void onProcessFinished(int exitCode, int exitStatus);

    // -----------------------------------------------------------------
    // Member Variables
    // -----------------------------------------------------------------

    ExecutionContext m_context;
    std::atomic<bool> m_isExecuting{false};
    bool m_stopOnError = true;
    std::atomic<bool> m_cancelled{false};

    std::vector<Action> m_executedActions;          ///< History of executed actions
    std::map<std::string, std::string> m_backups;   ///< Backup file mappings

    // Background execution thread
    std::thread m_executionThread;
    std::mutex m_threadMutex;

    // Current process handle for cancellation
#ifdef _WIN32
    HANDLE m_currentProcessHandle = INVALID_HANDLE_VALUE;
    std::mutex m_processMutex;
#endif

    // -----------------------------------------------------------------
    // Callback storage
    // -----------------------------------------------------------------

    struct PlanStartedCB      { PlanStartedCallback fn;      void* userData; };
    struct ActionStartedCB    { ActionStartedCallback fn;    void* userData; };
    struct ActionCompletedCB  { ActionCompletedCallback fn;  void* userData; };
    struct ActionFailedCB     { ActionFailedCallback fn;     void* userData; };
    struct ProgressUpdatedCB  { ProgressUpdatedCallback fn;  void* userData; };
    struct PlanCompletedCB    { PlanCompletedCallback fn;    void* userData; };
    struct UserInputNeededCB  { UserInputNeededCallback fn;  void* userData; };

    std::vector<PlanStartedCB>      m_planStartedCBs;
    std::vector<ActionStartedCB>    m_actionStartedCBs;
    std::vector<ActionCompletedCB>  m_actionCompletedCBs;
    std::vector<ActionFailedCB>     m_actionFailedCBs;
    std::vector<ProgressUpdatedCB>  m_progressUpdatedCBs;
    std::vector<PlanCompletedCB>    m_planCompletedCBs;
    std::vector<UserInputNeededCB>  m_userInputNeededCBs;
};
