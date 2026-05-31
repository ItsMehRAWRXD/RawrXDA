#pragma once
/**
 * AsyncPersistenceQueue - Enhancement #4: Non-Blocking Persistence
 * 
 * Queues persistence operations for background execution.
 * Prevents workflow stalls during I/O operations.
 * 
 * Symbols: APQ_PRIORITY_HIGH, APQ_PRIORITY_NORMAL, APQ_PRIORITY_LOW
 */

#include <string>
#include <functional>
#include <future>
#include <memory>
#include <nlohmann/json.hpp>

// Priority levels
#define APQ_PRIORITY_CRITICAL   0  // Immediate sync execution
#define APQ_PRIORITY_HIGH       1  // Process before normal
#define APQ_PRIORITY_NORMAL     2  // Default priority
#define APQ_PRIORITY_LOW        3  // Background/batch
#define APQ_PRIORITY_BACKGROUND 4  // Idle-time processing

// Queue configuration
#define APQ_DEFAULT_WORKERS       2
#define APQ_MAX_QUEUE_DEPTH       1000
#define APQ_BATCH_INTERVAL_MS     100

namespace AsyncPersistenceQueue {

    // Operation result callback
    using PersistenceCallback = std::function<void(bool success, const std::string& error)>;

    // Operation type
    enum class OperationType {
        PersistExecution,
        CreateCheckpoint,
        UpdateCheckpoint,
        DeleteExecution,
        CleanupOld,
        CompressState,
        EncryptState
    };

    /**
     * Persistence operation descriptor
     */
    struct Operation {
        std::string operationId;
        OperationType type;
        int priority = APQ_PRIORITY_NORMAL;
        std::string executionId;
        nlohmann::json payload;
        PersistenceCallback callback;
        std::chrono::steady_clock::time_point enqueueTime;
        std::chrono::steady_clock::time_point deadline;
        bool requiresConfirmation = false;
    };

    /**
     * Async persistence queue
     */
    class PersistenceQueue {
    public:
        PersistenceQueue();
        ~PersistenceQueue();

        // Initialize with worker threads
        bool initialize(size_t numWorkers = APQ_DEFAULT_WORKERS);
        void shutdown();

        // Enqueue operations
        std::string enqueue(Operation& op);
        std::string persistExecution(
            const std::string& executionId,
            const nlohmann::json& state,
            int priority = APQ_PRIORITY_NORMAL);
        std::string createCheckpoint(
            const std::string& executionId,
            const std::string& label,
            const nlohmann::json& state,
            int priority = APQ_PRIORITY_NORMAL);

        // Wait for specific operation
        bool waitFor(const std::string& operationId, int timeoutMs = 5000);
        
        // Wait for all pending operations
        bool waitForAll(int timeoutMs = 30000);

        // Cancel pending operation
        bool cancel(const std::string& operationId);

        // Get queue status
        struct Status {
            size_t pendingCount = 0;
            size_t inProgressCount = 0;
            size_t completedCount = 0;
            size_t failedCount = 0;
            size_t cancelledCount = 0;
            double avgLatencyMs = 0.0;
            double maxLatencyMs = 0.0;
        };
        Status getStatus() const;

        // Configure batching
        void setBatchingEnabled(bool enabled);
        void setBatchInterval(int intervalMs);

        // Emergency flush - sync all pending
        void emergencyFlush();

        // Pause/resume processing
        void pause();
        void resume();

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Write-behind cache for frequently updated states
     */
    class WriteBehindCache {
    public:
        WriteBehindCache();
        ~WriteBehindCache();

        // Stage update (returns immediately)
        void stageUpdate(
            const std::string& executionId,
            const nlohmann::json& delta);

        // Flush specific execution
        bool flush(const std::string& executionId);

        // Flush all staged updates
        void flushAll();

        // Get staged but unwritten state
        nlohmann::json getStagedState(const std::string& executionId) const;

        // Configure
        void setFlushInterval(int intervalMs);
        void setMaxStagedEntries(size_t maxEntries);

    private:
        class Impl;
        std::unique_ptr<Impl> m_impl;
    };

    /**
     * Global queue instance
     */
    PersistenceQueue& getGlobalQueue();
    WriteBehindCache& getGlobalCache();

    // Initialize global queue
    bool initializeGlobal(size_t numWorkers = APQ_DEFAULT_WORKERS);
    void shutdownGlobal();

} // namespace AsyncPersistenceQueue
