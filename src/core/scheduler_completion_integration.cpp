// ============================================================================
// scheduler_completion_integration.cpp — Completion as Scheduled Phase
// ============================================================================

#include "scheduler_completion_integration.h"
#include <chrono>

namespace RawrXD {

// ============================================================================
// CompletionScheduler Implementation
// ============================================================================

CompletionScheduler::CompletionScheduler() = default;
CompletionScheduler::~CompletionScheduler() { shutdown(); }

bool CompletionScheduler::initialize(ExecutionScheduler* execScheduler, 
                                      AST::ASTGraphEngine* astEngine) {
    m_execScheduler = execScheduler;
    m_astEngine = astEngine;
    m_nextTaskID.store(1);
    m_activeTasks.store(0);
    return true;
}

void CompletionScheduler::shutdown() {
    // Cancel all pending tasks
    std::unique_lock<std::mutex> lock(m_taskMutex);
    while (!m_taskQueue.empty()) {
        auto [priority, taskID] = m_taskQueue.top();
        m_taskQueue.pop();
        
        auto it = m_tasks.find(taskID);
        if (it != m_tasks.end()) {
            it->second->cancelled = true;
        }
    }
    lock.unlock();
    
    // Wait for active tasks
    while (m_activeTasks.load() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

uint64_t CompletionScheduler::requestCompletion(uint32_t fileID, uint32_t line, 
                                                 uint32_t column,
                                                 const std::string& partialSymbol) {
    // Check backpressure
    if (m_config.enableBackpressure && isUnderBackpressure()) {
        m_tasksDropped++;
        return 0; // Rejected
    }
    
    uint64_t taskID = m_nextTaskID.fetch_add(1);
    
    auto task = std::make_unique<CompletionTask>();
    task->taskID = taskID;
    task->fileID = fileID;
    task->line = line;
    task->column = column;
    task->partialSymbol = partialSymbol;
    task->enqueueTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    // Build fingerprint
    if (m_config.enableCache) {
        task->fingerprint = m_astEngine->buildFingerprint(fileID, line, column, partialSymbol);
    }
    
    // Check cache immediately
    if (m_config.enableCache) {
        const auto* cached = m_astEngine->getCachedCompletions(task->fingerprint);
        if (cached) {
            task->completions = *cached;
            task->completed = true;
            task->completeTime = task->enqueueTime;
            
            std::unique_lock<std::mutex> lock(m_taskMutex);
            m_tasks[taskID] = std::move(task);
            lock.unlock();
            
            m_tasksCompleted++;
            return taskID; // Return immediately with cached results
        }
    }
    
    // Queue for processing
    uint64_t priority = calculatePriority(task.get());
    
    std::unique_lock<std::mutex> lock(m_taskMutex);
    m_tasks[taskID] = std::move(task);
    m_taskQueue.emplace(priority, taskID);
    lock.unlock();
    
    m_tasksSubmitted++;
    return taskID;
}

void CompletionScheduler::cancelCompletion(uint64_t taskID) {
    std::unique_lock<std::mutex> lock(m_taskMutex);
    auto it = m_tasks.find(taskID);
    if (it != m_tasks.end()) {
        it->second->cancelled = true;
    }
}

bool CompletionScheduler::isComplete(uint64_t taskID) const {
    std::shared_lock<std::mutex> lock(m_taskMutex);
    auto it = m_tasks.find(taskID);
    if (it == m_tasks.end()) return false;
    return it->second->completed || it->second->cancelled;
}

std::vector<std::string> CompletionScheduler::awaitCompletion(uint64_t taskID, 
                                                               uint64_t timeoutMs) {
    auto start = std::chrono::steady_clock::now();
    
    while (!isComplete(taskID)) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            now - start).count();
        
        if (elapsed >= static_cast<int64_t>(timeoutMs)) {
            return {}; // Timeout
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    std::shared_lock<std::mutex> lock(m_taskMutex);
    auto it = m_tasks.find(taskID);
    if (it == m_tasks.end()) return {};
    return it->second->completions;
}

bool CompletionScheduler::runCompletionPhase(CompletionPhase phase) {
    m_currentPhase.store(phase);
    
    switch (phase) {
        case CompletionPhase::IDLE:
            return processTaskQueue();
            
        case CompletionPhase::PREFETCH_CONTEXT:
            // Pre-fetch AST context for queued tasks
            return prefetchContexts();
            
        case CompletionPhase::BUILD_FINGERPRINT:
            // Already done during submission
            return true;
            
        case CompletionPhase::CACHE_LOOKUP:
            // Already done during submission
            return true;
            
        case CompletionPhase::MODEL_INFERENCE:
            // Run model inference for cache misses
            return runModelInference();
            
        case CompletionPhase::POSTPROCESS:
            // Filter/rank results
            return postprocessResults();
            
        case CompletionPhase::COMPLETE:
            return finalizeCompletions();
            
        default:
            return false;
    }
}

bool CompletionScheduler::processTaskQueue() {
    // Check if we can start more tasks
    if (m_activeTasks.load() >= m_config.maxConcurrentTasks) {
        return false;
    }
    
    std::unique_lock<std::mutex> lock(m_taskMutex);
    if (m_taskQueue.empty()) {
        return false;
    }
    
    auto [priority, taskID] = m_taskQueue.top();
    m_taskQueue.pop();
    
    auto it = m_tasks.find(taskID);
    if (it == m_tasks.end() || it->second->cancelled) {
        lock.unlock();
        return processTaskQueue(); // Try next
    }
    
    CompletionTask* task = it->second.get();
    task->startTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    lock.unlock();
    
    m_activeTasks.fetch_add(1);
    
    // Run through phases
    bool success = true;
    for (int phase = static_cast<int>(CompletionPhase::PREFETCH_CONTEXT);
         phase < static_cast<int>(CompletionPhase::COMPLETE) && success;
         ++phase) {
        if (task->cancelled) {
            success = false;
            break;
        }
        success = executeTaskPhase(task, static_cast<CompletionPhase>(phase));
    }
    
    completeTask(task);
    m_activeTasks.fetch_sub(1);
    
    return true;
}

bool CompletionScheduler::executeTaskPhase(CompletionTask* task, CompletionPhase phase) {
    switch (phase) {
        case CompletionPhase::PREFETCH_CONTEXT: {
            if (!m_config.enablePrefetch) return true;
            
            // Pre-fetch AST nodes around cursor
            AST::NodeID cursorNode = m_astEngine->findNodeAt(
                task->fileID, task->line, task->column);
            
            // Touch nodes to ensure they're in cache
            m_astEngine->getNode(cursorNode);
            m_astEngine->getEnclosingScope(cursorNode);
            
            return true;
        }
        
        case CompletionPhase::BUILD_FINGERPRINT: {
            // Already built during submission
            return true;
        }
        
        case CompletionPhase::CACHE_LOOKUP: {
            // Already checked during submission
            return true;
        }
        
        case CompletionPhase::MODEL_INFERENCE: {
            // In real implementation: call to CompletionEngine
            // For now: generate placeholder completions
            task->completions = {
                task->partialSymbol + "_suggestion_1",
                task->partialSymbol + "_suggestion_2",
                task->partialSymbol + "_suggestion_3"
            };
            
            // Cache results
            if (m_config.enableCache) {
                m_astEngine->cacheCompletions(task->fingerprint, task->completions);
            }
            
            return true;
        }
        
        case CompletionPhase::POSTPROCESS: {
            // Filter duplicates, sort by relevance
            std::sort(task->completions.begin(), task->completions.end());
            task->completions.erase(
                std::unique(task->completions.begin(), task->completions.end()),
                task->completions.end()
            );
            return true;
        }
        
        default:
            return false;
    }
}

void CompletionScheduler::completeTask(CompletionTask* task) {
    task->completed = true;
    task->completeTime = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    if (!task->cancelled) {
        m_tasksCompleted++;
    } else {
        m_tasksCancelled++;
    }
}

void CompletionScheduler::dropStaleTasks() {
    if (!m_config.enableBackpressure) return;
    
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    
    std::unique_lock<std::mutex> lock(m_taskMutex);
    
    // Remove stale tasks from queue
    std::vector<std::pair<uint64_t, uint64_t>> kept;
    while (!m_taskQueue.empty()) {
        auto item = m_taskQueue.top();
        m_taskQueue.pop();
        
        auto it = m_tasks.find(item.second);
        if (it != m_tasks.end()) {
            if (now - it->second->enqueueTime > m_config.taskTimeoutMs) {
                it->second->cancelled = true;
                m_tasksDropped++;
            } else {
                kept.push_back(item);
            }
        }
    }
    
    for (const auto& item : kept) {
        m_taskQueue.push(item);
    }
}

uint64_t CompletionScheduler::calculatePriority(const CompletionTask* task) const {
    // Priority based on:
    // 1. Cursor proximity to visible area (lower = higher priority)
    // 2. Age (older = higher priority)
    // 3. Partial symbol length (longer = higher priority, more specific)
    
    uint64_t priority = task->enqueueTime; // Age-based
    priority -= task->partialSymbol.length() * 1000; // Length bonus
    
    return priority;
}

void CompletionScheduler::configure(const Config& config) {
    m_config = config;
}

CompletionScheduler::Config CompletionScheduler::getConfig() const {
    return m_config;
}

CompletionScheduler::Stats CompletionScheduler::getStats() const {
    Stats stats;
    stats.tasksSubmitted = m_tasksSubmitted.load();
    stats.tasksCompleted = m_tasksCompleted.load();
    stats.tasksCancelled = m_tasksCancelled.load();
    stats.tasksDropped = m_tasksDropped.load();
    
    // Calculate latencies from completed tasks
    // (simplified - would track actual latencies in production)
    
    return stats;
}

void CompletionScheduler::resetStats() {
    m_tasksSubmitted.store(0);
    m_tasksCompleted.store(0);
    m_tasksCancelled.store(0);
    m_tasksDropped.store(0);
    m_cacheHits.store(0);
    m_cacheMisses.store(0);
}

bool CompletionScheduler::isUnderBackpressure() const {
    return getQueueDepth() >= m_config.maxQueueDepth;
}

uint32_t CompletionScheduler::getQueueDepth() const {
    std::shared_lock<std::mutex> lock(m_taskMutex);
    return static_cast<uint32_t>(m_taskQueue.size());
}

// Helper methods
bool CompletionScheduler::prefetchContexts() {
    // Pre-fetch AST context for all queued tasks
    std::shared_lock<std::mutex> lock(m_taskMutex);
    
    auto tempQueue = m_taskQueue;
    lock.unlock();
    
    while (!tempQueue.empty()) {
        auto [priority, taskID] = tempQueue.top();
        tempQueue.pop();
        
        auto it = m_tasks.find(taskID);
        if (it != m_tasks.end()) {
            CompletionTask* task = it->second.get();
            
            // Pre-fetch nodes around cursor
            AST::NodeID cursorNode = m_astEngine->findNodeAt(
                task->fileID, task->line, task->column);
            
            // Touch related nodes
            m_astEngine->getNode(cursorNode);
            m_astEngine->getEnclosingScope(cursorNode);
        }
    }
    
    return true;
}

bool CompletionScheduler::runModelInference() {
    // Process tasks that need model inference
    return processTaskQueue();
}

bool CompletionScheduler::postprocessResults() {
    // Post-processing is done per-task in executeTaskPhase
    return true;
}

bool CompletionScheduler::finalizeCompletions() {
    // Finalization is done per-task in completeTask
    return true;
}

// ============================================================================
// Global Access
// ============================================================================

CompletionScheduler& getCompletionScheduler() {
    static CompletionScheduler instance;
    return instance;
}

} // namespace RawrXD
