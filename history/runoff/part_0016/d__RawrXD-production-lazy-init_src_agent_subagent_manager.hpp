#pragma once

#include <QString>
#include <QObject>
#include <QMap>
#include <QQueue>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QJsonObject>
#include <QJsonArray>
#include <memory>
#include <functional>
#include <atomic>
#include <chrono>

/**
 * @brief Subagent - Autonomous worker that can execute tasks in parallel
 * 
 * Each subagent is a lightweight worker capable of:
 * - Running in its own thread
 * - Processing multiple types of tasks
 * - Reporting progress and completion status
 * - Handling errors and retries
 * - Supporting cancellation
 */
class Subagent : public QObject {
    Q_OBJECT

public:
    enum class Status {
        Idle,           // Not executing any task
        Busy,           // Currently executing a task
        Waiting,        // Waiting for task input
        Failed,         // Last task failed
        Paused,         // Task execution paused
        Terminated      // Subagent terminated
    };

    enum class TaskPriority {
        Low = 0,
        Normal = 1,
        High = 2,
        Critical = 3
    };

    struct Task {
        QString taskId;
        QString description;
        std::function<QJsonObject()> executor;
        TaskPriority priority = TaskPriority::Normal;
        int maxRetries = 3;
        int timeoutMs = 30000;
        QJsonObject metadata;
        QDateTime createdAt;
        int currentRetry = 0;
    };

    explicit Subagent(const QString& agentId, QObject* parent = nullptr);
    ~Subagent() override;

    // Identity and status
    QString getAgentId() const { return m_agentId; }
    Status getStatus() const { return m_status; }
    QString getStatusString() const;
    bool isIdle() const { return m_status == Status::Idle; }
    bool isBusy() const { return m_status == Status::Busy; }

    // Task execution
    bool executeTask(const Task& task);
    bool cancelCurrentTask();
    
    // Performance metrics
    qint64 getTasksCompleted() const { return m_tasksCompleted; }
    qint64 getTasksFailed() const { return m_tasksFailed; }
    double getAverageTaskDurationMs() const { return m_averageTaskDurationMs; }
    double getSuccessRate() const;
    
    // State management
    void pause();
    void resume();
    void terminate();
    bool isTerminated() const { return m_status == Status::Terminated; }

    // Diagnostics
    QJsonObject getMetrics() const;
    QString getCurrentTaskId() const { return m_currentTaskId; }
    QDateTime getLastActivityTime() const { return m_lastActivityTime; }

signals:
    void taskStarted(const QString& taskId);
    void taskProgress(const QString& taskId, int percentComplete);
    void taskCompleted(const QString& taskId, const QJsonObject& result);
    void taskFailed(const QString& taskId, const QString& error);
    void taskCancelled(const QString& taskId);
    void statusChanged(int newStatus);
    void error(const QString& errorMessage);

private slots:
    void executeTaskInternal();
    void onTaskTimeout();

private:
    QString m_agentId;
    Status m_status = Status::Idle;
    
    // Current task state
    std::shared_ptr<Task> m_currentTask;
    QString m_currentTaskId;
    QTimer* m_timeoutTimer = nullptr;
    
    // Performance tracking
    qint64 m_tasksCompleted = 0;
    qint64 m_tasksFailed = 0;
    double m_averageTaskDurationMs = 0.0;
    std::vector<double> m_recentTaskDurations;
    
    // State management
    bool m_paused = false;
    QDateTime m_lastActivityTime;
    QElapsedTimer m_taskTimer;
    
    // Thread safety
    mutable QMutex m_mutex;
};

/**
 * @brief SubagentPool - Manages a pool of subagents for efficient task distribution
 * 
 * Features:
 * - Dynamic pool sizing (1-20 agents per chat session)
 * - Load balancing across idle agents
 * - Priority-based task queuing
 * - Resource awareness
 * - Metrics collection
 */
class SubagentPool : public QObject {
    Q_OBJECT

public:
    explicit SubagentPool(const QString& poolId, int maxAgents = 20, QObject* parent = nullptr);
    ~SubagentPool() override;

    // Pool management
    QString getPoolId() const { return m_poolId; }
    int getAgentCount() const { return m_agents.size(); }
    int getMaxAgents() const { return m_maxAgents; }
    int getIdleAgentCount() const;
    int getBusyAgentCount() const;

    // Task submission
    QString submitTask(const Subagent::Task& task);
    bool cancelTask(const QString& taskId);
    bool pausePool();
    bool resumePool();
    void terminatePool();

    // Agent management
    bool addAgent();
    bool removeAgent(const QString& agentId = QString());
    Subagent* getIdleAgent();
    Subagent* getAgent(const QString& agentId);
    QList<Subagent*> getAllAgents() const;

    // Statistics
    QJsonObject getPoolMetrics() const;
    QJsonArray getAllAgentMetrics() const;
    double getAverageLoadFactor() const;
    int getPendingTaskCount() const { return m_taskQueue.size(); }

    // Configuration
    void setMaxAgents(int max);
    void setLoadBalancingStrategy(const QString& strategy); // "round-robin", "least-busy", "random"
    void setAutoScaling(bool enabled, int minAgents = 1, int maxAgents = 20);

signals:
    void taskQueued(const QString& taskId);
    void taskStarted(const QString& taskId, const QString& agentId);
    void taskCompleted(const QString& taskId, const QJsonObject& result);
    void taskFailed(const QString& taskId, const QString& error);
    void agentAdded(const QString& agentId);
    void agentRemoved(const QString& agentId);
    void poolTerminated();
    void metricsUpdated(const QJsonObject& metrics);

private slots:
    void processTaskQueue();
    void onSubagentTaskCompleted(const QString& taskId, const QJsonObject& result);
    void onSubagentTaskFailed(const QString& taskId, const QString& error);
    void onSubagentStatusChanged(int newStatus);
    void evaluateAutoScaling();

private:
    QString m_poolId;
    int m_maxAgents;
    
    // Agent management
    QMap<QString, std::shared_ptr<Subagent>> m_agents;
    QList<QString> m_agentThreads;
    
    // Task queuing
    struct QueuedTask {
        QString taskId;
        Subagent::Task task;
        QDateTime enqueuedAt;
    };
    QQueue<QueuedTask> m_taskQueue;
    QMap<QString, QString> m_taskToAgentMap;  // taskId -> agentId
    
    // Load balancing
    QString m_loadBalancingStrategy = "least-busy";
    
    // Auto-scaling
    bool m_autoScalingEnabled = false;
    int m_minAgents = 1;
    int m_currentDesiredAgents = 5;
    QTimer* m_autoScalingTimer = nullptr;
    
    // State
    bool m_paused = false;
    
    // Metrics
    QJsonObject m_poolMetrics;
    
    // Thread safety
    mutable QMutex m_mutex;
    QWaitCondition m_taskAvailable;
    
    // Internal methods
    void selectAndDistributeTask();
    QString selectNextAgent();
    QString selectNextAgentRoundRobin();
    QString selectNextAgentLeastBusy();
    QString selectNextAgentRandom();
};

/**
 * @brief SubagentManager - Global manager for all subagent pools across chat sessions
 * 
 * Manages:
 * - Creation and lifecycle of subagent pools per chat session
 * - Global resource constraints
 * - Cross-pool coordination
 * - Telemetry and diagnostics
 */
class SubagentManager : public QObject {
    Q_OBJECT

public:
    static SubagentManager* getInstance();

    // Pool management
    std::shared_ptr<SubagentPool> createPool(const QString& poolId, int agentCount = 5);
    std::shared_ptr<SubagentPool> getPool(const QString& poolId);
    bool deletePool(const QString& poolId);
    QStringList getPoolIds() const;

    // Global configuration
    void setMaxPoolsPerSession(int max) { m_maxPoolsPerSession = max; }
    void setMaxAgentsPerPool(int max) { m_maxAgentsPerPool = max; }
    void setMaxConcurrentTasks(int max) { m_maxConcurrentTasks = max; }
    void setGlobalResourceLimit(qint64 memoryMB, int cpuPercent);

    // Task management across pools
    QString submitTaskToPool(const QString& poolId, const Subagent::Task& task);
    bool cancelTask(const QString& poolId, const QString& taskId);

    // Diagnostics
    QJsonObject getGlobalMetrics() const;
    QJsonArray getAllPoolMetrics() const;
    int getActiveTaskCount() const;

signals:
    void poolCreated(const QString& poolId);
    void poolDeleted(const QString& poolId);
    void taskSubmitted(const QString& poolId, const QString& taskId);
    void globalMetricsUpdated(const QJsonObject& metrics);

private:
    SubagentManager() = default;
    ~SubagentManager() override;

    static SubagentManager* s_instance;
    static QMutex s_mutex;

    QMap<QString, std::shared_ptr<SubagentPool>> m_pools;
    
    // Configuration
    int m_maxPoolsPerSession = 1;
    int m_maxAgentsPerPool = 20;
    int m_maxConcurrentTasks = 100;
    qint64 m_globalMemoryLimitMB = 4096;
    int m_globalCpuLimitPercent = 80;
    
    // Metrics
    QJsonObject m_globalMetrics;
    
    mutable QMutex m_mutex;
};
