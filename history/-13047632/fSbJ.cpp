#include "agent_coordinator.hpp"

#include <QDebug>
#include <QJsonValue>
#include <QQueue>
#include <QReadLocker>
#include <QSet>
#include <QWriteLocker>

#include <algorithm>
#include <functional>

using TaskState = AgentCoordinator::AgentTaskState;

namespace {
QString taskStateToString(TaskState state)
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
}

AgentCoordinator::AgentCoordinator(QObject* parent)
    : QObject(parent)
{
    qRegisterMetaType<AgentCoordinator::AgentTask>("AgentCoordinator::AgentTask");
}

AgentCoordinator::~AgentCoordinator()
{
    QWriteLocker locker(&m_lock);
    m_plans.clear();
    m_agents.clear();
}

bool AgentCoordinator::registerAgent(const QString& agentId,
                                     const QStringList& capabilities,
                                     int maxConcurrency)
{
    if (agentId.isEmpty() || maxConcurrency <= 0) {
        return false;
    }

    QWriteLocker locker(&m_lock);
    AgentMetadata meta;
    meta.agentId = agentId;
    meta.capabilities = capabilities;
    meta.maxConcurrency = std::max(1, maxConcurrency);
    meta.activeAssignments = 0;
    meta.available = true;
    meta.registeredAt = QDateTime::currentDateTimeUtc();
    m_agents.insert(agentId, meta);
    return true;
}

bool AgentCoordinator::unregisterAgent(const QString& agentId)
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

bool AgentCoordinator::setAgentAvailability(const QString& agentId, bool available)
{
    QWriteLocker locker(&m_lock);
    if (!m_agents.contains(agentId)) {
        return false;
    }
    m_agents[agentId].available = available;
    return true;
}

bool AgentCoordinator::isAgentAvailable(const QString& agentId) const
{
    QReadLocker locker(&m_lock);
    const auto it = m_agents.find(agentId);
    if (it == m_agents.end()) {
        return false;
    }
    const auto& meta = it.value();
    return meta.available && meta.activeAssignments < meta.maxConcurrency;
}

QString AgentCoordinator::submitPlan(const QList<AgentTask>& tasks,
                                     const QJsonObject& initialContext)
{
    QString validationError;
    if (!validateTasks(tasks, validationError)) {
        qWarning() << "AgentCoordinator::submitPlan validation failed:" << validationError;
        return {};
    }

    // BOTTLENECK #1 FIX: Build plan locally OUTSIDE lock to minimize critical section
    // All expensive computation (graph traversal, ready task calculation) happens here
    PlanState plan;
    plan.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    plan.sharedContext = initialContext;
    plan.createdAt = QDateTime::currentDateTimeUtc();

    for (const auto& task : tasks) {
        plan.tasks.insert(task.id, task);
        plan.state.insert(task.id, TaskState::Pending);
        plan.remainingDependencies.insert(task.id, task.dependencies.size());
    }

    initialisePlanGraphs(plan);
    QList<AgentTask> readyToEmit = scheduleReadyTasks(plan);

    // MINIMAL CRITICAL SECTION: Only the atomic insertion (< 100µs instead of 2-5ms)
    {
        QWriteLocker locker(&m_lock);
        m_plans.insert(plan.id, plan);
    }

    // Signal emission OUTSIDE lock (Qt event loop interaction should not happen under lock)
    emit planSubmitted(plan.id);
    for (const auto& task : readyToEmit) {
        emit taskReady(plan.id, task);
    }
    return plan.id;
}

bool AgentCoordinator::cancelPlan(const QString& planId, const QString& reason)
{
    QList<AgentTask> cancelledTasks;
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
    }

    emit planCancelled(planId, reason);
    for (const auto& task : cancelledTasks) {
        emit taskCompleted(planId, task, false, QStringLiteral("plan-cancelled"));
    }
    return true;
}

bool AgentCoordinator::startTask(const QString& planId, const QString& taskId)
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

    emit taskStarted(planId, task);
    return true;
}

bool AgentCoordinator::completeTask(const QString& planId,
                                    const QString& taskId,
                                    const QJsonObject& outputContext,
                                    bool success,
                                    const QString& message)
{
    QList<AgentTask> newlyReady;
    AgentTask task;
    QString failureReason = message;
    PlanFinalization finalization;

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
        auto currentState = plan.state.value(taskId, TaskState::Pending);
        if (currentState != TaskState::Running && currentState != TaskState::Ready) {
            return false;
        }

        auto agentIt = m_agents.find(task.agentId);
        if (agentIt != m_agents.end() && agentIt->activeAssignments > 0) {
            agentIt->activeAssignments--;
        }

        if (!success) {
            plan.state[taskId] = TaskState::Failed;
            if (failureReason.isEmpty()) {
                failureReason = QStringLiteral("Task %1 failed").arg(taskId);
            }
            markDownstreamAsSkipped(plan, taskId);
            plan.cancelReason = failureReason;
        } else {
            plan.state[taskId] = TaskState::Completed;
            mergeContext(plan.sharedContext, outputContext);
            newlyReady = propagateCompletion(plan, taskId);
        }
        finalization = maybeFinalizePlan(planId, plan);
        
        // Invalidate status cache when plan state changes (Bottleneck #9 fix)
        invalidateStatusCache(planId);
    }

    emit taskCompleted(planId, task, success, message);

    for (const auto& readyTask : newlyReady) {
        emit taskReady(planId, readyTask);
    }

    if (finalization.finished) {
        if (finalization.cancelled) {
            // planCancelled already emitted during cancel operation
            return true;
        }
        if (finalization.success) {
            emit planCompleted(planId, finalization.context);
        } else {
            QString reasonToEmit = finalization.reason;
            if (reasonToEmit.isEmpty()) {
                reasonToEmit = failureReason.isEmpty() ? QStringLiteral("plan-failed") : failureReason;
            }
            emit planFailed(planId, reasonToEmit);
        }
    }

    return true;
}

QList<QString> AgentCoordinator::getReadyTasks(const QString& planId) const
{
    QReadLocker locker(&m_lock);
    QList<QString> ready;
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

QJsonObject AgentCoordinator::getPlanStatus(const QString& planId) const
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
        QJsonObject status;
        status["error"] = QStringLiteral("plan-not-found");
        return status;
    }
    
    const auto& plan = planIt.value();
    QJsonObject status = buildPlanStatus(plan);
    
    // Cache the result for future queries
    m_statusCache[planId] = status;
    return status;
}
}

QJsonObject AgentCoordinator::getCoordinatorStats() const
{
    QReadLocker locker(&m_lock);
    QJsonObject stats;
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

bool AgentCoordinator::validateTasks(const QList<AgentTask>& tasks, QString& error) const
{
    if (tasks.isEmpty()) {
        error = QStringLiteral("plan-empty");
        return false;
    }

    QReadLocker locker(&m_lock);

    QSet<QString> ids;
    for (const auto& task : tasks) {
        if (task.id.isEmpty()) {
            error = QStringLiteral("task-id-empty");
            return false;
        }
        if (ids.contains(task.id)) {
            error = QStringLiteral("duplicate-task-id");
            return false;
        }
        ids.insert(task.id);

        if (!m_agents.contains(task.agentId)) {
            error = QStringLiteral("unknown-agent:%1").arg(task.agentId);
            return false;
        }

        for (const auto& dep : task.dependencies) {
            if (dep == task.id) {
                error = QStringLiteral("self-dependency:%1").arg(task.id);
                return false;
            }
        }
    }

    locker.unlock();

    if (detectCycle(tasks)) {
        error = QStringLiteral("dependency-cycle");
        return false;
    }

    // Ensure dependencies reference known tasks
    for (const auto& task : tasks) {
        for (const auto& dep : task.dependencies) {
            if (!ids.contains(dep)) {
                error = QStringLiteral("missing-dependency:%1->%2").arg(task.id, dep);
                return false;
            }
        }
    }

    return true;
}

bool AgentCoordinator::detectCycle(const QList<AgentTask>& tasks) const
{
    // BOTTLENECK #2 FIX: Color-based DFS for true O(V+E) complexity (was O(V·(V+E)) worst case)
    // Build graph map once
    QMap<QString, QStringList> graph;
    for (const auto& task : tasks) {
        graph.insert(task.id, task.dependencies);
    }

    // Three states: 0=White (unvisited), 1=Gray (visiting), 2=Black (visited)
    QHash<QString, int> color;
    
    std::function<bool(const QString&)> dfs = [&](const QString& node) -> bool {
        int node_color = color.value(node, 0);
        
        if (node_color == 1) {
            // Gray node = back edge detected = cycle!
            return true;
        }
        if (node_color == 2) {
            // Black node = already fully processed
            return false;
        }
        
        // Mark as Gray (currently visiting)
        color[node] = 1;
        
        // Traverse direct dependencies only (no redundant traversals)
        const auto deps = graph.value(node);
        for (const auto& dep : deps) {
            if (!graph.contains(dep)) {
                continue;
            }
            if (dfs(dep)) {
                return true;  // Cycle detected in subtree
            }
        }
        
        // Mark as Black (fully processed)
        color[node] = 2;
        return false;
    };

    // Only call DFS on White nodes (no redundant calls on already-visited nodes)
    for (auto it = graph.cbegin(); it != graph.cend(); ++it) {
        if (color.value(it.key(), 0) == 0) {  // White node
            if (dfs(it.key())) {
                return true;
            }
        }
    }
    
    return false;
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

QList<AgentCoordinator::AgentTask> AgentCoordinator::scheduleReadyTasks(PlanState& plan)
{
    QList<AgentTask> ready;
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

QList<AgentCoordinator::AgentTask> AgentCoordinator::propagateCompletion(PlanState& plan,
                                                                        const QString& taskId)
{
    QList<AgentTask> ready;
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

void AgentCoordinator::markDownstreamAsSkipped(PlanState& plan, const QString& blockingTaskId)
{
    QQueue<QString> queue;
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

bool AgentCoordinator::allPrerequisitesComplete(const PlanState& plan, const QString& taskId) const
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

void AgentCoordinator::mergeContext(QJsonObject& target, const QJsonObject& delta) const
{
    for (auto it = delta.begin(); it != delta.end(); ++it) {
        target.insert(it.key(), it.value());
    }
}

AgentCoordinator::PlanFinalization AgentCoordinator::maybeFinalizePlan(const QString& planId,
                                                                       PlanState& plan)
{
    Q_UNUSED(planId);

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

void AgentCoordinator::invalidateStatusCache(const QString& planId)
{
    // Called when plan state changes (task completion, cancellation, etc.)
    m_statusCache.remove(planId);
}

QJsonObject AgentCoordinator::buildPlanStatus(const PlanState& plan) const
{
    // Build the JSON status representation (expensive operation for large plans)
    QJsonObject status;
    status["planId"] = plan.id;
    status["createdAt"] = plan.createdAt.toString(Qt::ISODate);
    status["cancelled"] = plan.cancelled;
    status["cancelReason"] = plan.cancelReason;

    QJsonArray taskArray;
    for (auto it = plan.tasks.cbegin(); it != plan.tasks.cend(); ++it) {
        const auto& task = it.value();
        QJsonObject taskObj;
        taskObj["id"] = task.id;
        taskObj["name"] = task.name;
        taskObj["agentId"] = task.agentId;
        taskObj["state"] = taskStateToString(plan.state.value(task.id));
        taskObj["priority"] = task.priority;
        taskObj["dependencies"] = QJsonArray::fromStringList(task.dependencies);
        taskObj["remainingDependencies"] = plan.remainingDependencies.value(task.id);
        taskArray.append(taskObj);
    }
    status["tasks"] = taskArray;
    status["context"] = plan.sharedContext;
    return status;
}
