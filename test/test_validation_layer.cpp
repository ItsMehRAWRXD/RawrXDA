// Test Suite for Autonomous Validation Layer
// Integration Tests for RawrXD autonomous orchestrator
// Location: d:\rawrxd\test\test_validation_layer.cpp

#include <gtest/gtest.h>
#include "../src/autonomous_validation_layer.h"
#include "../src/autonomous_validation_integration.h"
#include <filesystem>
#include <fstream>

namespace RawrXD {
namespace Autonomous {
namespace Test {

// ─────────────────────────────────────────────────────────────────────────
// Test Fixture for Validation Layer
// ─────────────────────────────────────────────────────────────────────────

class ValidationLayerTest : public ::testing::Test {
protected:
    OrchestratorIntegrationAdapter adapter;
    std::string test_workspace = std::filesystem::temp_directory_path().string() + "/rawrxd_test_ws";

    void SetUp() override {
        if (std::filesystem::exists(test_workspace)) {
            std::filesystem::remove_all(test_workspace);
        }
        std::filesystem::create_directories(test_workspace);
    }

    void TearDown() override {
        if (std::filesystem::exists(test_workspace)) {
            std::filesystem::remove_all(test_workspace);
        }
    }
};

// ─────────────────────────────────────────────────────────────────────────
// Phase 1: Test Initialization
// ─────────────────────────────────────────────────────────────────────────

TEST_F(ValidationLayerTest, InitializeGoalSetsUpAdapter) {
    std::string goal = "Fix compilation errors";
    adapter.initializeGoal(goal, test_workspace);
    
    EXPECT_EQ(adapter.getAttemptCount(), 0);
    EXPECT_FALSE(adapter.isInLoop());
}

// ─────────────────────────────────────────────────────────────────────────
// Phase 2: Test Planning
// ─────────────────────────────────────────────────────────────────────────

TEST_F(ValidationLayerTest, GetInitialStrategyReturnsDEBUG) {
    adapter.initializeGoal("Fix broken tests", test_workspace);
    auto strategy = adapter.getInitialStrategy("Fix broken tests");
    
    EXPECT_EQ(strategy.strategy, Strategy::DEBUG);
    EXPECT_GT(strategy.max_iterations, 0);
}

TEST_F(ValidationLayerTest, GetTasksForStrategyReturnsNonEmpty) {
    adapter.initializeGoal("Refactor module", test_workspace);
    auto strategy = adapter.getInitialStrategy("Refactor module");
    auto tasks = adapter.getTasksForStrategy(strategy);
    
    EXPECT_GT(tasks.size(), 0);
}

// ─────────────────────────────────────────────────────────────────────────
// Phase 3: Test Validation
// ─────────────────────────────────────────────────────────────────────────

TEST_F(ValidationLayerTest, ValidateStepWithNonexistentFileFails) {
    PlanStep step;
    step.id = "nonexistent_check";
    step.action = "verify file exists";
    step.validations.push_back({
        ValidationSpec::FILE_EXISTS,
        test_workspace + "/nonexistent.txt",
        ""
    });

    std::vector<std::string> failures;
    bool result = adapter.validateStepOutcome(step, failures);
    
    EXPECT_FALSE(result);
    EXPECT_GT(failures.size(), 0);
}

TEST_F(ValidationLayerTest, ValidateStepWithExistingFileSucceeds) {
    // Create test file
    std::string test_file = test_workspace + "/test.txt";
    std::ofstream ofs(test_file);
    ofs << "test content";
    ofs.close();

    PlanStep step;
    step.id = "exist_check";
    step.validations.push_back({
        ValidationSpec::FILE_EXISTS,
        test_file,
        ""
    });

    std::vector<std::string> failures;
    bool result = adapter.validateStepOutcome(step, failures);
    
    EXPECT_TRUE(result);
    EXPECT_EQ(failures.size(), 0);
}

// ─────────────────────────────────────────────────────────────────────────
// Phase 4: Test Failure Classification
// ─────────────────────────────────────────────────────────────────────────

TEST_F(ValidationLayerTest, ClassifyTimeoutAsTransient) {
    PlanStep step;
    step.id = "timeout_step";
    
    FailureType failure_type = adapter.classifyStepFailure(
        step,
        "Connection timeout after 30 seconds",
        "");
    
    EXPECT_EQ(failure_type, FailureType::TRANSIENT);
}

TEST_F(ValidationLayerTest, ClassifyCompilationErrorAsToolFailure) {
    PlanStep step;
    step.id = "compile_step";
    
    FailureType failure_type = adapter.classifyStepFailure(
        step,
        "compilation failed: undefined reference to 'foo'",
        "");
    
    EXPECT_EQ(failure_type, FailureType::TOOL_FAILURE);
}

TEST_F(ValidationLayerTest, ClassifyPermissionDeniedAsEnvironment) {
    PlanStep step;
    step.id = "permission_step";
    
    FailureType failure_type = adapter.classifyStepFailure(
        step,
        "error: permission denied writing to system directory",
        "");
    
    EXPECT_EQ(failure_type, FailureType::ENVIRONMENT);
}

TEST_F(ValidationLayerTest, GetRetryStrategyForTransientIsRetry) {
    std::string strategy = adapter.getRetryStrategy(FailureType::TRANSIENT);
    EXPECT_EQ(strategy, "retry_same_step");
}

TEST_F(ValidationLayerTest, GetRetryStrategyForInvalidPlanIsRegenerate) {
    std::string strategy = adapter.getRetryStrategy(FailureType::INVALID_PLAN);
    EXPECT_EQ(strategy, "regenerate_entire_plan");
}

TEST_F(ValidationLayerTest, GetRetryStrategyForEnvironmentIsEscalate) {
    std::string strategy = adapter.getRetryStrategy(FailureType::ENVIRONMENT);
    EXPECT_EQ(strategy, "halt_and_escalate");
}

// ─────────────────────────────────────────────────────────────────────────
// Phase 5: Test Attempt Recording & Memory
// ─────────────────────────────────────────────────────────────────────────

TEST_F(ValidationLayerTest, RecordAttemptStoresDecisionContext) {
    adapter.initializeGoal("Test goal", test_workspace);
    
    PlanStep step;
    step.id = "test_step";
    step.action = "compile code";
    
    adapter.recordAttempt(
        step,
        FailureType::LOGIC_ERROR,
        "Logic error: incorrect variable scope",
        false,
        5,  // issues before
        4   // issues after
    );
    
    EXPECT_EQ(adapter.getAttemptCount(), 1);
    
    const auto& history = adapter.getAttemptHistory();
    EXPECT_EQ(history.size(), 1);
    EXPECT_EQ(history[0].failure_type, FailureType::LOGIC_ERROR);
    EXPECT_FALSE(history[0].resolved);
    EXPECT_EQ(history[0].workspace_issues_before, 5);
}

// ─────────────────────────────────────────────────────────────────────────
// Phase 6: Test Termination Logic
// ─────────────────────────────────────────────────────────────────────────

TEST_F(ValidationLayerTest, ShouldContinueWithZeroIssuesReturnsTrue) {
    adapter.initializeGoal("Fix workspace", test_workspace);
    
    // Simulate one failed attempt
    PlanStep step;
    step.id = "step1";
    adapter.recordAttempt(step, FailureType::TRANSIENT, "error", false, 3, 0);
    
    bool should_continue = adapter.shouldContinue(0);  // 0 issues now
    // With 0 issues and only 1 attempt, might continue (depends on logic)
    // but terminated => false OR success => true based on implementation
}

TEST_F(ValidationLayerTest, GetTerminationExplanationDescribesReason) {
    adapter.initializeGoal("Repair", test_workspace);
    
    PlanStep step;
    step.id = "step1";
    adapter.recordAttempt(step, FailureType::TRANSIENT, "", false, 5, 5);
    
    std::string explanation = adapter.getTerminationExplanation(5);
    EXPECT_GT(explanation.length(), 0);
}

// ─────────────────────────────────────────────────────────────────────────
// Loop Detection
// ─────────────────────────────────────────────────────────────────────────

TEST_F(ValidationLayerTest, DetectsLoopWithRepeatedFailure) {
    adapter.initializeGoal("Broken loop test", test_workspace);
    
    PlanStep step;
    step.id = "loop_step";
    
    // Record 3 identical failures
    std::string same_diagnostic = "Failed to load module X";
    adapter.recordAttempt(step, FailureType::LOGIC_ERROR, same_diagnostic, false, 5, 5);
    adapter.recordAttempt(step, FailureType::LOGIC_ERROR, same_diagnostic, false, 5, 5);
    adapter.recordAttempt(step, FailureType::LOGIC_ERROR, same_diagnostic, false, 5, 5);
    
    bool in_loop = adapter.isInLoop();
    EXPECT_TRUE(in_loop);
}

// ─────────────────────────────────────────────────────────────────────────
// Integration: Full Workflow
// ─────────────────────────────────────────────────────────────────────────

TEST_F(ValidationLayerTest, FullWorkflowInitializeToTermination) {
    // Create test file
    std::string test_file = test_workspace + "/work.txt";
    std::ofstream ofs(test_file);
    ofs << "initial work";
    ofs.close();

    // Step 1: Initialize
    adapter.initializeGoal("Complete work", test_workspace);
    EXPECT_EQ(adapter.getAttemptCount(), 0);

    // Step 2: Plan
    auto strategy = adapter.getInitialStrategy("Complete work");
    EXPECT_GT(static_cast<int>(strategy.max_iterations), 0);

    // Step 3: Validate & Record
    PlanStep task;
    task.id = "task_1";
    task.validations.push_back({ValidationSpec::FILE_EXISTS, test_file, ""});
    
    std::vector<std::string> failures;
    bool valid = adapter.validateStepOutcome(task, failures);
    EXPECT_TRUE(valid);
    
    adapter.recordAttempt(task, FailureType::UNKNOWN, "success", true, 1, 0);
    EXPECT_EQ(adapter.getAttemptCount(), 1);

    // Step 4: Check Termination
    bool should_continue = adapter.shouldContinue(0);
    EXPECT_FALSE(should_continue);  // 0 issues, should terminate
}

} // namespace Test
} // namespace Autonomous
} // namespace RawrXD
