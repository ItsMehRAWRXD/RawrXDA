// Complete Integration Example: autonomous_validation_layer_demo.cpp
// This demonstrates the full six-phase orchestration loop with the validation layer
// integrated into a concrete, runnable example.

#include "autonomous_validation_layer.h"
#include "autonomous_validation_integration.h"
#include <iostream>
#include <fstream>
#include <vector>

namespace RawrXD {
namespace Autonomous {
namespace Demo {

// ─────────────────────────────────────────────────────────────────────────
// Simulate a Real Autonomous Goal: "Fix Broken Build"
// ─────────────────────────────────────────────────────────────────────────

class DemoOrchestrator {
private:
    OrchestratorIntegrationAdapter adapter_;
    std::string workspace_root_;
    std::vector<std::string> workspace_files_;
    int issue_count_ = 5;  // Simulated workspace issues

public:
    DemoOrchestrator(const std::string& workspace) : workspace_root_(workspace) {}

    // Simulate workspace file system
    void initializeWorkspace() {
        workspace_files_ = {
            workspace_root_ + "/main.cpp",
            workspace_root_ + "/build.h",
            workspace_root_ + "/test.cpp"
        };
    }

    // Phase 1: Initialize Goal
    void phase1_initialize(const std::string& goal) {
        std::cout << "\n[PHASE 1] INITIALIZE GOAL\n";
        std::cout << "  Goal: " << goal << "\n";
        std::cout << "  Workspace: " << workspace_root_ << "\n";
        
        adapter_.initializeGoal(goal, workspace_root_);
        std::cout << "  ✓ Baseline recorded\n";
    }

    // Phase 2: Plan Strategy
    void phase2_planStrategy(const std::string& goal) {
        std::cout << "\n[PHASE 2] PLAN STRATEGY\n";
        
        auto strategy = adapter_.getInitialStrategy(goal);
        std::string strategy_name = 
            (strategy.strategy == Strategy::DEBUG) ? "DEBUG" :
            (strategy.strategy == Strategy::REFACTOR) ? "REFACTOR" :
            (strategy.strategy == Strategy::ENUMERATE) ? "ENUMERATE" : "UNKNOWN";
        
        std::cout << "  Strategy: " << strategy_name << "\n";
        std::cout << "  Rationale: " << strategy.rationale << "\n";
        std::cout << "  Max iterations: " << strategy.max_iterations << "\n";
        
        auto tasks = adapter_.getTasksForStrategy(strategy);
        std::cout << "  Tasks planned: " << tasks.size() << "\n";
        for (size_t i = 0; i < tasks.size(); ++i) {
            std::cout << "    [" << i << "] " << tasks[i].action << "\n";
        }
    }

    // Phase 3: Execute & Validate
    void phase3_executeAndValidate() {
        std::cout << "\n[PHASE 3] EXECUTE & VALIDATE\n";
        
        // Create a step with validation contract
        PlanStep step;
        step.id = "compile_step_0";
        step.action = "Fix compilation error in main.cpp";
        step.expected_outcome = "Code compiles without errors";
        step.validations.push_back({
            ValidationSpec::FILE_EXISTS,
            workspace_root_ + "/main.cpp",
            ""
        });
        
        std::cout << "  Executing: " << step.action << "\n";
        std::cout << "  Expected: " << step.expected_outcome << "\n";
        
        // Simulate execution
        std::vector<std::string> failures;
        bool valid = adapter_.validateStepOutcome(step, failures);
        
        if (valid) {
            std::cout << "  ✓ Validation PASSED\n";
            issue_count_--;  // Simulate fixing an issue
        } else {
            std::cout << "  ✗ Validation FAILED\n";
            for (const auto& fail : failures) {
                std::cout << "    - " << fail << "\n";
            }
        }
    }

    // Phase 4: Classify Failure & Route
    void phase4_classifyAndRoute() {
        std::cout << "\n[PHASE 4] CLASSIFY FAILURE & ROUTE\n";
        
        // Simulate an error output from failed execution
        std::string error_output = "Error: undefined reference to 'buildModule()'\n"
                                   "Compilation failed with 3 errors";
        
        PlanStep step;
        step.id = "compile_step_0";
        
        FailureType failure_type = adapter_.classifyStepFailure(step, error_output, "");
        std::string failure_name =
            (failure_type == FailureType::TRANSIENT) ? "TRANSIENT" :
            (failure_type == FailureType::LOGIC_ERROR) ? "LOGIC_ERROR" :
            (failure_type == FailureType::TOOL_FAILURE) ? "TOOL_FAILURE" :
            (failure_type == FailureType::INVALID_PLAN) ? "INVALID_PLAN" :
            (failure_type == FailureType::ENVIRONMENT) ? "ENVIRONMENT" : "UNKNOWN";
        
        std::cout << "  Error: " << error_output << "\n";
        std::cout << "  Classification: " << failure_name << "\n";
        
        std::string strategy = adapter_.getRetryStrategy(failure_type);
        std::cout << "  Retry Strategy: " << strategy << "\n";
    }

    // Phase 5: Record & Adapt
    void phase5_recordAndAdapt() {
        std::cout << "\n[PHASE 5] RECORD & ADAPT\n";
        
        PlanStep step;
        step.id = "compile_step_0";
        step.action = "Fix compilation error";
        
        // Record a failed attempt
        adapter_.recordAttempt(
            step,
            FailureType::TOOL_FAILURE,
            "Compilation failed: undefined reference to 'buildModule()'",
            false,  // not resolved
            5,      // issues before
            5       // issues after (no progress)
        );
        
        std::cout << "  Attempt recorded:\n";
        std::cout << "    - Failure Type: TOOL_FAILURE\n";
        std::cout << "    - Issues Before: 5\n";
        std::cout << "    - Issues After: 5\n";
        std::cout << "    - Resolved: No\n";
        
        auto history = adapter_.getAttemptHistory();
        std::cout << "  Total Attempts: " << history.size() << "\n";
    }

    // Phase 6: Check Termination
    void phase6_checkTermination() {
        std::cout << "\n[PHASE 6] CHECK TERMINATION\n";
        
        bool should_continue = adapter_.shouldContinue(issue_count_);
        bool in_loop = adapter_.isInLoop();
        std::string explanation = adapter_.getTerminationExplanation(issue_count_);
        
        std::cout << "  Current Issue Count: " << issue_count_ << "\n";
        std::cout << "  Should Continue: " << (should_continue ? "YES" : "NO") << "\n";
        std::cout << "  In Loop: " << (in_loop ? "YES (escalate to human)" : "NO") << "\n";
        std::cout << "  Explanation: " << explanation << "\n";
    }

    // Full Workflow
    void runFullDemo() {
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "AUTONOMOUS VALIDATION LAYER DEMO\n";
        std::cout << "Six-Phase Orchestration Loop Integration\n";
        std::cout << std::string(70, '=') << "\n";
        
        initializeWorkspace();
        
        std::string goal = "Fix broken compilation in workspace";
        
        phase1_initialize(goal);
        phase2_planStrategy(goal);
        phase3_executeAndValidate();
        phase4_classifyAndRoute();
        phase5_recordAndAdapt();
        phase6_checkTermination();
        
        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "DEMO COMPLETE\n";
        std::cout << std::string(70, '=') << "\n";
    }
};

} // namespace Demo
} // namespace Autonomous
} // namespace RawrXD

// ─────────────────────────────────────────────────────────────────────────
// Main Entry Point
// ─────────────────────────────────────────────────────────────────────────

int main() {
    try {
        RawrXD::Autonomous::Demo::DemoOrchestrator demo("./test_workspace");
        demo.runFullDemo();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
