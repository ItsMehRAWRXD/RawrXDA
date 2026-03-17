/**
 * @file action_executor.hpp
 * @brief Execution engine for structured action plans (Qt-free)
 */
#pragma once

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>
#include <cstdint>

enum class ActionType {
    FileEdit, SearchFiles, RunBuild, ExecuteTests, CommitGit,
    InvokeCommand, QueryUser, RecursiveAgent, Unknown
};

struct Action {
    ActionType type = ActionType::Unknown;
    std::string target;
    nlohmann::json params;
    std::string description;
    bool executed = false;
    bool success = false;
    std::string result;
    std::string error;
};

struct ExecutionContext {
    std::string projectRoot;
    std::vector<std::string> environmentVars;
    int timeoutMs = 30000;
    bool dryRun = false;
    nlohmann::json state;
    int currentActionIndex = 0;
    int totalActions = 0;
};

class ActionExecutor {
public:
    ActionExecutor() = default;
    ~ActionExecutor() = default;
    ActionExecutor(const ActionExecutor&) = delete;
    ActionExecutor& operator=(const ActionExecutor&) = delete;

    void setContext(const ExecutionContext& context);
    ExecutionContext context() const { return m_context; }
    bool executeAction(Action& action);
    void executePlan(const nlohmann::json& actions, bool stopOnError = true);
    void cancelExecution();
    bool isExecuting() const { return m_isExecuting; }
    std::vector<Action> executedActions() const { return m_executedActions; }
    bool rollbackAction(int actionIndex);
    nlohmann::json getAggregatedResult() const;

    // Callbacks (replace Qt signals)
    std::function<void(int)> onPlanStarted;
    std::function<void(int, const std::string&)> onActionStarted;
    std::function<void(int, bool, const nlohmann::json&)> onActionCompleted;
    std::function<void(int, const std::string&, bool)> onActionFailed;
    std::function<void(int, int)> onProgressUpdated;
    std::function<void(bool, const nlohmann::json&)> onPlanCompleted;
    std::function<void(const std::string&, const std::vector<std::string>&)> onUserInputNeeded;

private:
    bool handleFileEdit(Action& action);
    bool handleSearchFiles(Action& action);
    bool handleRunBuild(Action& action);
    bool handleExecuteTests(Action& action);
    bool handleCommitGit(Action& action);
    bool handleInvokeCommand(Action& action);
    bool handleRecursiveAgent(Action& action);
    bool handleQueryUser(Action& action);
    Action parseJsonAction(const nlohmann::json& jsonAction);
    bool createBackup(const std::string& filePath);
    bool restoreFromBackup(const std::string& filePath);
    nlohmann::json executeCommand(const std::string& command, const std::vector<std::string>& args, int timeoutMs);
    bool validateFileEditSafety(const std::string& filePath, const std::string& action);
    ActionType stringToActionType(const std::string& typeStr) const;

    ExecutionContext m_context;
    bool m_isExecuting = false;
    bool m_stopOnError = true;
    bool m_cancelled = false;
    std::vector<Action> m_executedActions;
    std::map<std::string, std::string> m_backups;
};
