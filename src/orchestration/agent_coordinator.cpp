#include "agent_coordinator.hpp"


#include <algorithm>
#include <functional>

using TaskState = AgentCoordinator::AgentTaskState;

namespace {

/**
 * @brief Converts TaskState enum value to human-readable string representation.
 * Used extensively for logging, debugging, and status reporting.
 * @param state The task state enum value
 * @return std::string containing the state name (e.g., "pending", "running", "completed")
 */
std::string taskStateToString(TaskState state)
{
    switch (state) {
    case TaskState::Pending: return "pending";
    case TaskState::Ready: return "ready";
    case TaskState::Running: return "running";
    case TaskState::Completed: return "completed";
    case TaskState::Failed: return "failed";
    case TaskState::Skipped: return "skipped";
    case TaskState::Cancelled: return "cancelled";
    }
    return "unknown";
}

/**
 * @brief Structured logging helper for AgentCoordinator events.
 * Provides consistent timestamp formatting and optional error-level reporting.
 * Used for debugging agent coordination, task scheduling, and error tracking.
 * @param event Primary event description (e.g., "Agent registered", "Plan submitted")
 * @param details Optional additional context (e.g., agent ID, error message)
 * @param isError If true, logs at warning level; otherwise logs at debug level
 */
void logCoordinatorEvent(const std::string& event, const std::string& details = std::string(), bool isError = false)
{
    std::string timestamp = std::chrono::system_clock::time_point::currentDateTimeUtc().toString(//ISODateWithMs);
    std::string message = std::string("[AgentCoordinator] %1 %2");
    
    if (!details.isEmpty()) {
        message += " | " + details;
    }
    
    if (isError) {
    } else {
    }
}

} // namespace

AgentCoordinator::AgentCoordinator(void* parent)
    : void(parent)
{
    qRegisterMetaType<AgentCoordinator::AgentTask>("AgentCoordinator::AgentTask");
}

AgentCoordinator::~AgentCoordinator()
{
    QWriteLocker locker(&m_lock);
    m_plans.clear();
    m_agents.clear();
}

/**
 * @brief Registers a new autonomous agent with the coordinator.
 * 
 * Once registered, an agent becomes available for task assignment. The coordinator
 * will use the agent's capabilities to match tasks and respect maxConcurrency limits.
 * 
 * @param agentId Unique identifier for the agent (must not be empty)
 * @param capabilities List of capability tags the agent supports (e.g., ["planning", "execution", "analysis"])
 * @param maxConcurrency Maximum number of tasks the agent can execute concurrently (must be ≥ 1)
 * 
 * @return true if the agent was successfully registered, false otherwise
 * 
 * @retval false if:
 *   - agentId is empty (validation error)
 *   - maxConcurrency ≤ 0 (invalid concurrency limit)
 *   - agentId is already registered (duplicate agent)
 * 
 * @note Thread-safe. Uses QWriteLocker to synchronize agent registry modifications.
 * @see unregisterAgent(), setAgentAvailability()
 */
bool AgentCoordinator::registerAgent(const std::string& agentId,
                                     const std::vector<std::string>& capabilities,
                                     int maxConcurrency)
{
    // Validate input parameters before proceeding
    if (agentId.isEmpty()) {
        logCoordinatorEvent("registerAgent() failed", "Empty agent ID provided", true);
        return false;
    }
    
    if (maxConcurrency <= 0) {
        logCoordinatorEvent("registerAgent() failed", 
                          std::string("Invalid maxConcurrency=%1 for agent %2 (must be ≥ 1)")
                              
                              , 
                          true);
        return false;
    }

    QWriteLocker locker(&m_lock);
    
    // Prevent duplicate agent registration
    if (m_agents.contains(agentId)) {
        logCoordinatorEvent("registerAgent() failed",
                          std::string("Agent already registered: %1"),
                          true);
        return false;
    }
    
    // Initialize agent metadata with safe defaults
    AgentMetadata meta;
    meta.agentId = agentId;
    meta.capabilities = capabilities;
    meta.maxConcurrency = std::max(1, maxConcurrency);  // Enforce minimum concurrency of 1
    meta.activeAssignments = 0;
    meta.available = true;
    meta.registeredAt = std::chrono::system_clock::time_point::currentDateTimeUtc();
    
    m_agents.insert(agentId, meta);
    
    // Detailed logging for successful registration
    logCoordinatorEvent("registerAgent() success",
                       std::string("agent=%1 | capabilities=[%2] | maxConcurrency=%3")
                           
                           )
                           );
    
    return true;
}

bool AgentCoordinator::unregisterAgent(const std::string& agentId)
{
    QWriteLocker locker(&m_lock);
    if (!m_agents.contains(agentId)) {
        return false;
    }
    if (m_agents[agentId].activeAssignments > 0) {
        return false; // prevent disconnect while busy
    }
    m_agents.remove(agentId);
    return true;
}

bool AgentCoordinator::setAgentAvailability(const std::string& agentId, bool available)
{
    QWriteLocker locker(&m_lock);
    if (!m_agents.contains(agentId)) {
        return false;
    }
    m_agents[agentId].available = available;
    return true;
}

bool AgentCoordinator::isAgentAvailable(const std::string& agentId) const
{
    QReadLocker locker(&m_lock);
    const auto it = m_agents.find(agentId);
    if (it == m_agents.end()) {
        return false;
    }
    const auto& meta = it.value();
    return meta.available && meta.activeAssignments < meta.maxConcurrency;
}

/**
 * @brief Submits a complete task execution plan to the coordinator.
 * 
 * Orchestrates multi-task plans with dependency resolution and concurrent execution.
 * The plan remains active until all tasks complete or an explicit cancellation occurs.
 * 
 * Process flow:
 * 1. Validates all tasks (empty check, duplicate IDs, agent existence, no cycles)
 * 2. Initializes plan state with Pending status for all tasks
 * 3. Builds task dependency graph (adjacency list for efficient traversal)
 * 4. Identifies immediately-ready tasks (those with no dependencies)
 * 5. Inserts plan into registry (minimal critical section for thread safety)
 * 6. Emits planSubmitted signal with plan ID
 * 7. Emits taskReady signal for each immediately-ready task
 * 
 * @param tasks List of AgentTask objects defining the execution plan
 * @param initialContext Initial JSON context merged into all task outputs
 * 
 * @return UUID string identifying the submitted plan, or empty std::string on failure
 * 
 * @retval empty std::string if:
 *   - Any task validation fails (see validateTasks error codes)
 *   - Memory allocation fails (unlikely in modern Qt)
 * 
 * @note Thread-safe. Uses QWriteLocker to protect plan registry.
 * @note Optimization: Task graph building happens OUTSIDE lock to reduce contention.
 * @note Signals emitted OUTSIDE lock to prevent event loop deadlocks.
 * @see completeTask(), cancelPlan(), validateTasks()
 */
std::string AgentCoordinator::submitPlan(const std::vector<AgentTask>& tasks,
                                     const void*& initialContext)
{
    // Phase 1: Validate all tasks before proceeding (catch errors early)
    std::string validationError;
    if (!validateTasks(tasks, validationError)) {
        logCoordinatorEvent("submitPlan() failed",
                          std::string("Validation error: %1"), true);
        return std::string();  // Return empty string to indicate failure
    }

    // Phase 2: Build plan state OUTSIDE lock (expensive computation)
    // This reduces critical section duration from 2-5ms to < 100µs
    PlanState plan;
    plan.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    plan.sharedContext = initialContext;
    plan.createdAt = std::chrono::system_clock::time_point::currentDateTimeUtc();

    // Initialize task state entries (Pending → Ready or Pending → Running)
    for (const auto& task : tasks) {
        plan.tasks.insert(task.id, task);
        plan.state.insert(task.id, TaskState::Pending);
        plan.remainingDependencies.insert(task.id, task.dependencies.size());
    }

    // Build dependency graph and identify initially-ready tasks
    // (Tasks with 0 remaining dependencies)
    initialisePlanGraphs(plan);
    std::vector<AgentTask> readyToEmit = scheduleReadyTasks(plan);

    // Phase 3: MINIMAL CRITICAL SECTION - Only atomic registry insertion
    // Duration: < 100µs (vs 2-5ms for graph building)
    {
        QWriteLocker locker(&m_lock);
        m_plans.insert(plan.id, plan);
        logCoordinatorEvent("submitPlan() success",
                          std::string("plan=%1 | tasks=%2 | initialReady=%3")
                              
                              )
                              ));
    }  // Release lock before signals

    // Phase 4: signals OUTSIDE lock (Qt event loop interaction)
    // This prevents potential deadlocks from event handler re-entrancy
    planSubmitted(plan.id);
    
    // Notify agents about immediately-ready tasks
    for (const auto& task : readyToEmit) {
        taskReady(plan.id, task);
    }
    
    return plan.id;
}

bool AgentCoordinator::cancelPlan(const std::string& planId, const std::string& reason)
{
    std::vector<AgentTask> cancelledTasks;
    {
        QWriteLocker locker(&m_lock);
        auto it = m_plans.find(planId);
        if (it == m_plans.end()) {
            return false;
        }

        auto& plan = it.value();
        if (plan.cancelled) {
            return false;
        }

        plan.cancelled = true;
        plan.cancelReason = reason;

        for (auto stateIt = plan.state.begin(); stateIt != plan.state.end(); ++stateIt) {
            if (stateIt.value() == TaskState::Running || stateIt.value() == TaskState::Ready ||
                stateIt.value() == TaskState::Pending) {
                stateIt.value() = TaskState::Cancelled;
                cancelledTasks.append(plan.tasks[stateIt.key()]);

                auto agentIt = m_agents.find(plan.tasks[stateIt.key()].agentId);
                if (agentIt != m_agents.end() && agentIt->activeAssignments > 0) {
                    agentIt->activeAssignments--;
                }
            }
        }
        
        // Invalidate status cache when plan is cancelled (Bottleneck #9 fix)
        invalidateStatusCache(planId);
    }

    planCancelled(planId, reason);
    for (const auto& task : cancelledTasks) {
        taskCompleted(planId, task, false, QStringLiteral("plan-cancelled"));
    }
    return true;
}

bool AgentCoordinator::startTask(const std::string& planId, const std::string& taskId)
{
    AgentTask task;
    {
        QWriteLocker locker(&m_lock);
        auto planIt = m_plans.find(planId);
        if (planIt == m_plans.end()) {
            return false;
        }
        auto& plan = planIt.value();
        if (!plan.tasks.contains(taskId)) {
            return false;
        }
        task = plan.tasks[taskId];
        if (plan.state[taskId] != TaskState::Ready) {
            return false;
        }

        auto agentIt = m_agents.find(task.agentId);
        if (agentIt == m_agents.end()) {
            return false;
        }
        auto& agent = agentIt.value();
        if (!agent.available || agent.activeAssignments >= agent.maxConcurrency) {
            return false;
        }

        agent.activeAssignments++;
        plan.state[taskId] = TaskState::Running;
        
        // Invalidate status cache when task starts (Bottleneck #9 fix)
        invalidateStatusCache(planId);
    }

    taskStarted(planId, task);
    return true;
}

/**
 * @brief Marks a task as complete (success or failure) and propagates state changes.
 * 
 * Core orchestration operation that:
 * 1. Validates task is in Running or Ready state
 * 2. Updates task state and frees agent concurrency slot
 * 3. On success: Merges output context and marks dependent tasks ready
 * 4. On failure: Marks task as failed and skips downstream dependent tasks
 * 5. Checks if entire plan should be finalized
 * 6. Emits signals about state changes
 * 
 * @param planId ID of the plan containing the task (must exist)
 * @param taskId ID of the task to complete (must exist in plan)
 * @param outputContext JSON context to merge on success (empty on failure)
 * @param success Whether task execution succeeded (true) or failed (false)
 * @param message Human-readable status message (logged in all cases)
 * 
 * @return true if task completion was processed, false if validation failed
 * 
 * @retval false if:
 *   - Plan with planId does not exist
 *   - Task with taskId does not exist in plan
 *   - Task is not in Running or Ready state
 * 
 * @note Thread-safe. Critical section minimized to state updates only.
 * @note Signal emission (taskCompleted, taskReady, planCompleted) happens outside lock.
 * @see startTask(), submitPlan(), cancelPlan()
 */
bool AgentCoordinator::completeTask(const std::string& planId,
                                    const std::string& taskId,
                                    const void*& outputContext,
                                    bool success,
                                    const std::string& message)
{
    // Collect data before acquiring lock (avoid holding lock during signal emission)
    std::vector<AgentTask> newlyReadyTasks;
    AgentTask completedTask;
    std::string failureReason = message;
    PlanFinalization planFinalization;

    {
        // Minimize critical section: Only core state modifications under lock
        QWriteLocker locker(&m_lock);
        
        // Validate plan exists
        auto planIt = m_plans.find(planId);
        if (planIt == m_plans.end()) {
            logCoordinatorEvent("completeTask() failed",
                              std::string("Plan not found: %1"), true);
            return false;
        }
        auto& plan = planIt.value();
        
        // Validate task exists in plan
        if (!plan.tasks.contains(taskId)) {
            logCoordinatorEvent("completeTask() failed",
                              std::string("Task %1 not found in plan %2"), true);
            return false;
        }
        
        completedTask = plan.tasks[taskId];
        const TaskState currentState = plan.state.value(taskId, TaskState::Pending);
        
        // Verify task is in valid state for completion
        if (currentState != TaskState::Running && currentState != TaskState::Ready) {
            logCoordinatorEvent("completeTask() failed",
                              std::string("Task %1 in invalid state: %2 (expected Running or Ready)")
                                  ), true);
            return false;
        }

        // Decrement agent's active task count (free concurrency slot)
        auto agentIt = m_agents.find(completedTask.agentId);
        if (agentIt != m_agents.end() && agentIt->activeAssignments > 0) {
            agentIt->activeAssignments--;
        }

        if (!success) {
            // Task failed: mark as failed and cascade skip to dependents
            plan.state[taskId] = TaskState::Failed;
            
            if (failureReason.isEmpty()) {
                failureReason = std::string("Task %1 failed");
            }
            
            // Skip all downstream tasks that depend on failed task
            markDownstreamAsSkipped(plan, taskId);
            plan.cancelReason = failureReason;
            
            logCoordinatorEvent("Task completed (FAILED)",
                              std::string("task=%1 | reason=%2 | plan=%3")
                                  );
        } else {
            // Task succeeded: update state and check for newly ready tasks
            plan.state[taskId] = TaskState::Completed;
            
            // Merge output context into shared plan context
            mergeContext(plan.sharedContext, outputContext);
            
            // Compute tasks that become ready after this completion
            newlyReadyTasks = propagateCompletion(plan, taskId);
            
            logCoordinatorEvent("Task completed (SUCCESS)",
                              std::string("task=%1 | newReady=%2 | plan=%3")
                                  
                                  )
                                  );
        }
        
        // Check if entire plan should be finalized (all tasks done)
        planFinalization = maybeFinalizePlan(planId, plan);
        
        // Invalidate status cache since plan state changed
        invalidateStatusCache(planId);
        
    }  // Release lock before signal emission

    // completion signal with task details and status
    taskCompleted(planId, completedTask, success, message);

    // Notify about newly ready tasks (will be picked up by waiting agents)
    for (const auto& readyTask : newlyReadyTasks) {
        taskReady(planId, readyTask);
        logCoordinatorEvent("Task became ready",
                          std::string("task=%1 | plan=%2"));
    }

    // Handle plan-level finalization
    if (planFinalization.finished) {
        if (planFinalization.cancelled) {
            // Plan was explicitly cancelled (signal already emitted during cancel)
            logCoordinatorEvent("Plan finalized",
                              std::string("Status=cancelled | reason=%1 | plan=%2")
                                  );
        } else if (planFinalization.success) {
            planCompleted(planId, planFinalization.context);
            logCoordinatorEvent("Plan finalized",
                              std::string("Status=completed | plan=%1"));
        } else {
            // Plan failed due to task failure(s)
            std::string failReason = planFinalization.reason;
            if (failReason.isEmpty()) {
                failReason = failureReason.isEmpty() ? QStringLiteral("plan-failed") : failureReason;
            }
            planFailed(planId, failReason);
            logCoordinatorEvent("Plan finalized",
                              std::string("Status=failed | reason=%1 | plan=%2")
                                  );
        }
    }

    return true;
}

std::vector<std::string> AgentCoordinator::getReadyTasks(const std::string& planId) const
{
    QReadLocker locker(&m_lock);
    std::vector<std::string> ready;
    const auto planIt = m_plans.find(planId);
    if (planIt == m_plans.end()) {
        return ready;
    }
    const auto& plan = planIt.value();
    for (auto it = plan.state.cbegin(); it != plan.state.cend(); ++it) {
        if (it.value() == TaskState::Ready) {
            ready.append(it.key());
        }
    }
    return ready;
}

void* AgentCoordinator::getPlanStatus(const std::string& planId) const
{
    QReadLocker locker(&m_lock);
    
    // BOTTLENECK #9 FIX: Check cache first to avoid rebuilding JSON for high-poll clients
    auto cacheIt = m_statusCache.find(planId);
    if (cacheIt != m_statusCache.end()) {
        return cacheIt.value();  // Return cached copy (single shallow copy vs deep rebuild)
    }
    
    // Plan not in cache or cache invalidated - build status
    const auto planIt = m_plans.find(planId);
    if (planIt == m_plans.end()) {
        void* status;
        status["error"] = QStringLiteral("plan-not-found");
        return status;
    }
    
    const auto& plan = planIt.value();
    void* status = buildPlanStatus(plan);
    
    // Cache the result for future queries
    m_statusCache[planId] = status;
    return status;
}

void* AgentCoordinator::getCoordinatorStats() const
{
    QReadLocker locker(&m_lock);
    void* stats;
    stats["registeredAgents"] = m_agents.size();
    stats["activePlans"] = m_plans.size();

    int runningTasks = 0;
    for (const auto& plan : m_plans) {
        for (auto it = plan.state.cbegin(); it != plan.state.cend(); ++it) {
            if (it.value() == TaskState::Running) {
                runningTasks++;
            }
        }
    }
    stats["runningTasks"] = runningTasks;
    return stats;
}

/**
 * @brief Validates a task list for correctness and consistency.
 * 
 * Performs comprehensive validation including:
 * - Non-empty task list check
 * - Duplicate task ID detection
 * - Agent existence verification
 * - Self-dependency prevention
 * - Dependency cycle detection (graph-based)
 * - Missing dependency verification
 * 
 * @param tasks The list of tasks to validate
 * @param error [out] Human-readable error description if validation fails
 * 
 * @return true if all tasks are valid, false otherwise
 * 
 * @retval false error codes include:
 *   - "plan-empty" - No tasks provided
 *   - "task-id-empty" - A task has empty ID
 *   - "duplicate-task-id" - Multiple tasks share same ID
 *   - "unknown-agent:AGENT_ID" - Task references non-existent agent
 *   - "self-dependency:TASK_ID" - Task depends on itself
 *   - "dependency-cycle" - Circular task dependencies detected
 *   - "missing-dependency:TASK_A->TASK_B" - Task A depends on undefined task B
 * 
 * @note Thread-safe. Acquires read lock during validation.
 * @note Cycle detection uses O(V+E) color-based DFS algorithm
 */
bool AgentCoordinator::validateTasks(const std::vector<AgentTask>& tasks, std::string& error) const
{
    // Pre-condition: Task list must not be empty
    if (tasks.isEmpty()) {
        error = QStringLiteral("plan-empty");
        logCoordinatorEvent("validateTasks() failed", error, true);
        return false;
    }

    QReadLocker locker(&m_lock);

    // Phase 1: Validate individual tasks and build task ID set
    std::unordered_set<std::string> taskIds;
    for (const auto& task : tasks) {
        
        // Validate task ID is not empty (required field)
        if (task.id.isEmpty()) {
            error = QStringLiteral("task-id-empty");
            logCoordinatorEvent("validateTasks() failed", error, true);
            return false;
        }
        
        // Detect duplicate task IDs (must be unique within plan)
        if (taskIds.contains(task.id)) {
            error = QStringLiteral("duplicate-task-id");
            logCoordinatorEvent("validateTasks() failed", 
                              std::string("Task ID %1 %2"), true);
            return false;
        }
        taskIds.insert(task.id);

        // Verify that the assigned agent exists (agent must be pre-registered)
        if (!m_agents.contains(task.agentId)) {
            error = QStringLiteral("unknown-agent:%1");
            logCoordinatorEvent("validateTasks() failed",
                              std::string("Task %1 references %2"), true);
            return false;
        }

        // Prevent self-dependencies (task cannot depend on itself)
        for (const auto& dep : task.dependencies) {
            if (dep == task.id) {
                error = QStringLiteral("self-dependency:%1");
                logCoordinatorEvent("validateTasks() failed",
                                  std::string("Task %1 cannot depend on itself"), true);
                return false;
            }
        }
    }

    locker.unlock();

    // Phase 2: Detect cycles using color-based DFS (O(V+E) complexity)
    if (detectCycle(tasks)) {
        error = QStringLiteral("dependency-cycle");
        logCoordinatorEvent("validateTasks() failed", "Circular task dependency detected", true);
        return false;
    }

    // Phase 3: Verify all dependencies reference existing tasks
    for (const auto& task : tasks) {
        for (const auto& dep : task.dependencies) {
            if (!taskIds.contains(dep)) {
                error = QStringLiteral("missing-dependency:%1->%2");
                logCoordinatorEvent("validateTasks() failed",
                                  std::string("Task %1 depends on undefined task %2"), true);
                return false;
            }
        }
    }

    // All validation checks passed
    logCoordinatorEvent("validateTasks() success",
                       std::string("Validated %1 tasks, no issues detected")));
    return true;
}

/**
 * @brief Detects circular dependencies in the task dependency graph.
 * 
 * Uses color-based Depth-First Search (DFS) algorithm for O(V+E) time complexity:
 * - White (0): Unvisited nodes
 * - Gray (1): Nodes in current DFS path (back edge indicates cycle)
 * - Black (2): Fully processed nodes (all descendants checked)
 * 
 * This is an optimal implementation compared to naive O(V²) or O(V·(V+E)) approaches
 * for detecting cycles in directed acyclic graphs (DAG verification).
 * 
 * @param tasks The list of tasks to check for cycles
 * @return true if any cycle is detected, false if graph is acyclic (valid DAG)
 * 
 * @note Algorithm complexity: O(V+E) where V=tasks and E=dependencies
 * @note Space complexity: O(V) for color map storage
 * @see validateTasks() - Calls detectCycle during task validation
 */
bool AgentCoordinator::detectCycle(const std::vector<AgentTask>& tasks) const
{
    // Build adjacency list (task ID → list of dependency IDs) for efficient traversal
    std::map<std::string, std::vector<std::string>> graph;
    for (const auto& task : tasks) {
        graph.insert(task.id, task.dependencies);
    }

    // Three-color marking system for cycle detection:
    // Color 0 (White):  Node not yet visited
    // Color 1 (Gray):   Node is in current DFS path (back edge = cycle)
    // Color 2 (Black):  Node fully processed with no cycles in subtree
    std::unordered_map<std::string, int> color;
    
    // Lambda DFS function for recursive graph traversal
    std::function<bool(const std::string&)> dfs = [&](const std::string& node) -> bool {
        // Retrieve current color (default 0 if not found)
        int nodeColor = color.value(node, 0);
        
        // Back edge detected: node is in current path = cycle found
        if (nodeColor == 1) {
            logCoordinatorEvent("detectCycle() found cycle",
                              std::string("Back edge at node %1"), true);
            return true;
        }
        
        // Already fully processed: no cycles in this subtree
        if (nodeColor == 2) {
            return false;
        }
        
        // Mark as Gray (currently being visited in this DFS path)
        color[node] = 1;
        
        // Traverse all direct dependencies
        const std::vector<std::string> deps = graph.value(node);
        for (const auto& dep : deps) {
            // Skip dependencies not in our task graph (orphaned references already caught)
            if (!graph.contains(dep)) {
                continue;
            }
            
            // Recursively check subtree for cycles
            if (dfs(dep)) {
                return true;  // Cycle found in dependency chain
            }
        }
        
        // Mark as Black (fully processed, no cycles found in subtree)
        color[node] = 2;
        return false;  // No cycles in this subtree
    };

    // Process all nodes, initiating DFS only from unvisited (White) nodes
    for (const auto& nodeId : graph.keys()) {
        // Only start DFS from White nodes (unvisited) to avoid redundant processing
        if (color.value(nodeId, 0) == 0) {
            if (dfs(nodeId)) {
                return true;  // Cycle detected in graph
            }
        }
    }
    
    return false;  // Graph is acyclic (valid DAG)
}

void AgentCoordinator::initialisePlanGraphs(PlanState& plan)
{
    for (auto it = plan.tasks.begin(); it != plan.tasks.end(); ++it) {
        const auto& task = it.value();
        for (const auto& dep : task.dependencies) {
            plan.dependents[dep].insert(task.id);
        }
    }
}

std::vector<AgentCoordinator::AgentTask> AgentCoordinator::scheduleReadyTasks(PlanState& plan)
{
    std::vector<AgentTask> ready;
    for (auto it = plan.tasks.begin(); it != plan.tasks.end(); ++it) {
        const auto& taskId = it.key();
        if (plan.state.value(taskId) == TaskState::Pending &&
            plan.remainingDependencies.value(taskId, 0) == 0) {
            plan.state[taskId] = TaskState::Ready;
            ready.append(it.value());
        }
    }
    return ready;
}

std::vector<AgentCoordinator::AgentTask> AgentCoordinator::propagateCompletion(PlanState& plan,
                                                                        const std::string& taskId)
{
    std::vector<AgentTask> ready;
    const auto dependents = plan.dependents.value(taskId);
    for (const auto& dependentId : dependents) {
        auto remaining = plan.remainingDependencies.value(dependentId, 0);
        if (remaining > 0) {
            remaining -= 1;
            plan.remainingDependencies[dependentId] = remaining;
        }
        if (remaining == 0 && plan.state.value(dependentId) == TaskState::Pending &&
            allPrerequisitesComplete(plan, dependentId)) {
            plan.state[dependentId] = TaskState::Ready;
            ready.append(plan.tasks.value(dependentId));
        }
    }
    return ready;
}

void AgentCoordinator::markDownstreamAsSkipped(PlanState& plan, const std::string& blockingTaskId)
{
    QQueue<std::string> queue;
    queue.enqueue(blockingTaskId);

    while (!queue.isEmpty()) {
        const auto current = queue.dequeue();
        const auto dependents = plan.dependents.value(current);
        for (const auto& dep : dependents) {
            auto& state = plan.state[dep];
            if (state == TaskState::Pending || state == TaskState::Ready) {
                state = TaskState::Skipped;
                queue.enqueue(dep);
            }
        }
    }
}

bool AgentCoordinator::allPrerequisitesComplete(const PlanState& plan, const std::string& taskId) const
{
    const auto taskIt = plan.tasks.find(taskId);
    if (taskIt == plan.tasks.end()) {
        return false;
    }
    const auto& dependencies = taskIt.value().dependencies;
    for (const auto& dep : dependencies) {
        if (plan.state.value(dep) != TaskState::Completed) {
            return false;
        }
    }
    return true;
}

void AgentCoordinator::mergeContext(void*& target, const void*& delta) const
{
    for (auto it = delta.begin(); it != delta.end(); ++it) {
        target.insert(it.key(), it.value());
    }
}

AgentCoordinator::PlanFinalization AgentCoordinator::maybeFinalizePlan(const std::string& planId,
                                                                       PlanState& plan)
{
    (planId);

    PlanFinalization result;

    bool runningOrPending = false;
    bool anyFailed = false;

    for (auto it = plan.state.cbegin(); it != plan.state.cend(); ++it) {
        switch (it.value()) {
        case TaskState::Pending:
        case TaskState::Ready:
        case TaskState::Running:
            runningOrPending = true;
            break;
        case TaskState::Failed:
            anyFailed = true;
            break;
        default:
            break;
        }
        if (runningOrPending) {
            break;
        }
    }

    if (!runningOrPending) {
        result.finished = true;
        result.context = plan.sharedContext;
        if (plan.cancelled) {
            result.cancelled = true;
            result.reason = plan.cancelReason;
        } else if (anyFailed) {
            result.success = false;
            result.reason = plan.cancelReason;
        } else {
            result.success = true;
        }
    }

    return result;
}

void AgentCoordinator::invalidateStatusCache(const std::string& planId)
{
    // Called when plan state changes (task completion, cancellation, etc.)
    m_statusCache.remove(planId);
}

void* AgentCoordinator::buildPlanStatus(const PlanState& plan) const
{
    // Build the JSON status representation (expensive operation for large plans)
    void* status;
    status["planId"] = plan.id;
    status["createdAt"] = plan.createdAt.toString(//ISODate);
    status["cancelled"] = plan.cancelled;
    status["cancelReason"] = plan.cancelReason;

    void* taskArray;
    for (auto it = plan.tasks.cbegin(); it != plan.tasks.cend(); ++it) {
        const auto& task = it.value();
        void* taskObj;
        taskObj["id"] = task.id;
        taskObj["name"] = task.name;
        taskObj["agentId"] = task.agentId;
        taskObj["state"] = taskStateToString(plan.state.value(task.id));
        taskObj["priority"] = task.priority;
        taskObj["dependencies"] = void*::fromStringList(task.dependencies);
        taskObj["remainingDependencies"] = plan.remainingDependencies.value(task.id);
        taskArray.append(taskObj);
    }
    status["tasks"] = taskArray;
    status["context"] = plan.sharedContext;
    return status;
}

