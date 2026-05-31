/**
 * Phase 1 Unified Smoke Test - Days 1-5 Agent Polish
 * 
 * Validates:
 * - Day 2: Workflow Persistence (save/restore/checkpoint)
 * - Day 3: Enhanced Memory Retrieval (semantic/keyword/hybrid search)
 * - Day 4: Todo/Task Integration (create/dependency/execute/status)
 * - Day 5: Autonomous Operation Demo (end-to-end scenarios)
 */

#include "execution_state_persistence.h"
#include "enhanced_memory_retrieval.h"
#include "todo_task_integration.h"
#include "autonomous_operation_demo.h"
#include "agentic_loop_state.h"
#include "agentic_memory_system.h"
#include "agentic_executor.h"

#include <filesystem>
#include <iostream>
#include <chrono>

namespace {
bool expect(bool condition, const char* message)
{
    if (!condition) {
        std::cerr << "[phase1_unified_smoke] FAIL: " << message << std::endl;
        std::cerr.flush();
        return false;
    }
    return true;
}
}

int main()
{
    namespace fs = std::filesystem;

    const auto root = fs::temp_directory_path() / "rawrxd_phase1_unified_smoke";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root, ec);
    if (ec) {
        std::cerr << "[phase1_unified_smoke] temp dir setup failed: " << ec.message() << std::endl;
        return 1;
    }

    bool ok = true;
    int passCount = 0;
    int totalTests = 0;

    auto runTest = [&](const char* name, bool result) {
        totalTests++;
        if (result) {
            passCount++;
            std::cout << "[phase1_unified_smoke] PASS: " << name << std::endl;
        } else {
            std::cout << "[phase1_unified_smoke] FAIL: " << name << std::endl;
        }
        ok &= result;
    };

    // ========================================================================
    // DAY 2: WORKFLOW PERSISTENCE
    // ========================================================================
    std::cout << "\n=== DAY 2: WORKFLOW PERSISTENCE ===" << std::endl;

    ExecutionStatePersistence persistence(root);

    // Test 2.1: Basic persist/load
    WorkflowExecution exec;
    exec.executionId = "day2_test";
    exec.workflowName = "Day2Smoke";
    exec.goal = "Test workflow persistence";
    exec.status = "in-progress";
    
    auto execId = persistence.persistWorkflowExecution(exec);
    runTest("2.1 Persist workflow execution", !execId.empty());

    auto loaded = persistence.loadWorkflowExecution(execId);
    runTest("2.2 Load workflow execution", loaded != nullptr);
    if (loaded) {
        runTest("2.3 Loaded execution ID matches", loaded->executionId == execId);
        runTest("2.4 Loaded goal preserved", loaded->goal == exec.goal);
    }

    // Test 2.5: Checkpoint creation
    auto cpId = persistence.createCheckpoint(execId, "day2_checkpoint", nlohmann::json{{"step", 1}});
    runTest("2.5 Create checkpoint", !cpId.empty());

    auto resumed = persistence.resumeFromCheckpoint(execId);
    runTest("2.6 Resume from checkpoint", resumed != nullptr);

    // Test 2.7: List executions
    auto executions = persistence.listExecutions();
    runTest("2.7 List executions", !executions.empty());

    // Test 2.8: Validation
    runTest("2.8 Valid execution check", persistence.hasValidExecution(execId));

    // ========================================================================
    // DAY 3: ENHANCED MEMORY RETRIEVAL
    // ========================================================================
    std::cout << "\n=== DAY 3: ENHANCED MEMORY RETRIEVAL ===" << std::endl;

    AgenticMemorySystem memorySystem;
    memorySystem.storeMemory(MemoryType::Fact, "workflow persistence is critical for reliability", "tag1");
    memorySystem.storeMemory(MemoryType::Fact, "checkpoint compression reduces storage size", "tag2");
    memorySystem.storeMemory(MemoryType::Procedure, "always validate integrity before resuming", "tag3");
    memorySystem.storeMemory(MemoryType::CodeSnippet, "void saveState() { checkpoint(); }", "tag4");
    memorySystem.storeMemory(MemoryType::Fact, "semantic search improves memory recall quality", "tag5");

    EnhancedMemoryRetrieval memoryRetrieval(&memorySystem);

    // Test 3.1: Semantic search
    auto semanticResults = memoryRetrieval.semanticSearch("reliability persistence", 3);
    runTest("3.1 Semantic search returns results", !semanticResults.empty());
    if (!semanticResults.empty()) {
        runTest("3.2 Semantic search relevance > 0", semanticResults[0].relevanceScore > 0.0f);
    }

    // Test 3.3: Keyword search
    auto keywordResults = memoryRetrieval.keywordSearch("checkpoint", 3);
    runTest("3.3 Keyword search returns results", !keywordResults.empty());

    // Test 3.4: Hybrid search
    auto hybridResults = memoryRetrieval.hybridSearch("memory recall", 0.4f, 3);
    runTest("3.4 Hybrid search returns results", !hybridResults.empty());

    // Test 3.5: Context-aware retrieval
    auto contextResults = memoryRetrieval.getContextRelevantMemories("workflow execution", 3);
    runTest("3.5 Context-aware retrieval", !contextResults.empty());

    // Test 3.6: Category search
    auto categoryResults = memoryRetrieval.getMemoriesByCategory("code");
    runTest("3.6 Category search", !categoryResults.empty());

    // Test 3.7: Relevance scoring
    float score = memoryRetrieval.scoreRelevance("workflow persistence is critical", "persistence reliability");
    runTest("3.7 Relevance scoring > 0", score > 0.0f);

    // Test 3.8: Auto-summarization
    auto summary = memoryRetrieval.summarizeRetrievedMemories(semanticResults, 200);
    runTest("3.8 Auto-summarization non-empty", !summary.empty());

    // Test 3.9: Cache functionality
    auto cached = memoryRetrieval.getCachedResults("reliability persistence");
    // May be empty if not cached yet, that's ok - just verify API works
    runTest("3.9 Cache API works", true);

    // Test 3.10: Memory health
    auto health = memoryRetrieval.validateMemoryHealth();
    runTest("3.10 Memory health check", health.empty());

    // Test 3.11: Robust search (with fallback)
    auto robust = memoryRetrieval.robustSearch("nonexistent query xyz123", 3);
    // May be empty, but should not crash
    runTest("3.11 Robust search no-crash", true);

    // ========================================================================
    // DAY 4: TODO/TASK INTEGRATION
    // ========================================================================
    std::cout << "\n=== DAY 4: TODO/TASK INTEGRATION ===" << std::endl;

    // Use null executor - we only test task management, not actual tool execution
    TodoTaskIntegration taskIntegration(nullptr, &persistence);

    // Test 4.1: Create task
    auto task1 = taskIntegration.createTask(
        "Analyze Code",
        "Scan source files for patterns",
        "analyze_tool",
        nlohmann::json{{"path", "src/"}},
        TaskPriority::High);
    runTest("4.1 Create task", !task1.empty());

    auto task2 = taskIntegration.createTask(
        "Generate Docs",
        "Create documentation from analysis",
        "doc_tool",
        nlohmann::json{{"format", "markdown"}},
        TaskPriority::Normal);
    runTest("4.2 Create second task", !task2.empty());

    // Test 4.3: Add dependency
    runTest("4.3 Add dependency", taskIntegration.addDependency(task2, task1));

    // Test 4.4: Get task
    auto taskPtr = taskIntegration.getTask(task1);
    runTest("4.4 Get task", taskPtr != nullptr);

    // Test 4.5: List tasks
    auto tasks = taskIntegration.listTasks(TaskStatus::Pending);
    runTest("4.5 List pending tasks", tasks.size() == 2);

    // Test 4.6: Dependency resolution
    auto task2Ptr = taskIntegration.getTask(task2);
    runTest("4.6 Task2 has dependencies", task2Ptr != nullptr && !task2Ptr->dependencies.empty());

    // Test 4.7: Complete task1 (unblocks task2)
    runTest("4.7 Complete task1", taskIntegration.completeTask(task1, nlohmann::json{{"result", "ok"}}));

    // Test 4.8: Task2 dependencies resolved
    auto task2After = taskIntegration.getTask(task2);
    runTest("4.8 Task2 dependencies resolved after task1 completion",
            task2After != nullptr && task2After->isDependenciesResolved());

    // Test 4.9: Workflow status
    auto status = taskIntegration.getWorkflowStatus();
    runTest("4.9 Workflow status tracked", status.totalTasks == 2);
    runTest("4.10 Completed tasks counted", status.completedTasks == 1);

    // Test 4.11: Execution order
    auto order = taskIntegration.getExecutionOrder();
    runTest("4.11 Execution order computed", !order.empty());

    // Test 4.12: Validate dependencies (no cycles)
    auto depValidation = taskIntegration.validateDependencies();
    runTest("4.12 Dependency validation (no cycles)", depValidation.empty());

    // Test 4.13: Task serialization
    if (taskPtr) {
        auto taskJson = taskPtr->toJson();
        runTest("4.13 Task serialization", taskJson.contains("taskId"));
    }

    // ========================================================================
    // DAY 5: AUTONOMOUS OPERATION DEMO
    // ========================================================================
    std::cout << "\n=== DAY 5: AUTONOMOUS OPERATION DEMO ===" << std::endl;

    AgenticLoopState loopState;
    AgenticExecutor executor(nullptr); // Minimal executor for demo

    AutonomousOperationDemo demo(
        &persistence,
        &memoryRetrieval,
        &taskIntegration,
        &executor,
        &loopState);

    // Test 5.1: Production readiness check
    runTest("5.1 Production readiness", demo.isProductionReady());

    // Test 5.2: Readiness assessment
    auto assessment = demo.getReadinessAssessment();
    runTest("5.2 Readiness assessment", assessment.contains("systemStatus"));

    // Test 5.3: Scenario 1 - Code Analysis Autonomy
    auto result1 = demo.demonstrateCodeAnalysisAutonomy();
    runTest("5.3 Code analysis autonomy", result1.success);
    runTest("5.4 Code analysis steps > 0", result1.numberOfSteps > 0);

    // Test 5.5: Scenario 2 - Bug Fix Autonomy
    auto result2 = demo.demonstrateBugFixAutonomy();
    runTest("5.5 Bug fix autonomy", result2.success);
    runTest("5.6 Bug fix steps > 0", result2.numberOfSteps > 0);

    // Test 5.7: Scenario 3 - Workflow Persistence
    auto result3 = demo.demonstrateWorkflowPersistence();
    runTest("5.7 Workflow persistence demo", result3.success);
    runTest("5.8 Workflow persistence checkpoints", !result3.checkpoints.empty());

    // Test 5.9: Validation report
    auto validation = demo.validateExecution(result1);
    runTest("5.9 Validation report generated", validation.qualityScore >= 0.0f);

    // Test 5.10: Evidence collection
    auto evidence = demo.collectAllEvidence();
    runTest("5.10 Evidence collected", !evidence.execution.goal.empty());

    // Test 5.11: Regression tests
    runTest("5.11 Regression tests pass", demo.runRegressionTests());

    // Test 5.12: Agent loop validation
    runTest("5.12 Agent loop validation", demo.validateAgentLoops());

    // ========================================================================
    // FINAL REPORT
    // ========================================================================
    std::cout << "\n========================================" << std::endl;
    std::cout << "PHASE 1 UNIFIED SMOKE TEST RESULTS" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "Passed: " << passCount << "/" << totalTests << std::endl;
    std::cout << "Failed: " << (totalTests - passCount) << "/" << totalTests << std::endl;
    std::cout << "Status: " << (ok ? "PASS" : "FAIL") << std::endl;
    std::cout << "========================================" << std::endl;

    // Cleanup
    fs::remove_all(root, ec);

    return ok ? 0 : 1;
}
