#pragma once
#ifndef ACTION_EXECUTOR_H
#define ACTION_EXECUTOR_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <chrono>
#include <unordered_map>
#include "model_invoker.hpp"

namespace RawrXD {

// Action execution status
enum class ExecutionStatus {
    PENDING,
    RUNNING,
    COMPLETED,
    FAILED,
    CANCELLED,
    ROLLED_BACK
};

// Individual action result
struct ActionResult {
    std::string actionId;
    ExecutionStatus status;
    std::string output;
    std::string error;
    std::chrono::milliseconds duration;
    nlohmann::json metadata;
    bool recoverable;
    
    ActionResult() : status(ExecutionStatus::PENDING), duration(0), recoverable(true) {}
    ActionResult(const std::string& id) : actionId(id), status(ExecutionStatus::PENDING), 
                                          duration(0), recoverable(true) {}
};

// Complete plan execution result
struct PlanResult {
    std::string planId;
    bool success;
    std::vector<ActionResult> actionResults;
    std::chrono::milliseconds totalDuration;
    std::string summary;
    nlohmann::json metadata;
    
    PlanResult() : success(false), totalDuration(0) {}
    PlanResult(const std::string& id) : planId(id), success(false), totalDuration(0) {}
};

// ActionExecutor - executes action plans with safety & observability
class ActionExecutor {
public:
    using ActionCallback = std::function<void(int, const std::string&)>;
    using ResultCallback = std::function<void(int, bool, const ActionResult&)>;
    using ProgressCallback = std::function<void(int, int)>;
    using PlanCallback = std::function<void(bool, const PlanResult&)>;
    using ErrorCallback = std::function<void(int, const std::string&, bool)>;
    
    ActionExecutor();
    ~ActionExecutor();
    
    // Configuration
    void setProjectRoot(const std::string& root);
    void setDryRunMode(bool enabled);
    void setStopOnError(bool enabled);
    void setAutoBackup(bool enabled);
    void setTimeout(std::chrono::milliseconds timeout);
    
    // Plan execution
    PlanResult executePlan(const ExecutionPlan& plan);
    void executePlanAsync(const ExecutionPlan& plan, PlanCallback callback);
    
    // Individual action execution
    ActionResult executeAction(const Action& action);
    
    // Control
    void cancelExecution();
    void pauseExecution();
    void resumeExecution();
    
    // Rollback
    bool rollbackPlan(const ExecutionPlan& plan, const PlanResult& result);
    
    // Signal equivalents (callbacks)
    void setPlanStartedCallback(std::function<void(int)> callback);
    void setActionStartedCallback(std::function<void(int, const std::string&)> callback);
    void setActionCompletedCallback(std::function<void(int, bool, const ActionResult&)> callback);
    void setActionFailedCallback(std::function<void(int, const std::string&, bool)> callback);
    void setProgressUpdatedCallback(std::function<void(int, int)> callback);
    void setPlanCompletedCallback(std::function<void(bool, const PlanResult&)> callback);
    
private:
    std::string m_projectRoot;
    bool m_dryRunMode;
    bool m_stopOnError;
    bool m_autoBackup;
    std::chrono::milliseconds m_timeout;
    bool m_cancelled;
    bool m_paused;
    
    std::unordered_map<std::string, std::string> m_backups;
    
    std::function<void(int)> m_planStartedCallback;
    std::function<void(int, const std::string&)> m_actionStartedCallback;
    std::function<void(int, bool, const ActionResult&)> m_actionCompletedCallback;
    std::function<void(int, const std::string&, bool)> m_actionFailedCallback;
    std::function<void(int, int)> m_progressUpdatedCallback;
    std::function<void(bool, const PlanResult&)> m_planCompletedCallback;
    
    // Action type implementations
    ActionResult executeFileEdit(const Action& action);
    ActionResult executeSearchFiles(const Action& action);
    ActionResult executeRunBuild(const Action& action);
    ActionResult executeExecuteTests(const Action& action);
    ActionResult executeCommitGit(const Action& action);
    ActionResult executeInvokeCommand(const Action& action);
    ActionResult executeRecursiveAgent(const Action& action);
    ActionResult executeQueryUser(const Action& action);
    
    // Helper methods
    bool backupFile(const std::string& filePath);
    bool restoreFile(const std::string& filePath);
    std::string executeCommand(const std::string& command);
    std::vector<std::string> searchFilesRecursive(const std::string& pattern, 
                                                   const std::string& directory);
    std::vector<std::string> grepFiles(const std::string& pattern, 
                                       const std::string& directory, 
                                       bool caseSensitive = false);
    
    // Validation
    bool validateAction(const Action& action);
    bool isSafePath(const std::string& path);
    
    // Async execution
    void executePlanAsyncInternal(const ExecutionPlan& plan, PlanCallback callback);
};

} // namespace RawrXD

#endif // ACTION_EXECUTOR_H