#pragma once

#include <QString>
#include <QObject>
#include <QMap>
#include <QList>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <memory>
#include <functional>

class SubagentPool;

/**
 * @brief SubagentTaskDistributor - Coordinates task distribution and multitasking
 * 
 * Features:
 * - Intelligent task decomposition (breaking large tasks into subtasks)
 * - Load balancing across subagents
 * - Dependency tracking between tasks
 * - Result aggregation and synchronization
 * - Progress tracking
 * - Error handling and recovery
 */
class SubagentTaskDistributor : public QObject {
    Q_OBJECT

public:
    enum class TaskDependency {
        NoDelay,       // Tasks can run in parallel
        Sequential,    // Tasks must run in order
        AllMustComplete  // Task waits for all previous tasks
    };

    struct DistributedTask {
        QString taskId;
        QString parentTaskId;  // For hierarchical task decomposition
        QString description;
        std::function<QJsonObject()> executor;
        int priority = 0;
        TaskDependency dependencyMode = TaskDependency::NoDelay;
        QStringList dependsOnTasks;  // Task IDs this task depends on
        int maxRetries = 3;
        int timeoutMs = 30000;
        QJsonObject metadata;
        
        // Tracking
        bool completed = false;
        QString status = "pending";
        QJsonObject result;
        QString error;
        int attemptCount = 0;
    };

    explicit SubagentTaskDistributor(std::shared_ptr<SubagentPool> pool, QObject* parent = nullptr);
    ~SubagentTaskDistributor() override;

    // Task distribution
    QString distributeTask(const DistributedTask& task);
    QString distributeComplexTask(const QString& description, 
                                  const QList<DistributedTask>& subtasks);
    
    // Task management
    bool cancelTask(const QString& taskId);
    bool retryTask(const QString& taskId);
    bool getTaskStatus(const QString& taskId, QJsonObject& outStatus) const;

    // Multitasking orchestration
    QString launchParallelTasks(const QList<DistributedTask>& tasks);
    QString launchSequentialTasks(const QList<DistributedTask>& tasks);
    QString launchConditionalTasks(const DistributedTask& condition,
                                   const DistributedTask& trueBranch,
                                   const DistributedTask& falseBranch);

    // Progress and results
    QJsonObject getTaskResult(const QString& taskId) const;
    double getTaskProgressPercent(const QString& taskId) const;
    QJsonArray getAllTaskResults(const QString& parentTaskId = QString()) const;
    QJsonObject getTaskHierarchy(const QString& rootTaskId) const;

    // Statistics
    QJsonObject getDistributorMetrics() const;
    int getPendingTaskCount() const { return m_pendingTasks.size(); }
    int getCompletedTaskCount() const { return m_completedTasks.size(); }
    int getFailedTaskCount() const { return m_failedTasks.size(); }

signals:
    void taskDistributed(const QString& taskId);
    void taskCompleted(const QString& taskId, const QJsonObject& result);
    void taskFailed(const QString& taskId, const QString& error);
    void taskProgress(const QString& taskId, double percentComplete);
    void allTasksCompleted(const QString& parentTaskId);
    void dependencyResolved(const QString& taskId, const QString& dependentId);

private slots:
    void onSubagentTaskCompleted(const QString& taskId, const QJsonObject& result);
    void onSubagentTaskFailed(const QString& taskId, const QString& error);
    void processPendingDependencies(const QString& completedTaskId);

private:
    std::shared_ptr<SubagentPool> m_pool;
    
    // Task tracking
    QMap<QString, DistributedTask> m_tasks;
    QList<QString> m_pendingTasks;
    QList<QString> m_completedTasks;
    QList<QString> m_failedTasks;
    
    // Task hierarchy for complex tasks
    QMap<QString, QList<QString>> m_taskHierarchy;  // parentId -> [childIds]
    
    // Dependency tracking
    QMap<QString, QStringList> m_taskDependencies;  // taskId -> [dependentIds]
    QMap<QString, int> m_completedDependencyCount;
    
    // Statistics
    QMap<QString, qint64> m_taskStartTimes;
    QMap<QString, qint64> m_taskEndTimes;
    
    mutable QMutex m_mutex;
    
    // Helper methods
    QString executeDistributedTask(const DistributedTask& task);
    bool canExecuteTask(const QString& taskId) const;
    void checkAndExecuteDependentTasks(const QString& completedTaskId);
    QJsonObject aggregateResults(const QString& parentTaskId) const;
};

/**
 * @brief MultitaskingCoordinator - Manages multitasking at the chat session level
 * 
 * Allows up to 20 concurrent subagents per chat session with:
 * - Task queuing and prioritization
 * - Resource management
 * - Performance monitoring
 * - Automatic task distribution
 * - Result caching
 */
class MultitaskingCoordinator : public QObject {
    Q_OBJECT

public:
    explicit MultitaskingCoordinator(const QString& sessionId, QObject* parent = nullptr);
    ~MultitaskingCoordinator() override;

    // Subagent management
    bool initializeSubagents(int agentCount = 5);
    int getSubagentCount() const;
    int getAvailableSubagentCount() const;
    bool addSubagent();
    bool removeSubagent();
    void scaleSubagents(int targetCount);

    // Task submission and coordination
    QString submitTask(const QString& description, 
                      std::function<QJsonObject()> executor,
                      int priority = 0);
    
    QString submitComplexTask(const QString& description,
                             const QList<SubagentTaskDistributor::DistributedTask>& subtasks);
    
    QString submitParallelTasks(const QList<SubagentTaskDistributor::DistributedTask>& tasks);
    
    QString submitSequentialTasks(const QList<SubagentTaskDistributor::DistributedTask>& tasks);

    // Task management
    bool cancelTask(const QString& taskId);
    bool waitForTask(const QString& taskId, int timeoutMs = 30000);
    QJsonObject getTaskResult(const QString& taskId) const;
    QJsonObject getTaskStatus(const QString& taskId) const;

    // Performance metrics
    QJsonObject getCoordinatorMetrics() const;
    QJsonArray getActiveTasksList() const;
    double getCpuUsagePercent() const { return m_cpuUsagePercent; }
    qint64 getMemoryUsageMB() const { return m_memoryUsageMB; }

    // Configuration
    void setMaxConcurrentTasks(int max) { m_maxConcurrentTasks = max; }
    void setResourceLimits(qint64 memoryMB, int cpuPercent);
    void enableAutoScaling(bool enabled) { m_autoScalingEnabled = enabled; }

signals:
    void taskSubmitted(const QString& taskId);
    void taskCompleted(const QString& taskId, const QJsonObject& result);
    void taskFailed(const QString& taskId, const QString& error);
    void taskProgress(const QString& taskId, double percentComplete);
    void subagentAdded(int totalCount);
    void subagentRemoved(int totalCount);
    void resourceWarning(const QString& resourceType, double usage);
    void allTasksCompleted();

private slots:
    void onDistributorTaskCompleted(const QString& taskId, const QJsonObject& result);
    void onDistributorTaskFailed(const QString& taskId, const QString& error);
    void onPoolAgentAdded(const QString& agentId);
    void onPoolAgentRemoved(const QString& agentId);
    void collectResourceMetrics();

private:
    QString m_sessionId;
    std::shared_ptr<SubagentPool> m_pool;
    std::shared_ptr<SubagentTaskDistributor> m_distributor;
    
    // Configuration
    int m_maxConcurrentTasks = 20;
    int m_maxSubagents = 20;
    qint64 m_maxMemoryMB = 2048;
    int m_maxCpuPercent = 80;
    bool m_autoScalingEnabled = true;
    
    // State tracking
    QMap<QString, QJsonObject> m_taskResults;
    QMap<QString, bool> m_taskWaitFlags;
    
    // Resource monitoring
    double m_cpuUsagePercent = 0.0;
    qint64 m_memoryUsageMB = 0;
    QTimer* m_metricsTimer = nullptr;
    
    mutable QMutex m_mutex;
    
    // Helper methods
    void monitorResources();
    void enforceResourceLimits();
};
