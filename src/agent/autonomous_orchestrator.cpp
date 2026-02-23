// ============================================================================
// autonomous_orchestrator.cpp — Ultimate Autonomous Agentic Orchestrator Implementation
// ============================================================================
#include "autonomous_orchestrator.hpp"
#include "agentic_deep_thinking_engine.hpp"
#include "meta_planner.hpp"
#include "autonomous_subagent.hpp"
#include "agentic_failure_detector.hpp"
#include "agentic_puppeteer.hpp"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <iomanip>
#include <random>
#include <cmath>

namespace fs = std::filesystem;

namespace RawrXD {

// ============================================================================
// TodoItem Implementation
// ============================================================================
std::string TodoItem::statusString() const {
    switch (status) {
        case Status::Pending:    return "pending";
        case Status::Queued:     return "queued";
        case Status::InProgress: return "in-progress";
        case Status::Completed:  return "completed";
        case Status::Failed:     return "failed";
        case Status::Skipped:    return "skipped";
        case Status::Blocked:    return "blocked";
    }
    return "unknown";
}

json TodoItem::toJSON() const {
    json j;
    j["id"] = id;
    j["title"] = title;
    j["description"] = description;
    j["category"] = category;
    j["targetFile"] = targetFile;
    j["relatedFiles"] = relatedFiles;
    j["priority"] = priority;
    j["complexity"] = complexity;
    j["estimatedIterations"] = estimatedIterations;
    j["estimatedTimeSeconds"] = estimatedTimeSeconds;
    j["status"] = statusString();
    j["result"] = result;
    j["actualIterations"] = actualIterations;
    j["actualTimeMs"] = actualTimeMs;
    j["confidence"] = confidence;
    j["dependencies"] = dependencies;
    j["blockedBy"] = blockedBy;
    return j;
}

TodoItem TodoItem::fromJSON(const json& j) {
    TodoItem todo;
    todo.id = j.value("id", 0ULL);
    todo.title = j.value("title", "");
    todo.description = j.value("description", "");
    todo.category = j.value("category", "");
    todo.targetFile = j.value("targetFile", "");
    if (j.contains("relatedFiles") && j["relatedFiles"].is_array()) {
        todo.relatedFiles = j["relatedFiles"].get<std::vector<std::string>>();
    }
    todo.priority = j.value("priority", 5);
    todo.complexity = j.value("complexity", 5);
    todo.estimatedIterations = j.value("estimatedIterations", 1);
    todo.estimatedTimeSeconds = j.value("estimatedTimeSeconds", 60);
    todo.result = j.value("result", "");
    todo.actualIterations = j.value("actualIterations", 0);
    todo.actualTimeMs = j.value("actualTimeMs", 0);
    todo.confidence = j.value("confidence", 0.0f);
    if (j.contains("dependencies") && j["dependencies"].is_array()) {
        todo.dependencies = j["dependencies"].get<std::vector<uint64_t>>();
    }
    if (j.contains("blockedBy") && j["blockedBy"].is_array()) {
        todo.blockedBy = j["blockedBy"].get<std::vector<uint64_t>>();
    }
    return todo;
}

// ============================================================================
// AuditResult Implementation
// ============================================================================
std::vector<TodoItem> AuditResult::getTopPriorityTodos(int n) const {
    auto sorted = todos;
    std::sort(sorted.begin(), sorted.end(), [](const TodoItem& a, const TodoItem& b) {
        if (a.priority != b.priority) return a.priority < b.priority;
        return a.complexity > b.complexity;
    });
    if ((int)sorted.size() > n) sorted.resize(n);
    return sorted;
}

std::vector<TodoItem> AuditResult::getMostDifficultTodos(int n) const {
    auto sorted = todos;
    std::sort(sorted.begin(), sorted.end(), [](const TodoItem& a, const TodoItem& b) {
        if (a.complexity != b.complexity) return a.complexity > b.complexity;
        return a.priority < b.priority;
    });
    if ((int)sorted.size() > n) sorted.resize(n);
    return sorted;
}

json AuditResult::toJSON() const {
    json j;
    j["totalFiles"] = totalFiles;
    j["filesWithIssues"] = filesWithIssues;
    j["totalLines"] = totalLines;
    j["criticalIssues"] = criticalIssues;
    j["warnings"] = warnings;
    j["suggestions"] = suggestions;
    j["issuesByCategory"] = issuesByCategory;
    j["productionReadinessGaps"] = productionReadinessGaps;
    j["todos"] = json::array();
    for (const auto& todo : todos) {
        j["todos"].push_back(todo.toJSON());
    }
    j["fullReport"] = fullReport;
    return j;
}

// ============================================================================
// TerminalLimits Implementation
// ============================================================================
int TerminalLimits::getTimeoutForTask(const TodoItem& task, const std::vector<TodoItem>& history) {
    if (!autoAdjust || strategy == AdjustmentStrategy::Fixed) {
        return currentTimeoutMs;
    }

    int baseTimeout = currentTimeoutMs;

    // Adjust based on complexity (1-10 scale)
    float complexityMultiplier = 1.0f;
    
    switch (strategy) {
        case AdjustmentStrategy::Linear:
            complexityMultiplier = 1.0f + (task.complexity / 10.0f);
            break;
        
        case AdjustmentStrategy::Exponential:
            complexityMultiplier = std::pow(1.2f, task.complexity);
            break;
        
        case AdjustmentStrategy::Adaptive:
            // Learn from history for similar tasks
            float avgTimeForCategory = 0.0f;
            int count = 0;
            for (const auto& h : history) {
                if (h.category == task.category && h.complexity == task.complexity) {
                    avgTimeForCategory += h.actualTimeMs;
                    count++;
                }
            }
            if (count > 0) {
                avgTimeForCategory /= count;
                // Use historical average with 20% buffer
                return std::clamp((int)(avgTimeForCategory * 1.2f), minTimeoutMs, maxTimeoutMs);
            }
            complexityMultiplier = 1.0f + (task.complexity / 10.0f) * 1.5f;
            break;
            
        default:
            break;
    }

    int adjustedTimeout = (int)(baseTimeout * complexityMultiplier);
    adjustedTimeout = std::clamp(adjustedTimeout, minTimeoutMs, maxTimeoutMs);

    return randomize ? getRandomizedTimeout() : adjustedTimeout;
}

int TerminalLimits::getRandomizedTimeout() const {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    
    int variance = (currentTimeoutMs * randomVariancePercent) / 100;
    std::uniform_int_distribution<> dis(-variance, variance);
    
    int randomized = currentTimeoutMs + dis(gen);
    return std::clamp(randomized, minTimeoutMs, maxTimeoutMs);
}

json TerminalLimits::toJSON() const {
    json j;
    j["minTimeoutMs"] = minTimeoutMs;
    j["maxTimeoutMs"] = maxTimeoutMs;
    j["currentTimeoutMs"] = currentTimeoutMs;
    j["autoAdjust"] = autoAdjust;
    j["randomize"] = randomize;
    j["randomVariancePercent"] = randomVariancePercent;
    j["strategy"] = (int)strategy;
    return j;
}

TerminalLimits TerminalLimits::fromJSON(const json& j) {
    TerminalLimits limits;
    limits.minTimeoutMs = j.value("minTimeoutMs", 1000);
    limits.maxTimeoutMs = j.value("maxTimeoutMs", 3600000);
    limits.currentTimeoutMs = j.value("currentTimeoutMs", 30000);
    limits.autoAdjust = j.value("autoAdjust", true);
    limits.randomize = j.value("randomize", false);
    limits.randomVariancePercent = j.value("randomVariancePercent", 10);
    limits.strategy = (TerminalLimits::AdjustmentStrategy)j.value("strategy", 3);
    return limits;
}

// ============================================================================
// AgentCycleConfig Implementation
// ============================================================================
int AgentCycleConfig::getTotalIterations(const TodoItem& task) const {
    // Base iterations from task estimate
    int base = task.estimatedIterations;
    
    // Apply cycle multiplier
    int total = base * cycleMultiplier;
    
    // Add per-agent overhead if multiple agents
    if (agentCount > 1) {
        switch (strategy) {
            case CycleStrategy::Competitive:
                // All agents do full work competitively
                total *= agentCount;
                break;
            case CycleStrategy::Collaborative:
                // Agents iterate together, slight overhead
                total = (int)(total * (1.0f + (agentCount - 1) * 0.2f));
                break;
            case CycleStrategy::Specialized:
                // Agents divide work, less total iterations
                total = (int)(total * 0.7f * agentCount);
                break;
            default:
                break;
        }
    }
    
    return std::max(1, total);
}

json AgentCycleConfig::toJSON() const {
    json j;
    j["cycleMultiplier"] = cycleMultiplier;
    j["agentCount"] = agentCount;
    j["enableDebate"] = enableDebate;
    j["enableVoting"] = enableVoting;
    j["consensusThreshold"] = consensusThreshold;
    j["agentModels"] = agentModels;
    j["strategy"] = (int)strategy;
    return j;
}

AgentCycleConfig AgentCycleConfig::fromJSON(const json& j) {
    AgentCycleConfig config;
    config.cycleMultiplier = std::clamp(j.value("cycleMultiplier", 1), 1, 99);
    config.agentCount = std::clamp(j.value("agentCount", 1), 1, 99);
    config.enableDebate = j.value("enableDebate", false);
    config.enableVoting = j.value("enableVoting", false);
    config.consensusThreshold = j.value("consensusThreshold", 0.7f);
    if (j.contains("agentModels") && j["agentModels"].is_array()) {
        config.agentModels = j["agentModels"].get<std::vector<std::string>>();
    }
    config.strategy = (AgentCycleConfig::CycleStrategy)j.value("strategy", 3);
    return config;
}

// ============================================================================
// OrchestratorConfig Implementation
// ============================================================================
json OrchestratorConfig::toJSON() const {
    json j;
    j["qualityMode"] = (int)qualityMode;
    j["executionMode"] = (int)executionMode;
    j["agentConfig"] = agentConfig.toJSON();
    j["terminalLimits"] = terminalLimits.toJSON();
    j["maxConcurrentTasks"] = maxConcurrentTasks;
    j["maxRetries"] = maxRetries;
    j["failFast"] = failFast;
    j["autoSave"] = autoSave;
    j["autoSaveIntervalSeconds"] = autoSaveIntervalSeconds;
    j["enableDetailedLogging"] = enableDetailedLogging;
    j["enableTelemetry"] = enableTelemetry;
    j["enableSelfHealing"] = enableSelfHealing;
    j["ignoreTokenLimits"] = ignoreTokenLimits;
    j["ignoreTimeLimits"] = ignoreTimeLimits;
    j["ignoreComplexityLimits"] = ignoreComplexityLimits;
    j["workspaceRoot"] = workspaceRoot;
    j["progressFile"] = progressFile;
    return j;
}

OrchestratorConfig OrchestratorConfig::fromJSON(const json& j) {
    OrchestratorConfig config;
    config.qualityMode = (QualityMode)j.value("qualityMode", 0);
    config.executionMode = (ExecutionMode)j.value("executionMode", 2);
    if (j.contains("agentConfig")) {
        config.agentConfig = AgentCycleConfig::fromJSON(j["agentConfig"]);
    }
    if (j.contains("terminalLimits")) {
        config.terminalLimits = TerminalLimits::fromJSON(j["terminalLimits"]);
    }
    config.maxConcurrentTasks = j.value("maxConcurrentTasks", 4);
    config.maxRetries = j.value("maxRetries", 3);
    config.failFast = j.value("failFast", false);
    config.autoSave = j.value("autoSave", true);
    config.autoSaveIntervalSeconds = j.value("autoSaveIntervalSeconds", 60);
    config.enableDetailedLogging = j.value("enableDetailedLogging", true);
    config.enableTelemetry = j.value("enableTelemetry", true);
    config.enableSelfHealing = j.value("enableSelfHealing", true);
    config.ignoreTokenLimits = j.value("ignoreTokenLimits", false);
    config.ignoreTimeLimits = j.value("ignoreTimeLimits", false);
    config.ignoreComplexityLimits = j.value("ignoreComplexityLimits", false);
    config.workspaceRoot = j.value("workspaceRoot", "");
    config.progressFile = j.value("progressFile", "orchestrator_progress.json");
    return config;
}

// ============================================================================
// ExecutionStats Implementation
// ============================================================================
float ExecutionStats::getCompletionPercent() const {
    if (totalTodos == 0) return 0.0f;
    return ((float)(completed + failed + skipped) / totalTodos) * 100.0f;
}

float ExecutionStats::getSuccessRate() const {
    int attempted = completed + failed;
    if (attempted == 0) return 0.0f;
    return ((float)completed / attempted) * 100.0f;
}

int ExecutionStats::getEstimatedRemainingTimeMs() const {
    int remaining = totalTodos - (completed + failed + skipped);
    if (remaining == 0 || completed == 0) return 0;
    return (int)(avgTaskTimeMs * remaining);
}

json ExecutionStats::toJSON() const {
    json j;
    j["totalTodos"] = totalTodos;
    j["completed"] = completed;
    j["failed"] = failed;
    j["skipped"] = skipped;
    j["inProgress"] = inProgress;
    j["totalIterations"] = totalIterations;
    j["totalAgentsSpawned"] = totalAgentsSpawned;
    j["avgConfidence"] = avgConfidence;
    j["avgTaskTimeMs"] = avgTaskTimeMs;
    j["totalTimeMs"] = totalTimeMs;
    j["completionPercent"] = getCompletionPercent();
    j["successRate"] = getSuccessRate();
    j["estimatedRemainingTimeMs"] = getEstimatedRemainingTimeMs();
    j["completedByCategory"] = completedByCategory;
    j["failedByCategory"] = failedByCategory;
    return j;
}

// ============================================================================
// IterationTracker Implementation
// ============================================================================
void IterationTracker::recordIteration(const IterationRecord& record) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_iterations.push_back(record);
    m_iterationsByTodo[record.todoId].push_back(record);
}

std::vector<IterationTracker::IterationRecord> IterationTracker::getIterationsForTodo(uint64_t todoId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_iterationsByTodo.find(todoId);
    if (it != m_iterationsByTodo.end()) {
        return it->second;
    }
    return {};
}

int IterationTracker::getTotalIterations() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return (int)m_iterations.size();
}

float IterationTracker::getAverageIterationsPerTodo() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_iterationsByTodo.empty()) return 0.0f;
    return (float)m_iterations.size() / m_iterationsByTodo.size();
}

json IterationTracker::getAnalytics() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    json j;
    j["totalIterations"] = m_iterations.size();
    j["uniqueTodos"] = m_iterationsByTodo.size();
    j["avgIterationsPerTodo"] = getAverageIterationsPerTodo();
    
    // Calculate success rates
    int successful = 0;
    int failed = 0;
    float totalConfidence = 0.0f;
    int totalDuration = 0;
    
    for (const auto& iter : m_iterations) {
        if (iter.success) successful++;
        else failed++;
        totalConfidence += iter.confidence;
        totalDuration += iter.durationMs;
    }
    
    j["successfulIterations"] = successful;
    j["failedIterations"] = failed;
    j["successRate"] = m_iterations.empty() ? 0.0f : ((float)successful / m_iterations.size() * 100.0f);
    j["avgConfidence"] = m_iterations.empty() ? 0.0f : (totalConfidence / m_iterations.size());
    j["avgDurationMs"] = m_iterations.empty() ? 0 : (totalDuration / (int)m_iterations.size());
    j["totalDurationMs"] = totalDuration;
    
    return j;
}

// ============================================================================
// AutonomousOrchestrator Implementation
// ============================================================================
AutonomousOrchestrator::AutonomousOrchestrator(const OrchestratorConfig& config)
    : m_config(config)
{
    // Initialize components
    m_thinkingEngine = std::make_unique<AgenticDeepThinkingEngine>();
    m_planner = std::make_unique<MetaPlanner>();
    m_failureDetector = std::make_unique<AgenticFailureDetector>();
    m_puppeteer = std::make_unique<AgenticPuppeteer>();
}

AutonomousOrchestrator::~AutonomousOrchestrator() {
    if (m_running.load()) {
        cancel();
    }
}

void AutonomousOrchestrator::setConfig(const OrchestratorConfig& config) {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    m_config = config;
    applyQualityMode(config.qualityMode);
}

void AutonomousOrchestrator::setQualityMode(QualityMode mode) {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    m_config.qualityMode = mode;
    applyQualityMode(mode);
}

void AutonomousOrchestrator::setAgentCycleMultiplier(int multiplier) {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    m_config.agentConfig.cycleMultiplier = std::clamp(multiplier, 1, 99);
}

void AutonomousOrchestrator::setAgentCount(int count) {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    m_config.agentConfig.agentCount = std::clamp(count, 1, 99);
}

// ============================================================================
// Codebase Audit Implementation
// ============================================================================
AuditResult AutonomousOrchestrator::auditCodebase(const std::string& path, bool deep) {
    AuditResult result;
    result.totalFiles = 0;
    result.filesWithIssues = 0;
    result.totalLines = 0;
    
    if (!fs::exists(path)) {
        result.fullReport = json{{"error", "Path does not exist"}};
        return result;
    }
    
    std::vector<TodoItem> allTodos = auditDirectory(path, deep);
    result.todos = allTodos;
    
    // Categorize issues
    for (const auto& todo : allTodos) {
        result.issuesByCategory[todo.category]++;
        
        if (todo.priority <= 3) result.criticalIssues++;
        else if (todo.priority <= 6) result.warnings++;
        else result.suggestions++;
        
        if (!todo.targetFile.empty()) {
            result.filesWithIssues++;
        }
    }
    
    // Production readiness check
    if (deep) {
        result.productionReadinessGaps = {
            "Missing comprehensive error handling",
            "Incomplete logging in critical paths",
            "No performance benchmarks for new features",
            "Missing unit tests for edge cases",
            "Documentation needs updates for new APIs"
        };
    }
    
    result.fullReport = json{
        {"auditPath", path},
        {"deep", deep},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
        {"summary", {
            {"totalTodos", result.todos.size()},
            {"criticalIssues", result.criticalIssues},
            {"warnings", result.warnings},
            {"suggestions", result.suggestions}
        }}
    };
    
    return result;
}

AuditResult AutonomousOrchestrator::auditFiles(const std::vector<std::string>& files) {
    AuditResult result;
    
    for (const auto& file : files) {
        auto fileTodos = auditFile(file);
        result.todos.insert(result.todos.end(), fileTodos.begin(), fileTodos.end());
        result.totalFiles++;
        if (!fileTodos.empty()) result.filesWithIssues++;
    }
    
    return result;
}

AuditResult AutonomousOrchestrator::auditProductionReadiness(const std::string& path) {
    AuditResult result = auditCodebase(path, true);
    
    // Add production-specific checks
    for (const auto& todo : result.todos) {
        auto prodChecks = checkProductionReadiness(todo.targetFile);
        result.todos.insert(result.todos.end(), prodChecks.begin(), prodChecks.end());
    }
    
    return result;
}

std::vector<TodoItem> AutonomousOrchestrator::generateTodos(const std::string& description) {
    std::vector<TodoItem> todos;
    
    // Use deep thinking engine to analyze the description
    AgenticDeepThinkingEngine::ThinkingContext ctx;
    ctx.problem = "Generate actionable todos from: " + description;
    ctx.maxIterations = 3;
    ctx.deepResearch = true;
    
    auto result = m_thinkingEngine->think(ctx);
    
    // Parse the result to extract todos
    std::regex todoPattern(R"(\[TODO\]\s*(.+?)\s*\[/TODO\])");
    std::smatch match;
    std::string answer = result.finalAnswer;
    
    uint64_t todoId = m_nextTodoId.load();
    while (std::regex_search(answer, match, todoPattern)) {
        TodoItem todo;
        todo.id = todoId++;
        todo.title = match[1].str();
        todo.description = description;
        todo.category = "generated";
        todo.priority = 5;
        todo.complexity = 5;
        todo.estimatedIterations = 1;
        todos.push_back(todo);
        answer = match.suffix();
    }
    
    m_nextTodoId.store(todoId);
    return todos;
}

// ============================================================================
// Todo Management
// ============================================================================
void AutonomousOrchestrator::addTodo(const TodoItem& todo) {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    TodoItem newTodo = todo;
    if (newTodo.id == 0) {
        newTodo.id = m_nextTodoId.fetch_add(1);
    }
    m_todos.push_back(newTodo);
}

void AutonomousOrchestrator::addTodos(const std::vector<TodoItem>& todos) {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    for (const auto& todo : todos) {
        TodoItem newTodo = todo;
        if (newTodo.id == 0) {
            newTodo.id = m_nextTodoId.fetch_add(1);
        }
        m_todos.push_back(newTodo);
    }
}

void AutonomousOrchestrator::removeTodo(uint64_t id) {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    m_todos.erase(
        std::remove_if(m_todos.begin(), m_todos.end(),
            [id](const TodoItem& t) { return t.id == id; }),
        m_todos.end()
    );
}

void AutonomousOrchestrator::clearTodos() {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    m_todos.clear();
}

std::vector<TodoItem> AutonomousOrchestrator::getTodos() const {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    return m_todos;
}

std::vector<TodoItem> AutonomousOrchestrator::getPendingTodos() const {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    std::vector<TodoItem> pending;
    for (const auto& todo : m_todos) {
        if (todo.status == TodoItem::Status::Pending || 
            todo.status == TodoItem::Status::Queued) {
            pending.push_back(todo);
        }
    }
    return pending;
}

std::vector<TodoItem> AutonomousOrchestrator::getCompletedTodos() const {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    std::vector<TodoItem> completed;
    for (const auto& todo : m_todos) {
        if (todo.status == TodoItem::Status::Completed) {
            completed.push_back(todo);
        }
    }
    return completed;
}

std::vector<TodoItem> AutonomousOrchestrator::getFailedTodos() const {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    std::vector<TodoItem> failed;
    for (const auto& todo : m_todos) {
        if (todo.status == TodoItem::Status::Failed) {
            failed.push_back(todo);
        }
    }
    return failed;
}

TodoItem* AutonomousOrchestrator::getTodo(uint64_t id) {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    for (auto& todo : m_todos) {
        if (todo.id == id) return &todo;
    }
    return nullptr;
}

// ============================================================================
// Execution
// ============================================================================
bool AutonomousOrchestrator::execute() {
    if (m_running.load()) return false;
    
    m_running.store(true);
    m_cancelRequested.store(false);
    m_paused.store(false);
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.startTime = std::chrono::steady_clock::now();
        m_stats.totalTodos = (int)m_todos.size();
    }
    
    executionLoop();
    
    m_running.store(false);
    
    if (m_onAllCompleted) {
        m_onAllCompleted(m_stats);
    }
    
    return true;
}

bool AutonomousOrchestrator::executeTopDifficult(int n) {
    auto todos = getTodos();
    std::sort(todos.begin(), todos.end(), [](const TodoItem& a, const TodoItem& b) {
        return a.complexity > b.complexity;
    });
    
    if ((int)todos.size() > n) todos.resize(n);
    
    clearTodos();
    addTodos(todos);
    
    return execute();
}

bool AutonomousOrchestrator::executeTopPriority(int n) {
    auto todos = getTodos();
    std::sort(todos.begin(), todos.end(), [](const TodoItem& a, const TodoItem& b) {
        return a.priority < b.priority;
    });
    
    if ((int)todos.size() > n) todos.resize(n);
    
    clearTodos();
    addTodos(todos);
    
    return execute();
}

bool AutonomousOrchestrator::executeCategory(const std::string& category) {
    auto todos = getTodos();
    todos.erase(
        std::remove_if(todos.begin(), todos.end(),
            [&category](const TodoItem& t) { return t.category != category; }),
        todos.end()
    );
    
    clearTodos();
    addTodos(todos);
    
    return execute();
}

bool AutonomousOrchestrator::executeTodo(uint64_t id) {
    std::lock_guard<std::mutex> lock(m_todosMutex);
    for (auto& todo : m_todos) {
        if (todo.id == id) {
            return executeTodoInternal(todo);
        }
    }
    return false;
}

void AutonomousOrchestrator::executeAsync() {
    std::thread([this]() {
        execute();
    }).detach();
}

void AutonomousOrchestrator::cancel() {
    m_cancelRequested.store(true);
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

void AutonomousOrchestrator::pause() {
    m_paused.store(true);
}

void AutonomousOrchestrator::resume() {
    m_paused.store(false);
}

// ============================================================================
// Progress Tracking
// ============================================================================
ExecutionStats AutonomousOrchestrator::getStats() const {
    std::lock_guard<std::mutex> lock(m_statsMutex);
    return m_stats;
}

float AutonomousOrchestrator::getProgress() const {
    return getStats().getCompletionPercent();
}

std::string AutonomousOrchestrator::getStatusSummary() const {
    auto stats = getStats();
    std::ostringstream oss;
    oss << "Progress: " << std::fixed << std::setprecision(1) << stats.getCompletionPercent() << "% "
        << "(" << stats.completed << "/" << stats.totalTodos << " completed, "
        << stats.failed << " failed, " << stats.inProgress << " in progress)";
    return oss.str();
}

json AutonomousOrchestrator::getDetailedStatus() const {
    json j;
    j["stats"] = getStats().toJSON();
    j["iterations"] = m_iterationTracker.getAnalytics();
    j["config"] = m_config.toJSON();
    j["todos"] = json::array();
    
    auto todos = getTodos();
    for (const auto& todo : todos) {
        j["todos"].push_back(todo.toJSON());
    }
    
    return j;
}

json AutonomousOrchestrator::getIterationAnalytics() const {
    return m_iterationTracker.getAnalytics();
}

// ============================================================================
// Persistence
// ============================================================================
bool AutonomousOrchestrator::saveProgress(const std::string& filename) {
    std::string file = filename.empty() ? m_config.progressFile : filename;
    
    json j = getDetailedStatus();
    
    try {
        std::ofstream ofs(file);
        ofs << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool AutonomousOrchestrator::loadProgress(const std::string& filename) {
    std::string file = filename.empty() ? m_config.progressFile : filename;
    
    try {
        std::ifstream ifs(file);
        json j = json::parse(ifs);
        
        if (j.contains("config")) {
            m_config = OrchestratorConfig::fromJSON(j["config"]);
        }
        
        if (j.contains("todos") && j["todos"].is_array()) {
            clearTodos();
            for (const auto& todoJson : j["todos"]) {
                addTodo(TodoItem::fromJSON(todoJson));
            }
        }
        
        return true;
    } catch (...) {
        return false;
    }
}

// ============================================================================
// Advanced Features
// ============================================================================
void AutonomousOrchestrator::autoOptimize() {
    // Analyze execution patterns
    analyzeExecutionPatterns();
    
    // Update terminal limits from history
    updateTerminalLimitsFromHistory();
    
    // Adjust quality mode if needed
    auto stats = getStats();
    if (stats.avgConfidence < 0.7f && m_config.qualityMode != QualityMode::Max) {
        setQualityMode(QualityMode::Max);
    }
}

json AutonomousOrchestrator::getOptimizationRecommendations() const {
    json j = json::array();
    
    auto stats = getStats();
    
    if (stats.avgConfidence < 0.7f) {
        j.push_back({
            {"type", "quality"},
            {"message", "Low average confidence. Consider increasing quality mode to Max."},
            {"action", "setQualityMode(Max)"}
        });
    }
    
    if (stats.getSuccessRate() < 80.0f) {
        j.push_back({
            {"type", "reliability"},
            {"message", "High failure rate. Enable self-healing and increase retry limit."},
            {"action", "enableSelfHealing + increaseMaxRetries"}
        });
    }
    
    if (m_config.agentConfig.cycleMultiplier == 1 && stats.totalTodos > 10) {
        j.push_back({
            {"type", "performance"},
            {"message", "Large workload detected. Consider increasing agent cycle multiplier."},
            {"action", "setAgentCycleMultiplier(4)"}
        });
    }
    
    return j;
}

// ============================================================================
// Internal Execution
// ============================================================================
void AutonomousOrchestrator::executionLoop() {
    while (!m_cancelRequested.load()) {
        // Wait if paused
        while (m_paused.load() && !m_cancelRequested.load()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (m_cancelRequested.load()) break;
        
        // Get executable todos (dependencies met)
        auto executableTodos = getExecutableTodos();
        if (executableTodos.empty()) break;
        
        // Execute based on mode
        int concurrency = m_config.executionMode == ExecutionMode::Sequential ? 
                         1 : m_config.maxConcurrentTasks;
        
        for (int i = 0; i < std::min((int)executableTodos.size(), concurrency); ++i) {
            if (m_cancelRequested.load()) break;
            
            TodoItem* todo = executableTodos[i];
            todo->status = TodoItem::Status::Queued;
            
            // Execute todo
            bool success = executeTodoInternal(*todo);
            
            // Update dependencies
            updateDependencies(todo->id);
            
            // Auto-save if enabled
            if (m_config.autoSave) {
                saveProgress();
            }
            
            if (!success && m_config.failFast) {
                m_cancelRequested.store(true);
                break;
            }
        }
    }
}

bool AutonomousOrchestrator::executeTodoInternal(TodoItem& todo) {
    if (m_onTodoStarted) {
        m_onTodoStarted(todo);
    }
    
    todo.status = TodoItem::Status::InProgress;
    todo.startTime = std::chrono::steady_clock::now();
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.inProgress++;
    }
    
    bool success = false;
    
    // Check if multi-agent is beneficial for this todo
    if (m_config.agentConfig.agentCount > 1 && todo.complexity >= 7) {
        success = executeTodoWithMultiAgent(todo);
    } else {
        int cycles = m_config.agentConfig.getTotalIterations(todo);
        success = executeTodoWithCycles(todo, cycles);
    }
    
    todo.endTime = std::chrono::steady_clock::now();
    todo.actualTimeMs = (int)std::chrono::duration_cast<std::chrono::milliseconds>(
        todo.endTime - todo.startTime).count();
    
    todo.status = success ? TodoItem::Status::Completed : TodoItem::Status::Failed;
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.inProgress--;
        if (success) {
            m_stats.completed++;
            m_stats.completedByCategory[todo.category]++;
            m_completedHistory.push_back(todo);
        } else {
            m_stats.failed++;
            m_stats.failedByCategory[todo.category]++;
            m_failedHistory.push_back(todo);
        }
        
        // Update averages
        float totalTime = m_stats.avgTaskTimeMs * (m_stats.completed + m_stats.failed - 1) + todo.actualTimeMs;
        m_stats.avgTaskTimeMs = totalTime / (m_stats.completed + m_stats.failed);
        
        float totalConf = m_stats.avgConfidence * (m_stats.completed - 1) + todo.confidence;
        m_stats.avgConfidence = m_stats.completed > 0 ? totalConf / m_stats.completed : 0.0f;
    }
    
    if (m_onTodoCompleted) {
        m_onTodoCompleted(todo, success);
    }
    
    return success;
}

bool AutonomousOrchestrator::executeTodoWithMultiAgent(TodoItem& todo) {
    AgenticDeepThinkingEngine::ThinkingContext ctx;
    ctx.problem = todo.description;
    ctx.language = "cpp";
    ctx.projectRoot = m_config.workspaceRoot;
    ctx.maxIterations = getMaxIterationsForQuality(todo);
    ctx.deepResearch = m_config.qualityMode == QualityMode::Max;
    ctx.enableMultiAgent = true;
    ctx.agentCount = m_config.agentConfig.agentCount;
    ctx.enableAgentDebate = m_config.agentConfig.enableDebate;
    ctx.enableAgentVoting = m_config.agentConfig.enableVoting;
    ctx.consensusThreshold = m_config.agentConfig.consensusThreshold;
    
    auto multiResult = m_thinkingEngine->thinkMultiAgent(ctx);
    
    todo.result = multiResult.consensusResult.finalAnswer;
    todo.confidence = multiResult.consensusConfidence;
    todo.actualIterations = multiResult.consensusResult.iterationCount;
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalAgentsSpawned += m_config.agentConfig.agentCount;
        m_stats.totalIterations += todo.actualIterations;
    }
    
    return multiResult.consensusReached;
}

bool AutonomousOrchestrator::executeTodoWithCycles(TodoItem& todo, int cycles) {
    AgenticDeepThinkingEngine::ThinkingContext ctx;
    ctx.problem = todo.description;
    ctx.language = "cpp";
    ctx.projectRoot = m_config.workspaceRoot;
    ctx.maxIterations = cycles;
    ctx.deepResearch = m_config.qualityMode == QualityMode::Max;
    ctx.cycleMultiplier = m_config.agentConfig.cycleMultiplier;
    
    auto result = m_thinkingEngine->think(ctx);
    
    todo.result = result.finalAnswer;
    todo.confidence = result.overallConfidence;
    todo.actualIterations = result.iterationCount;
    
    {
        std::lock_guard<std::mutex> lock(m_statsMutex);
        m_stats.totalIterations += todo.actualIterations;
    }
    
    // Track iteration details
    for (int i = 0; i < result.iterationCount; ++i) {
        IterationTracker::IterationRecord record;
        record.todoId = todo.id;
        record.iterationNumber = i + 1;
        record.agentId = 0;
        record.agentModel = "default";
        record.success = result.overallConfidence > 0.7f;
        record.confidence = result.overallConfidence;
        record.durationMs = (int)(result.elapsedMilliseconds / result.iterationCount);
        m_iterationTracker.recordIteration(record);
    }
    
    return result.overallConfidence > 0.7f;
}

// ============================================================================
// Dependency Management
// ============================================================================
std::vector<TodoItem*> AutonomousOrchestrator::getExecutableTodos() {
    std::vector<TodoItem*> executable;
    
    for (auto& todo : m_todos) {
        if (todo.status == TodoItem::Status::Pending && checkDependenciesMet(todo)) {
            executable.push_back(&todo);
        }
    }
    
    return executable;
}

bool AutonomousOrchestrator::checkDependenciesMet(const TodoItem& todo) {
    for (uint64_t depId : todo.dependencies) {
        const TodoItem* dep = const_cast<AutonomousOrchestrator*>(this)->getTodo(depId);
        if (dep && dep->status != TodoItem::Status::Completed) {
            return false;
        }
    }
    return true;
}

void AutonomousOrchestrator::updateDependencies(uint64_t completedId) {
    for (auto& todo : m_todos) {
        auto it = std::find(todo.blockedBy.begin(), todo.blockedBy.end(), completedId);
        if (it != todo.blockedBy.end()) {
            todo.blockedBy.erase(it);
            if (todo.blockedBy.empty() && todo.status == TodoItem::Status::Blocked) {
                todo.status = TodoItem::Status::Pending;
            }
        }
    }
}

// ============================================================================
// Quality Mode Adjustments
// ============================================================================
void AutonomousOrchestrator::applyQualityMode(QualityMode mode) {
    switch (mode) {
        case QualityMode::Auto:
            m_config.agentConfig.cycleMultiplier = 2;
            m_config.agentConfig.agentCount = 2;
            m_config.maxRetries = 2;
            break;
        
        case QualityMode::Balance:
            m_config.agentConfig.cycleMultiplier = 4;
            m_config.agentConfig.agentCount = 4;
            m_config.maxRetries = 3;
            break;
        
        case QualityMode::Max:
            m_config.agentConfig.cycleMultiplier = 8;
            m_config.agentConfig.agentCount = 8;
            m_config.maxRetries = 5;
            m_config.ignoreTokenLimits = true;
            m_config.ignoreTimeLimits = true;
            m_config.ignoreComplexityLimits = true;
            break;
    }
}

int AutonomousOrchestrator::getMaxIterationsForQuality(const TodoItem& todo) const {
    int base = todo.estimatedIterations;
    
    switch (m_config.qualityMode) {
        case QualityMode::Auto:
            return base * 2;
        case QualityMode::Balance:
            return base * 4;
        case QualityMode::Max:
            return base * 8;
    }
    return base;
}

int AutonomousOrchestrator::getMaxAgentsForQuality(const TodoItem& todo) const {
    if (todo.complexity < 5) return 1;
    
    switch (m_config.qualityMode) {
        case QualityMode::Auto:
            return 2;
        case QualityMode::Balance:
            return 4;
        case QualityMode::Max:
            return 8;
    }
    return 1;
}

// ============================================================================
// Terminal Time Management
// ============================================================================
int AutonomousOrchestrator::getAdjustedTerminalTimeout(const TodoItem& todo) const {
    return m_config.terminalLimits.getTimeoutForTask(todo, m_completedHistory);
}

void AutonomousOrchestrator::updateTerminalLimitsFromHistory() {
    if (m_completedHistory.empty()) return;
    
    // Calculate average time per complexity level
    std::unordered_map<int, std::vector<int>> timesByComplexity;
    for (const auto& todo : m_completedHistory) {
        timesByComplexity[todo.complexity].push_back(todo.actualTimeMs);
    }
    
    // Adjust current timeout based on median of complexity 5 tasks
    if (timesByComplexity.count(5) && !timesByComplexity[5].empty()) {
        auto& times = timesByComplexity[5];
        std::sort(times.begin(), times.end());
        int median = times[times.size() / 2];
        m_config.terminalLimits.currentTimeoutMs = std::clamp(
            median, 
            m_config.terminalLimits.minTimeoutMs,
            m_config.terminalLimits.maxTimeoutMs
        );
    }
}

// ============================================================================
// Audit Implementation
// ============================================================================
std::vector<TodoItem> AutonomousOrchestrator::auditDirectory(const std::string& dir, bool recursive) {
    std::vector<TodoItem> todos;
    
    if (!fs::exists(dir)) return todos;
    
    try {
        auto iterator = recursive ? 
            fs::recursive_directory_iterator(dir) : 
            fs::directory_iterator(dir);
            
        for (const auto& entry : iterator) {
            if (entry.is_regular_file()) {
                auto ext = entry.path().extension().string();
                if (ext == ".cpp" || ext == ".hpp" || ext == ".h" || ext == ".c") {
                    auto fileTodos = auditFile(entry.path().string());
                    todos.insert(todos.end(), fileTodos.begin(), fileTodos.end());
                }
            }
        }
    } catch (...) {
        // Ignore filesystem errors
    }
    
    return todos;
}

std::vector<TodoItem> AutonomousOrchestrator::auditFile(const std::string& file) {
    std::vector<TodoItem> todos;
    
    try {
        std::ifstream ifs(file);
        if (!ifs.good()) return todos;
        
        std::string line;
        int lineNum = 0;
        
        while (std::getline(ifs, line)) {
            lineNum++;
            
            // Check for TODO/FIXME/BUG comments
            if (line.find("TODO") != std::string::npos ||
                line.find("FIXME") != std::string::npos ||
                line.find("BUG") != std::string::npos) {
                
                TodoItem todo;
                todo.id = m_nextTodoId.fetch_add(1);
                todo.title = "Code comment at " + file + ":" + std::to_string(lineNum);
                todo.description = line;
                todo.targetFile = file;
                todo.category = "code-comment";
                todo.priority = 5;
                todo.complexity = 3;
                todos.push_back(todo);
            }
            
            // Check for potential issues
            if (line.find("throw ") != std::string::npos && 
                line.find("// PRODUCTION") == std::string::npos) {
                TodoItem todo;
                todo.id = m_nextTodoId.fetch_add(1);
                todo.title = "Exception usage at " + file + ":" + std::to_string(lineNum);
                todo.description = "Consider no-exception error handling";
                todo.targetFile = file;
                todo.category = "exception-handling";
                todo.priority = 4;
                todo.complexity = 6;
                todos.push_back(todo);
            }
        }
    } catch (...) {
        // Ignore read errors
    }
    
    return todos;
}

std::vector<TodoItem> AutonomousOrchestrator::checkProductionReadiness(const std::string& file) {
    std::vector<TodoItem> todos;
    
    // Check for missing error handling, logging, tests, etc.
    // This is a simplified version - real implementation would be much more comprehensive
    
    TodoItem todo;
    todo.id = m_nextTodoId.fetch_add(1);
    todo.title = "Production readiness check for " + file;
    todo.description = "Verify error handling, logging, tests, documentation";
    todo.targetFile = file;
    todo.category = "production-readiness";
    todo.priority = 3;
    todo.complexity = 7;
    todos.push_back(todo);
    
    return todos;
}

// ============================================================================
// Analysis
// ============================================================================
void AutonomousOrchestrator::analyzeExecutionPatterns() {
    if (m_completedHistory.size() < 10) return;
    
    // Analyze patterns in completed todos to optimize future execution
    std::unordered_map<std::string, std::vector<const TodoItem*>> byCategory;
    
    for (const auto& todo : m_completedHistory) {
        byCategory[todo.category].push_back(&todo);
    }
    
    // Calculate average metrics per category
    for (const auto& [category, todos] : byCategory) {
        float avgTime = 0.0f;
        float avgConfidence = 0.0f;
        
        for (const auto* todo : todos) {
            avgTime += todo->actualTimeMs;
            avgConfidence += todo->confidence;
        }
        
        avgTime /= todos.size();
        avgConfidence /= todos.size();
        
        // Use these metrics to adjust future estimates
        // (This is where machine learning could be applied)
    }
}

float AutonomousOrchestrator::estimateTaskComplexity(const TodoItem& todo) const {
    float complexity = (float)todo.complexity;
    
    // Adjust based on historical data for this category
    float historicalAvg = 5.0f;
    int count = 0;
    
    for (const auto& h : m_completedHistory) {
        if (h.category == todo.category) {
            historicalAvg += h.complexity;
            count++;
        }
    }
    
    if (count > 0) {
        historicalAvg /= count;
        complexity = (complexity + historicalAvg) / 2.0f;
    }
    
    return complexity;
}

} // namespace RawrXD
