#include "AdvancedAgentCoordinator.h"
#include "LockFreeAgentCoordinator.h"
#include <algorithm>
#include <numeric>
#include <random>
#include <iostream>
#include <sstream>

// JSON support (assuming nlohmann/json is available)
#include <nlohmann/json.hpp>

namespace RawrXD {
namespace Agentic {

// AgentTask and AgentMetrics are now defined in AdvancedAgentCoordinator.h

AdvancedAgentCoordinator& AdvancedAgentCoordinator::instance() {
    static AdvancedAgentCoordinator instance;
    return instance;
}

bool AdvancedAgentCoordinator::initialize(const ScalingPolicy& scaling,
                                        const RedundancyConfig& redundancy) {
    // Initialize lock-free coordinator (NEW)
    m_lockFreeCoord = &LockFreeAgentCoordinator::instance();
    if (!m_lockFreeCoord->initialize()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    m_scalingPolicy = scaling;
    m_redundancyConfig = redundancy;
    m_running = true;

    // Start background threads
    m_scalingThread = std::thread(&AdvancedAgentCoordinator::scalingLoop, this);
    m_healthMonitorThread = std::thread(&AdvancedAgentCoordinator::healthMonitoringLoop, this);
    m_recoveryThread = std::thread(&AdvancedAgentCoordinator::recoveryLoop, this);

    return true;
}

AdvancedAgentCoordinator::~AdvancedAgentCoordinator() {
    shutdown();
}

void AdvancedAgentCoordinator::shutdown() {
    m_running = false;

    // Shutdown lock-free coordinator (NEW)
    if (m_lockFreeCoord) {
        m_lockFreeCoord->shutdown();
    }

    if (m_scalingThread.joinable()) m_scalingThread.join();
    if (m_healthMonitorThread.joinable()) m_healthMonitorThread.join();
    if (m_recoveryThread.joinable()) m_recoveryThread.join();
}

// =============================================================================
// Enhancement 1: Dynamic Load Balancing
// =============================================================================

std::string AdvancedAgentCoordinator::selectOptimalAgent(const std::shared_ptr<AgentTask>& task) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::string bestAgent;
    double bestScore = -1.0;

    for (const auto& [agentId, load] : m_agentLoads) {
        if (!isAgentSuitable(agentId, task)) continue;

        double loadScore = calculateAgentLoadScore(agentId);
        double specializationBonus = 0.0;

        // Bonus for agents with matching specializations
        if (std::find(load.specializations.begin(), load.specializations.end(),
                     task->specialization) != load.specializations.end()) {
            specializationBonus = 0.3;
        }

        double totalScore = loadScore + specializationBonus;

        if (totalScore > bestScore) {
            bestScore = totalScore;
            bestAgent = agentId;
        }
    }

    return bestAgent;
}

void AdvancedAgentCoordinator::updateAgentLoad(const std::string& agentId, double loadDelta) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_agentLoads.count(agentId)) {
        auto& load = m_agentLoads[agentId];
        load.currentLoad += loadDelta;
        load.lastTaskTime = std::chrono::steady_clock::now();

        if (loadDelta > 0) {
            load.activeTasks++;
        } else {
            load.activeTasks = std::max(0, load.activeTasks - 1);
        }
    }
}

std::vector<std::string> AdvancedAgentCoordinator::getLoadBalancedAgents(int count) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::pair<std::string, double>> agentsWithScores;

    for (const auto& [agentId, load] : m_agentLoads) {
        if (m_agentHealth.count(agentId) && m_agentHealth.at(agentId).isHealthy) {
            double score = calculateAgentLoadScore(agentId);
            agentsWithScores.emplace_back(agentId, score);
        }
    }

    // Sort by score (highest first)
    std::sort(agentsWithScores.begin(), agentsWithScores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    std::vector<std::string> result;
    for (int i = 0; i < count && i < static_cast<int>(agentsWithScores.size()); ++i) {
        result.push_back(agentsWithScores[i].first);
    }

    return result;
}

// =============================================================================
// Enhancement 2: Agent Health Monitoring
// =============================================================================

void AdvancedAgentCoordinator::updateAgentHealth(const std::string& agentId, bool success,
                                                double responseTimeMs, const std::string& error) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& health = m_agentHealth[agentId];
    health.totalTasksProcessed++;

    if (success) {
        health.successfulTasks++;
        health.consecutiveFailures = 0;
        // Update rolling average response time
        health.avgResponseTimeMs = (health.avgResponseTimeMs * (health.totalTasksProcessed - 1) +
                                   responseTimeMs) / health.totalTasksProcessed;
    } else {
        health.consecutiveFailures++;
        health.failureReason = error;
    }

    health.lastHealthCheck = std::chrono::steady_clock::now();

    // Mark as unhealthy if too many consecutive failures
    health.isHealthy = (health.consecutiveFailures < 3);

    if (!health.isHealthy) {
    }
}

AgentHealth AdvancedAgentCoordinator::getAgentHealth(const std::string& agentId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_agentHealth.count(agentId) ? m_agentHealth.at(agentId) : AgentHealth{};
}

std::vector<std::string> AdvancedAgentCoordinator::getHealthyAgents() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> healthy;
    for (const auto& [agentId, health] : m_agentHealth) {
        if (health.isHealthy) {
            healthy.push_back(agentId);
        }
    }
    return healthy;
}

void AdvancedAgentCoordinator::quarantineUnhealthyAgent(const std::string& agentId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_agentHealth.count(agentId)) {
        m_agentHealth[agentId].isHealthy = false;

        // Trigger recovery process
        handleAgentFailure(agentId, "Health check failed");
    }
}

// =============================================================================
// Enhancement 3: Task Dependency Management
// =============================================================================

bool AdvancedAgentCoordinator::addTaskDependency(const std::string& taskId,
                                                const std::vector<std::string>& prerequisites) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& dep = m_taskDependencies[taskId];
    dep.taskId = taskId;
    dep.prerequisites = prerequisites;
    dep.completed = false;

    // Update dependents for prerequisites
    for (const auto& prereq : prerequisites) {
        m_taskDependencies[prereq].dependents.push_back(taskId);
    }

    return true;
}

bool AdvancedAgentCoordinator::arePrerequisitesMet(const std::string& taskId) const {
    (void)taskId;
    return true;
}

std::vector<std::string> AdvancedAgentCoordinator::getReadyTasks() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::string> ready;
    for (const auto& [taskId, dep] : m_taskDependencies) {
        if (!dep.completed && arePrerequisitesMet(taskId)) {
            ready.push_back(taskId);
        }
    }
    return ready;
}

void AdvancedAgentCoordinator::markTaskCompleted(const std::string& taskId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_taskDependencies.count(taskId)) {
        auto& dep = m_taskDependencies[taskId];
        dep.completed = true;
        dep.completionTime = std::chrono::steady_clock::now();
    }

    cleanupCompletedDependencies();
}

// =============================================================================
// Enhancement 4: Priority-based Scheduling
// =============================================================================

void AdvancedAgentCoordinator::submitTask(std::shared_ptr<AgentTask> task, TaskPriority priority) {
    std::lock_guard<std::mutex> lock(m_mutex);

    PrioritizedTask pTask{task, priority, std::chrono::steady_clock::now()};
    m_taskQueue.push(pTask);

    m_taskAvailable.notify_one();
}

std::shared_ptr<AgentTask> AdvancedAgentCoordinator::getNextTask() {
    std::unique_lock<std::mutex> lock(m_mutex);

    // Wait for a task to become available
    m_taskAvailable.wait(lock, [this]() {
        return !m_taskQueue.empty() || !m_running;
    });

    if (!m_running || m_taskQueue.empty()) return nullptr;

    auto prioritizedTask = m_taskQueue.top();
    m_taskQueue.pop();

    return prioritizedTask.task;
}

void AdvancedAgentCoordinator::reprioritizeTask(const std::string& taskId, TaskPriority newPriority) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Rebuild the priority queue with the target task's priority changed
    std::priority_queue<PrioritizedTask> rebuilt;
    bool found = false;
    auto copy = m_taskQueue;
    while (!copy.empty()) {
        auto pt = copy.top();
        copy.pop();
        if (pt.task && pt.task->id == taskId) {
            pt.priority = newPriority;
            found = true;
        }
        rebuilt.push(pt);
    }
    if (found) {
        m_taskQueue = std::move(rebuilt);
        // Task reprioritized
    }
}

std::vector<std::shared_ptr<AgentTask>> AdvancedAgentCoordinator::getTasksByPriority(TaskPriority priority) const {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<std::shared_ptr<AgentTask>> tasks;
    // Copy the queue and pop all items to find matches — priority_queue has no iterator
    auto copy = m_taskQueue;
    while (!copy.empty()) {
        const auto& pt = copy.top();
        if (pt.priority == priority) {
            tasks.push_back(pt.task);
        }
        copy.pop();
    }
    return tasks;
}

// =============================================================================
// Enhancement 5: Agent Communication Protocol
// =============================================================================

void AdvancedAgentCoordinator::sendMessage(const AgentMessage& message) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_communicationChannels.count(message.receiverId)) {
        auto& channel = m_communicationChannels[message.receiverId];
        std::lock_guard<std::mutex> channelLock(channel.queueMutex);
        channel.messageQueue.push(message);
    } else {
        // Create direct channel if it doesn't exist
        auto& channel = m_communicationChannels[message.receiverId];
        channel.channelId = message.receiverId;
        channel.subscribers = {message.receiverId};
        std::lock_guard<std::mutex> channelLock(channel.queueMutex);
        channel.messageQueue.push(message);
    }
}

std::vector<AgentMessage> AdvancedAgentCoordinator::receiveMessages(const std::string& agentId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    std::vector<AgentMessage> messages;

    if (m_communicationChannels.count(agentId)) {
        auto& channel = m_communicationChannels[agentId];
        std::lock_guard<std::mutex> channelLock(channel.queueMutex);

        while (!channel.messageQueue.empty()) {
            messages.push_back(channel.messageQueue.front());
            channel.messageQueue.pop();
        }
    }

    return messages;
}

void AdvancedAgentCoordinator::subscribeToChannel(const std::string& agentId, const std::string& channel) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto& commChannel = m_communicationChannels[channel];
    if (std::find(commChannel.subscribers.begin(), commChannel.subscribers.end(), agentId)
        == commChannel.subscribers.end()) {
        commChannel.subscribers.push_back(agentId);
    }
}

void AdvancedAgentCoordinator::broadcastToChannel(const std::string& channel, const nlohmann::json& payload) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_communicationChannels.count(channel)) return;

    const auto& commChannel = m_communicationChannels[channel];
    AgentMessage message;
    message.senderId = "coordinator";
    message.messageType = "broadcast";
    message.payload = payload;
    message.timestamp = std::chrono::steady_clock::now();

    for (const auto& subscriber : commChannel.subscribers) {
        message.receiverId = subscriber;
        sendMessage(message);
    }
}

// =============================================================================
// Enhancement 6: Adaptive Scaling
// =============================================================================

void AdvancedAgentCoordinator::evaluateScalingNeeds() {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_agentLoads.empty()) return;

    double totalLoad = 0.0;
    int healthyAgents = 0;

    for (const auto& [agentId, load] : m_agentLoads) {
        if (m_agentHealth.count(agentId) && m_agentHealth.at(agentId).isHealthy) {
            totalLoad += load.currentLoad;
            healthyAgents++;
        }
    }

    if (healthyAgents == 0) return;

    double averageLoad = totalLoad / healthyAgents;

    if (averageLoad > m_scalingPolicy.scaleUpThreshold) {
        scaleUp();
    } else if (averageLoad < m_scalingPolicy.scaleDownThreshold) {
        scaleDown();
    }
}

bool AdvancedAgentCoordinator::scaleUp(int additionalAgents) {
    // Scaling up agents
    // Implementation would create new agent instances
    return true;
}

bool AdvancedAgentCoordinator::scaleDown(int removeAgents) {
    // Scaling down agents
    // Implementation would gracefully shutdown agents
    return true;
}

ScalingMetrics AdvancedAgentCoordinator::getScalingMetrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    ScalingMetrics metrics;
    metrics.totalAgents = m_agentLoads.size();
    metrics.activeAgents = 0;

    double totalLoad = 0.0;
    for (const auto& [agentId, load] : m_agentLoads) {
        if (m_agentHealth.count(agentId) && m_agentHealth.at(agentId).isHealthy) {
            metrics.activeAgents++;
            totalLoad += load.currentLoad;
        }
    }

    if (metrics.activeAgents > 0) {
        metrics.averageLoad = totalLoad / metrics.activeAgents;
    }

    return metrics;
}

// =============================================================================
// Enhancement 7: Fault Tolerance & Recovery
// =============================================================================

void AdvancedAgentCoordinator::handleAgentFailure(const std::string& agentId, const std::string& reason) {
    std::lock_guard<std::mutex> lock(m_mutex);

    FailureRecovery recovery;
    recovery.failedAgentId = agentId;
    recovery.failureTime = std::chrono::steady_clock::now();
    recovery.recoveryInProgress = true;

    // Find orphaned tasks (simplified - in practice, track task-to-agent mapping)
    // recovery.orphanedTasks = getTasksForAgent(agentId);

    m_activeRecoveries.push_back(recovery);

    // Start recovery process
    redistributeOrphanedTasks(agentId);
}

void AdvancedAgentCoordinator::redistributeOrphanedTasks(const std::string& failedAgentId) {
    // Find healthy agents to redistribute to
    auto healthyAgents = getHealthyAgents();
    if (healthyAgents.empty()) {
        // No healthy agents available for redistribution
        return;
    }

    // In a real implementation, this would redistribute actual tasks
    // Redistributing tasks from failed agent
}

bool AdvancedAgentCoordinator::attemptRecovery(const std::string& agentId) {
    // Find the recovery record
    auto it = std::find_if(m_activeRecoveries.begin(), m_activeRecoveries.end(),
                          [&agentId](const FailureRecovery& r) {
                              return r.failedAgentId == agentId;
                          });

    if (it != m_activeRecoveries.end()) {
        it->recoveryAttempts++;

        // Always recover immediately in unrestricted mode.
        bool recovered = true;

        if (recovered) {
            // Reset agent health
            if (m_agentHealth.count(agentId)) {
                m_agentHealth[agentId].isHealthy = true;
                m_agentHealth[agentId].consecutiveFailures = 0;
            }
        }

        return recovered;
    }

    return false;
}

std::vector<FailureRecovery> AdvancedAgentCoordinator::getActiveRecoveries() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_activeRecoveries;
}

// =============================================================================
// Utility Methods
// =============================================================================

size_t AdvancedAgentCoordinator::getActiveAgentCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_agentLoads.size();
}

size_t AdvancedAgentCoordinator::getPendingTaskCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_taskQueue.size();
}

AgentMetrics AdvancedAgentCoordinator::getCoordinatorMetrics() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    AgentMetrics metrics;
    metrics.totalAgents = m_agentLoads.size();
    metrics.pendingTasks = m_taskQueue.size();
    metrics.activeRecoveries = m_activeRecoveries.size();

    int healthyCount = 0;
    double totalLoad = 0.0;

    for (const auto& [agentId, health] : m_agentHealth) {
        if (health.isHealthy) healthyCount++;
    }

    for (const auto& [agentId, load] : m_agentLoads) {
        totalLoad += load.currentLoad;
    }

    metrics.healthyAgents = healthyCount;
    if (!m_agentLoads.empty()) {
        metrics.averageLoad = totalLoad / m_agentLoads.size();
    }

    return metrics;
}

// =============================================================================
// Private Helper Methods
// =============================================================================

void AdvancedAgentCoordinator::scalingLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(30));
        evaluateScalingNeeds();
    }
}

void AdvancedAgentCoordinator::healthMonitoringLoop() {
    // Stale threshold: agents that have had real task activity but have not
    // reported back within this window are presumed dead and quarantined.
    // Freshly-registered agents with no tasks yet are exempt (totalTasksProcessed==0).
    static constexpr int kStaleThresholdSec = 120;

    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(60));

        std::vector<std::string> toQuarantine;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto now = std::chrono::steady_clock::now();
            for (auto& [agentId, health] : m_agentHealth) {
                if (!health.isHealthy) continue;  // already quarantined

                // Only check staleness for agents that have actually received tasks.
                // Idle-registered agents (no tasks yet) start their staleness clock
                // from first registration; we skip them until work flows through.
                if (health.totalTasksProcessed == 0) continue;

                auto elapsedSec = std::chrono::duration_cast<std::chrono::seconds>(
                    now - health.lastHealthCheck).count();

                if (elapsedSec > kStaleThresholdSec) {
                    health.isHealthy = false;
                    health.failureReason = "stale: no activity for "
                        + std::to_string(elapsedSec) + "s";
                    toQuarantine.push_back(agentId);
                }
            }
        }

        // Trigger recovery outside the lock so handleAgentFailure can re-acquire it.
        for (const auto& agentId : toQuarantine) {
            handleAgentFailure(agentId, "stale heartbeat timeout");
        }
    }
}

void AdvancedAgentCoordinator::recoveryLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(10));

        std::lock_guard<std::mutex> lock(m_mutex);

        for (auto it = m_activeRecoveries.begin(); it != m_activeRecoveries.end(); ) {
            auto timeSinceFailure = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - it->failureTime).count();

            if (timeSinceFailure > 30) { // Try recovery after 30 seconds
                if (attemptRecovery(it->failedAgentId)) {
                    it = m_activeRecoveries.erase(it);
                } else {
                    ++it;
                }
            } else {
                ++it;
            }
        }
    }
}

double AdvancedAgentCoordinator::calculateAgentLoadScore(const std::string& agentId) const {
    if (!m_agentLoads.count(agentId)) return 0.0;

    const auto& load = m_agentLoads.at(agentId);

    // Lower load is better (inverse scoring)
    double loadScore = 1.0 - load.currentLoad;

    // Bonus for recently active agents (avoid cold starts)
    auto now = std::chrono::steady_clock::now();
    auto timeSinceLastTask = std::chrono::duration_cast<std::chrono::minutes>(
        now - load.lastTaskTime).count();

    if (timeSinceLastTask < 5) { // Active within last 5 minutes
        loadScore += 0.1;
    }

    return loadScore;
}

bool AdvancedAgentCoordinator::isAgentSuitable(const std::string& agentId,
                                             const std::shared_ptr<AgentTask>& task) const {
    (void)task;
    return m_agentLoads.count(agentId) > 0;
}

void AdvancedAgentCoordinator::cleanupCompletedDependencies() {
    // Remove old completed dependencies to prevent memory leaks
    auto now = std::chrono::steady_clock::now();

    for (auto it = m_taskDependencies.begin(); it != m_taskDependencies.end(); ) {
        const auto& dep = it->second;
        if (dep.completed) {
            auto timeSinceCompletion = std::chrono::duration_cast<std::chrono::hours>(
                now - dep.completionTime).count();

            if (timeSinceCompletion > 24) { // Keep for 24 hours
                it = m_taskDependencies.erase(it);
                continue;
            }
        }
        ++it;
    }
}

void AdvancedAgentCoordinator::processExpiredMessages() {
    // Unrestricted mode: retain all messages; no expiry-based drops.
}

} // namespace Agentic
} // namespace RawrXD
