#pragma once
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <atomic>
#include "nlohmann/json.hpp"
#include "agentic_engine.h"

// Action Types definition
enum class ActionType {
    Unknown,
    FileEdit,
    SearchFiles,
    RunBuild,
    ExecuteTests,
    CommitGit,
    InvokeCommand,
    RecursiveAgent,
    QueryUser
};

struct Action {
    ActionType type = ActionType::Unknown;
    std::string target;
    std::string description;
    nlohmann::json params;
    bool executed = false;
    bool success = false;
    std::string result;
    std::string error;
};

struct ExecutionContext {
    std::string projectRoot;
    int timeoutMs = 30000;
    int currentActionIndex = 0;
};

class ActionExecutor {
public:
    ActionExecutor();
    ~ActionExecutor();

    void setContext(const ExecutionContext& context);
    void setAgenticEngine(AgenticEngine* engine) { m_agenticEngine = engine; }
    
    // Core Execution Method
    bool executeAction(Action& action);
    void executePlan(const nlohmann::json& actions, bool stopOnError = true);
    void cancelExecution();
    bool rollbackAction(int actionIndex);
    
    // Callbacks
    std::function<void(int total)> onPlanStarted;
    std::function<void(int index, const std::string& desc)> onActionStarted;
    std::function<void(int index, bool success, const nlohmann::json& result)> onActionCompleted;
    std::function<void(int index, const std::string& error, bool canRetry)> onActionFailed;
    std::function<void(int current, int total)> onProgressUpdated;
    std::function<void(bool success, const nlohmann::json& summary)> onPlanCompleted;
    std::function<void(const std::string& prompt, const std::vector<std::string>& options)> onUserInputNeeded;

    nlohmann::json getAggregatedResult() const;

private:
    ExecutionContext m_context;
    AgenticEngine* m_agenticEngine = nullptr;
    std::atomic<bool> m_isExecuting;
    std::atomic<bool> m_stopOnError;
    std::atomic<bool> m_cancelled;
    
    std::vector<Action> m_executedActions;
    std::map<std::string, std::string> m_backups;

    // Helpers
    Action parseJsonAction(const nlohmann::json& jsonAction);
    ActionType stringToActionType(const std::string& typeStr) const;
    bool createBackup(const std::string& filePath);
    bool restoreFromBackup(const std::string& filePath);
    nlohmann::json executeCommand(const std::string& command, const std::vector<std::string>& args, int timeoutMs);

    // Specific Handlers
    bool handleFileEdit(Action& action);
    bool handleSearchFiles(Action& action);
    bool handleRunBuild(Action& action);
    bool handleExecuteTests(Action& action);
    bool handleCommitGit(Action& action);
    bool handleInvokeCommand(Action& action);
    bool handleRecursiveAgent(Action& action);
    bool handleQueryUser(Action& action);
};
