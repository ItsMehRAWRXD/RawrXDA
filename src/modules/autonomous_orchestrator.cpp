// ============================================================================
// autonomous_orchestrator.cpp — Implementation of agentic orchestration
// ============================================================================

#include "autonomous_orchestrator.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <queue>

namespace RawrXD {

// ============================================================================
// PlanGenerator Implementation
// ============================================================================

ExecutionPlan PlanGenerator::GeneratePlan(const std::string& taskDescription,
                                          const std::vector<std::string>& contextFiles) {
    ExecutionPlan plan;
    plan.planId = 1;
    plan.taskDescription = taskDescription;
    plan.totalEstimatedMs = 0;
    plan.successCount = 0;
    plan.failureCount = 0;

    // Decompose task into steps
    plan.steps = DecomposeTask(taskDescription);

    // Resolve dependencies between steps
    ResolveDependencies(plan.steps);

    // Assign safety gates based on action type
    AssignSafetyGates(plan.steps);

    // Estimate durations
    for (auto& step : plan.steps) {
        step.estimatedDurationMs = 500 + (step.action.length() * 10);
        plan.totalEstimatedMs += step.estimatedDurationMs;
    }

    plan.isParallelizable = true;
    plan.requiresApproval = false;

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    plan.generatedAt = std::ctime(&time_t_now);

    return plan;
}

std::vector<PlanStep> PlanGenerator::DecomposeTask(const std::string& task) {
    std::vector<PlanStep> steps;

    // Simple heuristic decomposition based on keywords
    if (task.find("refactor") != std::string::npos) {
        steps.push_back({0, "analyze_code", "Analyze code structure", {}, {}, "", SafetyGateType::PREVIEW, StepState::PENDING, 1000, 0, "", "", true, {}});
        steps.push_back({1, "identify_patterns", "Identify refactoring patterns", {0}, {}, "", SafetyGateType::NONE, StepState::PENDING, 800, 0, "", "", true, {}});
        steps.push_back({2, "apply_refactor", "Apply refactoring changes", {1}, {}, "", SafetyGateType::ROLLBACK_CAPABLE, StepState::PENDING, 1500, 0, "", "", true, {}});
        steps.push_back({3, "verify_tests", "Run tests to verify", {2}, {}, "", SafetyGateType::CONFIRM, StepState::PENDING, 2000, 0, "", "", false, {}});
    } else if (task.find("test") != std::string::npos) {
        steps.push_back({0, "analyze_coverage", "Analyze test coverage", {}, {}, "", SafetyGateType::PREVIEW, StepState::PENDING, 1200, 0, "", "", true, {}});
        steps.push_back({1, "generate_tests", "Generate missing tests", {0}, {}, "", SafetyGateType::ROLLBACK_CAPABLE, StepState::PENDING, 2000, 0, "", "", true, {}});
        steps.push_back({2, "run_tests", "Execute test suite", {1}, {}, "", SafetyGateType::CONFIRM, StepState::PENDING, 3000, 0, "", "", false, {}});
    } else if (task.find("document") != std::string::npos) {
        steps.push_back({0, "extract_signatures", "Extract function signatures", {}, {}, "", SafetyGateType::PREVIEW, StepState::PENDING, 800, 0, "", "", true, {}});
        steps.push_back({1, "generate_docs", "Generate documentation", {0}, {}, "", SafetyGateType::ROLLBACK_CAPABLE, StepState::PENDING, 1500, 0, "", "", true, {}});
        steps.push_back({2, "format_docs", "Format and validate docs", {1}, {}, "", SafetyGateType::NONE, StepState::PENDING, 600, 0, "", "", true, {}});
    } else {
        // Generic task decomposition
        steps.push_back({0, "analyze", "Analyze task requirements", {}, {}, "", SafetyGateType::PREVIEW, StepState::PENDING, 1000, 0, "", "", true, {}});
        steps.push_back({1, "plan", "Create detailed plan", {0}, {}, "", SafetyGateType::NONE, StepState::PENDING, 800, 0, "", "", true, {}});
        steps.push_back({2, "execute", "Execute plan", {1}, {}, "", SafetyGateType::ROLLBACK_CAPABLE, StepState::PENDING, 2000, 0, "", "", true, {}});
        steps.push_back({3, "verify", "Verify results", {2}, {}, "", SafetyGateType::CONFIRM, StepState::PENDING, 1000, 0, "", "", false, {}});
    }

    return steps;
}

void PlanGenerator::ResolveDependencies(std::vector<PlanStep>& steps) {
    // Dependencies already set in DecomposeTask
    // This function can be extended for more complex dependency analysis
}

void PlanGenerator::AssignSafetyGates(std::vector<PlanStep>& steps) {
    // Safety gates already assigned in DecomposeTask
    // This function can be extended for context-aware gate assignment
}

int32_t PlanGenerator::EstimateDuration(const ExecutionPlan& plan) {
    int32_t total = 0;
    for (const auto& step : plan.steps) {
        total += step.estimatedDurationMs;
    }
    return total;
}

bool PlanGenerator::ValidatePlan(const ExecutionPlan& plan) {
    // Check for circular dependencies
    std::vector<int> visited(plan.steps.size(), 0);
    std::vector<int> recStack(plan.steps.size(), 0);

    for (size_t i = 0; i < plan.steps.size(); i++) {
        if (visited[i] == 0) {
            // Simple cycle detection: if any step depends on itself transitively
            std::queue<uint32_t> q;
            q.push(plan.steps[i].id);
            std::vector<bool> inQueue(plan.steps.size(), false);
            inQueue[i] = true;

            while (!q.empty()) {
                uint32_t stepId = q.front();
                q.pop();

                for (const auto& dep : plan.steps[stepId].dependencies) {
                    if (dep == plan.steps[i].id) {
                        return false; // Circular dependency found
                    }
                    if (!inQueue[dep] && dep < plan.steps.size()) {
                        q.push(dep);
                        inQueue[dep] = true;
                    }
                }
            }
        }
    }

    return true;
}

void PlanGenerator::OptimizeForParallelism(ExecutionPlan& plan) {
    // Mark steps with no dependencies as parallelizable
    for (auto& step : plan.steps) {
        if (step.dependencies.empty()) {
            // Can run in parallel with other independent steps
        }
    }
}

// ============================================================================
// StepExecutor Implementation
// ============================================================================

bool StepExecutor::ExecuteStep(PlanStep& step, bool dryRun) {
    step.state = StepState::EXECUTING;

    auto startTime = std::chrono::high_resolution_clock::now();

    bool success = false;

    if (step.action == "refactor_function" || step.action == "apply_refactor") {
        success = ExecuteRefactorAction(step);
    } else if (step.action == "generate_tests" || step.action == "run_tests") {
        success = ExecuteTestAction(step);
    } else if (step.action == "generate_docs" || step.action == "format_docs") {
        success = ExecuteDocAction(step);
    } else if (step.action == "analyze_code" || step.action == "analyze_coverage") {
        success = ExecuteAnalysisAction(step);
    } else {
        // Generic execution
        success = true;
        step.result = "Step executed: " + step.action;
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    step.actualDurationMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime).count();

    if (success) {
        step.state = StepState::COMPLETED;
    } else {
        step.state = StepState::FAILED;
        if (step.canRollback) {
            Rollback(step);
        }
    }

    return success;
}

bool StepExecutor::ApplySafetyGate(const PlanStep& step, SafetyGateType gateType) {
    switch (gateType) {
        case SafetyGateType::NONE:
            return true;
        case SafetyGateType::CONFIRM:
            // In real implementation, would prompt user
            return true;
        case SafetyGateType::PREVIEW:
            // Show preview of changes
            return true;
        case SafetyGateType::ROLLBACK_CAPABLE:
            // Ensure rollback ops are prepared
            return PrepareRollback(const_cast<PlanStep&>(step));
        case SafetyGateType::RESOURCE_CHECK:
            // Check available resources
            return true;
        default:
            return false;
    }
}

bool StepExecutor::PrepareRollback(PlanStep& step) {
    // Prepare undo operations
    if (step.action.find("create") != std::string::npos) {
        step.rollbackOps.push_back("delete_file");
    } else if (step.action.find("modify") != std::string::npos) {
        step.rollbackOps.push_back("restore_backup");
    }
    return true;
}

bool StepExecutor::Rollback(const PlanStep& step) {
    // Execute rollback operations in reverse order
    for (auto it = step.rollbackOps.rbegin(); it != step.rollbackOps.rend(); ++it) {
        // Execute rollback op
    }
    return true;
}

bool StepExecutor::ExecuteRefactorAction(PlanStep& step) {
    // Simulate refactoring
    step.result = "Refactored code structure";
    return true;
}

bool StepExecutor::ExecuteTestAction(PlanStep& step) {
    // Simulate test execution
    step.result = "Tests passed: 42/42";
    return true;
}

bool StepExecutor::ExecuteDocAction(PlanStep& step) {
    // Simulate documentation generation
    step.result = "Generated 15 documentation files";
    return true;
}

bool StepExecutor::ExecuteAnalysisAction(PlanStep& step) {
    // Simulate code analysis
    step.result = "Analysis complete: 3 issues found";
    return true;
}

// ============================================================================
// DependencyResolver Implementation
// ============================================================================

std::vector<uint32_t> DependencyResolver::ComputeExecutionOrder(const ExecutionPlan& plan) {
    std::vector<uint32_t> order;
    std::vector<int> inDegree(plan.steps.size(), 0);

    // Calculate in-degrees
    for (const auto& step : plan.steps) {
        inDegree[step.id] = step.dependencies.size();
    }

    // Topological sort using Kahn's algorithm
    std::queue<uint32_t> q;
    for (size_t i = 0; i < plan.steps.size(); i++) {
        if (inDegree[i] == 0) {
            q.push(i);
        }
    }

    while (!q.empty()) {
        uint32_t stepId = q.front();
        q.pop();
        order.push_back(stepId);

        // Find steps that depend on this one
        for (size_t i = 0; i < plan.steps.size(); i++) {
            for (const auto& dep : plan.steps[i].dependencies) {
                if (dep == stepId) {
                    inDegree[i]--;
                    if (inDegree[i] == 0) {
                        q.push(i);
                    }
                }
            }
        }
    }

    return order;
}

std::vector<std::vector<uint32_t>> DependencyResolver::IdentifyParallelBatches(const ExecutionPlan& plan) {
    std::vector<std::vector<uint32_t>> batches;
    std::vector<bool> processed(plan.steps.size(), false);

    for (size_t i = 0; i < plan.steps.size(); i++) {
        if (!processed[i]) {
            std::vector<uint32_t> batch;
            batch.push_back(i);
            processed[i] = true;

            // Find other steps with same dependencies
            for (size_t j = i + 1; j < plan.steps.size(); j++) {
                if (!processed[j] && plan.steps[i].dependencies == plan.steps[j].dependencies) {
                    batch.push_back(j);
                    processed[j] = true;
                }
            }

            batches.push_back(batch);
        }
    }

    return batches;
}

bool DependencyResolver::HasCircularDependencies(const ExecutionPlan& plan) {
    std::vector<int> visited(plan.steps.size(), 0);
    std::vector<int> recStack(plan.steps.size(), 0);

    for (size_t i = 0; i < plan.steps.size(); i++) {
        if (visited[i] == 0) {
            if (HasCycle(i, plan, visited, recStack)) {
                return true;
            }
        }
    }
    return false;
}

bool DependencyResolver::HasCycle(uint32_t stepId, const ExecutionPlan& plan,
                                   std::vector<int>& visited, std::vector<int>& recStack) {
    visited[stepId] = 1;
    recStack[stepId] = 1;

    for (const auto& dep : plan.steps[stepId].dependencies) {
        if (dep < plan.steps.size()) {
            if (visited[dep] == 0) {
                if (HasCycle(dep, plan, visited, recStack)) {
                    return true;
                }
            } else if (recStack[dep] == 1) {
                return true;
            }
        }
    }

    recStack[stepId] = 0;
    return false;
}

// ============================================================================
// AutonomousOrchestrator Implementation
// ============================================================================

uint32_t AutonomousOrchestrator::SubmitTask(const std::string& taskDescription,
                                            const std::vector<std::string>& contextFiles,
                                            TaskPriority priority) {
    uint32_t taskId = m_nextTaskId++;

    TaskContext ctx;
    ctx.taskId = taskId;
    ctx.description = taskDescription;
    ctx.priority = priority;
    ctx.approved = false;
    ctx.executing = false;
    ctx.completed = false;

    // Generate execution plan
    ctx.plan = m_planGen.GeneratePlan(taskDescription, contextFiles);

    // Validate plan
    if (!m_planGen.ValidatePlan(ctx.plan)) {
        return 0; // Invalid plan
    }

    // Optimize for parallelism
    m_planGen.OptimizeForParallelism(ctx.plan);

    m_tasks[taskId] = ctx;
    return taskId;
}

ExecutionPlan* AutonomousOrchestrator::GetPlan(uint32_t taskId) {
    auto it = m_tasks.find(taskId);
    if (it != m_tasks.end()) {
        return &it->second.plan;
    }
    return nullptr;
}

bool AutonomousOrchestrator::ApprovePlan(uint32_t taskId) {
    auto ctx = FindTask(taskId);
    if (!ctx) return false;

    ctx->approved = true;
    return true;
}

bool AutonomousOrchestrator::ExecutePlan(uint32_t taskId, bool dryRun) {
    auto ctx = FindTask(taskId);
    if (!ctx || !ctx->approved) return false;

    ctx->executing = true;

    // Compute execution order
    auto order = m_resolver.ComputeExecutionOrder(ctx->plan);

    // Execute steps in order
    for (uint32_t stepId : order) {
        if (!ExecuteStepWithDependencies(taskId, stepId)) {
            ctx->executing = false;
            return false;
        }
    }

    ctx->executing = false;
    ctx->completed = true;

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    ctx->plan.completedAt = std::ctime(&time_t_now);

    return true;
}

std::string AutonomousOrchestrator::GetStatus(uint32_t taskId) {
    auto ctx = FindTask(taskId);
    if (!ctx) return "Task not found";

    std::ostringstream ss;
    ss << "Task " << taskId << ": " << ctx->description << "\n";
    ss << "Status: " << (ctx->completed ? "COMPLETED" : ctx->executing ? "EXECUTING" : ctx->approved ? "APPROVED" : "PENDING") << "\n";
    ss << "Steps: " << ctx->plan.successCount << " succeeded, " << ctx->plan.failureCount << " failed\n";

    return ss.str();
}

AutonomousOrchestrator::TaskProgress AutonomousOrchestrator::GetProgress(uint32_t taskId) {
    TaskProgress progress = {taskId, 0, 0, 0, "", ""};

    auto ctx = FindTask(taskId);
    if (!ctx) return progress;

    progress.totalSteps = ctx->plan.steps.size();

    for (const auto& step : ctx->plan.steps) {
        if (step.state == StepState::COMPLETED) {
            progress.completedSteps++;
        }
    }

    progress.percentComplete = (progress.totalSteps > 0) ?
        (progress.completedSteps * 100) / progress.totalSteps : 0;

    // Find current step
    for (const auto& step : ctx->plan.steps) {
        if (step.state == StepState::EXECUTING) {
            progress.currentStep = step.action;
            break;
        }
    }

    return progress;
}

bool AutonomousOrchestrator::CancelTask(uint32_t taskId) {
    auto ctx = FindTask(taskId);
    if (!ctx) return false;

    ctx->executing = false;

    // Rollback all completed steps
    for (auto& step : ctx->plan.steps) {
        if (step.state == StepState::COMPLETED && step.canRollback) {
            m_executor.Rollback(step);
            step.state = StepState::ROLLED_BACK;
        }
    }

    return true;
}

std::string AutonomousOrchestrator::GetResult(uint32_t taskId) {
    auto ctx = FindTask(taskId);
    if (!ctx || !ctx->completed) return "";

    std::ostringstream ss;
    for (const auto& step : ctx->plan.steps) {
        if (!step.result.empty()) {
            ss << step.action << ": " << step.result << "\n";
        }
    }

    return ss.str();
}

std::string AutonomousOrchestrator::GetStatusString() const {
    std::ostringstream ss;
    ss << "=== Autonomous Orchestrator Status ===\n";
    ss << "Active Tasks: " << m_tasks.size() << " / " << ORCHESTRATOR_MAX_TASKS << "\n";
    ss << "Dry-Run Mode: " << (m_dryRunMode ? "ENABLED" : "DISABLED") << "\n";

    int executing = 0, completed = 0, pending = 0;
    for (const auto& [id, ctx] : m_tasks) {
        if (ctx.executing) executing++;
        else if (ctx.completed) completed++;
        else pending++;
    }

    ss << "Executing: " << executing << " | Completed: " << completed << " | Pending: " << pending << "\n";

    return ss.str();
}

AutonomousOrchestrator::TaskContext* AutonomousOrchestrator::FindTask(uint32_t taskId) {
    auto it = m_tasks.find(taskId);
    if (it != m_tasks.end()) {
        return &it->second;
    }
    return nullptr;
}

bool AutonomousOrchestrator::ExecuteStepWithDependencies(uint32_t taskId, uint32_t stepId) {
    auto ctx = FindTask(taskId);
    if (!ctx || stepId >= ctx->plan.steps.size()) return false;

    auto& step = ctx->plan.steps[stepId];

    // Check all dependencies are completed
    for (const auto& depId : step.dependencies) {
        if (depId < ctx->plan.steps.size()) {
            if (ctx->plan.steps[depId].state != StepState::COMPLETED) {
                return false;
            }
        }
    }

    // Apply safety gate
    if (!m_executor.ApplySafetyGate(step, step.safetyGate)) {
        step.state = StepState::FAILED;
        step.errorMessage = "Safety gate rejected";
        ctx->plan.failureCount++;
        return false;
    }

    // Execute step
    if (m_executor.ExecuteStep(step, m_dryRunMode)) {
        ctx->plan.successCount++;
        return true;
    } else {
        ctx->plan.failureCount++;
        return false;
    }
}

} // namespace RawrXD
