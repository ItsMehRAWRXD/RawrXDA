// agentic_planning_orchestrator.cpp
// Multi-step planning orchestrator: risk gates, Win32IDE mutation-gate alignment, approval queue.

#include "agentic_planning_orchestrator.hpp"
#include "agentic_audit_sink.hpp"
#include "observability/Logger.hpp"
#include "../core/execution_governor.h"
#include "../core/agent_safety_contract.h"

#include <algorithm>
#include <ctime>
#include <functional>

namespace Agentic
{

// ============================================================================
// ExecutionPlan
// ============================================================================

nlohmann::json ExecutionPlan::toJson() const
{
    nlohmann::json j;
    j["plan_id"] = plan_id;
    j["description"] = description;
    j["workspace_root"] = workspace_root;
    j["source_task"] = source_task;
    j["planner_model"] = planner_model;
    j["confidence_score"] = confidence_score;
    j["total_duration_ms"] = total_estimated_duration_ms;
    j["has_critical_steps"] = has_critical_steps;
    j["requires_human_review"] = requires_human_review;
    j["pending_approvals"] = pending_approvals;
    j["approved_steps"] = approved_steps;
    j["rejected_steps"] = rejected_steps;
    j["current_step"] = current_step_index.load();
    j["is_executing"] = is_executing.load();
    return j;
}

std::string ExecutionPlan::toSummary() const
{
    std::string summary;
    summary += "Plan: " + plan_id + "\n";
    summary += "Description: " + description + "\n";
    summary += "Steps: " + std::to_string(steps.size()) + "\n";
    summary += "Status: ";
    if (is_executing.load())
    {
        summary += "EXECUTING (step " + std::to_string(current_step_index.load()) + ")\n";
    }
    else if (pending_approvals > 0)
    {
        summary += "AWAITING APPROVAL (" + std::to_string(pending_approvals) + " steps)\n";
    }
    else
    {
        summary += "READY\n";
    }
    summary += "Confidence: " + std::to_string(static_cast<int>(confidence_score * 100)) + "%\n";
    return summary;
}

// ============================================================================
// ApprovalPolicy
// ============================================================================

nlohmann::json ApprovalPolicy::toJson() const
{
    nlohmann::json j;
    j["auto_approve_very_low_risk"] = auto_approve_very_low_risk;
    j["auto_approve_low_risk"] = auto_approve_low_risk;
    j["require_approval_medium"] = require_approval_medium;
    j["require_approval_high"] = require_approval_high;
    j["require_approval_critical"] = require_approval_critical;
    j["min_complexity_for_human_review"] = min_complexity_for_human_review;
    j["approval_timeout_hours"] = approval_timeout_hours;
    j["allow_parallel_approvals"] = allow_parallel_approvals;
    j["allow_partial_execution"] = allow_partial_execution;
    return j;
}

ApprovalPolicy ApprovalPolicy::fromJson(const nlohmann::json& j)
{
    ApprovalPolicy p;
    if (j.contains("auto_approve_very_low_risk"))
        p.auto_approve_very_low_risk = j["auto_approve_very_low_risk"];
    if (j.contains("auto_approve_low_risk"))
        p.auto_approve_low_risk = j["auto_approve_low_risk"];
    if (j.contains("require_approval_medium"))
        p.require_approval_medium = j["require_approval_medium"];
    if (j.contains("require_approval_high"))
        p.require_approval_high = j["require_approval_high"];
    if (j.contains("require_approval_critical"))
        p.require_approval_critical = j["require_approval_critical"];
    if (j.contains("min_complexity_for_human_review"))
        p.min_complexity_for_human_review = j["min_complexity_for_human_review"];
    if (j.contains("approval_timeout_hours"))
        p.approval_timeout_hours = j["approval_timeout_hours"];
    if (j.contains("allow_parallel_approvals"))
        p.allow_parallel_approvals = j["allow_parallel_approvals"];
    if (j.contains("allow_partial_execution"))
        p.allow_partial_execution = j["allow_partial_execution"];
    return p;
}

ApprovalPolicy ApprovalPolicy::Conservative()
{
    return {true, false, true, true, true, 8, 24, false, false};
}

ApprovalPolicy ApprovalPolicy::Standard()
{
    return {true, false, true, true, true, 6, 24, false, false};
}

ApprovalPolicy ApprovalPolicy::Aggressive()
{
    return {true, true, false, true, true, 4, 12, true, true};
}

// ============================================================================
// AgenticPlanningOrchestrator
// ============================================================================

AgenticPlanningOrchestrator::AgenticPlanningOrchestrator() : m_policy(ApprovalPolicy::Standard()) {}

AgenticPlanningOrchestrator::~AgenticPlanningOrchestrator() = default;

ExecutionPlan* AgenticPlanningOrchestrator::finalizeStoredPlanUnderLock(ExecutionPlan plan,
                                                                        bool applyBuiltinApprovalPolicy)
{
    if (plan.plan_id.empty())
    {
        plan.plan_id = "plan_" + std::to_string(std::time(nullptr));
    }

    plan.current_step_index.store(-1);
    plan.is_executing.store(false);
    plan.has_critical_steps = false;
    plan.pending_approvals = 0;
    plan.approved_steps = 0;
    plan.rejected_steps = 0;

    for (auto& step : plan.steps)
    {
        if (m_riskAnalysisFn)
        {
            step.risk_level = m_riskAnalysisFn(step);
        }

        if (step.risk_level == StepRisk::Critical)
        {
            plan.has_critical_steps = true;
        }

        if (applyBuiltinApprovalPolicy)
        {
            step.eligible_for_auto_approval = shouldAutoApproveStep(step);
            if (!step.eligible_for_auto_approval)
            {
                plan.pending_approvals++;
                step.approval_status = ApprovalStatus::Pending;
            }
            else
            {
                step.approval_status = ApprovalStatus::ApprovedAuto;
                step.approval_user = "system";
                step.approval_time = std::chrono::system_clock::now();
                plan.approved_steps++;
            }
        }
        else
        {
            step.eligible_for_auto_approval = false;
            step.approval_status = ApprovalStatus::Pending;
            plan.pending_approvals++;
        }
        step.status = ExecutionStatus::Waiting;
    }

    plan.requires_human_review =
        applyBuiltinApprovalPolicy ? (plan.pending_approvals > 0 || plan.has_critical_steps) : true;

    const std::string pid = plan.plan_id;
    auto plan_ptr = std::make_unique<ExecutionPlan>(std::move(plan));
    ExecutionPlan* result = plan_ptr.get();
    m_activePlans[pid] = std::move(plan_ptr);

    if (m_execLogFn)
    {
        m_execLogFn("Plan registered: " + pid + " — pending=" + std::to_string(result->pending_approvals) +
                    " auto_ok=" + std::to_string(result->approved_steps));
    }
    LOG_INFO("AgenticPlanning", "Plan registered: " + pid);
    return result;
}

ExecutionPlan* AgenticPlanningOrchestrator::generatePlanForTask(const std::string& task_description)
{
    ExecutionPlan plan;
    if (m_planGenFn)
    {
        plan = m_planGenFn(task_description);
    }
    else
    {
        plan.description = task_description;
        plan.source_task = task_description;
        plan.planner_model = "default";
        PlanStep step;
        step.id = "step_0";
        step.title = "Execute: " + task_description;
        step.description = "Generated from task: " + task_description;
        step.is_mutating = true;
        step.risk_level = StepRisk::Medium;
        plan.steps.push_back(step);
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    return finalizeStoredPlanUnderLock(std::move(plan), true);
}

ExecutionPlan* AgenticPlanningOrchestrator::ingestExternalPlan(ExecutionPlan plan)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return finalizeStoredPlanUnderLock(std::move(plan), false);
}

bool AgenticPlanningOrchestrator::shouldAutoApproveStep(const PlanStep& step) const
{
    if (!step.is_mutating)
    {
        return true;
    }
    switch (step.risk_level)
    {
        case StepRisk::VeryLow:
            return m_policy.auto_approve_very_low_risk;
        case StepRisk::Low:
            return m_policy.auto_approve_low_risk;
        case StepRisk::Medium:
            return !m_policy.require_approval_medium;
        case StepRisk::High:
            return !m_policy.require_approval_high;
        case StepRisk::Critical:
            return !m_policy.require_approval_critical;
        default:
            return false;
    }
}

namespace
{
constexpr std::uint8_t kW32_PendingClassification = 0;
constexpr std::uint8_t kW32_AutoApproved = 1;
constexpr std::uint8_t kW32_AwaitingHuman = 2;
constexpr std::uint8_t kW32_HumanApproved = 3;
constexpr std::uint8_t kW32_HumanRejected = 4;
constexpr std::uint8_t kW32_SafetyBlocked = 5;
constexpr std::uint8_t kW32_SkippedByPolicy = 6;

SafetyRiskTier mapStepRiskToSafety(StepRisk r)
{
    switch (r)
    {
        case StepRisk::VeryLow:  return SafetyRiskTier::None;
        case StepRisk::Low:      return SafetyRiskTier::Low;
        case StepRisk::Medium:   return SafetyRiskTier::Medium;
        case StepRisk::High:     return SafetyRiskTier::High;
        case StepRisk::Critical: return SafetyRiskTier::Critical;
        default:                 return SafetyRiskTier::Medium;
    }
}

ActionClass classifyStepAction(const PlanStep& step)
{
    if (!step.is_mutating)
    {
        return step.affected_files.empty() ? ActionClass::InspectState : ActionClass::ReadFile;
    }
    for (const auto& action : step.actions)
    {
        if (action.find("delete") != std::string::npos || action.find("remove") != std::string::npos)
            return ActionClass::DeleteFile;
        if (action.find("run") != std::string::npos || action.find("exec") != std::string::npos ||
            action.find("build") != std::string::npos)
            return ActionClass::RunCommand;
        if (action.find("kill") != std::string::npos || action.find("system") != std::string::npos)
            return ActionClass::ProcessKill;
    }
    return step.affected_files.empty() ? ActionClass::RunCommand : ActionClass::EditFile;
}
}  // namespace

void AgenticPlanningOrchestrator::applyWin32MutationGateSnapshot(ExecutionPlan* plan,
                                                                 const std::vector<std::uint8_t>& mutation_gate_u8,
                                                                 const std::vector<std::string>& gate_details)
{
    if (!plan || mutation_gate_u8.size() != plan->steps.size())
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    m_approvalQueue.erase(std::remove_if(m_approvalQueue.begin(), m_approvalQueue.end(),
                                         [plan](const ApprovalGate& g) { return g.plan == plan; }),
                          m_approvalQueue.end());

    plan->pending_approvals = 0;
    plan->approved_steps = 0;
    plan->rejected_steps = 0;
    plan->has_critical_steps = false;

    const auto now = std::chrono::system_clock::now();

    for (size_t i = 0; i < plan->steps.size(); ++i)
    {
        auto& st = plan->steps[i];
        st.status = ExecutionStatus::Waiting;
        const std::uint8_t g = i < mutation_gate_u8.size() ? mutation_gate_u8[i] : kW32_PendingClassification;
        const std::string& detail = i < gate_details.size() ? gate_details[i] : std::string();

        if (st.risk_level == StepRisk::Critical)
        {
            plan->has_critical_steps = true;
        }

        switch (g)
        {
            case kW32_AutoApproved:
                st.approval_status = ApprovalStatus::ApprovedAuto;
                st.approval_user = "workspace_policy";
                st.approval_reason = detail;
                st.approval_time = now;
                st.eligible_for_auto_approval = true;
                plan->approved_steps++;
                break;
            case kW32_HumanApproved:
                st.approval_status = ApprovalStatus::Approved;
                st.approval_user = "ide_user";
                st.approval_reason = detail;
                st.approval_time = now;
                st.eligible_for_auto_approval = false;
                plan->approved_steps++;
                break;
            case kW32_HumanRejected:
            case kW32_SafetyBlocked:
            case kW32_SkippedByPolicy:
                st.approval_status = ApprovalStatus::Rejected;
                st.approval_user = "workspace_gate";
                st.approval_reason = detail;
                st.approval_time = now;
                st.eligible_for_auto_approval = false;
                plan->rejected_steps++;
                break;
            case kW32_AwaitingHuman:
            case kW32_PendingClassification:
            default:
                st.approval_status = ApprovalStatus::Pending;
                st.approval_user.clear();
                st.approval_reason = detail;
                st.eligible_for_auto_approval = false;
                plan->pending_approvals++;
                break;
        }
    }

    plan->requires_human_review = (plan->pending_approvals > 0 || plan->has_critical_steps);

    for (size_t i = 0; i < plan->steps.size(); ++i)
    {
        if (plan->steps[i].approval_status == ApprovalStatus::Pending)
        {
            pushApprovalGateUnlocked(plan, static_cast<int>(i));
        }
    }

    if (m_execLogFn)
    {
        m_execLogFn("Win32 gate snapshot: " + plan->plan_id + " pending=" + std::to_string(plan->pending_approvals) +
                    " approved=" + std::to_string(plan->approved_steps) +
                    " rejected=" + std::to_string(plan->rejected_steps));
    }
}

ApprovalStatus AgenticPlanningOrchestrator::pushApprovalGateUnlocked(ExecutionPlan* plan, int step_index)
{
    if (!plan || step_index < 0 || step_index >= static_cast<int>(plan->steps.size()))
    {
        return ApprovalStatus::Rejected;
    }

    auto& step = plan->steps[step_index];
    ApprovalGate gate{};
    gate.plan = plan;
    gate.step_index = step_index;
    gate.status = ApprovalStatus::Pending;
    gate.requested_at = std::chrono::system_clock::now();
    gate.expires_at = gate.requested_at + std::chrono::hours(m_policy.approval_timeout_hours);
    m_approvalQueue.push_back(gate);
    step.approval_status = ApprovalStatus::Pending;

    if (m_approvalCallback)
    {
        m_approvalCallback(*plan, step_index);
    }
    if (m_execLogFn)
    {
        m_execLogFn("Approval queue: plan " + plan->plan_id + " step " + std::to_string(step_index));
    }
    return ApprovalStatus::Pending;
}

ApprovalStatus AgenticPlanningOrchestrator::requestApproval(const ExecutionPlan* plan, int step_index)
{
    if (!plan || step_index < 0 || step_index >= static_cast<int>(plan->steps.size()))
    {
        return ApprovalStatus::Rejected;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    return pushApprovalGateUnlocked(const_cast<ExecutionPlan*>(plan), step_index);
}

void AgenticPlanningOrchestrator::approveStep(const ExecutionPlan* plan, int step_index, const std::string& user_id,
                                              const std::string& reason)
{
    if (!plan)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* mutablePlan = const_cast<ExecutionPlan*>(plan);
    if (step_index < 0 || step_index >= static_cast<int>(mutablePlan->steps.size()))
    {
        return;
    }
    auto& step = mutablePlan->steps[step_index];
    const ApprovalStatus prev = step.approval_status;
    step.approval_status = ApprovalStatus::Approved;
    step.approval_user = user_id;
    step.approval_reason = reason;
    step.approval_time = std::chrono::system_clock::now();

    if (prev == ApprovalStatus::Pending)
    {
        if (mutablePlan->pending_approvals > 0)
        {
            mutablePlan->pending_approvals--;
        }
        mutablePlan->approved_steps++;
    }

    m_approvalQueue.erase(
        std::remove_if(m_approvalQueue.begin(), m_approvalQueue.end(), [plan, step_index](const ApprovalGate& g)
                       { return g.plan == const_cast<ExecutionPlan*>(plan) && g.step_index == step_index; }),
        m_approvalQueue.end());

    if (m_execLogFn)
    {
        m_execLogFn("Step APPROVED: " + step.id + " by " + user_id);
    }

    {
        nlohmann::json aj;
        aj["event"] = "approve_step";
        aj["plan_id"] = mutablePlan->plan_id;
        aj["step_index"] = step_index;
        aj["step_id"] = step.id;
        aj["user"] = user_id;
        aj["reason"] = reason;
        aj["workspace_hash"] = std::to_string(std::hash<std::string>{}(mutablePlan->workspace_root));
        agenticAuditEmit("approval", aj.dump());
    }
}

void AgenticPlanningOrchestrator::rejectStep(const ExecutionPlan* plan, int step_index, const std::string& user_id,
                                             const std::string& reason)
{
    if (!plan)
    {
        return;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    auto* mutablePlan = const_cast<ExecutionPlan*>(plan);
    if (step_index < 0 || step_index >= static_cast<int>(mutablePlan->steps.size()))
    {
        return;
    }
    auto& step = mutablePlan->steps[step_index];
    const ApprovalStatus prev = step.approval_status;
    step.approval_status = ApprovalStatus::Rejected;
    step.approval_user = user_id;
    step.approval_reason = reason;
    step.approval_time = std::chrono::system_clock::now();

    if (prev == ApprovalStatus::Pending)
    {
        if (mutablePlan->pending_approvals > 0)
        {
            mutablePlan->pending_approvals--;
        }
        mutablePlan->rejected_steps++;
    }

    m_approvalQueue.erase(
        std::remove_if(m_approvalQueue.begin(), m_approvalQueue.end(), [plan, step_index](const ApprovalGate& g)
                       { return g.plan == const_cast<ExecutionPlan*>(plan) && g.step_index == step_index; }),
        m_approvalQueue.end());

    if (m_execLogFn)
    {
        m_execLogFn("Step REJECTED: " + step.id + " by " + user_id);
    }
}

bool AgenticPlanningOrchestrator::executeNextApprovedStep(ExecutionPlan* plan)
{
    if (!plan)
    {
        return false;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    for (size_t i = 0; i < plan->steps.size(); ++i)
    {
        auto& step = plan->steps[i];
        if (step.status != ExecutionStatus::Waiting)
        {
            continue;
        }
        if (step.approval_status != ApprovalStatus::Approved && step.approval_status != ApprovalStatus::ApprovedAuto)
        {
            continue;
        }

        bool deps_ready = true;
        for (const auto& dep : step.dependencies)
        {
            bool found = false;
            for (const auto& prev : plan->steps)
            {
                if (prev.id == dep && prev.status == ExecutionStatus::Success)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                deps_ready = false;
                break;
            }
        }
        if (!deps_ready)
        {
            continue;
        }

        plan->current_step_index.store(static_cast<int>(i));
        plan->is_executing.store(true);
        step.status = ExecutionStatus::Executing;

        // ── SafetyContract pre-check ───────────────────────────────────
        auto& safety = AgentSafetyContract::instance();
        if (safety.isInitialized())
        {
            auto check = safety.checkAndConsume(
                classifyStepAction(step), mapStepRiskToSafety(step.risk_level),
                "Plan step: " + step.title, plan->confidence_score);
            if (check.verdict == ContractVerdict::Denied)
            {
                step.status = ExecutionStatus::Failed;
                step.error_message = "SafetyContract DENIED: " + check.reason;
                plan->is_executing.store(false);
                plan->current_step_index.store(-1);
                if (m_execLogFn) m_execLogFn("Step " + step.id + " BLOCKED: " + check.reason);
                return true;
            }
            if (check.verdict == ContractVerdict::Escalated)
            {
                step.status = ExecutionStatus::Failed;
                step.error_message = "SafetyContract ESCALATED: " + check.reason;
                plan->is_executing.store(false);
                plan->current_step_index.store(-1);
                if (m_execLogFn) m_execLogFn("Step " + step.id + " ESCALATED: " + check.reason);
                return true;
            }

            // Register rollback before mutating
            if (step.is_rollbackable && step.is_mutating)
            {
                std::string target = step.affected_files.empty() ? step.id : step.affected_files[0];
                step.rollback_id = safety.registerRollback(
                    classifyStepAction(step), "Undo step: " + step.title, target, "",
                    m_rollbackExecFn ? [this, step_copy = step]() { m_rollbackExecFn(step_copy); }
                                     : std::function<void()>(nullptr));
            }
        }

        // ── Execute via tool executor or governor ──────────────────────
        std::string combined;
        bool ok = true;
        if (m_toolExecFn && !step.actions.empty())
        {
            for (const auto& action : step.actions)
            {
                std::string out;
                if (!m_toolExecFn(action, "", out))
                {
                    ok = false;
                    step.error_message = "Tool failed: " + action;
                    break;
                }
                combined += out + "\n";
            }
        }
        else if (!step.actions.empty())
        {
            // Fallback: submit first action through ExecutionGovernor
            auto& governor = ExecutionGovernor::instance();
            if (governor.isInitialized())
            {
                GovernorRiskTier govRisk = GovernorRiskTier::Medium;
                switch (step.risk_level)
                {
                    case StepRisk::VeryLow:  govRisk = GovernorRiskTier::None;     break;
                    case StepRisk::Low:      govRisk = GovernorRiskTier::Low;      break;
                    case StepRisk::Medium:   govRisk = GovernorRiskTier::Medium;   break;
                    case StepRisk::High:     govRisk = GovernorRiskTier::High;     break;
                    case StepRisk::Critical: govRisk = GovernorRiskTier::Critical; break;
                }
                const uint64_t timeout = step.estimated_duration_ms > 0
                    ? static_cast<uint64_t>(step.estimated_duration_ms)
                    : 30000ULL;
                auto taskId = governor.submitCommand(
                    step.actions[0], timeout, govRisk, step.title);
                step.governor_task_id = taskId;
                auto result = governor.waitForTask(taskId, timeout + 5000);
                combined = result.output;
                if (result.timedOut)
                {
                    ok = false;
                    step.error_message = "Governor timeout after " +
                        std::to_string(result.durationMs) + "ms";
                }
                else if (result.exitCode != 0)
                {
                    ok = false;
                    step.error_message = "Exit code " + std::to_string(result.exitCode);
                }
            }
            else
            {
                combined = "No executor available for: " + step.title;
            }
        }
        else
        {
            combined = "No-op step: " + step.title;
        }

        step.execution_result = combined;
        step.status = ok ? ExecutionStatus::Success : ExecutionStatus::Failed;

        if (!ok)
        {
            plan->is_executing.store(false);
            plan->current_step_index.store(-1);
            if (m_execLogFn)
            {
                m_execLogFn("executeNextApprovedStep: stopping after failure on " + step.id);
            }
            {
                nlohmann::json aj;
                aj["event"] = "tool_execute_failed";
                aj["plan_id"] = plan->plan_id;
                aj["step_id"] = step.id;
                aj["error"] = step.error_message;
                aj["workspace_hash"] = std::to_string(std::hash<std::string>{}(plan->workspace_root));
                agenticAuditEmit("execution", aj.dump());
            }
            return true;
        }

        bool all_done = true;
        for (const auto& s : plan->steps)
        {
            if (s.status == ExecutionStatus::Waiting || s.status == ExecutionStatus::Executing)
            {
                all_done = false;
                break;
            }
        }
        if (all_done)
        {
            plan->is_executing.store(false);
            plan->current_step_index.store(-1);
        }

        if (m_execLogFn)
        {
            m_execLogFn(ok ? ("Step done: " + step.id) : ("Step failed: " + step.id));
        }
        return true;
    }

    return false;
}

void AgenticPlanningOrchestrator::rollbackStep(ExecutionPlan* plan, int step_index)
{
    if (!plan || step_index < 0 || step_index >= static_cast<int>(plan->steps.size()))
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    auto& step = plan->steps[step_index];

    bool rolled_back = false;

    // Try SafetyContract registered rollback first (has saved state)
    if (step.rollback_id != 0)
    {
        auto& safety = AgentSafetyContract::instance();
        if (safety.isInitialized())
        {
            rolled_back = safety.executeRollback(step.rollback_id);
            if (rolled_back && m_execLogFn)
                m_execLogFn("Rollback via SafetyContract id=" + std::to_string(step.rollback_id) + ": " + step.id);
        }
    }

    // Fallback: use the orchestrator's own rollback callback
    if (!rolled_back && m_rollbackExecFn)
    {
        m_rollbackExecFn(step);
        rolled_back = true;
    }

    step.status = ExecutionStatus::Rolled_Back;
    if (m_execLogFn)
    {
        m_execLogFn("Rollback: " + step.id + (rolled_back ? "" : " (no executor available)"));
    }
    LOG_INFO("AgenticPlanning", "Step rolled back: " + step.id);
}

void AgenticPlanningOrchestrator::executeEntirePlan(ExecutionPlan* plan)
{
    if (!plan)
    {
        return;
    }
    plan->is_executing.store(true);
    if (m_execLogFn)
    {
        m_execLogFn("=== executeEntirePlan: " + plan->plan_id +
                    " (" + std::to_string(plan->steps.size()) + " steps) ===");
    }

    bool allow_partial = false;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        allow_partial = m_policy.allow_partial_execution;
    }

    while (plan->is_executing.load())
    {
        if (!executeNextApprovedStep(plan))
        {
            // Check for failures requiring cascade rollback
            bool has_failed = false;
            for (const auto& s : plan->steps)
            {
                if (s.status == ExecutionStatus::Failed) { has_failed = true; break; }
            }

            if (has_failed && !allow_partial)
            {
                if (m_execLogFn)
                    m_execLogFn("Plan " + plan->plan_id + " has failures — rolling back completed steps");
                for (int i = static_cast<int>(plan->steps.size()) - 1; i >= 0; --i)
                {
                    if (plan->steps[i].status == ExecutionStatus::Success && plan->steps[i].is_rollbackable)
                    {
                        rollbackStep(plan, i);
                    }
                }
            }

            plan->is_executing.store(false);
            break;
        }
    }

    if (m_execLogFn)
    {
        int ok = 0, fail = 0, rolled = 0;
        for (const auto& s : plan->steps)
        {
            if (s.status == ExecutionStatus::Success) ok++;
            else if (s.status == ExecutionStatus::Failed) fail++;
            else if (s.status == ExecutionStatus::Rolled_Back) rolled++;
        }
        m_execLogFn("=== Plan " + plan->plan_id + " finished: " +
                    std::to_string(ok) + " ok, " + std::to_string(fail) +
                    " failed, " + std::to_string(rolled) + " rolled back ===");
    }
}

std::vector<std::pair<ExecutionPlan*, int>> AgenticPlanningOrchestrator::getPendingApprovals() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::pair<ExecutionPlan*, int>> out;
    for (const auto& g : m_approvalQueue)
    {
        out.push_back({g.plan, g.step_index});
    }
    return out;
}

int AgenticPlanningOrchestrator::getPendingApprovalCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return static_cast<int>(m_approvalQueue.size());
}

void AgenticPlanningOrchestrator::setApprovalPolicy(const ApprovalPolicy& policy)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_policy = policy;
}

ApprovalPolicy AgenticPlanningOrchestrator::getApprovalPolicy() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_policy;
}

StepRisk AgenticPlanningOrchestrator::analyzeStepRisk(const PlanStep& step) const
{
    if (!step.is_mutating)
    {
        return StepRisk::VeryLow;
    }

    // File-count heuristic
    StepRisk risk = StepRisk::VeryLow;
    const auto n = step.affected_files.size();
    if (n == 0)      risk = StepRisk::VeryLow;
    else if (n == 1) risk = StepRisk::Low;
    else if (n <= 5) risk = StepRisk::Medium;
    else if (n <= 20) risk = StepRisk::High;
    else             risk = StepRisk::Critical;

    // Escalate based on action keywords
    for (const auto& action : step.actions)
    {
        if (action.find("delete") != std::string::npos || action.find("remove") != std::string::npos ||
            action.find("drop") != std::string::npos)
            risk = std::max(risk, StepRisk::High);
        if (action.find("exec") != std::string::npos || action.find("run") != std::string::npos ||
            action.find("shell") != std::string::npos)
            risk = std::max(risk, StepRisk::Medium);
        if (action.find("system") != std::string::npos || action.find("admin") != std::string::npos ||
            action.find("kill") != std::string::npos)
            risk = std::max(risk, StepRisk::Critical);
    }

    // High complexity bumps risk
    if (step.complexity_score >= 0.8f && risk < StepRisk::High)
        risk = StepRisk::High;

    return risk;
}

ExecutionPlan* AgenticPlanningOrchestrator::getPlan(const std::string& plan_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_activePlans.find(plan_id);
    if (it == m_activePlans.end())
    {
        return nullptr;
    }
    return it->second.get();
}

std::vector<ExecutionPlan*> AgenticPlanningOrchestrator::getActivePlans() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ExecutionPlan*> out;
    for (const auto& p : m_activePlans)
    {
        out.push_back(p.second.get());
    }
    return out;
}

nlohmann::json AgenticPlanningOrchestrator::getPlanJson(const ExecutionPlan* plan) const
{
    if (!plan)
    {
        return nlohmann::json::object();
    }
    return plan->toJson();
}

nlohmann::json AgenticPlanningOrchestrator::getApprovalQueueJson() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& g : m_approvalQueue)
    {
        nlohmann::json e;
        e["plan_id"] = g.plan ? g.plan->plan_id : "";
        e["step_index"] = g.step_index;
        e["status"] = static_cast<int>(g.status);
        arr.push_back(e);
    }
    return arr;
}

nlohmann::json AgenticPlanningOrchestrator::getExecutionStatusJson() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    nlohmann::json j;
    j["active_plans"] = m_activePlans.size();
    j["pending_approvals"] = m_approvalQueue.size();
    nlohmann::json exec = nlohmann::json::array();
    for (const auto& p : m_activePlans)
    {
        if (p.second->is_executing.load())
        {
            exec.push_back(p.first);
        }
    }
    j["executing_plans"] = exec;
    return j;
}

}  // namespace Agentic
