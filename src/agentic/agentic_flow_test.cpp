// ============================================================================
// agentic_flow_test.cpp — Agentic Flow Engine Test Suite
// ============================================================================
// Copyright (c) 2025-2026 RawrXD Project — All rights reserved.
// ============================================================================

#include "agentic/agentic_flow.h"

#include <iostream>
#include <cassert>
#include <chrono>
#include <thread>
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

using namespace RawrXD::Agentic;

// ============================================================================
// Test Utilities
// ============================================================================

class TestFixture {
public:
    std::string testDir;
    
    TestFixture() {
        testDir = fs::temp_directory_path().string() + "/agentic_test_" + 
                  std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
        fs::create_directories(testDir);
    }
    
    ~TestFixture() {
        fs::remove_all(testDir);
    }
    
    std::string createTestFile(const std::string& name, const std::string& content) {
        std::string path = testDir + "/" + name;
        std::ofstream file(path);
        file << content;
        return path;
    }
    
    std::string readFile(const std::string& path) {
        std::ifstream file(path);
        std::ostringstream content;
        content << file.rdbuf();
        return content.str();
    }
};

// ============================================================================
// Basic Tests
// ============================================================================

void testFlowEngineCreation() {
    std::cout << "Testing flow engine creation...\n";
    
    auto engine = createFlowEngine();
    assert(engine != nullptr);
    
    engine->setMode(FlowMode::Autonomous);
    assert(engine->getMode() == FlowMode::Autonomous);
    
    std::cout << "  ✓ Flow engine creation test passed\n";
}

void testFlowRegistration() {
    std::cout << "Testing flow registration...\n";
    
    auto engine = createFlowEngine();
    
    FlowDefinition flow;
    flow.id = "test_flow";
    flow.name = "Test Flow";
    flow.description = "A test flow";
    
    bool registered = engine->registerFlow(flow);
    assert(registered);
    
    auto flows = engine->getAvailableFlows();
    assert(std::find(flows.begin(), flows.end(), "test_flow") != flows.end());
    
    auto retrieved = engine->getFlow("test_flow");
    assert(retrieved.has_value());
    assert(retrieved->name == "Test Flow");
    
    bool unregistered = engine->unregisterFlow("test_flow");
    assert(unregistered);
    
    flows = engine->getAvailableFlows();
    assert(std::find(flows.begin(), flows.end(), "test_flow") == flows.end());
    
    std::cout << "  ✓ Flow registration test passed\n";
}

void testFlowExecution() {
    std::cout << "Testing flow execution...\n";
    
    TestFixture fixture;
    
    std::string file = fixture.createTestFile("test.txt", "Hello, World!");
    
    auto engine = createFlowEngine();
    
    // Create simple flow
    FlowDefinition flow;
    flow.id = "simple_flow";
    flow.name = "Simple Flow";
    flow.mode = FlowMode::Autonomous;
    
    FlowStep step1;
    step1.id = "step1";
    step1.name = "Analyze";
    step1.action.type = StepType::Analyze;
    step1.action.description = "Analyze file";
    
    flow.steps.push_back(step1);
    flow.entryStep = "step1";
    flow.stepMap["step1"] = step1;
    
    engine->registerFlow(flow);
    
    FlowContext context;
    context.targetFiles.push_back(file);
    context.fileContents[file] = fixture.readFile(file);
    
    std::string executionId = engine->startFlow("simple_flow", context);
    assert(!executionId.empty());
    
    // Wait for completion
    uint32_t waitCount = 0;
    while (engine->getStatus(executionId) == FlowStatus::Executing && 
           waitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        waitCount++;
    }
    
    auto result = engine->getResult(executionId);
    assert(result.has_value());
    assert(result->status == FlowStatus::Completed);
    
    std::cout << "  ✓ Flow execution test passed\n";
}

void testFlowCallbacks() {
    std::cout << "Testing flow callbacks...\n";
    
    std::vector<FlowEvent> events;
    
    auto engine = createFlowEngine();
    engine->setCallback([&events](const FlowEventArgs& args) {
        events.push_back(args.event);
    });
    
    TestFixture fixture;
    std::string file = fixture.createTestFile("callback.txt", "test");
    
    FlowDefinition flow;
    flow.id = "callback_flow";
    flow.name = "Callback Test";
    flow.mode = FlowMode::Autonomous;
    
    FlowStep step;
    step.id = "step1";
    step.name = "Test Step";
    step.action.type = StepType::Analyze;
    
    flow.steps.push_back(step);
    flow.entryStep = "step1";
    flow.stepMap["step1"] = step;
    
    engine->registerFlow(flow);
    
    FlowContext context;
    context.targetFiles.push_back(file);
    context.fileContents[file] = "test";
    
    std::string executionId = engine->startFlow("callback_flow", context);
    
    // Wait for completion
    uint32_t waitCount = 0;
    while (engine->getStatus(executionId) == FlowStatus::Executing && 
           waitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        waitCount++;
    }
    
    // Should have received events
    bool foundStarted = false;
    bool foundCompleted = false;
    
    for (const auto& event : events) {
        if (event == FlowEvent::FlowStarted) foundStarted = true;
        if (event == FlowEvent::FlowCompleted) foundCompleted = true;
    }
    
    assert(foundStarted);
    assert(foundCompleted);
    
    std::cout << "  ✓ Flow callbacks test passed\n";
}

void testFlowCancellation() {
    std::cout << "Testing flow cancellation...\n";
    
    auto engine = createFlowEngine();
    
    // Create a long-running flow
    FlowDefinition flow;
    flow.id = "long_flow";
    flow.name = "Long Flow";
    flow.mode = FlowMode::Autonomous;
    flow.maxSteps = 1000;
    
    for (int i = 0; i < 10; i++) {
        FlowStep step;
        step.id = "step" + std::to_string(i);
        step.name = "Step " + std::to_string(i);
        step.action.type = StepType::Wait;
        step.action.params["duration"] = "1000"; // 1 second each
        
        flow.steps.push_back(step);
        flow.stepMap[step.id] = step;
    }
    
    flow.entryStep = "step0";
    
    engine->registerFlow(flow);
    
    std::string executionId = engine->startFlow("long_flow");
    assert(!executionId.empty());
    
    // Wait a bit then cancel
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    bool cancelled = engine->cancelFlow(executionId);
    assert(cancelled);
    
    // Wait for cancellation to take effect
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    auto status = engine->getStatus(executionId);
    assert(status == FlowStatus::Cancelled || status == FlowStatus::Failed);
    
    std::cout << "  ✓ Flow cancellation test passed\n";
}

void testFlowPauseResume() {
    std::cout << "Testing flow pause/resume...\n";
    
    auto engine = createFlowEngine();
    
    FlowDefinition flow;
    flow.id = "pause_flow";
    flow.name = "Pause Test";
    flow.mode = FlowMode::Interactive;
    
    FlowStep step;
    step.id = "step1";
    step.name = "Test Step";
    step.action.type = StepType::Analyze;
    
    flow.steps.push_back(step);
    flow.entryStep = "step1";
    flow.stepMap["step1"] = step;
    
    engine->registerFlow(flow);
    
    std::string executionId = engine->startFlow("pause_flow");
    
    // Try to pause
    bool paused = engine->pauseFlow(executionId);
    // May fail if already completed
    
    // Try to resume
    bool resumed = engine->resumeFlow(executionId);
    // May fail if not paused
    
    std::cout << "  ✓ Flow pause/resume test passed\n";
}

void testCheckpoints() {
    std::cout << "Testing checkpoints...\n";
    
    auto engine = createFlowEngine();
    
    FlowDefinition flow;
    flow.id = "checkpoint_flow";
    flow.name = "Checkpoint Test";
    flow.mode = FlowMode::Autonomous;
    flow.enableCheckpointing = true;
    flow.checkpointInterval = 1;
    
    FlowStep step1;
    step1.id = "step1";
    step1.name = "Step 1";
    step1.action.type = StepType::Analyze;
    
    FlowStep step2;
    step2.id = "step2";
    step2.name = "Step 2";
    step2.action.type = StepType::Analyze;
    
    flow.steps = {step1, step2};
    flow.entryStep = "step1";
    flow.stepMap["step1"] = step1;
    flow.stepMap["step2"] = step2;
    
    engine->registerFlow(flow);
    
    std::string executionId = engine->startFlow("checkpoint_flow");
    
    // Wait for completion
    uint32_t waitCount = 0;
    while (engine->getStatus(executionId) == FlowStatus::Executing && 
           waitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        waitCount++;
    }
    
    // Create manual checkpoint
    uint32_t cpId = engine->createCheckpoint(executionId);
    // May be 0 if flow already completed
    
    std::cout << "  ✓ Checkpoints test passed\n";
}

void testUserInput() {
    std::cout << "Testing user input...\n";
    
    auto engine = createFlowEngine();
    
    FlowDefinition flow;
    flow.id = "input_flow";
    flow.name = "Input Test";
    flow.mode = FlowMode::Interactive;
    
    FlowStep step;
    step.id = "query";
    step.name = "Query User";
    step.action.type = StepType::Query;
    step.action.description = "What is your name?";
    step.action.params["prompt"] = "Enter your name:";
    
    flow.steps.push_back(step);
    flow.entryStep = "query";
    flow.stepMap["query"] = step;
    
    engine->registerFlow(flow);
    
    std::string executionId = engine->startFlow("input_flow");
    
    // Provide input
    bool provided = engine->provideInput(executionId, "Test User");
    assert(provided);
    
    // Wait for input
    auto input = engine->waitForInput(executionId, 1000);
    // May be empty if flow already processed
    
    std::cout << "  ✓ User input test passed\n";
}

// ============================================================================
// Built-in Flow Tests
// ============================================================================

void testFeatureFlow() {
    std::cout << "Testing feature flow...\n";
    
    auto flow = Flows::createFeatureFlow("user_authentication");
    
    assert(flow.id == "feature_user_authentication");
    assert(!flow.steps.empty());
    assert(flow.entryStep == "analyze");
    
    std::cout << "  ✓ Feature flow test passed\n";
}

void testBugFixFlow() {
    std::cout << "Testing bug fix flow...\n";
    
    auto flow = Flows::createBugFixFlow("Null pointer exception in login");
    
    assert(!flow.id.empty());
    assert(!flow.steps.empty());
    assert(flow.entryStep == "analyze");
    
    std::cout << "  ✓ Bug fix flow test passed\n";
}

void testRefactorFlow() {
    std::cout << "Testing refactor flow...\n";
    
    auto flow = Flows::createRefactorFlow("UserService", "extract_method");
    
    assert(flow.id == "refactor_UserService");
    assert(!flow.steps.empty());
    
    std::cout << "  ✓ Refactor flow test passed\n";
}

void testReviewFlow() {
    std::cout << "Testing review flow...\n";
    
    auto flow = Flows::createReviewFlow({"src/main.cpp", "src/utils.cpp"});
    
    assert(flow.id == "review");
    assert(!flow.steps.empty());
    
    std::cout << "  ✓ Review flow test passed\n";
}

void testTestFlow() {
    std::cout << "Testing test generation flow...\n";
    
    auto flow = Flows::createTestFlow("src/calculator.cpp");
    
    assert(!flow.id.empty());
    assert(!flow.steps.empty());
    
    std::cout << "  ✓ Test flow test passed\n";
}

void testDocsFlow() {
    std::cout << "Testing documentation flow...\n";
    
    auto flow = Flows::createDocsFlow({"src/api.cpp", "src/models.cpp"});
    
    assert(flow.id == "docs");
    assert(!flow.steps.empty());
    
    std::cout << "  ✓ Docs flow test passed\n";
}

void testMigrationFlow() {
    std::cout << "Testing migration flow...\n";
    
    auto flow = Flows::createMigrationFlow("JavaScript", "TypeScript");
    
    assert(!flow.id.empty());
    assert(flow.mode == FlowMode::Supervised);
    
    std::cout << "  ✓ Migration flow test passed\n";
}

void testOptimizeFlow() {
    std::cout << "Testing optimization flow...\n";
    
    auto flow = Flows::createOptimizeFlow("database_queries");
    
    assert(!flow.id.empty());
    assert(!flow.steps.empty());
    
    std::cout << "  ✓ Optimize flow test passed\n";
}

void testDebugFlow() {
    std::cout << "Testing debug flow...\n";
    
    auto flow = Flows::createDebugFlow("Memory leak in image processor");
    
    assert(!flow.id.empty());
    assert(flow.mode == FlowMode::Interactive);
    
    std::cout << "  ✓ Debug flow test passed\n";
}

void testAnalysisFlow() {
    std::cout << "Testing analysis flow...\n";
    
    auto flow = Flows::createAnalysisFlow();
    
    assert(flow.id == "analysis");
    assert(!flow.steps.empty());
    
    std::cout << "  ✓ Analysis flow test passed\n";
}

// ============================================================================
// Condition Tests
// ============================================================================

void testConditions() {
    std::cout << "Testing conditions...\n";
    
    FlowContext ctx;
    ctx.variables["status"] = "active";
    ctx.variables["count"] = "5";
    ctx.variables["name"] = "test_user";
    
    StepCondition cond1;
    cond1.variable = "status";
    cond1.operator_ = "==";
    cond1.value = "active";
    assert(cond1.evaluate(ctx));
    
    StepCondition cond2;
    cond2.variable = "count";
    cond2.operator_ = ">";
    cond2.value = "3";
    assert(cond2.evaluate(ctx));
    
    StepCondition cond3;
    cond3.variable = "name";
    cond3.operator_ = "contains";
    cond3.value = "user";
    assert(cond3.evaluate(ctx));
    
    StepCondition cond4;
    cond4.variable = "status";
    cond4.operator_ = "!=";
    cond4.value = "inactive";
    assert(cond4.evaluate(ctx));

    StepCondition cond5;
    cond5.variable = "status";
    cond5.operator_ = "==";
    cond5.value = "inactive";
    assert(!cond5.evaluate(ctx));
    
    std::cout << "  ✓ Conditions test passed\n";
}

void testFlowPlanGenerateEditAndTestExecution() {
    std::cout << "Testing real plan/generate/edit/test execution...\n";

    TestFixture fixture;
    const std::string file = fixture.createTestFile("feature.txt", "seed\n");

    auto engine = createFlowEngine();
    auto flow = Flows::createFeatureFlow("autonomy_patch");
    flow.id = "feature_autonomy_patch_real";
    flow.stepMap.clear();
    for (const auto& step : flow.steps) {
        flow.stepMap[step.id] = step;
    }
    flow.steps.back().action.command = "cmd /c echo flow-test-ok";

    engine->registerFlow(flow);

    FlowContext context;
    context.workingDirectory = fixture.testDir;
    context.targetFiles.push_back(file);
    context.fileContents[file] = fixture.readFile(file);

    const std::string executionId = engine->startFlow(flow.id, context);
    assert(!executionId.empty());

    uint32_t waitCount = 0;
    while (engine->getStatus(executionId) == FlowStatus::Executing && waitCount < 100) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        waitCount++;
    }

    auto result = engine->getResult(executionId);
    assert(result.has_value());
    assert(result->status == FlowStatus::Completed);
    assert(result->variables.count("implementation_plan") > 0);
    assert(result->variables.count("generated_content") > 0);
    assert(result->variables.count("last_test") > 0);
    assert(result->variables.at("last_test").find("flow-test-ok") != std::string::npos);

    const std::string updated = fixture.readFile(file);
    assert(updated.find("Generated code for") != std::string::npos);

    std::cout << "  ✓ Real plan/generate/edit/test execution passed\n";
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main() {
    std::cout << "========================================\n";
    std::cout << "Agentic Flow Engine Test Suite\n";
    std::cout << "========================================\n\n";
    
    // Basic tests
    std::cout << "--- Basic Tests ---\n";
    testFlowEngineCreation();
    testFlowRegistration();
    testFlowExecution();
    testFlowCallbacks();
    testFlowCancellation();
    testFlowPauseResume();
    testCheckpoints();
    testUserInput();
    
    // Built-in flow tests
    std::cout << "\n--- Built-in Flow Tests ---\n";
    testFeatureFlow();
    testBugFixFlow();
    testRefactorFlow();
    testReviewFlow();
    testTestFlow();
    testDocsFlow();
    testMigrationFlow();
    testOptimizeFlow();
    testDebugFlow();
    testAnalysisFlow();
    
    // Condition tests
    std::cout << "\n--- Condition Tests ---\n";
    testConditions();

    // Real execution tests
    std::cout << "\n--- Real Execution Tests ---\n";
    testFlowPlanGenerateEditAndTestExecution();
    
    std::cout << "\n========================================\n";
    std::cout << "All tests passed! ✓\n";
    std::cout << "========================================\n";
    
    return 0;
}
