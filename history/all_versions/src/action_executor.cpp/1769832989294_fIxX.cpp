#include "agent/action_executor.hpp"
#include <iostream>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

bool ActionExecutor::executeAction(Action& action) {
    if (onActionStarted) onActionStarted(0, action.description);
    
    std::cout << "[ActionExecutor] Executing: " << action.description << " (" << (int)action.type << ")" << std::endl;
    
    // Simulate work
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    action.executed = true;
        action.result = nlohmann::json({{"status", "success"}}).dump();
    
    if (onActionCompleted) onActionCompleted(0, true, action.result);
    return true;
}

void ActionExecutor::executePlan(const nlohmann::json& actions, bool stopOnError) {
    if (onPlanStarted) onPlanStarted(actions.size());
    
    int index = 0;
    for (const auto& item : actions) {
        // Parse action
        Action action;
        action.description = item.value("description", "Unknown Action");
        // ... (Simplified parsing)
        
        executeAction(action);
        index++;
        if (onProgressUpdated) onProgressUpdated(index, actions.size());
    }
    
    if (onPlanCompleted) onPlanCompleted(true, nlohmann::json({{"total", index}}));
}

void ActionExecutor::cancelExecution() {
    std::cout << "[ActionExecutor] Cancelled." << std::endl;
}

bool ActionExecutor::rollbackAction(int actionIndex) {
    return true;
}

nlohmann::json ActionExecutor::getAggregatedResult() const {
    return {{"status", "completed"}};
}
