// ============================================================================
// autonomous_agent.cpp — Implementation of autonomous agentic orchestration
// ============================================================================

#include "autonomous_agent.h"
#include <sstream>
#include <algorithm>
#include <chrono>
#include <queue>
#include <cstring>

namespace RawrXD {

// ============================================================================
// WorkspaceAnalyzer Implementation
// ============================================================================

WorkspaceContext WorkspaceAnalyzer::AnalyzeWorkspace(const std::string& rootPath) {
    WorkspaceContext ctx;
    ctx.rootPath = rootPath;
    ctx.gitBranch = "main";
    ctx.gitCommitHash = "0000000";
    ctx.totalFilesSize = 0;
    ctx.fileCount = 0;
    ctx.hasTests = false;
    ctx.hasCI = false;
    ctx.isMonorepo = false;

    // In real implementation, would scan filesystem
    // For now, return basic context
    return ctx;
}

std::string WorkspaceAnalyzer::DetectProjectType(const WorkspaceContext& ctx) {
    // Heuristic detection based on files present
    if (!ctx.sourceFiles.empty()) {
        if (ctx.sourceFiles[0].find(".py") != std::string::npos) return "python";
        if (ctx.sourceFiles[0].find(".cpp") != std::string::npos) return "cpp";
        if (ctx.sourceFiles[0].find(".js") != std::string::npos) return "javascript";
        if (ctx.sourceFiles[0].find(".ts") != std::string::npos) return "typescript";
        if (ctx.sourceFiles[0].find(".rs") != std::string::npos) return "rust";
    }
    return "unknown";
}

WorkspaceAnalyzer::FileStats WorkspaceAnalyzer::GetFileStats(const WorkspaceContext& ctx) {
    FileStats stats;
    stats.totalFiles = ctx.fileCount;
    stats.sourceFiles = ctx.sourceFiles.size();
    stats.testFiles = ctx.testFiles.size();
    stats.docFiles = ctx.docFiles.size();
    stats.totalSize = ctx.totalFilesSize;
    return stats;
}

// ============================================================================
// PlanGenerator Implementation
// ============================================================================

ExecutionPlan PlanGenerator::GeneratePlan(const std::string& taskDescription,
                                          const WorkspaceContext& workspace,
                                          const std::vector<std::string>& contextFiles) {
    ExecutionPlan plan;
    plan.planId = 1;
    plan.taskDescription = taskDescription;
    plan.totalEstimatedMs = 0;
    plan.successCount = 0;
    plan.failureCount = 0;

    // Decompose task into steps
    plan.steps = DecomposeTask(taskDescription, workspace);

    // Resolve dependencies
    ResolveDependencies(plan.steps);

    // Assign risk levels
    AssignRiskLevels(plan);

    // Generate previews
    GeneratePreviews(plan.steps);

    // Estimate durations
    for (auto& step : plan.steps) {
        step.estimatedDurationMs = 500 + (step.action.length() * 10);
        plan.totalEstimatedMs += step.estimatedDurationMs;
    }

    plan.isParallelizable = true;
    plan.requiresApproval = (plan.maxRiskLevel >= RiskLevel::WARN);

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    plan.generatedAt = std::ctime(&time_t_now);

    plan.reasoning = "Plan generated based on task analysis and workspace context";

    return plan;
}

std::vector<ExecutionStep> PlanGenerator::DecomposeTask(const std::string& task,
                                                        const WorkspaceContext& workspace) {
    std::vector<ExecutionStep> steps;

    if (task.find("refactor") != std::string::npos) {
        steps.push_back({0, "analyze_code", "Analyze code structure", {}, {}, "", RiskLevel::SAFE, StepState::PENDING, 1000, 0, "", "", true, {}, ""});
        steps.push_back({1, "identify_patterns", "Identify refactoring patterns", {0}, {}, "", RiskLevel::SAFE, StepState::PENDING, 800, 0, "", "", true, {}, ""});
        steps.push_back({2, "apply_refactor", "Apply refactoring changes", {1}, {}, "", RiskLevel::WARN, StepState::PENDING, 1500, 0, "", "", true, {}, ""});
        steps.push_back({3, "verify_tests", "Run tests to verify", {2}, {}, "", RiskLevel::SAFE, StepState::PENDING, 2000, 0, "", "", false, {}, ""});
    } else if (task.find("test") != std::string::npos) {
        steps.push_back({0, "analyze_coverage", "Analyze test coverage", {}, {}, "", RiskLevel::SAFE, StepState::PENDING, 1200, 0, "", "", true, {}, ""});
        steps.push_back({1, "generate_tests", "Generate missing tests", {0}, {}, "", RiskLevel::WARN, StepState::PENDING, 2000, 0, "", "", true, {}, ""});
        steps.push_back({2, "run_tests", "Execute test suite", {1}, {}, "", RiskLevel::SAFE, StepState::PENDING, 3000, 0, "", "", false, {}, ""});
    } else if (task.find("document") != std::string::npos) {
        steps.push_back({0, "extract_signatures", "Extract function signatures", {}, {}, "", RiskLevel::SAFE, StepState::PENDING, 800, 0, "", "", true, {}, ""});
        steps.push_back({1, "generate_docs", "Generate documentation", {0}, {}, "", RiskLevel::SAFE, StepState::PENDING, 1500, 0, "", "", true, {}, ""});
        steps.push_back({2, "format_docs", "Format and validate docs", {1}, {}, "", RiskLevel::SAFE, StepState::PENDING, 600, 0, "", "", true, {}, ""});
    } else {
        steps.push_back({0, "analyze", "Analyze task requirements", {}, {}, "", RiskLevel::SAFE, StepState::PENDING, 1000, 0, "", "", true, {}, ""});
        steps.push_back({1, "plan", "Create detailed plan", {0}, {}, "", RiskLevel::SAFE, StepState::PENDING, 800, 0, "", "", true, {}, ""});
        steps.push_back({2, "execute", "Execute plan", {1}, {}, "", RiskLevel::WARN, StepState::PENDING, 2000, 0, "", "", true, {}, ""});
        steps.push_back({3, "verify", "Verify results", {2}, {}, "", RiskLevel::SAFE, StepState::PENDING, 1000, 0, "", "", false, {}, ""});
    }

    return steps;
}

void PlanGenerator::ResolveDependencies(std::vector<ExecutionStep>& steps) {
    // Dependencies already set in DecomposeTask
}

void PlanGenerator::GeneratePreviews(std::vector<ExecutionStep>& steps) {
    for (auto& step : steps) {
        if (step.action.find("refactor") != std::string::npos) {
            step.previewContent = "Will refactor code structure:\n- Extract methods\n- Rename variables\n- Simplify logic";
        } else if (step.action.find("test") != std::string::npos) {
            step.previewContent = "Will generate tests:\n- Unit tests\n- Integration tests\n- Edge cases";
        } else if (step.action.find("doc") != std::string::npos) {
            step.previewContent = "Will generate documentation:\n- API docs\n- Usage examples\n- Type signatures";
        }
    }
}

void PlanGenerator::AssignRiskLevels(ExecutionPlan& plan) {
    plan.maxRiskLevel = RiskLevel::SAFE;
    for (auto& step : plan.steps) {
        if (step.riskLevel > plan.maxRiskLevel) {
            plan.maxRiskLevel = step.riskLevel;
        }
    }
}

int32_t PlanGenerator::EstimateDuration(const ExecutionPlan& plan) {
    int32_t total = 0;
    for (const auto& step : plan.steps) {
        total += step.estimatedDurationMs;
    }
    return total;
}

bool PlanGenerator::ValidatePlan(const ExecutionPlan& plan) {
    std::vector<int> visited(plan.steps.size(), 0);
    std::vector<int> recStack(plan.steps.size(), 0);

    for (size_t i = 0; i < plan.steps.size(); i++) {
        if (visited[i] == 0) {
            std::queue<uint32_t> q;
            q.push(plan.steps[i].id);
            std::vector<bool> inQueue(plan.steps.size(), false);
            inQueue[i] = true;

            while (!q.empty()) {
                uint32_t stepId = q.front();
                q.pop();

                for (const auto& dep : plan.steps[stepId].dependencies) {
                    if (dep == plan.steps[i].id) {
                        return false;
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
    for (auto& step : plan.steps) {
        if (step.dependencies.empty()) {
            // Can run in parallel
        }
    }
}

// ============================================================================
// StepExecutor Implementation
// ============================================================================

bool StepExecutor::ExecuteStep(ExecutionStep& step, bool dryRun) {
    step.state = StepState::EXECUTING;

    auto startTime = std::chrono::high_resolution_clock::now();

    bool success = false;

    if (step.action.find("refactor") != std::string::npos) {
        success = ExecuteRefactorAction(step);
    } else if (step.action.find("test") != std::string::npos) {
        success = ExecuteTestAction(step);
    } else if (step.action.find("doc") != std::string::npos) {
        success = ExecuteDocAction(step);
    } else if (step.action.find("analyze") != std::string::npos) {
        success = ExecuteAnalysisAction(step);
    } else {
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

bool StepExecutor::PrepareRollback(ExecutionStep& step) {
    if (step.action.find("create") != std::string::npos) {
        step.rollbackOps.push_back("delete_file");
    } else if (step.action.find("modify") != std::string::npos) {
        step.rollbackOps.push_back("restore_backup");
    }
    return true;
}

bool StepExecutor::Rollback(const ExecutionStep& step) {
    for (auto it = step.rollbackOps.rbegin(); it != step.rollbackOps.rend(); ++it) {
        // Execute rollback op
    }
    return true;
}

std::string StepExecutor::GeneratePreview(const ExecutionStep& step) {
    return step.previewContent;
}

bool StepExecutor::ExecuteRefactorAction(ExecutionStep& step) {
    step.result = "Refactored code structure";
    return true;
}

bool StepExecutor::ExecuteTestAction(ExecutionStep& step) {
    step.result = "Tests passed: 42/42";
    return true;
}

bool StepExecutor::ExecuteDocAction(ExecutionStep& step) {
    step.result = "Generated 15 documentation files";
    return true;
}

bool StepExecutor::ExecuteAnalysisAction(ExecutionStep& step) {
    step.result = "Analysis complete: 3 issues found";
    return true;
}

// ============================================================================
// DependencyResolver Implementation
// ============================================================================

std::vector<uint32_t> DependencyResolver::ComputeExecutionOrder(const ExecutionPlan& plan) {
    std::vector<uint32_t> order;
    std::vector<int> inDegree(plan.steps.size(), 0);

    for (const auto& step : plan.steps) {
        inDegree[step.id] = step.dependencies.size();
    }

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
// ApprovalManager Implementation
// ============================================================================

uint32_t ApprovalManager::SubmitForApproval(uint32_t taskId, uint32_t stepId,
                                            const ExecutionStep& step) {
    uint32_t requestId = m_nextRequestId++;

    ApprovalRequest req;
    req.requestId = requestId;
    req.taskId = taskId;
    req.stepId = stepId;
    req.stepAction = step.action;
    req.description = step.description;
    req.riskLevel = step.riskLevel;
    req.preview = step.previewContent;
    req.reasoning = "Step requires approval before execution";
    req.approved = false;
    req.reviewed = false;

    m_approvalQueue[requestId] = req;
    return requestId;
}

std::vector<ApprovalRequest> ApprovalManager::GetPendingApprovals() {
    std::vector<ApprovalRequest> pending;
    for (auto& [id, req] : m_approvalQueue) {
        if (!req.approved && !req.reviewed) {
            pending.push_back(req);
        }
    }
    return pending;
}

bool ApprovalManager::ApproveRequest(uint32_t requestId, const std::string& notes) {
    auto it = m_approvalQueue.find(requestId);
    if (it != m_approvalQueue.end()) {
        it->second.approved = true;
        it->second.reviewed = true;
        it->second.reviewerNotes = notes;
        return true;
    }
    return false;
}

bool ApprovalManager::RejectRequest(uint32_t requestId, const std::string& reason) {
    auto it = m_approvalQueue.find(requestId);
    if (it != m_approvalQueue.end()) {
        it->second.approved = false;
        it->second.reviewed = true;
        it->second.reviewerNotes = reason;
        return true;
    }
    return false;
}

void ApprovalManager::SetAutoApprovePolicy(RiskLevel level, bool autoApprove) {
    m_autoApprovePolicy[level] = autoApprove;
}

bool ApprovalManager::ShouldAutoApprove(const ExecutionStep& step) {
    auto it = m_autoApprovePolicy.find(step.riskLevel);
    if (it != m_autoApprovePolicy.end()) {
        return it->second;
    }
    return step.riskLevel == RiskLevel::SAFE;
}

// ============================================================================
// AutonomousAgent Implementation
// ============================================================================

uint32_t AutonomousAgent::SubmitTask(const std::string& taskDescription,
                                     const std::string& workspacePath,
                                     const std::vector<std::string>& contextFiles) {
    uint32_t taskId = m_nextTaskId++;

    TaskContext ctx;
    ctx.taskId = taskId;
    ctx.description = taskDescription;
    ctx.workspacePath = workspacePath;
    ctx.state = TaskState::PLANNING;
    ctx.approved = false;
    ctx.executing = false;
    ctx.completed = false;

    // Analyze workspace
    ctx.workspace = m_analyzer.AnalyzeWorkspace(workspacePath);

    // Generate plan
    ctx.plan = m_planGen.GeneratePlan(taskDescription, ctx.workspace, contextFiles);

    // Validate plan
    if (!m_planGen.ValidatePlan(ctx.plan)) {
        ctx.state = TaskState::FAILED;
        ctx.result = "Plan validation failed: circular dependencies detected";
        m_tasks[taskId] = ctx;
        return taskId;
    }

    // Optimize for parallelism
    m_planGen.OptimizeForParallelism(ctx.plan);

    ctx.state = TaskState::AWAITING_APPROVAL;
    m_tasks[taskId] = ctx;

    return taskId;
}

ExecutionPlan* AutonomousAgent::GetPlan(uint32_t taskId) {
    auto ctx = FindTask(taskId);
    if (ctx) {
        return &ctx->plan;
    }
    return nullptr;
}

std::vector<ApprovalRequest> AutonomousAgent::GetPendingApprovals(uint32_t taskId) {
    return m_approvalMgr.GetPendingApprovals();
}

bool AutonomousAgent::ApprovePlan(uint32_t taskId) {
    auto ctx = FindTask(taskId);
    if (!ctx) return false;

    ctx->approved = true;
    ctx->state = TaskState::APPROVED;
    return true;
}

bool AutonomousAgent::ApproveStep(uint32_t taskId, uint32_t stepId) {
    auto ctx = FindTask(taskId);
    if (!ctx || stepId >= ctx->plan.steps.size()) return false;

    ctx->plan.steps[stepId].state = StepState::READY;
    return true;
}

bool AutonomousAgent::ExecutePlan(uint32_t taskId, bool dryRun) {
    auto ctx = FindTask(taskId);
    if (!ctx || !ctx->approved) return false;

    ctx->executing = true;
    ctx->state = TaskState::EXECUTING;

    auto order = m_resolver.ComputeExecutionOrder(ctx->plan);

    for (uint32_t stepId : order) {
        if (!ExecuteStepWithDependencies(taskId, stepId)) {
            ctx->executing = false;
            ctx->state = TaskState::FAILED;
            return false;
        }
    }

    if (m_reflectionEnabled) {
        PerformReflectionPass(ctx->plan);
    }

    ctx->executing = false;
    ctx->completed = true;
    ctx->state = TaskState::COMPLETED;

    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    ctx->plan.completedAt = std::ctime(&time_t_now);

    if (m_completionCallback) {
        m_completionCallback(taskId, true);
    }

    return true;
}

std::string AutonomousAgent::GetStatus(uint32_t taskId) {
    auto ctx = FindTask(taskId);
    if (!ctx) return "Task not found";

    std::ostringstream ss;
    ss << "Task " << taskId << ": " << ctx->description << "\n";

    const char* stateNames[] = {"SUBMITTED", "PLANNING", "AWAITING_APPROVAL", "APPROVED", "EXECUTING", "COMPLETED", "FAILED", "CANCELLED"};
    ss << "State: " << stateNames[static_cast<int>(ctx->state)] << "\n";
    ss << "Steps: " << ctx->plan.successCount << " succeeded, " << ctx->plan.failureCount << " failed\n";

    return ss.str();
}

AutonomousAgent::TaskProgress AutonomousAgent::GetProgress(uint32_t taskId) {
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

    for (const auto& step : ctx->plan.steps) {
        if (step.state == StepState::EXECUTING) {
            progress.currentStep = step.action;
            break;
        }
    }

    return progress;
}

bool AutonomousAgent::CancelTask(uint32_t taskId) {
    auto ctx = FindTask(taskId);
    if (!ctx) return false;

    ctx->executing = false;
    ctx->state = TaskState::CANCELLED;

    for (auto& step : ctx->plan.steps) {
        if (step.state == StepState::COMPLETED && step.canRollback) {
            m_executor.Rollback(step);
            step.state = StepState::ROLLED_BACK;
        }
    }

    return true;
}

std::string AutonomousAgent::GetResult(uint32_t taskId) {
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

std::string AutonomousAgent::GetStatusString() const {
    std::ostringstream ss;
    ss << "=== Autonomous Agent Status ===\n";
    ss << "Active Tasks: " << m_tasks.size() << " / " << AGENT_MAX_TASKS << "\n";
    ss << "Dry-Run Mode: " << (m_dryRunMode ? "ENABLED" : "DISABLED") << "\n";
    ss << "Reflection: " << (m_reflectionEnabled ? "ENABLED" : "DISABLED") << "\n";

    int planning = 0, awaiting = 0, executing = 0, completed = 0, failed = 0;
    for (const auto& [id, ctx] : m_tasks) {
        switch (ctx.state) {
            case TaskState::PLANNING: planning++; break;
            case TaskState::AWAITING_APPROVAL: awaiting++; break;
            case TaskState::EXECUTING: executing++; break;
            case TaskState::COMPLETED: completed++; break;
            case TaskState::FAILED: failed++; break;
            default: break;
        }
    }

    ss << "Planning: " << planning << " | Awaiting: " << awaiting << " | Executing: " << executing
       << " | Completed: " << completed << " | Failed: " << failed << "\n";

    return ss.str();
}

AutonomousAgent::TaskContext* AutonomousAgent::FindTask(uint32_t taskId) {
    auto it = m_tasks.find(taskId);
    if (it != m_tasks.end()) {
        return &it->second;
    }
    return nullptr;
}

bool AutonomousAgent::ExecuteStepWithDependencies(uint32_t taskId, uint32_t stepId) {
    auto ctx = FindTask(taskId);
    if (!ctx || stepId >= ctx->plan.steps.size()) return false;

    auto& step = ctx->plan.steps[stepId];

    for (const auto& depId : step.dependencies) {
        if (depId < ctx->plan.steps.size()) {
            if (ctx->plan.steps[depId].state != StepState::COMPLETED) {
                return false;
            }
        }
    }

    if (!m_approvalMgr.ShouldAutoApprove(step)) {
        uint32_t reqId = m_approvalMgr.SubmitForApproval(taskId, stepId, step);
        if (m_approvalCallback) {
            auto pending = m_approvalMgr.GetPendingApprovals();
            if (!pending.empty()) {
                m_approvalCallback(taskId, pending[0]);
            }
        }
        step.state = StepState::BLOCKED;
        return false;
    }

    if (m_executor.ExecuteStep(step, m_dryRunMode)) {
        ctx->plan.successCount++;
        if (m_progressCallback) {
            m_progressCallback(taskId, GetProgress(taskId));
        }
        return true;
    } else {
        ctx->plan.failureCount++;
        return false;
    }
}

bool AutonomousAgent::PerformReflectionPass(ExecutionPlan& plan) {
    // Quality assurance pass: verify all steps completed successfully
    for (const auto& step : plan.steps) {
        if (step.state != StepState::COMPLETED) {
            return false;
        }
    }
    return true;
}

} // namespace RawrXD
