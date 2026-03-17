#ifndef TOOL_SCHEDULER_HPP
#define TOOL_SCHEDULER_HPP

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QJsonObject>
#include <QMap>
#include <QQueue>
#include <QMutex>
#include <QWaitCondition>

struct ScheduledTask {
    QString taskId;
    QString toolName;
    QStringList parameters;
    QDateTime scheduledTime;
    QDateTime executionTime;
    QString status; // "scheduled", "executing", "completed", "failed"
    QJsonObject result;
    int priority;
    int retryCount;
    QString recurrence; // "once", "daily", "weekly", "monthly"
};

struct ResourceAllocation {
    QString resourceType; // "cpu", "memory", "disk", "network"
    double allocation;
    double capacity;
    double utilization;
    QDateTime lastUpdated;
};

class ToolScheduler : public QObject {
    Q_OBJECT
    
public:
    static ToolScheduler* instance();
    
    // Task scheduling
    QString scheduleTask(const QString& toolName, const QStringList& parameters, 
                        const QDateTime& scheduledTime, int priority = 5);
    bool cancelTask(const QString& taskId);
    bool rescheduleTask(const QString& taskId, const QDateTime& newTime);
    
    // Recurring tasks
    QString scheduleRecurringTask(const QString& toolName, const QStringList& parameters,
                                 const QString& recurrence, const QDateTime& startTime);
    bool updateRecurringTask(const QString& taskId, const QString& newRecurrence);
    
    // Priority management
    void setTaskPriority(const QString& taskId, int priority);
    void setDefaultPriority(int priority);
    int getTaskPriority(const QString& taskId) const;
    
    // Resource management
    bool allocateResources(const QString& taskId, const QJsonObject& resourceRequirements);
    void releaseResources(const QString& taskId);
    QJsonObject getResourceUtilization() const;
    
    // Batch operations
    QList<QString> scheduleBatchTasks(const QJsonArray& tasks);
    bool executeBatch(const QList<QString>& taskIds);
    QJsonObject getBatchStatus(const QList<QString>& taskIds) const;
    
    // Dependency management
    void addTaskDependency(const QString& taskId, const QString& dependsOnTaskId);
    void removeTaskDependency(const QString& taskId, const QString& dependsOnTaskId);
    QList<QString> getTaskDependencies(const QString& taskId) const;
    
    // Execution control
    bool pauseTask(const QString& taskId);
    bool resumeTask(const QString& taskId);
    bool restartTask(const QString& taskId);
    
    // Monitoring and status
    ScheduledTask getTaskStatus(const QString& taskId) const;
    QList<ScheduledTask> getPendingTasks() const;
    QList<ScheduledTask> getCompletedTasks(const QDateTime& from, const QDateTime& to) const;
    
    // Performance optimization
    void optimizeSchedule();
    QJsonObject getScheduleEfficiency() const;
    void setOptimizationStrategy(const QString& strategy);
    
    // Enterprise features
    QJsonObject generateScheduleReport(const QDateTime& from, const QDateTime& to);
    QJsonObject predictScheduleLoad(const QDateTime& from, const QDateTime& to);
    bool meetsSLARequirements(const QString& taskId) const;
    
    // Cleanup and maintenance
    void cleanupCompletedTasks(int daysToKeep);
    void compressTaskHistory();
    void backupScheduleData(const QString& backupPath);
    
signals:
    void taskScheduled(const QString& taskId);
    void taskExecuting(const QString& taskId);
    void taskCompleted(const QString& taskId, const QJsonObject& result);
    void taskFailed(const QString& taskId, const QString& error);
    void scheduleOptimized(const QJsonObject& optimizationResults);
    void resourceAllocated(const QString& taskId, const QJsonObject& resources);
    void resourceContention(const QString& resourceType, double utilization);
    
private:
    explicit ToolScheduler(QObject *parent = nullptr);
    ~ToolScheduler();
    
    class Private;
    QScopedPointer<Private> d_ptr;
    
    Q_DISABLE_COPY(ToolScheduler)
};

#endif // TOOL_SCHEDULER_HPP