/**
 * @file agent_mode_handler.cpp
 * @brief Implementation of Agent Mode Handler
 */

#include "agent_mode_handler.hpp"
#include "unified_backend.hpp"
#include "../agent/meta_planner.hpp"
#include "../backend/agentic_tools.hpp"


#include <algorithm>

AgentModeHandler::AgentModeHandler(UnifiedBackend* backend, MetaPlanner* planner, void* parent)
    : void(parent)
    , m_backend(backend)
    , m_planner(planner)
{
    // Initialize tool executor
    m_toolExecutor = std::make_unique<AgenticToolExecutor>();

    if (m_toolExecutor) {
// Qt connect removed
// Qt connect removed
    }
}

AgentModeHandler::~AgentModeHandler() = default;

void AgentModeHandler::executeplan(const Plan& plan)
{
    if (plan.steps.isEmpty()) {
        executionFailed(-1, "Plan is empty");
        return;
    }

    m_executionPlan = plan;
    mapPlanToExecutionSteps(plan);
    m_currentStepIndex = 0;
    m_executionComplete = false;
    m_executionPaused = false;
    m_modifiedFiles.clear();

    executionStarted();
    progressUpdated(0.0f, "Starting execution...");

    executeNextStep();
}

void AgentModeHandler::pauseExecution()
{
    m_executionPaused = true;
    executionPaused();
    progressUpdated(getProgressPercentage(), "Execution paused");
}

void AgentModeHandler::resumeExecution()
{
    m_executionPaused = false;
    executionResumed();
    executeNextStep();
}

void AgentModeHandler::skipCurrentStep()
{
    if (m_currentStepIndex >= 0 && m_currentStepIndex < m_executionSteps.size()) {
        m_executionSteps[m_currentStepIndex].status = ExecutionStep::Skipped;
        stepProgress(m_currentStepIndex, "Step skipped by user");
        m_currentStepIndex++;
        executeNextStep();
    }
}

void AgentModeHandler::cancelExecution()
{
    m_executionComplete = true;
    m_executionPaused = true;

    progressUpdated(getProgressPercentage(), "Cancelling execution and rolling back...");

    // Rollback changes
    rollbackChanges();

    executionCancelled();
}

float AgentModeHandler::getProgressPercentage() const
{
    if (m_executionSteps.isEmpty()) {
        return 0.0f;
    }

    int completedCount = 0;
    for (const auto& step : m_executionSteps) {
        if (step.status == ExecutionStep::Completed || step.status == ExecutionStep::Skipped) {
            completedCount++;
        }
    }

    return (completedCount * 100.0f) / m_executionSteps.size();
}

void AgentModeHandler::onToolExecutionCompleted(const std::string& toolName, const std::string& output)
{
    if (m_currentStepIndex < 0 || m_currentStepIndex >= m_executionSteps.size()) {
        return;
    }

    auto& step = m_executionSteps[m_currentStepIndex];
    step.output = output;
    step.status = ExecutionStep::Completed;

    stepCompleted(m_currentStepIndex, step);
    stepOutput(m_currentStepIndex, output);

    m_currentStepIndex++;
    executeNextStep();
}

void AgentModeHandler::onToolExecutionError(const std::string& toolName, const std::string& error)
{
    if (m_currentStepIndex < 0 || m_currentStepIndex >= m_executionSteps.size()) {
        return;
    }

    auto& step = m_executionSteps[m_currentStepIndex];
    step.status = ExecutionStep::Failed;
    step.errorMessage = error;

    stepFailed(m_currentStepIndex, error);
    errorOccurred(std::string("Tool %1 failed: %2"));

    // Attempt recovery
    if (!attemptRecovery(m_currentStepIndex)) {
        executionFailed(m_currentStepIndex, error);
        rollbackChanges();
        m_executionComplete = true;
    }
}

void AgentModeHandler::onStepTimeout(int stepIndex)
{
    if (stepIndex >= 0 && stepIndex < m_executionSteps.size()) {
        errorOccurred(std::string("Step %1 timed out"));
        
        // Skip to next step or fail
        if (stepIndex == m_currentStepIndex) {
            m_currentStepIndex++;
            executeNextStep();
        }
    }
}

void AgentModeHandler::executeNextStep()
{
    if (m_executionPaused || m_executionComplete) {
        return;
    }

    // Check if all steps are complete
    if (m_currentStepIndex >= m_executionSteps.size()) {
        m_executionComplete = true;
        progressUpdated(100.0f, "Execution completed successfully");
        executionCompleted(m_executionSteps);
        return;
    }

    ExecutionStep& step = m_executionSteps[m_currentStepIndex];

    if (step.status != ExecutionStep::Pending) {
        m_currentStepIndex++;
        executeNextStep();
        return;
    }

    step.status = ExecutionStep::InProgress;
    stepStarting(m_currentStepIndex, step);
    progressUpdated(getProgressPercentage(), std::string("Executing step %1: %2")
        );

    executeSingleStep(step);
}

void AgentModeHandler::executeSingleStep(const ExecutionStep& step)
{
    if (!m_toolExecutor) {
        onToolExecutionError("executor", "Tool executor not initialized");
        return;
    }

    // Determine which tool to use based on step
    // This is a simplified approach - in production, AI would determine the tool
    std::string toolName = "file_operations";  // Default tool

    if (step.title.contains("compile", //CaseInsensitive)) {
        toolName = "compile";
    } else if (step.title.contains("test", //CaseInsensitive)) {
        toolName = "run_tests";
    } else if (step.title.contains("git", //CaseInsensitive)) {
        toolName = "git";
    } else if (step.title.contains("install", //CaseInsensitive)) {
        toolName = "install_packages";
    }

    stepExecuting(m_currentStepIndex, toolName);

    // Execute the tool
    m_toolExecutor->executeTool(toolName, step.title);

    // Set a timeout for the step
    void*::singleShot(30000, this, [this, stepIdx = m_currentStepIndex]() {
        onStepTimeout(stepIdx);
    });
}

bool AgentModeHandler::attemptRecovery(int stepIndex)
{
    // Simple recovery: skip failed step and continue
    // In production, AI would determine recovery strategy
    if (stepIndex < m_executionSteps.size() - 1) {
        errorOccurred(std::string("Skipping failed step %1 and continuing..."));
        m_executionSteps[stepIndex].status = ExecutionStep::Skipped;
        return true;
    }

    return false;
}

void AgentModeHandler::rollbackChanges()
{
    // Rollback all modified files
    for (const auto& file : m_modifiedFiles) {
        // In production, restore from backup
        progressUpdated(getProgressPercentage(), std::string("Rolling back: %1"));
    }
    m_modifiedFiles.clear();
}

void AgentModeHandler::mapPlanToExecutionSteps(const Plan& plan)
{
    m_executionSteps.clear();

    for (int i = 0; i < plan.steps.size(); ++i) {
        const auto& planStep = plan.steps[i];

        ExecutionStep execStep;
        execStep.stepId = planStep.id;
        execStep.title = planStep.title;
        execStep.filesModified = planStep.requiredFiles;  // Will be populated during execution
        execStep.status = ExecutionStep::Pending;

        m_executionSteps.append(execStep);
    }
}

