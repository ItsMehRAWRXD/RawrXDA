// agentic_orchestrator_integration.cpp
// Integration: wires AgenticPlanningOrchestrator into RawrXD IDE lifecycle

#include "agentic_orchestrator_integration.hpp"
#include "hotpatch/Engine.hpp"
#include "observability/Logger.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace Agentic
{

namespace {

std::string toLowerCopy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

bool containsAnyToken(const std::string& haystackLower, const std::vector<std::string>& tokens)
{
    for (const auto& t : tokens)
    {
        if (haystackLower.find(t) != std::string::npos)
            return true;
    }
    return false;
}

bool isLikelyAbsolutePathToken(const std::string& token)
{
    if (token.size() > 3 && std::isalpha(static_cast<unsigned char>(token[0])) && token[1] == ':' &&
        (token[2] == '\\' || token[2] == '/'))
    {
        return true;
    }
    if (token.rfind("src/", 0) == 0 || token.rfind("src\\", 0) == 0)
    {
        return true;
    }
    return false;
}

constexpr size_t kDefaultContextWindowTokens = 32768;
constexpr size_t kMinContextWindowTokens = 4096;
constexpr size_t kMaxContextWindowTokens = 1048576;

size_t sanitizeContextWindow(size_t tokens)
{
    if (tokens == 0)
    {
        return kDefaultContextWindowTokens;
    }
    return std::clamp(tokens, kMinContextWindowTokens, kMaxContextWindowTokens);
}

int deriveComplexityReviewThreshold(size_t contextTokens)
{
    if (contextTokens <= 16384)
        return 5;
    if (contextTokens <= 32768)
        return 6;
    if (contextTokens >= 262144)
        return 8;
    return 7;
}

std::string buildActionArgsEnvelope(const ExecutionPlan& plan, const PlanStep& step, int stepIdx,
                                    const std::string& action)
{
    nlohmann::json j;
    j["plan_id"] = plan.plan_id;
    j["source_task"] = plan.source_task;
    j["workspace_root"] = plan.workspace_root;
    j["step_index"] = stepIdx;
    j["step_id"] = step.id;
    j["step_title"] = step.title;
    j["step_description"] = step.description;
    j["action"] = action;
    j["risk_level"] = static_cast<int>(step.risk_level);
    j["is_mutating"] = step.is_mutating;
    j["affected_files"] = step.affected_files;
    j["dependencies"] = step.dependencies;

    return j.dump();
}

std::vector<std::string> extractLikelyFiles(const std::string& task)
{
    static const std::regex pathLike(R"(([A-Za-z]:[\\/][^\s\"']+|(?:src[\\/][^\s\"']+\.(?:c|cc|cpp|cxx|h|hpp|hh|json|yaml|yml|md|txt))))",
                                    std::regex::icase);
    std::unordered_set<std::string> dedup;
    std::vector<std::string> out;

    for (auto it = std::sregex_iterator(task.begin(), task.end(), pathLike); it != std::sregex_iterator(); ++it)
    {
        std::string candidate = it->str();
        if (!candidate.empty() && dedup.insert(candidate).second)
        {
            out.push_back(candidate);
            if (out.size() >= 12)
                break;
        }
    }
    return out;
}

void configureHotpatchProfileForTask(const std::string& task, ExecutionPlan& plan)
{
    auto& hp = RawrXD::Agentic::Hotpatch::Engine::instance();
    const std::string lower = toLowerCopy(task);

    const bool highRisk =
        containsAnyToken(lower, {"kernel", "driver", "boot", "registry", "system32", "firmware", "delete", "wipe"});
    const bool investigative =
        containsAnyToken(lower, {"audit", "analyze", "diagnose", "investigate", "review", "trace"});
    const bool editHeavy =
        containsAnyToken(lower, {"implement", "refactor", "rewrite", "patch", "fix", "modify"});

    // Automatic hotpatch strategy selection.
    // High-risk tasks force conservative mode; investigative tasks stay cool and safe;
    // implementation tasks run in balanced mode for practical autonomy.
    if (highRisk)
    {
        hp.setHotpatchingEnabled(true);
        hp.setModelTemperature(0.22);
        hp.setUnrestrictiveDial(0.15);
        plan.confidence_score = 0.66f;
        plan.planner_model = "hotpatch_conservative_v1";
    }
    else if (investigative)
    {
        hp.setHotpatchingEnabled(true);
        hp.setModelTemperature(0.35);
        hp.setUnrestrictiveDial(0.30);
        plan.confidence_score = 0.78f;
        plan.planner_model = "hotpatch_audit_v1";
    }
    else if (editHeavy)
    {
        hp.setHotpatchingEnabled(true);
        hp.setModelTemperature(0.56);
        hp.setUnrestrictiveDial(0.52);
        plan.confidence_score = 0.82f;
        plan.planner_model = "hotpatch_balanced_v1";
    }
    else
    {
        hp.setHotpatchingEnabled(true);
        hp.setModelTemperature(0.45);
        hp.setUnrestrictiveDial(0.40);
        plan.confidence_score = 0.80f;
        plan.planner_model = "hotpatch_default_v1";
    }
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

void OrchestratorIntegration::setContextWindow(size_t tokens)
{
    const size_t previous = m_contextWindowTokens;
    const size_t sanitized = sanitizeContextWindow(tokens);
    m_contextWindowTokens = sanitized;

    if (!m_orchestrator)
    {
        return;
    }

    if (!m_adaptiveContextBudgeting)
    {
        return;
    }

    auto policy = m_orchestrator->getApprovalPolicy();
    policy.min_complexity_for_human_review = deriveComplexityReviewThreshold(sanitized);

    if (sanitized <= 16384)
    {
        policy.allow_partial_execution = false;
        policy.allow_parallel_approvals = false;
        policy.auto_approve_low_risk = false;
    }
    else if (sanitized >= 262144)
    {
        policy.allow_parallel_approvals = true;
    }

    m_orchestrator->setApprovalPolicy(policy);

    if (sanitized != previous)
    {
        LOG_INFO("AgenticOrch", "Context window set to " + std::to_string(sanitized) +
                                   " tokens (adaptive policy tuning applied)");
    }
}

size_t OrchestratorIntegration::getContextWindow() const {
    return m_contextWindowTokens;
}

void OrchestratorIntegration::enableAdaptiveContextBudgeting(bool enable) {
    m_adaptiveContextBudgeting = enable;
}

bool OrchestratorIntegration::isAdaptiveContextBudgetingEnabled() const {
    return m_adaptiveContextBudgeting;
}

void OrchestratorIntegration::initialize()
{
    if (!m_orchestrator || m_initialized)
    {
        return;
    }

    // Wire planner: adaptive planner with automatic hotpatch profile selection.
    m_orchestrator->setPlanGenerationFn(
        [this](const std::string& task) -> ExecutionPlan
        {
            ExecutionPlan plan;
            plan.description = task;
            plan.source_task = task;
            plan.workspace_root = (std::getenv("RAWRXD_REPO_ROOT") ? std::getenv("RAWRXD_REPO_ROOT") : "");
            const std::string lower = toLowerCopy(task);
            const auto likelyFiles = extractLikelyFiles(task);

            configureHotpatchProfileForTask(task, plan);

            if (m_adaptiveContextBudgeting)
            {
                // Context-window-aware confidence shaping. This is applied after
                // hotpatch profile selection so the adaptive budget is additive.
                const size_t current = std::max<size_t>(4096, m_contextWindowTokens);
                const bool highMutationIntent = containsAnyToken(
                    lower, {"implement", "refactor", "rewrite", "patch", "fix", "modify", "edit"});
                const bool diagnosticIntent = containsAnyToken(
                    lower, {"audit", "analyze", "diagnose", "investigate", "review", "trace"});
                const bool systemCriticalIntent = containsAnyToken(
                    lower, {"kernel", "driver", "boot", "registry", "firmware", "system32"});

                size_t wordCount = 0;
                bool inWord = false;
                for (unsigned char c : task)
                {
                    if (std::isspace(c))
                    {
                        inWord = false;
                    }
                    else if (!inWord)
                    {
                        ++wordCount;
                        inWord = true;
                    }
                }

                const size_t complexityScore = wordCount + (likelyFiles.size() * 6) +
                                               (highMutationIntent ? 18 : 0) +
                                               (diagnosticIntent ? 6 : 0) +
                                               (systemCriticalIntent ? 16 : 0);

                float delta = 0.0f;

                // Base window calibration.
                if (current >= 524288)
                    delta += 0.08f;
                else if (current >= 262144)
                    delta += 0.05f;
                else if (current >= 131072)
                    delta += 0.02f;
                else if (current <= 16384)
                    delta -= 0.12f;
                else if (current <= 32768)
                    delta -= 0.08f;
                else if (current <= 65536)
                    delta -= 0.04f;

                // Intent-conditioned corrections.
                if (highMutationIntent && current <= 65536)
                    delta -= 0.05f;
                if (diagnosticIntent && current >= 131072)
                    delta += 0.03f;
                if (systemCriticalIntent && current <= 131072)
                    delta -= 0.06f;

                // Complexity-pressure correction.
                if (complexityScore > 80 && current <= 65536)
                    delta -= 0.05f;
                else if (complexityScore < 24 && current >= 131072)
                    delta += 0.02f;

                plan.confidence_score = std::clamp(plan.confidence_score + delta, 0.45f, 0.95f);
            }

            auto mkStep = [](const std::string& id, const std::string& title, const std::string& desc,
                             bool mutating, StepRisk risk, int etaMs) {
                PlanStep s;
                s.id = id;
                s.title = title;
                s.description = desc;
                s.is_mutating = mutating;
                s.risk_level = risk;
                s.estimated_duration_ms = etaMs;
                return s;
            };

            PlanStep analyze = mkStep("step_1_analyze", "Analyze Scope",
                                      "Parse intent, identify touched files, and derive tool strategy.", false,
                                      StepRisk::VeryLow, 5000);
            analyze.actions = {"semantic_search", "search_code", "read_file"};
            analyze.affected_files = likelyFiles;
            plan.steps.push_back(std::move(analyze));

            const bool needsBuild =
                containsAnyToken(lower, {"build", "compile", "cmake", "ninja", "msbuild", "link"});
            const bool needsTests =
                containsAnyToken(lower, {"test", "verify", "validation", "regression", "smoke"});
            const bool needsFileMutation =
                containsAnyToken(lower, {"fix", "implement", "patch", "edit", "refactor", "rewrite", "change"});

            PlanStep prepare = mkStep("step_2_prepare", "Prepare Execution Context",
                                      "Collect diagnostics and preconditions before applying changes.", false,
                                      StepRisk::Low, 10000);
            prepare.dependencies.push_back("step_1_analyze");
            prepare.actions = {"list_dir", "get_diagnostics"};
            prepare.affected_files = likelyFiles;
            plan.steps.push_back(std::move(prepare));

            if (needsFileMutation)
            {
                PlanStep implement = mkStep("step_3_implement", "Apply Changes",
                                            "Perform focused file edits and capture mutation evidence.", true,
                                            StepRisk::Medium, 25000);
                implement.dependencies.push_back("step_2_prepare");
                implement.actions = {"read_file", "replace_in_file", "write_file"};
                implement.affected_files = likelyFiles.empty() ? std::vector<std::string>{"src/"} : likelyFiles;

                // Escalate risk automatically for system-heavy tasks.
                if (containsAnyToken(lower, {"kernel", "driver", "system", "boot", "registry"}))
                {
                    implement.risk_level = StepRisk::High;
                }
                plan.steps.push_back(std::move(implement));
            }

            if (needsBuild)
            {
                PlanStep build = mkStep("step_4_build", "Build / Compile",
                                        "Run compile/build to validate mutation integrity.", false, StepRisk::Low,
                                        45000);
                build.dependencies.push_back(needsFileMutation ? "step_3_implement" : "step_2_prepare");
                build.actions = {"execute_command"};
                plan.steps.push_back(std::move(build));
            }

            if (needsTests)
            {
                PlanStep test = mkStep("step_5_test", "Run Verification",
                                       "Execute tests or smoke checks and collect failures.", false, StepRisk::Low,
                                       60000);
                test.dependencies.push_back(needsBuild ? "step_4_build"
                                                       : (needsFileMutation ? "step_3_implement" : "step_2_prepare"));
                test.actions = {"execute_command", "get_diagnostics"};
                plan.steps.push_back(std::move(test));
            }

            PlanStep finalize = mkStep("step_9_finalize", "Finalize",
                                       "Summarize outcomes, unresolved risks, and next actions.", false,
                                       StepRisk::VeryLow, 3000);
            if (needsTests)
                finalize.dependencies.push_back("step_5_test");
            else if (needsBuild)
                finalize.dependencies.push_back("step_4_build");
            else if (needsFileMutation)
                finalize.dependencies.push_back("step_3_implement");
            else
                finalize.dependencies.push_back("step_2_prepare");
            finalize.actions = {"read_file"};
            plan.steps.push_back(std::move(finalize));

            int totalMs = 0;
            for (const auto& s : plan.steps)
                totalMs += s.estimated_duration_ms;
            plan.total_estimated_duration_ms = totalMs;

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

    if (step_idx < 0 || static_cast<size_t>(step_idx) >= plan->steps.size())
    {
        return;
    }

    auto& step = plan->steps[step_idx];

    if (!(step.approval_status == ApprovalStatus::Approved ||
          step.approval_status == ApprovalStatus::ApprovedAuto))
    {
        step.status = ExecutionStatus::Failed;
        step.error_message = "Step execution attempted without approval";
        return;
    }

    step.status = ExecutionStatus::Executing;
    step.execution_result.clear();
    step.error_message.clear();

    if (step.actions.empty())
    {
        step.execution_result = "No actions declared for this step";
        step.status = ExecutionStatus::Success;
        return;
    }

    // Execute each action in the step
    for (size_t i = 0; i < step.actions.size(); ++i)
    {
        const auto& action = step.actions[i];
        const std::string args = buildActionArgsEnvelope(*plan, step, step_idx, action);
        std::string output;
        bool ok = false;
        try
        {
            ok = m_toolExecutor(action, args, output);
        }
        catch (const std::exception& ex)
        {
            step.status = ExecutionStatus::Failed;
            step.error_message = "Tool executor exception at action '" + action + "': " + ex.what();
            return;
        }
        catch (...)
        {
            step.status = ExecutionStatus::Failed;
            step.error_message = "Unknown tool executor failure at action '" + action + "'";
            return;
        }

        if (ok)
        {
            step.execution_result += "[" + std::to_string(i + 1) + "/" + std::to_string(step.actions.size()) +
                                     "] " + action + "\n";
            if (!output.empty())
            {
                step.execution_result += output + "\n";
            }
        }
        else
        {
            step.status = ExecutionStatus::Failed;
            step.error_message = "Tool execution failed: " + action;
            if (!output.empty())
            {
                step.error_message += " | details: " + output;
            }
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
