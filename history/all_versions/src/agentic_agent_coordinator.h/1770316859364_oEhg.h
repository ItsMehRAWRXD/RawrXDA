#pragma once

#include <string>
#include <map>
#include <vector>
#include <memory>
#include <chrono>
#include <functional>
#include <iostream>

// Forward declarations
class AgenticEngine;
namespace CPUInference {
    class CPUInferenceEngine;
}
class AgenticIterativeReasoning;
class AgenticLoopState;

// Simple replacement for QJsonObject
struct AgentStatus {
    std::string agent_id;
    int role;
    bool available;
    float utilization;
    int tasks_completed;
    std::string last_active;
    
    // To JSON helper if needed, or just public members
};

// Task Assignment replacement
struct TaskAssignment {
    std::string taskId;
    std::string assignedAgentId;
    int requiredRole; // AgentRole
    std::string description;
    std::map<std::string, std::string> parameters;
    std::chrono::system_clock::time_point assignedTime;
    std::string status;
    std::map<std::string, std::string> result;
    std::chrono::system_clock::time_point completionTime;
};

struct AgentConflict {
    std::string conflictId;
    std::string agent1Id;
    std::string agent2Id;
    std::string conflictReason;
    std::string resolution;
    std::chrono::system_clock::time_point timestamp;
};

class AgenticAgentCoordinator {
public:
    enum class AgentRole {
        Developer = 0,
        Architect = 1,
        Tester = 2,
        Reviewer = 3,
        Analyzer = 4 // Added from cpp usage
    };

    struct AgentInstance {
        std::string agentId;
        AgentRole role;
        std::unique_ptr<AgenticIterativeReasoning> reasoner;
        std::unique_ptr<AgenticLoopState> state;
        bool isAvailable;
        float utilization;
        int tasksCompleted;
        std::chrono::system_clock::time_point lastActive;
        std::string currentTask;
        std::string lastResult;
    };

    AgenticAgentCoordinator();
    ~AgenticAgentCoordinator();

    void initialize(AgenticEngine* engine, CPUInference::CPUInferenceEngine* inference);

    std::string createAgent(AgentRole role);
    void removeAgent(const std::string& agentId);
    
    AgentInstance* getAgent(const std::string& agentId);
    std::vector<AgentInstance*> getAvailableAgents(AgentRole role);
    std::vector<std::string> getAllAgentIds();

    AgentStatus getAgentStatus(const std::string& agentId);
    std::vector<AgentStatus> getAllAgentStatuses();

    // Task Assignment
    std::string assignTask(const std::string& taskDescription, 
                           const std::map<std::string, std::string>& parameters, 
                           AgentRole requiredRole);
                           
    bool executeAssignedTask(const std::string& taskId);
    
    std::string getTaskStatus(const std::string& taskId);
    std::map<std::string, std::string> getTaskResult(const std::string& taskId);
    std::vector<std::map<std::string, std::string>> getAllTaskStatuses();

    // Coordination
    std::vector<std::string> decomposeLargeTask(const std::string& goal); // Simplified return
    void synchronizeAgentStates();
    bool detectStateConflict(const std::string& agentId1, const std::string& agentId2);
    std::string resolveStateConflict(const std::string& agentId1, const std::string& agentId2);
    bool buildConsensus(const std::vector<std::string>& agentIds, const std::string& question);
    std::string resolveDisagreement(const std::vector<std::string>& agentIds);
    
    void allocateResources(const std::string& agentId, float cpuShare, float memoryShare);
    void rebalanceResources();

    // Monitoring
    std::map<std::string, float> getCoordinationMetrics() const;
    float getTotalUtilization() const;
    int getTotalTasksCompleted() const;
    float getAverageTaskDuration() const;
    std::vector<AgentConflict> getConflictHistory();
    
    // Load Balancing
    std::string selectBestAgentForTask(const std::string& taskDescription);
    void rebalanceWorkload();
    
    // State
    // Simplified Global State representation
    struct GlobalState {
        std::vector<AgentStatus> agents;
        std::vector<std::map<std::string, std::string>> tasks;
        std::map<std::string, float> metrics;
    };
    
    GlobalState getGlobalState();
    bool restoreGlobalState(const GlobalState& state);
    void saveCheckpoint();
    bool restoreFromCheckpoint();

    // Callbacks replacements (function pointers or std::function)
    using TaskAssignedCallback = std::function<void(const std::string& taskId, const std::string& agentId)>;
    using TaskCompletedCallback = std::function<void(const std::string& taskId)>;
    using TaskFailedCallback = std::function<void(const std::string& taskId, const std::string& reason)>;
    
    void setTaskAssignedCallback(TaskAssignedCallback cb) { m_taskAssignedCallback = cb; }
    void setTaskCompletedCallback(TaskCompletedCallback cb) { m_taskCompletedCallback = cb; }

private:
    std::string getAgentOpinion(const std::string& agentId, const std::string& question);
    std::string reconcileConflictingStates(const std::map<std::string, std::string>& state1, const std::map<std::string, std::string>& state2);
    void syncStateWithAgent(const std::string& agentId);
    void recordConflict(const std::string& agent1, const std::string& agent2, const std::string& reason, const std::string& resolution);
    void updateAgentMetrics(const std::string& agentId);

    AgenticEngine* m_engine = nullptr;
    CPUInference::CPUInferenceEngine* m_inference = nullptr;
    
    // Use m_inference in cpp, but cpp used m_inferenceEngine, so let's keep it consistent
    CPUInference::CPUInferenceEngine* m_inferenceEngine = nullptr; 

    std::chrono::system_clock::time_point m_coordinationStartTime;

    std::map<std::string, std::unique_ptr<AgentInstance>> m_agents;
    std::map<std::string, std::unique_ptr<TaskAssignment>> m_assignments;
    std::vector<std::pair<std::string, int>> m_taskDurations; // description, duration_ms
    std::vector<AgentConflict> m_conflicts;

    int m_totalTasksAssigned = 0;
    int m_totalTasksCompleted = 0;
    int m_totalConflicts = 0;
    
    GlobalState m_lastCheckpoint;
    std::chrono::system_clock::time_point m_lastCheckpointTime;
    
    TaskAssignedCallback m_taskAssignedCallback;
    TaskCompletedCallback m_taskCompletedCallback;
};
