// ============================================================================
// scheduler_completion_integration.h — CompletionEngine as Scheduled Phase
// ============================================================================
// Binds CompletionEngine into ExecutionScheduler DAG as a scheduled compute
// phase rather than a "floating" UI feature.
//
// Key Changes:
//   - Completion requests become DAG tasks
//   - AST context is pre-fetched (like tensor prefetch)
//   - Results cached with structural fingerprints
//   - Backpressure-aware (drops stale requests)
// ============================================================================

#pragma once

#include "execution_scheduler.h"
#include "ast_graph_engine.h"
#include <functional>
#include <memory>
#include <queue>
#include <atomic>

namespace RawrXD {

// Forward declarations
class CompletionEngine;

// ============================================================================
// Completion Task — DAG node for completion requests
// ============================================================================
struct CompletionTask {
    uint64_t                taskID;
    uint32_t                fileID;
    uint32_t                line;
    uint32_t                column;
    std::string             partialSymbol;
    AST::ContextFingerprint fingerprint;
    
    // Timing
    uint64_t                enqueueTime;
    uint64_t                startTime;
    uint64_t                completeTime;
    
    // Result
    std::vector<std::string> completions;
    bool                    completed;
    bool                    cancelled;
    
    CompletionTask() : taskID(0), fileID(0), line(0), column(0), 
                       enqueueTime(0), startTime(0), completeTime(0),
                       completed(false), cancelled(false) {}
};

// ============================================================================
// Completion Phase — Scheduled compute phase for completions
// ============================================================================
enum class CompletionPhase : uint8_t {
    IDLE = 0,
    PREFETCH_CONTEXT,       // Phase 1: Pre-fetch AST context (like tensor prefetch)
    BUILD_FINGERPRINT,      // Phase 2: Build context fingerprint
    CACHE_LOOKUP,           // Phase 3: Check completion cache
    MODEL_INFERENCE,        // Phase 4: Run model inference (if cache miss)
    POSTPROCESS,            // Phase 5: Filter/rank results
    COMPLETE
};

// ============================================================================
// Completion Scheduler — DAG-integrated completion engine
// ============================================================================
class CompletionScheduler {
public:
    CompletionScheduler();
    ~CompletionScheduler();

    // ---- Lifecycle ----
    bool initialize(ExecutionScheduler* execScheduler, AST::ASTGraphEngine* astEngine);
    void shutdown();
    
    // ---- Request Submission ----
    // Submit a completion request (returns task ID, 0 if rejected due to backpressure)
    uint64_t requestCompletion(uint32_t fileID, uint32_t line, uint32_t column,
                                const std::string& partialSymbol);
    
    // Cancel a pending completion request
    void cancelCompletion(uint64_t taskID);
    
    // Check if completion is ready (non-blocking)
    bool isComplete(uint64_t taskID) const;
    
    // Get completion results (blocking until ready or timeout)
    std::vector<std::string> awaitCompletion(uint64_t taskID, uint64_t timeoutMs);
    
    // ---- DAG Integration ----
    // Run one completion phase (called by ExecutionScheduler)
    // Returns true if work was done
    bool runCompletionPhase(CompletionPhase phase);
    
    // Get current phase
    CompletionPhase getCurrentPhase() const { return m_currentPhase.load(); }
    
    // ---- Configuration ----
    struct Config {
        uint32_t    maxConcurrentTasks = 4;         // Max parallel completions
        uint32_t    maxQueueDepth = 16;             // Backpressure threshold
        uint64_t    taskTimeoutMs = 5000;           // Max time per completion
        bool        enablePrefetch = true;          // Pre-fetch AST context
        bool        enableCache = true;             // Use completion cache
        bool        enableBackpressure = true;      // Drop stale requests
        uint32_t    prefetchAhead = 2;              // Lines ahead to prefetch
    };
    void configure(const Config& config);
    Config getConfig() const;
    
    // ---- Statistics ----
    struct Stats {
        uint64_t    tasksSubmitted;
        uint64_t    tasksCompleted;
        uint64_t    tasksCancelled;
        uint64_t    tasksDropped;       // Due to backpressure
        uint64_t    cacheHits;
        uint64_t    cacheMisses;
        double      avgLatencyMs;
        double      p99LatencyMs;
    };
    Stats getStats() const;
    void resetStats();
    
    // ---- Backpressure ----
    bool isUnderBackpressure() const;
    uint32_t getQueueDepth() const;
    
private:
    // ---- State ----
    ExecutionScheduler*     m_execScheduler;
    AST::ASTGraphEngine*    m_astEngine;
    CompletionEngine*       m_completionEngine;
    Config                    m_config;
    
    // Task queue (priority: cursor position proximity)
    std::priority_queue<std::pair<uint64_t, uint64_t>, 
                        std::vector<std::pair<uint64_t, uint64_t>>,
                        std::greater<>> m_taskQueue; // <priority, taskID>
    std::unordered_map<uint64_t, std::unique_ptr<CompletionTask>> m_tasks;
    mutable std::mutex      m_taskMutex;
    
    // Active tasks
    std::atomic<uint32_t> m_activeTasks{0};
    std::atomic<uint64_t> m_nextTaskID{1};
    
    // Phase tracking
    std::atomic<CompletionPhase> m_currentPhase{CompletionPhase::IDLE};
    
    // Statistics
    mutable std::atomic<uint64_t> m_tasksSubmitted{0};
    mutable std::atomic<uint64_t> m_tasksCompleted{0};
    mutable std::atomic<uint64_t> m_tasksCancelled{0};
    mutable std::atomic<uint64_t> m_tasksDropped{0};
    mutable std::atomic<uint64_t> m_cacheHits{0};
    mutable std::atomic<uint64_t> m_cacheMisses{0};
    
    // ---- Internal Methods ----
    void processTaskQueue();
    bool executeTaskPhase(CompletionTask* task, CompletionPhase phase);
    void completeTask(CompletionTask* task);
    void dropStaleTasks();
    uint64_t calculatePriority(const CompletionTask* task) const;
};

// ============================================================================
// ExecutionScheduler Extension — Adds completion phases to scheduler
// ============================================================================
class ExecutionSchedulerWithCompletion : public ExecutionScheduler {
public:
    ExecutionSchedulerWithCompletion();
    ~ExecutionSchedulerWithCompletion();
    
    // Initialize with completion support
    bool initializeWithCompletion(CompletionEngine* completionEngine,
                                   AST::ASTGraphEngine* astEngine);
    
    // Override runForwardPass to include completion phases
    bool runForwardPassWithCompletion(float* state, float* scratch, int seqPos,
                                       const std::vector<CompletionTask>& pendingCompletions);
    
    // Get completion scheduler
    CompletionScheduler* getCompletionScheduler() { return &m_completionScheduler; }
    
private:
    CompletionScheduler m_completionScheduler;
};

// ============================================================================
// Global Access
// ============================================================================
CompletionScheduler& getCompletionScheduler();

} // namespace RawrXD
