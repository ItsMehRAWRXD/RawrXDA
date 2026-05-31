// ============================================================================
// autonomous_subagent.hpp — Autonomous SubAgent for Repetitive Bulk Fixes
// ============================================================================
// Architecture: C++20, Win32, no exceptions, no Qt
// Provides self-directing subagents that models/swarms can spawn for
// repetitive, patterned operations (bulk code fixes, mass refactors,
// uniform error correction, format enforcement, etc.).
//
// An AutonomousSubAgent differs from a plain SubAgent:
//   1. It owns a BulkFixStrategy that describes the pattern to apply
//   2. It iterates over a target list autonomously
//   3. It integrates AgenticFailureDetector + AgenticPuppeteer for self-healing
//   4. It reports per-item results via structured TodoItem tracking
//   5. It can be spawned from swarms, chains, planner, or model output
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
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

// Forward declarations
class AgenticEngine;
class AgenticFailureDetector;
class AgenticPuppeteer;
class SubAgentManager;
struct SubAgent;

// ============================================================================
// BulkFixTarget — One item to be fixed (file, function, endpoint, etc.)
// ============================================================================
struct BulkFixTarget {
    std::string id;             ///< Unique target identifier (filename, symbol, etc.)
    std::string path;           ///< File path or resource locator
    std::string context;        ///< Additional context (line range, function name, etc.)
    std::string category;       ///< Category grouping for parallel dispatch

    enum class Status {
        Pending, InProgress, Fixed, Failed, Skipped, Verified
    } status = Status::Pending;

    std::string result;         ///< Fix result or error message
    int retryCount = 0;         ///< Number of retry attempts made
    int elapsedMs = 0;          ///< Time spent on this target

    std::string statusString() const {
        switch (status) {
            case Status::Pending:    return "pending";
            case Status::InProgress: return "in-progress";
            case Status::Fixed:      return "fixed";
            case Status::Failed:     return "failed";
            case Status::Skipped:    return "skipped";
            case Status::Verified:   return "verified";
        }
        return "unknown";
    }
};

// ============================================================================
// BulkFixStrategy — Describes the pattern for a repetitive fix operation
// ============================================================================
struct BulkFixStrategy {
    std::string name;                   ///< Human-readable strategy name
    std::string description;            ///< What this bulk fix does
    std::string promptTemplate;         ///< Prompt template with {{target}} / {{context}} etc.
    std::string verificationPrompt;     ///< Optional: prompt to verify the fix was applied
    int maxRetries = 3;                 ///< Max retries per target on failure
    int maxParallel = 4;                ///< Max concurrent subagents for this strategy
    int perTargetTimeoutMs = 60000;     ///< Timeout per individual target fix
    bool failFast = false;              ///< Stop all on first failure
    bool autoVerify = true;             ///< Run verification pass after fix
    bool selfHeal = true;               ///< Use failure detector + puppeteer

    enum class MergeMode {
        Individual,     ///< Each target result stands alone
        Concatenate,    ///< Merge all results
        Summarize       ///< LLM summary of all results
    } mergeMode = MergeMode::Individual;

    /// Optional: custom validator function pointer (no std::function in hot path)
    bool (*customValidator)(const std::string& target, const std::string& result) = nullptr;
};

// ============================================================================
// BulkFixProgress — Aggregate progress for a bulk operation
// ============================================================================
struct BulkFixProgress {
    int totalTargets = 0;
    int completed = 0;
    int failed = 0;
    int skipped = 0;
    int verified = 0;
    int inProgress = 0;
    int retries = 0;
    float percentComplete = 0.0f;
    std::chrono::steady_clock::time_point startTime;
    int elapsedMs = 0;

    std::string summary() const;
};

// ============================================================================
// BulkFixResult — Final outcome of a bulk fix operation
// ============================================================================
struct BulkFixResult {
    bool success = false;
    std::string bulkFixId;
    std::string strategyName;
    std::vector<BulkFixTarget> targets;
    BulkFixProgress progress;
    std::string mergedResult;
    std::string error;

    static BulkFixResult ok(const std::string& id, const std::string& strategy,
                             const std::vector<BulkFixTarget>& targets,
                             const std::string& merged);
    static BulkFixResult fail(const std::string& id, const std::string& msg);

    int fixedCount() const;
    int failedCount() const;
    std::vector<BulkFixTarget> failedTargets() const;
};

// ============================================================================
// Callback types (no Qt signals)
// ============================================================================
using BulkFixTargetStartedCb   = std::function<void(const std::string& bulkFixId, const BulkFixTarget& target)>;
using BulkFixTargetCompletedCb = std::function<void(const std::string& bulkFixId, const BulkFixTarget& target, bool success)>;
using BulkFixProgressCb        = std::function<void(const std::string& bulkFixId, const BulkFixProgress& progress)>;
using BulkFixCompletedCb       = std::function<void(const BulkFixResult& result)>;

// ============================================================================
// AutonomousSubAgent — Self-directing agent for bulk repetitive operations
// ============================================================================
class AutonomousSubAgent {
public:
    explicit AutonomousSubAgent(SubAgentManager* manager,
                                 AgenticEngine* engine,
                                 AgenticFailureDetector* detector = nullptr,
                                 AgenticPuppeteer* puppeteer = nullptr);
    ~AutonomousSubAgent();

    // ---- Configuration ----
    void setStrategy(const BulkFixStrategy& strategy);
    const BulkFixStrategy& strategy() const { return m_strategy; }

    void setTargets(const std::vector<BulkFixTarget>& targets);
    void addTarget(const BulkFixTarget& target);
    const std::vector<BulkFixTarget>& targets() const { return m_targets; }

    // ---- Execution ----
    /// Execute bulk fix synchronously (blocks until all targets processed)
    BulkFixResult execute(const std::string& parentId);

    /// Execute bulk fix asynchronously (returns immediately, calls onComplete)
    std::string executeAsync(const std::string& parentId, BulkFixCompletedCb onComplete);

    /// Cancel a running bulk fix
    void cancel();

    /// Check if currently executing
    bool isRunning() const { return m_running.load(); }

    // ---- Progress ----
    BulkFixProgress getProgress() const;
    std::string getProgressJSON() const;

    // ---- Callbacks ----
    void setOnTargetStarted(BulkFixTargetStartedCb cb)     { m_onTargetStarted = cb; }
    void setOnTargetCompleted(BulkFixTargetCompletedCb cb)  { m_onTargetCompleted = cb; }
    void setOnProgress(BulkFixProgressCb cb)                { m_onProgress = cb; }
    void setOnCompleted(BulkFixCompletedCb cb)              { m_onCompleted = cb; }

    // ---- Status ----
    std::string getStatusSummary() const;

private:
    /// Process a single target (spawn subagent, wait, validate, retry)
    bool processTarget(BulkFixTarget& target, const std::string& parentId);

    /// Build the prompt for a target from the strategy template
    std::string buildPrompt(const BulkFixTarget& target) const;

    /// Verify a fix was applied correctly
    bool verifyFix(const BulkFixTarget& target, const std::string& result,
                   const std::string& parentId);

    /// Apply self-healing if fix failed
    std::string selfHeal(const std::string& failedResult, const BulkFixTarget& target);

    /// Merge all target results based on strategy merge mode
    std::string mergeResults(const std::string& parentId);

    /// Update progress and fire callback
    void updateProgress();

    /// Generate UUID
    std::string generateId() const;

    // ---- Members ----
    SubAgentManager*        m_manager   = nullptr;
    AgenticEngine*          m_engine    = nullptr;
    AgenticFailureDetector* m_detector  = nullptr;
    AgenticPuppeteer*       m_puppeteer = nullptr;

    BulkFixStrategy         m_strategy;
    std::vector<BulkFixTarget> m_targets;
    std::string             m_bulkFixId;

    mutable std::mutex      m_mutex;
    std::atomic<bool>       m_running{false};
    std::atomic<bool>       m_cancelled{false};

    BulkFixProgress         m_progress;

    // Callbacks
    BulkFixTargetStartedCb   m_onTargetStarted;
    BulkFixTargetCompletedCb m_onTargetCompleted;
    BulkFixProgressCb        m_onProgress;
    BulkFixCompletedCb       m_onCompleted;
};

// ============================================================================
// BulkFixOrchestrator — High-level coordinator for multi-strategy bulk fixes
// ============================================================================
// Models, swarms, and the planner can all call into this to dispatch
// autonomous bulk fix operations.
// ============================================================================
class BulkFixOrchestrator {
public:
    explicit BulkFixOrchestrator(SubAgentManager* manager,
                                  AgenticEngine* engine,
                                  AgenticFailureDetector* detector = nullptr,
                                  AgenticPuppeteer* puppeteer = nullptr);
    ~BulkFixOrchestrator();

    // ---- Predefined Strategy Factories ----
    static BulkFixStrategy makeCompileErrorFixStrategy();
    static BulkFixStrategy makeFormatEnforcementStrategy();
    static BulkFixStrategy makeRefactorRenameStrategy(const std::string& oldName,
                                                        const std::string& newName);
    static BulkFixStrategy makeStubImplementationStrategy();
    static BulkFixStrategy makeHeaderIncludeFixStrategy();
    static BulkFixStrategy makeLintFixStrategy();
    static BulkFixStrategy makeTestGenerationStrategy();
    static BulkFixStrategy makeDocCommentStrategy();
    static BulkFixStrategy makeSecurityAuditFixStrategy();
    static BulkFixStrategy makeCustomStrategy(const std::string& name,
                                                const std::string& promptTemplate);

    // ---- High-Level Dispatch ----
    /// Scan + fix all compile errors in given files
    BulkFixResult fixCompileErrors(const std::string& parentId,
                                    const std::vector<std::string>& filePaths);

    /// Apply a uniform refactor across files
    BulkFixResult applyBulkRefactor(const std::string& parentId,
                                     const BulkFixStrategy& strategy,
                                     const std::vector<BulkFixTarget>& targets);

    /// Execute an arbitrary bulk fix strategy asynchronously
    std::string dispatchBulkFix(const std::string& parentId,
                                 const BulkFixStrategy& strategy,
                                 const std::vector<BulkFixTarget>& targets,
                                 BulkFixCompletedCb onComplete = nullptr);

    /// Parse a bulk_fix tool call from model output
    bool parseBulkFixToolCall(const std::string& modelOutput,
                               BulkFixStrategy& strategy,
                               std::vector<BulkFixTarget>& targets) const;

    // ---- Active Operations ----
    BulkFixProgress getProgress(const std::string& bulkFixId) const;
    void cancelBulkFix(const std::string& bulkFixId);
    void cancelAll();
    std::vector<std::string> activeOperations() const;

    // ---- Statistics ----
    struct Stats {
        int totalBulkOps = 0;
        int totalTargetsProcessed = 0;
        int totalFixed = 0;
        int totalFailed = 0;
        int totalRetries = 0;
        int totalVerified = 0;
    };
    Stats getStats() const;
    void resetStats();

private:
    SubAgentManager*        m_manager   = nullptr;
    AgenticEngine*          m_engine    = nullptr;
    AgenticFailureDetector* m_detector  = nullptr;
    AgenticPuppeteer*       m_puppeteer = nullptr;

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, std::shared_ptr<AutonomousSubAgent>> m_active;
    Stats m_stats;
};

