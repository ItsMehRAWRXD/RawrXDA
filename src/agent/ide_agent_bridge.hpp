/**
 * @file ide_agent_bridge.hpp
 * @brief Plugin interface connecting IDE UI to agent execution pipeline (Qt-free)
 */
#pragma once

#include "model_invoker.hpp"
#include "action_executor.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

struct ExecutionPlan {
    std::string wish;
    nlohmann::json actions;
    std::string reasoning;
    int estimatedTimeMs = 0;
    std::string status;
};

class IDEAgentBridge {
public:
    IDEAgentBridge();
    virtual ~IDEAgentBridge() = default;

    void initialize(const std::string& endpoint, const std::string& backend = "ollama", const std::string& apiKey = "");
    void setProjectRoot(const std::string& root);
    std::string projectRoot() const { return m_projectRoot; }
    void executeWish(const std::string& wish, bool requireApproval = true);
    void planWish(const std::string& wish);
    void approvePlan();
    void rejectPlan();
    void cancelExecution();
    bool isExecuting() const { return m_isExecuting; }
    ExecutionPlan currentPlan() const { return m_currentPlan; }
    nlohmann::json executionHistory() const { return m_executionHistory; }
    void setDryRunMode(bool enabled);
    void setStopOnError(bool stopOnError);

    /** Autonomous retry: on plan generation failure, retry up to maxRetries with exponential backoff */
    void setRetryPolicy(int maxRetries, int initialBackoffMs = 1000);

    // Callbacks (replace Qt signals)
    std::function<void(const std::string&)> onAgentThinkingStarted;
    std::function<void(const ExecutionPlan&)> onAgentGeneratedPlan;
    std::function<void(const ExecutionPlan&)> onPlanApprovalNeeded;
    std::function<void(int)> onAgentExecutionStarted;
    std::function<void(int, const std::string&, bool)> onAgentExecutionProgress;
    std::function<void(int, int, int)> onAgentProgressUpdated;
    std::function<void(const nlohmann::json&, int)> onAgentCompleted;
    std::function<void(const std::string&, bool)> onAgentError;
    std::function<void(const std::string&, const std::vector<std::string>&)> onUserInputRequested;
    std::function<void()> onExecutionCancelled;

protected:
    // Event handlers (replace Qt slots)
    void onPlanGenerated(const LLMResponse& response);
    void onActionCompleted(int index, bool success, const nlohmann::json& result);
    void onActionFailed(int index, const std::string& error, bool recoverable);
    void onPlanCompleted(bool success, const nlohmann::json& result);
    void onUserInputNeeded(const std::string& query, const std::vector<std::string>& options);

    std::string buildExecutionContext() const;
    ExecutionPlan convertToExecutionPlan(const nlohmann::json& llmPlan);
    void executeCurrentPlan();
    void recordExecution(const std::string& wish, bool success, const nlohmann::json& result, int elapsedMs);

    std::unique_ptr<ModelInvoker> m_invoker;
    std::unique_ptr<ActionExecutor> m_executor;
    bool m_isExecuting = false;
    bool m_waitingForApproval = false;
    bool m_dryRun = false;
    std::string m_projectRoot;
    ExecutionPlan m_currentPlan;
    nlohmann::json m_executionHistory = nlohmann::json::array();
    int m_executionStartTime = 0;
    bool m_requireApproval = true;
    bool m_stopOnError = true;

    int m_maxRetries = 0;
    int m_retryBackoffMs = 1000;
    int m_retriesLeft = 0;
    int m_currentRetryBackoffMs = 1000;
    InvocationParams m_lastParams;
    void retryPlanGeneration();
};
