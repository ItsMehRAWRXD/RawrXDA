#pragma once
/**
 * ExecutionStatePersistence - Days 1-2: Workflow state serialization/deserialization
 * 
 * Production-grade persistence layer for agent execution state.
 * Enables workflows to survive restarts and resume from checkpoints.
 */

#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <nlohmann/json.hpp>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#endif

class AgenticLoopState;
class AgenticMemorySystem;
class AgenticExecutor;

/**
 * @class ExecutionCheckpoint
 * @brief Single checkpoint within a workflow (progress marker)
 */
class ExecutionCheckpoint {
public:
    std::string checkpointId;
    std::string label;
    int sequenceNumber = 0;
    std::string timestamp;
    nlohmann::json stateSnapshot;
    bool isRecoveryPoint = false;
    std::string recoveryReason;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

/**
 * @class WorkflowExecution
 * @brief Complete workflow execution state (all checkpoints + metadata)
 */
class WorkflowExecution {
public:
    std::string executionId;
    std::string workflowName;
    std::string goal;
    std::string status;  // "in-progress", "completed", "failed", "paused"
    std::string startTime;
    std::string lastUpdateTime;
    std::string completionTime;
    
    int currentCheckpointIndex = 0;
    std::vector<ExecutionCheckpoint> checkpoints;
    nlohmann::json globalContext;
    nlohmann::json metadata;
    std::vector<std::string> errorLog;
    
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

/**
 * @class ExecutionStatePersistence
 * @brief Production execution state serialization/deserialization engine
 * 
 * Guarantees:
 * - Safe JSON serialization of all state artifacts
 * - Atomic write operations (temp + move pattern)
 * - Corruption detection and rollback capability
 * - Automatic compression for large states
 * - Memory-efficient incremental snapshots
 */
class ExecutionStatePersistence {
public:
    explicit ExecutionStatePersistence(const std::filesystem::path& persistenceRoot);
    ~ExecutionStatePersistence();

    // ===== Persistence API =====
    
    /**
     * Save complete workflow execution state to disk
     * @param execution Workflow execution to persist
     * @return execution ID if successful, empty string on failure
     */
    std::string persistWorkflowExecution(const WorkflowExecution& execution);

    /**
     * Load workflow execution state from disk
     * @param executionId ID of execution to restore
     * @return loaded execution or nullptr if not found
     */
    std::unique_ptr<WorkflowExecution> loadWorkflowExecution(const std::string& executionId) const;

    /**
     * Save checkpoint within active workflow
     * @param checkpoint Checkpoint to persist
     * @param executionId Parent execution ID
     * @return checkpoint ID if successful
     */
    std::string createCheckpoint(
        const std::string& executionId,
        const std::string& label,
        const nlohmann::json& stateSnapshot);

    // ===== Workflow Recovery =====

    /**
     * List all persisted workflow executions
     * @return vector of execution IDs
     */
    std::vector<std::string> listExecutions() const;

    /**
     * Check if execution exists and is valid
     */
    bool hasValidExecution(const std::string& executionId) const;

    /**
     * Get the last incomplete execution (for resumption)
     */
    std::unique_ptr<WorkflowExecution> getLastIncompleteExecution();

    /**
     * Resume execution from checkpoint
     * @param executionId Execution to resume
     * @param checkpointIndex Which checkpoint to resume from (default: last)
     * @return resumed execution state
     */
    std::unique_ptr<WorkflowExecution> resumeFromCheckpoint(
        const std::string& executionId,
        int checkpointIndex = -1);

    /**
     * Restore persisted subsystem state from a workflow execution envelope.
     */
    bool restoreExecutionState(
        const WorkflowExecution& execution,
        AgenticLoopState* loopState,
        AgenticMemorySystem* memorySystem) const;

    /**
     * Restore persisted subsystem state from a checkpoint snapshot.
     */
    bool restoreCheckpointState(
        const ExecutionCheckpoint& checkpoint,
        AgenticLoopState* loopState,
        AgenticMemorySystem* memorySystem) const;

    // ===== State Capture from Live Systems =====

    /**
     * Capture current agent state into workflow execution
     * Snapshots loop state, memory system, and executor context
     */
    WorkflowExecution captureCurrentExecution(
        const std::string& workflowName,
        const std::string& goal,
        AgenticLoopState* loopState,
        AgenticMemorySystem* memorySystem,
        AgenticExecutor* executor);

    // ===== Error Recovery =====

    /**
     * Validate persisted state for corruption
     * @return error message if corrupted, empty string if valid
     */
    std::string validateExecution(const std::string& executionId) const;

    /**
     * Recover from corrupt state (rollback to last valid checkpoint)
     */
    bool recoverFromCorruption(const std::string& executionId);

    /**
     * Remove persisted execution (cleanup)
     */
    bool deleteExecution(const std::string& executionId);

    // ===== Statistics =====

    struct PersistenceStats {
        size_t totalExecutions = 0;
        size_t activeExecutions = 0;
        size_t completedExecutions = 0;
        size_t failedExecutions = 0;
        size_t totalCheckpoints = 0;
        size_t diskUsageBytes = 0;
    };

    PersistenceStats getStatistics() const;

    /**
     * Cleanup old executions (retention policy)
     * @param maxAgeHours Delete executions older than this (0 = no limit)
     * @return number of deleted executions
     */
    int cleanupOldExecutions(int maxAgeHours = 0);

    // ===== Enhancement 1: Checkpoint Compression =====
    
    /**
     * Compression level for state snapshots (0-9, 0=none, 9=max)
     */
    void setCompressionLevel(int level);
    int getCompressionLevel() const { return m_compressionLevel; }
    
    /**
     * Compress state snapshot for storage
     */
    std::string compressState(const std::string& jsonState) const;
    std::string decompressState(const std::string& compressedState) const;
    
    // ===== Enhancement 2: Incremental State Diffing =====
    
    /**
     * Create checkpoint with incremental diff from previous
     * Stores only changed fields, dramatically reducing size
     */
    std::string createIncrementalCheckpoint(
        const std::string& executionId,
        const std::string& label,
        const nlohmann::json& currentState);
    
    /**
     * Reconstruct full state from base + incremental diffs
     */
    nlohmann::json reconstructState(
        const WorkflowExecution& execution,
        int checkpointIndex) const;
    
    // ===== Enhancement 3: Memory-Mapped Persistence =====
    
    /**
     * Enable memory-mapped I/O for hot path access
     * Provides zero-latency reads for active executions
     */
    void enableMemoryMapping(bool enable);
    bool isMemoryMappingEnabled() const { return m_memoryMappingEnabled; }
    
    /**
     * Get memory-mapped view of execution state (read-only)
     * Returns nullptr if not available
     */
    const char* getMemoryMappedView(const std::string& executionId);
    
    // ===== Enhancement 4: Semantic Memory Index =====
    
    /**
     * Build semantic index for memory entries
     * Enables fast content-based retrieval
     */
    void buildMemoryIndex(const std::string& executionId);
    
    /**
     * Search memories by semantic similarity
     */
    std::vector<std::string> searchMemories(
        const std::string& executionId,
        const std::string& query,
        int maxResults = 10);
    
    // ===== Enhancement 5: Priority-Based Checkpoint Pruning =====
    
    /**
     * Set checkpoint retention policy
     * @param maxCheckpoints Maximum checkpoints to keep (0 = unlimited)
     * @param minIntervalMs Minimum time between checkpoints
     */
    void setCheckpointPolicy(size_t maxCheckpoints, int64_t minIntervalMs);
    
    /**
     * Prune checkpoints based on priority and age
     * Keeps recovery points and recent checkpoints
     */
    int pruneCheckpoints(const std::string& executionId);
    
    // ===== Enhancement 6: Cross-Session Execution Resumption =====
    
    /**
     * Save session metadata for cross-session resumption
     */
    bool saveSessionMetadata(
        const std::string& sessionId,
        const std::vector<std::string>& activeExecutions);
    
    /**
     * Resume all executions from previous session
     */
    std::vector<std::unique_ptr<WorkflowExecution>> resumeSession(
        const std::string& sessionId);
    
    /**
     * List all resumable sessions
     */
    std::vector<std::string> listResumableSessions() const;
    
    // ===== Enhancement 7: Checkpoint Integrity Verification =====
    
    /**
     * Compute SHA-256 hash of checkpoint for integrity
     */
    std::string computeCheckpointHash(const ExecutionCheckpoint& checkpoint) const;
    
    /**
     * Verify checkpoint integrity against stored hash
     */
    bool verifyCheckpointIntegrity(const ExecutionCheckpoint& checkpoint) const;
    
    /**
     * Sign checkpoint with execution-level integrity chain
     */
    void signCheckpoint(ExecutionCheckpoint& checkpoint, const std::string& prevHash) const;
    
    // ===== Enhancement 8: Async Persistence with WAL =====
    
    /**
     * Enable async persistence with write-ahead logging
     * Provides durability without blocking execution
     */
    void enableAsyncPersistence(bool enable);
    bool isAsyncPersistenceEnabled() const { return m_asyncPersistenceEnabled; }
    
    /**
     * Flush pending async writes (call before critical operations)
     */
    void flushAsyncWrites();
    
    /**
     * Get WAL statistics
     */
    struct WalStats {
        size_t pendingWrites = 0;
        size_t committedWrites = 0;
        size_t failedWrites = 0;
        size_t walSizeBytes = 0;
    };
    WalStats getWalStats() const;

private:
    std::filesystem::path m_persistenceRoot;
    std::string generateExecutionId();
    std::string getExecutionPath(const std::string& executionId) const;
    bool ensurePersistenceRoot();
    
    // Atomic write with rollback pattern
    bool atomicWrite(
        const std::filesystem::path& target,
        const std::string& content) const;

    // Validation and corruption detection
    bool validateJsonSchema(const nlohmann::json& j) const;
    int findResumableCheckpointIndex(const WorkflowExecution& execution, int requestedIndex) const;
    bool restoreStatePayload(
        const nlohmann::json& payload,
        AgenticLoopState* loopState,
        AgenticMemorySystem* memorySystem) const;
    
    // Enhancement 1: Compression
    int m_compressionLevel = 6; // Default: balanced compression
    
    // Enhancement 2: Incremental diffing
    struct DiffState {
        nlohmann::json baseState;
        std::vector<nlohmann::json> diffs;
    };
    mutable std::unordered_map<std::string, DiffState> m_diffCache;
    nlohmann::json computeStateDiff(const nlohmann::json& base, const nlohmann::json& current) const;
    nlohmann::json applyStateDiff(const nlohmann::json& base, const nlohmann::json& diff) const;
    
    // Enhancement 3: Memory mapping
    bool m_memoryMappingEnabled = false;
    mutable std::unordered_map<std::string, std::pair<const char*, size_t>> m_memoryMaps;
    bool mapExecutionToMemory(const std::string& executionId);
    void unmapExecution(const std::string& executionId);
    
    // Enhancement 4: Semantic index
    struct MemoryIndex {
        std::unordered_map<std::string, std::vector<std::string>> keywordIndex;
        std::chrono::system_clock::time_point lastUpdated;
    };
    mutable std::unordered_map<std::string, MemoryIndex> m_memoryIndices;
    void updateMemoryIndex(const std::string& executionId, const nlohmann::json& memoryState);
    
    // Enhancement 5: Checkpoint policy
    size_t m_maxCheckpoints = 50;
    int64_t m_minCheckpointIntervalMs = 1000;
    mutable std::unordered_map<std::string, int64_t> m_lastCheckpointTime;
    
    // Enhancement 6: Session management
    std::string getSessionPath(const std::string& sessionId) const;
    
    // Enhancement 7: Integrity
    std::string computeHash(const std::string& data) const;
    
    // Enhancement 8: Async WAL
    bool m_asyncPersistenceEnabled = false;
    struct WalEntry {
        std::string executionId;
        std::string operation;
        nlohmann::json payload;
        int64_t timestamp;
        bool committed = false;
    };
    mutable std::mutex m_walMutex;
    mutable std::vector<WalEntry> m_walQueue;
    mutable std::thread m_walThread;
    mutable std::atomic<bool> m_walShutdown{false};
    mutable std::atomic<size_t> m_walCommitted{0};
    mutable std::atomic<size_t> m_walFailed{0};
    void walWorkerThread();
    void enqueueWalWrite(const WalEntry& entry);
    std::filesystem::path getWalPath() const;
    void replayWal();
};
