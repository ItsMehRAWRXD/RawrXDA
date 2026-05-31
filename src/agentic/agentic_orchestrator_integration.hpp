// agentic_orchestrator_integration.hpp
// Singleton integration point: wires AgenticPlanningOrchestrator into RawrXD IDE

#pragma once

#include "agentic_planning_orchestrator.hpp"
#include <memory>
#include <string>
#include <functional>

namespace Agentic {

// ============================================================================
// Global Orchestrator Singleton
// ============================================================================

class OrchestratorIntegration {
public:
    static OrchestratorIntegration& instance() {
        static OrchestratorIntegration s_instance;
        return s_instance;
    }
    
    // Initialize orchestrator with callbacks wired to IDE subsystems
    void initialize();
    
    // Main entry point: user task → plan → approval → execution
    ExecutionPlan* planAndApproveTask(const std::string& task_description);
    
    // Plan execution control
    bool executePlanStep(ExecutionPlan* plan, int step_idx);
    bool executeEntirePlan(ExecutionPlan* plan);
    void cancelPlan(ExecutionPlan* plan);
    void pausePlan(ExecutionPlan* plan);
    void resumePlan(ExecutionPlan* plan);
    
    // Direct getter
    AgenticPlanningOrchestrator* getOrchestrator() { return m_orchestrator.get(); }
    
    // Callbacks: wire to IDE systems
    using ToolExecutorFn = std::function<bool(const std::string& tool_name, 
                                              const std::string& args,
                                              std::string& output)>;
    using RiskAnalyzerFn = std::function<StepRisk(const PlanStep&)>;
    using RollbackExecutorFn = std::function<void(const PlanStep&)>;
    
    // Lifecycle event callbacks for IDE integration
    using PlanGeneratedFn = std::function<void(const ExecutionPlan& plan)>;
    using PlanExecutionStartedFn = std::function<void(const ExecutionPlan& plan)>;
    using PlanCompletedFn = std::function<void(const ExecutionPlan& plan)>;
    using PlanFailedFn = std::function<void(const ExecutionPlan& plan, const std::string& reason)>;
    using PlanCancelledFn = std::function<void(const ExecutionPlan& plan)>;
    using PlanPausedFn = std::function<void(const ExecutionPlan& plan)>;
    using PlanResumedFn = std::function<void(const ExecutionPlan& plan)>;
    using StepExecutionStartedFn = std::function<void(const ExecutionPlan& plan, int step_idx)>;
    using StepCompletedFn = std::function<void(const ExecutionPlan& plan, int step_idx)>;
    using StepFailedFn = std::function<void(const ExecutionPlan& plan, int step_idx, const std::string& error)>;
    using StepRolledBackFn = std::function<void(const ExecutionPlan& plan, int step_idx)>;
    using ApprovalRequiredFn = std::function<void(const ExecutionPlan& plan, int step_idx)>;
    using StepApprovedFn = std::function<void(const ExecutionPlan& plan, int step_idx)>;
    using LogMessageFn = std::function<void(const std::string& message)>;
    
    void setToolExecutor(ToolExecutorFn fn) { m_toolExecutor = fn; }
    void setRiskAnalyzer(RiskAnalyzerFn fn) { m_riskAnalyzer = fn; }
    void setRollbackExecutor(RollbackExecutorFn fn) { m_rollbackExecutor = fn; }
    
    void setOnPlanGenerated(PlanGeneratedFn fn) { m_onPlanGenerated = fn; }
    void setOnPlanExecutionStarted(PlanExecutionStartedFn fn) { m_onPlanExecutionStarted = fn; }
    void setOnPlanCompleted(PlanCompletedFn fn) { m_onPlanCompleted = fn; }
    void setOnPlanFailed(PlanFailedFn fn) { m_onPlanFailed = fn; }
    void setOnPlanCancelled(PlanCancelledFn fn) { m_onPlanCancelled = fn; }
    void setOnPlanPaused(PlanPausedFn fn) { m_onPlanPaused = fn; }
    void setOnPlanResumed(PlanResumedFn fn) { m_onPlanResumed = fn; }
    void setOnStepExecutionStarted(StepExecutionStartedFn fn) { m_onStepExecutionStarted = fn; }
    void setOnStepCompleted(StepCompletedFn fn) { m_onStepCompleted = fn; }
    void setOnStepFailed(StepFailedFn fn) { m_onStepFailed = fn; }
    void setOnStepRolledBack(StepRolledBackFn fn) { m_onStepRolledBack = fn; }
    void setOnApprovalRequired(ApprovalRequiredFn fn) { m_onApprovalRequired = fn; }
    void setOnStepApproved(StepApprovedFn fn) { m_onStepApproved = fn; }
    void setOnLogMessage(LogMessageFn fn) { m_onLogMessage = fn; }
    
    // Status queries
    int getPendingApprovalCount() const;
    std::vector<std::pair<ExecutionPlan*, int>> getPendingApprovals() const;
    
private:
    OrchestratorIntegration();
    ~OrchestratorIntegration();
    OrchestratorIntegration(const OrchestratorIntegration&) = delete;
    OrchestratorIntegration& operator=(const OrchestratorIntegration&) = delete;
    
    std::unique_ptr<AgenticPlanningOrchestrator> m_orchestrator;
    bool m_initialized = false;

    ToolExecutorFn m_toolExecutor;
    RiskAnalyzerFn m_riskAnalyzer;
    RollbackExecutorFn m_rollbackExecutor;
    
    // Lifecycle event callbacks
    PlanGeneratedFn m_onPlanGenerated;
    PlanExecutionStartedFn m_onPlanExecutionStarted;
    PlanCompletedFn m_onPlanCompleted;
    PlanFailedFn m_onPlanFailed;
    PlanCancelledFn m_onPlanCancelled;
    PlanPausedFn m_onPlanPaused;
    PlanResumedFn m_onPlanResumed;
    StepExecutionStartedFn m_onStepExecutionStarted;
    StepCompletedFn m_onStepCompleted;
    StepFailedFn m_onStepFailed;
    StepRolledBackFn m_onStepRolledBack;
    ApprovalRequiredFn m_onApprovalRequired;
    StepApprovedFn m_onStepApproved;
    LogMessageFn m_onLogMessage;
    
    // Internal callbacks
    void onPlanGeneration(const std::string& task, ExecutionPlan& plan);
    void onStepExecution(ExecutionPlan* plan, int step_idx);
    void onRollbackRequest(ExecutionPlan* plan, int step_idx);
};

} // namespace Agentic

// ============================================================================
// Convenience Macros for IDE Code
// ============================================================================

#define AGENTIC_PLAN_TASK(task_desc) \
    Agentic::OrchestratorIntegration::instance().planAndApproveTask(task_desc)

#define AGENTIC_GET_PENDING_COUNT() \
    Agentic::OrchestratorIntegration::instance().getPendingApprovalCount()

#define AGENTIC_GET_ORCHESTRATOR() \
    Agentic::OrchestratorIntegration::instance().getOrchestrator()
