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
    
    // Direct getter
    AgenticPlanningOrchestrator* getOrchestrator() { return m_orchestrator.get(); }
    
    // Callbacks: wire to IDE systems
    using ToolExecutorFn = std::function<bool(const std::string& tool_name, 
                                              const std::string& args,
                                              std::string& output)>;
    using RiskAnalyzerFn = std::function<StepRisk(const PlanStep&)>;
    using RollbackExecutorFn = std::function<void(const PlanStep&)>;
    
    void setToolExecutor(ToolExecutorFn fn) { m_toolExecutor = fn; }
    void setRiskAnalyzer(RiskAnalyzerFn fn) { m_riskAnalyzer = fn; }
    void setRollbackExecutor(RollbackExecutorFn fn) { m_rollbackExecutor = fn; }
    
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
