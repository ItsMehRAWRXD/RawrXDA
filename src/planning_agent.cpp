#include "planning_agent.h"
#include "agentic_engine.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <chrono>
#include <random>
<<<<<<< HEAD
#include <thread>
#include <queue>
#include <unordered_set>
#include <iomanip>
#include <algorithm>
#include <mutex>
#include <fstream>
=======
>>>>>>> origin/main

using json = nlohmann::json;

// Windows UUID
#ifdef _WIN32
#include <rpc.h>
#pragma comment(lib, "Rpcrt4.lib")
#endif

PlanningAgent::PlanningAgent() {
    m_executor = std::make_unique<ActionExecutor>();
}

PlanningAgent::~PlanningAgent() {
}

void PlanningAgent::initialize() {
    // Setup initial state if needed
}

void PlanningAgent::setAgenticEngine(AgenticEngine* engine) {
    m_engine = engine;
}

std::string PlanningAgent::generateUUID() {
#ifdef _WIN32
    UUID uuid;
    UuidCreate(&uuid);
    unsigned char* str;
    UuidToStringA(&uuid, &str);
    std::string s((char*)str);
    RpcStringFreeA(&str);
    return s;
#else
    return "uuid-" + std::to_string(std::rand());
#endif
}

void PlanningAgent::createPlan(const std::string& goal) {
    m_tasks.clear();
    
    // Explicit Logic: Real AI Planning
    if (m_engine) {
        std::string prompt = "You are an expert planner. Create a step-by-step plan for the following goal: " + goal + 
                           "\nReturn a JSON array where each object has 'title' (string) and 'action_type' (string: file_edit, search, build, test, command, unknown) and 'params' (object).";
                           
        std::string response = m_engine->generateResponse(prompt);
        
        try {
            // Find JSON in response
            auto start = response.find("[");
            auto end = response.rfind("]");
            if (start != std::string::npos && end != std::string::npos) {
                json plan = json::parse(response.substr(start, end - start + 1));
                
                for (const auto& item : plan) {
                    PlanningTask task;
                    task.id = generateUUID();
                    task.title = item.value("title", "Untitled Task");
                    task.status = "pending";
                    
                    std::string type = item.value("action_type", "unknown");
                    // Map string to ActionType
                    if (type == "file_edit") task.associatedAction.type = ActionType::FileEdit;
                    else if (type == "search") task.associatedAction.type = ActionType::SearchFiles;
                    else if (type == "build") task.associatedAction.type = ActionType::RunBuild;
                    else if (type == "test") task.associatedAction.type = ActionType::ExecuteTests;
                    else if (type == "command") task.associatedAction.type = ActionType::InvokeCommand;
                    else task.associatedAction.type = ActionType::Unknown;
                    
                    if (item.contains("params")) {
                        // Manual conversion from nlohmann::json to std::map<string, any> or similar if Action uses that
                        // Assuming Action params is nlohmann::json or compatible
                        task.associatedAction.params = item["params"];
                    }
                    
                    m_tasks.push_back(task);
                }
                return; // Success
            }
        } catch (...) {
            std::cerr << "PlanningAgent: Failed to parse AI plan. Falling back to heuristic." << std::endl;
        }
    }

    // Heuristic Fallback
    std::string lowerGoal = goal;
    std::transform(lowerGoal.begin(), lowerGoal.end(), lowerGoal.begin(), ::tolower);
    
    if (lowerGoal.find("code") != std::string::npos || lowerGoal.find("implement") != std::string::npos) {
        generateCodePlan(goal);
    } else if (lowerGoal.find("debug") != std::string::npos || lowerGoal.find("fix") != std::string::npos) {
        generateDebugPlan(goal);
    } else {
        generateGenericPlan(goal);
    }
}

void PlanningAgent::addTask(const PlanningTask& task) {
    m_tasks.push_back(task);
}

std::vector<PlanningTask> PlanningAgent::getTasks() const {
    return m_tasks;
}

void PlanningAgent::executePlan() {
    ExecutionContext ctx;
    ctx.projectRoot = "D:\\rawrxd"; // Default
    m_executor->setContext(ctx);
    
    for (auto& task : m_tasks) {
        if (task.status == "completed") continue;
        
        task.status = "in-progress";
        
        // Execute the action associated with the task
        bool success = false;
        if (task.associatedAction.type != ActionType::Unknown) {
            success = m_executor->executeAction(task.associatedAction);
        } else {
            // No action defined. Treat as informational step unless it claims to be actionable.
            // In a real system, we might ask the LLM to refine this step.
            // For now, we explicitly fail undefined executable tasks to avoid "simulated success".
            // Only purely descriptive tasks should pass.
            success = false; 
        }
        
        if (success) {
            task.status = "completed";
        } else {
            task.status = "failed";
            // Stop on failure?
            // break;
        }
    }
}

// Plan Generators logic - converting abstract goals into concrete Actions where possible

void PlanningAgent::generateCodePlan(const std::string& goal) {
    // 1. Analyze
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Analyze Requirements";
    t1.status = "pending";
<<<<<<< HEAD
    t1.associatedAction.type = ActionType::SearchFiles;
    t1.associatedAction.params["query"] = goal;
    t1.associatedAction.description = "Search codebase for context: " + goal;
    m_tasks.push_back(t1);

=======
    t1.associatedAction.type = ActionType::SearchFiles; // Search for context
    t1.associatedAction.params["query"] = goal;
    m_tasks.push_back(t1);
    
>>>>>>> origin/main
    // 2. Implement
    PlanningTask t2;
    t2.id = generateUUID();
    t2.title = "Implement Code";
    t2.status = "pending";
<<<<<<< HEAD
    t2.dependsOn.push_back(t1.id);
    t2.associatedAction.type = ActionType::FileEdit;
    t2.associatedAction.description = "Implement: " + goal;
    m_tasks.push_back(t2);

    // 3. Build
    PlanningTask t3;
    t3.id = generateUUID();
    t3.title = "Build & Verify";
    t3.status = "pending";
    t3.dependsOn.push_back(t2.id);
    t3.associatedAction.type = ActionType::RunBuild;
    t3.associatedAction.description = "Compile and verify: " + goal;
    m_tasks.push_back(t3);

    // 4. Test
    PlanningTask t4;
    t4.id = generateUUID();
    t4.title = "Run Tests";
    t4.status = "pending";
    t4.dependsOn.push_back(t3.id);
    t4.associatedAction.type = ActionType::ExecuteTests;
    t4.associatedAction.description = "Validate implementation";
    m_tasks.push_back(t4);
}

void PlanningAgent::generateDebugPlan(const std::string& goal) {
    // 1. Reproduce (Search)
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Search for Error Context";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::SearchFiles;
    t1.associatedAction.params["query"] = goal;
    t1.associatedAction.description = "Find error patterns: " + goal;
    m_tasks.push_back(t1);

    // 2. Diagnose root cause
    PlanningTask t2;
    t2.id = generateUUID();
    t2.title = "Diagnose Root Cause";
    t2.status = "pending";
    t2.dependsOn.push_back(t1.id);
    t2.associatedAction.type = ActionType::SearchFiles;
    t2.associatedAction.params["query"] = "stack trace error " + goal;
    t2.associatedAction.description = "Trace through code path";
    m_tasks.push_back(t2);

    // 3. Apply fix
    PlanningTask t3;
    t3.id = generateUUID();
    t3.title = "Apply Minimal Fix";
    t3.status = "pending";
    t3.dependsOn.push_back(t2.id);
    t3.associatedAction.type = ActionType::FileEdit;
    t3.associatedAction.description = "Fix: " + goal;
    m_tasks.push_back(t3);

    // 4. Verify fix
    PlanningTask t4;
    t4.id = generateUUID();
    t4.title = "Verify Fix (Build + Test)";
    t4.status = "pending";
    t4.dependsOn.push_back(t3.id);
    t4.associatedAction.type = ActionType::RunBuild;
    t4.associatedAction.description = "Verify no regressions";
    m_tasks.push_back(t4);
}

void PlanningAgent::generateGenericPlan(const std::string& goal) {
    // 1. Research
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Research Strategy";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::SearchFiles;
    t1.associatedAction.params["query"] = goal;
    t1.associatedAction.description = "Research: " + goal;
    m_tasks.push_back(t1);

    // 2. Execute
    PlanningTask t2;
    t2.id = generateUUID();
    t2.title = "Execute Strategy";
    t2.status = "pending";
    t2.dependsOn.push_back(t1.id);
    t2.associatedAction.type = ActionType::InvokeCommand;
    t2.associatedAction.description = "Execute: " + goal;
    m_tasks.push_back(t2);

    // 3. Validate
    PlanningTask t3;
    t3.id = generateUUID();
    t3.title = "Validate Results";
    t3.status = "pending";
    t3.dependsOn.push_back(t2.id);
    t3.associatedAction.type = ActionType::SearchFiles;
    t3.associatedAction.description = "Verify outcome";
    m_tasks.push_back(t3);
}

bool PlanningAgent::isComplete() const {
    for (const auto& task : m_tasks) {
        if (task.status != "completed") return false;
    }
    return !m_tasks.empty();
}

std::string PlanningAgent::generateSummary() const {
    std::stringstream ss;
    ss << "Plan Status:\n";
    int completed = 0, failed = 0, pending = 0;
    for (const auto& task : m_tasks) {
        ss << " - [" << task.status << "] " << task.title;
        if (task.elapsedMs() > 0 && task.status != "pending")
            ss << " (" << task.elapsedMs() << "ms)";
        if (!task.lastError.empty())
            ss << " ERR: " << task.lastError;
        ss << "\n";
        if (task.status == "completed") completed++;
        else if (task.status == "failed") failed++;
        else if (task.status == "pending") pending++;
    }
    ss << "Summary: " << completed << " completed, " << failed << " failed, "
       << pending << " pending / " << m_tasks.size() << " total";
    if (!m_tasks.empty()) {
        ss << " (" << std::fixed << std::setprecision(1)
           << (getProgress() * 100.0f) << "%)";
    }
    return ss.str();
}

// ============================================================================
// executeTask — run a single task with rollback support
// ============================================================================

bool PlanningAgent::executeTask(PlanningTask& task) {
    task.status = "in-progress";
    task.startTime = std::chrono::steady_clock::now();

    if (task.associatedAction.type == ActionType::FileEdit) {
        createRollbackPoint(task);
    }

    bool success = false;
    if (task.associatedAction.type != ActionType::Unknown) {
        success = m_executor->executeAction(task.associatedAction);
    }

    task.endTime = std::chrono::steady_clock::now();
    if (success) {
        task.status = "completed";
    } else {
        task.status = "failed";
        task.lastError = "Execution failed for action type " +
            std::to_string((int)task.associatedAction.type);
    }
    return success;
}

// ============================================================================
// executeTaskWithRetry — retry a task with exponential backoff
// ============================================================================

bool PlanningAgent::executeTaskWithRetry(PlanningTask& task, int maxRetries) {
    for (int attempt = 0; attempt <= maxRetries; attempt++) {
        if (attempt > 0) {
            int backoffMs = 100 * (1 << (attempt - 1));
            std::this_thread::sleep_for(std::chrono::milliseconds(backoffMs));
            task.retryCount = attempt;
            task.status = "pending";
        }
        bool success = executeTask(task);
        if (success) return true;
        task.lastError = "Attempt " + std::to_string(attempt + 1) + "/" +
                         std::to_string(maxRetries + 1) + " failed";
    }
    return false;
}

// ============================================================================
// executePlanWithRetry — execute full plan with per-task retry + DAG deps
// ============================================================================

void PlanningAgent::executePlanWithRetry(int maxRetries) {
    m_planStartTime = std::chrono::steady_clock::now();
    ExecutionContext ctx;
    ctx.projectRoot = "D:\\rawrxd";
    m_executor->setContext(ctx);

    for (int i = 0; i < (int)m_tasks.size(); i++) {
        auto& task = m_tasks[i];
        if (task.status == "completed" || task.status == "skipped") continue;

        // Check dependencies
        bool depsReady = true;
        for (const auto& depId : task.dependsOn) {
            const auto* dep = getTaskById(depId);
            if (!dep || dep->status != "completed") {
                depsReady = false;
                break;
            }
        }
        if (!depsReady) {
            task.status = "skipped";
            task.lastError = "Dependency not met";
            notifyProgress(i, "skipped");
            continue;
        }

        notifyProgress(i, "in-progress");
        int retries = std::min(maxRetries, task.maxRetries);
        bool success = executeTaskWithRetry(task, retries);
        notifyProgress(i, task.status);

        if (!success) {
            // Skip downstream dependents
            for (int j = i + 1; j < (int)m_tasks.size(); j++) {
                for (const auto& depId : m_tasks[j].dependsOn) {
                    if (depId == task.id) {
                        m_tasks[j].status = "skipped";
                        m_tasks[j].lastError = "Upstream '" + task.title + "' failed";
                    }
                }
            }
        }
    }
    m_planEndTime = std::chrono::steady_clock::now();
}

// ============================================================================
// executePlanParallel — run independent (no-dep) tasks concurrently in waves
// ============================================================================

void PlanningAgent::executePlanParallel() {
    m_planStartTime = std::chrono::steady_clock::now();
    ExecutionContext ctx;
    ctx.projectRoot = "D:\\rawrxd";
    m_executor->setContext(ctx);

    bool madeProgress = true;
    while (madeProgress) {
        madeProgress = false;
        auto readyIds = getReadyTaskIds();
        if (readyIds.empty()) break;

        std::vector<std::thread> threads;
        std::mutex resultMutex;

        for (const auto& taskId : readyIds) {
            PlanningTask* task = getTaskById(taskId);
            if (!task) continue;

            threads.emplace_back([this, task, &resultMutex]() {
                bool success = executeTaskWithRetry(*task, task->maxRetries);
                std::lock_guard<std::mutex> lock(resultMutex);
                (void)success;
            });
        }

        for (auto& t : threads) {
            if (t.joinable()) t.join();
        }

        madeProgress = true;
        for (int i = 0; i < (int)m_tasks.size(); i++) {
            notifyProgress(i, m_tasks[i].status);
        }
    }
    m_planEndTime = std::chrono::steady_clock::now();
}

// ============================================================================
// Task Management
// ============================================================================

void PlanningAgent::addTaskWithDeps(const PlanningTask& task,
                                     const std::vector<std::string>& deps) {
    PlanningTask t = task;
    t.dependsOn = deps;
    if (t.id.empty()) t.id = generateUUID();
    m_tasks.push_back(t);
}

void PlanningAgent::removeTask(const std::string& taskId) {
    m_tasks.erase(
        std::remove_if(m_tasks.begin(), m_tasks.end(),
            [&](const PlanningTask& t) { return t.id == taskId; }),
        m_tasks.end());
    // Remove from dependency lists
    for (auto& task : m_tasks) {
        task.dependsOn.erase(
            std::remove(task.dependsOn.begin(), task.dependsOn.end(), taskId),
            task.dependsOn.end());
    }
}

PlanningTask* PlanningAgent::getTaskById(const std::string& id) {
    for (auto& task : m_tasks) {
        if (task.id == id) return &task;
    }
    return nullptr;
}

const PlanningTask* PlanningAgent::getTaskById(const std::string& id) const {
    for (const auto& task : m_tasks) {
        if (task.id == id) return &task;
    }
    return nullptr;
}

// ============================================================================
// Retry / Rollback
// ============================================================================

bool PlanningAgent::retryFailedTasks(int maxRetries) {
    bool anyRetried = false;
    for (auto& task : m_tasks) {
        if (task.status == "failed") {
            task.status = "pending";
            task.lastError.clear();
            task.retryCount = 0;
            bool success = executeTaskWithRetry(task, maxRetries);
            if (success) anyRetried = true;
        }
    }
    return anyRetried;
}

bool PlanningAgent::rollbackTask(const std::string& taskId) {
    PlanningTask* task = getTaskById(taskId);
    if (!task || !task->canRollback) return false;

    if (!task->rollbackFilePath.empty() && !task->rollbackContent.empty()) {
        Action rollbackAction;
        rollbackAction.type = ActionType::FileEdit;
        rollbackAction.params["path"] = task->rollbackFilePath;
        rollbackAction.params["content"] = task->rollbackContent;
        rollbackAction.description = "Rollback: " + task->title;

        bool success = m_executor->executeAction(rollbackAction);
        if (success) {
            task->status = "rolled-back";
            task->canRollback = false;
            return true;
        }
    }
    return false;
}

void PlanningAgent::rollbackAll() {
    for (int i = (int)m_tasks.size() - 1; i >= 0; i--) {
        if (m_tasks[i].canRollback && m_tasks[i].status == "completed") {
            rollbackTask(m_tasks[i].id);
        }
    }
}

// ============================================================================
// Validation
// ============================================================================

bool PlanningAgent::validatePlan() const {
    return getValidationErrors().empty();
}

std::vector<std::string> PlanningAgent::getValidationErrors() const {
    std::vector<std::string> errors;
    if (m_tasks.empty()) {
        errors.push_back("Plan has no tasks");
        return errors;
    }

    // Check for missing IDs
    for (const auto& task : m_tasks) {
        if (task.id.empty()) {
            errors.push_back("Task has empty ID: '" + task.title + "'");
        }
    }

    // Check for duplicate IDs
    std::unordered_set<std::string> seenIds;
    for (const auto& task : m_tasks) {
        if (!seenIds.insert(task.id).second) {
            errors.push_back("Duplicate task ID: " + task.id);
        }
    }

    // Check for missing dependencies
    for (const auto& task : m_tasks) {
        for (const auto& depId : task.dependsOn) {
            if (seenIds.find(depId) == seenIds.end()) {
                errors.push_back("Task '" + task.title +
                    "' depends on unknown ID: " + depId);
            }
        }
    }

    // Check for cyclic dependencies
    if (hasCyclicDeps()) {
        errors.push_back("Plan contains cyclic dependencies");
    }

    // Warn about Unknown action types
    int unknownCount = 0;
    for (const auto& task : m_tasks) {
        if (task.associatedAction.type == ActionType::Unknown) unknownCount++;
    }
    if (unknownCount > 0) {
        errors.push_back("Warning: " + std::to_string(unknownCount) +
                         " task(s) have Unknown action type");
    }
    return errors;
}

// ============================================================================
// Status & Reporting
// ============================================================================

bool PlanningAgent::hasFailures() const {
    for (const auto& task : m_tasks) {
        if (task.status == "failed") return true;
    }
    return false;
}

float PlanningAgent::getProgress() const {
    if (m_tasks.empty()) return 0.0f;
    return (float)getCompletedCount() / (float)m_tasks.size();
}

int PlanningAgent::getCompletedCount() const {
    int count = 0;
    for (const auto& task : m_tasks) {
        if (task.status == "completed") count++;
    }
    return count;
}

int PlanningAgent::getFailedCount() const {
    int count = 0;
    for (const auto& task : m_tasks) {
        if (task.status == "failed") count++;
    }
    return count;
}

int PlanningAgent::getPendingCount() const {
    int count = 0;
    for (const auto& task : m_tasks) {
        if (task.status == "pending") count++;
    }
    return count;
}

int PlanningAgent::getTotalCount() const {
    return (int)m_tasks.size();
}

std::string PlanningAgent::getPlanJSON() const {
    std::ostringstream oss;
    oss << "{\"tasks\":[";
    for (int i = 0; i < (int)m_tasks.size(); i++) {
        if (i > 0) oss << ",";
        const auto& t = m_tasks[i];
        oss << "{\"id\":\"" << t.id << "\""
            << ",\"title\":\"" << t.title << "\""
            << ",\"status\":\"" << t.status << "\""
            << ",\"priority\":" << t.priority
            << ",\"retryCount\":" << t.retryCount
            << ",\"maxRetries\":" << t.maxRetries
            << ",\"elapsedMs\":" << t.elapsedMs()
            << ",\"canRollback\":" << (t.canRollback ? "true" : "false")
            << ",\"actionType\":" << (int)t.associatedAction.type
            << ",\"dependsOn\":[";
        for (int d = 0; d < (int)t.dependsOn.size(); d++) {
            if (d > 0) oss << ",";
            oss << "\"" << t.dependsOn[d] << "\"";
        }
        oss << "]";
        if (!t.lastError.empty())
            oss << ",\"lastError\":\"" << t.lastError << "\"";
        oss << "}";
    }
    oss << "],\"progress\":" << getProgress()
        << ",\"completed\":" << getCompletedCount()
        << ",\"failed\":" << getFailedCount()
        << ",\"pending\":" << getPendingCount()
        << ",\"total\":" << getTotalCount()
        << "}";
    return oss.str();
}

// ============================================================================
// Plan Generators — Specialized templates
// ============================================================================

void PlanningAgent::generateRefactorPlan(const std::string& goal) {
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Identify Refactoring Targets";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::SearchFiles;
    t1.associatedAction.params["query"] = goal;
    t1.associatedAction.description = "Search for code to refactor";
    m_tasks.push_back(t1);

    PlanningTask t2;
    t2.id = generateUUID();
    t2.title = "Analyze Code Quality Metrics";
    t2.status = "pending";
    t2.dependsOn.push_back(t1.id);
    t2.associatedAction.type = ActionType::SearchFiles;
    t2.associatedAction.params["query"] = "complexity duplication coupling";
    t2.associatedAction.description = "Measure complexity";
    m_tasks.push_back(t2);

    PlanningTask t3;
    t3.id = generateUUID();
    t3.title = "Apply Refactoring Changes";
    t3.status = "pending";
    t3.dependsOn.push_back(t2.id);
    t3.associatedAction.type = ActionType::FileEdit;
    t3.associatedAction.description = "Refactor: " + goal;
    m_tasks.push_back(t3);

    PlanningTask t4;
    t4.id = generateUUID();
    t4.title = "Verify Refactoring (Build + Test)";
    t4.status = "pending";
    t4.dependsOn.push_back(t3.id);
    t4.associatedAction.type = ActionType::RunBuild;
    t4.associatedAction.description = "Verify build after refactoring";
    m_tasks.push_back(t4);
}

void PlanningAgent::generateTestPlan(const std::string& goal) {
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Identify Test Targets";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::SearchFiles;
    t1.associatedAction.params["query"] = goal;
    m_tasks.push_back(t1);

    PlanningTask t2;
    t2.id = generateUUID();
    t2.title = "Generate Test Cases";
    t2.status = "pending";
    t2.dependsOn.push_back(t1.id);
    t2.associatedAction.type = ActionType::FileEdit;
    t2.associatedAction.description = "Write tests for: " + goal;
    m_tasks.push_back(t2);

    PlanningTask t3;
    t3.id = generateUUID();
    t3.title = "Execute Test Suite";
    t3.status = "pending";
    t3.dependsOn.push_back(t2.id);
    t3.associatedAction.type = ActionType::ExecuteTests;
    t3.associatedAction.description = "Run tests";
    m_tasks.push_back(t3);

    PlanningTask t4;
    t4.id = generateUUID();
    t4.title = "Analyze Coverage Report";
    t4.status = "pending";
    t4.dependsOn.push_back(t3.id);
    t4.associatedAction.type = ActionType::InvokeCommand;
    t4.associatedAction.description = "Generate coverage report";
    m_tasks.push_back(t4);
}

void PlanningAgent::generateBuildPlan(const std::string& goal) {
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Check Build Prerequisites";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::InvokeCommand;
    t1.associatedAction.description = "Verify toolchain: " + goal;
    m_tasks.push_back(t1);

    PlanningTask t2;
    t2.id = generateUUID();
    t2.title = "Execute Build";
    t2.status = "pending";
    t2.dependsOn.push_back(t1.id);
    t2.associatedAction.type = ActionType::RunBuild;
    t2.associatedAction.description = "Build: " + goal;
    t2.priority = 1;
    m_tasks.push_back(t2);

    PlanningTask t3;
    t3.id = generateUUID();
    t3.title = "Post-Build Test Verification";
    t3.status = "pending";
    t3.dependsOn.push_back(t2.id);
    t3.associatedAction.type = ActionType::ExecuteTests;
    t3.associatedAction.description = "Run tests after build";
    m_tasks.push_back(t3);
}

// ============================================================================
// Internal Helpers
// ============================================================================

void PlanningAgent::notifyProgress(int index, const std::string& status) {
    if (m_progressCallback && index >= 0 && index < (int)m_tasks.size()) {
        m_progressCallback(index, (int)m_tasks.size(),
                            m_tasks[index].title, status);
    }
}

std::vector<std::string> PlanningAgent::getReadyTaskIds() const {
    std::vector<std::string> ready;
    for (const auto& task : m_tasks) {
        if (task.status != "pending") continue;
        bool allDepsMet = true;
        for (const auto& depId : task.dependsOn) {
            const auto* dep = getTaskById(depId);
            if (!dep || dep->status != "completed") {
                allDepsMet = false;
                break;
            }
        }
        if (allDepsMet) ready.push_back(task.id);
    }
    return ready;
}

bool PlanningAgent::hasCyclicDeps() const {
    // Kahn's algorithm: topological sort cycle detection
    std::unordered_map<std::string, int> inDegree;
    std::unordered_map<std::string, std::vector<std::string>> adj;

    for (const auto& task : m_tasks) {
        if (inDegree.find(task.id) == inDegree.end()) inDegree[task.id] = 0;
        for (const auto& depId : task.dependsOn) {
            adj[depId].push_back(task.id);
            inDegree[task.id]++;
        }
    }

    std::queue<std::string> q;
    for (const auto& [id, deg] : inDegree) {
        if (deg == 0) q.push(id);
    }

    int visited = 0;
    while (!q.empty()) {
        std::string node = q.front(); q.pop();
        visited++;
        for (const auto& neighbor : adj[node]) {
            if (--inDegree[neighbor] == 0) q.push(neighbor);
        }
    }
    return visited < (int)m_tasks.size();
}

void PlanningAgent::createRollbackPoint(PlanningTask& task) {
    if (task.associatedAction.type == ActionType::FileEdit &&
        task.associatedAction.params.count("path")) {
        std::string path = task.associatedAction.params["path"];
        // Read current file content for rollback
        std::ifstream ifs(path);
        if (ifs.good()) {
            std::ostringstream oss;
            oss << ifs.rdbuf();
            task.rollbackContent = oss.str();
        }
        task.rollbackFilePath = path;
        task.canRollback = true;
    }
=======
    t2.associatedAction.type = ActionType::FileEdit; // Placeholder for edit
    t2.associatedAction.description = "Implement: " + goal;
    m_tasks.push_back(t2);
    
    // 3. Build Not included in default plan to avoid errors, 
    // but user can add it.
}

void PlanningAgent::generateDebugPlan(const std::string& goal) {
     // 1. Reproduce (Search)
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Search for Error Context";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::SearchFiles; 
    t1.associatedAction.params["query"] = goal;
    m_tasks.push_back(t1);
}

void PlanningAgent::generateGenericPlan(const std::string& goal) {
    PlanningTask t1;
    t1.id = generateUUID();
    t1.title = "Research Strategy";
    t1.status = "pending";
    t1.associatedAction.type = ActionType::Unknown; // Abstract
    m_tasks.push_back(t1);
}

bool PlanningAgent::isComplete() const {
    for (const auto& task : m_tasks) {
        if (task.status != "completed") return false;
    }
    return !m_tasks.empty();
}

std::string PlanningAgent::generateSummary() const {
    std::stringstream ss;
    ss << "Plan Status:\n";
    for (const auto& task : m_tasks) {
        ss << " - " << task.title << ": " << task.status << "\n";
    }
    return ss.str();
>>>>>>> origin/main
}
