// ============================================================================
// Workflow Engine — Business Process Automation
// Automates complex business workflows with conditional logic
// ============================================================================
#pragma once
#include "../inference/SovereignInferenceClient.h"
#include "../core/session_manager.h"
#include <memory>
#include <vector>
#include <string>
#include <map>
#include <mutex>
#include <functional>
#include <queue>

namespace RawrXD::Workflow {

enum class WorkflowStatus {
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

enum class TaskStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    SKIPPED
};

struct WorkflowTask {
    std::string id;
    std::string name;
    std::string type;
    std::map<std::string, std::string> parameters;
    std::vector<std::string> dependencies;
    std::vector<std::string> nextTasks;
    std::function<bool(const std::map<std::string, std::string>&)> condition;
    TaskStatus status;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point completedAt;
    std::string errorMessage;
};

struct WorkflowDefinition {
    std::string id;
    std::string name;
    std::string description;
    std::vector<WorkflowTask> tasks;
    std::map<std::string, std::string> variables;
    std::chrono::system_clock::time_point createdAt;
    std::string createdBy;
};

struct WorkflowInstance {
    std::string id;
    std::string definitionId;
    WorkflowStatus status;
    std::map<std::string, std::string> context;
    std::map<std::string, TaskStatus> taskStatuses;
    std::chrono::system_clock::time_point startedAt;
    std::chrono::system_clock::time_point completedAt;
    std::string currentTask;
};

struct WorkflowMetrics {
    int totalExecutions;
    int successfulExecutions;
    int failedExecutions;
    double averageExecutionTime;
    std::map<std::string, int> taskFailureCounts;
};

class WorkflowEngine {
public:
    explicit WorkflowEngine(std::shared_ptr<Core::SessionManager> sessionManager)
        : m_sessionManager(sessionManager) {}

    void RegisterWorkflow(const WorkflowDefinition& definition) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_definitions[definition.id] = definition;
    }

    WorkflowInstance StartWorkflow(const std::string& definitionId, 
                                  const std::map<std::string, std::string>& context) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        WorkflowInstance instance;
        instance.id = GenerateInstanceId();
        instance.definitionId = definitionId;
        instance.status = WorkflowStatus::RUNNING;
        instance.context = context;
        instance.startedAt = std::chrono::system_clock::now();
        
        auto defIt = m_definitions.find(definitionId);
        if (defIt != m_definitions.end()) {
            // Initialize task statuses
            for (const auto& task : defIt->second.tasks) {
                instance.taskStatuses[task.id] = TaskStatus::PENDING;
            }
            
            // Start first task
            ExecuteNextTasks(instance);
        }
        
        m_instances[instance.id] = instance;
        return instance;
    }

    void ExecuteTask(const std::string& instanceId, const std::string& taskId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto instIt = m_instances.find(instanceId);
        if (instIt == m_instances.end()) return;
        
        auto& instance = instIt->second;
        
        auto defIt = m_definitions.find(instance.definitionId);
        if (defIt == m_definitions.end()) return;
        
        // Find task
        for (auto& task : defIt->second.tasks) {
            if (task.id == taskId) {
                ExecuteSingleTask(instance, task);
                break;
            }
        }
    }

    void PauseWorkflow(const std::string& instanceId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_instances.find(instanceId);
        if (it != m_instances.end()) {
            it->second.status = WorkflowStatus::PAUSED;
        }
    }

    void ResumeWorkflow(const std::string& instanceId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_instances.find(instanceId);
        if (it != m_instances.end() && it->second.status == WorkflowStatus::PAUSED) {
            it->second.status = WorkflowStatus::RUNNING;
            ExecuteNextTasks(it->second);
        }
    }

    void CancelWorkflow(const std::string& instanceId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_instances.find(instanceId);
        if (it != m_instances.end()) {
            it->second.status = WorkflowStatus::CANCELLED;
        }
    }

    WorkflowStatus GetWorkflowStatus(const std::string& instanceId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        auto it = m_instances.find(instanceId);
        if (it != m_instances.end()) {
            return it->second.status;
        }
        return WorkflowStatus::FAILED;
    }

    WorkflowMetrics GetMetrics(const std::string& definitionId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        
        WorkflowMetrics metrics = {0, 0, 0, 0.0, {}};
        
        for (const auto& [id, instance] : m_instances) {
            if (instance.definitionId == definitionId) {
                metrics.totalExecutions++;
                
                if (instance.status == WorkflowStatus::COMPLETED) {
                    metrics.successfulExecutions++;
                } else if (instance.status == WorkflowStatus::FAILED) {
                    metrics.failedExecutions++;
                }
                
                if (instance.completedAt > instance.startedAt) {
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(
                        instance.completedAt - instance.startedAt).count();
                    metrics.averageExecutionTime = (metrics.averageExecutionTime * 
                        (metrics.totalExecutions - 1) + duration) / metrics.totalExecutions;
                }
            }
        }
        
        return metrics;
    }

    std::string GenerateWorkflowReport(const std::string& definitionId) {
        std::ostringstream report;
        report << "# Workflow Report\n\n";
        
        auto defIt = m_definitions.find(definitionId);
        if (defIt != m_definitions.end()) {
            report << "## " << defIt->second.name << "\n\n";
            report << defIt->second.description << "\n\n";
        }
        
        auto metrics = GetMetrics(definitionId);
        report << "## Metrics\n";
        report << "- **Total Executions:** " << metrics.totalExecutions << "\n";
        report << "- **Successful:** " << metrics.successfulExecutions << "\n";
        report << "- **Failed:** " << metrics.failedExecutions << "\n";
        report << "- **Average Execution Time:** " << metrics.averageExecutionTime << "s\n\n";
        
        report << "## Active Instances\n";
        for (const auto& [id, instance] : m_instances) {
            if (instance.definitionId == definitionId && 
                (instance.status == WorkflowStatus::RUNNING || 
                 instance.status == WorkflowStatus::PAUSED)) {
                report << "- " << id << " (" << StatusToString(instance.status) << ")\n";
            }
        }
        
        return report.str();
    }

private:
    std::shared_ptr<Core::SessionManager> m_sessionManager;
    mutable std::mutex m_mutex;
    std::map<std::string, WorkflowDefinition> m_definitions;
    std::map<std::string, WorkflowInstance> m_instances;

    void ExecuteNextTasks(WorkflowInstance& instance) {
        auto defIt = m_definitions.find(instance.definitionId);
        if (defIt == m_definitions.end()) return;
        
        for (auto& task : defIt->second.tasks) {
            if (instance.taskStatuses[task.id] == TaskStatus::PENDING) {
                // Check dependencies
                bool dependenciesMet = true;
                for (const auto& dep : task.dependencies) {
                    if (instance.taskStatuses[dep] != TaskStatus::COMPLETED) {
                        dependenciesMet = false;
                        break;
                    }
                }
                
                if (dependenciesMet) {
                    // Check condition
                    if (!task.condition || task.condition(instance.context)) {
                        ExecuteSingleTask(instance, task);
                    } else {
                        instance.taskStatuses[task.id] = TaskStatus::SKIPPED;
                    }
                }
            }
        }
        
        // Check if workflow is complete
        bool allCompleted = true;
        for (const auto& [taskId, status] : instance.taskStatuses) {
            if (status == TaskStatus::PENDING || status == TaskStatus::RUNNING) {
                allCompleted = false;
                break;
            }
        }
        
        if (allCompleted) {
            instance.status = WorkflowStatus::COMPLETED;
            instance.completedAt = std::chrono::system_clock::now();
        }
    }

    void ExecuteSingleTask(WorkflowInstance& instance, WorkflowTask& task) {
        task.status = TaskStatus::RUNNING;
        task.startedAt = std::chrono::system_clock::now();
        instance.taskStatuses[task.id] = TaskStatus::RUNNING;
        instance.currentTask = task.id;
        
        try {
            // Execute task logic
            bool success = ExecuteTaskLogic(task, instance.context);
            
            if (success) {
                task.status = TaskStatus::COMPLETED;
                instance.taskStatuses[task.id] = TaskStatus::COMPLETED;
            } else {
                task.status = TaskStatus::FAILED;
                instance.taskStatuses[task.id] = TaskStatus::FAILED;
                task.errorMessage = "Task execution failed";
            }
            
            task.completedAt = std::chrono::system_clock::now();
            
        } catch (const std::exception& e) {
            task.status = TaskStatus::FAILED;
            instance.taskStatuses[task.id] = TaskStatus::FAILED;
            task.errorMessage = e.what();
            task.completedAt = std::chrono::system_clock::now();
        }
        
        // Continue with next tasks
        if (task.status == TaskStatus::COMPLETED) {
            ExecuteNextTasks(instance);
        }
    }

    bool ExecuteTaskLogic(const WorkflowTask& task, std::map<std::string, std::string>& context) {
        // Execute task based on type
        if (task.type == "script") {
            // Execute script
            return true;
        } else if (task.type == "api") {
            // Call API
            return true;
        } else if (task.type == "notification") {
            // Send notification
            return true;
        }
        
        return true;
    }

    std::string GenerateInstanceId() {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << "wf_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
        return oss.str();
    }

    std::string StatusToString(WorkflowStatus status) {
        switch (status) {
            case WorkflowStatus::PENDING: return "Pending";
            case WorkflowStatus::RUNNING: return "Running";
            case WorkflowStatus::PAUSED: return "Paused";
            case WorkflowStatus::COMPLETED: return "Completed";
            case WorkflowStatus::FAILED: return "Failed";
            case WorkflowStatus::CANCELLED: return "Cancelled";
            default: return "Unknown";
        }
    }
};

} // namespace RawrXD::Workflow
