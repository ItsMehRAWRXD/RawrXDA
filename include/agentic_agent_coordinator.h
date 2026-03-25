<<<<<<< HEAD
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include "../src/nlohmann/json.hpp"

using json = nlohmann::json;

#include "../include/agentic_iterative_reasoning.h"

class AgenticLoopState;
class AgenticEngine;
namespace CPUInference { class CPUInferenceEngine; }

/**
 * @class AgenticAgentCoordinator
 * @brief Multi-agent coordination for complex problem solving
 */
class AgenticAgentCoordinator
{
public:
    enum class AgentRole {
        Analyzer,
        Planner,
        Executor,
        Verifier,
        Optimizer,
        Learner
    };

    struct AgentInstance {
        std::string agentId;
        AgentRole role;
        std::unique_ptr<AgenticIterativeReasoning> reasoner;
        std::unique_ptr<AgenticLoopState> state;
        std::string currentTask;
        bool isAvailable;
        float utilization;
        int tasksCompleted;
        std::string lastResult;
        std::chrono::system_clock::time_point lastActive;
    };

    struct TaskAssignment {
        std::string taskId;
        std::string assignedAgentId;
        AgentRole requiredRole;
        std::string description;
        json parameters;
        std::chrono::system_clock::time_point assignedTime;
        std::chrono::system_clock::time_point completionTime;
        std::string status;
        json result;
        float estimatedComplexity;
    };

    struct AgentConflict {
        std::string conflictId;
        std::string agent1Id;
        std::string agent2Id;
        std::string conflictReason;
        json state1;
        json state2;
        std::string resolution;
        std::chrono::system_clock::time_point timestamp;
    };

public:
    explicit AgenticAgentCoordinator();
    ~AgenticAgentCoordinator();

    void initialize(AgenticEngine* engine, CPUInference::CPUInferenceEngine* inference);

    std::string createAgent(AgentRole role);
    void removeAgent(const std::string& agentId);
    AgentInstance* getAgent(const std::string& agentId);
    std::vector<AgentInstance*> getAvailableAgents(AgentRole role);
    std::vector<std::string> getAllAgentIds() const;
    json getAgentStatus(const std::string& agentId);
    json getAllAgentStatuses();

    std::string assignTask(
        const std::string& taskDescription,
        const json& parameters,
        AgentRole preferredRole = AgentRole::Analyzer);

    void updateTaskStatus(const std::string& taskId, const std::string& status, const json& result = json());
    json getTaskResult(const std::string& taskId);

    void resolveConflicts();
    void synchronizeState();
    json performJointInference(const std::vector<std::string>& agentIds, const std::string& prompt);

protected:
    void logCoordination(const std::string& message);

private:
    std::string selectBestAgentForTask(const std::string& taskDescription);

    AgenticEngine* m_engine = nullptr;
    CPUInference::CPUInferenceEngine* m_inference = nullptr;
    
    std::unordered_map<std::string, std::unique_ptr<AgentInstance>> m_agents;
    std::unordered_map<std::string, std::unique_ptr<TaskAssignment>> m_assignments;
    std::vector<AgentConflict> m_conflicts;
    
    std::chrono::system_clock::time_point m_coordinationStartTime;
    int m_totalTasksAssigned = 0;
};
=======
#pragma once

#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <iostream>
#include "../src/nlohmann/json.hpp"

using json = nlohmann::json;

#include "../include/agentic_iterative_reasoning.h"

class AgenticLoopState;
class AgenticEngine;
namespace CPUInference { class CPUInferenceEngine; }

/**
 * @class AgenticAgentCoordinator
 * @brief Multi-agent coordination for complex problem solving
 */
class AgenticAgentCoordinator
{
public:
    enum class AgentRole {
        Analyzer,
        Planner,
        Executor,
        Verifier,
        Optimizer,
        Learner
    };

    struct AgentInstance {
        std::string agentId;
        AgentRole role;
        std::unique_ptr<AgenticIterativeReasoning> reasoner;
        std::unique_ptr<AgenticLoopState> state;
        std::string currentTask;
        bool isAvailable;
        float utilization;
        int tasksCompleted;
        std::string lastResult;
        std::chrono::system_clock::time_point lastActive;
    };

    struct TaskAssignment {
        std::string taskId;
        std::string assignedAgentId;
        AgentRole requiredRole;
        std::string description;
        json parameters;
        std::chrono::system_clock::time_point assignedTime;
        std::chrono::system_clock::time_point completionTime;
        std::string status;
        json result;
        float estimatedComplexity;
    };

    struct AgentConflict {
        std::string conflictId;
        std::string agent1Id;
        std::string agent2Id;
        std::string conflictReason;
        json state1;
        json state2;
        std::string resolution;
        std::chrono::system_clock::time_point timestamp;
    };

public:
    explicit AgenticAgentCoordinator();
    ~AgenticAgentCoordinator();

    void initialize(AgenticEngine* engine, CPUInference::CPUInferenceEngine* inference);

    std::string createAgent(AgentRole role);
    void removeAgent(const std::string& agentId);
    AgentInstance* getAgent(const std::string& agentId);
    std::vector<AgentInstance*> getAvailableAgents(AgentRole role);
    std::vector<std::string> getAllAgentIds() const;
    json getAgentStatus(const std::string& agentId);
    json getAllAgentStatuses();

    std::string assignTask(
        const std::string& taskDescription,
        const json& parameters,
        AgentRole preferredRole = AgentRole::Analyzer);

    void updateTaskStatus(const std::string& taskId, const std::string& status, const json& result = json());
    json getTaskResult(const std::string& taskId);

    void resolveConflicts();
    void synchronizeState();
    json performJointInference(const std::vector<std::string>& agentIds, const std::string& prompt);

protected:
    void logCoordination(const std::string& message);

private:
    std::string selectBestAgentForTask(const std::string& taskDescription);

    AgenticEngine* m_engine = nullptr;
    CPUInference::CPUInferenceEngine* m_inference = nullptr;
    
    std::unordered_map<std::string, std::unique_ptr<AgentInstance>> m_agents;
    std::unordered_map<std::string, std::unique_ptr<TaskAssignment>> m_assignments;
    std::vector<AgentConflict> m_conflicts;
    
    std::chrono::system_clock::time_point m_coordinationStartTime;
    int m_totalTasksAssigned = 0;
};
>>>>>>> origin/main
