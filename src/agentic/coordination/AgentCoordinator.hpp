#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

namespace RawrXD::Agentic::Coordination {

// Agent state machine
enum class AgentState : uint8_t {
    UNINITIALIZED = 0,
    IDLE = 1,
    PLANNING = 2,
    EXECUTING = 3,
    CONFLICTED = 4,
    SUSPENDED = 5,
    DEAD = 6
};

// Conflict resolution strategies
enum class ConflictStrategy : uint8_t {
    PRIORITY_BASED = 0,      // Higher priority wins
    VOTING = 1,              // Majority consensus
    SEQUENTIAL = 2,          // Serialize execution
    ROLLBACK = 3,            // Revert conflicting changes
    MERGE = 4                // Attempt automatic merge
};

// Agent capabilities & resource requirements
struct AgentCapabilities {
    uint64_t requiredMemoryMB = 512;
    uint32_t requiredCores = 1;
    uint32_t requiredGPUMemoryMB = 0;
    std::vector<std::string> requiredLibraries;
    std::vector<std::string> supportedQuantizations;
    uint32_t contextWindowSize = 4096;
    uint32_t priority = 50;  // 0-100, higher = more important
};

// Task metadata for long-running operations
struct TaskMetadata {
    uint64_t taskId = 0;
    uint32_t agentId = 0;
    std::string taskName;
    std::string description;
    AgentCapabilities requirements;
    std::chrono::steady_clock::time_point createdAt;
    std::chrono::steady_clock::time_point estimatedCompletion;
    uint32_t progressPercentage = 0;
    uint32_t checkpointCount = 0;
    std::vector<uint64_t> dependentTaskIds;  // Tasks that depend on this one
    bool isLongRunning = false;
    uint32_t maxDurationSeconds = 3600;  // Default 1 hour
};

// Conflict record for resolution
struct ConflictRecord {
    uint64_t conflictId = 0;
    std::vector<uint32_t> conflictingAgents;
    std::vector<uint64_t> conflictingTaskIds;
    std::string resourceInContention;
    ConflictStrategy suggestedStrategy;
    std::chrono::steady_clock::time_point detectedAt;
    bool resolved = false;
    uint32_t resolutionAttempts = 0;
};

// Lease token for resource locks
struct LeaseToken {
    uint64_t leaseId = 0;
    uint32_t agentId = 0;
    std::string resourcePath;
    std::chrono::steady_clock::time_point acquiredAt;
    std::chrono::steady_clock::time_point expiresAt;
    uint32_t renewalCount = 0;
    bool isValid = false;
};

// Checkpoint for task resumption
struct Checkpoint {
    uint64_t checkpointId = 0;
    uint64_t taskId = 0;
    uint32_t progressPercentage = 0;
    std::chrono::steady_clock::time_point createdAt;
    std::string serializationFormat;  // "JSON", "MSGPACK", "PROTOBUF"
    uint64_t dataSize = 0;
    std::string dataHash;  // SHA256
    std::vector<uint8_t> metadataBlob;
};

class AgentCoordinator {
public:
    static AgentCoordinator& instance();

    // Lifecycle management
    uint32_t registerAgent(const std::string& agentName, const AgentCapabilities& capabilities);
    void unregisterAgent(uint32_t agentId);
    AgentState getAgentState(uint32_t agentId) const;
    void setAgentState(uint32_t agentId, AgentState newState);

    // Task management
    uint64_t submitTask(uint32_t agentId, const TaskMetadata& metadata);
    void cancelTask(uint64_t taskId);
    bool updateTaskProgress(uint64_t taskId, uint32_t progressPercentage);
    TaskMetadata getTaskMetadata(uint64_t taskId) const;

    // Conflict detection & resolution
    uint64_t detectConflict(const std::vector<uint32_t>& conflictingAgents,
                           const std::vector<uint64_t>& conflictingTasks,
                           const std::string& resource);
    ConflictRecord getConflictRecord(uint64_t conflictId) const;
    bool resolveConflict(uint64_t conflictId, ConflictStrategy strategy, uint32_t winner);
    std::vector<uint64_t> getActiveConflicts() const;

    // Resource locking with lease-based semantics
    LeaseToken acquireLease(uint32_t agentId, const std::string& resourcePath,
                           std::chrono::milliseconds leaseDuration);
    bool renewLease(const LeaseToken& token, std::chrono::milliseconds additionalDuration);
    void releaseLease(const LeaseToken& token);
    bool validateLease(const LeaseToken& token) const;

    // Checkpoint management for long-running tasks
    uint64_t createCheckpoint(uint64_t taskId, uint32_t progressPercentage,
                             const std::vector<uint8_t>& metadataBlob);
    Checkpoint getLatestCheckpoint(uint64_t taskId) const;
    bool restoreFromCheckpoint(uint64_t checkpointId);
    std::vector<uint64_t> getCheckpoints(uint64_t taskId) const;

    // Dependency management
    bool addTaskDependency(uint64_t taskId, uint64_t dependsOnTaskId);
    std::vector<uint64_t> getDependencies(uint64_t taskId) const;
    std::vector<uint64_t> getDependents(uint64_t taskId) const;
    bool areAllDependenciesSatisfied(uint64_t taskId) const;

    // Monitoring & statistics
    struct CoordinationStats {
        uint32_t totalAgentsRegistered = 0;
        uint32_t activeAgents = 0;
        uint64_t totalTasksSubmitted = 0;
        uint64_t activeTasks = 0;
        uint64_t completedTasks = 0;
        uint64_t failedTasks = 0;
        uint64_t totalConflictsDetected = 0;
        uint64_t resolvedConflicts = 0;
        std::chrono::milliseconds averageTaskDuration;
        uint32_t checkpointStorageUsedMB = 0;
    };
    CoordinationStats getStatistics() const;

    // Cleanup & persistence
    void prune_completed_tasks(std::chrono::hours olderThan);
    bool export_state(const std::string& filepath);
    bool import_state(const std::string& filepath);
    void shutdown();

    // Expose priority for ConflictResolver
    uint32_t getAgentPriority(uint32_t agentId) const;

private:
    AgentCoordinator();
    ~AgentCoordinator();
    AgentCoordinator(const AgentCoordinator&) = delete;
    AgentCoordinator& operator=(const AgentCoordinator&) = delete;

    // Internal state management
    struct AgentRecord {
        uint32_t agentId;
        std::string agentName;
        AgentCapabilities capabilities;
        AgentState state;
        std::chrono::steady_clock::time_point lastHeartbeat;
        uint64_t tasksExecuted = 0;
    };

    mutable std::mutex coordinatorMutex_;
    std::map<uint32_t, AgentRecord> agents_;
    std::map<uint64_t, TaskMetadata> tasks_;
    std::map<uint64_t, LeaseToken> leases_;
    std::map<uint64_t, ConflictRecord> conflicts_;
    std::map<uint64_t, std::vector<Checkpoint>> checkpoints_;
    std::map<uint64_t, std::vector<uint64_t>> taskDependencies_;

    uint32_t nextAgentId_ = 1;
    uint64_t nextTaskId_ = 1000;
    uint64_t nextConflictId_ = 1;
    uint64_t nextLeaseId_ = 1;
    uint64_t nextCheckpointId_ = 1;

    // Heartbeat mechanism for detecting stalled agents
    std::atomic<bool> heartbeatThreadRunning_{false};
    void heartbeat_monitor();

    // Conflict detector background thread
    std::atomic<bool> conflictDetectorRunning_{false};
    void conflict_detector();
};

} // namespace RawrXD::Agentic::Coordination
