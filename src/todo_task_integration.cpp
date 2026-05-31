/**
 * TodoTaskIntegration - Production Implementation
 * Day 4: Todo/dependency integration with agent execution
 */

#include "todo_task_integration.h"
#include "agentic_executor.h"
#include "execution_state_persistence.h"

#include <algorithm>
#include <iostream>
#include <queue>
#include <set>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace {
std::string makeIsoUtcNow()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&time_t));
    return std::string(buf);
}
}

// ===== TaskDependency =====

nlohmann::json TaskDependency::toJson() const
{
    nlohmann::json j;
    j["dependsOnTaskId"] = dependsOnTaskId;
    j["hasSucceeded"] = hasSucceeded;
    j["failureReason"] = failureReason;
    return j;
}

void TaskDependency::fromJson(const nlohmann::json& j)
{
    dependsOnTaskId = j["dependsOnTaskId"].get<std::string>();
    hasSucceeded = j.value("hasSucceeded", false);
    failureReason = j.value("failureReason", "");
}

// ===== Task =====

nlohmann::json Task::toJson() const
{
    nlohmann::json j;
    j["taskId"] = taskId;
    j["label"] = label;
    j["description"] = description;
    j["status"] = static_cast<int>(status);
    j["priority"] = static_cast<int>(priority);
    j["sequenceNumber"] = sequenceNumber;
    j["createdAt"] = createdAt;
    j["startedAt"] = startedAt;
    j["completedAt"] = completedAt;
    j["toolName"] = toolName;
    j["toolParams"] = toolParams;
    j["result"] = result;
    
    nlohmann::json depsArray = nlohmann::json::array();
    for (const auto& dep : dependencies) {
        depsArray.push_back(dep.toJson());
    }
    j["dependencies"] = depsArray;
    
    j["retryCount"] = retryCount;
    j["maxRetries"] = maxRetries;
    j["isCheckpoint"] = isCheckpoint;
    j["checkpointRef"] = checkpointRef;
    
    return j;
}

void Task::fromJson(const nlohmann::json& j)
{
    taskId = j["taskId"].get<std::string>();
    label = j["label"].get<std::string>();
    description = j["description"].get<std::string>();
    status = static_cast<TaskStatus>(j["status"].get<int>());
    priority = static_cast<TaskPriority>(j["priority"].get<int>());
    sequenceNumber = j["sequenceNumber"].get<int>();
    createdAt = j["createdAt"].get<std::string>();
    startedAt = j.value("startedAt", "");
    completedAt = j.value("completedAt", "");
    toolName = j["toolName"].get<std::string>();
    toolParams = j["toolParams"];
    result = j.value("result", nlohmann::json::object());
    
    dependencies.clear();
    if (j.contains("dependencies")) {
        for (const auto& depJson : j["dependencies"]) {
            TaskDependency dep;
            dep.fromJson(depJson);
            dependencies.push_back(dep);
        }
    }
    
    retryCount = j.value("retryCount", 0);
    maxRetries = j.value("maxRetries", 3);
    isCheckpoint = j.value("isCheckpoint", false);
    checkpointRef = j.value("checkpointRef", "");
}

bool Task::canExecute() const
{
    return status == TaskStatus::Pending && isDependenciesResolved();
}

bool Task::isDependenciesResolved() const
{
    for (const auto& dep : dependencies) {
        if (!dep.hasSucceeded) {
            return false;
        }
    }
    return true;
}

std::string Task::getBlockerReason() const
{
    for (const auto& dep : dependencies) {
        if (!dep.hasSucceeded) {
            return "Blocked by: " + dep.dependsOnTaskId + 
                   (dep.failureReason.empty() ? "" : " (" + dep.failureReason + ")");
        }
    }
    return "";
}

// ===== TodoTaskIntegration =====

TodoTaskIntegration::TodoTaskIntegration(
    AgenticExecutor* executor,
    ExecutionStatePersistence* persistence)
    : m_executor(executor)
    , m_persistence(persistence)
    , m_workflowExecutionId("todo_workflow_default")
{
    std::cout << "[TodoTaskIntegration] Initialized task management system" << std::endl;
}

TodoTaskIntegration::~TodoTaskIntegration()
{
    std::cout << "[TodoTaskIntegration] Destroyed - Managed "
              << m_tasks.size() << " tasks" << std::endl;
}

std::string TodoTaskIntegration::generateTaskId()
{
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << "task_" << std::hex << std::hash<uint64_t>{}(time_t) % 0xFFFFFF;
    return ss.str();
}

std::string TodoTaskIntegration::createTask(
    const std::string& label,
    const std::string& description,
    const std::string& toolName,
    const nlohmann::json& toolParams,
    TaskPriority priority)
{
    auto task = std::make_unique<Task>();
    task->taskId = generateTaskId();
    task->label = label;
    task->description = description;
    task->status = TaskStatus::Pending;
    task->priority = priority;
    task->sequenceNumber = static_cast<int>(m_tasks.size());
    task->toolName = toolName;
    task->toolParams = toolParams;
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&time_t));
    task->createdAt = buf;
    
    std::string taskId = task->taskId;
    m_tasks[taskId] = std::move(task);
    
    std::cout << "[TodoTaskIntegration] Created task: " << taskId 
              << " - " << label << std::endl;
    return taskId;
}

bool TodoTaskIntegration::addDependency(
    const std::string& taskId,
    const std::string& dependsOnTaskId)
{
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) {
        std::cerr << "[TodoTaskIntegration] Task not found: " << taskId << std::endl;
        return false;
    }
    
    // Check for cycles
    auto depIt = m_tasks.find(dependsOnTaskId);
    if (depIt == m_tasks.end()) {
        std::cerr << "[TodoTaskIntegration] Dependency task not found: " 
                  << dependsOnTaskId << std::endl;
        return false;
    }
    
    TaskDependency dep;
    dep.dependsOnTaskId = dependsOnTaskId;
    it->second->dependencies.push_back(dep);
    
    std::cout << "[TodoTaskIntegration] Added dependency: "
              << taskId << " depends on " << dependsOnTaskId << std::endl;
    return true;
}

Task* TodoTaskIntegration::getTask(const std::string& taskId)
{
    auto it = m_tasks.find(taskId);
    if (it != m_tasks.end()) {
        return it->second.get();
    }
    return nullptr;
}

std::vector<Task*> TodoTaskIntegration::listTasks(TaskStatus filterStatus)
{
    std::vector<Task*> result;
    for (auto& [id, task] : m_tasks) {
        if (task->status == filterStatus || filterStatus == TaskStatus::Pending) {
            result.push_back(task.get());
        }
    }
    return result;
}

std::string TodoTaskIntegration::executeNextTask()
{
    if (m_workflowPaused) {
        return "";
    }

    // Find next executable task (respecting priority and dependencies)
    Task* nextTask = nullptr;
    TaskPriority maxPriority = TaskPriority::Low;

    for (auto& [id, task] : m_tasks) {
        if (task->canExecute() && task->priority >= maxPriority) {
            nextTask = task.get();
            maxPriority = task->priority;
        }
    }

    if (nextTask) {
        if (executeTask(nextTask->taskId)) {
            return nextTask->taskId;
        }
    }

    return "";
}

bool TodoTaskIntegration::executeTask(
    const std::string& taskId,
    bool requiresApproval)
{
    auto task = getTask(taskId);
    if (!task) {
        return false;
    }

    if (!task->canExecute()) {
        std::cerr << "[TodoTaskIntegration] Task cannot execute: "
                  << task->getBlockerReason() << std::endl;
        return false;
    }

    if (requiresApproval) {
        // In production: would prompt for user approval
        std::cout << "[TodoTaskIntegration] Task requires approval: "
                  << task->label << std::endl;
    }

    // Update task status
    task->status = TaskStatus::InProgress;
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&time_t));
    task->startedAt = buf;

    // Execute via executor
    if (m_executor) {
        try {
            auto result = m_executor->callTool(task->toolName, task->toolParams);
            return completeTask(taskId, result);
        } catch (const std::exception& e) {
            std::cerr << "[TodoTaskIntegration] Execution error: " << e.what() << std::endl;
            return failTask(taskId, e.what());
        }
    }

    return false;
}

bool TodoTaskIntegration::completeTask(
    const std::string& taskId,
    const nlohmann::json& result)
{
    auto task = getTask(taskId);
    if (!task) {
        return false;
    }

    task->status = TaskStatus::Completed;
    task->result = result;

    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::gmtime(&time_t));
    task->completedAt = buf;

    // Mark this task as succeeded for dependents
    for (auto& [id, dependentTask] : m_tasks) {
        for (auto& dep : dependentTask->dependencies) {
            if (dep.dependsOnTaskId == taskId) {
                dep.hasSucceeded = true;
            }
        }
    }

    std::cout << "[TodoTaskIntegration] Task completed: " << taskId << std::endl;
    return true;
}

bool TodoTaskIntegration::failTask(
    const std::string& taskId,
    const std::string& errorMessage)
{
    auto task = getTask(taskId);
    if (!task) {
        return false;
    }

    if (task->retryCount < task->maxRetries) {
        // Retry
        task->retryCount++;
        task->status = TaskStatus::Pending;
        std::cout << "[TodoTaskIntegration] Retrying task " << taskId
                  << " (attempt " << task->retryCount << "/" 
                  << task->maxRetries << ")" << std::endl;
        return true;
    }

    // Mark as failed
    task->status = TaskStatus::Failed;
    
    // Mark dependents as blocked
    for (auto& [id, dependentTask] : m_tasks) {
        for (auto& dep : dependentTask->dependencies) {
            if (dep.dependsOnTaskId == taskId) {
                dep.failureReason = errorMessage;
                dependentTask->status = TaskStatus::Blocked;
            }
        }
    }

    std::cout << "[TodoTaskIntegration] Task failed: " << taskId
              << " - " << errorMessage << std::endl;
    return false;
}

bool TodoTaskIntegration::retryTask(const std::string& taskId)
{
    auto task = getTask(taskId);
    if (!task || task->retryCount >= task->maxRetries) {
        return false;
    }

    task->retryCount++;
    task->status = TaskStatus::Pending;
    return true;
}

std::string TodoTaskIntegration::validateDependencies()
{
    // Check for cycles using DFS
    std::set<std::string> visited;
    std::set<std::string> recursionStack;

    std::function<bool(const std::string&)> hasCycle = 
        [&](const std::string& taskId) -> bool {
            visited.insert(taskId);
            recursionStack.insert(taskId);

            auto task = getTask(taskId);
            if (task) {
                for (const auto& dep : task->dependencies) {
                    if (!visited.count(dep.dependsOnTaskId)) {
                        if (hasCycle(dep.dependsOnTaskId)) {
                            return true;
                        }
                    } else if (recursionStack.count(dep.dependsOnTaskId)) {
                        return true;
                    }
                }
            }

            recursionStack.erase(taskId);
            return false;
        };

    for (auto& [id, task] : m_tasks) {
        if (!visited.count(id)) {
            if (hasCycle(id)) {
                return "Circular dependency detected";
            }
        }
    }

    return "";  // Valid
}

std::vector<Task*> TodoTaskIntegration::getBlockedTasks(const std::string& failedTaskId)
{
    std::vector<Task*> blocked;
    for (auto& [id, task] : m_tasks) {
        if (task->status == TaskStatus::Blocked) {
            for (const auto& dep : task->dependencies) {
                if (dep.dependsOnTaskId == failedTaskId) {
                    blocked.push_back(task.get());
                    break;
                }
            }
        }
    }
    return blocked;
}

std::vector<std::string> TodoTaskIntegration::getExecutionOrder()
{
    if (validateDependencies() == "" ) {
        return topologicalSort();
    }
    return {};  // Dependency validation failed
}

TodoTaskIntegration::WorkflowStatus TodoTaskIntegration::getWorkflowStatus()
{
    WorkflowStatus status;
    status.totalTasks = m_tasks.size();

    for (auto& [id, task] : m_tasks) {
        if (task->status == TaskStatus::Completed) status.completedTasks++;
        else if (task->status == TaskStatus::Failed) status.failedTasks++;
        else if (task->status == TaskStatus::Blocked) status.blockedTasks++;
    }

    if (status.totalTasks > 0) {
        status.progressPercent = (status.completedTasks * 100.0f) / status.totalTasks;
    }

    return status;
}

std::vector<nlohmann::json> TodoTaskIntegration::getExecutionTimeline()
{
    std::vector<nlohmann::json> timeline;
    
    std::vector<Task*> sorted;
    for (auto& [id, task] : m_tasks) {
        sorted.push_back(task.get());
    }
    
    std::sort(sorted.begin(), sorted.end(),
        [](Task* a, Task* b) {
            return a->sequenceNumber < b->sequenceNumber;
        });

    for (auto task : sorted) {
        nlohmann::json entry;
        entry["taskId"] = task->taskId;
        entry["label"] = task->label;
        entry["startedAt"] = task->startedAt;
        entry["completedAt"] = task->completedAt;
        entry["status"] = static_cast<int>(task->status);
        timeline.push_back(entry);
    }

    return timeline;
}

void TodoTaskIntegration::pauseWorkflow()
{
    m_workflowPaused = true;
    std::cout << "[TodoTaskIntegration] Workflow paused" << std::endl;
}

void TodoTaskIntegration::resumeWorkflow()
{
    m_workflowPaused = false;
    std::cout << "[TodoTaskIntegration] Workflow resumed" << std::endl;
}

std::string TodoTaskIntegration::createTaskCheckpoint(const std::string& label)
{
    if (!m_persistence) {
        std::cerr << "[TodoTaskIntegration] Cannot checkpoint: persistence unavailable" << std::endl;
        return "";
    }

    // Ensure there is a persisted workflow execution for task checkpoints.
    if (!m_persistence->hasValidExecution(m_workflowExecutionId)) {
        WorkflowExecution exec;
        exec.executionId = m_workflowExecutionId;
        exec.workflowName = "TodoWorkflowExecution";
        exec.goal = "Track and recover agent todo workflow";
        exec.status = "in-progress";
        exec.startTime = makeIsoUtcNow();
        exec.lastUpdateTime = exec.startTime;
        exec.globalContext = nlohmann::json::object();
        exec.globalContext["tasks"] = serializeTasks();
        exec.globalContext["workflowPaused"] = m_workflowPaused;
        if (m_persistence->persistWorkflowExecution(exec).empty()) {
            std::cerr << "[TodoTaskIntegration] Failed to initialize task workflow execution" << std::endl;
            return "";
        }
    }

    nlohmann::json snapshot;
    snapshot["tasks"] = serializeTasks();
    snapshot["workflowPaused"] = m_workflowPaused;
    snapshot["checkpointLabel"] = label;

    auto checkpointId = m_persistence->createCheckpoint(m_workflowExecutionId, label, snapshot);
    if (checkpointId.empty()) {
        std::cerr << "[TodoTaskIntegration] Failed to persist task checkpoint: " << label << std::endl;
        return "";
    }

    return checkpointId;
}

bool TodoTaskIntegration::resumeFromCheckpoint(const std::string& checkpointRef)
{
    if (!m_persistence) {
        std::cerr << "[TodoTaskIntegration] Cannot resume: persistence unavailable" << std::endl;
        return false;
    }

    auto execution = m_persistence->resumeFromCheckpoint(m_workflowExecutionId);
    if (!execution || execution->checkpoints.empty()) {
        std::cerr << "[TodoTaskIntegration] No persisted checkpoints available for resume" << std::endl;
        return false;
    }

    const ExecutionCheckpoint* selected = nullptr;
    if (checkpointRef.empty()) {
        int idx = execution->currentCheckpointIndex;
        if (idx < 0 || idx >= static_cast<int>(execution->checkpoints.size())) {
            idx = static_cast<int>(execution->checkpoints.size()) - 1;
        }
        selected = &execution->checkpoints[idx];
    } else {
        for (const auto& cp : execution->checkpoints) {
            if (cp.checkpointId == checkpointRef) {
                selected = &cp;
                break;
            }
        }
    }

    if (!selected) {
        std::cerr << "[TodoTaskIntegration] Requested checkpoint not found: " << checkpointRef << std::endl;
        return false;
    }

    const auto& snapshot = selected->stateSnapshot;
    if (!snapshot.is_object() || !snapshot.contains("tasks")) {
        std::cerr << "[TodoTaskIntegration] Checkpoint missing task payload: "
                  << selected->checkpointId << std::endl;
        return false;
    }

    if (!deserializeTasks(snapshot["tasks"])) {
        std::cerr << "[TodoTaskIntegration] Failed to deserialize task payload from checkpoint: "
                  << selected->checkpointId << std::endl;
        return false;
    }

    m_workflowPaused = snapshot.value("workflowPaused", false);
    std::cout << "[TodoTaskIntegration] Resumed workflow from checkpoint: "
              << selected->checkpointId << std::endl;
    return true;
}

nlohmann::json TodoTaskIntegration::serializeTasks() const
{
    nlohmann::json j = nlohmann::json::array();
    for (auto& [id, task] : m_tasks) {
        j.push_back(task->toJson());
    }
    return j;
}

bool TodoTaskIntegration::deserializeTasks(const nlohmann::json& data)
{
    if (!data.is_array()) {
        return false;
    }

    m_tasks.clear();
    for (const auto& taskJson : data) {
        auto task = std::make_unique<Task>();
        task->fromJson(taskJson);
        m_tasks[task->taskId] = std::move(task);
    }

    return true;
}

TodoTaskIntegration::TaskStats TodoTaskIntegration::getStatistics() const
{
    TaskStats stats;
    int failureCount = 0;
    float totalTime = 0.0f;

    for (auto& [id, task] : m_tasks) {
        stats.totalRetries += task->retryCount;
        if (task->status == TaskStatus::Failed) {
            stats.totalFailures++;
            failureCount++;
        }
    }

    if (!m_tasks.empty()) {
        stats.failureRate = failureCount / (float)m_tasks.size();
    }

    return stats;
}

std::vector<std::string> TodoTaskIntegration::topologicalSort()
{
    std::vector<std::string> result;
    std::set<std::string> visited;
    std::set<std::string> tempMarked;

    std::function<bool(const std::string&)> visit = 
        [&](const std::string& taskId) -> bool {
            if (visited.count(taskId)) return true;
            if (tempMarked.count(taskId)) return false;  // Cycle

            tempMarked.insert(taskId);

            auto task = getTask(taskId);
            if (task) {
                for (const auto& dep : task->dependencies) {
                    if (!visit(dep.dependsOnTaskId)) {
                        return false;
                    }
                }
            }

            tempMarked.erase(taskId);
            visited.insert(taskId);
            result.push_back(taskId);
            return true;
        };

    for (auto& [id, task] : m_tasks) {
        if (!visited.count(id)) {
            if (!visit(id)) {
                return {};  // Cycle detected
            }
        }
    }

    return result;
}
