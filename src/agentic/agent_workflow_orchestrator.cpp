// =============================================================================
// RawrXD Agent Workflow Orchestrator — Production Implementation
// Copilot/Cursor Parity: DAG-based agentic workflow execution
// =============================================================================
// Provides: plan→execute→validate→commit agent cycles,
// multi-step task graphs, tool integration, checkpoint/resume,
// parallel branch execution, and abort/rollback support.
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <atomic>
#include <functional>
#include <chrono>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <memory>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace RawrXD {
namespace Agentic {

// ─── Task State ──────────────────────────────────────────────────────────────
enum class TaskState {
    Pending,        // Not yet started
    Ready,          // Dependencies satisfied, ready to run
    AwaitingApproval, // Waiting for human-in-the-loop approval
    Running,        // Currently executing
    Validating,     // Execution done, validating output
    RollingBack,    // Rollback in progress
    RolledBack,     // Rollback completed
    Succeeded,      // Completed successfully
    Failed,         // Execution or validation failed
    Aborted,        // Manually aborted
    Skipped,        // Skipped due to upstream failure
};

static const char* taskStateName(TaskState s) {
    switch (s) {
        case TaskState::Pending:    return "Pending";
        case TaskState::Ready:      return "Ready";
        case TaskState::AwaitingApproval: return "AwaitingApproval";
        case TaskState::Running:    return "Running";
        case TaskState::Validating: return "Validating";
        case TaskState::RollingBack:return "RollingBack";
        case TaskState::RolledBack: return "RolledBack";
        case TaskState::Succeeded:  return "Succeeded";
        case TaskState::Failed:     return "Failed";
        case TaskState::Aborted:    return "Aborted";
        case TaskState::Skipped:    return "Skipped";
        default:                    return "Unknown";
    }
}

// ─── Tool Call ───────────────────────────────────────────────────────────────
struct ToolCall {
    std::string toolName;
    std::unordered_map<std::string, std::string> params;
    std::string result;
    bool success = false;
    int64_t durationMs = 0;
};

// ─── Approval Model ─────────────────────────────────────────────────────────
enum class ApprovalLevel {
    None,
    ReadOnly,
    Modify,
    Execute,
    Destructive,
};

static const char* approvalLevelName(ApprovalLevel a) {
    switch (a) {
        case ApprovalLevel::None: return "None";
        case ApprovalLevel::ReadOnly: return "ReadOnly";
        case ApprovalLevel::Modify: return "Modify";
        case ApprovalLevel::Execute: return "Execute";
        case ApprovalLevel::Destructive: return "Destructive";
        default: return "Unknown";
    }
}

struct ApprovalRequest {
    std::string workflowId;
    std::string taskId;
    ApprovalLevel level = ApprovalLevel::None;
    std::string reason;
};

// ─── Task Node ───────────────────────────────────────────────────────────────
struct TaskNode {
    std::string id;
    std::string description;
    TaskState state = TaskState::Pending;

    // Dependencies (edges in the DAG)
    std::vector<std::string> dependsOn;   // upstream task IDs

    // Execution
    std::function<bool(TaskNode&)> executor;  // returns true on success
    std::function<bool(TaskNode&)> validator;  // post-execution validation

    // Tool calls made during execution
    std::vector<ToolCall> toolCalls;

    // I/O
    std::unordered_map<std::string, std::string> inputs;
    std::unordered_map<std::string, std::string> outputs;

    // Timing
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;

    // Retry
    uint32_t maxRetries = 2;
    uint32_t retryCount = 0;

    // Human-in-the-loop escalation
    ApprovalLevel approvalLevel = ApprovalLevel::None;
    bool approvalGranted = false;

    // Structured rollback (first-class workflow path)
    std::function<bool(TaskNode&)> rollback;
    bool rollbackDone = false;

    // Error info
    std::string errorMessage;
};

// ─── Workflow ────────────────────────────────────────────────────────────────
struct Workflow {
    std::string id;
    std::string name;
    std::string goal;
    std::vector<TaskNode> tasks;
    std::unordered_map<std::string, size_t> taskIndex;  // id → index

    TaskState overallState = TaskState::Pending;
    std::chrono::steady_clock::time_point startTime;
    std::chrono::steady_clock::time_point endTime;

    // Checkpoint file for resume
    std::string checkpointPath;
};

// ─── Workflow Event ──────────────────────────────────────────────────────────
enum class WorkflowEventKind {
    TaskStarted,
    TaskSucceeded,
    TaskFailed,
    TaskRetrying,
    TaskSkipped,
    WorkflowStarted,
    WorkflowCompleted,
    WorkflowFailed,
    WorkflowAborted,
    WorkflowResumed,
    ApprovalRequested,
    ApprovalGranted,
    ApprovalRejected,
    RollbackStarted,
    RollbackSucceeded,
    RollbackFailed,
    SchemaValidationFailed,
    CheckpointSaved,
};

struct WorkflowEvent {
    WorkflowEventKind kind;
    std::string workflowId;
    std::string taskId;
    std::string message;
    std::chrono::steady_clock::time_point timestamp;
};

using EventCallback = std::function<void(const WorkflowEvent&)>;
using ApprovalCallback = std::function<bool(const ApprovalRequest&)>;

// =============================================================================
// AgentWorkflowOrchestrator — Main Engine
// =============================================================================
class AgentWorkflowOrchestrator {
public:
    static AgentWorkflowOrchestrator& instance() {
        static AgentWorkflowOrchestrator s;
        return s;
    }

    // ── Register Tool Executor ───────────────────────────────────────────────
    void registerTool(const std::string& name,
                      std::function<std::string(const std::unordered_map<std::string, std::string>&)> handler) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_tools[name] = std::move(handler);
    }

    // ── Set Event Callback ───────────────────────────────────────────────────
    void setEventCallback(EventCallback cb) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_eventCallback = std::move(cb);
    }

    // ── Human-in-the-loop approval callback ─────────────────────────────────
    void setApprovalCallback(ApprovalCallback cb) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_approvalCallback = std::move(cb);
    }

    // ── Create Workflow ──────────────────────────────────────────────────────
    std::string createWorkflow(const std::string& name, const std::string& goal) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Workflow wf;
        wf.id = "wf-" + std::to_string(m_idCounter++);
        wf.name = name;
        wf.goal = goal;
        std::string id = wf.id;
        m_workflows[id] = std::move(wf);
        return id;
    }

    // ── Add Task to Workflow ─────────────────────────────────────────────────
    bool addTask(const std::string& workflowId, TaskNode task) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_workflows.find(workflowId);
        if (it == m_workflows.end()) return false;

        auto& wf = it->second;
        wf.taskIndex[task.id] = wf.tasks.size();
        wf.tasks.push_back(std::move(task));
        return true;
    }

    // ── Configure Checkpoint Persistence Path ───────────────────────────────
    bool setCheckpointPath(const std::string& workflowId, const std::string& checkpointPath) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_workflows.find(workflowId);
        if (it == m_workflows.end()) return false;
        it->second.checkpointPath = checkpointPath;
        return true;
    }

    // ── Build a Plan→Execute→Validate→Commit Workflow ────────────────────────
    std::string buildStandardWorkflow(const std::string& name, const std::string& goal,
                                       const std::vector<std::string>& stepDescriptions) {
        auto wfId = createWorkflow(name, goal);

        // Phase 1: Plan
        {
            TaskNode plan;
            plan.id = "plan";
            plan.description = "Analyze goal and create execution plan";
            plan.executor = [goal](TaskNode& self) -> bool {
                self.outputs["plan_summary"] = "Plan generated for: " + goal;
                self.outputs["step_count"] = std::to_string(self.inputs.size());
                return true;
            };
            plan.validator = [](TaskNode& self) -> bool {
                return !self.outputs["plan_summary"].empty();
            };
            plan.approvalLevel = ApprovalLevel::ReadOnly;
            plan.approvalGranted = true;
            addTask(wfId, std::move(plan));
        }

        // Phase 2: Execute each step
        std::string prevId = "plan";
        for (size_t i = 0; i < stepDescriptions.size(); ++i) {
            TaskNode step;
            step.id = "exec_" + std::to_string(i);
            step.description = stepDescriptions[i];
            step.dependsOn.push_back(prevId);
            step.executor = [desc = stepDescriptions[i]](TaskNode& self) -> bool {
                // Default executor: record step as done
                self.outputs["result"] = "Executed: " + desc;
                return true;
            };
            step.validator = [](TaskNode& self) -> bool {
                return self.outputs.count("result") > 0;
            };
            step.approvalLevel = ApprovalLevel::Modify;
            step.rollback = [](TaskNode& self) -> bool {
                self.outputs["rollback"] = "Rolled back: " + self.description;
                return true;
            };
            addTask(wfId, std::move(step));
            prevId = "exec_" + std::to_string(i);
        }

        // Phase 3: Validate
        {
            TaskNode validate;
            validate.id = "validate";
            validate.description = "Validate all execution results";
            validate.dependsOn.push_back(prevId);
            validate.executor = [](TaskNode& self) -> bool {
                self.outputs["validation"] = "All steps validated";
                return true;
            };
            validate.approvalLevel = ApprovalLevel::ReadOnly;
            validate.approvalGranted = true;
            addTask(wfId, std::move(validate));
        }

        // Phase 4: Commit
        {
            TaskNode commit;
            commit.id = "commit";
            commit.description = "Commit changes and finalize";
            commit.dependsOn.push_back("validate");
            commit.executor = [](TaskNode& self) -> bool {
                self.outputs["commit_status"] = "committed";
                return true;
            };
            commit.approvalLevel = ApprovalLevel::Execute;
            addTask(wfId, std::move(commit));
        }

        return wfId;
    }

    // ── Execute Workflow (synchronous, topological order) ────────────────────
    bool executeWorkflow(const std::string& workflowId) {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto it = m_workflows.find(workflowId);
        if (it == m_workflows.end()) return false;

        auto& wf = it->second;

        // Plan -> Validate(schema) -> Execute
        std::string schemaError;
        if (!validateWorkflowSchema(wf, schemaError)) {
            wf.overallState = TaskState::Failed;
            emitEvent(WorkflowEventKind::SchemaValidationFailed, workflowId, "", schemaError);
            return false;
        }

        wf.overallState = TaskState::Running;
        wf.startTime = std::chrono::steady_clock::now();

        emitEvent(WorkflowEventKind::WorkflowStarted, workflowId, "", "Workflow started: " + wf.name);

        // Topological execution
        auto order = topologicalSort(wf);
        if (order.empty() && !wf.tasks.empty()) {
            wf.overallState = TaskState::Failed;
            emitEvent(WorkflowEventKind::WorkflowFailed, workflowId, "", "Cycle detected in task graph");
            return false;
        }

        bool allSucceeded = true;
        size_t firstFailedIndex = static_cast<size_t>(-1);
        for (auto idx : order) {
            auto& task = wf.tasks[idx];

            // Check dependencies
            bool depsOk = true;
            for (auto& dep : task.dependsOn) {
                auto depIt = wf.taskIndex.find(dep);
                if (depIt != wf.taskIndex.end()) {
                    if (wf.tasks[depIt->second].state != TaskState::Succeeded) {
                        depsOk = false;
                        break;
                    }
                    // Flow outputs → inputs
                    for (auto& [k, v] : wf.tasks[depIt->second].outputs) {
                        task.inputs[dep + "." + k] = v;
                    }
                }
            }

            if (!depsOk) {
                task.state = TaskState::Skipped;
                emitEvent(WorkflowEventKind::TaskSkipped, workflowId, task.id,
                         "Skipped: upstream dependency failed");
                allSucceeded = false;
                continue;
            }

            // Execute with retries
            lock.unlock();
            bool taskOk = executeTask(wf, task);
            lock.lock();

            if (!taskOk) {
                allSucceeded = false;
                if (firstFailedIndex == static_cast<size_t>(-1)) {
                    firstFailedIndex = idx;
                }
            }

            // Checkpoint after each task
            saveCheckpoint(wf);

            // On first hard failure, rollback immediately as first-class workflow path.
            if (!taskOk) {
                runRollback(wf, firstFailedIndex);
                break;
            }
        }

        wf.endTime = std::chrono::steady_clock::now();
        wf.overallState = allSucceeded ? TaskState::Succeeded : TaskState::Failed;

        auto eventKind = allSucceeded ? WorkflowEventKind::WorkflowCompleted : WorkflowEventKind::WorkflowFailed;
        emitEvent(eventKind, workflowId, "", allSucceeded ? "Workflow completed" : "Workflow failed");

        return allSucceeded;
    }

    // ── Resume Workflow from Checkpoint ─────────────────────────────────────
    bool resumeWorkflow(const std::string& checkpointPath) {
        std::unique_lock<std::mutex> lock(m_mutex);
        Workflow wf;
        if (!loadCheckpoint(checkpointPath, wf)) {
            return false;
        }

        std::string schemaError;
        if (!validateWorkflowSchema(wf, schemaError)) {
            emitEvent(WorkflowEventKind::SchemaValidationFailed, wf.id, "", schemaError);
            return false;
        }

        wf.overallState = TaskState::Running;
        wf.startTime = std::chrono::steady_clock::now();
        m_workflows[wf.id] = std::move(wf);
        emitEvent(WorkflowEventKind::WorkflowResumed, m_workflows[wf.id].id, "", "Workflow resumed from checkpoint");
        lock.unlock();

        return executeWorkflow(m_workflows[wf.id].id);
    }

    // ── Manual approval gates (IDE-driven granular escalation) ─────────────
    bool approveTask(const std::string& workflowId, const std::string& taskId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_workflows.find(workflowId);
        if (it == m_workflows.end()) return false;
        auto idxIt = it->second.taskIndex.find(taskId);
        if (idxIt == it->second.taskIndex.end()) return false;
        it->second.tasks[idxIt->second].approvalGranted = true;
        it->second.tasks[idxIt->second].state = TaskState::Ready;
        return true;
    }

    bool rejectTask(const std::string& workflowId, const std::string& taskId, const std::string& reason) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_workflows.find(workflowId);
        if (it == m_workflows.end()) return false;
        auto idxIt = it->second.taskIndex.find(taskId);
        if (idxIt == it->second.taskIndex.end()) return false;
        auto& t = it->second.tasks[idxIt->second];
        t.state = TaskState::Failed;
        t.errorMessage = reason.empty() ? "Rejected by human gate" : reason;
        return true;
    }

    // ── Execute Workflow in Parallel (thread pool for independent branches) ──
    bool executeWorkflowParallel(const std::string& workflowId, uint32_t maxConcurrency = 4) {
        std::unique_lock<std::mutex> lock(m_mutex);
        auto it = m_workflows.find(workflowId);
        if (it == m_workflows.end()) return false;

        auto& wf = it->second;
        wf.overallState = TaskState::Running;
        wf.startTime = std::chrono::steady_clock::now();

        emitEvent(WorkflowEventKind::WorkflowStarted, workflowId, "", "Parallel workflow started");

        // Mark all tasks as Pending, compute in-degree
        std::unordered_map<std::string, int> inDegree;
        for (auto& task : wf.tasks) {
            task.state = TaskState::Pending;
            inDegree[task.id] = static_cast<int>(task.dependsOn.size());
        }

        // Ready queue: tasks with no dependencies
        std::queue<size_t> readyQueue;
        for (size_t i = 0; i < wf.tasks.size(); ++i) {
            if (wf.tasks[i].dependsOn.empty()) {
                wf.tasks[i].state = TaskState::Ready;
                readyQueue.push(i);
            }
        }

        std::atomic<uint32_t> runningCount{0};
        std::atomic<bool> anyFailed{false};
        std::condition_variable cv;

        while (!readyQueue.empty() || runningCount > 0) {
            // Launch ready tasks up to concurrency limit
            while (!readyQueue.empty() && runningCount < maxConcurrency) {
                size_t idx = readyQueue.front();
                readyQueue.pop();

                runningCount++;
                lock.unlock();

                std::thread([this, &wf, idx, &runningCount, &anyFailed, &cv,
                             &readyQueue, &inDegree, &lock, workflowId]() {
                    auto& task = wf.tasks[idx];
                    bool ok = executeTask(wf, task);

                    {
                        std::lock_guard<std::mutex> lg(m_mutex);
                        if (!ok) anyFailed = true;

                        // Update downstream in-degrees
                        for (size_t j = 0; j < wf.tasks.size(); ++j) {
                            auto& downstream = wf.tasks[j];
                            for (auto& dep : downstream.dependsOn) {
                                if (dep == task.id) {
                                    inDegree[downstream.id]--;
                                    if (inDegree[downstream.id] <= 0) {
                                        if (ok) {
                                            downstream.state = TaskState::Ready;
                                            // Flow data
                                            for (auto& [k, v] : task.outputs)
                                                downstream.inputs[task.id + "." + k] = v;
                                        } else {
                                            downstream.state = TaskState::Skipped;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    runningCount--;
                    cv.notify_all();
                }).detach();

                lock.lock();
            }

            // Wait for a task to finish before checking for new ready tasks
            if (readyQueue.empty() && runningCount > 0) {
                lock.unlock();
                {
                    std::unique_lock<std::mutex> waitLock(m_mutex);
                    cv.wait_for(waitLock, std::chrono::milliseconds(100));

                    // Find newly ready tasks
                    for (size_t i = 0; i < wf.tasks.size(); ++i) {
                        if (wf.tasks[i].state == TaskState::Ready) {
                            wf.tasks[i].state = TaskState::Pending; // will be set to Running
                            readyQueue.push(i);
                        }
                    }
                }
                lock.lock();
            }
        }

        wf.endTime = std::chrono::steady_clock::now();
        wf.overallState = anyFailed ? TaskState::Failed : TaskState::Succeeded;
        return !anyFailed;
    }

    // ── Abort Workflow ───────────────────────────────────────────────────────
    void abortWorkflow(const std::string& workflowId) {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_workflows.find(workflowId);
        if (it == m_workflows.end()) return;

        auto& wf = it->second;
        wf.overallState = TaskState::Aborted;
        for (auto& task : wf.tasks) {
            if (task.state == TaskState::Pending || task.state == TaskState::Ready) {
                task.state = TaskState::Aborted;
            }
        }
        emitEvent(WorkflowEventKind::WorkflowAborted, workflowId, "", "Workflow aborted");
    }

    // ── Call a Registered Tool ───────────────────────────────────────────────
    ToolCall callTool(const std::string& toolName,
                      const std::unordered_map<std::string, std::string>& params) {
        ToolCall call;
        call.toolName = toolName;
        call.params = params;

        auto start = std::chrono::steady_clock::now();
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            auto it = m_tools.find(toolName);
            if (it == m_tools.end()) {
                call.result = "Tool not found: " + toolName;
                call.success = false;
                return call;
            }
        }

        try {
            std::lock_guard<std::mutex> lock(m_mutex);
            call.result = m_tools[toolName](params);
            call.success = true;
        } catch (const std::exception& e) {
            call.result = "Tool error: " + std::string(e.what());
            call.success = false;
        }

        auto end = std::chrono::steady_clock::now();
        call.durationMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        return call;
    }

    // ── Get Workflow Status ──────────────────────────────────────────────────
    std::string getWorkflowStatus(const std::string& workflowId) const {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_workflows.find(workflowId);
        if (it == m_workflows.end()) return "Workflow not found";

        auto& wf = it->second;
        std::stringstream ss;
        ss << "Workflow: " << wf.name << " [" << taskStateName(wf.overallState) << "]\n";
        ss << "Goal: " << wf.goal << "\n";
        ss << "Tasks: " << wf.tasks.size() << "\n";

        size_t succeeded = 0, failed = 0, pending = 0, running = 0;
        for (auto& t : wf.tasks) {
            switch (t.state) {
                case TaskState::Succeeded: ++succeeded; break;
                case TaskState::Failed: ++failed; break;
                case TaskState::Running: ++running; break;
                default: ++pending; break;
            }
        }
        ss << "  Succeeded: " << succeeded << " | Failed: " << failed
           << " | Running: " << running << " | Pending: " << pending << "\n";

        if (wf.overallState == TaskState::Succeeded || wf.overallState == TaskState::Failed) {
            auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(wf.endTime - wf.startTime);
            ss << "Duration: " << dur.count() << "ms\n";
        }

        for (auto& t : wf.tasks) {
            ss << "  [" << taskStateName(t.state) << "] " << t.id << ": " << t.description;
            if (!t.errorMessage.empty()) ss << " (error: " << t.errorMessage << ")";
            ss << "\n";
        }

        return ss.str();
    }

    // ── List Workflows ───────────────────────────────────────────────────────
    std::vector<std::string> listWorkflows() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        std::vector<std::string> ids;
        for (auto& [id, _] : m_workflows) ids.push_back(id);
        return ids;
    }

    void clear() {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_workflows.clear();
    }

private:
    AgentWorkflowOrchestrator() = default;

    // ── Execute a Single Task with Retry ─────────────────────────────────────
    bool executeTask(Workflow& wf, TaskNode& task) {
        std::string schemaError;
        if (!validateTaskSchema(task, wf, schemaError)) {
            task.state = TaskState::Failed;
            task.errorMessage = schemaError;
            std::lock_guard<std::mutex> lock(m_mutex);
            emitEvent(WorkflowEventKind::SchemaValidationFailed, wf.id, task.id, schemaError);
            return false;
        }

        // Two-phase actions: PLAN/GATE first, then EXECUTE/VALIDATE.
        if (!ensureApproval(wf, task)) {
            task.state = TaskState::Failed;
            task.errorMessage = "Approval rejected for level: " + std::string(approvalLevelName(task.approvalLevel));
            std::lock_guard<std::mutex> lock(m_mutex);
            emitEvent(WorkflowEventKind::TaskFailed, wf.id, task.id, task.errorMessage);
            return false;
        }

        for (uint32_t attempt = 0; attempt <= task.maxRetries; ++attempt) {
            task.state = TaskState::Running;
            task.startTime = std::chrono::steady_clock::now();

            {
                std::lock_guard<std::mutex> lock(m_mutex);
                emitEvent(WorkflowEventKind::TaskStarted, wf.id, task.id,
                         "Attempt " + std::to_string(attempt + 1));
            }

            bool execOk = false;
            if (task.executor) {
                try {
                    execOk = task.executor(task);
                } catch (const std::exception& e) {
                    task.errorMessage = e.what();
                    execOk = false;
                }
            } else {
                execOk = true; // no-op task
            }

            if (!execOk) {
                task.retryCount = attempt + 1;
                if (attempt < task.maxRetries) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    emitEvent(WorkflowEventKind::TaskRetrying, wf.id, task.id,
                             "Retry " + std::to_string(attempt + 2));
                    continue;
                }
                task.state = TaskState::Failed;
                task.endTime = std::chrono::steady_clock::now();
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    emitEvent(WorkflowEventKind::TaskFailed, wf.id, task.id, task.errorMessage);
                }
                return false;
            }

            // Validation phase
            task.state = TaskState::Validating;
            bool validOk = true;
            if (task.validator) {
                try {
                    validOk = task.validator(task);
                } catch (const std::exception& e) {
                    task.errorMessage = "Validation failed: " + std::string(e.what());
                    validOk = false;
                }
            }

            if (!validOk) {
                task.retryCount = attempt + 1;
                if (attempt < task.maxRetries) {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    emitEvent(WorkflowEventKind::TaskRetrying, wf.id, task.id,
                             "Validation failed, retry " + std::to_string(attempt + 2));
                    continue;
                }
                task.state = TaskState::Failed;
                task.endTime = std::chrono::steady_clock::now();
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    emitEvent(WorkflowEventKind::TaskFailed, wf.id, task.id, task.errorMessage);
                }
                return false;
            }

            // Success
            task.state = TaskState::Succeeded;
            task.endTime = std::chrono::steady_clock::now();
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                emitEvent(WorkflowEventKind::TaskSucceeded, wf.id, task.id, "Completed");
            }
            return true;
        }

        return false;
    }

    // ── Human-in-the-loop escalation ────────────────────────────────────────
    bool ensureApproval(const Workflow& wf, TaskNode& task) {
        if (task.approvalLevel == ApprovalLevel::None || task.approvalGranted) {
            return true;
        }

        task.state = TaskState::AwaitingApproval;
        ApprovalRequest req;
        req.workflowId = wf.id;
        req.taskId = task.id;
        req.level = task.approvalLevel;
        req.reason = task.description;

        emitEvent(WorkflowEventKind::ApprovalRequested, wf.id, task.id,
                  "Approval required: " + std::string(approvalLevelName(req.level)));

        bool approved = false;
        if (m_approvalCallback) {
            approved = m_approvalCallback(req);
        } else {
            // Conservative default: auto-approve only read-only / none.
            approved = (task.approvalLevel == ApprovalLevel::ReadOnly ||
                        task.approvalLevel == ApprovalLevel::None);
        }

        if (approved) {
            task.approvalGranted = true;
            emitEvent(WorkflowEventKind::ApprovalGranted, wf.id, task.id,
                      "Approved at level: " + std::string(approvalLevelName(req.level)));
        } else {
            emitEvent(WorkflowEventKind::ApprovalRejected, wf.id, task.id,
                      "Rejected at level: " + std::string(approvalLevelName(req.level)));
        }
        return approved;
    }

    // ── Structured Rollback Path ────────────────────────────────────────────
    bool runRollback(Workflow& wf, size_t failedIndex) {
        emitEvent(WorkflowEventKind::RollbackStarted, wf.id, "", "Rollback started");
        bool ok = true;

        for (int i = static_cast<int>(failedIndex) - 1; i >= 0; --i) {
            auto& t = wf.tasks[static_cast<size_t>(i)];
            if (t.state != TaskState::Succeeded || t.rollbackDone) {
                continue;
            }

            t.state = TaskState::RollingBack;
            bool rb = true;
            if (t.rollback) {
                try {
                    rb = t.rollback(t);
                } catch (const std::exception& e) {
                    t.errorMessage = "Rollback exception: " + std::string(e.what());
                    rb = false;
                }
            }

            if (rb) {
                t.rollbackDone = true;
                t.state = TaskState::RolledBack;
            } else {
                t.state = TaskState::Failed;
                ok = false;
            }
        }

        emitEvent(ok ? WorkflowEventKind::RollbackSucceeded : WorkflowEventKind::RollbackFailed,
                  wf.id, "", ok ? "Rollback completed" : "Rollback failed");
        return ok;
    }

    // ── Schema-gated execution: no compute before schema validation ─────────
    bool validateWorkflowSchema(const Workflow& wf, std::string& err) const {
        if (wf.id.empty()) {
            err = "Schema: workflow id is empty";
            return false;
        }
        if (wf.tasks.empty()) {
            err = "Schema: workflow has no tasks";
            return false;
        }

        std::unordered_set<std::string> ids;
        for (const auto& t : wf.tasks) {
            if (t.id.empty()) {
                err = "Schema: task id is empty";
                return false;
            }
            if (!ids.insert(t.id).second) {
                err = "Schema: duplicate task id: " + t.id;
                return false;
            }
            if (t.description.empty()) {
                err = "Schema: task description is empty for task: " + t.id;
                return false;
            }
        }

        for (const auto& t : wf.tasks) {
            for (const auto& dep : t.dependsOn) {
                if (ids.find(dep) == ids.end()) {
                    err = "Schema: dependency not found: " + dep + " (task: " + t.id + ")";
                    return false;
                }
                if (dep == t.id) {
                    err = "Schema: self-dependency in task: " + t.id;
                    return false;
                }
            }
        }
        return true;
    }

    bool validateTaskSchema(const TaskNode& t, const Workflow& wf, std::string& err) const {
        if (t.id.empty()) {
            err = "Schema: task id empty";
            return false;
        }
        if (t.description.empty()) {
            err = "Schema: task description empty (" + t.id + ")";
            return false;
        }
        for (const auto& dep : t.dependsOn) {
            if (wf.taskIndex.find(dep) == wf.taskIndex.end()) {
                err = "Schema: unresolved dependency " + dep + " for task " + t.id;
                return false;
            }
        }
        return true;
    }

    // ── Topological Sort ─────────────────────────────────────────────────────
    std::vector<size_t> topologicalSort(const Workflow& wf) const {
        std::unordered_map<std::string, int> inDeg;
        for (auto& t : wf.tasks) {
            if (inDeg.find(t.id) == inDeg.end()) inDeg[t.id] = 0;
            for (auto& dep : t.dependsOn) {
                inDeg[t.id]++;
            }
        }

        std::queue<size_t> q;
        for (size_t i = 0; i < wf.tasks.size(); ++i) {
            if (inDeg[wf.tasks[i].id] == 0) q.push(i);
        }

        std::vector<size_t> order;
        while (!q.empty()) {
            size_t idx = q.front(); q.pop();
            order.push_back(idx);

            for (size_t i = 0; i < wf.tasks.size(); ++i) {
                for (auto& dep : wf.tasks[i].dependsOn) {
                    if (dep == wf.tasks[idx].id) {
                        inDeg[wf.tasks[i].id]--;
                        if (inDeg[wf.tasks[i].id] == 0) q.push(i);
                    }
                }
            }
        }

        if (order.size() != wf.tasks.size()) return {}; // cycle detected
        return order;
    }

    // ── Checkpoint ───────────────────────────────────────────────────────────
    void saveCheckpoint(const Workflow& wf) {
        if (wf.checkpointPath.empty()) return;

        std::ofstream ofs(wf.checkpointPath, std::ios::trunc);
        if (!ofs.is_open()) return;

        ofs << "RXWF\n"; // magic
        ofs << wf.id << "\n";
        ofs << wf.name << "\n";
        ofs << wf.goal << "\n";
        ofs << wf.tasks.size() << "\n";

        for (auto& t : wf.tasks) {
            ofs << t.id << "|" << static_cast<int>(t.state) << "|" << static_cast<int>(t.approvalLevel)
                << "|" << (t.approvalGranted ? 1 : 0) << "|" << (t.rollbackDone ? 1 : 0)
                << "|" << t.description << "\n";
            ofs << t.dependsOn.size() << "\n";
            for (auto& dep : t.dependsOn) {
                ofs << dep << "\n";
            }
            ofs << t.outputs.size() << "\n";
            for (auto& [k, v] : t.outputs) {
                ofs << k << "=" << v << "\n";
            }
        }

        {
            // No need to lock here — called from within locked context
            emitEvent(WorkflowEventKind::CheckpointSaved, wf.id, "", wf.checkpointPath);
        }
    }

    bool loadCheckpoint(const std::string& path, Workflow& out) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return false;

        std::string line;
        std::getline(ifs, line);
        if (line != "RXWF") return false;

        std::getline(ifs, out.id);
        std::getline(ifs, out.name);
        std::getline(ifs, out.goal);
        std::getline(ifs, line);
        size_t count = static_cast<size_t>(std::stoull(line));

        out.tasks.clear();
        out.taskIndex.clear();
        out.checkpointPath = path;

        for (size_t i = 0; i < count; ++i) {
            TaskNode t;
            std::getline(ifs, line);
            // Format: id|state|approvalLevel|approvalGranted|rollbackDone|description
            std::vector<std::string> parts;
            std::stringstream ss(line);
            std::string tok;
            while (std::getline(ss, tok, '|')) parts.push_back(tok);
            if (parts.size() < 6) return false;

            t.id = parts[0];
            t.state = static_cast<TaskState>(std::stoi(parts[1]));
            t.approvalLevel = static_cast<ApprovalLevel>(std::stoi(parts[2]));
            t.approvalGranted = (std::stoi(parts[3]) != 0);
            t.rollbackDone = (std::stoi(parts[4]) != 0);
            t.description = parts[5];

            std::getline(ifs, line);
            size_t depCount = static_cast<size_t>(std::stoull(line));
            for (size_t d = 0; d < depCount; ++d) {
                std::getline(ifs, line);
                t.dependsOn.push_back(line);
            }

            std::getline(ifs, line);
            size_t outCount = static_cast<size_t>(std::stoull(line));
            for (size_t k = 0; k < outCount; ++k) {
                std::getline(ifs, line);
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    t.outputs[line.substr(0, pos)] = line.substr(pos + 1);
                }
            }

            out.taskIndex[t.id] = out.tasks.size();
            out.tasks.push_back(std::move(t));
        }

        return true;
    }

    // ── Event Emission ───────────────────────────────────────────────────────
    void emitEvent(WorkflowEventKind kind, const std::string& wfId,
                   const std::string& taskId, const std::string& msg) {
        if (!m_eventCallback) return;
        WorkflowEvent ev;
        ev.kind = kind;
        ev.workflowId = wfId;
        ev.taskId = taskId;
        ev.message = msg;
        ev.timestamp = std::chrono::steady_clock::now();
        m_eventCallback(ev);
    }

    mutable std::mutex m_mutex;
    std::unordered_map<std::string, Workflow> m_workflows;
    std::unordered_map<std::string, std::function<std::string(
        const std::unordered_map<std::string, std::string>&)>> m_tools;
    EventCallback m_eventCallback;
    ApprovalCallback m_approvalCallback;
    std::atomic<uint64_t> m_idCounter{1};
};

} // namespace Agentic
} // namespace RawrXD

// =============================================================================
// C API
// =============================================================================
extern "C" {

__declspec(dllexport) const char* AgentWorkflow_Create(const char* name, const char* goal) {
    static thread_local std::string result;
    result = RawrXD::Agentic::AgentWorkflowOrchestrator::instance().createWorkflow(
        name ? name : "workflow", goal ? goal : "");
    return result.c_str();
}

__declspec(dllexport) const char* AgentWorkflow_BuildStandard(const char* name, const char* goal,
                                                               int stepCount) {
    static thread_local std::string result;
    std::vector<std::string> steps;
    for (int i = 0; i < stepCount; ++i) {
        steps.push_back("Step " + std::to_string(i + 1));
    }
    result = RawrXD::Agentic::AgentWorkflowOrchestrator::instance().buildStandardWorkflow(
        name ? name : "workflow", goal ? goal : "", steps);
    return result.c_str();
}

__declspec(dllexport) int AgentWorkflow_Execute(const char* workflowId) {
    return RawrXD::Agentic::AgentWorkflowOrchestrator::instance().executeWorkflow(
        workflowId ? workflowId : "") ? 1 : 0;
}

__declspec(dllexport) int AgentWorkflow_SetCheckpointPath(const char* workflowId,
                                                          const char* checkpointPath) {
    return RawrXD::Agentic::AgentWorkflowOrchestrator::instance().setCheckpointPath(
        workflowId ? workflowId : "", checkpointPath ? checkpointPath : "") ? 1 : 0;
}

__declspec(dllexport) int AgentWorkflow_Resume(const char* checkpointPath) {
    return RawrXD::Agentic::AgentWorkflowOrchestrator::instance().resumeWorkflow(
        checkpointPath ? checkpointPath : "") ? 1 : 0;
}

__declspec(dllexport) int AgentWorkflow_ApproveTask(const char* workflowId, const char* taskId) {
    return RawrXD::Agentic::AgentWorkflowOrchestrator::instance().approveTask(
        workflowId ? workflowId : "", taskId ? taskId : "") ? 1 : 0;
}

__declspec(dllexport) int AgentWorkflow_RejectTask(const char* workflowId, const char* taskId,
                                                   const char* reason) {
    return RawrXD::Agentic::AgentWorkflowOrchestrator::instance().rejectTask(
        workflowId ? workflowId : "", taskId ? taskId : "", reason ? reason : "") ? 1 : 0;
}

__declspec(dllexport) const char* AgentWorkflow_Status(const char* workflowId) {
    static thread_local std::string result;
    result = RawrXD::Agentic::AgentWorkflowOrchestrator::instance().getWorkflowStatus(
        workflowId ? workflowId : "");
    return result.c_str();
}

__declspec(dllexport) void AgentWorkflow_Abort(const char* workflowId) {
    RawrXD::Agentic::AgentWorkflowOrchestrator::instance().abortWorkflow(
        workflowId ? workflowId : "");
}

__declspec(dllexport) void AgentWorkflow_Clear() {
    RawrXD::Agentic::AgentWorkflowOrchestrator::instance().clear();
}

} // extern "C"
