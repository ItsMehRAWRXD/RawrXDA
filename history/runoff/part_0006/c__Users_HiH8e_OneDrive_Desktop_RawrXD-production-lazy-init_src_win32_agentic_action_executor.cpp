#include "action_executor.hpp"
#include "model_invoker.hpp"
#include <chrono>
#include <thread>

namespace RawrXD {

ActionExecutor::ActionExecutor()
    : m_projectRoot("."), m_dryRunMode(false), m_stopOnError(true),
      m_autoBackup(true), m_timeout(std::chrono::seconds(30)),
      m_cancelled(false), m_paused(false) {}

ActionExecutor::~ActionExecutor() {}

void ActionExecutor::setProjectRoot(const std::string& root) { m_projectRoot = root; }
void ActionExecutor::setDryRunMode(bool enabled) { m_dryRunMode = enabled; }
void ActionExecutor::setStopOnError(bool enabled) { m_stopOnError = enabled; }
void ActionExecutor::setAutoBackup(bool enabled) { m_autoBackup = enabled; }
void ActionExecutor::setTimeout(std::chrono::milliseconds timeout) { m_timeout = timeout; }

PlanResult ActionExecutor::executePlan(const ExecutionPlan& plan) {
    PlanResult result(plan.id);
    result.success = true;
    auto start = std::chrono::steady_clock::now();
    int index = 0;
    if (m_planStartedCallback) m_planStartedCallback((int)plan.actions.size());
    for (const auto& act : plan.actions) {
        ++index;
        if (m_cancelled) break;
        if (m_actionStartedCallback) m_actionStartedCallback(index, act.description);
        ActionResult ar = executeAction(act);
        if (m_actionCompletedCallback) m_actionCompletedCallback(index, ar.status == ExecutionStatus::COMPLETED, ar);
        result.actionResults.push_back(ar);
        if (m_progressUpdatedCallback) m_progressUpdatedCallback(index, (int)plan.actions.size());
        if (ar.status != ExecutionStatus::COMPLETED && m_stopOnError) {
            result.success = false;
            break;
        }
    }
    auto end = std::chrono::steady_clock::now();
    result.totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    result.summary = result.success ? "Plan executed successfully" : "Plan execution failed";
    if (m_planCompletedCallback) m_planCompletedCallback(result.success, result);
    return result;
}

void ActionExecutor::executePlanAsync(const ExecutionPlan& plan, PlanCallback callback) {
    std::thread([this, plan, callback]() {
        PlanResult res = this->executePlan(plan);
        if (callback) callback(res.success, res);
    }).detach();
}

ActionResult ActionExecutor::executeAction(const Action& action) {
    ActionResult result(action.id);
    auto start = std::chrono::steady_clock::now();
    // Simple stub handling – each type returns success with placeholder output.
    switch (action.type) {
        case ActionType::FILE_EDIT:
            result = executeFileEdit(action);
            break;
        case ActionType::SEARCH_FILES:
            result = executeSearchFiles(action);
            break;
        case ActionType::RUN_BUILD:
            result = executeRunBuild(action);
            break;
        case ActionType::EXECUTE_TESTS:
            result = executeExecuteTests(action);
            break;
        case ActionType::COMMIT_GIT:
            result = executeCommitGit(action);
            break;
        case ActionType::INVOKE_COMMAND:
            result = executeInvokeCommand(action);
            break;
        case ActionType::RECURSIVE_AGENT:
            result = executeRecursiveAgent(action);
            break;
        case ActionType::QUERY_USER:
            result = executeQueryUser(action);
            break;
        default:
            result.status = ExecutionStatus::FAILED;
            result.error = "Unsupported action type";
            result.recoverable = false;
            break;
    }
    auto end = std::chrono::steady_clock::now();
    result.duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    return result;
}

// Stub implementations – each returns a successful result with placeholder output.
ActionResult ActionExecutor::executeFileEdit(const Action&) {
    ActionResult r("file_edit");
    r.status = ExecutionStatus::COMPLETED;
    r.output = "File edit simulated";
    return r;
}
ActionResult ActionExecutor::executeSearchFiles(const Action&) {
    ActionResult r("search_files");
    r.status = ExecutionStatus::COMPLETED;
    r.output = "Search completed";
    return r;
}
ActionResult ActionExecutor::executeRunBuild(const Action&) {
    ActionResult r("run_build");
    r.status = ExecutionStatus::COMPLETED;
    r.output = "Build simulated";
    return r;
}
ActionResult ActionExecutor::executeExecuteTests(const Action&) {
    ActionResult r("execute_tests");
    r.status = ExecutionStatus::COMPLETED;
    r.output = "Tests simulated";
    return r;
}
ActionResult ActionExecutor::executeCommitGit(const Action&) {
    ActionResult r("commit_git");
    r.status = ExecutionStatus::COMPLETED;
    r.output = "Git commit simulated";
    return r;
}
ActionResult ActionExecutor::executeInvokeCommand(const Action&) {
    ActionResult r("invoke_command");
    r.status = ExecutionStatus::COMPLETED;
    r.output = "Command executed";
    return r;
}
ActionResult ActionExecutor::executeRecursiveAgent(const Action&) {
    ActionResult r("recursive_agent");
    r.status = ExecutionStatus::COMPLETED;
    r.output = "Recursive agent simulated";
    return r;
}
ActionResult ActionExecutor::executeQueryUser(const Action&) {
    ActionResult r("query_user");
    r.status = ExecutionStatus::COMPLETED;
    r.output = "User input simulated";
    return r;
}

void ActionExecutor::cancelExecution() { m_cancelled = true; }
void ActionExecutor::pauseExecution() { m_paused = true; }
void ActionExecutor::resumeExecution() { m_paused = false; }

bool ActionExecutor::rollbackPlan(const ExecutionPlan&, const PlanResult&) {
    // Stub – no real rollback performed.
    return true;
}

void ActionExecutor::setPlanStartedCallback(std::function<void(int)> callback) { m_planStartedCallback = std::move(callback); }
void ActionExecutor::setActionStartedCallback(std::function<void(int, const std::string&)> callback) { m_actionStartedCallback = std::move(callback); }
void ActionExecutor::setActionCompletedCallback(std::function<void(int, bool, const ActionResult&)> callback) { m_actionCompletedCallback = std::move(callback); }
void ActionExecutor::setActionFailedCallback(std::function<void(int, const std::string&, bool)> callback) { m_actionFailedCallback = std::move(callback); }
void ActionExecutor::setProgressUpdatedCallback(std::function<void(int, int)> callback) { m_progressUpdatedCallback = std::move(callback); }
void ActionExecutor::setPlanCompletedCallback(std::function<void(bool, const PlanResult&)> callback) { m_planCompletedCallback = std::move(callback); }

} // namespace RawrXD
