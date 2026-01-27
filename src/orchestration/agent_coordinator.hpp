#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QMutex>
#include <QObject>
#include <QQueue>
#include <QReadWriteLock>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QUuid>

#include <functional>

/**
 * @class AgentCoordinator
 * @brief Coordinates multiple AI agents and manages task DAG execution
 * 
 * Provides:
 * - Agent pool management (Research, Coder, Reviewer, Optimizer, Deployer)
 * - Task DAG (directed acyclic graph) execution with dependency resolution
 * - Inter-agent context sharing
 * - Resource conflict resolution
 * - Real-time progress tracking
 */
class AgentCoordinator : public QObject {
    Q_OBJECT

public:
    explicit AgentCoordinator(QObject* parent = nullptr);
    ~AgentCoordinator() override;

    struct AgentTask {
        QString id;                      //!< Unique task identifier
        QString name;                    //!< Human-readable label
        QString agentId;                 //!< Agent responsible for execution
        QStringList dependencies;        //!< Upstream task identifiers
        QJsonObject payload;             //!< Task-specific metadata/prompt
        int priority = 0;                //!< Larger value = higher priority
        int maxRetries = 0;              //!< Allowed automatic retries
    };

    enum class AgentTaskState {
        Pending,
        Ready,
        Running,
        Completed,
        Failed,
        Skipped,
        Cancelled
    };

    struct AgentMetadata {
        QString agentId;
        QStringList capabilities;
        int maxConcurrency = 1;
        int activeAssignments = 0;
        bool available = true;
        QDateTime registeredAt;
    };

    // ===== Agent Management =====
    bool registerAgent(const QString& agentId,
                       const QStringList& capabilities,
                       int maxConcurrency = 1);
    bool unregisterAgent(const QString& agentId);
    bool setAgentAvailability(const QString& agentId, bool available);
    bool isAgentAvailable(const QString& agentId) const;

    // ===== Plan Submission =====
    QString submitPlan(const QList<AgentTask>& tasks,
                       const QJsonObject& initialContext = {});
    bool cancelPlan(const QString& planId, const QString& reason = {});

    // ===== Task Lifecycle =====
    bool startTask(const QString& planId, const QString& taskId);
    bool completeTask(const QString& planId,
                      const QString& taskId,
                      const QJsonObject& outputContext = {},
                      bool success = true,
                      const QString& message = {});
    QList<QString> getReadyTasks(const QString& planId) const;

    // ===== Introspection =====
    QJsonObject getPlanStatus(const QString& planId) const;
    QJsonObject getCoordinatorStats() const;

signals:
    void planSubmitted(const QString& planId);
    void planCancelled(const QString& planId, const QString& reason);
    void planFailed(const QString& planId, const QString& reason);
    void planCompleted(const QString& planId, const QJsonObject& finalContext);
    void taskReady(const QString& planId, const AgentTask& task);
    void taskStarted(const QString& planId, const AgentTask& task);
    void taskCompleted(const QString& planId,
                       const AgentTask& task,
                       bool success,
                       const QString& message);

private:
    struct PlanState {
        QString id;
        QMap<QString, AgentTask> tasks;
        QMap<QString, AgentTaskState> state;
        QMap<QString, int> remainingDependencies;
        QMap<QString, QSet<QString>> dependents;
        QJsonObject sharedContext;
        QDateTime createdAt;
        bool cancelled = false;
        QString cancelReason;
    };

    struct PlanFinalization {
        bool finished = false;
        bool success = false;
        bool cancelled = false;
        QString reason;
        QJsonObject context;
    };

    mutable QReadWriteLock m_lock;
    QMap<QString, AgentMetadata> m_agents;
    QMap<QString, PlanState> m_plans;
    
    // Status cache for high-poll clients (Bottleneck #9 fix - avoid rebuilding JSON on every query)
    mutable QHash<QString, QJsonObject> m_statusCache;

    // ===== Helpers =====
    bool validateTasks(const QList<AgentTask>& tasks, QString& error) const;
    bool detectCycle(const QList<AgentTask>& tasks) const;
    void initialisePlanGraphs(PlanState& plan);
    QList<AgentTask> scheduleReadyTasks(PlanState& plan);
    QList<AgentTask> propagateCompletion(PlanState& plan, const QString& taskId);
    void markDownstreamAsSkipped(PlanState& plan, const QString& blockingTaskId);
    bool allPrerequisitesComplete(const PlanState& plan, const QString& taskId) const;
    void mergeContext(QJsonObject& target, const QJsonObject& delta) const;
    PlanFinalization maybeFinalizePlan(const QString& planId, PlanState& plan);
    void invalidateStatusCache(const QString& planId);
    QJsonObject buildPlanStatus(const PlanState& plan) const;
};

Q_DECLARE_METATYPE(AgentCoordinator::AgentTask)
