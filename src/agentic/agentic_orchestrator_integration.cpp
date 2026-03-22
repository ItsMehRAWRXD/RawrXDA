// agentic_orchestrator_integration.cpp
// Integration: wires AgenticPlanningOrchestrator into RawrXD IDE lifecycle

#include "agentic_orchestrator_integration.hpp"
#include "observability/Logger.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <vector>

namespace Agentic
{

namespace {

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

    // Wire planner: for now, use a stub that generates basic plans
    m_orchestrator->setPlanGenerationFn(
        [this](const std::string& task) -> ExecutionPlan
        {
            ExecutionPlan plan;
            plan.description = task;
            plan.source_task = task;
            plan.planner_model = "default_stub";
            plan.confidence_score = 0.75f;

            // Create a default multi-step plan
            // In production, this would call an actual LLM planner
            PlanStep step1;
            step1.id = "step_1_analyze";
            step1.title = "Analyze task requirements";
            step1.description = "Parse task to understand scope and dependencies";
            step1.is_mutating = false;
            step1.risk_level = StepRisk::VeryLow;
            plan.steps.push_back(step1);

            PlanStep step2;
            step2.id = "step_2_prepare";
            step2.title = "Prepare workspace";
            step2.description = "Set up build environment and dependencies";
            step2.is_mutating = false;
            step2.risk_level = StepRisk::Low;
            step2.dependencies.push_back(step1.id);
            plan.steps.push_back(step2);

            PlanStep step3;
            step3.id = "step_3_implement";
            step3.title = "Implement changes";
            step3.description = "Execute code modifications as planned";
            step3.is_mutating = true;
            step3.risk_level = StepRisk::Medium;  // Will be re-analyzed
            step3.dependencies.push_back(step2.id);
            step3.affected_files.push_back("src/implementation.cpp");
            plan.steps.push_back(step3);

            PlanStep step4;
            step4.id = "step_4_validate";
            step4.title = "Validate and test";
            step4.description = "Run tests to verify implementation";
            step4.is_mutating = false;
            step4.risk_level = StepRisk::VeryLow;
            step4.dependencies.push_back(step3.id);
            plan.steps.push_back(step4);

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
