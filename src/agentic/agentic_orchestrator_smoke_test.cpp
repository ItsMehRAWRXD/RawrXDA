// agentic_orchestrator_smoke_test.cpp
// Build & runtime validation for AgenticPlanningOrchestrator

#include "agentic_planning_orchestrator.hpp"
#include "agentic_orchestrator_integration.hpp"
#include <cassert>

using namespace Agentic;

void test_basic_orchestration() {
    AgenticPlanningOrchestrator orchestrator;
    orchestrator.setApprovalPolicy(ApprovalPolicy::Standard());
    
    auto* plan = orchestrator.generatePlanForTask(
        "Add Q8K quantization support to GGUF loader"
    );
    
    assert(plan != nullptr);
    assert(!plan->plan_id.empty());
    assert(!plan->steps.empty());
}

void test_risk_analysis() {
    AgenticPlanningOrchestrator orchestrator;
    
    PlanStep step_read_only;
    step_read_only.is_mutating = false;
    auto risk = orchestrator.analyzeStepRisk(step_read_only);
    assert(risk == StepRisk::VeryLow);
    
    PlanStep step_single_file;
    step_single_file.is_mutating = true;
    step_single_file.affected_files.push_back("file.cpp");
    risk = orchestrator.analyzeStepRisk(step_single_file);
    assert(risk == StepRisk::Low);
    
    PlanStep step_many_files;
    step_many_files.is_mutating = true;
    for (int i = 0; i < 50; ++i) {
        step_many_files.affected_files.push_back("file" + std::to_string(i) + ".cpp");
    }
    risk = orchestrator.analyzeStepRisk(step_many_files);
    assert(risk == StepRisk::Critical);
}

void test_approval_policies() {
    {
        auto policy = ApprovalPolicy::Conservative();
        assert(policy.auto_approve_very_low_risk);
        assert(!policy.auto_approve_low_risk);
    }
    
    {
        auto policy = ApprovalPolicy::Standard();
        assert(policy.auto_approve_very_low_risk);
        assert(!policy.auto_approve_low_risk);
    }
    
    {
        auto policy = ApprovalPolicy::Aggressive();
        assert(policy.auto_approve_very_low_risk);
        assert(policy.auto_approve_low_risk);
    }
}

void test_approval_gates() {
    AgenticPlanningOrchestrator orchestrator;
    orchestrator.setApprovalPolicy(ApprovalPolicy::Standard());
    
    auto* plan = orchestrator.generatePlanForTask("Test task");
    assert(plan != nullptr);
    
    int pending = orchestrator.getPendingApprovalCount();
    (void)pending;
    
    if (!plan->steps.empty()) {
        auto status = orchestrator.requestApproval(plan, 0);
        assert(status == ApprovalStatus::Pending);
        
        orchestrator.approveStep(plan, 0, "test_user", "Approved for testing");
        assert(plan->steps[0].approval_status == ApprovalStatus::Approved);
    }
}

void test_integration() {
    auto& integration = OrchestratorIntegration::instance();
    integration.initialize();
    
    auto* plan = integration.planAndApproveTask("Integration test task");
    assert(plan != nullptr);
    
    int pending = integration.getPendingApprovalCount();
    (void)pending;
}

void test_json_export() {
    AgenticPlanningOrchestrator orchestrator;
    auto* plan = orchestrator.generatePlanForTask("JSON export test");
    
    auto plan_json = orchestrator.getPlanJson(plan);
    assert(!plan_json.is_null());
    assert(plan_json.contains("plan_id"));
    assert(plan_json.contains("steps"));
    
    auto queue_json = orchestrator.getApprovalQueueJson();
    assert(queue_json.is_array());
    
    auto status_json = orchestrator.getExecutionStatusJson();
    assert(status_json.contains("active_plans"));
    assert(status_json.contains("pending_approvals"));
}

int main() {
    test_basic_orchestration();
    test_risk_analysis();
    test_approval_policies();
    test_approval_gates();
    test_integration();
    test_json_export();
    
    return 0;
}
