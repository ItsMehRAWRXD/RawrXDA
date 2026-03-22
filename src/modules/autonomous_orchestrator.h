// ============================================================================
// autonomous_orchestrator.h — High-end Agentic Task Orchestration Engine
// ============================================================================
// Autonomous multi-step task planning, decomposition, and execution with:
//   • Plan generation from natural language tasks
//   • Dependency graph resolution
//   • Safety gates and rollback capability
//   • Real-time progress tracking
//   • Parallel step execution where safe
//   • Integration with CopilotGapCloser subsystems
// ============================================================================

#pragma once

#include "copilot_gap_closer.h"
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>

namespace RawrXD {

// ============================================================================
// Constants
// ============================================================================
constexpr int PLAN_MAX_STEPS         = 256;
constexpr int PLAN_MAX_DEPENDENCIES  = 16;
constexpr int PLAN_MAX_ROLLBACK_OPS  = 512;
constexpr int ORCHESTRATOR_MAX_TASKS = 64;

// Step execution states
enum class StepState : uint8_t {
    PENDING = 0,
    READY = 1,
    EXECUTING = 2,
    COMPLETED = 3,
    FAILED = 4,
    ROLLED_BACK = 5
};

// Task priority levels
enum class TaskPriority : uint8_t {
    LOW = 0,
    NORMAL = 1,
    HIGH = 2,
    CRITICAL = 3
};

// Safety gate types
enum class SafetyGateType : uint8_t {
    NONE = 0,
    CONFIRM = 1,
    PREVIEW = 2,
    ROLLBACK_CAPABLE = 3,
    RESOURCE_CHECK = 4
};

// ============================================================================
// PlanStep — Individual step in an execution plan
// ============================================================================
struct PlanStep {
    uint32_t id;
    std::string action;                    // e.g., "refactor_function", "add_tests"
    std::string description;
    std::vector<uint32_t> dependencies;    // Step IDs this depends on
    std::vector<std::string> inputs;       // File paths, code snippets, etc.
    std::string expectedOutput;
    SafetyGateType safetyGate;
    StepState state;
    int32_t estimatedDurationMs;
    int32_t actualDurationMs;
    std::string result;
    std::string errorMessage;
    bool canRollback;
    std::vector<std::string> rollbackOps;  // Undo operations if needed
};

// ============================================================================
// ExecutionPlan — Complete multi-step plan for a task
// ============================================================================
struct ExecutionPlan {
    uint32_t planId;
    std::string taskDescription;
    std::vector<PlanStep> steps;
    int32_t totalEstimatedMs;
    int32_t totalActualMs;
    bool isParallelizable;
    bool requiresApproval;
    std::string generatedAt;
    std::string executedAt;
    std::string completedAt;
    int successCount;
    int failureCount;
};

// ============================================================================
// PlanGenerator — Converts tasks to execution plans using semantic analysis
// ============================================================================
class PlanGenerator {
public:
    PlanGenerator(VectorDatabase& vecDb, GitContextProvider& gitCtx)
        : m_vecDb(vecDb), m_gitCtx(gitCtx) {}

    /// Generate a multi-step execution plan from a natural language task.
    /// Uses semantic search to find similar past tasks and patterns.
    ExecutionPlan GeneratePlan(const std::string& taskDescription,
                               const std::vector<std::string>& contextFiles = {});

    /// Analyze task complexity and estimate total duration.
    int32_t EstimateDuration(const ExecutionPlan& plan);

    /// Validate plan for circular dependencies and resource conflicts.
    bool ValidatePlan(const ExecutionPlan& plan);

    /// Optimize plan for parallel execution where safe.
    void OptimizeForParallelism(ExecutionPlan& plan);

private:
    VectorDatabase& m_vecDb;
    GitContextProvider& m_gitCtx;

    std::vector<PlanStep> DecomposeTask(const std::string& task);
    void ResolveDependencies(std::vector<PlanStep>& steps);
    void AssignSafetyGates(std::vector<PlanStep>& steps);
};

// ============================================================================
// StepExecutor — Executes individual plan steps with safety gates
// ============================================================================
class StepExecutor {
public:
    StepExecutor(MultiFileComposer& composer, CrdtEngine& crdt)
        : m_composer(composer), m_crdt(crdt) {}

    /// Execute a single step with all safety gates applied.
    bool ExecuteStep(PlanStep& step, bool dryRun = false);

    /// Apply safety gate (confirmation, preview, etc.)
    bool ApplySafetyGate(const PlanStep& step, SafetyGateType gateType);

    /// Prepare rollback operations before execution.
    bool PrepareRollback(PlanStep& step);

    /// Execute rollback for a failed step.
    bool Rollback(const PlanStep& step);

private:
    MultiFileComposer& m_composer;
    CrdtEngine& m_crdt;

    bool ExecuteRefactorAction(PlanStep& step);
    bool ExecuteTestAction(PlanStep& step);
    bool ExecuteDocAction(PlanStep& step);
    bool ExecuteAnalysisAction(PlanStep& step);
};

// ============================================================================
// DependencyResolver — Topological sort and parallel execution planning
// ============================================================================
class DependencyResolver {
public:
    /// Compute execution order respecting dependencies.
    /// Returns list of step IDs in safe execution order.
    std::vector<uint32_t> ComputeExecutionOrder(const ExecutionPlan& plan);

    /// Identify steps that can execute in parallel.
    std::vector<std::vector<uint32_t>> IdentifyParallelBatches(const ExecutionPlan& plan);

    /// Check for circular dependencies.
    bool HasCircularDependencies(const ExecutionPlan& plan);

private:
    bool HasCycle(uint32_t stepId, const ExecutionPlan& plan,
                  std::vector<int>& visited, std::vector<int>& recStack);
};

// ============================================================================
// AutonomousOrchestrator — Main agentic orchestration engine
// ============================================================================
class AutonomousOrchestrator {
public:
    AutonomousOrchestrator(CopilotGapCloser& gapCloser)
        : m_gapCloser(gapCloser),
          m_planGen(gapCloser.GetVecDb(), gapCloser.GetGitCtx()),
          m_executor(gapCloser.GetComposer(), gapCloser.GetCrdt()),
          m_resolver() {}

    /// Submit a high-level task for autonomous execution.
    /// Returns task ID (> 0) on success.
    uint32_t SubmitTask(const std::string& taskDescription,
                        const std::vector<std::string>& contextFiles = {},
                        TaskPriority priority = TaskPriority::NORMAL);

    /// Get the execution plan for a task (before execution).
    ExecutionPlan* GetPlan(uint32_t taskId);

    /// Approve and start execution of a plan.
    bool ApprovePlan(uint32_t taskId);

    /// Execute a plan step-by-step with real-time progress.
    bool ExecutePlan(uint32_t taskId, bool dryRun = false);

    /// Get current execution status.
    std::string GetStatus(uint32_t taskId);

    /// Get detailed progress for a running task.
    struct TaskProgress {
        uint32_t taskId;
        int completedSteps;
        int totalSteps;
        int percentComplete;
        std::string currentStep;
        std::string estimatedTimeRemaining;
    };
    TaskProgress GetProgress(uint32_t taskId);

    /// Cancel a running task and rollback changes.
    bool CancelTask(uint32_t taskId);

    /// Get result of completed task.
    std::string GetResult(uint32_t taskId);

    /// Enable/disable dry-run mode (preview changes without applying).
    void SetDryRunMode(bool enabled) { m_dryRunMode = enabled; }

    /// Get comprehensive orchestrator status.
    std::string GetStatusString() const;

private:
    struct TaskContext {
        uint32_t taskId;
        std::string description;
        ExecutionPlan plan;
        TaskPriority priority;
        bool approved;
        bool executing;
        bool completed;
        std::string result;
    };

    CopilotGapCloser& m_gapCloser;
    PlanGenerator m_planGen;
    StepExecutor m_executor;
    DependencyResolver m_resolver;

    std::map<uint32_t, TaskContext> m_tasks;
    uint32_t m_nextTaskId = 1;
    bool m_dryRunMode = false;

    TaskContext* FindTask(uint32_t taskId);
    bool ExecuteStepWithDependencies(uint32_t taskId, uint32_t stepId);
};

} // namespace RawrXD
