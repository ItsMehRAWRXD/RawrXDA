// Autonomous Mission Scheduler - Production-grade background task orchestration
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QQueue>
#include <QMap>
#include <QTimer>
#include <QThread>
#include <QMutex>
#include <QWaitCondition>
#include <QElapsedTimer>
#include <memory>
#include <functional>

// Forward declarations
class AutonomousIntelligenceOrchestrator;
class ProductionReadinessOrchestrator;

/**
 * @brief Autonomous Mission defines a background task to be executed
 * 
 * Missions can be:
 * - One-time execution
 * - Recurring at fixed intervals
 * - Recurring based on system events
 * - Adaptive (frequency adjusts based on system load)
 */
struct AutonomousMission {
    QString missionId;              // Unique identifier
    QString missionName;            // Human-readable name
    QString description;            // Mission description
    std::function<QJsonObject()> task; // The actual task to execute
    
    // Scheduling parameters
    enum MissionPriority { Low = 0, Medium = 1, High = 2, Critical = 3 };
    MissionPriority priority = Medium;
    
    enum MissionType { OneTime = 0, FixedInterval = 1, Event = 2, Adaptive = 3 };
    MissionType type = OneTime;
    
    int intervalMs = 0;             // For FixedInterval type (0 = one-time)
    double minIntervalMs = 5000;    // For Adaptive type
    double maxIntervalMs = 60000;   // For Adaptive type
    
    // Resource requirements
    qint64 estimatedMemoryMB = 64;  // Estimated memory needed
    int estimatedCpuPercent = 10;   // Estimated CPU usage
    int maxDurationMs = 30000;      // Maximum allowed execution time
    
    // State tracking
    QDateTime createdAt;
    QDateTime lastExecutedAt;
    QDateTime lastExecuted; // For compatibility
    QDateTime nextScheduledAt;
    qint64 totalExecutions = 0;
    qint64 executionCount = 0; // For compatibility
    qint64 successfulExecutions = 0;
    qint64 failedExecutions = 0;
    qint64 failureCount = 0; // For compatibility
    double averageDurationMs = 0.0;
    
    // Retry policy
    int maxRetries = 3;
    int currentRetries = 0;
    int retryDelayMs = 1000;
    
    // Flags
    bool enabled = true;
    bool isRunning = false;
    bool autoRetry = true;
    
    // Result tracking
    QString lastError;
    QJsonObject lastResult;
};

/**
 * @brief Autonomous Mission Scheduler
 * 
 * Implements production-grade background task orchestration with:
 * - Priority-based mission queuing
 * - Adaptive scheduling (frequency adjusts based on system load)
 * - Resource-aware execution (prevents resource exhaustion)
 * - Real-time monitoring and metrics
 * - Automatic retry and error recovery
 * - Integration with Win32 system monitoring
 */
class AutonomousMissionScheduler : public QObject {
    Q_OBJECT

public:
    explicit AutonomousMissionScheduler(QObject* parent = nullptr);
    ~AutonomousMissionScheduler();

    // Mission management
    bool registerMission(const AutonomousMission& mission);
    bool unregisterMission(const QString& missionId);
    bool enableMission(const QString& missionId);
    bool disableMission(const QString& missionId);
    bool rescheduleMission(const QString& missionId, int newIntervalMs);
    
    // Mission execution
    void start();
    void stop();
    void pauseAll();
    void resumeAll();
    bool isPaused() const { return m_paused; }
    
    // Mission retrieval
    AutonomousMission* getMission(const QString& missionId);
    QList<AutonomousMission*> getAllMissions() const {
        QList<AutonomousMission*> result;
        for (const auto& mission : m_missions) {
            result.append(mission.get());
        }
        return result;
    }
    QList<AutonomousMission*> getActiveMissions() const;
    QList<AutonomousMission*> getPendingMissions() const;
    
    // Scheduling strategy
    enum SchedulingStrategy { FIFO = 0, PriorityBased = 1, AdaptiveLoad = 2 };
    void setSchedulingStrategy(SchedulingStrategy strategy) { m_schedulingStrategy = strategy; }
    SchedulingStrategy getSchedulingStrategy() const { return m_schedulingStrategy; }
    
    // System integration
    void setProductionReadiness(ProductionReadinessOrchestrator* readiness);
    void setIntelligenceOrchestrator(AutonomousIntelligenceOrchestrator* orchestrator);
    
    // Resource management
    void setResourceConstraints(qint64 maxConcurrentMemoryMB, int maxConcurrentTasks);
    void setSystemLoadThreshold(double cpuPercent, double memoryPercent);
    bool canExecuteMission(const AutonomousMission& mission) const;
    
    // Metrics and diagnostics
    QJsonObject getSchedulerMetrics() const;
    QJsonObject getMissionMetrics(const QString& missionId) const;
    QJsonArray getAllMissionMetrics() const;
    double getSystemLoadFactor() const;
    int getPendingMissionCount() const { return m_missionQueue.size(); }
    int getActiveTaskCount() const { return m_activeTasks.size(); }
    
    // Configuration
    void setMaxConcurrentTasks(int max) { m_maxConcurrentTasks = max; }
    void setTaskTimeoutMs(int timeoutMs) { m_taskTimeoutMs = timeoutMs; }
    void setMetricsCollectionInterval(int intervalMs) { m_metricsIntervalMs = intervalMs; }
    
    // Adaptive scheduling
    void enableAdaptiveScheduling(bool enabled) { m_adaptiveSchedulingEnabled = enabled; }
    bool isAdaptiveSchedulingEnabled() const { return m_adaptiveSchedulingEnabled; }

public slots:
    // Slot for manual mission execution
    void executeMissionNow(const QString& missionId);
    
    // Internal slots
    void onSchedulingCycle();
    void onMetricsCollection();
    void onSystemLoadChanged(double cpuPercent, double memoryPercent);
    void onTaskCompleted(const QString& missionId, const QJsonObject& result);
    void onTaskFailed(const QString& missionId, const QString& errorMessage);

signals:
    // Mission events
    void missionRegistered(const QString& missionId);
    void missionUnregistered(const QString& missionId);
    void missionStarted(const QString& missionId);
    void missionCompleted(const QString& missionId, const QJsonObject& result);
    void missionFailed(const QString& missionId, const QString& error);
    void missionRetrying(const QString& missionId, int retryCount);
    
    // Scheduler events
    void schedulerStarted();
    void schedulerStopped();
    void schedulerPaused();
    void schedulerResumed();
    
    // Resource events
    void systemLoadHigh(double cpuPercent, double memoryPercent);
    void resourceConstraintExceeded(const QString& resourceType);
    void adaptiveSchedulingAdjusted(const QString& missionId, int newIntervalMs);
    
    // Metrics events
    void metricsUpdated(const QJsonObject& metrics);
    void performanceAlert(const QString& alertMessage);

private slots:
    void processMissionQueue();
    void handleTaskTimeout();
    void collectSystemMetrics();

private:
    // Mission management
    QMap<QString, std::shared_ptr<AutonomousMission>> m_missions;
    QQueue<QString> m_missionQueue;
    QMap<QString, QThread*> m_activeTasks;
    QMap<QString, QTimer*> m_missionTimers;
    
    // Scheduling state
    bool m_running = false;
    bool m_paused = false;
    SchedulingStrategy m_schedulingStrategy = PriorityBased;
    
    // Resource tracking
    qint64 m_maxConcurrentMemoryMB = 2048;
    int m_maxConcurrentTasks = 8;
    qint64 m_currentMemoryUsageMB = 0;
    double m_currentCpuUsagePercent = 0.0;
    double m_currentMemoryUsagePercent = 0.0;
    
    // System thresholds
    double m_cpuLoadThreshold = 75.0;
    double m_memoryLoadThreshold = 80.0;
    double m_currentLoad = 0.0; // Added to fix link error
    
    // Timers
    QTimer* m_schedulingTimer = nullptr;
    QTimer* m_metricsTimer = nullptr;
    QTimer* m_timeoutWatchdog = nullptr;
    int m_schedulingIntervalMs = 1000;      // Schedule every 1 second
    int m_metricsIntervalMs = 5000;         // Collect metrics every 5 seconds
    
    // Metrics collection
    QJsonObject m_schedulerMetrics;
    QMap<QString, QJsonObject> m_missionMetrics;
    QElapsedTimer m_uptimeTimer;
    
    // Adaptive scheduling
    bool m_adaptiveSchedulingEnabled = true;
    QMap<QString, double> m_missionLoadFactors;  // Per-mission load adjustment
    
    // Pointers to external systems
    ProductionReadinessOrchestrator* m_productionReadiness = nullptr;
    AutonomousIntelligenceOrchestrator* m_intelligenceOrchestrator = nullptr;
    
    // Task execution timeout
    int m_taskTimeoutMs = 30000;
    
    // Internal methods
    void selectAndExecuteNextMission();
    void executeMissionInternal(AutonomousMission* mission);
    void completeMission(AutonomousMission* mission, const QJsonObject& result);
    void failMission(AutonomousMission* mission, const QString& error);
    void retryMission(AutonomousMission* mission);
    
    // Scheduling decision
    AutonomousMission* selectMissionByStrategy();
    AutonomousMission* selectMissionFIFO();
    AutonomousMission* selectMissionByPriority();
    AutonomousMission* selectMissionAdaptiveLoad();
    
    // Resource checking
    bool isResourceAvailable(const AutonomousMission& mission) const;
    void updateSystemLoadMetrics();
    void adjustAdaptiveScheduling();
    
    // Metrics and logging
    void recordMissionExecution(AutonomousMission* mission, qint64 durationMs, bool success);
    void updateSchedulerMetrics();
    void logMissionEvent(const QString& missionId, const QString& eventType, const QString& message);
    
    // Thread safety
    mutable QMutex m_mutex;
    QWaitCondition m_taskCondition;
};
