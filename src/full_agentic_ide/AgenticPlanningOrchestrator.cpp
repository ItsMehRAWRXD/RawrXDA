// =============================================================================
// AgenticPlanningOrchestrator — implementation
// =============================================================================

#include <windows.h>

#include "AgenticIOCPBridge.hpp"
#include "AgenticPlanningOrchestrator.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

namespace full_agentic_ide
{

namespace
{

std::string toLower(std::string s)
{
    for (char& c : s)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}

PlanRiskTier riskTierFromString(const std::string& riskField)
{
    std::string r = toLower(riskField);
    if (r.find("crit") != std::string::npos)
        return PlanRiskTier::Critical;
    if (r.find("high") != std::string::npos)
        return PlanRiskTier::High;
    if (r.find("med") != std::string::npos)
        return PlanRiskTier::Medium;
    if (r.find("low") != std::string::npos)
        return PlanRiskTier::Low;
    return PlanRiskTier::Unknown;
}

void bumpTier(PlanRiskTier& t)
{
    switch (t)
    {
        case PlanRiskTier::Unknown:
            t = PlanRiskTier::Medium;
            break;
        case PlanRiskTier::Low:
            t = PlanRiskTier::Medium;
            break;
        case PlanRiskTier::Medium:
            t = PlanRiskTier::High;
            break;
        case PlanRiskTier::High:
            t = PlanRiskTier::Critical;
            break;
        default:
            break;
    }
}

bool isReadOnlyPlanType(PlanStepType ty)
{
    return ty == PlanStepType::Analysis || ty == PlanStepType::Verification;
}

bool shellCommandBlocked(const std::string& desc)
{
    std::string d = toLower(desc);
    static const char* bad[] = {"format ",
                                "del /f",
                                "del /q",
                                "rmdir /s",
                                "rm -rf",
                                "rm -fr",
                                "remove-item -recurse",
                                "remove-item -r ",
                                "cipher /w",
                                "diskpart",
                                "mkfs",
                                ":(){",
                                "shutdown ",
                                "restart-computer",
                                nullptr};
    for (int i = 0; bad[i]; ++i)
    {
        if (d.find(bad[i]) != std::string::npos)
            return true;
    }
    return false;
}

#ifdef _WIN32
bool pathHasIllegalTraversal(const std::string& p)
{
    std::string s = toLower(p);
    return s.find("..") != std::string::npos;
}

bool pathUnderWorkspace(const std::string& filePath, const std::string& workspaceRoot)
{
    if (filePath.empty())
        return true;
    if (pathHasIllegalTraversal(filePath))
        return false;

    std::error_code ec;
    fs::path wsPath = workspaceRoot.empty() ? fs::current_path(ec) : fs::path(workspaceRoot);
    if (ec)
        wsPath = fs::path(".");

    fs::path candidate(filePath);
    if (candidate.is_relative())
        candidate = wsPath / candidate;

    fs::path wsCanon = fs::weakly_canonical(wsPath, ec);
    if (ec)
    {
        ec.clear();
        wsCanon = fs::absolute(wsPath, ec);
    }
    fs::path fileCanon = fs::weakly_canonical(candidate, ec);
    if (ec)
    {
        ec.clear();
        fileCanon = fs::absolute(candidate, ec);
    }

    std::string wsS = wsCanon.generic_string();
    std::string fS = fileCanon.generic_string();
    if (wsS.empty())
        return true;

    std::string wsL = toLower(wsS);
    std::string fL = toLower(fS);
    if (fL.size() < wsL.size())
        return false;
    if (fL.compare(0, wsL.size(), wsL) != 0)
        return false;
    if (fL.size() == wsL.size())
        return true;
    char next = fL[wsL.size()];
    return next == '/' || next == '\\';
}
#else
bool pathUnderWorkspace(const std::string& filePath, const std::string& workspaceRoot)
{
    if (filePath.empty())
        return true;
    if (filePath.find("..") != std::string::npos)
        return false;
    std::error_code ec;
    fs::path wsPath = workspaceRoot.empty() ? fs::current_path(ec) : fs::path(workspaceRoot);
    fs::path candidate(filePath);
    if (candidate.is_relative())
        candidate = wsPath / candidate;
    fs::path wsCanon = fs::weakly_canonical(wsPath, ec);
    fs::path fileCanon = fs::weakly_canonical(candidate, ec);
    auto w = wsCanon.generic_string();
    auto f = fileCanon.generic_string();
    return f.size() >= w.size() && f.compare(0, w.size(), w) == 0;
}
#endif

}  // namespace

void AgenticPlanningOrchestrator::classifyPlan(AgentPlan& plan)
{
    for (auto& step : plan.steps)
    {
        step.riskTier = riskTierFromString(step.risk);
        if (step.riskTier == PlanRiskTier::Unknown)
            step.riskTier = PlanRiskTier::Medium;

        if (step.type == PlanStepType::FileDelete || step.type == PlanStepType::ShellCommand)
            bumpTier(step.riskTier);
        if (step.type == PlanStepType::FileCreate || step.type == PlanStepType::CodeEdit)
        {
            if (step.riskTier == PlanRiskTier::Low)
                bumpTier(step.riskTier);
        }

        step.mutationGate = PlanMutationGate::PendingClassification;
        step.gateDetail.clear();

        if (step.riskTier == PlanRiskTier::Critical)
        {
            step.mutationGate = PlanMutationGate::AwaitingHuman;
            step.gateDetail = "Critical tier — requires explicit plan approval.";
        }
        else if (step.riskTier == PlanRiskTier::Low && isReadOnlyPlanType(step.type))
        {
            step.mutationGate = PlanMutationGate::AutoApproved;
            step.gateDetail = "Low-risk read-only — auto-eligible.";
        }
        else
        {
            step.mutationGate = PlanMutationGate::AwaitingHuman;
            step.gateDetail = "Awaiting plan approval.";
        }
    }
}

void AgenticPlanningOrchestrator::applyWorkspaceSafetyGates(AgentPlan& plan, const std::string& workspaceRoot)
{
    for (auto& step : plan.steps)
    {
        if (step.mutationGate == PlanMutationGate::SafetyBlocked)
            continue;

        if (step.type == PlanStepType::ShellCommand)
        {
            const std::string& probe = step.description.empty() ? step.title : step.description;
            if (shellCommandBlocked(probe))
            {
                step.mutationGate = PlanMutationGate::SafetyBlocked;
                step.gateDetail = "Shell policy: blocked pattern in description/title.";
                continue;
            }
        }

        if (!step.targetFile.empty())
        {
            if (!pathUnderWorkspace(step.targetFile, workspaceRoot))
            {
                step.mutationGate = PlanMutationGate::SafetyBlocked;
                step.gateDetail = "Path outside workspace or unsafe traversal.";
            }
        }
    }
}

PlanGateStatistics AgenticPlanningOrchestrator::gateStatistics(const AgentPlan& plan)
{
    PlanGateStatistics s{};
    for (const auto& step : plan.steps)
    {
        switch (step.mutationGate)
        {
            case PlanMutationGate::AutoApproved:
                ++s.autoApproved;
                break;
            case PlanMutationGate::AwaitingHuman:
                ++s.awaitingHuman;
                break;
            case PlanMutationGate::SafetyBlocked:
                ++s.safetyBlocked;
                break;
            case PlanMutationGate::SkippedByPolicy:
                ++s.skippedByPolicy;
                break;
            default:
                break;
        }
    }
    return s;
}

std::string AgenticPlanningOrchestrator::formatGateSummary(const AgentPlan& plan)
{
    PlanGateStatistics s = gateStatistics(plan);
    std::ostringstream o;
    o << "Gates: " << s.autoApproved << " auto, " << s.awaitingHuman << " review, " << s.safetyBlocked << " blocked";
    if (s.skippedByPolicy)
        o << ", " << s.skippedByPolicy << " skipped-by-policy";
    return o.str();
}

void AgenticPlanningOrchestrator::approveAllPendingMutations(AgentPlan& plan)
{
    for (auto& step : plan.steps)
    {
        if (step.mutationGate == PlanMutationGate::SafetyBlocked)
            continue;
        if (step.mutationGate == PlanMutationGate::SkippedByPolicy)
            continue;
        if (step.mutationGate == PlanMutationGate::AwaitingHuman)
            step.mutationGate = PlanMutationGate::HumanApproved;
    }
}

void AgenticPlanningOrchestrator::approveLowRiskOnly(AgentPlan& plan)
{
    for (auto& step : plan.steps)
    {
        if (step.mutationGate == PlanMutationGate::SafetyBlocked)
            continue;

        if (step.riskTier == PlanRiskTier::Low)
        {
            if (step.mutationGate == PlanMutationGate::AwaitingHuman ||
                step.mutationGate == PlanMutationGate::AutoApproved)
                step.mutationGate = PlanMutationGate::HumanApproved;
        }
        else
        {
            if (step.mutationGate != PlanMutationGate::SafetyBlocked)
                step.mutationGate = PlanMutationGate::SkippedByPolicy;
        }
    }
}

bool AgenticPlanningOrchestrator::stepShouldExecute(const AgentPlan& plan, int stepIndex)
{
    if (stepIndex < 0 || stepIndex >= static_cast<int>(plan.steps.size()))
        return false;
    const PlanStep& s = plan.steps[stepIndex];

    if (s.mutationGate == PlanMutationGate::SafetyBlocked)
        return false;
    if (s.mutationGate == PlanMutationGate::SkippedByPolicy)
        return false;
    if (s.mutationGate == PlanMutationGate::HumanRejected)
        return false;

    if (plan.executionPolicy == PlanExecutionPolicy::SafeStepsOnly)
    {
        if (s.riskTier != PlanRiskTier::Low)
            return false;
    }

    return s.mutationGate == PlanMutationGate::AutoApproved || s.mutationGate == PlanMutationGate::HumanApproved;
}

const char* AgenticPlanningOrchestrator::mutationGateLabel(PlanMutationGate g)
{
    switch (g)
    {
        case PlanMutationGate::PendingClassification:
            return "pending";
        case PlanMutationGate::AutoApproved:
            return "auto";
        case PlanMutationGate::AwaitingHuman:
            return "review";
        case PlanMutationGate::HumanApproved:
            return "ok";
        case PlanMutationGate::HumanRejected:
            return "reject";
        case PlanMutationGate::SafetyBlocked:
            return "BLOCK";
        case PlanMutationGate::SkippedByPolicy:
            return "skip";
        default:
            return "?";
    }
}

// ============================================================================
// ASYNC GATEWAY (IOCP-backed) Implementation
// ============================================================================

static std::atomic<bool> g_async_bridge_initialized{false};

bool AgenticPlanningOrchestrator::initializeAsyncBridge(int numWorkerThreads)
{
    if (g_async_bridge_initialized.exchange(true))
    {
        return false;  // Already initialized
    }
    return AgenticIOCPBridge::initialize(numWorkerThreads);
}

bool AgenticPlanningOrchestrator::shutdownAsyncBridge()
{
    if (!g_async_bridge_initialized.load())
    {
        return false;  // Not initialized
    }
    bool result = AgenticIOCPBridge::shutdown();
    g_async_bridge_initialized.store(false);
    return result;
}

bool AgenticPlanningOrchestrator::isAsyncBridgeActive()
{
    return g_async_bridge_initialized.load() && AgenticIOCPBridge::isActive();
}

int AgenticPlanningOrchestrator::getAsyncPendingCount()
{
    if (!isAsyncBridgeActive())
    {
        return 0;
    }
    return AgenticIOCPBridge::getPendingCount();
}

bool AgenticPlanningOrchestrator::queueAsyncGateEvaluation(
    const AgentPlan& plan, int stepIndex, const std::string& workspaceRoot,
    std::function<void(int stepIndex, bool approved)> onApprovalDecision)
{

    if (!isAsyncBridgeActive())
    {
        return false;  // Bridge not initialized
    }

    if (stepIndex < 0 || stepIndex >= static_cast<int>(plan.steps.size()))
    {
        return false;
    }

    const PlanStep& step = plan.steps[stepIndex];

    // ========================================================================
    // Synchronous pre-evaluation (fast-path for clearly blocked/approved steps)
    // ========================================================================

    if (step.mutationGate == PlanMutationGate::SafetyBlocked)
    {
        // Already blocked; no async evaluation needed
        if (onApprovalDecision)
        {
            onApprovalDecision(stepIndex, false);
        }
        return true;
    }

    if (step.riskTier == PlanRiskTier::Low && isReadOnlyPlanType(step.type))
    {
        // Low-risk read-only: auto-approve immediately (no async needed)
        if (onApprovalDecision)
        {
            onApprovalDecision(stepIndex, true);
        }
        return true;
    }

    // ========================================================================
    // Async evaluation for uncertain/high-risk steps
    // ========================================================================

    IOCPWorkItem workItem{};
    workItem.planStepId = stepIndex;
    workItem.riskTier = static_cast<uint32_t>(step.riskTier);
    workItem.mutationGate = static_cast<uint32_t>(step.mutationGate);
    workItem.userData = nullptr;

    auto approvalCallback = [onApprovalDecision, stepIndex](const IOCPWorkItem& item, bool approved)
    {
        if (onApprovalDecision)
        {
            onApprovalDecision(stepIndex, approved);
        }
    };

    return AgenticIOCPBridge::queueApprovalRequest(workItem, approvalCallback);
}

}  // namespace full_agentic_ide
