// distributed_pipeline_orchestrator.cpp — Phase 13: Distributed Pipeline Orchestrator
// DAG-based task scheduling with work-stealing thread pool,
// pipeline parallelism, deadline-aware priority queues, and
// dynamic load balancing across heterogeneous compute nodes.
//
// Rule: NO SOURCE FILE IS TO BE SIMPLIFIED
// Rule: No exceptions. Structured PatchResult returns only.
// Rule: All threading via std::mutex + std::lock_guard. No recursive locks.
#include "distributed_pipeline_orchestrator.hpp"
#include <cassert>
#include <cmath>
#include <sstream>

// ============================================================================
// Singleton
// ============================================================================
DistributedPipelineOrchestrator& DistributedPipelineOrchestrator::instance() {
    static DistributedPipelineOrchestrator inst;
    return inst;
}

DistributedPipelineOrchestrator::DistributedPipelineOrchestrator() = default;

DistributedPipelineOrchestrator::~DistributedPipelineOrchestrator() {
    if (m_running.load()) {
        shutdown();
    }
}

// ============================================================================
// Lifecycle
// ============================================================================
PatchResult DistributedPipelineOrchestrator::initialize(uint32_t workerThreads) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_running.load()) {
        return PatchResult::error("Pipeline orchestrator already running", -1);
    }

    m_numWorkers = workerThreads;
    if (m_numWorkers == 0) {
        m_numWorkers = std::thread::hardware_concurrency();
        if (m_numWorkers == 0) m_numWorkers = 4;  // Fallback
    }
    // Cap at 256 workers
    if (m_numWorkers > 256) m_numWorkers = 256;

    // Create per-worker steal deques
    m_workerDeques.clear();
    m_workerDeques.reserve(m_numWorkers);
    for (uint32_t i = 0; i < m_numWorkers; i++) {
        m_workerDeques.push_back(std::make_unique<WorkStealingDeque>(8192));
    }

    m_running.store(true);

    // Launch worker threads
    m_workers.clear();
    m_workers.reserve(m_numWorkers);
    for (uint32_t i = 0; i < m_numWorkers; i++) {
        m_workers.emplace_back(&DistributedPipelineOrchestrator::workerLoop, this, i);
    }

    // Launch heartbeat monitor
    m_heartbeatThread = std::thread(&DistributedPipelineOrchestrator::heartbeatMonitorLoop, this);

    return PatchResult::ok("Pipeline orchestrator initialized");
}

PatchResult DistributedPipelineOrchestrator::shutdown() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_running.load()) {
            return PatchResult::error("Pipeline orchestrator not running", -1);
        }
        m_running.store(false);
    }
    m_workAvailable.notify_all();

    // Join workers
    for (auto& t : m_workers) {
        if (t.joinable()) t.join();
    }
    m_workers.clear();

    // Join heartbeat
    if (m_heartbeatThread.joinable()) {
        m_heartbeatThread.join();
    }

    // Cleanup tasks
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (auto& [id, task] : m_tasks) {
            if (task.cleanup && task.taskData) {
                task.cleanup(task.taskData);
            }
        }
        m_tasks.clear();
        m_results.clear();
        m_workerDeques.clear();
        // Clear the priority queue
        while (!m_readyQueue.empty()) m_readyQueue.pop();
    }

    return PatchResult::ok("Pipeline orchestrator shut down");
}

// ============================================================================
// Task Submission
// ============================================================================
TaskResult DistributedPipelineOrchestrator::submitTask(PipelineTask& task) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running.load()) {
        return TaskResult::error(0, "Pipeline orchestrator not running", -1);
    }

    // Assign ID
    task.id = m_nextTaskId.fetch_add(1);
    task.state = StageState::Pending;
    task.submitTime = std::chrono::steady_clock::now();

    if (!task.execute) {
        return TaskResult::error(task.id, "Task has no execute function", -2);
    }

    // Register task
    m_tasks[task.id] = task;
    m_stats.tasksSubmitted.fetch_add(1);

    // Check if dependencies are met
    if (areDependenciesMet(task.id)) {
        m_tasks[task.id].state = StageState::Queued;
        m_readyQueue.push({task.priority, task.id});

        // Update peak queue depth
        size_t depth = m_readyQueue.size();
        uint64_t peak = m_stats.peakQueueDepth.load();
        while (depth > peak && !m_stats.peakQueueDepth.compare_exchange_weak(peak, depth)) {}
    }

    m_workAvailable.notify_one();
    return TaskResult::ok(task.id);
}

TaskResult DistributedPipelineOrchestrator::submitBatch(std::vector<PipelineTask>& tasks) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_running.load()) {
        return TaskResult::error(0, "Pipeline orchestrator not running", -1);
    }

    uint64_t firstId = 0;
    uint64_t count = 0;

    for (auto& task : tasks) {
        task.id = m_nextTaskId.fetch_add(1);
        task.state = StageState::Pending;
        task.submitTime = std::chrono::steady_clock::now();

        if (!task.execute) continue;

        m_tasks[task.id] = task;
        m_stats.tasksSubmitted.fetch_add(1);

        if (firstId == 0) firstId = task.id;
        count++;

        if (areDependenciesMet(task.id)) {
            m_tasks[task.id].state = StageState::Queued;
            m_readyQueue.push({task.priority, task.id});
        }
    }

    // Wake all workers for batch
    m_workAvailable.notify_all();

    return TaskResult::ok(firstId, 0, count);
}

PatchResult DistributedPipelineOrchestrator::cancelTask(uint64_t taskId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) {
        return PatchResult::error("Task not found", -1);
    }

    auto& task = it->second;
    if (task.state == StageState::Completed || task.state == StageState::Cancelled) {
        return PatchResult::error("Task already finished or cancelled", -2);
    }

    task.state = StageState::Cancelled;
    m_stats.tasksCancelled.fetch_add(1);

    if (m_eventCb) {
        m_eventCb(taskId, StageState::Cancelled, m_eventData);
    }

    return PatchResult::ok("Task cancelled");
}

PatchResult DistributedPipelineOrchestrator::cancelAll() {
    std::lock_guard<std::mutex> lock(m_mutex);

    uint64_t cancelled = 0;
    for (auto& [id, task] : m_tasks) {
        if (task.state == StageState::Pending || task.state == StageState::Queued) {
            task.state = StageState::Cancelled;
            cancelled++;
        }
    }

    m_stats.tasksCancelled.fetch_add(cancelled);
    while (!m_readyQueue.empty()) m_readyQueue.pop();

    return PatchResult::ok("All pending tasks cancelled");
}

// ============================================================================
// DAG Construction
// ============================================================================
PatchResult DistributedPipelineOrchestrator::addDependency(uint64_t fromTask, uint64_t toTask) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto fromIt = m_tasks.find(fromTask);
    auto toIt   = m_tasks.find(toTask);

    if (fromIt == m_tasks.end() || toIt == m_tasks.end()) {
        return PatchResult::error("One or both tasks not found", -1);
    }

    // toTask depends on fromTask (fromTask → toTask)
    toIt->second.dependencies.push_back(fromTask);
    fromIt->second.dependents.push_back(toTask);

    // Check for cycles
    if (detectCycle()) {
        // Roll back
        toIt->second.dependencies.pop_back();
        fromIt->second.dependents.pop_back();
        return PatchResult::error("Adding dependency would create a cycle", -2);
    }

    return PatchResult::ok("Dependency added");
}

PatchResult DistributedPipelineOrchestrator::removeDependency(uint64_t fromTask, uint64_t toTask) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto fromIt = m_tasks.find(fromTask);
    auto toIt   = m_tasks.find(toTask);

    if (fromIt == m_tasks.end() || toIt == m_tasks.end()) {
        return PatchResult::error("One or both tasks not found", -1);
    }

    auto& deps = toIt->second.dependencies;
    deps.erase(std::remove(deps.begin(), deps.end(), fromTask), deps.end());

    auto& dependents = fromIt->second.dependents;
    dependents.erase(std::remove(dependents.begin(), dependents.end(), toTask), dependents.end());

    return PatchResult::ok("Dependency removed");
}

bool DistributedPipelineOrchestrator::hasCycle() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return detectCycle();
}

std::vector<uint64_t> DistributedPipelineOrchestrator::topologicalSort() const {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Kahn's algorithm
    std::unordered_map<uint64_t, uint32_t> inDegree;
    for (auto& [id, task] : m_tasks) {
        if (inDegree.find(id) == inDegree.end()) inDegree[id] = 0;
        for (auto depId : task.dependents) {
            inDegree[depId]++;
        }
    }

    std::queue<uint64_t> zeroIn;
    for (auto& [id, deg] : inDegree) {
        if (deg == 0) zeroIn.push(id);
    }

    std::vector<uint64_t> sorted;
    sorted.reserve(m_tasks.size());

    while (!zeroIn.empty()) {
        uint64_t current = zeroIn.front();
        zeroIn.pop();
        sorted.push_back(current);

        auto it = m_tasks.find(current);
        if (it != m_tasks.end()) {
            for (auto depId : it->second.dependents) {
                inDegree[depId]--;
                if (inDegree[depId] == 0) {
                    zeroIn.push(depId);
                }
            }
        }
    }

    return sorted;
}

// ============================================================================
// Compute Nodes
// ============================================================================
PatchResult DistributedPipelineOrchestrator::registerNode(const ComputeNode& node) {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_nodes.find(node.nodeId) != m_nodes.end()) {
        return PatchResult::error("Node already registered", -1);
    }

    ComputeNode n = node;
    n.alive = true;
    n.lastHeartbeat = std::chrono::steady_clock::now();
    m_nodes[n.nodeId] = n;

    return PatchResult::ok("Node registered");
}

PatchResult DistributedPipelineOrchestrator::deregisterNode(uint32_t nodeId) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) {
        return PatchResult::error("Node not found", -1);
    }

    m_nodes.erase(it);
    return PatchResult::ok("Node deregistered");
}

PatchResult DistributedPipelineOrchestrator::heartbeat(uint32_t nodeId, double loadAvg,
                                                        uint32_t freeCores, uint64_t freeMem) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_nodes.find(nodeId);
    if (it == m_nodes.end()) {
        return PatchResult::error("Node not found", -1);
    }

    it->second.loadAverage    = loadAvg;
    it->second.availableCores = freeCores;
    it->second.availableMemory = freeMem;
    it->second.alive          = true;
    it->second.lastHeartbeat  = std::chrono::steady_clock::now();

    return PatchResult::ok("Heartbeat received");
}

std::vector<ComputeNode> DistributedPipelineOrchestrator::getNodeStatus() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<ComputeNode> result;
    result.reserve(m_nodes.size());
    for (auto& [id, node] : m_nodes) {
        result.push_back(node);
    }
    return result;
}

uint32_t DistributedPipelineOrchestrator::aliveNodeCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    uint32_t count = 0;
    for (auto& [id, node] : m_nodes) {
        if (node.alive) count++;
    }
    return count;
}

// ============================================================================
// Named Pipelines
// ============================================================================
PatchResult DistributedPipelineOrchestrator::definePipeline(
    const std::string& name, const std::vector<uint64_t>& stageOrder) {
    std::lock_guard<std::mutex> lock(m_mutex);

    PipelineDefinition def;
    def.name = name;
    def.stageOrder = stageOrder;
    def.paused = false;
    m_pipelines[name] = def;

    return PatchResult::ok("Pipeline defined");
}

PatchResult DistributedPipelineOrchestrator::executePipeline(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_pipelines.find(name);
    if (it == m_pipelines.end()) {
        return PatchResult::error("Pipeline not found", -1);
    }

    // Wire up sequential dependencies
    auto& stages = it->second.stageOrder;
    for (size_t i = 1; i < stages.size(); i++) {
        auto prevIt = m_tasks.find(stages[i - 1]);
        auto currIt = m_tasks.find(stages[i]);
        if (prevIt == m_tasks.end() || currIt == m_tasks.end()) continue;

        currIt->second.dependencies.push_back(stages[i - 1]);
        prevIt->second.dependents.push_back(stages[i]);
    }

    // Enqueue first stage
    if (!stages.empty()) {
        auto firstIt = m_tasks.find(stages[0]);
        if (firstIt != m_tasks.end() && areDependenciesMet(stages[0])) {
            firstIt->second.state = StageState::Queued;
            m_readyQueue.push({firstIt->second.priority, stages[0]});
            m_workAvailable.notify_one();
        }
    }

    it->second.paused = false;
    return PatchResult::ok("Pipeline execution started");
}

PatchResult DistributedPipelineOrchestrator::pausePipeline(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pipelines.find(name);
    if (it == m_pipelines.end()) return PatchResult::error("Pipeline not found", -1);
    it->second.paused = true;
    return PatchResult::ok("Pipeline paused");
}

PatchResult DistributedPipelineOrchestrator::resumePipeline(const std::string& name) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_pipelines.find(name);
    if (it == m_pipelines.end()) return PatchResult::error("Pipeline not found", -1);
    it->second.paused = false;
    enqueueReadyTasks();
    m_workAvailable.notify_all();
    return PatchResult::ok("Pipeline resumed");
}

// ============================================================================
// Monitoring
// ============================================================================
void DistributedPipelineOrchestrator::resetStats() {
    m_stats.tasksSubmitted.store(0);
    m_stats.tasksCompleted.store(0);
    m_stats.tasksFailed.store(0);
    m_stats.tasksCancelled.store(0);
    m_stats.tasksTimedOut.store(0);
    m_stats.tasksRetried.store(0);
    m_stats.tasksStolen.store(0);
    m_stats.totalExecutionTimeUs.store(0);
    m_stats.totalBytesProcessed.store(0);
    m_stats.deadlineMisses.store(0);
    m_stats.peakQueueDepth.store(0);
}

StageState DistributedPipelineOrchestrator::getTaskState(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return StageState::Pending;
    return it->second.state;
}

TaskResult DistributedPipelineOrchestrator::getTaskResult(uint64_t taskId) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = m_results.find(taskId);
    if (it == m_results.end()) return TaskResult::error(taskId, "No result available");
    return it->second;
}

std::vector<uint64_t> DistributedPipelineOrchestrator::getPendingTasks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint64_t> result;
    for (auto& [id, task] : m_tasks) {
        if (task.state == StageState::Pending || task.state == StageState::Queued) {
            result.push_back(id);
        }
    }
    return result;
}

std::vector<uint64_t> DistributedPipelineOrchestrator::getRunningTasks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<uint64_t> result;
    for (auto& [id, task] : m_tasks) {
        if (task.state == StageState::Running) {
            result.push_back(id);
        }
    }
    return result;
}

size_t DistributedPipelineOrchestrator::totalQueueDepth() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t depth = m_readyQueue.size();
    for (auto& deque : m_workerDeques) {
        depth += deque->size();
    }
    return depth;
}

// ============================================================================
// Callbacks
// ============================================================================
void DistributedPipelineOrchestrator::setCompletionCallback(TaskCompletionCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_completionCb   = cb;
    m_completionData = userData;
}

void DistributedPipelineOrchestrator::setEventCallback(PipelineEventCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_eventCb   = cb;
    m_eventData = userData;
}

void DistributedPipelineOrchestrator::setNodeFailureCallback(NodeFailureCallback cb, void* userData) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_nodeFailCb   = cb;
    m_nodeFailData = userData;
}

// ============================================================================
// Configuration
// ============================================================================
void DistributedPipelineOrchestrator::setMaxQueueDepth(size_t depth) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxQueueDepth = depth;
}

void DistributedPipelineOrchestrator::setStealingEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_stealingEnabled = enabled;
}

void DistributedPipelineOrchestrator::setHeartbeatTimeoutMs(int64_t ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_heartbeatTimeout = ms;
}

void DistributedPipelineOrchestrator::setDefaultTimeoutMs(int64_t ms) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_defaultTimeout = ms;
}

// ============================================================================
// Worker Thread Loop
// ============================================================================
void DistributedPipelineOrchestrator::workerLoop(uint32_t workerId) {
    while (m_running.load()) {
        bool executed = tryExecuteTask(workerId);

        if (!executed) {
            // Try work-stealing
            uint64_t stolenTask = 0;
            if (m_stealingEnabled && tryStealWork(workerId, stolenTask)) {
                m_stats.tasksStolen.fetch_add(1);
                executeTask(stolenTask, workerId);
            } else {
                // Wait for work
                std::unique_lock<std::mutex> lock(m_mutex);
                m_workAvailable.wait_for(lock, std::chrono::milliseconds(50),
                    [this]() { return !m_running.load() || !m_readyQueue.empty(); });
            }
        }
    }
}

bool DistributedPipelineOrchestrator::tryExecuteTask(uint32_t workerId) {
    uint64_t taskId = 0;

    // Try local deque first (LIFO — cache-friendly)
    if (workerId < m_workerDeques.size() && m_workerDeques[workerId]->pop(taskId)) {
        executeTask(taskId, workerId);
        return true;
    }

    // Try global ready queue
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_readyQueue.empty()) return false;

        auto [prio, id] = m_readyQueue.top();
        m_readyQueue.pop();
        taskId = id;
    }

    executeTask(taskId, workerId);
    return true;
}

bool DistributedPipelineOrchestrator::tryStealWork(uint32_t workerId, uint64_t& taskId) {
    // Round-robin steal from other workers
    for (uint32_t i = 1; i < m_numWorkers; i++) {
        uint32_t victim = (workerId + i) % m_numWorkers;
        if (victim < m_workerDeques.size() && m_workerDeques[victim]->steal(taskId)) {
            return true;
        }
    }
    return false;
}

void DistributedPipelineOrchestrator::executeTask(uint64_t taskId, uint32_t workerId) {
    PipelineTask* task = nullptr;

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        auto it = m_tasks.find(taskId);
        if (it == m_tasks.end()) return;

        task = &it->second;
        if (task->state == StageState::Cancelled) return;

        task->state = StageState::Running;
        task->startTime = std::chrono::steady_clock::now();
    }

    if (m_eventCb) {
        m_eventCb(taskId, StageState::Running, m_eventData);
    }

    // Execute the task
    void* output = nullptr;
    size_t outSize = 0;
    bool success = false;

    if (task->execute) {
        success = task->execute(task->taskData, &output, &outSize);
    }

    auto endTime = std::chrono::steady_clock::now();
    int64_t durationUs = std::chrono::duration_cast<std::chrono::microseconds>(
        endTime - task->startTime).count();

    if (success) {
        TaskResult result = TaskResult::ok(taskId, durationUs, outSize);
        handleTaskCompletion(taskId, result);
    } else {
        handleTaskFailure(taskId, "Task execution failed", -1);
    }
}

void DistributedPipelineOrchestrator::handleTaskCompletion(uint64_t taskId, const TaskResult& result) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return;

    it->second.state   = StageState::Completed;
    it->second.endTime = std::chrono::steady_clock::now();
    m_results[taskId]  = result;

    m_stats.tasksCompleted.fetch_add(1);
    m_stats.totalExecutionTimeUs.fetch_add(result.executionTimeUs);
    m_stats.totalBytesProcessed.fetch_add(result.bytesProcessed);

    // Check deadline
    if (it->second.deadlineMs > 0) {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            it->second.endTime - it->second.submitTime).count();
        if (elapsed > it->second.deadlineMs) {
            m_stats.deadlineMisses.fetch_add(1);
        }
    }

    // Notify callback
    if (m_completionCb) {
        m_completionCb(result, m_completionData);
    }
    if (m_eventCb) {
        m_eventCb(taskId, StageState::Completed, m_eventData);
    }

    // Enqueue dependents whose dependencies are now met
    for (auto depId : it->second.dependents) {
        if (areDependenciesMet(depId)) {
            auto depIt = m_tasks.find(depId);
            if (depIt != m_tasks.end() && depIt->second.state == StageState::Pending) {
                depIt->second.state = StageState::Queued;
                m_readyQueue.push({depIt->second.priority, depId});
            }
        }
    }

    m_workAvailable.notify_all();
}

void DistributedPipelineOrchestrator::handleTaskFailure(uint64_t taskId,
                                                         const char* reason, int code) {
    std::lock_guard<std::mutex> lock(m_mutex);

    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return;

    auto& task = it->second;

    // Retry logic with exponential backoff
    if (task.retryCount < task.maxRetries) {
        task.retryCount++;
        task.state = StageState::Retrying;
        m_stats.tasksRetried.fetch_add(1);

        int64_t backoff = computeBackoff(task.retryCount, task.retryBackoffBaseMs);
        // Re-enqueue after backoff (simplified — immediate re-enqueue)
        task.state = StageState::Queued;
        m_readyQueue.push({task.priority, taskId});
        m_workAvailable.notify_one();

        if (m_eventCb) {
            m_eventCb(taskId, StageState::Retrying, m_eventData);
        }
        return;
    }

    // Max retries exhausted
    task.state   = StageState::Failed;
    task.endTime = std::chrono::steady_clock::now();
    m_stats.tasksFailed.fetch_add(1);

    TaskResult failResult = TaskResult::error(taskId, reason, code);
    m_results[taskId] = failResult;

    if (m_completionCb) {
        m_completionCb(failResult, m_completionData);
    }
    if (m_eventCb) {
        m_eventCb(taskId, StageState::Failed, m_eventData);
    }
}

void DistributedPipelineOrchestrator::scheduleRetry(uint64_t taskId) {
    // Called with lock held
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return;

    it->second.state = StageState::Queued;
    m_readyQueue.push({it->second.priority, taskId});
    m_workAvailable.notify_one();
}

// ============================================================================
// Scheduling Helpers
// ============================================================================
uint32_t DistributedPipelineOrchestrator::selectBestNode(const PipelineTask& task) const {
    // Lock already held by caller
    uint32_t bestNode = UINT32_MAX;
    double bestScore = 999.0;

    for (auto& [id, node] : m_nodes) {
        if (!node.alive) continue;

        // Check affinity
        if (task.affinity == NodeAffinity::GPURequired && !node.hasGPU) continue;
        if (task.affinity == NodeAffinity::CPUOnly && node.hasGPU) continue; // Prefer non-GPU

        // Check resources
        if (node.availableCores < task.requiredCores) continue;
        if (node.availableMemory < task.requiredMemoryBytes) continue;

        double score = node.utilizationScore();

        // Prefer GPU nodes for GPU-preferred tasks
        if (task.affinity == NodeAffinity::GPUPreferred && node.hasGPU) {
            score -= 0.5;
        }

        if (score < bestScore) {
            bestScore = score;
            bestNode  = id;
        }
    }

    return bestNode;
}

void DistributedPipelineOrchestrator::enqueueReadyTasks() {
    // Called with lock held
    for (auto& [id, task] : m_tasks) {
        if (task.state == StageState::Pending && areDependenciesMet(id)) {
            task.state = StageState::Queued;
            m_readyQueue.push({task.priority, id});
        }
    }
}

bool DistributedPipelineOrchestrator::areDependenciesMet(uint64_t taskId) const {
    // Called with lock held
    auto it = m_tasks.find(taskId);
    if (it == m_tasks.end()) return false;

    for (auto depId : it->second.dependencies) {
        auto depIt = m_tasks.find(depId);
        if (depIt == m_tasks.end()) continue; // Missing dep — treat as met
        if (depIt->second.state != StageState::Completed) return false;
    }
    return true;
}

int64_t DistributedPipelineOrchestrator::computeBackoff(int retryCount, int64_t baseMs) const {
    // Exponential backoff with jitter: base * 2^retry + random(0, base)
    int64_t delay = baseMs * (1LL << retryCount);
    // Cap at 60 seconds
    if (delay > 60000) delay = 60000;
    return delay;
}

// ============================================================================
// Cycle Detection (Kahn's Algorithm)
// ============================================================================
bool DistributedPipelineOrchestrator::detectCycle() const {
    // Called with lock held
    std::unordered_map<uint64_t, uint32_t> inDegree;

    for (auto& [id, task] : m_tasks) {
        if (inDegree.find(id) == inDegree.end()) inDegree[id] = 0;
        for (auto depId : task.dependents) {
            inDegree[depId]++;
        }
    }

    std::queue<uint64_t> zeroIn;
    for (auto& [id, deg] : inDegree) {
        if (deg == 0) zeroIn.push(id);
    }

    size_t visited = 0;
    while (!zeroIn.empty()) {
        uint64_t current = zeroIn.front();
        zeroIn.pop();
        visited++;

        auto it = m_tasks.find(current);
        if (it != m_tasks.end()) {
            for (auto depId : it->second.dependents) {
                inDegree[depId]--;
                if (inDegree[depId] == 0) {
                    zeroIn.push(depId);
                }
            }
        }
    }

    return visited != m_tasks.size(); // Cycle if not all visited
}

// ============================================================================
// Heartbeat Monitor
// ============================================================================
void DistributedPipelineOrchestrator::heartbeatMonitorLoop() {
    while (m_running.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5000));

        if (!m_running.load()) break;

        std::lock_guard<std::mutex> lock(m_mutex);
        auto now = std::chrono::steady_clock::now();

        for (auto& [id, node] : m_nodes) {
            if (!node.alive) continue;

            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - node.lastHeartbeat).count();

            if (elapsed > m_heartbeatTimeout) {
                node.alive = false;
                if (m_nodeFailCb) {
                    m_nodeFailCb(id, "Heartbeat timeout", m_nodeFailData);
                }
            }
        }
    }
}
