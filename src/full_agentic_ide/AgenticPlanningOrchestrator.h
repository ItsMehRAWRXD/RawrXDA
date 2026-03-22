#pragma once

// =============================================================================
// AgenticPlanningOrchestrator — unified workspace-aware plan gates + approval
// =============================================================================
// Bridges: parsed AgentPlan (Win32IDE_PlanExecutor) ↔ risk tiers ↔ safety
// denylists ↔ human approval (full vs low-risk-only execution policy).
// Async variant: Uses AgenticIOCPBridge for non-blocking gate evaluation.
// No exceptions; Win32IDE_Types.h is the single source of plan POD shapes.
// =============================================================================

#include "../win32app/Win32IDE_Types.h"

#include <string>
#include <functional>

namespace full_agentic_ide {

struct PlanGateStatistics {
    int autoApproved     = 0;
    int awaitingHuman    = 0;
    int safetyBlocked    = 0;
    int skippedByPolicy  = 0;
};

class AgenticPlanningOrchestrator {
public:
    /// After parsing model output into steps: assign riskTier + initial mutationGate.
    static void classifyPlan(AgentPlan& plan);

    /// Workspace containment + shell denylist. Mutates mutationGate / gateDetail.
    static void applyWorkspaceSafetyGates(AgentPlan& plan, const std::string& workspaceRoot);

    static PlanGateStatistics gateStatistics(const AgentPlan& plan);
    static std::string formatGateSummary(const AgentPlan& plan);

    /// Full "Approve" in dialog: all non-blocked awaiting steps become runnable.
    static void approveAllPendingMutations(AgentPlan& plan);

    /// "Approve low-risk only": Low tier & not blocked → HumanApproved; others → SkippedByPolicy.
    static void approveLowRiskOnly(AgentPlan& plan);

    static bool stepShouldExecute(const AgentPlan& plan, int stepIndex);

    /// Short label for ListView / logs
    static const char* mutationGateLabel(PlanMutationGate g);

    // =========================================================================
    // ASYNC GATEWAY (IOCP-backed): Evaluate gates without blocking inference
    // =========================================================================

    /// Queue plan for async gate evaluation (called from inference thread)
    /// onApprovalDecision: invoked on worker thread with (stepId, approved) result
    static bool queueAsyncGateEvaluation(
        const AgentPlan& plan,
        int stepIndex,
        const std::string& workspaceRoot,
        std::function<void(int stepIndex, bool approved)> onApprovalDecision);

    /// Initialize async bridge (call once at startup)
    static bool initializeAsyncBridge(int numWorkerThreads = 4);

    /// Shutdown async bridge (call at shutdown)
    static bool shutdownAsyncBridge();

    /// Check if async bridge is active
    static bool isAsyncBridgeActive();

    /// Statistics for async pipeline
    static int getAsyncPendingCount();
};

} // namespace full_agentic_ide
