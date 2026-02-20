// ============================================================================
// autonomous_workflow_engine.hpp — End-to-End Autonomous Workflow Engine
// ============================================================================
// Architecture: C++20, Win32, no exceptions, no Qt
//
// Provides fully autonomous multi-stage pipelines that execute:
//   scan → bulk_fix → verify → build → test → summarize diff
// without human intervention. Each stage gates on the previous stage's
// success, with rollback and diff preview at each transition.
//
// This is the feature that converts RawrXD from "IDE with AI" into
// "agentic execution platform" — the single biggest valuation lever.
//
// Integration:
//   - BulkFixOrchestrator (agent/autonomous_subagent.hpp)
//   - ReasoningProfileManager (include/reasoning_profile.h)
//   - UnifiedHotpatchManager (core/unified_hotpatch_manager.hpp)
//   - AgenticFailureDetector / AgenticPuppeteer
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// ============================================================================
#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <memory>
#include "../core/model_memory_hotpatch.hpp"

// Forward declarations
class BulkFixOrchestrator;
class AgenticFailureDetector;
class AgenticPuppeteer;
struct BulkFixResult;
struct BulkFixTarget;
struct ReasoningProfile;

// ============================================================================
// WorkflowStage — Pipeline stage identifiers
// ============================================================================
enum class WorkflowStage : uint8_t {
    Idle                = 0,
    Scan                = 1,   // Discover targets (compile errors, stubs, etc.)
    BulkFix             = 2,   // Autonomous parallel fix dispatch
    Verify              = 3,   // Verify all fixes applied correctly
    Build               = 4,   // Rebuild to confirm no regressions
    Test                = 5,   // Run self-tests / unit tests
    SummarizeDiff       = 6,   // Generate human-readable change summary
    Complete            = 7,   // All stages passed
    RolledBack          = 8,   // Failure triggered rollback
    Aborted             = 9,   // User or system abort
    Count               = 10
};

// ============================================================================
// WorkflowStageResult — Outcome of a single stage
// ============================================================================
struct WorkflowStageResult {
    WorkflowStage   stage;
    bool            success;
    std::string     detail;
    int             itemsProcessed;
    int             itemsFailed;
    double          durationMs;
    std::string     diffPreview;       // Unified diff preview (for verify/summarize)
    std::string     buildOutput;       // Build stdout/stderr (for build stage)
    std::vector<std::string> testResults;  // Per-test outcomes (for test stage)

    static WorkflowStageResult ok(WorkflowStage s, const std::string& msg, int processed = 0) {
        WorkflowStageResult r;
        r.stage = s;
        r.success = true;
        r.detail = msg;
        r.itemsProcessed = processed;
        r.itemsFailed = 0;
        r.durationMs = 0;
        return r;
    }

    static WorkflowStageResult fail(WorkflowStage s, const std::string& msg, int failed = 0) {
        WorkflowStageResult r;
        r.stage = s;
        r.success = false;
        r.detail = msg;
        r.itemsProcessed = 0;
        r.itemsFailed = failed;
        r.durationMs = 0;
        return r;
    }
};

// ============================================================================
// WorkflowPolicy — Controls behavior of the workflow engine
// ============================================================================
struct WorkflowPolicy {
    bool            rollbackOnBuildFailure;      // Revert all fixes if build fails
    bool            rollbackOnTestFailure;        // Revert if tests fail
    bool            requireDiffApproval;          // Pause for diff review before build
    bool            abortOnScanEmpty;             // Stop if scan finds nothing
    bool            generateDiffSummary;          // Generate LLM-summarized diff
    int             maxBuildRetries;              // How many build attempts before rollback
    int             maxTestRetries;               // How many test re-runs before rollback
    int             maxTotalTimeMs;               // Hard timeout for entire workflow
    std::string     buildTarget;                  // CMake build target
    std::string     testTarget;                   // Test suite target or command
    std::string     buildConfig;                  // "Release", "Debug", etc.
    std::vector<std::string> scanPatterns;        // Glob patterns for scan stage

    WorkflowPolicy()
        : rollbackOnBuildFailure(true),
          rollbackOnTestFailure(true),
          requireDiffApproval(false),
          abortOnScanEmpty(true),
          generateDiffSummary(true),
          maxBuildRetries(2),
          maxTestRetries(2),
          maxTotalTimeMs(600000),   // 10 minutes default
          buildTarget("RawrXD-Shell"),
          testTarget("self_test_gate"),
          buildConfig("Release") {
        scanPatterns.push_back("src/**/*.cpp");
        scanPatterns.push_back("src/**/*.hpp");
        scanPatterns.push_back("src/**/*.h");
    }
};

// ============================================================================
// WorkflowSnapshot — Checkpoint for rollback
// ============================================================================
struct WorkflowSnapshot {
    std::string     snapshotId;
    WorkflowStage   takenAtStage;
    std::chrono::steady_clock::time_point timestamp;

    struct FileBackup {
        std::string filePath;
        std::vector<uint8_t> originalContent;
        size_t      originalSize;
    };
    std::vector<FileBackup> fileBackups;

    bool            valid;
};

// ============================================================================
// WorkflowResult — Final outcome of an autonomous workflow
// ============================================================================
struct WorkflowResult {
    std::string     workflowId;
    bool            success;
    WorkflowStage   finalStage;         // Where we stopped
    std::string     summary;            // Human-readable summary
    std::string     diffSummary;        // LLM-generated diff summary
    double          totalDurationMs;
    int             totalItemsFixed;
    int             totalItemsFailed;
    std::vector<WorkflowStageResult> stageResults;

    static WorkflowResult ok(const std::string& id, const std::string& summary) {
        WorkflowResult r;
        r.workflowId = id;
        r.success = true;
        r.finalStage = WorkflowStage::Complete;
        r.summary = summary;
        r.totalDurationMs = 0;
        r.totalItemsFixed = 0;
        r.totalItemsFailed = 0;
        return r;
    }

    static WorkflowResult fail(const std::string& id, WorkflowStage at, const std::string& reason) {
        WorkflowResult r;
        r.workflowId = id;
        r.success = false;
        r.finalStage = at;
        r.summary = reason;
        r.totalDurationMs = 0;
        r.totalItemsFixed = 0;
        r.totalItemsFailed = 0;
        return r;
    }
};

// ============================================================================
// Callbacks — Progress and gate notifications
// ============================================================================
using WorkflowStageStartedCb     = std::function<void(const std::string& workflowId, WorkflowStage stage)>;
using WorkflowStageCompletedCb   = std::function<void(const std::string& workflowId, const WorkflowStageResult& result)>;
using WorkflowDiffApprovalCb     = std::function<bool(const std::string& workflowId, const std::string& diffPreview)>;
using WorkflowCompletedCb        = std::function<void(const WorkflowResult& result)>;

// ============================================================================
// AutonomousWorkflowEngine — Orchestrates full end-to-end pipelines
// ============================================================================
class AutonomousWorkflowEngine {
public:
    static AutonomousWorkflowEngine& instance();

    // ---- Workflow Execution ----

    /// Execute a full autonomous workflow synchronously (blocks until complete or abort)
    WorkflowResult execute(const std::string& strategy,
                           const WorkflowPolicy& policy);

    /// Execute a workflow asynchronously
    std::string executeAsync(const std::string& strategy,
                             const WorkflowPolicy& policy,
                             WorkflowCompletedCb onComplete = nullptr);

    /// Abort a running workflow
    void abort(const std::string& workflowId);

    /// Abort all running workflows
    void abortAll();

    /// Check if any workflow is running
    bool isRunning() const;

    // ---- Predefined Workflow Factories ----
    static WorkflowPolicy makeCompileFixWorkflow();
    static WorkflowPolicy makeStubImplementWorkflow();
    static WorkflowPolicy makeLintCleanupWorkflow();
    static WorkflowPolicy makeSecurityAuditWorkflow();
    static WorkflowPolicy makeFullRefactorWorkflow(const std::string& oldName,
                                                     const std::string& newName);

    // ---- Progress & Status ----
    WorkflowStage getCurrentStage(const std::string& workflowId) const;
    std::vector<WorkflowStageResult> getStageResults(const std::string& workflowId) const;
    std::string getProgressJSON(const std::string& workflowId) const;

    // ---- Callbacks ----
    void setOnStageStarted(WorkflowStageStartedCb cb)     { m_onStageStarted = cb; }
    void setOnStageCompleted(WorkflowStageCompletedCb cb)  { m_onStageCompleted = cb; }
    void setOnDiffApproval(WorkflowDiffApprovalCb cb)      { m_onDiffApproval = cb; }
    void setOnCompleted(WorkflowCompletedCb cb)            { m_onCompleted = cb; }

    // ---- Dependency Injection ----
    void setBulkFixOrchestrator(BulkFixOrchestrator* orchestrator);
    void setFailureDetector(AgenticFailureDetector* detector);
    void setPuppeteer(AgenticPuppeteer* puppeteer);

    // ---- Snapshot/Rollback (prerequisite for safe-by-default refactors) ----
    WorkflowSnapshot takeSnapshot(const std::vector<std::string>& filePaths);
    PatchResult restoreSnapshot(const WorkflowSnapshot& snapshot);

    // ---- Statistics ----
    struct Stats {
        std::atomic<uint64_t> totalWorkflows{0};
        std::atomic<uint64_t> successfulWorkflows{0};
        std::atomic<uint64_t> failedWorkflows{0};
        std::atomic<uint64_t> rolledBackWorkflows{0};
        std::atomic<uint64_t> totalItemsFixed{0};
        std::atomic<uint64_t> totalBuildRetries{0};
    };
    const Stats& getStats() const { return m_stats; }
    void resetStats();

private:
    AutonomousWorkflowEngine();
    ~AutonomousWorkflowEngine();
    AutonomousWorkflowEngine(const AutonomousWorkflowEngine&) = delete;
    AutonomousWorkflowEngine& operator=(const AutonomousWorkflowEngine&) = delete;

    // ---- Stage executors ----
    WorkflowStageResult executeScan(const std::string& strategy,
                                     const WorkflowPolicy& policy,
                                     std::vector<BulkFixTarget>& outTargets);
    WorkflowStageResult executeBulkFix(const std::string& strategy,
                                        const WorkflowPolicy& policy,
                                        const std::vector<BulkFixTarget>& targets);
    WorkflowStageResult executeVerify(const std::string& strategy,
                                       const WorkflowPolicy& policy);
    WorkflowStageResult executeBuild(const WorkflowPolicy& policy);
    WorkflowStageResult executeTest(const WorkflowPolicy& policy);
    WorkflowStageResult executeSummarizeDiff(const WorkflowPolicy& policy,
                                              const std::vector<WorkflowStageResult>& priorResults);

    // ---- Internal helpers ----
    std::string generateWorkflowId() const;
    void emitStageStarted(const std::string& id, WorkflowStage stage);
    void emitStageCompleted(const std::string& id, const WorkflowStageResult& result);
    bool checkTimeout(const std::chrono::steady_clock::time_point& start, int maxMs) const;
    std::string generateUnifiedDiff(const WorkflowSnapshot& before) const;

    // ---- Members ----
    mutable std::mutex m_mutex;
    BulkFixOrchestrator*    m_orchestrator  = nullptr;
    AgenticFailureDetector* m_detector      = nullptr;
    AgenticPuppeteer*       m_puppeteer     = nullptr;

    struct ActiveWorkflow {
        std::string             id;
        WorkflowStage           currentStage;
        std::atomic<bool>       aborted{false};
        WorkflowPolicy          policy;
        WorkflowSnapshot        snapshot;
        std::vector<WorkflowStageResult> stageResults;
        std::chrono::steady_clock::time_point startTime;
    };
    std::unordered_map<std::string, std::shared_ptr<ActiveWorkflow>> m_active;

    Stats m_stats;

    // Callbacks
    WorkflowStageStartedCb     m_onStageStarted;
    WorkflowStageCompletedCb   m_onStageCompleted;
    WorkflowDiffApprovalCb     m_onDiffApproval;
    WorkflowCompletedCb        m_onCompleted;
};
