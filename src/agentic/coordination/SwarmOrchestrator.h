#pragma once
#include <vector>
#include <string>
#include <memory>
#include <atomic>
#include <mutex>
#include <thread>
#include <queue>
#include <expected>
#include <unordered_map>
#include <chrono>
#include <functional>
#include <future>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

// Forward declarations to avoid circular dependencies
namespace RawrXD {
    class PlanOrchestrator;
    class CPUInferenceEngine;
    class AgenticEngine;
}

namespace RawrXD {

enum class SwarmError {
    Success = 0,
    AgentCreationFailed,
    TaskDistributionFailed,
    ConsensusFailed,
    CommunicationFailed,
    Timeout,
    InvalidTask,
    InvalidAgent,
    ExecutionFailed,
    ValidationFailed,
    BacktrackingFailed,
    MaxDepthExceeded,
    CancellationRequested,
    NoAvailableAgents,
    InsufficientConfidence,
    ResultAggregationFailed,
    ConflictResolutionFailed,
    ResourceExhausted,
    Cancelled
};

enum class AgentSpecialization {
    Coding,
    Debugging,
    Optimization,
    Analysis,
    Testing,
    Documentation,
    Security,
    Performance,
    Refactoring,
    Architecture
};

struct SwarmTask {
    std::string id;
    std::string description;
    std::vector<std::string> subtasks;
    std::unordered_map<std::string, std::string> context;
    std::chrono::steady_clock::time_point createdAt;
    std::atomic<float> progress{0.0f};
    std::atomic<bool> isCompleted{false};
    std::atomic<bool> isCancelled{false};
    std::chrono::milliseconds timeout{30000};
    float requiredConfidence{0.7f};
    std::vector<std::string> constraints;
    std::vector<std::string> dependencies;
};

struct SwarmAgent {
    std::string id;
    AgentSpecialization specialization;
    std::vector<AgentSpecialization> capabilities;
    std::atomic<bool> isBusy{false};
    std::atomic<float> confidence{1.0f};
    std::atomic<float> successRate{1.0f};
    std::chrono::steady_clock::time_point lastActive;
    std::unique_ptr<AgenticEngine> engine;
    std::atomic<size_t> tasksCompleted{0};
    std::atomic<size_t> tasksFailed{0};
    std::chrono::milliseconds averageExecutionTime{0};
    std::unordered_map<std::string, float> specializationScores;
};

struct SwarmResult {
    std::string taskId;
    std::string agentId;
    std::string result;
    float confidence;
    bool success;
    std::chrono::milliseconds duration;
    std::vector<std::string> errors;
    std::unordered_map<std::string, std::string> metadata;
};

struct ConsensusResult {
    std::string consensus;
    float confidence;
    std::vector<std::string> supportingAgents;
    std::vector<std::string> dissentingAgents;
    std::vector<std::string> reasoning;
};

class SwarmOrchestrator {
public:
    explicit SwarmOrchestrator(size_t maxAgents = 8);
    ~SwarmOrchestrator();
    
    // Non-copyable
    SwarmOrchestrator(const SwarmOrchestrator&) = delete;
    SwarmOrchestrator& operator=(const SwarmOrchestrator&) = delete;
    
    // Real swarm coordination
    std::expected<ConsensusResult, SwarmError> executeTask(const std::string& task);
    std::expected<void, SwarmError> submitTask(std::unique_ptr<SwarmTask> task);
    
    // Real agent management
    std::expected<void, SwarmError> addAgent(
        AgentSpecialization specialization,
        const std::vector<AgentSpecialization>& capabilities
    );
    
    std::expected<void, SwarmError> removeAgent(const std::string& agentId);
    std::expected<void, SwarmError> updateAgentConfidence(
        const std::string& agentId,
        float confidence
    );
    
    // Real task management
    std::expected<void, SwarmError> cancelTask(const std::string& taskId);
    std::expected<SwarmTask*, SwarmError> getTask(const std::string& taskId);
    std::vector<SwarmTask*> getActiveTasks() const;
    std::vector<SwarmTask*> getCompletedTasks() const;
    
    // Real load balancing
    std::expected<std::vector<SwarmAgent*>, SwarmError> selectAgentsForTask(
        const SwarmTask& task,
        size_t maxAgents
    );
    
    // Real consensus mechanisms
    std::expected<ConsensusResult, SwarmError> reachConsensus(
        const std::vector<SwarmResult>& results,
        const SwarmTask& task
    );
    
    // Real communication
    std::expected<void, SwarmError> broadcastMessage(
        const std::string& message,
        const std::vector<SwarmAgent*>& agents
    );
    
    std::expected<std::string, SwarmError> sendMessage(
        SwarmAgent* from,
        SwarmAgent* to,
        const std::string& message
    );
    
    // Real quality assessment
    std::expected<float, SwarmError> assessResultQuality(
        const SwarmResult& result,
        const SwarmTask& task
    );
    
    // Real conflict resolution
    std::expected<ConsensusResult, SwarmError> resolveConflicts(
        const std::vector<SwarmResult>& conflictingResults,
        const SwarmTask& task
    );
    
    // Status
    nlohmann::json getStatus() const;
    size_t getAgentCount() const { return m_agents.size(); }
    size_t getActiveTaskCount() const;
    size_t getCompletedTaskCount() const;
    float getAverageAgentConfidence() const;
    float getAverageSuccessRate() const;
    
    // Configuration
    void setMaxAgents(size_t max) { m_maxAgents = max; }
    void setTaskTimeout(std::chrono::milliseconds timeout) { m_defaultTimeout = timeout; }
    void setRequiredConfidence(float confidence) { m_requiredConfidence = confidence; }
    
private:
    size_t m_maxAgents;
    std::atomic<bool> m_running{false};
    std::atomic<bool> m_initialized{false};
    mutable std::mutex m_mutex;
    mutable std::mutex m_taskMutex;
    mutable std::mutex m_agentMutex;
    mutable std::mutex m_resultMutex;
    
    // Real components
    std::vector<std::unique_ptr<SwarmAgent>> m_agents;
    std::queue<std::unique_ptr<SwarmTask>> m_taskQueue;
    std::unordered_map<std::string, std::unique_ptr<SwarmTask>> m_activeTasks;
    std::unordered_map<std::string, std::unique_ptr<SwarmTask>> m_completedTasks;
    std::unique_ptr<PlanOrchestrator> m_planOrchestrator;
    std::unique_ptr<CPUInferenceEngine> m_inferenceEngine;
    
    std::thread m_swarmThread;
    std::thread m_taskProcessorThread;
    std::condition_variable m_taskCondition;
    
    std::atomic<size_t> m_taskIdCounter{0};
    std::atomic<size_t> m_agentIdCounter{0};
    
    // Storage for results to aggregate later
    std::vector<SwarmResult> m_subtaskResults;

    // Configuration
    std::chrono::milliseconds m_defaultTimeout{30000};
    float m_requiredConfidence{0.7f};
    size_t m_maxTaskRetries{3};
    size_t m_maxConsensusIterations{5};
    
    // Real implementation methods
    std::expected<std::vector<std::string>, SwarmError> decomposeTask(
        const std::string& task,
        const std::unordered_map<std::string, std::string>& context
    );
    
    std::expected<SwarmResult, SwarmError> executeSubtask(
        SwarmAgent* agent,
        const std::string& subtask,
        const std::unordered_map<std::string, std::string>& context,
        std::chrono::milliseconds timeout
    );
    
    std::expected<float, SwarmError> calculateAgentScore(
        const SwarmAgent* agent,
        const SwarmTask& task
    );
    
    std::expected<ConsensusResult, SwarmError> weightedVotingConsensus(
        const std::vector<SwarmResult>& results,
        const SwarmTask& task
    );
    
    std::expected<ConsensusResult, SwarmError> iterativeRefinementConsensus(
        const std::vector<SwarmResult>& results,
        const SwarmTask& task
    );
    
    std::expected<ConsensusResult, SwarmError> confidenceWeightedConsensus(
        const std::vector<SwarmResult>& results,
        const SwarmTask& task
    );
    
    void swarmLoop();
    void taskProcessorLoop() { /* Implemented as part of main loop in sample, but split here if needed */ }
    
    // Real quality metrics
    float calculateResultQuality(
        const SwarmResult& result,
        const SwarmTask& task
    );
    
    float calculateAgentConfidence(
        const SwarmAgent* agent,
        const SwarmTask& task,
        const SwarmResult& result
    );
    
    // Real load balancing
    std::vector<SwarmAgent*> selectAgentsByLoad(
        const std::vector<SwarmAgent*>& candidates,
        size_t count
    );
    
    std::vector<SwarmAgent*> selectAgentsBySpecialization(
        const std::vector<SwarmAgent*>& candidates,
        const SwarmTask& task,
        size_t count
    );
    
    // Real error recovery
    std::expected<SwarmResult, SwarmError> retrySubtask(
        const SwarmTask& task,
        const std::string& subtask,
        size_t retryCount
    );
    
    // Real result aggregation
    std::expected<std::string, SwarmError> aggregateResults(
        const std::vector<SwarmResult>& results,
        const SwarmTask& task
    );
    
    // Real conflict resolution
    std::expected<ConsensusResult, SwarmError> resolveResultConflicts(
        const std::vector<SwarmResult>& results,
        const SwarmTask& task
    );
    
    // Helpers
    std::string generateTaskId();
    std::string generateAgentId();
    AgentSpecialization stringToSpecialization(const std::string& str);
    std::string specializationToString(AgentSpecialization spec);
    void updateAgentStats(SwarmAgent* agent, const SwarmResult& result);
    void logSwarmOperation(const std::string& operation, const std::string& details);
};
} // namespace RawrXD
