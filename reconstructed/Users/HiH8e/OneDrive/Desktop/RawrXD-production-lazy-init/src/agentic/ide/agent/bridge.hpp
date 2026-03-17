#pragma once
#include "model_invoker.hpp"
#include "action_executor.hpp"
#include <string>
#include <functional>

class IDEAgentBridge {
public:
    enum class ExecutionMode {
        AUTO_EXECUTE,
        DRY_RUN,
        MANUAL_APPROVAL
    };

    struct WishResult {
        bool success;
        std::string planId;
        std::string executionLog;
        std::string finalOutput;
    };

    IDEAgentBridge();
    ~IDEAgentBridge();

    bool initialize();
    WishResult executeWish(const std::string& wish, ExecutionMode mode = ExecutionMode::MANUAL_APPROVAL);
    
    void setModelInvokerConfig(const ModelInvoker::Config& config);
    void setExecutionMode(ExecutionMode mode);
    
    // Callbacks for UI integration
    void setPlanGeneratedCallback(std::function<void(const ModelInvoker::Plan&)> callback);
    void setProgressCallback(std::function<void(int, const std::string&)> callback);
    void setCompletionCallback(std::function<void(const WishResult&)> callback);

private:
    ModelInvoker modelInvoker_;
    ActionExecutor actionExecutor_;
    ExecutionMode executionMode_;
    
    std::function<void(const ModelInvoker::Plan&)> planGeneratedCallback_;
    std::function<void(int, const std::string&)> progressCallback_;
    std::function<void(const WishResult&)> completionCallback_;
    
    std::vector<ActionExecutor::Action> convertPlanToActions(const ModelInvoker::Plan& plan);
    bool getUserApproval(const ModelInvoker::Plan& plan);
};