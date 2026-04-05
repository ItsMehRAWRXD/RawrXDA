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
            LOG_INFO("AgenticOrch", "Loaded approval policy from " + path);
            return;
        }
        catch (...)
        {
            LOG_INFO("AgenticOrch", "approval_policy.json present but invalid JSON: " + path);
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

    m_orchestrator->setExecutionLogFn([](const std::string& log_entry) { LOG_INFO("AgenticOrch", log_entry); });

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

        // Use custom analyzer if provided, otherwise use built-in
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

        // Determine eligibility for auto-approval based on policy and risk
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
        }
        else
        {
            // Request human approval
            m_orchestrator->requestApproval(plan, i);
        }
    }

    return plan;
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
    // Called during plan generation; allows customization
    // (Currently used internally)
}

void OrchestratorIntegration::onStepExecution(ExecutionPlan* plan, int step_idx)
{
    if (!plan || !m_toolExecutor)
        return;

    auto& step = plan->steps[step_idx];

    // Execute each action in the step
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
            return;
        }
    }

    step.status = ExecutionStatus::Success;
}

void OrchestratorIntegration::onRollbackRequest(ExecutionPlan* plan, int step_idx)
{
    if (!plan || !m_rollbackExecutor)
        return;

    auto& step = plan->steps[step_idx];
    m_rollbackExecutor(step);
}

}  // namespace Agentic
