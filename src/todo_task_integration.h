#pragma once
/**
 * TodoTaskIntegration - Day 4: Todo/dependency integration with agent execution
 * 
 * Production-grade task management wired into agent workflow execution.
 * Enables complex multi-step operations with explicit dependency tracking.
 */

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <nlohmann/json.hpp>

class AgenticExecutor;
class ExecutionStatePersistence;

enum class TaskStatus {
    Pending,      // Waiting to be scheduled
    InProgress,   // Currently executing
    Completed,    // Successfully finished
    Failed,       // Execution failed
    Blocked,      // Cannot proceed (dep failed)    Paused       // Manually paused
};

enum class TaskPriority {
    Low = 0,
    Normal = 1,
    High = 2,
    Critical = 3
};

/**
 * @struct TaskDependency
 * Represents one task depending on another
 */
struct TaskDependency {
    std::string dependsOnTaskId;
    bool hasSucceeded = false;
    std::string failureReason;

    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
};

/**
 * @struct Task
 * Single unit of work in a workflow
 */
class Task {
public:
    std::string taskId;
    std::string label;
    std::string description;
    TaskStatus status = TaskStatus::Pending;
    TaskPriority priority = TaskPriority::Normal;
    
    int sequenceNumber = 0;
    std::string createdAt;
    std::string startedAt;
    std::string completedAt;
    
    // Execution parameters
    std::string toolName;
    nlohmann::json toolParams;
    nlohmann::json result;
    
    // Dependency management
    std::vector<TaskDependency> dependencies;
    int retryCount = 0;
    int maxRetries = 3;
    
    // Metadata
    bool isCheckpoint = false;
    std::string checkpointRef;
    
    nlohmann::json toJson() const;
    void fromJson(const nlohmann::json& j);
    
    bool canExecute() const;
    bool isDependenciesResolved() const;
    std::string getBlockerReason() const;
};

/**
 * @class TodoTaskIntegration  
 * @brief Production task management for agent workflows
 * 
 * Features:
 * - Multi-step task decomposition and execution
 * - Dependency graph resolution and validation
 * - Status tracking and state transitions
 * - Retry logic with exponential backoff
 * - Checkpoint integration for crash recovery
 * - Human-in-loop approval for sensitive operations
 */
class TodoTaskIntegration {
public:
    explicit TodoTaskIntegration(
        AgenticExecutor* executor,
        ExecutionStatePersistence* persistence);
    ~TodoTaskIntegration();

    // ===== Task Management =====

    /**
     * Create a task within executing workflow
     */
    std::string createTask(
        const std::string& label,
        const std::string& description,
        const std::string& toolName,
        const nlohmann::json& toolParams,
        TaskPriority priority = TaskPriority::Normal);

    /**
     * Add dependency between tasks
     */
    bool addDependency(
        const std::string& taskId,
        const std::string& dependsOnTaskId);

    /**
     * Get task by ID
     */
    Task* getTask(const std::string& taskId);

    /**
     * List all tasks with optional filtering
     */
    std::vector<Task*> listTasks(TaskStatus filterStatus = TaskStatus::Pending);

    // ===== Execution Flow =====

    /**
     * Execute next available task
     * Respects dependencies and priority
     * @return task ID if executed, empty if nothing to execute
     */
    std::string executeNextTask();

    /**
     * Execute a specific task
     * @param taskId Task to execute
     * @param requiresApproval If true, waits for human approval before executing
     */
    bool executeTask(
        const std::string& taskId,
        bool requiresApproval = false);

    /**
     * Mark task as completed with result
     */
    bool completeTask(
        const std::string& taskId,
        const nlohmann::json& result);

    /**
     * Mark task as failed
     */
    bool failTask(
        const std::string& taskId,
        const std::string& errorMessage);

    /**
     * Retry failed task (with exponential backoff)
     */
    bool retryTask(const std::string& taskId);

    // ===== Dependency Resolution =====

    /**
     * Validate task dependency graph
     * Checks for cycles and unresolvable dependencies
     * @return error message if invalid, empty if valid
     */
    std::string validateDependencies();

    /**
     * Get all tasks blocked by failed task
     */
    std::vector<Task*> getBlockedTasks(const std::string& failedTaskId);

    /**
     * Get execution order respecting dependencies  
     * @return ordered list of task IDs ready to execute
     */
    std::vector<std::string> getExecutionOrder();

    // ===== State Management =====

    /**
     * Get current workflow status
     */
    struct WorkflowStatus {
        int totalTasks = 0;
        int completedTasks = 0;
        int failedTasks = 0;
        int blockedTasks = 0;
        float progressPercent = 0.0f;
        bool allTasksComplete() const;
    };

    WorkflowStatus getWorkflowStatus();

    /**
     * Get task execution timeline
     */
    std::vector<nlohmann::json> getExecutionTimeline();

    /**
     * Pause/Resume workflow
     */
    void pauseWorkflow();
    void resumeWorkflow();

    // ===== Checkpoint Integration =====

    /**
     * Create checkpoint from current task state
     */
    std::string createTaskCheckpoint(const std::string& label);

    /**
     * Resume from task checkpoint
     */
    bool resumeFromCheckpoint(const std::string& checkpointRef);

    // ===== Persistence =====

    /**
     * Persist task state
     */
    nlohmann::json serializeTasks() const;

    /**
     * Restore task state
     */
    bool deserializeTasks(const nlohmann::json& data);

    // ===== Statistics =====

    struct TaskStats {
        float averageExecutionTimeMs = 0.0f;
        int totalRetries = 0;
        int totalFailures = 0;
        float failureRate = 0.0f;
    };

    TaskStats getStatistics() const;

private:
    AgenticExecutor* m_executor;
    ExecutionStatePersistence* m_persistence;
    std::string m_workflowExecutionId;
    
    std::map<std::string, std::unique_ptr<Task>> m_tasks;
    std::vector<std::string> m_executionOrder;
    bool m_workflowPaused = false;
    
    std::string generateTaskId();
    bool resolveDependencies(Task* task);
    bool canTaskRun(const Task* task) const;
    std::vector<std::string> topologicalSort();
};
