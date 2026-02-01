#pragma once

#include "model_invoker.hpp"
#include "action_executor.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

struct ExecutionPlan {
    std::string wish;
    json actions;
    std::string reasoning;
    int estimatedTimeMs = 0;
    std::string status;
};

class IDEAgentBridge {
public:
    IDEAgentBridge();
    ~IDEAgentBridge();

    // No copy
    IDEAgentBridge(const IDEAgentBridge&) = delete;
    IDEAgentBridge& operator=(const IDEAgentBridge&) = delete;

    void initialize(const std::string& endpoint, const std::string& backend, const std::string& apiKey);
    void setProjectRoot(const std::string& root);
    
    void executeWish(const std::string& wish, bool requireApproval);
    void planWish(const std::string& wish);
    void cancelExecution();
    void approveExecution();
    void rejectExecution();
    
    // Explicit Logic: New Code Completion API
    std::string generateCodeCompletion(const std::string& context, const std::string& prefix);

    void setDryRun(bool dryRun);

    // Callbacks to replace Signals
    std::function<void(const std::string& msg)> onAgentThinkingStarted;
    std::function<void(const std::string& planJson)> onPlanGenerated;
    std::function<void(const std::string& error, bool retryable)> onAgentError;
    std::function<void(int step, int total, const std::string& desc)> onProgressUpdated;
    std::function<void(const std::string& result)> onExecutionComplete;
    std::function<void(const std::string& planJson)> onApprovalRequested;

    // Accessors for derived classes and integration
    ModelInvoker* getModelInvoker() const { return m_invoker.get(); }
    ActionExecutor* getActionExecutor() const { return m_executor.get(); }

private:
    std::unique_ptr<ModelInvoker> m_invoker;
    std::unique_ptr<ActionExecutor> m_executor;
    
    std::string m_projectRoot;
    std::string m_lastPlanJson;
    bool m_requireApproval = false;
    bool m_isExecuting = false;
    bool m_dryRun = false;

    // Helper to build context
    json buildExecutionContext();
    
    // Handlers
    void handlePlanGenerated(const LLMResponse& response);
    void handleExecutionResult(bool success, const std::string& result);
};
