// ============================================================================
// autonomous_agent.h — High-end Autonomous Agentic Orchestration Engine
// ============================================================================
// Complete autonomous task planning, decomposition, and execution with:
//   • Workspace-aware multi-step planning
//   • Risk-tiered safety gates (SAFE/WARN/CRITICAL)
//   • Human-in-the-loop approval queue
//   • Parallel execution with dependency resolution
//   • Automatic rollback on failure
//   • Real-time progress tracking
//   • Reflection pass for quality assurance
// ============================================================================

#pragma once

#include "copilot_gap_closer.h"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace RawrXD {

// ============================================================================
// Constants
// ============================================================================
constexpr int AGENT_MAX_TASKS = 128;
constexpr int AGENT_MAX_STEPS = 512;
constexpr int AGENT_MAX_DEPS = 32;
constexpr int AGENT_APPROVAL_QUEUE_SIZE = 64;

// Risk levels for safety gates
enum class RiskLevel : uint8_t {
    SAFE = 0,           // Auto-approve, no user interaction
    WARN = 1,           // Show preview, require confirmation
    CRITICAL = 2        // Block, require explicit approval + review
};

// Step execution states
enum class StepState : uint8_t {
    PENDING = 0,
    READY = 1,
    EXECUTING = 2,
    COMPLETED = 3,
    FAILED = 4,
    ROLLED_BACK = 5,
    BLOCKED = 6
};

// Task states
enum class TaskState : uint8_t {
    SUBMITTED = 0,
    PLANNING = 1,
    AWAITING_APPROVAL = 2,
    APPROVED = 3,
    EXECUTING = 4,
    COMPLETED = 5,
    FAILED = 6,
    CANCELLED = 7
};

// ============================================================================
// WorkspaceContext — Workspace-aware planning context
// ============================================================================
struct WorkspaceContext {
    std::string rootPath;
    std::vector<std::string> sourceFiles;
    std::vector<std::string> testFiles;
    std::vector<std::string> docFiles;
    std::string gitBranch;
    std::string gitCommitHash;
    int64_t totalFilesSize;
    int fileCount;
    bool hasTests;
    bool hasCI;
    bool isMonorepo;
};

// ============================================================================
// ExecutionStep — Individual step in a plan
// ============================================================================
struct ExecutionStep {
    uint32_t id;
    std::string action;                    // e.g., "refactor_function", "add_tests"
    std::string description;
    std::vector<uint32_t> dependencies;    // Step IDs this depends on
    std::vector<std::string> inputs;       // Files, code snippets
    std::string expectedOutput;
    RiskLevel riskLevel;
    StepState state;
    int32_t estimatedDurationMs;
    int32_t actualDurationMs;
    std::string result;
    std::string errorMessage;
    bool canRollback;
    std::vector<std::string> rollbackOps;
    std::string previewContent;            // Preview of changes before execution
};

// ============================================================================
// ExecutionPlan — Complete multi-step plan for a task
// ============================================================================
struct ExecutionPlan {
    uint32_t planId;
    std::string taskDescription;
    std::vector<ExecutionStep> steps;
    int32_t totalEstimatedMs;
    int32_t totalActualMs;
    bool isParallelizable;
    RiskLevel maxRiskLevel;                // Highest risk in plan
    bool requiresApproval;
    std::string generatedAt;
    std::string executedAt;
    std::string completedAt;
    int successCount;
    int failureCount;
    std::string reasoning;                 // Why this plan was chosen
};

// ============================================================================
// ApprovalRequest — Request for user approval
// ============================================================================
struct ApprovalRequest {
    uint32_t requestId;
    uint32_t taskId;
    uint32_t stepId;
    std::string stepAction;
    std::string description;
    RiskLevel riskLevel;
    std::string preview;                   // What will change
    std::string reasoning;                 // Why this step is needed
    bool approved;
    bool reviewed;
    std::string reviewerNotes;
};

// ============================================================================
// WorkspaceAnalyzer — Analyzes workspace for planning context
// ============================================================================
class WorkspaceAnalyzer {
public:
    WorkspaceAnalyzer(GitContextProvider& gitCtx)
        : m_gitCtx(gitCtx) {}

    /// Analyze workspace structure and return context
    WorkspaceContext AnalyzeWorkspace(const std::string& rootPath);

    /// Detect project type (web, cli, lib, etc.)
    std::string DetectProjectType(const WorkspaceContext& ctx);

    /// Get file statistics
    struct FileStats {
        int totalFiles;
        int sourceFiles;
        int testFiles;
        int docFiles;
        int64_t totalSize;
    };
    FileStats GetFileStats(const WorkspaceContext& ctx);

private:
    GitContextProvider& m_gitCtx;
};

// ============================================================================
// PlanGenerator — Generates multi-step plans from tasks
// ============================================================================
class PlanGenerator {
public:
    PlanGenerator(VectorDatabase& vecDb, WorkspaceAnalyzer& analyzer)
        : m_vecDb(vecDb), m_analyzer(analyzer) {}

    /// Generate execution plan from task description
    ExecutionPlan GeneratePlan(const std::string& taskDescription,
                               const WorkspaceContext& workspace,
                               const std::vector<std::string>& contextFiles = {});

    /// Analyze task complexity and estimate duration
    int32_t EstimateDuration(const ExecutionPlan& plan);

    /// Validate plan for circular dependencies
    bool ValidatePlan(const ExecutionPlan& plan);

    /// Optimize for parallel execution
    void OptimizeForParallelism(ExecutionPlan& plan);

    /// Assign risk levels to steps
    void AssignRiskLevels(ExecutionPlan& plan);

private:
    VectorDatabase& m_vecDb;
    WorkspaceAnalyzer& m_analyzer;

    std::vector<ExecutionStep> DecomposeTask(const std::string& task,
                                             const WorkspaceContext& workspace);
    void ResolveDependencies(std::vector<ExecutionStep>& steps);
    void GeneratePreviews(std::vector<ExecutionStep>& steps);
};

// ============================================================================
// StepExecutor — Executes individual steps with safety gates
// ============================================================================
class StepExecutor {
public:
    StepExecutor(MultiFileComposer& composer, CrdtEngine& crdt)
        : m_composer(composer), m_crdt(crdt) {}

    /// Execute step with all safety gates
    bool ExecuteStep(ExecutionStep& step, bool dryRun = false);

    /// Prepare rollback operations
    bool PrepareRollback(ExecutionStep& step);

    /// Execute rollback
    bool Rollback(const ExecutionStep& step);

    /// Generate preview of changes
    std::string GeneratePreview(const ExecutionStep& step);

private:
    MultiFileComposer& m_composer;
    CrdtEngine& m_crdt;

    bool ExecuteRefactorAction(ExecutionStep& step);
    bool ExecuteTestAction(ExecutionStep& step);
    bool ExecuteDocAction(ExecutionStep& step);
    bool ExecuteAnalysisAction(ExecutionStep& step);
};

// ============================================================================
// DependencyResolver — Topological sort and parallel execution planning
// ============================================================================
class DependencyResolver {
public:
    /// Compute execution order respecting dependencies
    std::vector<uint32_t> ComputeExecutionOrder(const ExecutionPlan& plan);

    /// Identify steps that can execute in parallel
    std::vector<std::vector<uint32_t>> IdentifyParallelBatches(const ExecutionPlan& plan);

    /// Check for circular dependencies
    bool HasCircularDependencies(const ExecutionPlan& plan);

private:
    bool HasCycle(uint32_t stepId, const ExecutionPlan& plan,
                  std::vector<int>& visited, std::vector<int>& recStack);
};

// ============================================================================
// ApprovalManager — Manages approval queue and policies
// ============================================================================
class ApprovalManager {
public:
    /// Submit step for approval
    uint32_t SubmitForApproval(uint32_t taskId, uint32_t stepId,
                               const ExecutionStep& step);

    /// Get pending approvals
    std::vector<ApprovalRequest> GetPendingApprovals();

    /// Approve a request
    bool ApproveRequest(uint32_t requestId, const std::string& notes = "");

    /// Reject a request
    bool RejectRequest(uint32_t requestId, const std::string& reason = "");

    /// Set auto-approve policy for risk level
    void SetAutoApprovePolicy(RiskLevel level, bool autoApprove);

    /// Check if step should auto-approve
    bool ShouldAutoApprove(const ExecutionStep& step);

private:
    std::map<uint32_t, ApprovalRequest> m_approvalQueue;
    std::map<RiskLevel, bool> m_autoApprovePolicy;
    uint32_t m_nextRequestId = 1;
};

// ============================================================================
// AutonomousAgent — Main agentic orchestration engine
// ============================================================================
class AutonomousAgent {
public:
    AutonomousAgent(CopilotGapCloser& gapCloser)
        : m_gapCloser(gapCloser),
          m_analyzer(gapCloser.GetGitCtx()),
          m_planGen(gapCloser.GetVecDb(), m_analyzer),
          m_executor(gapCloser.GetComposer(), gapCloser.GetCrdt()),
          m_resolver(),
          m_approvalMgr() {}

    /// Submit high-level task for autonomous execution
    uint32_t SubmitTask(const std::string& taskDescription,
                        const std::string& workspacePath = "",
                        const std::vector<std::string>& contextFiles = {});

    /// Get execution plan for a task
    ExecutionPlan* GetPlan(uint32_t taskId);

    /// Get pending approvals for a task
    std::vector<ApprovalRequest> GetPendingApprovals(uint32_t taskId);

    /// Approve and start execution
    bool ApprovePlan(uint32_t taskId);

    /// Approve specific step
    bool ApproveStep(uint32_t taskId, uint32_t stepId);

    /// Execute plan with real-time progress
    bool ExecutePlan(uint32_t taskId, bool dryRun = false);

    /// Get current task status
    std::string GetStatus(uint32_t taskId);

    /// Get detailed progress
    struct TaskProgress {
        uint32_t taskId;
        int completedSteps;
        int totalSteps;
        int percentComplete;
        std::string currentStep;
        std::string estimatedTimeRemaining;
    };
    TaskProgress GetProgress(uint32_t taskId);

    /// Cancel task and rollback
    bool CancelTask(uint32_t taskId);

    /// Get task result
    std::string GetResult(uint32_t taskId);

    /// Enable dry-run mode (preview without applying)
    void SetDryRunMode(bool enabled) { m_dryRunMode = enabled; }

    /// Enable reflection pass (quality assurance)
    void SetReflectionEnabled(bool enabled) { m_reflectionEnabled = enabled; }

    /// Get comprehensive status
    std::string GetStatusString() const;

    /// Register callbacks
    using TaskCompletionCallback = std::function<void(uint32_t taskId, bool success)>;
    using ApprovalNeededCallback = std::function<void(uint32_t taskId, const ApprovalRequest&)>;
    using ProgressCallback = std::function<void(uint32_t taskId, const TaskProgress&)>;

    void OnTaskCompletion(TaskCompletionCallback cb) { m_completionCallback = cb; }
    void OnApprovalNeeded(ApprovalNeededCallback cb) { m_approvalCallback = cb; }
    void OnProgress(ProgressCallback cb) { m_progressCallback = cb; }

private:
    struct TaskContext {
        uint32_t taskId;
        std::string description;
        std::string workspacePath;
        WorkspaceContext workspace;
        ExecutionPlan plan;
        TaskState state;
        bool approved;
        bool executing;
        bool completed;
        std::string result;
    };

    CopilotGapCloser& m_gapCloser;
    WorkspaceAnalyzer m_analyzer;
    PlanGenerator m_planGen;
    StepExecutor m_executor;
    DependencyResolver m_resolver;
    ApprovalManager m_approvalMgr;

    std::map<uint32_t, TaskContext> m_tasks;
    uint32_t m_nextTaskId = 1;
    bool m_dryRunMode = false;
    bool m_reflectionEnabled = true;

    TaskContext* FindTask(uint32_t taskId);
    bool ExecuteStepWithDependencies(uint32_t taskId, uint32_t stepId);
    bool PerformReflectionPass(ExecutionPlan& plan);

    TaskCompletionCallback m_completionCallback;
    ApprovalNeededCallback m_approvalCallback;
    ProgressCallback m_progressCallback;
};

} // namespace RawrXD
