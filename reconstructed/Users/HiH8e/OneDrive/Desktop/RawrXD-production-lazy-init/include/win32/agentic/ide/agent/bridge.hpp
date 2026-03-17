#pragma once
#ifndef IDE_AGENT_BRIDGE_H
#define IDE_AGENT_BRIDGE_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <unordered_map>
#include "model_invoker.hpp"
#include "action_executor.hpp"

namespace RawrXD {

// Agent execution history entry
struct ExecutionHistoryEntry {
    std::string id;
    std::string wish;
    std::chrono::system_clock::time_point timestamp;
    bool success;
    std::chrono::milliseconds duration;
    std::string summary;
    
    ExecutionHistoryEntry() : success(false), duration(0) {}
    ExecutionHistoryEntry(const std::string& w) : wish(w), success(false), duration(0) {
        id = generateId();
        timestamp = std::chrono::system_clock::now();
    }
    
private:
    std::string generateId() {
        auto now = std::chrono::system_clock::now();
        auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            now.time_since_epoch()).count();
        return "exec_" + std::to_string(timestamp);
    }
};

// IDEAgentBridge - main plugin interface for IDE integration
class IDEAgentBridge {
public:
    using WishCallback = std::function<void(const std::string&)>;
    using PlanCallback = std::function<void(const ExecutionPlan&)>;
    using ProgressCallback = std::function<void(int, const std::string&, bool)>;
    using ResultCallback = std::function<void(const std::string&, std::chrono::milliseconds)>;
    using ErrorCallback = std::function<void(const std::string&, bool)>;
    using InputCallback = std::function<void(const std::string&, const std::vector<std::string>&)>;
    
    IDEAgentBridge();
    ~IDEAgentBridge();
    
    // Initialization
    void initialize(const std::string& endpoint = "http://localhost:11434", 
                   LLMBackend backend = LLMBackend::OLLAMA);
    void setProjectRoot(const std::string& root);
    void setDryRunMode(bool enabled);
    void setRequireApproval(bool enabled);
    
    // Main execution methods
    void executeWish(const std::string& wish, bool requireApproval = true);
    void planWish(const std::string& wish); // Preview mode
    
    // Approval workflow
    void approvePlan();
    void rejectPlan();
    
    // Execution control
    void cancelExecution();
    void pauseExecution();
    void resumeExecution();
    
    // History management
    std::vector<ExecutionHistoryEntry> getExecutionHistory(int limit = 100);
    void clearExecutionHistory();
    
    // Configuration
    void setModel(const std::string& model);
    void setTemperature(float temperature);
    void setMaxTokens(int maxTokens);
    
    // Signal equivalents (callbacks)
    void setAgentThinkingStartedCallback(std::function<void(const std::string&)> callback);
    void setAgentGeneratedPlanCallback(std::function<void(const ExecutionPlan&)> callback);
    void setPlanApprovalNeededCallback(std::function<void(const ExecutionPlan&)> callback);
    void setAgentExecutionStartedCallback(std::function<void(int)> callback);
    void setAgentExecutionProgressCallback(std::function<void(int, const std::string&, bool)> callback);
    void setAgentProgressUpdatedCallback(std::function<void(int, int, std::chrono::milliseconds)> callback);
    void setAgentCompletedCallback(std::function<void(const std::string&, std::chrono::milliseconds)> callback);
    void setAgentErrorCallback(std::function<void(const std::string&, bool)> callback);
    void setUserInputRequestedCallback(std::function<void(const std::string&, const std::vector<std::string>&)> callback);
    
private:
    std::unique_ptr<ModelInvoker> m_modelInvoker;
    std::unique_ptr<ActionExecutor> m_actionExecutor;
    
    std::string m_projectRoot;
    bool m_dryRunMode;
    bool m_requireApproval;
    bool m_executionInProgress;
    
    ExecutionPlan m_currentPlan;
    std::vector<ExecutionHistoryEntry> m_executionHistory;
    
    std::chrono::system_clock::time_point m_executionStartTime;
    
    std::function<void(const std::string&)> m_agentThinkingStartedCallback;
    std::function<void(const ExecutionPlan&)> m_agentGeneratedPlanCallback;
    std::function<void(const ExecutionPlan&)> m_planApprovalNeededCallback;
    std::function<void(int)> m_agentExecutionStartedCallback;
    std::function<void(int, const std::string&, bool)> m_agentExecutionProgressCallback;
    std::function<void(int, int, std::chrono::milliseconds)> m_agentProgressUpdatedCallback;
    std::function<void(const std::string&, std::chrono::milliseconds)> m_agentCompletedCallback;
    std::function<void(const std::string&, bool)> m_agentErrorCallback;
    std::function<void(const std::string&, const std::vector<std::string>&)> m_userInputRequestedCallback;
    
    // Internal methods
    void onPlanGenerated(const LLMResponse& response);
    void onPlanExecutionStarted(int totalActions);
    void onActionStarted(int index, const std::string& description);
    void onActionCompleted(int index, bool success, const ActionResult& result);
    void onActionFailed(int index, const std::string& error, bool recoverable);
    void onProgressUpdated(int current, int total);
    void onPlanCompleted(bool success, const PlanResult& result);
    void onInvocationError(const std::string& error, bool recoverable);
    
    // History management
    void addExecutionHistory(const std::string& wish, bool success, 
                           std::chrono::milliseconds duration, const std::string& summary);
    
    // Timing
    std::chrono::milliseconds getElapsedTime() const;
};

} // namespace RawrXD

#endif // IDE_AGENT_BRIDGE_H