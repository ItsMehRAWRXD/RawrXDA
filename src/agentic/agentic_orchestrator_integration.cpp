// agentic_orchestrator_integration.cpp
// Integration: wires AgenticPlanningOrchestrator into RawrXD IDE lifecycle

#include "agentic_orchestrator_integration.hpp"
#include "observability/Logger.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <cctype>
#include <chrono>

namespace Agentic
{

namespace {

std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

bool hasAnyTerm(const std::string& haystack, const std::initializer_list<const char*> terms)
{
    for (const char* t : terms)
    {
        if (haystack.find(t) != std::string::npos)
            return true;
    }
    return false;
}

PlanStep makeStep(const std::string& id, const std::string& title, const std::string& description,
                  bool isMutating, StepRisk risk)
{
    PlanStep s;
    s.id = id;
    s.title = title;
    s.description = description;
    s.is_mutating = isMutating;
    s.risk_level = risk;
    return s;
}

void tryLoadApprovalPolicyFromDisk(AgenticPlanningOrchestrator& orch)
{
    std::vector<std::string> candidates;
    if (const char* root = std::getenv("RAWRXD_REPO_ROOT"))
    {
        candidates.push_back(std::string(root) + "\\config\\approval_policy.json");
    }
    candidates.push_back("config\\approval_policy.json");
    candidates.push_back("approval_policy.json");
    if (const char* ad = std::getenv("APPDATA"))
    {
        candidates.push_back(std::string(ad) + "\\RawrXD\\approval_policy.json");
    }

    for (const auto& path : candidates)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in)
            continue;
        std::ostringstream ss;
        ss << in.rdbuf();
        try
        {
            auto j = nlohmann::json::parse(ss.str());
            orch.setApprovalPolicy(ApprovalPolicy::fromJson(j));
            return;
        }
        catch (...)
        {
            // Invalid JSON in approval policy file
        }
    }
}

}  // namespace

OrchestratorIntegration::OrchestratorIntegration() : m_orchestrator(std::make_unique<AgenticPlanningOrchestrator>()) {}

OrchestratorIntegration::~OrchestratorIntegration() {}

void OrchestratorIntegration::initialize()
{
    if (!m_orchestrator || m_initialized)
    {
        return;
    }

    // Wire planner with deterministic task-aware plan generation.
    m_orchestrator->setPlanGenerationFn(
        [this](const std::string& task) -> ExecutionPlan
        {
            ExecutionPlan plan;
            plan.description = task;
            plan.source_task = task;
            plan.planner_model = "deterministic_rule_planner_v1";
            plan.confidence_score = 0.82f;

            const std::string lowered = toLower(task);
            const bool isBugFix = hasAnyTerm(lowered, {"fix", "bug", "crash", "error", "regression"});
            const bool isFeature = hasAnyTerm(lowered, {"add", "implement", "feature", "support"});
            const bool isRefactor = hasAnyTerm(lowered, {"refactor", "cleanup", "restructure"});
            const bool isPerf = hasAnyTerm(lowered, {"optimize", "latency", "performance", "throughput"});
            const bool touchesBuild = hasAnyTerm(lowered, {"cmake", "build", "link", "compile"});
            const bool touchesTests = hasAnyTerm(lowered, {"test", "coverage", "ctest", "smoke"});

            PlanStep discover = makeStep(
                "step_1_discover",
                "Discover current behavior",
                "Locate relevant symbols and verify current execution path before making changes.",
                false,
                StepRisk::VeryLow);
            discover.actions.push_back("semantic_search");
            discover.actions.push_back("read_file");
            plan.steps.push_back(std::move(discover));

            PlanStep design = makeStep(
                "step_2_design",
                "Design concrete change",
                "Define minimal code edits and safety checks for the task.",
                false,
                isRefactor ? StepRisk::Low : StepRisk::VeryLow);
            design.dependencies.push_back("step_1_discover");
            plan.steps.push_back(std::move(design));

            PlanStep implement = makeStep(
                "step_3_implement",
                isBugFix ? "Patch failing behavior" : (isFeature ? "Implement feature behavior" : "Apply code changes"),
                "Edit impacted files and wire runtime paths to concrete implementations.",
                true,
                (isPerf || isRefactor) ? StepRisk::Medium : StepRisk::Low);
            implement.dependencies.push_back("step_2_design");
            implement.actions.push_back("apply_patch");
            plan.steps.push_back(std::move(implement));

            PlanStep build = makeStep(
                "step_4_build",
                "Build and verify",
                "Compile updated targets and verify no regressions were introduced.",
                false,
                touchesBuild ? StepRisk::Medium : StepRisk::Low);
            build.dependencies.push_back("step_3_implement");
            build.actions.push_back("cmake_build");
            plan.steps.push_back(std::move(build));

            PlanStep validate = makeStep(
                "step_5_validate",
                touchesTests ? "Run validation tests" : "Run smoke validation",
                "Execute targeted checks and confirm behavior with runtime evidence.",
                false,
                StepRisk::VeryLow);
            validate.dependencies.push_back("step_4_build");
            validate.actions.push_back(touchesTests ? "run_tests" : "smoke_test");
            plan.steps.push_back(std::move(validate));

            return plan;
        });

    if (m_riskAnalyzer)
    {
        m_orchestrator->setRiskAnalysisFn(m_riskAnalyzer);
    }

    m_orchestrator->setExecutionLogFn(
        [this](const std::string& log_entry)
        {
            if (m_onLogMessage)
            {
                m_onLogMessage(log_entry);
            }
        });

    m_orchestrator->setApprovalCallback(
        [this](const ExecutionPlan& plan, int step_idx)
        {
            if (m_onApprovalRequired)
            {
                m_onApprovalRequired(plan, step_idx);
            }
        });

    // Wire tool executor: delegates to the integration's callback
    m_orchestrator->setToolExecutorFn(
        [this](const std::string& tool_name, const std::string& args, std::string& output) -> bool
        {
            if (m_toolExecutor)
            {
                return m_toolExecutor(tool_name, args, output);
            }
            output = "No tool executor configured";
            return false;
        });

    // Wire rollback executor: delegates to the integration's callback
    m_orchestrator->setRollbackExecutorFn(
        [this](const PlanStep& step)
        {
            if (m_rollbackExecutor)
            {
                m_rollbackExecutor(step);
            }
        });

    // Default policy, optionally overridden by machine-readable config (E07)
    m_orchestrator->setApprovalPolicy(ApprovalPolicy::Standard());
    tryLoadApprovalPolicyFromDisk(*m_orchestrator);
    m_initialized = true;
}

ExecutionPlan* OrchestratorIntegration::planAndApproveTask(const std::string& task_description)
{
    if (!m_orchestrator)
    {
        return nullptr;
    }

    // Step 1: Generate the plan
    auto* plan = m_orchestrator->generatePlanForTask(task_description);
    if (!plan)
        return nullptr;

    // Step 2: Analyze risk for each step
    for (size_t i = 0; i < plan->steps.size(); ++i)
    {
        auto& step = plan->steps[i];

        if (m_riskAnalyzer)
        {
            step.risk_level = m_riskAnalyzer(step);
        }
        else
        {
            step.risk_level = m_orchestrator->analyzeStepRisk(step);
        }
    }

    // Step 3: Check approval policy and request approvals as needed
    for (size_t i = 0; i < plan->steps.size(); ++i)
    {
        auto& step = plan->steps[i];
        auto policy = m_orchestrator->getApprovalPolicy();

        bool should_auto_approve = false;
        if (step.risk_level == StepRisk::VeryLow && policy.auto_approve_very_low_risk)
        {
            should_auto_approve = true;
        }
        else if (step.risk_level == StepRisk::Low && policy.auto_approve_low_risk)
        {
            should_auto_approve = true;
        }

        if (should_auto_approve)
        {
            step.approval_status = ApprovalStatus::ApprovedAuto;
            step.approval_user = "system";
            step.approval_reason = "Auto-approved by policy";
            if (m_onStepApproved)
            {
                m_onStepApproved(*plan, static_cast<int>(i));
            }
        }
        else
        {
            m_orchestrator->requestApproval(plan, static_cast<int>(i));
            if (m_onApprovalRequired)
            {
                m_onApprovalRequired(*plan, static_cast<int>(i));
            }
        }
    }

    if (m_onPlanGenerated)
    {
        m_onPlanGenerated(*plan);
    }

    return plan;
}

bool OrchestratorIntegration::executePlanStep(ExecutionPlan* plan, int step_idx)
{
    if (!plan || !m_orchestrator)
        return false;

    if (m_onStepExecutionStarted)
    {
        m_onStepExecutionStarted(*plan, step_idx);
    }

    bool success = m_orchestrator->executeNextApprovedStep(plan);

    if (success)
    {
        if (m_onStepCompleted)
        {
            m_onStepCompleted(*plan, step_idx);
        }
    }
    else
    {
        if (m_onStepFailed)
        {
            m_onStepFailed(*plan, step_idx, plan->steps[step_idx].error_message);
        }
    }

    return success;
}

bool OrchestratorIntegration::executeEntirePlan(ExecutionPlan* plan)
{
    if (!plan || !m_orchestrator)
        return false;

    if (m_onPlanExecutionStarted)
    {
        m_onPlanExecutionStarted(*plan);
    }

    plan->is_executing.store(true);
    bool all_success = true;

    for (size_t i = 0; i < plan->steps.size(); ++i)
    {
        if (!executePlanStep(plan, static_cast<int>(i)))
        {
            all_success = false;
            break;
        }
    }

    plan->is_executing.store(false);

    if (all_success)
    {
        if (m_onPlanCompleted)
        {
            m_onPlanCompleted(*plan);
        }
    }
    else
    {
        if (m_onPlanFailed)
        {
            m_onPlanFailed(*plan, "One or more steps failed during execution");
        }
    }

    return all_success;
}

void OrchestratorIntegration::cancelPlan(ExecutionPlan* plan)
{
    if (!plan)
        return;

    plan->is_executing.store(false);
    for (auto& step : plan->steps)
    {
        if (step.status == ExecutionStatus::Executing)
        {
            step.status = ExecutionStatus::Failed;
            step.error_message = "Cancelled by user";
        }
    }

    if (m_onPlanCancelled)
    {
        m_onPlanCancelled(*plan);
    }
}

void OrchestratorIntegration::pausePlan(ExecutionPlan* plan)
{
    if (!plan)
        return;

    plan->is_executing.store(false);
    if (m_onPlanPaused)
    {
        m_onPlanPaused(*plan);
    }
}

void OrchestratorIntegration::resumePlan(ExecutionPlan* plan)
{
    if (!plan)
        return;

    plan->is_executing.store(true);
    if (m_onPlanResumed)
    {
        m_onPlanResumed(*plan);
    }
}

int OrchestratorIntegration::getPendingApprovalCount() const
{
    if (!m_orchestrator)
        return 0;
    return m_orchestrator->getPendingApprovalCount();
}

std::vector<std::pair<ExecutionPlan*, int>> OrchestratorIntegration::getPendingApprovals() const
{
    if (!m_orchestrator)
        return {};
    return m_orchestrator->getPendingApprovals();
}

void OrchestratorIntegration::onPlanGeneration(const std::string& task, ExecutionPlan& plan)
{
    (void)task;
    if (m_onPlanGenerated)
    {
        m_onPlanGenerated(plan);
    }
}

void OrchestratorIntegration::onStepExecution(ExecutionPlan* plan, int step_idx)
{
    if (!plan || !m_toolExecutor)
        return;

    auto& step = plan->steps[step_idx];

    for (const auto& action : step.actions)
    {
        std::string output;
        if (m_toolExecutor(action, "", output))
        {
            step.execution_result += output + "\n";
        }
        else
        {
            step.error_message = "Tool execution failed: " + action;
            step.status = ExecutionStatus::Failed;
            if (m_onStepFailed)
            {
                m_onStepFailed(*plan, step_idx, step.error_message);
            }
            return;
        }
    }

    step.status = ExecutionStatus::Success;
    if (m_onStepCompleted)
    {
        m_onStepCompleted(*plan, step_idx);
    }
}

void OrchestratorIntegration::onRollbackRequest(ExecutionPlan* plan, int step_idx)
{
    if (!plan || !m_rollbackExecutor)
        return;

    auto& step = plan->steps[step_idx];
    m_rollbackExecutor(step);
    step.status = ExecutionStatus::Rolled_Back;

    if (m_onStepRolledBack)
    {
        m_onStepRolledBack(*plan, step_idx);
    }
}

}  // namespace Agentic
