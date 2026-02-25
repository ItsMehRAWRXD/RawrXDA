#pragma once
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
/* Qt removed */

#include <unordered_map>
#include <chrono>
#include <future>
#include <nlohmann/json.hpp>
#include "utils/Expected.h"

// Forward declarations if not available
class AgenticEngine; 
class PlanOrchestrator; 

using json = nlohmann::json;

namespace RawrXD {

enum class SwarmError {
    Success = 0,
    AgentCreationFailed,
    TaskDistributionFailed,
    ConsensusFailed,
    CommunicationFailed,
    Timeout,
    ExecutionFailed
};

struct SwarmTask {
    std::string id;
    std::string description;
    std::vector<std::string> subtasks;
    std::unordered_map<std::string, std::string> context;
    std::chrono::steady_clock::time_point createdAt;
    std::atomic<float> progress{0.0f};
    std::atomic<bool> isCompleted{false};
};

struct SwarmAgent {
    std::string id;
    std::string specialization;
    std::atomic<bool> isBusy{false};
    std::atomic<float> confidence{1.0f};
    std::chrono::steady_clock::time_point lastActive;
    std::unique_ptr<AgenticEngine> engine;
};

class SwarmOrchestrator {
public:
    SwarmOrchestrator(size_t maxAgents = 8);
    ~SwarmOrchestrator();
    
    // Real swarm coordination
    RawrXD::Expected<std::string, SwarmError> executeTask(const std::string& task);
    RawrXD::Expected<void, SwarmError> addAgent(const std::string& specialization);
    
    // Real task distribution
    RawrXD::Expected<void, SwarmError> distributeTask(
        SwarmTask& task,
        std::vector<SwarmAgent*>& agents
    );
    
    // Real consensus mechanisms
    RawrXD::Expected<std::string, SwarmError> reachConsensus(
        const std::vector<std::string>& proposals,
        const std::vector<float>& confidences
    );
    
    // Status
    std::vector<SwarmAgent*> getAvailableAgents() const;
    
private:
    size_t m_maxAgents;
    std::vector<std::unique_ptr<SwarmAgent>> m_agents;
    std::queue<std::unique_ptr<SwarmTask>> m_taskQueue;
    std::atomic<bool> m_running{false};
    mutable std::mutex m_mutex;
    std::thread m_swarmThread;
    
    // Helper to simulate inference engine if separate class not ready
    std::unique_ptr<AgenticEngine> m_inferenceEngine;

    void swarmLoop();
    RawrXD::Expected<std::vector<std::string>, SwarmError> decomposeTask(const std::string& task);

    RawrXD::Expected<std::string, SwarmError> executeSubtask(
        SwarmAgent* agent,
        const std::string& subtask,
        const std::unordered_map<std::string, std::string>& context
    );
    
    RawrXD::Expected<std::string, SwarmError> weightedVotingConsensus(
        const std::vector<std::string>& proposals,
        const std::vector<float>& confidences
    );
    
    std::string generateTaskId() { return "task_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()); }
    float calculateResultQuality(const std::string& result);
};

} // namespace RawrXD

