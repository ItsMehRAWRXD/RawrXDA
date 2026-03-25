#pragma once
// ============================================================================
// planning_agent.h — Production Planning Agent with Retry, Rollback, DAG
// ============================================================================

#include <string>
#include <vector>
#include <memory>
#include <functional>
<<<<<<< HEAD
#include <chrono>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <atomic>
#include "action_executor.h"

// ============================================================================
// PlanningTask — enriched with deps, timing, retry, metadata
// ============================================================================
struct PlanningTask {
    std::string id;
    std::string title;
    std::string status;      // "pending", "in-progress", "completed", "failed", "skipped", "rolled-back"
    int priority = 0;        // 0=normal, 1=high, 2=critical
    Action associatedAction;

    // Dependency graph: this task cannot start until all deps are completed
    std::vector<std::string> dependsOn;

    // Retry tracking
    int retryCount    = 0;
    int maxRetries    = 3;
    std::string lastError;

    // Timing
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;

    int elapsedMs() const {
        auto end = (status == "in-progress")
            ? std::chrono::steady_clock::now() : endTime;
        return (int)std::chrono::duration_cast<std::chrono::milliseconds>(
            end - startTime).count();
    }

    // Rollback info
    bool canRollback = false;
    std::string rollbackFilePath;   // backup path if FileEdit
    std::string rollbackContent;    // original content
=======
#include "action_executor.h"

struct PlanningTask {
    std::string id;
    std::string title;
    std::string status; // "pending", "in-progress", "completed", "failed"
    int priority;
    Action associatedAction;
>>>>>>> origin/main
};

class AgenticEngine; // Forward decl

<<<<<<< HEAD
// ============================================================================
// PlanProgressCallback — fired per-task status change
// ============================================================================
using PlanProgressCallback = std::function<void(int taskIndex, int totalTasks,
                                                 const std::string& taskTitle,
                                                 const std::string& status)>;

// ============================================================================
// PlanningAgent — Full production planner: DAG, retry, rollback, parallel
// ============================================================================
=======
>>>>>>> origin/main
class PlanningAgent {
public:
    explicit PlanningAgent();
    ~PlanningAgent();
<<<<<<< HEAD

    void initialize();
    void setAgenticEngine(AgenticEngine* engine);

    // ---- Core operations ----
    void createPlan(const std::string& goal);
    void executePlan();
    void executePlanWithRetry(int maxRetries = 3);
    void executePlanParallel();          // run independent (no-dep) tasks concurrently

    // ---- Task management ----
    void addTask(const PlanningTask& task);
    void addTaskWithDeps(const PlanningTask& task, const std::vector<std::string>& deps);
    void removeTask(const std::string& taskId);
    std::vector<PlanningTask> getTasks() const;
    PlanningTask* getTaskById(const std::string& id);
    const PlanningTask* getTaskById(const std::string& id) const;

    // ---- Retry / Rollback ----
    bool retryFailedTasks(int maxRetries = 3);
    bool rollbackTask(const std::string& taskId);
    void rollbackAll();

    // ---- Validation ----
    bool validatePlan() const;           // check for cycles, missing deps, empty actions
    std::vector<std::string> getValidationErrors() const;

    // ---- Status & Reporting ----
    bool isComplete() const;
    bool hasFailures() const;
    float getProgress() const;
    int getCompletedCount() const;
    int getFailedCount() const;
    int getPendingCount() const;
    int getTotalCount() const;
    std::string generateSummary() const;
    std::string getPlanJSON() const;

    // ---- Callbacks ----
    void setProgressCallback(PlanProgressCallback cb) { m_progressCallback = std::move(cb); }
    void setMaxRetries(int n) { m_maxRetries = n; }
=======
    
    void initialize();
    void setAgenticEngine(AgenticEngine* engine);
    
    // High level operations
    void createPlan(const std::string& goal);
    void executePlan();
    
    // Task management
    void addTask(const PlanningTask& task);
    std::vector<PlanningTask> getTasks() const;
    
    // Status
    bool isComplete() const;
    std::string generateSummary() const;
>>>>>>> origin/main

private:
    std::string generateUUID();
    std::vector<PlanningTask> m_tasks;
    std::unique_ptr<ActionExecutor> m_executor;
    AgenticEngine* m_engine = nullptr;
<<<<<<< HEAD

    // Config
    PlanProgressCallback m_progressCallback;
    int m_maxRetries = 3;

    // Timing
    std::chrono::steady_clock::time_point m_planStartTime;
    std::chrono::steady_clock::time_point m_planEndTime;

    // ---- Plan generators ----
    void generateCodePlan(const std::string& goal);
    void generateDebugPlan(const std::string& goal);
    void generateRefactorPlan(const std::string& goal);
    void generateTestPlan(const std::string& goal);
    void generateBuildPlan(const std::string& goal);
    void generateGenericPlan(const std::string& goal);

    // ---- Internal helpers ----
    bool executeTask(PlanningTask& task);
    bool executeTaskWithRetry(PlanningTask& task, int maxRetries);
    void notifyProgress(int index, const std::string& status);
    std::vector<std::string> getReadyTaskIds() const;  // tasks whose deps are all completed
    bool hasCyclicDeps() const;
    void createRollbackPoint(PlanningTask& task);
=======
    
    // Internal logic for plan generation (template based for now)
    void generateCodePlan(const std::string& goal);
    void generateDebugPlan(const std::string& goal);
    void generateGenericPlan(const std::string& goal);
>>>>>>> origin/main
};
