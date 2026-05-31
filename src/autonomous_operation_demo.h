#pragma once
/**
 * AutonomousOperationDemo - Day 5: End-to-end autonomous operation
 * 
 * Production demonstration of autonomous agent execution with
 * workflow persistence, memory retrieval, task management, and validation.
 */

#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>

class ExecutionStatePersistence;
class EnhancedMemoryRetrieval;
class TodoTaskIntegration;
class AgenticExecutor;
class AgenticLoopState;

/**
 * @struct AutonomousExecutionResult
 * Results from autonomous operation demonstration
 */
struct AutonomousExecutionResult {
    bool success = false;
    std::string executionId;
    std::string goal;
    int numberOfSteps = 0;
    float executionTimeSeconds = 0.0f;
    
    // Evidence collection
    std::vector<std::string> checkpoints;
    std::vector<std::string> decisions;
    std::vector<std::string> validationResults;
    std::string errorLog;
    
    nlohmann::json toJson() const;
};

/**
 * @class AutonomousOperationDemo
 * @brief Production demonstration: Days 1-5 autonomous operation end-to-end
 * 
 * Executes a meaningful contained scenario that demonstrates:
 * 1. Workflow persistence (state survives restart)
 * 2. Memory retrieval (context awareness)
 * 3. Todo/task execution (multi-step operations)
 * 4. Autonomous decision-making
 * 5. Validation and error recovery
 * 
 * Evidence generated:
 * - Build artifacts (compiles successfully)
 * - Runtime logs (execution traces)
 * - Regression tests (validates core agent loops)
 * - Performance measurements
 */
class AutonomousOperationDemo {
public:
    explicit AutonomousOperationDemo(
        ExecutionStatePersistence* persistence,
        EnhancedMemoryRetrieval* memory,
        TodoTaskIntegration* tasks,
        AgenticExecutor* executor,
        AgenticLoopState* loopState);
    ~AutonomousOperationDemo();

    // ===== Scenarios =====

    /**
     * Scenario 1: Code Analysis and Documentation Generation
     * Demonstrates: File I/O, memory recall, multi-step task execution
     * 
     * Steps:
     * 1. Analyze source file structure
     * 2. Retrieve relevant documentation patterns
     * 3. Generate documentation tasks
     * 4. Execute and validate
     */
    AutonomousExecutionResult demonstrateCodeAnalysisAutonomy();

    /**
     * Scenario 2: Bug Detection and Fix Generation
     * Demonstrates: Context gathering, decision making, error recovery
     * 
     * Steps:
     * 1. Analyze code for potential bugs
     * 2. Recall similar bug patterns from memory
     * 3. Generate fix tasks
     * 4. Test and validate
     * 5. Checkpoint on success
     */
    AutonomousExecutionResult demonstrateBugFixAutonomy();

    /**
     * Scenario 3: Workflow Persistence and Resumption
     * Demonstrates: State persistence, crash recovery, resumption
     * 
     * Steps:
     * 1. Start complex multi-step workflow
     * 2. Create checkpoint mid-execution
     * 3. Simulate crash/restart
     * 4. Resume from checkpoint
     * 5. Complete execution
     */
    AutonomousExecutionResult demonstrateWorkflowPersistence();

    // ===== Validation Framework =====

    /**
     * Validate autonomous execution result
     * Checks: success, evidence completeness, no regressions
     */
    struct ValidationReport {
        bool buildSucceeded = false;
        bool allTasksCompleted = false;
        bool memoryRetrievalWorked = false;
        bool checkpointingWorked = false;
        std::vector<std::string> regressions;
        float qualityScore = 0.0f;  // 0-100
        
        bool isPassed() const;
        nlohmann::json toJson() const;
    };

    ValidationReport validateExecution(const AutonomousExecutionResult& result);

    // ===== Evidence Collection =====

    /**
     * Collect all evidence from demonstration
     * Generate reports suitable for production handoff
     */
    struct EvidencePackage {
        std::string buildLog;
        std::string runtimeLog;
        std::vector<std::string> regressionTestResults;
        AutonomousExecutionResult execution;
        ValidationReport validation;
        nlohmann::json performanceMetrics;
        
        std::string generateReport() const;
    };

    EvidencePackage collectAllEvidence();

    // ===== Regression Testing =====

    /**
     * Run regression tests to ensure existing workflows still work
     */
    bool runRegressionTests();

    /**
     * Test core agent loops remain functional
     */
    bool validateAgentLoops();

    // ===== Performance Measurement =====

    struct PerformanceMetrics {
        float totalExecutionTimeMs = 0.0f;
        float averageTaskTimeMs = 0.0f;
        float memoryRetrievalTimeMs = 0.0f;
        int totalToolCalls = 0;
        int successfulToolCalls = 0;
        float memoryUsageMb = 0.0f;
        
        nlohmann::json toJson() const;
    };

    PerformanceMetrics getPerformanceMetrics() const { return m_metrics; }

    // ===== Production Readiness Gates =====

    /**
     * Check if autonomous operation meets production readiness
     * Checks: stability (no crashes), correctness, performance, safety
     */
    bool isProductionReady() const;

    /**
     * Get detailed readiness assessment
     */
    nlohmann::json getReadinessAssessment() const;

private:
    ExecutionStatePersistence* m_persistence;
    EnhancedMemoryRetrieval* m_memory;
    TodoTaskIntegration* m_tasks;
    AgenticExecutor* m_executor;
    AgenticLoopState* m_loopState;

    PerformanceMetrics m_metrics;
    std::string m_buildLog;
    std::string m_runtimeLog;

    // Helpers
    void recordLog(const std::string& message);
    void recordBuildLog(const std::string& message);
    bool executeWithTiming(std::function<bool()> operation, float& timeMs);
    AutonomousExecutionResult wrapDemonstration(const std::string& scenarioName);
};
