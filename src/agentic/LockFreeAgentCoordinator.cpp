/**
 * @file LockFreeAgentCoordinator.cpp
 * @brief Lock-Free Agent Coordinator Implementation
 * 
 * Implements the atomic dependency counter pattern for zero-stall
 * task coordination in high-throughput environments.
 */

#include "LockFreeAgentCoordinator.h"
#include <algorithm>
#include <random>
#include <chrono>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// Singleton Instance
// ============================================================================

LockFreeAgentCoordinator& LockFreeAgentCoordinator::instance() {
    static LockFreeAgentCoordinator inst;
    return inst;
}

LockFreeAgentCoordinator::~LockFreeAgentCoordinator() {
    shutdown();
}

// ============================================================================
// Initialization
// ============================================================================

bool LockFreeAgentCoordinator::initialize(int workerThreads) {
    if (m_running.exchange(true)) {
        return false; // Already initialized
    }
    
    // Start worker threads
    for (int i = 0; i < workerThreads; ++i) {
        m_workers.emplace_back(&LockFreeAgentCoordinator::workerLoop, this, i);
    }
    
    return true;
}

void LockFreeAgentCoordinator::shutdown() {
    if (!m_running.exchange(false)) {
        return; // Already shut down
    }
    
    // Wait for all workers to finish
    for (auto& worker : m_workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    m_workers.clear();
}

// ============================================================================
// Task Submission (Lock-Free)
// ============================================================================

TaskNode* LockFreeAgentCoordinator::submitTask(
    const std::string& id,
    const std::string& description,
    const std::string& specialization,
    const nlohmann::json& params,
    const std::vector<std::string>& dependencies,
    std::function<void(const nlohmann::json&)> callback) {
    
    // Create task node
    auto node = std::make_unique<TaskNode>(
        id, description, specialization, 
        static_cast<int32_t>(dependencies.size()));
    
    node->parameters = params;
    node->callback = callback;
    
    TaskNode* rawPtr = node.get();
    
    // Register task (brief lock only for registration)
    {
        std::unique_lock<std::shared_mutex> lock(m_taskMutex);
        m_tasks[id] = std::move(node);
    }
    
    // If no dependencies, immediately make ready (lock-free)
    if (dependencies.empty()) {
        if (rawPtr->tryMarkReady()) {
            m_taskQueue.enqueue(rawPtr);
        }
    }
    // Otherwise, dependencies will call onParentComplete() when they finish
    
    return rawPtr;
}

// ============================================================================
// Dependency Management
// ============================================================================

bool LockFreeAgentCoordinator::addDependency(TaskNode* parent, TaskNode* child) {
    if (!parent || !child) return false;
    
    // Add bidirectional edges
    parent->children.push_back(child);
    child->parents.push_back(parent);
    
    // Increment child's dependency count
    child->dependencyCount.fetch_add(1, std::memory_order_acq_rel);
    
    return true;
}

// ============================================================================
// Agent Registration
// ============================================================================

void LockFreeAgentCoordinator::registerAgent(
    const std::string& agentId,
    const std::vector<std::string>& specializations,
    int maxConcurrent) {
    
    auto state = std::make_unique<AgentState>();
    state->specializations = specializations;
    state->maxConcurrentTasks = maxConcurrent;
    state->updateActivity();
    
    std::unique_lock<std::shared_mutex> lock(m_agentMutex);
    m_agents[agentId] = std::move(state);
}

// ============================================================================
// Agent Selection (Lock-Free, RCU-Style)
// ============================================================================

std::string LockFreeAgentCoordinator::selectOptimalAgent(
    const std::string& specialization) {
    
    std::shared_lock<std::shared_mutex> lock(m_agentMutex);
    
    std::string bestAgent;
    double bestScore = -1.0;
    
    for (const auto& [agentId, state] : m_agents) {
        // Check if agent is healthy
        if (!state->isHealthy.load(std::memory_order_acquire)) {
            continue;
        }
        
        // Check specialization match
        bool hasSpec = std::find(state->specializations.begin(),
                                 state->specializations.end(),
                                 specialization) != state->specializations.end();
        
        if (!hasSpec && !specialization.empty()) {
            continue; // Skip if specialization required but not matched
        }
        
        // Calculate score (lower load = higher score)
        double load = state->currentLoad.load(std::memory_order_relaxed);
        double active = state->activeTasks.load(std::memory_order_relaxed);
        double maxConcurrent = state->maxConcurrentTasks;
        
        // Score based on available capacity
        double capacityScore = 1.0 - (active / maxConcurrent);
        double loadScore = 1.0 - load;
        
        // Specialization bonus
        double specBonus = hasSpec ? 0.3 : 0.0;
        
        double totalScore = (capacityScore * 0.5) + (loadScore * 0.3) + specBonus;
        
        if (totalScore > bestScore) {
            bestScore = totalScore;
            bestAgent = agentId;
        }
    }
    
    return bestAgent;
}

// ============================================================================
// Worker Thread
// ============================================================================

void LockFreeAgentCoordinator::workerLoop(int threadId) {
    (void)threadId; // May be used for NUMA affinity
    
    TaskNode* task = nullptr;
    
    while (m_running.load(std::memory_order_acquire)) {
        // Try to get a ready task (lock-free)
        if (m_taskQueue.dequeue(task)) {
            // Select optimal agent
            std::string agentId = selectOptimalAgent(task->specialization);
            
            if (agentId.empty()) {
                // No agent available, re-queue
                m_taskQueue.enqueue(task);
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }
            
            // Try to mark as running
            if (!task->tryMarkRunning()) {
                // Task state changed, skip
                continue;
            }
            
            // Execute task
            executeTask(task, agentId);
        } else {
            // No tasks available, brief sleep
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

void LockFreeAgentCoordinator::executeTask(TaskNode* task, 
                                            const std::string& agentId) {
    // Update agent state
    AgentState* agent = nullptr;
    {
        std::shared_lock<std::shared_mutex> lock(m_agentMutex);
        auto it = m_agents.find(agentId);
        if (it != m_agents.end()) {
            agent = it->second.get();
        }
    }
    
    if (agent) {
        agent->recordTaskStart();
    }
    
    // Execute task
    bool success = true;
    try {
        if (task->callback) {
            task->callback(task->parameters);
        }
    } catch (...) {
        success = false;
    }
    
    // Mark complete
    task->markCompleted();
    
    // Update agent state
    if (agent) {
        agent->recordTaskComplete(success);
    }
    
    // Update statistics
    if (success) {
        m_completedTasks.fetch_add(1, std::memory_order_relaxed);
    } else {
        m_failedTasks.fetch_add(1, std::memory_order_relaxed);
    }
    
    // Children are notified via markCompleted() -> onParentComplete()
    // If any child reaches dependencyCount == 0, it becomes ready
    for (auto* child : task->children) {
        if (child->onParentComplete()) {
            if (child->tryMarkReady()) {
                m_taskQueue.enqueue(child);
            }
        }
    }
}

// ============================================================================
// Health Monitoring
// ============================================================================

void LockFreeAgentCoordinator::updateAgentHealth(const std::string& agentId, 
                                                bool healthy) {
    std::shared_lock<std::shared_mutex> lock(m_agentMutex);
    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) {
        it->second->isHealthy.store(healthy, std::memory_order_release);
    }
}

bool LockFreeAgentCoordinator::isAgentHealthy(const std::string& agentId) const {
    std::shared_lock<std::shared_mutex> lock(m_agentMutex);
    auto it = m_agents.find(agentId);
    if (it != m_agents.end()) {
        return it->second->isHealthy.load(std::memory_order_acquire);
    }
    return false;
}

// ============================================================================
// Metrics
// ============================================================================

LockFreeAgentCoordinator::Metrics LockFreeAgentCoordinator::getMetrics() const {
    Metrics metrics{};
    
    // Task counts
    {
        std::shared_lock<std::shared_mutex> lock(m_taskMutex);
        for (const auto& [id, task] : m_tasks) {
            TaskState state = task->state.load(std::memory_order_acquire);
            switch (state) {
                case TaskState::PENDING: metrics.pendingTasks++; break;
                case TaskState::READY: metrics.readyTasks++; break;
                case TaskState::RUNNING: metrics.runningTasks++; break;
                case TaskState::COMPLETED: metrics.completedTasks++; break;
                case TaskState::FAILED: metrics.failedTasks++; break;
            }
        }
    }
    
    // Agent metrics
    {
        std::shared_lock<std::shared_mutex> lock(m_agentMutex);
        double totalLoad = 0.0;
        for (const auto& [id, state] : m_agents) {
            totalLoad += state->currentLoad.load(std::memory_order_relaxed);
            if (state->isHealthy.load(std::memory_order_acquire)) {
                metrics.healthyAgents++;
            }
        }
        if (!m_agents.empty()) {
            metrics.averageLoad = totalLoad / m_agents.size();
        }
    }
    
    return metrics;
}

// ============================================================================
// C API Implementation
// ============================================================================

extern "C" {

void* LockFreeCoordinator_Create() {
    auto* coord = new LockFreeAgentCoordinator();
    if (!coord->initialize()) {
        delete coord;
        return nullptr;
    }
    return coord;
}

void LockFreeCoordinator_Destroy(void* coordinator) {
    if (coordinator) {
        auto* coord = static_cast<LockFreeAgentCoordinator*>(coordinator);
        coord->shutdown();
        delete coord;
    }
}

void* LockFreeCoordinator_SubmitTask(
    void* coordinator,
    const char* id,
    const char* description,
    const char* specialization,
    const char* paramsJson,
    const char** dependencies,
    int depCount) {
    
    if (!coordinator || !id || !description) return nullptr;
    
    auto* coord = static_cast<LockFreeAgentCoordinator*>(coordinator);
    
    nlohmann::json params;
    if (paramsJson) {
        try {
            params = nlohmann::json::parse(paramsJson);
        } catch (...) {
            params = nlohmann::json::object();
        }
    }
    
    std::vector<std::string> deps;
    if (dependencies && depCount > 0) {
        for (int i = 0; i < depCount; ++i) {
            if (dependencies[i]) {
                deps.push_back(dependencies[i]);
            }
        }
    }
    
    return coord->submitTask(id, description, 
                            specialization ? specialization : "",
                            params, deps, nullptr);
}

bool LockFreeCoordinator_AddDependency(void* coordinator, void* parent, void* child) {
    if (!coordinator || !parent || !child) return false;
    auto* coord = static_cast<LockFreeAgentCoordinator*>(coordinator);
    return coord->addDependency(static_cast<TaskNode*>(parent),
                                static_cast<TaskNode*>(child));
}

void LockFreeCoordinator_RegisterAgent(
    void* coordinator,
    const char* agentId,
    const char** specializations,
    int specCount,
    int maxConcurrent) {
    
    if (!coordinator || !agentId) return;
    
    auto* coord = static_cast<LockFreeAgentCoordinator*>(coordinator);
    
    std::vector<std::string> specs;
    if (specializations && specCount > 0) {
        for (int i = 0; i < specCount; ++i) {
            if (specializations[i]) {
                specs.push_back(specializations[i]);
            }
        }
    }
    
    coord->registerAgent(agentId, specs, maxConcurrent);
}

const char* LockFreeCoordinator_SelectAgent(void* coordinator, 
                                           const char* specialization) {
    if (!coordinator) return nullptr;
    
    auto* coord = static_cast<LockFreeAgentCoordinator*>(coordinator);
    std::string agentId = coord->selectOptimalAgent(specialization ? specialization : "");
    
    // Return persistent string (caller must not free)
    static thread_local std::string result;
    result = agentId;
    return result.c_str();
}

} // extern "C"

} // namespace Agentic
} // namespace RawrXD
