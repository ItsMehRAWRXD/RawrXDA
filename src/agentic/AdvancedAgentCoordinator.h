#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <chrono>
#include <functional>
#include <memory>
#include <atomic>
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agentic {

// Forward declaration
class AgentCoordinator;

// Full definitions (used by AgentOrchestrator and AdvancedAgentCoordinator)
struct AgentTask {
    std::string id;
    std::string description;
    std::string specialization;
    nlohmann::json parameters;
    std::function<void(const nlohmann::json&)> callback;
};

struct AgentMetrics {
    size_t totalAgents     = 0;
    size_t healthyAgents   = 0;
    size_t pendingTasks    = 0;
    double averageLoad     = 0.0;
    size_t activeRecoveries = 0;
};

// =============================================================================
// Batch 2: Advanced Agent Coordination (7 Enhancements)
// =============================================================================

// Enhancement 1: Dynamic Load Balancing
struct AgentLoad {
    double currentLoad = 0.0;        // 0.0-1.0
    int activeTasks = 0;
    int maxConcurrentTasks = 4;
    std::chrono::steady_clock::time_point lastTaskTime;
    std::vector<std::string> specializations;
};

// Enhancement 2: Agent Health Monitoring
struct AgentHealth {
    bool isHealthy = true;
    int consecutiveFailures = 0;
    int totalTasksProcessed = 0;
    int successfulTasks = 0;
    double avgResponseTimeMs = 0.0;
    std::chrono::steady_clock::time_point lastHealthCheck;
    std::string failureReason;
};

// Enhancement 3: Task Dependency Management
struct TaskDependency {
    std::string taskId;
    std::vector<std::string> prerequisites;
    std::vector<std::string> dependents;
    bool completed = false;
    std::chrono::steady_clock::time_point completionTime;
};

// Enhancement 4: Priority-based Scheduling
enum class TaskPriority {
    CRITICAL = 0,    // System stability, immediate response required
    HIGH = 1,        // User-facing features, deadlines approaching
    NORMAL = 2,      // Regular development tasks
    LOW = 3,         // Background maintenance, optimization
    BACKGROUND = 4   // Non-urgent tasks, can be delayed
};

struct PrioritizedTask {
    std::shared_ptr<AgentTask> task;
    TaskPriority priority;
    std::chrono::steady_clock::time_point submissionTime;
    int retryCount = 0;

    bool operator<(const PrioritizedTask& other) const {
        // Higher priority (lower enum value) comes first
        if (priority != other.priority) {
            return static_cast<int>(priority) > static_cast<int>(other.priority);
        }
        // Same priority: earlier submission time comes first
        return submissionTime > other.submissionTime;
    }
};

// Enhancement 5: Agent Communication Protocol
struct AgentMessage {
    std::string senderId;
    std::string receiverId;
    std::string messageType;
    nlohmann::json payload;
    std::chrono::steady_clock::time_point timestamp;
    int ttl = 10; // Time to live in hops
};

struct CommunicationChannel {
    std::string channelId;
    std::vector<std::string> subscribers;
    std::queue<AgentMessage> messageQueue;
    std::mutex queueMutex;
};

// Enhancement 6: Adaptive Scaling
struct ScalingPolicy {
    int minAgents = 2;
    int maxAgents = 16;
    double scaleUpThreshold = 0.8;    // Scale up when avg load > 80%
    double scaleDownThreshold = 0.2;  // Scale down when avg load < 20%
    int cooldownSeconds = 300;        // Don't scale again for 5 minutes
};

struct ScalingMetrics {
    double averageLoad = 0.0;
    int totalAgents = 0;
    int activeAgents = 0;
    std::chrono::steady_clock::time_point lastScalingEvent;
};

// Enhancement 7: Fault Tolerance & Recovery
struct FailureRecovery {
    std::string failedAgentId;
    std::vector<std::shared_ptr<AgentTask>> orphanedTasks;
    std::chrono::steady_clock::time_point failureTime;
    int recoveryAttempts = 0;
    bool recoveryInProgress = false;
};

struct RedundancyConfig {
    int replicationFactor = 2;        // Number of agents per specialization
    bool enableTaskRedundancy = true; // Duplicate critical tasks
    int maxRecoveryAttempts = 3;
    std::chrono::seconds recoveryTimeout = std::chrono::seconds(30);
};

// Core Advanced Agent Coordinator
class AdvancedAgentCoordinator {
public:
    static AdvancedAgentCoordinator& instance();

    // Initialization
    bool initialize(const ScalingPolicy& scaling = {},
                   const RedundancyConfig& redundancy = {});
    void shutdown();

    // Enhancement 1: Dynamic Load Balancing
    std::string selectOptimalAgent(const std::shared_ptr<AgentTask>& task);
    void updateAgentLoad(const std::string& agentId, double loadDelta);
    std::vector<std::string> getLoadBalancedAgents(int count);

    // Enhancement 2: Agent Health Monitoring
    void updateAgentHealth(const std::string& agentId, bool success,
                          double responseTimeMs, const std::string& error = "");
    AgentHealth getAgentHealth(const std::string& agentId) const;
    std::vector<std::string> getHealthyAgents() const;
    void quarantineUnhealthyAgent(const std::string& agentId);

    // Enhancement 3: Task Dependency Management
    bool addTaskDependency(const std::string& taskId, const std::vector<std::string>& prerequisites);
    bool arePrerequisitesMet(const std::string& taskId) const;
    std::vector<std::string> getReadyTasks() const;
    void markTaskCompleted(const std::string& taskId);

    // Enhancement 4: Priority-based Scheduling
    void submitTask(std::shared_ptr<AgentTask> task, TaskPriority priority = TaskPriority::NORMAL);
    std::shared_ptr<AgentTask> getNextTask();
    void reprioritizeTask(const std::string& taskId, TaskPriority newPriority);
    std::vector<std::shared_ptr<AgentTask>> getTasksByPriority(TaskPriority priority) const;

    // Enhancement 5: Agent Communication Protocol
    void sendMessage(const AgentMessage& message);
    std::vector<AgentMessage> receiveMessages(const std::string& agentId);
    void subscribeToChannel(const std::string& agentId, const std::string& channel);
    void broadcastToChannel(const std::string& channel, const nlohmann::json& payload);

    // Enhancement 6: Adaptive Scaling
    void evaluateScalingNeeds();
    bool scaleUp(int additionalAgents = 1);
    bool scaleDown(int removeAgents = 1);
    ScalingMetrics getScalingMetrics() const;

    // Enhancement 7: Fault Tolerance & Recovery
    void handleAgentFailure(const std::string& agentId, const std::string& reason);
    void redistributeOrphanedTasks(const std::string& failedAgentId);
    bool attemptRecovery(const std::string& agentId);
    std::vector<FailureRecovery> getActiveRecoveries() const;

    // Utility methods
    size_t getActiveAgentCount() const;
    size_t getPendingTaskCount() const;
    AgentMetrics getCoordinatorMetrics() const;

    // Constructor is public to allow unique_ptr ownership;
    // use instance() for singleton access, or construct directly for owned instances.
    AdvancedAgentCoordinator() = default;
    ~AdvancedAgentCoordinator() = default;

private:
    AdvancedAgentCoordinator(const AdvancedAgentCoordinator&) = delete;
    AdvancedAgentCoordinator& operator=(const AdvancedAgentCoordinator&) = delete;

    // Core data structures
    std::unordered_map<std::string, AgentLoad> m_agentLoads;
    std::unordered_map<std::string, AgentHealth> m_agentHealth;
    std::unordered_map<std::string, TaskDependency> m_taskDependencies;
    std::priority_queue<PrioritizedTask> m_taskQueue;
    std::unordered_map<std::string, CommunicationChannel> m_communicationChannels;
    std::vector<FailureRecovery> m_activeRecoveries;

    // Configuration
    ScalingPolicy m_scalingPolicy;
    RedundancyConfig m_redundancyConfig;

    // Synchronization
    mutable std::mutex m_mutex;
    std::condition_variable m_taskAvailable;
    std::atomic<bool> m_running{false};

    // Background threads
    std::thread m_scalingThread;
    std::thread m_healthMonitorThread;
    std::thread m_recoveryThread;

    // Helper methods
    void scalingLoop();
    void healthMonitoringLoop();
    void recoveryLoop();
    double calculateAgentLoadScore(const std::string& agentId) const;
    bool isAgentSuitable(const std::string& agentId, const std::shared_ptr<AgentTask>& task) const;
    void cleanupCompletedDependencies();
    void processExpiredMessages();
};

} // namespace Agentic
} // namespace RawrXD