// agentic_orchestrator_smoke_test.cpp
// Build & runtime validation for AgenticPlanningOrchestrator

#include "agentic_planning_orchestrator.hpp"
#include "agentic_orchestrator_integration.hpp"
#include <iostream>
#include <cassert>

using namespace Agentic;

void test_basic_orchestration() {
    std::cout << "=== TEST: Basic Orchestration ===\n";
    
    AgenticPlanningOrchestrator orchestrator;
    orchestrator.setApprovalPolicy(ApprovalPolicy::Standard());
    
    // Generate a plan
    auto* plan = orchestrator.generatePlanForTask(
        "Add Q8K quantization support to GGUF loader"
    );
    
    assert(plan != nullptr);
    assert(!plan->plan_id.empty());
    assert(!plan->steps.empty());
    
    std::cout << "✓ Plan generated: " << plan->plan_id << "\n";
    std::cout << "✓ Steps: " << plan->steps.size() << "\n";
    std::cout << "✓ Pending approvals: " << plan->pending_approvals << "\n";
}

void test_risk_analysis() {
    std::cout << "\n=== TEST: Risk Analysis ===\n";
    
    AgenticPlanningOrchestrator orchestrator;
    
    // Test various risk scenarios
    PlanStep step_read_only;
    step_read_only.is_mutating = false;
    auto risk = orchestrator.analyzeStepRisk(step_read_only);
    assert(risk == StepRisk::VeryLow);
    std::cout << "✓ Read-only step → VeryLow risk\n";
    
    PlanStep step_single_file;
    step_single_file.is_mutating = true;
    step_single_file.affected_files.push_back("file.cpp");
    risk = orchestrator.analyzeStepRisk(step_single_file);
    assert(risk == StepRisk::Low);
    std::cout << "✓ Single-file mutation → Low risk\n";
    
    PlanStep step_many_files;
    step_many_files.is_mutating = true;
    for (int i = 0; i < 50; ++i) {
        step_many_files.affected_files.push_back("file" + std::to_string(i) + ".cpp");
    }
    risk = orchestrator.analyzeStepRisk(step_many_files);
    assert(risk == StepRisk::Critical);
    std::cout << "✓ 50-file mutation → Critical risk\n";
}

void test_approval_policies() {
    std::cout << "\n=== TEST: Approval Policies ===\n";
    
    // Test Conservative
    {
        auto policy = ApprovalPolicy::Conservative();
        assert(policy.auto_approve_very_low_risk);
        assert(!policy.auto_approve_low_risk);
        std::cout << "✓ Conservative policy: strict, only auto-approve VeryLow\n";
    }
    
    // Test Standard
    {
        auto policy = ApprovalPolicy::Standard();
        assert(policy.auto_approve_very_low_risk);
        assert(!policy.auto_approve_low_risk);
        std::cout << "✓ Standard policy: moderate approval\n";
    }
    
    // Test Aggressive
    {
        auto policy = ApprovalPolicy::Aggressive();
        assert(policy.auto_approve_very_low_risk);
        assert(policy.auto_approve_low_risk);
        std::cout << "✓ Aggressive policy: lenient, auto-approve VeryLow+Low\n";
    }
}

void test_approval_gates() {
    std::cout << "\n=== TEST: Approval Gates ===\n";
    
    AgenticPlanningOrchestrator orchestrator;
    orchestrator.setApprovalPolicy(ApprovalPolicy::Standard());
    
    auto* plan = orchestrator.generatePlanForTask("Test task");
    assert(plan != nullptr);
    
    // Check pending approvals
    int pending = orchestrator.getPendingApprovalCount();
    std::cout << "✓ Pending approvals in queue: " << pending << "\n";
    
    // Request approval for a step
    if (!plan->steps.empty()) {
        auto status = orchestrator.requestApproval(plan, 0);
        assert(status == ApprovalStatus::Pending);
        std::cout << "✓ Approval request queued\n";
        
        // Approve the step
        orchestrator.approveStep(plan, 0, "test_user", "Approved for testing");
        assert(plan->steps[0].approval_status == ApprovalStatus::Approved);
        std::cout << "✓ Step approved by test_user\n";
    }
}

void test_integration() {
    std::cout << "\n=== TEST: Integration Singleton ===\n";
    
    auto& integration = OrchestratorIntegration::instance();
    integration.initialize();
    
    auto* plan = integration.planAndApproveTask("Integration test task");
    assert(plan != nullptr);
    std::cout << "✓ Integration single ton initialized\n";
    std::cout << "✓ Plan created through integration layer\n";
    
    int pending = integration.getPendingApprovalCount();
    std::cout << "✓ Pending approvals: " << pending << "\n";
}

void test_json_export() {
    std::cout << "\n=== TEST: JSON Export ===\n";
    
    AgenticPlanningOrchestrator orchestrator;
    auto* plan = orchestrator.generatePlanForTask("JSON export test");
    
    auto plan_json = orchestrator.getPlanJson(plan);
    assert(!plan_json.is_null());
    assert(plan_json.contains("plan_id"));
    assert(plan_json.contains("steps"));
    std::cout << "✓ Plan exported to JSON\n";
    
    auto queue_json = orchestrator.getApprovalQueueJson();
    assert(queue_json.is_array());
    std::cout << "✓ Approval queue exported to JSON\n";
    
    auto status_json = orchestrator.getExecutionStatusJson();
    assert(status_json.contains("active_plans"));
    assert(status_json.contains("pending_approvals"));
    std::cout << "✓ Execution status exported to JSON\n";
}

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║  AgenticPlanningOrchestrator Smoke Tests               ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n";
    
    try {
        test_basic_orchestration();
        test_risk_analysis();
        test_approval_policies();
        test_approval_gates();
        test_integration();
        test_json_export();
        
        std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
        std::cout << "║  ✓ ALL TESTS PASSED                                    ║\n";
        std::cout << "╚════════════════════════════════════════════════════════╝\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << "\n";
        return 1;
    }
}
