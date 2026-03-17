#include "ide_agent_bridge.hpp"
#include <windows.h>
#include <iostream>

IDEAgentBridge::IDEAgentBridge() : executionMode_(ExecutionMode::MANUAL_APPROVAL) {}

IDEAgentBridge::~IDEAgentBridge() {}

bool IDEAgentBridge::initialize() {
    // Default configuration for model invoker
    ModelInvoker::Config config;
    config.backend = ModelInvoker::Backend::OLLAMA;
    config.endpoint = "http://localhost:11434";
    config.modelName = "llama2";
    config.maxTokens = 1000;
    config.temperature = 0.7;
    
    if (!modelInvoker_.initialize(config)) {
        return false;
    }
    
    // Set up action executor callbacks
    actionExecutor_.setProgressCallback([this](int progress, const std::string& message) {
        if (progressCallback_) {
            progressCallback_(progress, message);
        }
    });
    
    return true;
}

IDEAgentBridge::WishResult IDEAgentBridge::executeWish(const std::string& wish, ExecutionMode mode) {
    WishResult result;
    executionMode_ = mode;
    
    if (progressCallback_) {
        progressCallback_(0, "Generating plan for: " + wish);
    }
    
    // Step 1: Generate plan using model invoker
    ModelInvoker::Plan plan = modelInvoker_.invoke(wish);
    
    if (planGeneratedCallback_) {
        planGeneratedCallback_(plan);
    }
    
    if (progressCallback_) {
        progressCallback_(25, "Plan generated: " + plan.description);
    }
    
    // Step 2: Get user approval if needed
    if (executionMode_ == ExecutionMode::MANUAL_APPROVAL && plan.requiresApproval) {
        if (!getUserApproval(plan)) {
            result.success = false;
            result.executionLog = "Execution cancelled by user";
            
            if (completionCallback_) {
                completionCallback_(result);
            }
            return result;
        }
    }
    
    if (progressCallback_) {
        progressCallback_(50, "Converting plan to actions");
    }
    
    // Step 3: Convert plan to executable actions
    std::vector<ActionExecutor::Action> actions = convertPlanToActions(plan);
    
    if (progressCallback_) {
        progressCallback_(75, "Executing " + std::to_string(actions.size()) + " actions");
    }
    
    // Step 4: Execute actions
    ActionExecutor::Result executionResult;
    
    if (executionMode_ == ExecutionMode::DRY_RUN) {
        executionResult.success = true;
        executionResult.output = "Dry run completed - would execute " + std::to_string(actions.size()) + " actions";
    } else {
        executionResult = actionExecutor_.executePlan(actions);
    }
    
    // Step 5: Finalize result
    result.success = executionResult.success;
    result.planId = plan.id;
    result.executionLog = executionResult.output;
    result.finalOutput = executionResult.success ? "Wish execution completed successfully" : executionResult.error;
    
    if (progressCallback_) {
        progressCallback_(100, result.finalOutput);
    }
    
    if (completionCallback_) {
        completionCallback_(result);
    }
    
    return result;
}

std::vector<ActionExecutor::Action> IDEAgentBridge::convertPlanToActions(const ModelInvoker::Plan& plan) {
    std::vector<ActionExecutor::Action> actions;
    
    // Simple conversion - in production, this would parse the plan more intelligently
    for (const auto& planAction : plan.actions) {
        ActionExecutor::Action action;
        
        // Basic action type detection based on description
        if (planAction.find("edit") != std::string::npos || planAction.find("write") != std::string::npos) {
            action.type = ActionExecutor::ActionType::FILE_EDIT;
        } else if (planAction.find("search") != std::string::npos || planAction.find("find") != std::string::npos) {
            action.type = ActionExecutor::ActionType::SEARCH_FILES;
        } else if (planAction.find("build") != std::string::npos || planAction.find("compile") != std::string::npos) {
            action.type = ActionExecutor::ActionType::RUN_BUILD;
        } else if (planAction.find("commit") != std::string::npos) {
            action.type = ActionExecutor::ActionType::GIT_COMMIT;
        } else if (planAction.find("push") != std::string::npos) {
            action.type = ActionExecutor::ActionType::GIT_PUSH;
        } else if (planAction.find("create") != std::string::npos && planAction.find("directory") != std::string::npos) {
            action.type = ActionExecutor::ActionType::CREATE_DIRECTORY;
        } else if (planAction.find("delete") != std::string::npos || planAction.find("remove") != std::string::npos) {
            action.type = ActionExecutor::ActionType::DELETE_FILE;
        } else {
            action.type = ActionExecutor::ActionType::INVOKE_COMMAND;
        }
        
        action.description = planAction;
        actions.push_back(action);
    }
    
    return actions;
}

bool IDEAgentBridge::getUserApproval(const ModelInvoker::Plan& plan) {
    // TODO: Implement proper UI dialog for approval
    // For now, use a simple console prompt
    std::cout << "\n=== PLAN APPROVAL REQUIRED ===" << std::endl;
    std::cout << "Plan: " << plan.description << std::endl;
    std::cout << "Actions:" << std::endl;
    for (const auto& action : plan.actions) {
        std::cout << "  - " << action << std::endl;
    }
    std::cout << "Reasoning: " << plan.reasoning << std::endl;
    std::cout << "\nApprove execution? (y/n): ";
    
    char response;
    std::cin >> response;
    return (response == 'y' || response == 'Y');
}

void IDEAgentBridge::setModelInvokerConfig(const ModelInvoker::Config& config) {
    modelInvoker_.initialize(config);
}

void IDEAgentBridge::setExecutionMode(ExecutionMode mode) {
    executionMode_ = mode;
}

void IDEAgentBridge::setPlanGeneratedCallback(std::function<void(const ModelInvoker::Plan&)> callback) {
    planGeneratedCallback_ = callback;
}

void IDEAgentBridge::setProgressCallback(std::function<void(int, const std::string&)> callback) {
    progressCallback_ = callback;
}

void IDEAgentBridge::setCompletionCallback(std::function<void(const WishResult&)> callback) {
    completionCallback_ = callback;
}