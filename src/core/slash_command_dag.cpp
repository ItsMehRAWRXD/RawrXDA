// ============================================================================
// slash_command_dag.cpp — Slash Commands as DAG Tasks Implementation
// ============================================================================

#include "slash_command_dag.h"
#include <chrono>

namespace RawrXD {
namespace SlashCommand {

// ============================================================================
// SlashCommandScheduler Implementation
// ============================================================================

SlashCommandScheduler::SlashCommandScheduler() = default;
SlashCommandScheduler::~SlashCommandScheduler() { shutdown(); }

bool SlashCommandScheduler::initialize(ExecutionScheduler* execScheduler,
                                        AST::ASTGraphEngine* astEngine) {
    m_execScheduler = execScheduler;
    m_astEngine = astEngine;
    m_nextTaskID.store(1);
    m_activeTasks.store(0);
    
    // Register built-in handlers
    registerHandler(CommandType::Explain, std::make_unique<ExplainCommandHandler>());
    registerHandler(CommandType::Fix, std::make_unique<FixCommandHandler>());
    registerHandler(CommandType::Test, std::make_unique<TestCommandHandler>());
    registerHandler(CommandType::Optimize, std::make_unique<OptimizeCommandHandler>());
    
    return true;
}

void SlashCommandScheduler::shutdown() {
    // Cancel all pending tasks
    std::unique_lock<std::mutex> lock(m_taskMutex);
    while (!m_taskQueue.empty()) {
        auto [priority, taskID] = m_taskQueue.top();
        m_taskQueue.pop();
        
        auto it = m_tasks.find(taskID);
        if (it != m_tasks.end()) {
            it->second->state = CommandState::Cancelled;
        }
    }
    lock.unlock();
    
    // Wait for active tasks
    while (m_activeTasks.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

void SlashCommandScheduler::registerHandler(CommandType type, 
                                             std::unique_ptr<ICommandHandler> handler) {
    std::unique_lock<std::shared_mutex> lock(m_handlerMutex);
    m_handlers[type] = std::move(handler);
}

void SlashCommandScheduler::unregisterHandler(CommandType type) {
    std::unique_lock<std::shared_mutex> lock(m_handlerMutex);
    m_handlers.erase(type);
}

uint64_t SlashCommandScheduler::submitCommand(CommandType type,
                                               uint32_t fileID, uint32_t line, uint32_t column,
                                               const std::string& target,
                                               const std::vector<std::string>& arguments,
                                               CommandPriority priority) {
    // Check backpressure
    if (m_config.enableBackpressure && isUnderBackpressure()) {
        // Drop low priority tasks when under pressure
        if (priority > CommandPriority::High) {
            m_commandsDropped++;
            return 0; // Rejected
        }
    }
    
    uint64_t taskID = m_nextTaskID.fetch_add(1);
    
    auto task = std::make_unique<CommandTask>();
    task->taskID = taskID;
    task->type = type;
    task->priority = priority;
    task->state = CommandState::Pending;
    task->fileID = fileID;
    task->line = line;
    task->column = column;
    task->target = target;
    task->arguments = arguments;
    task->enqueueTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Queue task
    uint64_t priorityValue = calculatePriorityValue(priority, task->enqueueTime);
    
    std::unique_lock<std::mutex> lock(m_taskMutex);
    m_tasks[taskID] = std::move(task);
    m_taskQueue.emplace(priorityValue, taskID);
    lock.unlock();
    
    m_commandsSubmitted++;
    return taskID;
}

void SlashCommandScheduler::cancelCommand(uint64_t taskID) {
    std::unique_lock<std::mutex> lock(m_taskMutex);
    auto it = m_tasks.find(taskID);
    if (it == m_tasks.end()) return;
    
    CommandTask* task = it->second.get();
    task->cancelRequested.store(true);
    
    // If running, call handler cancel
    if (task->state == CommandState::Executing) {
        lock.unlock();
        
        std::shared_lock<std::shared_mutex> handlerLock(m_handlerMutex);
        auto handlerIt = m_handlers.find(task->type);
        if (handlerIt != m_handlers.end()) {
            handlerIt->second->cancel(task);
        }
    }
}

CommandState SlashCommandScheduler::getCommandState(uint64_t taskID) const {
    std::shared_lock<std::mutex> lock(m_taskMutex);
    auto it = m_tasks.find(taskID);
    if (it == m_tasks.end()) return CommandState::Unknown;
    return it->second->state;
}

bool SlashCommandScheduler::awaitCommand(uint64_t taskID, std::string& result, 
                                           uint64_t timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    
    while (true) {
        CommandState state = getCommandState(taskID);
        
        if (state == CommandState::Completed) {
            std::shared_lock<std::mutex> lock(m_taskMutex);
            auto it = m_tasks.find(taskID);
            if (it != m_tasks.end()) {
                result = it->second->result;
                return it->second->success;
            }
            return false;
        }
        
        if (state == CommandState::Failed || state == CommandState::Cancelled) {
            return false;
        }
        
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start).count();
        
        if (elapsed >= static_cast<int64_t>(timeoutMs)) {
            return false; // Timeout
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

bool SlashCommandScheduler::processCommandTask() {
    // Check if we can start more tasks
    if (m_activeTasks.load() >= m_config.maxConcurrentCommands) {
        return false;
    }
    
    std::unique_lock<std::mutex> lock(m_taskMutex);
    if (m_taskQueue.empty()) {
        return false;
    }
    
    auto [priority, taskID] = m_taskQueue.top();
    m_taskQueue.pop();
    
    auto it = m_tasks.find(taskID);
    if (it == m_tasks.end()) {
        lock.unlock();
        return processCommandTask(); // Try next
    }
    
    CommandTask* task = it->second.get();
    if (task->cancelRequested.load()) {
        task->state = CommandState::Cancelled;
        lock.unlock();
        m_commandsCancelled++;
        return processCommandTask(); // Try next
    }
    
    lock.unlock();
    
    // Get handler
    std::shared_lock<std::shared_mutex> handlerLock(m_handlerMutex);
    auto handlerIt = m_handlers.find(task->type);
    if (handlerIt == m_handlers.end()) {
        task->state = CommandState::Failed;
        task->errorMessage = "No handler registered for command type";
        m_commandsFailed++;
        return true;
    }
    
    ICommandHandler* handler = handlerIt->second.get();
    handlerLock.unlock();
    
    // Execute command
    m_activeTasks.fetch_add(1);
    task->startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Phase 1: Pre-fetch AST context
    if (m_config.enablePrefetch) {
        task->state = CommandState::Prefetching;
        prefetchASTContext(task);
    }
    
    // Phase 2: Validate
    task->state = CommandState::Validating;
    std::string error;
    if (!handler->validate(task, error)) {
        task->state = CommandState::Failed;
        task->errorMessage = error;
        m_activeTasks.fetch_sub(1);
        m_commandsFailed++;
        return true;
    }
    
    // Phase 3: Execute
    task->state = CommandState::Executing;
    bool success = handler->execute(task);
    
    // Phase 4: Complete
    if (success) {
        task->state = CommandState::Completed;
        task->success = true;
        m_commandsCompleted++;
    } else if (task->cancelRequested.load()) {
        task->state = CommandState::Cancelled;
        m_commandsCancelled++;
    } else {
        task->state = CommandState::Failed;
        task->errorMessage = "Execution failed";
        m_commandsFailed++;
    }
    
    task->completeTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    m_activeTasks.fetch_sub(1);
    return true;
}

size_t SlashCommandScheduler::getPendingCount() const {
    std::shared_lock<std::mutex> lock(m_taskMutex);
    return m_taskQueue.size();
}

void SlashCommandScheduler::configure(const Config& config) {
    m_config = config;
}

SlashCommandScheduler::Config SlashCommandScheduler::getConfig() const {
    return m_config;
}

SlashCommandScheduler::Stats SlashCommandScheduler::getStats() const {
    Stats stats;
    stats.commandsSubmitted = m_commandsSubmitted.load();
    stats.commandsCompleted = m_commandsCompleted.load();
    stats.commandsCancelled = m_commandsCancelled.load();
    stats.commandsDropped = m_commandsDropped.load();
    stats.commandsFailed = m_commandsFailed.load();
    
    return stats;
}

void SlashCommandScheduler::resetStats() {
    m_commandsSubmitted.store(0);
    m_commandsCompleted.store(0);
    m_commandsCancelled.store(0);
    m_commandsDropped.store(0);
    m_commandsFailed.store(0);
}

void SlashCommandScheduler::prefetchASTContext(CommandTask* task) {
    // Pre-fetch AST nodes for the command context
    task->cursorNode = m_astEngine->findNodeAt(task->fileID, task->line, task->column);
    task->scopeNode = m_astEngine->getEnclosingScope(task->cursorNode);
    
    // Touch nodes to ensure they're cached
    m_astEngine->getNode(task->cursorNode);
    m_astEngine->getNode(task->scopeNode);
}

void SlashCommandScheduler::dropLowPriorityTasks() {
    if (!m_config.enableBackpressure) return;
    
    // When under pressure, drop Normal and Low priority tasks
    std::unique_lock<std::mutex> lock(m_taskMutex);
    
    std::vector<std::pair<uint64_t, uint64_t>> kept;
    while (!m_taskQueue.empty()) {
        auto item = m_taskQueue.top();
        m_taskQueue.pop();
        
        auto it = m_tasks.find(item.second);
        if (it != m_tasks.end()) {
            // Check priority (encoded in priority value)
            // Higher priority value = lower actual priority
            if (item.first > 1000000 && m_activeTasks.load() >= m_config.maxConcurrentCommands) {
                it->second->state = CommandState::Cancelled;
                m_commandsDropped++;
            } else {
                kept.push_back(item);
            }
        }
    }
    
    for (const auto& item : kept) {
        m_taskQueue.push(item);
    }
}

uint64_t SlashCommandScheduler::calculatePriorityValue(CommandPriority priority, 
                                                         uint64_t enqueueTime) const {
    // Priority encoding: lower value = higher priority
    // Critical: 0-999999, High: 1000000-1999999, Normal: 2000000+, Low: 3000000+
    uint64_t base = static_cast<uint64_t>(priority) * 1000000ULL;
    return base + enqueueTime;
}

// ============================================================================
// Built-in Command Handlers
// ============================================================================

bool ExplainCommandHandler::validate(const CommandTask* task, std::string& error) {
    if (task->target.empty()) {
        error = "Explain command requires a target";
        return false;
    }
    return true;
}

bool ExplainCommandHandler::execute(CommandTask* task) {
    // In real implementation: call AI model to generate explanation
    task->result = "Explanation for: " + task->target + "\n\n" +
                   "This is a placeholder explanation. In production, " +
                   "this would use the AST context to provide a detailed " +
                   "explanation of the selected code.";
    return true;
}

void ExplainCommandHandler::cancel(CommandTask* task) {
    // Signal cancellation to any ongoing AI inference
    task->cancelRequested.store(true);
}

uint64_t ExplainCommandHandler::estimateCost(const CommandTask* task) const {
    // Estimate based on target complexity
    return 1000 + task->target.length() * 10;
}

bool FixCommandHandler::validate(const CommandTask* task, std::string& error) {
    if (task->target.empty()) {
        error = "Fix command requires a target";
        return false;
    }
    return true;
}

bool FixCommandHandler::execute(CommandTask* task) {
    task->result = "Fix suggestions for: " + task->target + "\n\n" +
                   "1. Consider null checking\n" +
                   "2. Add error handling\n" +
                   "3. Review variable naming";
    return true;
}

void FixCommandHandler::cancel(CommandTask* task) {
    task->cancelRequested.store(true);
}

uint64_t FixCommandHandler::estimateCost(const CommandTask* task) const {
    return 2000 + task->target.length() * 20;
}

bool TestCommandHandler::validate(const CommandTask* task, std::string& error) {
    if (task->target.empty()) {
        error = "Test command requires a target";
        return false;
    }
    return true;
}

bool TestCommandHandler::execute(CommandTask* task) {
    task->result = "Test generation for: " + task->target + "\n\n" +
                   "Generated test cases:\n" +
                   "- test_basic_functionality\n" +
                   "- test_edge_cases\n" +
                   "- test_error_handling";
    return true;
}

void TestCommandHandler::cancel(CommandTask* task) {
    task->cancelRequested.store(true);
}

uint64_t TestCommandHandler::estimateCost(const CommandTask* task) const {
    return 3000 + task->target.length() * 30;
}

bool OptimizeCommandHandler::validate(const CommandTask* task, std::string& error) {
    if (task->target.empty()) {
        error = "Optimize command requires a target";
        return false;
    }
    return true;
}

bool OptimizeCommandHandler::execute(CommandTask* task) {
    task->result = "Optimization suggestions for: " + task->target + "\n\n" +
                   "1. Consider using const references\n" +
                   "2. Cache repeated calculations\n" +
                   "3. Review loop structure";
    return true;
}

void OptimizeCommandHandler::cancel(CommandTask* task) {
    task->cancelRequested.store(true);
}

uint64_t OptimizeCommandHandler::estimateCost(const CommandTask* task) const {
    return 2500 + task->target.length() * 25;
}

// ============================================================================
// Global Access
// ============================================================================

SlashCommandScheduler& getSlashCommandScheduler() {
    static SlashCommandScheduler instance;
    return instance;
}

} // namespace SlashCommand
} // namespace RawrXD
