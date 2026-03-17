// ============================================================================
// autonomous_orchestrator.hpp — Ultimate Autonomous Agentic Orchestrator
// ============================================================================
// Architecture: C++20, Win32, no exceptions, no Qt
// 
// The most advanced autonomous orchestration system for RawrXD IDE:
// - Automatic codebase audit and todo generation
// - Autonomous execution with self-adjusting parameters
// - Multi-agent cycling (1x-99x agents)
// - Quality modes: Auto, Balance, Max
// - Self-adjusting time limits for PowerShell terminals
// - Complex iteration tracking and analysis
// - Production-ready with quantum-level optimization
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <memory>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// Forward declarations
class AgenticDeepThinkingEngine;
class MetaPlanner;
class AutonomousSubAgent;
class AgenticFailureDetector;
class AgenticPuppeteer;

namespace RawrXD {

// ============================================================================
// QualityMode — Auto, Balance, or Max quality settings
// ============================================================================
enum class QualityMode {
    Auto = 0,      ///< Automatically balance quality and speed based on context
    Balance = 1,   ///< Balanced quality and performance
    Max = 2        ///< Maximum quality, no speed constraints
};

// ============================================================================
// ExecutionMode — How the orchestrator executes tasks
// ============================================================================
enum class ExecutionMode {
    Sequential = 0,     ///< Execute one task at a time
    Parallel = 1,       ///< Execute independent tasks in parallel  
    Adaptive = 2        ///< Automatically adapt based on system resources
};

// ============================================================================
// TodoItem — A single auditable task
// ============================================================================
struct TodoItem {
    uint64_t id = 0;
    std::string title;
    std::string description;
    std::string category;           ///< bug, optimization, enhancement, refactor, etc.
    std::string targetFile;         ///< Primary file this todo affects
    std::vector<std::string> relatedFiles;
    int priority = 5;               ///< 1 (highest) - 10 (lowest)
    int complexity = 5;             ///< 1 (trivial) - 10 (extremely complex)
    int estimatedIterations = 1;    ///< How many agent iterations expected
    int estimatedTimeSeconds = 60;  ///< Estimated time to complete

    enum class Status {
        Pending, Queued, InProgress, Completed, Failed, Skipped, Blocked
    } status = Status::Pending;

    std::string result;             ///< Completion result or error message
    int actualIterations = 0;       ///< Actual iterations used
    int actualTimeMs = 0;           ///< Actual time taken
    float confidence = 0.0f;        ///< Confidence in the solution (0.0-1.0)

    std::vector<uint64_t> dependencies;  ///< IDs of todos that must complete first
    std::vector<uint64_t> blockedBy;     ///< IDs of todos blocking this one

    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;

    std::string statusString() const;
    json toJSON() const;
    static TodoItem fromJSON(const json& j);
};

// ============================================================================
// AuditResult — Result of codebase audit
// ============================================================================
struct AuditResult {
    std::vector<TodoItem> todos;
    int totalFiles = 0;
    int filesWithIssues = 0;
    int totalLines = 0;
    int criticalIssues = 0;
    int warnings = 0;
    int suggestions = 0;
    std::unordered_map<std::string, int> issuesByCategory;
    std::vector<std::string> productionReadinessGaps;
    json fullReport;

    /// Get top N most important todos
    std::vector<TodoItem> getTopPriorityTodos(int n) const;
    
    /// Get top N most difficult todos
    std::vector<TodoItem> getMostDifficultTodos(int n) const;

    json toJSON() const;
};

// ============================================================================
// TerminalLimits — Self-adjusting PowerShell terminal time limits
// ============================================================================
struct TerminalLimits {
    int minTimeoutMs = 1000;        ///< Minimum timeout (1 second)
    int maxTimeoutMs = 3600000;     ///< Maximum timeout (1 hour)
    int currentTimeoutMs = 30000;   ///< Current timeout (30 seconds default)
    
    bool autoAdjust = true;         ///< Automatically adjust based on task
    bool randomize = false;         ///< Add random variation to avoid patterns
    int randomVariancePercent = 10; ///< +/- variance if randomize enabled

    enum class AdjustmentStrategy {
        Fixed,          ///< Never adjust
        Linear,         ///< Linear adjustment based on complexity
        Exponential,    ///< Exponential adjustment for complex tasks
        Adaptive        ///< Learn from past executions
    } strategy = AdjustmentStrategy::Adaptive;

    /// Adjust timeout based on task complexity and history
    int getTimeoutForTask(const TodoItem& task, const std::vector<TodoItem>& history);

    /// Get a randomized timeout within variance
    int getRandomizedTimeout() const;

    json toJSON() const;
    static TerminalLimits fromJSON(const json& j);
};

// ============================================================================
// AgentCycleConfig — Multi-agent cycling configuration (1x-99x)
// ============================================================================
struct AgentCycleConfig {
    int cycleMultiplier = 1;        ///< 1x-99x multiplier for agent cycles
    int agentCount = 1;             ///< Number of parallel agents (1-99)
    bool enableDebate = false;      ///< Enable agent debate/critique
    bool enableVoting = false;      ///< Use voting for consensus
    float consensusThreshold = 0.7f;///< Required agreement (0.5-1.0)
    
    std::vector<std::string> agentModels;  ///< Specific models per agent

    enum class CycleStrategy {
        Uniform,        ///< All agents do same work
        Specialized,    ///< Each agent specializes in different aspect
        Competitive,    ///< Agents compete, best result wins
        Collaborative   ///< Agents work together iteratively
    } strategy = CycleStrategy::Collaborative;

    /// Calculate total iterations for a task
    int getTotalIterations(const TodoItem& task) const;

    json toJSON() const;
    static AgentCycleConfig fromJSON(const json& j);
};

// ============================================================================
// OrchestratorConfig — Master configuration
// ============================================================================
struct OrchestratorConfig {
    QualityMode qualityMode = QualityMode::Auto;
    ExecutionMode executionMode = ExecutionMode::Adaptive;
    AgentCycleConfig agentConfig;
    TerminalLimits terminalLimits;

    int maxConcurrentTasks = 4;     ///< Max parallel tasks
    int maxRetries = 3;             ///< Max retries per task
    bool failFast = false;          ///< Stop all on first failure
    bool autoSave = true;           ///< Auto-save progress
    int autoSaveIntervalSeconds = 60;

    bool enableDetailedLogging = true;
    bool enableTelemetry = true;
    bool enableSelfHealing = true;

    // No token, time, or complexity constraints when Max mode
    bool ignoreTokenLimits = false;
    bool ignoreTimeLimits = false;
    bool ignoreComplexityLimits = false;

    std::string workspaceRoot;
    std::string progressFile = "orchestrator_progress.json";

    json toJSON() const;
    static OrchestratorConfig fromJSON(const json& j);
};

// ============================================================================
// ExecutionStats — Real-time execution statistics
// ============================================================================
struct ExecutionStats {
    int totalTodos = 0;
    int completed = 0;
    int failed = 0;
    int skipped = 0;
    int inProgress = 0;
    int totalIterations = 0;
    int totalAgentsSpawned = 0;
    float avgConfidence = 0.0f;
    float avgTaskTimeMs = 0.0f;
    int totalTimeMs = 0;
    
    std::unordered_map<std::string, int> completedByCategory;
    std::unordered_map<std::string, int> failedByCategory;
    
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point lastUpdateTime;

    float getCompletionPercent() const;
    float getSuccessRate() const;
    int getEstimatedRemainingTimeMs() const;

    json toJSON() const;
};

// ============================================================================
// IterationTracker — Track and analyze agent iterations
// ============================================================================
class IterationTracker {
public:
    struct IterationRecord {
        uint64_t todoId;
        int iterationNumber;
        int agentId;
        std::string agentModel;
        std::chrono::steady_clock::time_point startTime;
        std::chrono::steady_clock::time_point endTime;
        int durationMs;
        bool success;
        float confidence;
        std::string output;
        std::string error;
    };

    void recordIteration(const IterationRecord& record);
    std::vector<IterationRecord> getIterationsForTodo(uint64_t todoId) const;
    int getTotalIterations() const;
    float getAverageIterationsPerTodo() const;
    json getAnalytics() const;

private:
    mutable std::mutex m_mutex;
    std::vector<IterationRecord> m_iterations;
    std::unordered_map<uint64_t, std::vector<IterationRecord>> m_iterationsByTodo;
};

// ============================================================================
// AutonomousOrchestrator — Main orchestration engine
// ============================================================================
class AutonomousOrchestrator {
public:
    explicit AutonomousOrchestrator(const OrchestratorConfig& config = OrchestratorConfig{});
    ~AutonomousOrchestrator();

    // ---- Configuration ----
    void setConfig(const OrchestratorConfig& config);
    const OrchestratorConfig& getConfig() const { return m_config; }

    void setQualityMode(QualityMode mode);
    QualityMode getQualityMode() const { return m_config.qualityMode; }

    void setAgentCycleMultiplier(int multiplier);
    int getAgentCycleMultiplier() const { return m_config.agentConfig.cycleMultiplier; }

    void setAgentCount(int count);
    int getAgentCount() const { return m_config.agentConfig.agentCount; }

    // ---- Codebase Audit ----
    /// Perform comprehensive codebase audit and generate todos
    AuditResult auditCodebase(const std::string& path, bool deep = true);

    /// Audit specific files
    AuditResult auditFiles(const std::vector<std::string>& files);

    /// Audit for production readiness
    AuditResult auditProductionReadiness(const std::string& path);

    /// Generate todos from natural language description
    std::vector<TodoItem> generateTodos(const std::string& description);

    // ---- Todo Management ----
    void addTodo(const TodoItem& todo);
    void addTodos(const std::vector<TodoItem>& todos);
    void removeTodo(uint64_t id);
    void clearTodos();

    std::vector<TodoItem> getTodos() const;
    std::vector<TodoItem> getPendingTodos() const;
    std::vector<TodoItem> getCompletedTodos() const;
    std::vector<TodoItem> getFailedTodos() const;

    TodoItem* getTodo(uint64_t id);

    // ---- Execution ----
    /// Execute todos synchronously
    bool execute();

    /// Execute top N most difficult todos
    bool executeTopDifficult(int n);

    /// Execute top N highest priority todos
    bool executeTopPriority(int n);

    /// Execute specific category of todos
    bool executeCategory(const std::string& category);

    /// Execute specific todo
    bool executeTodo(uint64_t id);

    /// Execute asynchronously (returns immediately)
    void executeAsync();

    /// Cancel execution
    void cancel();

    /// Pause execution
    void pause();

    /// Resume execution
    void resume();

    bool isRunning() const { return m_running.load(); }
    bool isPaused() const { return m_paused.load(); }

    // ---- Progress Tracking ----
    ExecutionStats getStats() const;
    float getProgress() const;
    std::string getStatusSummary() const;
    json getDetailedStatus() const;

    // ---- Iteration Tracking ----
    const IterationTracker& getIterationTracker() const { return m_iterationTracker; }
    json getIterationAnalytics() const;

    // ---- Callbacks ----
    using TodoStartedCb = std::function<void(const TodoItem&)>;
    using TodoCompletedCb = std::function<void(const TodoItem&, bool success)>;
    using TodoProgressCb = std::function<void(const TodoItem&, float progress)>;
    using AllCompletedCb = std::function<void(const ExecutionStats&)>;

    void setOnTodoStarted(TodoStartedCb cb) { m_onTodoStarted = cb; }
    void setOnTodoCompleted(TodoCompletedCb cb) { m_onTodoCompleted = cb; }
    void setOnTodoProgress(TodoProgressCb cb) { m_onTodoProgress = cb; }
    void setOnAllCompleted(AllCompletedCb cb) { m_onAllCompleted = cb; }

    // ---- Persistence ----
    bool saveProgress(const std::string& filename = "");
    bool loadProgress(const std::string& filename = "");

    // ---- Advanced Features ----
    /// Automatically analyze and adjust settings based on performance
    void autoOptimize();

    /// Get recommendations for improving execution
    json getOptimizationRecommendations() const;

private:
    // ---- Internal Execution ----
    void executionLoop();
    bool executeTodoInternal(TodoItem& todo);
    bool executeTodoWithMultiAgent(TodoItem& todo);
    bool executeTodoWithCycles(TodoItem& todo, int cycles);

    // ---- Dependency Management ----
    std::vector<TodoItem*> getExecutableTodos();
    bool checkDependenciesMet(const TodoItem& todo);
    void updateDependencies(uint64_t completedId);

    // ---- Quality Mode Adjustments ----
    void applyQualityMode(QualityMode mode);
    int getMaxIterationsForQuality(const TodoItem& todo) const;
    int getMaxAgentsForQuality(const TodoItem& todo) const;

    // ---- Terminal Time Management ----
    int getAdjustedTerminalTimeout(const TodoItem& todo) const;
    void updateTerminalLimitsFromHistory();

    // ---- Audit Implementation ----
    std::vector<TodoItem> auditDirectory(const std::string& dir, bool recursive);
    std::vector<TodoItem> auditFile(const std::string& file);
    std::vector<TodoItem> checkProductionReadiness(const std::string& file);

    // ---- Analysis ----
    void analyzeExecutionPatterns();
    float estimateTaskComplexity(const TodoItem& todo) const;

    // ---- Data ----
    OrchestratorConfig m_config;
    std::vector<TodoItem> m_todos;
    mutable std::mutex m_todosMutex;

    ExecutionStats m_stats;
    mutable std::mutex m_statsMutex;

    IterationTracker m_iterationTracker;

    std::atomic<bool> m_running{false};
    std::atomic<bool> m_paused{false};
    std::atomic<bool> m_cancelRequested{false};

    // Components
    std::unique_ptr<AgenticDeepThinkingEngine> m_thinkingEngine;
    std::unique_ptr<MetaPlanner> m_planner;
    std::unique_ptr<AgenticFailureDetector> m_failureDetector;
    std::unique_ptr<AgenticPuppeteer> m_puppeteer;

    // Callbacks
    TodoStartedCb m_onTodoStarted;
    TodoCompletedCb m_onTodoCompleted;
    TodoProgressCb m_onTodoProgress;
    AllCompletedCb m_onAllCompleted;

    // ID generation
    std::atomic<uint64_t> m_nextTodoId{1};

    // History for learning
    std::vector<TodoItem> m_completedHistory;
    std::vector<TodoItem> m_failedHistory;
};

} // namespace RawrXD
