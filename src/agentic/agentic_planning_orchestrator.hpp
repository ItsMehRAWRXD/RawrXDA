// agentic_planning_orchestrator.hpp
// Multi-step planning with risk-tiered safety gates and human-in-the-loop approval
// Orchestrates planning -> risk analysis -> approval gate -> execution -> rollback

#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>


namespace Agentic
{

// ============================================================================
// Risk & Safety Definitions
// ============================================================================

enum class StepRisk : uint8_t
{
    VeryLow = 1,  // Read-only or trivial writes
    Low = 2,      // Single-file, non-critical change
    Medium = 3,   // Multi-file or critical file, tested path
    High = 4,     // System-level, compilation impact
    Critical = 5  // Architecture, auto-approve disabled
};

enum class ApprovalStatus : uint8_t
{
    Pending = 1,
    Approved = 2,
    ApprovedAuto = 3,
    Rejected = 4,
    Expired = 5
};

enum class ExecutionStatus : uint8_t
{
    Waiting = 1,  // Awaiting approval
    Approved = 2,
    Executing = 3,
    Success = 4,
    Failed = 5,
    Rolled_Back = 6
};

// ============================================================================
// Planning & Approval Data Structures
// ============================================================================

struct PlanStep
{
    std::string id;
    std::string title;
    std::string description;
    std::vector<std::string> actions;         // Tool calls to execute
    std::vector<std::string> affected_files;  // Files that will be modified
    std::vector<std::string> dependencies;    // Must run after these steps
    StepRisk risk_level;
    float complexity_score;  // 0.0 - 1.0
    bool is_mutating;        // Modifies files/state
    bool is_rollbackable;
    int estimated_duration_ms;

    // Auto-approval criteria
    bool eligible_for_auto_approval;

    // Execution result
    ExecutionStatus status;
    ApprovalStatus approval_status;
    std::string approval_reason;
    std::string approval_user;  // Who approved (or "system" for auto)
    std::chrono::system_clock::time_point approval_time;

    // Execution tracking
    std::string execution_result;
    std::vector<std::string> generated_artifacts;
    std::string error_message;
    uint64_t rollback_id = 0;       // SafetyContract rollback reference
    uint64_t governor_task_id = 0;  // ExecutionGovernor task reference

    PlanStep()
        : risk_level(StepRisk::Medium), complexity_score(0.5f), is_mutating(false), is_rollbackable(true),
          estimated_duration_ms(30000), eligible_for_auto_approval(true), status(ExecutionStatus::Waiting),
          approval_status(ApprovalStatus::Pending)
    {
    }
};

struct ExecutionPlan
{
    std::string plan_id;
    std::string description;
    std::string workspace_root;
    std::vector<PlanStep> steps;
    std::string source_task;    // User's original request
    std::string planner_model;  // Which model generated this
    float confidence_score;     // Planner's confidence in the plan
    int total_estimated_duration_ms;
    bool has_critical_steps;
    bool requires_human_review;

    // Approval tracking
    int pending_approvals;
    int approved_steps;
    int rejected_steps;

    // Execution state — atomic because executeEntirePlan polls from calling thread
    std::atomic<int> current_step_index{-1};
    std::atomic<bool> is_executing{false};
    std::chrono::system_clock::time_point created_at;
    std::chrono::system_clock::time_point approval_deadline;

    ExecutionPlan()
        : confidence_score(0.8f), has_critical_steps(false), requires_human_review(false), pending_approvals(0),
          approved_steps(0), rejected_steps(0), created_at(std::chrono::system_clock::now())
    {
        approval_deadline = created_at + std::chrono::hours(24);
    }

    // std::atomic members delete implicit copy/move — provide explicit move
    ExecutionPlan(ExecutionPlan&& o) noexcept
        : plan_id(std::move(o.plan_id)), description(std::move(o.description)),
          workspace_root(std::move(o.workspace_root)), steps(std::move(o.steps)), source_task(std::move(o.source_task)),
          planner_model(std::move(o.planner_model)), confidence_score(o.confidence_score),
          total_estimated_duration_ms(o.total_estimated_duration_ms), has_critical_steps(o.has_critical_steps),
          requires_human_review(o.requires_human_review), pending_approvals(o.pending_approvals),
          approved_steps(o.approved_steps), rejected_steps(o.rejected_steps),
          current_step_index(o.current_step_index.load()), is_executing(o.is_executing.load()),
          created_at(o.created_at), approval_deadline(o.approval_deadline)
    {
    }

    ExecutionPlan& operator=(ExecutionPlan&& o) noexcept
    {
        if (this != &o)
        {
            plan_id = std::move(o.plan_id);
            description = std::move(o.description);
            workspace_root = std::move(o.workspace_root);
            steps = std::move(o.steps);
            source_task = std::move(o.source_task);
            planner_model = std::move(o.planner_model);
            confidence_score = o.confidence_score;
            total_estimated_duration_ms = o.total_estimated_duration_ms;
            has_critical_steps = o.has_critical_steps;
            requires_human_review = o.requires_human_review;
            pending_approvals = o.pending_approvals;
            approved_steps = o.approved_steps;
            rejected_steps = o.rejected_steps;
            current_step_index.store(o.current_step_index.load());
            is_executing.store(o.is_executing.load());
            created_at = o.created_at;
            approval_deadline = o.approval_deadline;
        }
        return *this;
    }

    ExecutionPlan(const ExecutionPlan&) = delete;
    ExecutionPlan& operator=(const ExecutionPlan&) = delete;

    nlohmann::json toJson() const;
    std::string toSummary() const;
};

// ============================================================================
// Approval Policy Engine
// ============================================================================

struct ApprovalPolicy
{
    bool auto_approve_very_low_risk = true;
    bool auto_approve_low_risk = false;  // Requires policy override
    bool require_approval_medium = true;
    bool require_approval_high = true;
    bool require_approval_critical = true;

    int min_complexity_for_human_review = 7;  // 0-10 scale
    int approval_timeout_hours = 24;

    bool allow_parallel_approvals = false;  // Must approve sequentially
    bool allow_partial_execution = false;   // All-or-nothing by default

    nlohmann::json toJson() const;
    static ApprovalPolicy fromJson(const nlohmann::json& j);
    static ApprovalPolicy Conservative();
    static ApprovalPolicy Standard();
    static ApprovalPolicy Aggressive();
};

// ============================================================================
// Main Orchestrator
// ============================================================================

class AgenticPlanningOrchestrator
{
  public:
    using PlanGenerationFn = std::function<ExecutionPlan(const std::string& task)>;
    using RiskAnalysisFn = std::function<StepRisk(const PlanStep&)>;
    using ApprovalCallbackFn = std::function<void(const ExecutionPlan&, int step_idx)>;
    using ExecutionLogFn = std::function<void(const std::string& log_entry)>;
    using ToolExecutorFn =
        std::function<bool(const std::string& tool_name, const std::string& args, std::string& output)>;
    using RollbackExecutorFn = std::function<void(const PlanStep& step)>;

    AgenticPlanningOrchestrator();
    ~AgenticPlanningOrchestrator();

    // Core orchestration
    ExecutionPlan* generatePlanForTask(const std::string& task_description);

    /// Register a plan produced outside the built-in generator (e.g. Win32IDE agent parse).
    ExecutionPlan* ingestExternalPlan(ExecutionPlan plan);

    /// Align Agentic approval state + queue with Win32IDE `PlanMutationGate` rows (same order as steps).
    /// `mutation_gate_u8[i]` must match `PlanMutationGate` numeric values in Win32IDE_Types.h.
    void applyWin32MutationGateSnapshot(ExecutionPlan* plan, const std::vector<std::uint8_t>& mutation_gate_u8,
                                        const std::vector<std::string>& gate_details);

    ApprovalStatus requestApproval(const ExecutionPlan* plan, int step_index);
    void approveStep(const ExecutionPlan* plan, int step_index, const std::string& user_id, const std::string& reason);
    void rejectStep(const ExecutionPlan* plan, int step_index, const std::string& user_id, const std::string& reason);

    // Execution
    bool executeNextApprovedStep(ExecutionPlan* plan);
    void rollbackStep(ExecutionPlan* plan, int step_index);
    void executeEntirePlan(ExecutionPlan* plan);

    // Approval queue management
    std::vector<std::pair<ExecutionPlan*, int>> getPendingApprovals() const;
    int getPendingApprovalCount() const;

    // Risk & policy
    void setApprovalPolicy(const ApprovalPolicy& policy);
    ApprovalPolicy getApprovalPolicy() const;
    StepRisk analyzeStepRisk(const PlanStep& step) const;

    // Callbacks & logging
    void setPlanGenerationFn(PlanGenerationFn fn) { m_planGenFn = std::move(fn); }
    void setRiskAnalysisFn(RiskAnalysisFn fn) { m_riskAnalysisFn = std::move(fn); }
    void setApprovalCallback(ApprovalCallbackFn fn) { m_approvalCallback = std::move(fn); }
    void setExecutionLogFn(ExecutionLogFn fn) { m_execLogFn = std::move(fn); }
    void setToolExecutorFn(ToolExecutorFn fn) { m_toolExecFn = std::move(fn); }
    void setRollbackExecutorFn(RollbackExecutorFn fn) { m_rollbackExecFn = std::move(fn); }

    // State access
    ExecutionPlan* getPlan(const std::string& plan_id);
    std::vector<ExecutionPlan*> getActivePlans() const;

    // JSON export for UI/monitoring
    nlohmann::json getPlanJson(const ExecutionPlan* plan) const;
    nlohmann::json getApprovalQueueJson() const;
    nlohmann::json getExecutionStatusJson() const;

  private:
    struct ApprovalGate
    {
        ExecutionPlan* plan;
        int step_index;
        ApprovalStatus status;
        std::vector<std::string> approvers;
        std::chrono::system_clock::time_point requested_at;
        std::chrono::system_clock::time_point expires_at;
    };

    bool shouldAutoApproveStep(const PlanStep& step) const;

    /// Precondition: m_mutex is held.
    ExecutionPlan* finalizeStoredPlanUnderLock(ExecutionPlan plan, bool applyBuiltinApprovalPolicy);

    ApprovalStatus pushApprovalGateUnlocked(ExecutionPlan* plan, int step_index);

    mutable std::mutex m_mutex;
    std::map<std::string, std::unique_ptr<ExecutionPlan>> m_activePlans;
    std::vector<ApprovalGate> m_approvalQueue;
    ApprovalPolicy m_policy;

    PlanGenerationFn m_planGenFn;
    RiskAnalysisFn m_riskAnalysisFn;
    ApprovalCallbackFn m_approvalCallback;
    ExecutionLogFn m_execLogFn;
    ToolExecutorFn m_toolExecFn;
    RollbackExecutorFn m_rollbackExecFn;
};

}  // namespace Agentic
