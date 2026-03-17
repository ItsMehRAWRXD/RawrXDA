#pragma once

#include <string>
#include <vector>
#include <functional>
/* Qt removed */
#include <condition_variable>

struct AgentTask {
    std::string taskId;
    std::string input;
    std::function<std::string(const std::string&)> executor;
    std::function<void(const struct AgentResult&)> callback;
};

struct AgentResult {
    std::string taskId;
    std::string agentId;
    std::string output;
    bool success;
    std::string error;
};

class MultiAgentEngine {
public:
    explicit MultiAgentEngine(int numAgents = 4);
    ~MultiAgentEngine();
    
    // Submit task to agent pool
    void submitTask(const AgentTask& task);
    
    // Execute multiple tasks in parallel and wait for all
    std::vector<AgentResult> executeParallel(const std::vector<AgentTask>& tasks);
    
    // Execute single task
    AgentResult executeSingle(const AgentTask& task);
    
    // Query status
    int getActiveAgents() const;
    int getTotalAgents() const;
    
private:
    class Impl;
    Impl* m_impl;
};
