#include "AgentCoordinator.hpp"
#include <algorithm>
#include <thread>
#include <iomanip>
#include <sstream>
#include <fstream>

namespace RawrXD::Agentic::Coordination {

// Singleton implementation
AgentCoordinator& AgentCoordinator::instance() {
    static AgentCoordinator instance;
    return instance;
}

AgentCoordinator::AgentCoordinator() = default;

AgentCoordinator::~AgentCoordinator() {
    shutdown();
}

uint32_t AgentCoordinator::registerAgent(const std::string& agentName,
                                        const AgentCapabilities& capabilities) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    uint32_t agentId = nextAgentId_++;

    AgentRecord record;
    record.agentId = agentId;
    record.agentName = agentName;
    record.capabilities = capabilities;
    record.state = AgentState::IDLE;
    record.lastHeartbeat = std::chrono::steady_clock::now();

    agents_[agentId] = record;

    return agentId;
}

void AgentCoordinator::unregisterAgent(uint32_t agentId) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = agents_.find(agentId);
    if (it != agents_.end()) {
        it->second.state = AgentState::DEAD;
        // Note: Don't erase immediately; keep for statistics
    }
}

AgentState AgentCoordinator::getAgentState(uint32_t agentId) const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = agents_.find(agentId);
    if (it != agents_.end()) {
        return it->second.state;
    }
    return AgentState::UNINITIALIZED;
}

void AgentCoordinator::setAgentState(uint32_t agentId, AgentState newState) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = agents_.find(agentId);
    if (it != agents_.end()) {
        it->second.state = newState;
        it->second.lastHeartbeat = std::chrono::steady_clock::now();
    }
}

uint64_t AgentCoordinator::submitTask(uint32_t agentId, const TaskMetadata& metadata) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    // Verify agent exists
    if (agents_.find(agentId) == agents_.end()) {
        return 0;  // Invalid agent
    }

    uint64_t taskId = nextTaskId_++;

    TaskMetadata task = metadata;
    task.taskId = taskId;
    task.agentId = agentId;
    task.createdAt = std::chrono::steady_clock::now();

    tasks_[taskId] = task;

    return taskId;
}

void AgentCoordinator::cancelTask(uint64_t taskId) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        it->second.progressPercentage = 0;  // Mark as cancelled
    }
}

bool AgentCoordinator::updateTaskProgress(uint64_t taskId, uint32_t progressPercentage) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        it->second.progressPercentage = std::min(progressPercentage, 100u);
        return true;
    }
    return false;
}

TaskMetadata AgentCoordinator::getTaskMetadata(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = tasks_.find(taskId);
    if (it != tasks_.end()) {
        return it->second;
    }
    return TaskMetadata();
}

uint64_t AgentCoordinator::detectConflict(const std::vector<uint32_t>& conflictingAgents,
                                         const std::vector<uint64_t>& conflictingTasks,
                                         const std::string& resource) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    uint64_t conflictId = nextConflictId_++;

    ConflictRecord conflict;
    conflict.conflictId = conflictId;
    conflict.conflictingAgents = conflictingAgents;
    conflict.conflictingTaskIds = conflictingTasks;
    conflict.resourceInContention = resource;
    conflict.detectedAt = std::chrono::steady_clock::now();

    // Suggest strategy based on priority
    uint32_t maxPriority = 0;
    for (uint32_t agentId : conflictingAgents) {
        auto it = agents_.find(agentId);
        if (it != agents_.end() && it->second.capabilities.priority > maxPriority) {
            maxPriority = it->second.capabilities.priority;
        }
    }
    conflict.suggestedStrategy = ConflictStrategy::PRIORITY_BASED;

    conflicts_[conflictId] = conflict;

    return conflictId;
}

ConflictRecord AgentCoordinator::getConflictRecord(uint64_t conflictId) const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = conflicts_.find(conflictId);
    if (it != conflicts_.end()) {
        return it->second;
    }
    return ConflictRecord();
}

bool AgentCoordinator::resolveConflict(uint64_t conflictId, ConflictStrategy strategy,
                                       uint32_t winner) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = conflicts_.find(conflictId);
    if (it == conflicts_.end()) {
        return false;
    }

    it->second.resolved = true;
    it->second.resolutionAttempts++;

    // Mark losing agents as suspended temporarily
    for (uint32_t agentId : it->second.conflictingAgents) {
        if (agentId != winner) {
            auto agentIt = agents_.find(agentId);
            if (agentIt != agents_.end()) {
                agentIt->second.state = AgentState::SUSPENDED;
            }
        }
    }

    return true;
}

std::vector<uint64_t> AgentCoordinator::getActiveConflicts() const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    std::vector<uint64_t> active;
    for (const auto& [conflictId, conflict] : conflicts_) {
        if (!conflict.resolved) {
            active.push_back(conflictId);
        }
    }
    return active;
}

LeaseToken AgentCoordinator::acquireLease(uint32_t agentId, const std::string& resourcePath,
                                         std::chrono::milliseconds leaseDuration) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    LeaseToken token;
    token.leaseId = nextLeaseId_++;
    token.agentId = agentId;
    token.resourcePath = resourcePath;
    token.acquiredAt = std::chrono::steady_clock::now();
    token.expiresAt = token.acquiredAt + leaseDuration;
    token.isValid = true;

    leases_[token.leaseId] = token;

    return token;
}

bool AgentCoordinator::renewLease(const LeaseToken& token,
                                 std::chrono::milliseconds additionalDuration) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = leases_.find(token.leaseId);
    if (it != leases_.end() && it->second.agentId == token.agentId) {
        it->second.expiresAt = std::chrono::steady_clock::now() + additionalDuration;
        it->second.renewalCount++;
        return true;
    }
    return false;
}

void AgentCoordinator::releaseLease(const LeaseToken& token) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = leases_.find(token.leaseId);
    if (it != leases_.end()) {
        it->second.isValid = false;
    }
}

bool AgentCoordinator::validateLease(const LeaseToken& token) const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = leases_.find(token.leaseId);
    if (it == leases_.end()) {
        return false;
    }

    if (!it->second.isValid) {
        return false;
    }

    if (std::chrono::steady_clock::now() > it->second.expiresAt) {
        return false;
    }

    return it->second.agentId == token.agentId;
}

uint64_t AgentCoordinator::createCheckpoint(uint64_t taskId, uint32_t progressPercentage,
                                           const std::vector<uint8_t>& metadataBlob) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    uint64_t checkpointId = nextCheckpointId_++;

    Checkpoint checkpoint;
    checkpoint.checkpointId = checkpointId;
    checkpoint.taskId = taskId;
    checkpoint.progressPercentage = progressPercentage;
    checkpoint.createdAt = std::chrono::steady_clock::now();
    checkpoint.dataSize = metadataBlob.size();
    checkpoint.metadataBlob = metadataBlob;

    checkpoints_[taskId].push_back(checkpoint);

    return checkpointId;
}

Checkpoint AgentCoordinator::getLatestCheckpoint(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = checkpoints_.find(taskId);
    if (it != checkpoints_.end() && !it->second.empty()) {
        return it->second.back();
    }
    return Checkpoint();
}

bool AgentCoordinator::restoreFromCheckpoint(uint64_t checkpointId) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    for (auto& [taskId, checkpointList] : checkpoints_) {
        for (const auto& checkpoint : checkpointList) {
            if (checkpoint.checkpointId == checkpointId) {
                // Mark task as requiring restoration
                auto taskIt = tasks_.find(taskId);
                if (taskIt != tasks_.end()) {
                    taskIt->second.progressPercentage = checkpoint.progressPercentage;
                    return true;
                }
            }
        }
    }
    return false;
}

std::vector<uint64_t> AgentCoordinator::getCheckpoints(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    std::vector<uint64_t> result;
    auto it = checkpoints_.find(taskId);
    if (it != checkpoints_.end()) {
        for (const auto& checkpoint : it->second) {
            result.push_back(checkpoint.checkpointId);
        }
    }
    return result;
}

bool AgentCoordinator::addTaskDependency(uint64_t taskId, uint64_t dependsOnTaskId) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    if (tasks_.find(taskId) == tasks_.end() || tasks_.find(dependsOnTaskId) == tasks_.end()) {
        return false;
    }

    taskDependencies_[taskId].push_back(dependsOnTaskId);
    return true;
}

std::vector<uint64_t> AgentCoordinator::getDependencies(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = taskDependencies_.find(taskId);
    if (it != taskDependencies_.end()) {
        return it->second;
    }
    return {};
}

std::vector<uint64_t> AgentCoordinator::getDependents(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    std::vector<uint64_t> dependents;
    for (const auto& [depTaskId, dependencies] : taskDependencies_) {
        if (std::find(dependencies.begin(), dependencies.end(), taskId) != dependencies.end()) {
            dependents.push_back(depTaskId);
        }
    }
    return dependents;
}

bool AgentCoordinator::areAllDependenciesSatisfied(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto it = taskDependencies_.find(taskId);
    if (it == taskDependencies_.end()) {
        return true;  // No dependencies
    }

    for (uint64_t depTaskId : it->second) {
        auto depIt = tasks_.find(depTaskId);
        if (depIt == tasks_.end() || depIt->second.progressPercentage != 100) {
            return false;
        }
    }
    return true;
}

AgentCoordinator::CoordinationStats AgentCoordinator::getStatistics() const {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    CoordinationStats stats;
    stats.totalAgentsRegistered = agents_.size();

    uint32_t activeCount = 0;
    for (const auto& [_, agent] : agents_) {
        if (agent.state != AgentState::DEAD && agent.state != AgentState::UNINITIALIZED) {
            activeCount++;
        }
    }
    stats.activeAgents = activeCount;

    stats.totalTasksSubmitted = tasks_.size();
    uint64_t completedCount = 0;
    uint64_t failedCount = 0;

    for (const auto& [_, task] : tasks_) {
        if (task.progressPercentage == 100) {
            completedCount++;
        }
    }

    stats.completedTasks = completedCount;
    stats.activeTasks = tasks_.size() - completedCount;
    stats.totalConflictsDetected = conflicts_.size();

    uint64_t resolvedCount = 0;
    for (const auto& [_, conflict] : conflicts_) {
        if (conflict.resolved) {
            resolvedCount++;
        }
    }
    stats.resolvedConflicts = resolvedCount;

    uint32_t totalCheckpointSize = 0;
    for (const auto& [_, checkpointList] : checkpoints_) {
        for (const auto& checkpoint : checkpointList) {
            totalCheckpointSize += checkpoint.dataSize;
        }
    }
    stats.checkpointStorageUsedMB = totalCheckpointSize / (1024 * 1024);

    return stats;
}

void AgentCoordinator::prune_completed_tasks(std::chrono::hours olderThan) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    auto now = std::chrono::steady_clock::now();
    std::vector<uint64_t> tasksToRemove;

    for (const auto& [taskId, task] : tasks_) {
        if (task.progressPercentage == 100) {
            auto age = std::chrono::duration_cast<std::chrono::hours>(now - task.createdAt);
            if (age > olderThan) {
                tasksToRemove.push_back(taskId);
            }
        }
    }

    for (uint64_t taskId : tasksToRemove) {
        tasks_.erase(taskId);
        checkpoints_.erase(taskId);
        taskDependencies_.erase(taskId);
    }
}

bool AgentCoordinator::export_state(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    std::ofstream ofs(filepath, std::ios::binary | std::ios::trunc);
    if (!ofs.is_open()) {
        return false;
    }

    // JSON serialization of coordinator state
    std::ostringstream json;
    json << "{\n";

    // Serialize agents
    json << "  \"agents\": [\n";
    bool firstAgent = true;
    for (const auto& [id, rec] : agents_) {
        if (!firstAgent) json << ",\n";
        firstAgent = false;
        json << "    {\"id\":" << id
             << ",\"name\":\"" << rec.agentName << "\""
             << ",\"state\":" << static_cast<int>(rec.state)
             << ",\"tasksExecuted\":" << rec.tasksExecuted
             << ",\"priority\":" << rec.capabilities.priority
             << ",\"contextWindow\":" << rec.capabilities.contextWindowSize
             << "}";
    }
    json << "\n  ],\n";

    // Serialize tasks
    json << "  \"tasks\": [\n";
    bool firstTask = true;
    for (const auto& [id, task] : tasks_) {
        if (!firstTask) json << ",\n";
        firstTask = false;
        json << "    {\"id\":" << id
             << ",\"agentId\":" << task.agentId
             << ",\"name\":\"" << task.taskName << "\""
             << ",\"progress\":" << task.progressPercentage
             << ",\"checkpoints\":" << task.checkpointCount
             << ",\"isLongRunning\":" << (task.isLongRunning ? "true" : "false")
             << "}";
    }
    json << "\n  ],\n";

    // Serialize conflicts
    json << "  \"conflicts\": [\n";
    bool firstConflict = true;
    for (const auto& [id, conflict] : conflicts_) {
        if (!firstConflict) json << ",\n";
        firstConflict = false;
        json << "    {\"id\":" << id
             << ",\"resource\":\"" << conflict.resourceInContention << "\""
             << ",\"resolved\":" << (conflict.resolved ? "true" : "false")
             << ",\"attempts\":" << conflict.resolutionAttempts
             << "}";
    }
    json << "\n  ],\n";

    // Serialize counters
    json << "  \"nextAgentId\":" << nextAgentId_ << ",\n"
         << "  \"nextTaskId\":" << nextTaskId_ << ",\n"
         << "  \"nextConflictId\":" << nextConflictId_ << ",\n"
         << "  \"nextLeaseId\":" << nextLeaseId_ << ",\n"
         << "  \"nextCheckpointId\":" << nextCheckpointId_ << "\n";
    json << "}\n";

    ofs << json.str();
    return ofs.good();
}

bool AgentCoordinator::import_state(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    std::ifstream ifs(filepath, std::ios::binary);
    if (!ifs.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(ifs)),
                        std::istreambuf_iterator<char>());
    if (content.empty()) {
        return false;
    }

    // Simple JSON parser: extract counter values to restore state
    auto extractUint = [&](const std::string& key) -> uint64_t {
        std::string search = "\"" + key + "\":";
        auto pos = content.find(search);
        if (pos == std::string::npos) return 0;
        pos += search.length();
        while (pos < content.size() && content[pos] == ' ') pos++;
        std::string numStr;
        while (pos < content.size() && content[pos] >= '0' && content[pos] <= '9') {
            numStr += content[pos++];
        }
        return numStr.empty() ? 0 : std::stoull(numStr);
    };

    nextAgentId_ = static_cast<uint32_t>(extractUint("nextAgentId"));
    nextTaskId_ = extractUint("nextTaskId");
    nextConflictId_ = extractUint("nextConflictId");
    nextLeaseId_ = extractUint("nextLeaseId");
    nextCheckpointId_ = extractUint("nextCheckpointId");

    // Ensure minimum values
    if (nextAgentId_ == 0) nextAgentId_ = 1;
    if (nextTaskId_ == 0) nextTaskId_ = 1000;
    if (nextConflictId_ == 0) nextConflictId_ = 1;
    if (nextLeaseId_ == 0) nextLeaseId_ = 1;
    if (nextCheckpointId_ == 0) nextCheckpointId_ = 1;

    return true;
}

void AgentCoordinator::shutdown() {
    std::lock_guard<std::mutex> lock(coordinatorMutex_);

    heartbeatThreadRunning_ = false;
    conflictDetectorRunning_ = false;

    agents_.clear();
    tasks_.clear();
    leases_.clear();
    conflicts_.clear();
    checkpoints_.clear();
    taskDependencies_.clear();
}

void AgentCoordinator::heartbeat_monitor() {
    heartbeatThreadRunning_ = true;
    while (heartbeatThreadRunning_) {
        {
            std::lock_guard<std::mutex> lock(coordinatorMutex_);
            auto now = std::chrono::steady_clock::now();
            const auto staleThreshold = std::chrono::seconds(30);

            for (auto& [id, agent] : agents_) {
                if (agent.state == AgentState::DEAD ||
                    agent.state == AgentState::UNINITIALIZED) {
                    continue;
                }

                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                    now - agent.lastHeartbeat);

                if (elapsed > staleThreshold) {
                    // Agent has gone stale — mark as suspended
                    agent.state = AgentState::SUSPENDED;
                }
            }
        }

        // Sleep 5 seconds between heartbeat checks
        for (int i = 0; i < 50 && heartbeatThreadRunning_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

void AgentCoordinator::conflict_detector() {
    conflictDetectorRunning_ = true;
    while (conflictDetectorRunning_) {
        {
            std::lock_guard<std::mutex> lock(coordinatorMutex_);

            // Build map of resource → list of agents holding leases
            std::map<std::string, std::vector<uint32_t>> resourceAgents;
            auto now = std::chrono::steady_clock::now();

            for (const auto& [id, lease] : leases_) {
                if (!lease.isValid) continue;
                if (now > lease.expiresAt) continue; // expired
                resourceAgents[lease.resourcePath].push_back(lease.agentId);
            }

            // Detect multiple agents competing for same resource
            for (const auto& [resource, agentIds] : resourceAgents) {
                if (agentIds.size() > 1) {
                    // Check if this conflict is already tracked
                    bool alreadyTracked = false;
                    for (const auto& [cid, conflict] : conflicts_) {
                        if (!conflict.resolved &&
                            conflict.resourceInContention == resource) {
                            alreadyTracked = true;
                            break;
                        }
                    }

                    if (!alreadyTracked) {
                        // Create new conflict record
                        ConflictRecord newConflict;
                        newConflict.conflictId = nextConflictId_++;
                        newConflict.conflictingAgents.assign(agentIds.begin(), agentIds.end());
                        newConflict.resourceInContention = resource;
                        newConflict.suggestedStrategy = ConflictStrategy::PRIORITY_BASED;
                        newConflict.detectedAt = now;
                        newConflict.resolved = false;
                        newConflict.resolutionAttempts = 0;

                        conflicts_[newConflict.conflictId] = newConflict;
                    }
                }
            }
        }

        // Sleep 2 seconds between conflict scans
        for (int i = 0; i < 20 && conflictDetectorRunning_; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}

}  // namespace RawrXD::Agentic::Coordination

