// Autonomous Mission Scheduler Implementation
#include "AutonomousMissionScheduler.h"
#include "production_readiness.h"
#include "../autonomous_intelligence_orchestrator.h"
#include <QDebug>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QThread>
#include <QCoreApplication>
#include <algorithm>
#include <cmath>

#ifdef _WIN32
    #include <windows.h>
    #include <psapi.h>
    #include <pdh.h>
    #pragma comment(lib, "psapi.lib")
    #pragma comment(lib, "pdh.lib")
#endif

AutonomousMissionScheduler::AutonomousMissionScheduler(QObject* parent)
    : QObject(parent)
{
    // Initialize timers
    m_schedulingTimer = new QTimer(this);
    m_metricsTimer = new QTimer(this);
    m_timeoutWatchdog = new QTimer(this);
    
    connect(m_schedulingTimer, &QTimer::timeout, this, &AutonomousMissionScheduler::processMissionQueue);
    connect(m_metricsTimer, &QTimer::timeout, this, &AutonomousMissionScheduler::onMetricsCollection);
    connect(m_timeoutWatchdog, &QTimer::timeout, this, &AutonomousMissionScheduler::handleTaskTimeout);
    
    // Initialize metrics
    m_uptimeTimer.start();
    
    qInfo() << "[MissionScheduler] Initialized with strategy:" 
            << (m_schedulingStrategy == FIFO ? "FIFO" : 
                m_schedulingStrategy == PriorityBased ? "Priority" : "AdaptiveLoad");
}

AutonomousMissionScheduler::~AutonomousMissionScheduler()
{
    stop();
    
    // Clean up all missions
    for (auto& mission : m_missions) {
        if (mission->isRunning) {
            mission->enabled = false;
        }
    }
    m_missions.clear();
    
    qInfo() << "[MissionScheduler] Scheduler destroyed - Total missions processed:"
            << m_schedulerMetrics["total_missions_executed"].toInt();
}

bool AutonomousMissionScheduler::registerMission(const AutonomousMission& mission)
{
    QMutexLocker locker(&m_mutex);
    
    if (m_missions.contains(mission.missionId)) {
        qWarning() << "[MissionScheduler] Mission already registered:" << mission.missionId;
        return false;
    }
    
    auto newMission = std::make_shared<AutonomousMission>(mission);
    newMission->createdAt = QDateTime::currentDateTime();
    newMission->nextScheduledAt = QDateTime::currentDateTime().addMSecs(newMission->intervalMs);
    
    m_missions[mission.missionId] = newMission;
    
    // Create timer for fixed interval missions
    if (newMission->type == AutonomousMission::FixedInterval && newMission->intervalMs > 0) {
        QTimer* timer = new QTimer(this);
        timer->setSingleShot(false);
        timer->setInterval(newMission->intervalMs);
        connect(timer, &QTimer::timeout, this, [this, id = mission.missionId]() {
            executeMissionNow(id);
        });
        m_missionTimers[mission.missionId] = timer;
        
        if (m_running) {
            timer->start();
        }
    }
    
    // Initialize mission metrics
    m_missionMetrics[mission.missionId] = QJsonObject();
    m_missionMetrics[mission.missionId]["mission_id"] = mission.missionId;
    m_missionMetrics[mission.missionId]["mission_name"] = mission.missionName;
    m_missionMetrics[mission.missionId]["created_at"] = newMission->createdAt.toString(Qt::ISODate);
    
    emit missionRegistered(mission.missionId);
    qInfo() << "[MissionScheduler] Mission registered:" << mission.missionId 
            << "-" << mission.missionName;
    
    return true;
}

bool AutonomousMissionScheduler::unregisterMission(const QString& missionId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return false;
    }
    
    auto mission = *it;
    if (mission->isRunning) {
        mission->enabled = false;
        qWarning() << "[MissionScheduler] Cannot unregister running mission:" << missionId;
        return false;
    }
    
    // Clean up timer if exists
    if (m_missionTimers.contains(missionId)) {
        QTimer* timer = m_missionTimers[missionId];
        timer->stop();
        timer->deleteLater();
        m_missionTimers.remove(missionId);
    }
    
    m_missions.erase(it);
    m_missionMetrics.remove(missionId);
    m_missionQueue.removeAll(missionId);
    
    emit missionUnregistered(missionId);
    qInfo() << "[MissionScheduler] Mission unregistered:" << missionId;
    
    return true;
}

bool AutonomousMissionScheduler::enableMission(const QString& missionId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return false;
    }
    
    (*it)->enabled = true;
    
    // Start timer if it's a fixed interval mission
    if (m_missionTimers.contains(missionId)) {
        m_missionTimers[missionId]->start();
    }
    
    qInfo() << "[MissionScheduler] Mission enabled:" << missionId;
    return true;
}

bool AutonomousMissionScheduler::disableMission(const QString& missionId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return false;
    }
    
    (*it)->enabled = false;
    
    // Stop timer if it's a fixed interval mission
    if (m_missionTimers.contains(missionId)) {
        m_missionTimers[missionId]->stop();
    }
    
    qInfo() << "[MissionScheduler] Mission disabled:" << missionId;
    return true;
}

bool AutonomousMissionScheduler::rescheduleMission(const QString& missionId, int newIntervalMs)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_missions.find(missionId);
    if (it == m_missions.end()) {
        return false;
    }
    
    auto mission = *it;
    mission->intervalMs = newIntervalMs;
    mission->nextScheduledAt = QDateTime::currentDateTime().addMSecs(newIntervalMs);
    
    if (m_missionTimers.contains(missionId)) {
        m_missionTimers[missionId]->setInterval(newIntervalMs);
    }
    
    qInfo() << "[MissionScheduler] Mission rescheduled:" << missionId 
            << "interval:" << newIntervalMs << "ms";
    
    return true;
}

void AutonomousMissionScheduler::start()
{
    if (m_running) {
        return;
    }
    
    m_running = true;
    
    m_schedulingTimer->start(m_schedulingIntervalMs);
    m_metricsTimer->start(m_metricsIntervalMs);
    m_timeoutWatchdog->start(m_taskTimeoutMs / 2);
    
    // Start all fixed-interval mission timers
    for (auto& timer : m_missionTimers) {
        timer->start();
    }
    
    emit schedulerStarted();
    qInfo() << "[MissionScheduler] Scheduler started with" 
            << m_missions.size() << "registered missions";
}

void AutonomousMissionScheduler::stop()
{
    if (!m_running) {
        return;
    }
    
    m_running = false;
    
    m_schedulingTimer->stop();
    m_metricsTimer->stop();
    m_timeoutWatchdog->stop();
    
    // Stop all mission timers
    for (auto& timer : m_missionTimers) {
        timer->stop();
    }
    
    // Wait for active tasks to complete (with timeout)
    int waitTime = 0;
    while (!m_activeTasks.isEmpty() && waitTime < 5000) {
        QCoreApplication::processEvents();
        QThread::msleep(100);
        waitTime += 100;
    }
    
    emit schedulerStopped();
    qInfo() << "[MissionScheduler] Scheduler stopped";
}

void AutonomousMissionScheduler::pauseAll()
{
    QMutexLocker locker(&m_mutex);
    m_paused = true;
    
    for (auto& timer : m_missionTimers) {
        timer->stop();
    }
    
    emit schedulerPaused();
    qInfo() << "[MissionScheduler] All missions paused";
}

void AutonomousMissionScheduler::resumeAll()
{
    QMutexLocker locker(&m_mutex);
    m_paused = false;
    
    for (auto& timer : m_missionTimers) {
        if (m_running) {
            timer->start();
        }
    }
    
    emit schedulerResumed();
    qInfo() << "[MissionScheduler] All missions resumed";
}

void AutonomousMissionScheduler::setProductionReadiness(ProductionReadinessOrchestrator* readiness)
{
    m_productionReadiness = readiness;
    qDebug() << "[MissionScheduler] Production readiness orchestrator registered";
}

void AutonomousMissionScheduler::setIntelligenceOrchestrator(AutonomousIntelligenceOrchestrator* orchestrator)
{
    m_intelligenceOrchestrator = orchestrator;
    qDebug() << "[MissionScheduler] Intelligence orchestrator registered";
}

void AutonomousMissionScheduler::setResourceConstraints(qint64 maxConcurrentMemoryMB, int maxConcurrentTasks)
{
    m_maxConcurrentMemoryMB = maxConcurrentMemoryMB;
    m_maxConcurrentTasks = maxConcurrentTasks;
    qInfo() << "[MissionScheduler] Resource constraints updated:"
            << "Memory:" << maxConcurrentMemoryMB << "MB"
            << "Tasks:" << maxConcurrentTasks;
}

void AutonomousMissionScheduler::setSystemLoadThreshold(double cpuPercent, double memoryPercent)
{
    m_cpuLoadThreshold = cpuPercent;
    m_memoryLoadThreshold = memoryPercent;
    qDebug() << "[MissionScheduler] System load thresholds updated:"
             << "CPU:" << cpuPercent << "%"
             << "Memory:" << memoryPercent << "%";
}

bool AutonomousMissionScheduler::canExecuteMission(const AutonomousMission& mission) const
{
    // Check concurrent task limit
    if (m_activeTasks.size() >= m_maxConcurrentTasks) {
        return false;
    }
    
    // Check memory constraint
    if (mission.estimatedMemoryMB > 0) {
        if (m_currentMemoryUsageMB + mission.estimatedMemoryMB > m_maxConcurrentMemoryMB) {
            return false;
        }
    }
    
    // Check system load
    if (m_currentCpuUsagePercent >= m_cpuLoadThreshold) {
        return false;
    }
    
    if (m_currentMemoryUsagePercent >= m_memoryLoadThreshold) {
        return false;
    }
    
    return true;
}

QJsonObject AutonomousMissionScheduler::getSchedulerMetrics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonObject metrics = m_schedulerMetrics;
    metrics["uptime_seconds"] = m_uptimeTimer.elapsed() / 1000;
    metrics["running"] = m_running;
    metrics["paused"] = m_paused;
    metrics["active_tasks"] = m_activeTasks.size();
    metrics["pending_missions"] = m_missionQueue.size();
    metrics["total_missions_registered"] = m_missions.size();
    metrics["system_cpu_usage_percent"] = m_currentCpuUsagePercent;
    metrics["system_memory_usage_percent"] = m_currentMemoryUsagePercent;
    metrics["scheduling_strategy"] = 
        m_schedulingStrategy == FIFO ? "FIFO" : 
        m_schedulingStrategy == PriorityBased ? "Priority" : "AdaptiveLoad";
    
    return metrics;
}

QJsonObject AutonomousMissionScheduler::getMissionMetrics(const QString& missionId) const
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_missionMetrics.find(missionId);
    if (it != m_missionMetrics.end()) {
        return it.value();
    }
    
    return QJsonObject();
}

QJsonArray AutonomousMissionScheduler::getAllMissionMetrics() const
{
    QMutexLocker locker(&m_mutex);
    
    QJsonArray array;
    for (const auto& metrics : m_missionMetrics) {
        array.append(metrics);
    }
    
    return array;
}

double AutonomousMissionScheduler::getSystemLoadFactor() const
{
    // Calculate combined load factor (0.0 to 1.0)
    double cpuFactor = m_currentCpuUsagePercent / 100.0;
    double memoryFactor = m_currentMemoryUsagePercent / 100.0;
    double taskFactor = double(m_activeTasks.size()) / double(m_maxConcurrentTasks);
    
    return (cpuFactor + memoryFactor + taskFactor) / 3.0;
}

void AutonomousMissionScheduler::executeMissionNow(const QString& missionId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_missions.find(missionId);
    if (it == m_missions.end() || !(*it)->enabled) {
        return;
    }
    
    auto mission = *it;
    if (mission->isRunning) {
        qDebug() << "[MissionScheduler] Mission already running:" << missionId;
        return;
    }
    
    if (!canExecuteMission(*mission)) {
        // Queue for later execution
        if (!m_missionQueue.contains(missionId)) {
            m_missionQueue.enqueue(missionId);
        }
        return;
    }
    
    executeMissionInternal(mission.get());
}

void AutonomousMissionScheduler::processMissionQueue()
{
    if (m_paused || m_missionQueue.isEmpty()) {
        return;
    }
    
    QMutexLocker locker(&m_mutex);
    
    // Select next mission based on strategy
    AutonomousMission* mission = selectMissionByStrategy();
    
    if (mission && canExecuteMission(*mission)) {
        executeMissionInternal(mission);
    }
    
    // Collect metrics
    updateSchedulerMetrics();
    
    // Adjust adaptive scheduling if enabled
    if (m_adaptiveSchedulingEnabled) {
        adjustAdaptiveScheduling();
    }
}

void AutonomousMissionScheduler::onMetricsCollection()
{
    QMutexLocker locker(&m_mutex);
    
    updateSystemLoadMetrics();
    updateSchedulerMetrics();
    
    QJsonObject metrics = getSchedulerMetrics();
    emit metricsUpdated(metrics);
    
    // Check for high load conditions
    if (m_currentCpuUsagePercent > m_cpuLoadThreshold || 
        m_currentMemoryUsagePercent > m_memoryLoadThreshold) {
        emit systemLoadHigh(m_currentCpuUsagePercent, m_currentMemoryUsagePercent);
    }
}

void AutonomousMissionScheduler::executeMissionInternal(AutonomousMission* mission)
{
    if (!mission || !mission->enabled || mission->isRunning) {
        return;
    }
    
    mission->isRunning = true;
    m_activeTasks[mission->missionId] = nullptr;  // Placeholder for thread
    
    emit missionStarted(mission->missionId);
    qDebug() << "[MissionScheduler] Starting mission:" << mission->missionId;
    
    QElapsedTimer executionTimer;
    executionTimer.start();
    
    try {
        // Execute the mission task
        if (mission->task) {
            QJsonObject result = mission->task();
            mission->lastResult = result;
            
            qint64 durationMs = executionTimer.elapsed();
            completeMission(mission, result);
            recordMissionExecution(mission, durationMs, true);
        }
    } catch (const std::exception& e) {
        QString error = QString::fromStdString(e.what());
        failMission(mission, error);
        recordMissionExecution(mission, executionTimer.elapsed(), false);
    } catch (...) {
        failMission(mission, "Unknown exception");
        recordMissionExecution(mission, executionTimer.elapsed(), false);
    }
    
    mission->isRunning = false;
    m_activeTasks.remove(mission->missionId);
}

void AutonomousMissionScheduler::completeMission(AutonomousMission* mission, const QJsonObject& result)
{
    if (!mission) return;
    
    mission->lastExecutedAt = QDateTime::currentDateTime();
    mission->successfulExecutions++;
    mission->totalExecutions++;
    mission->currentRetries = 0;
    mission->lastError.clear();
    
    // Schedule next execution
    if (mission->type == AutonomousMission::FixedInterval && mission->intervalMs > 0) {
        mission->nextScheduledAt = QDateTime::currentDateTime().addMSecs(mission->intervalMs);
    }
    
    emit missionCompleted(mission->missionId, result);
    qInfo() << "[MissionScheduler] Mission completed:" << mission->missionId
            << "Total executions:" << mission->totalExecutions;
}

void AutonomousMissionScheduler::failMission(AutonomousMission* mission, const QString& error)
{
    if (!mission) return;
    
    mission->failedExecutions++;
    mission->totalExecutions++;
    mission->lastError = error;
    mission->lastExecutedAt = QDateTime::currentDateTime();
    
    emit missionFailed(mission->missionId, error);
    qWarning() << "[MissionScheduler] Mission failed:" << mission->missionId 
               << "Error:" << error;
    
    // Auto-retry if enabled
    if (mission->autoRetry && mission->currentRetries < mission->maxRetries) {
        retryMission(mission);
    }
}

void AutonomousMissionScheduler::retryMission(AutonomousMission* mission)
{
    if (!mission || mission->currentRetries >= mission->maxRetries) {
        return;
    }
    
    mission->currentRetries++;
    emit missionRetrying(mission->missionId, mission->currentRetries);
    
    qInfo() << "[MissionScheduler] Retrying mission:" << mission->missionId
            << "Attempt:" << mission->currentRetries;
    
    // Schedule retry after delay
    QTimer::singleShot(mission->retryDelayMs * mission->currentRetries, this, 
        [this, id = mission->missionId]() {
            executeMissionNow(id);
        });
}

void AutonomousMissionScheduler::handleTaskTimeout()
{
    QMutexLocker locker(&m_mutex);
    
    QDateTime now = QDateTime::currentDateTime();
    
    for (auto it = m_missions.begin(); it != m_missions.end(); ++it) {
        auto mission = *it;
        
        if (mission->isRunning && mission->lastExecutedAt.isValid()) {
            qint64 elapsedMs = mission->lastExecutedAt.msecsTo(now);
            
            if (elapsedMs > mission->maxDurationMs) {
                qWarning() << "[MissionScheduler] Mission timeout detected:" << mission->missionId;
                failMission(mission.get(), "Execution timeout");
                mission->isRunning = false;
                m_activeTasks.remove(mission->missionId);
                emit performanceAlert("Mission " + mission->missionId + " exceeded maximum duration");
            }
        }
    }
}

AutonomousMission* AutonomousMissionScheduler::selectMissionByStrategy()
{
    switch (m_schedulingStrategy) {
        case FIFO:
            return selectMissionFIFO();
        case PriorityBased:
            return selectMissionByPriority();
        case AdaptiveLoad:
            return selectMissionAdaptiveLoad();
        default:
            return selectMissionFIFO();
    }
}

AutonomousMission* AutonomousMissionScheduler::selectMissionFIFO()
{
    while (!m_missionQueue.isEmpty()) {
        QString missionId = m_missionQueue.dequeue();
        auto it = m_missions.find(missionId);
        if (it != m_missions.end() && (*it)->enabled) {
            return it->get();
        }
    }
    return nullptr;
}

AutonomousMission* AutonomousMissionScheduler::selectMissionByPriority()
{
    AutonomousMission* bestMission = nullptr;
    AutonomousMission::MissionPriority bestPriority = AutonomousMission::Low;
    
    for (auto& missionId : m_missionQueue) {
        auto it = m_missions.find(missionId);
        if (it != m_missions.end()) {
            auto mission = it->get();
            if (mission->enabled && (bestMission == nullptr || mission->priority > bestPriority)) {
                bestMission = mission;
                bestPriority = mission->priority;
            }
        }
    }
    
    if (bestMission) {
        m_missionQueue.removeAll(bestMission->missionId);
    }
    
    return bestMission;
}

AutonomousMission* AutonomousMissionScheduler::selectMissionAdaptiveLoad()
{
    // Under high load, prefer short-running, low-resource missions
    double loadFactor = getSystemLoadFactor();
    
    AutonomousMission* bestMission = nullptr;
    double bestScore = -1.0;
    
    for (auto& missionId : m_missionQueue) {
        auto it = m_missions.find(missionId);
        if (it != m_missions.end()) {
            auto mission = it->get();
            if (!mission->enabled) continue;
            
            // Score based on priority, resource requirements, and system load
            double priorityScore = mission->priority / 3.0;
            double resourceScore = 1.0 - (mission->estimatedMemoryMB / 256.0);  // Prefer low-resource missions
            double durationScore = 1.0 - (mission->maxDurationMs / 30000.0);   // Prefer fast missions
            
            double finalScore = (priorityScore * 0.4) + (resourceScore * 0.3) + (durationScore * 0.3);
            
            if (loadFactor > 0.7) {
                finalScore *= (1.0 - loadFactor);  // Heavily prefer low-resource missions under high load
            }
            
            if (finalScore > bestScore) {
                bestScore = finalScore;
                bestMission = mission;
            }
        }
    }
    
    if (bestMission) {
        m_missionQueue.removeAll(bestMission->missionId);
    }
    
    return bestMission;
}

void AutonomousMissionScheduler::updateSystemLoadMetrics()
{
    #ifdef _WIN32
        // Get CPU usage from Windows
        SYSTEM_INFO sysInfo;
        GetSystemInfo(&sysInfo);
        
        // Simplified CPU usage calculation (would need PDH for accurate per-processor usage)
        // For now, estimate based on active tasks
        m_currentCpuUsagePercent = (double(m_activeTasks.size()) / double(sysInfo.dwNumberOfProcessors)) * 100.0;
        
        // Get system memory status
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            m_currentMemoryUsagePercent = 100.0 * (1.0 - (double(memStatus.ullAvailPhys) / double(memStatus.ullTotalPhys)));
            
            qint64 usedMemoryMB = (memStatus.ullTotalPhys - memStatus.ullAvailPhys) / (1024 * 1024);
            m_currentMemoryUsageMB = usedMemoryMB;
        }
    #endif
}

void AutonomousMissionScheduler::adjustAdaptiveScheduling()
{
    double loadFactor = getSystemLoadFactor();
    
    for (auto& mission : m_missions) {
        if (mission->type != AutonomousMission::Adaptive) {
            continue;
        }
        
        // Adjust interval based on system load
        double baseInterval = (mission->minIntervalMs + mission->maxIntervalMs) / 2.0;
        double adaptedInterval = baseInterval;
        
        if (loadFactor < 0.3) {
            // System is idle, increase frequency
            adaptedInterval = mission->minIntervalMs;
        } else if (loadFactor < 0.6) {
            // System is moderately loaded
            adaptedInterval = baseInterval * (1.0 - loadFactor);
        } else {
            // System is heavily loaded, decrease frequency
            adaptedInterval = mission->maxIntervalMs * loadFactor;
        }
        
        int newIntervalMs = static_cast<int>(std::clamp(adaptedInterval, 
                                                        mission->minIntervalMs, 
                                                        mission->maxIntervalMs));
        
        if (newIntervalMs != mission->intervalMs) {
            mission->intervalMs = newIntervalMs;
            
            if (m_missionTimers.contains(mission->missionId)) {
                m_missionTimers[mission->missionId]->setInterval(newIntervalMs);
            }
            
            emit adaptiveSchedulingAdjusted(mission->missionId, newIntervalMs);
        }
    }
}

void AutonomousMissionScheduler::recordMissionExecution(AutonomousMission* mission, 
                                                       qint64 durationMs, bool success)
{
    if (!mission) return;
    
    // Update running average of mission duration
    if (mission->totalExecutions > 0) {
        mission->averageDurationMs = 
            (mission->averageDurationMs * (mission->totalExecutions - 1) + durationMs) / 
            mission->totalExecutions;
    } else {
        mission->averageDurationMs = durationMs;
    }
    
    // Update mission metrics
    auto& metrics = m_missionMetrics[mission->missionId];
    metrics["last_executed"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    metrics["total_executions"] = (int)mission->totalExecutions;
    metrics["successful_executions"] = (int)mission->successfulExecutions;
    metrics["failed_executions"] = (int)mission->failedExecutions;
    metrics["average_duration_ms"] = mission->averageDurationMs;
    metrics["last_result"] = mission->lastResult;
    
    if (!mission->lastError.isEmpty()) {
        metrics["last_error"] = mission->lastError;
    }
}

void AutonomousMissionScheduler::updateSchedulerMetrics()
{
    m_schedulerMetrics["last_updated"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    m_schedulerMetrics["uptime_seconds"] = m_uptimeTimer.elapsed() / 1000;
    m_schedulerMetrics["active_tasks"] = m_activeTasks.size();
    m_schedulerMetrics["pending_missions"] = m_missionQueue.size();
    
    // Calculate aggregate metrics
    qint64 totalExecutions = 0;
    qint64 totalSuccessful = 0;
    double totalAverageDuration = 0.0;
    
    for (const auto& mission : m_missions) {
        totalExecutions += mission->totalExecutions;
        totalSuccessful += mission->successfulExecutions;
        totalAverageDuration += mission->averageDurationMs;
    }
    
    m_schedulerMetrics["total_missions_executed"] = (int)totalExecutions;
    m_schedulerMetrics["total_successful_executions"] = (int)totalSuccessful;
    m_schedulerMetrics["success_rate_percent"] = 
        totalExecutions > 0 ? (100.0 * totalSuccessful / totalExecutions) : 0.0;
}

void AutonomousMissionScheduler::logMissionEvent(const QString& missionId, 
                                                 const QString& eventType, 
                                                 const QString& message)
{
    qDebug() << "[MissionScheduler]" << missionId << eventType << message;
}

AutonomousMission* AutonomousMissionScheduler::getMission(const QString& missionId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = m_missions.find(missionId);
    if (it != m_missions.end()) {
        return it->get();
    }
    
    return nullptr;
}

QList<AutonomousMission*> AutonomousMissionScheduler::getActiveMissions() const
{
    QMutexLocker locker(&m_mutex);
    
    QList<AutonomousMission*> active;
    for (const auto& mission : m_missions) {
        if (mission->isRunning) {
            active.append(mission.get());
        }
    }
    
    return active;
}

QList<AutonomousMission*> AutonomousMissionScheduler::getPendingMissions() const
{
    QMutexLocker locker(&m_mutex);
    
    QList<AutonomousMission*> pending;
    for (const auto& missionId : m_missionQueue) {
        auto it = m_missions.find(missionId);
        if (it != m_missions.end()) {
            pending.append(it->get());
        }
    }
    
    return pending;
}

// Slot implementations
void AutonomousMissionScheduler::onSchedulingCycle()
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "[MissionScheduler] Scheduling cycle triggered";
    // Scheduling logic is handled by the timer in the main scheduler loop
}

void AutonomousMissionScheduler::onSystemLoadChanged(double cpuPercent, double memoryPercent)
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "[MissionScheduler] System load changed - CPU:" << cpuPercent << "% Memory:" << memoryPercent << "%";
    m_currentLoad = (cpuPercent + memoryPercent) / 2.0;
}

void AutonomousMissionScheduler::onTaskCompleted(const QString& missionId, const QJsonObject& result)
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "[MissionScheduler] Task completed:" << missionId;
    
    auto it = m_missions.find(missionId);
    if (it != m_missions.end()) {
        auto& mission = *it;
        mission->isRunning = false;
        mission->lastExecuted = QDateTime::currentDateTime();
        mission->executionCount++;
    }
}

void AutonomousMissionScheduler::onTaskFailed(const QString& missionId, const QString& errorMessage)
{
    QMutexLocker locker(&m_mutex);
    qDebug() << "[MissionScheduler] Task failed:" << missionId << "Error:" << errorMessage;
    
    auto it = m_missions.find(missionId);
    if (it != m_missions.end()) {
        auto& mission = *it;
        mission->isRunning = false;
        mission->failureCount++;
        mission->lastError = errorMessage;
    }
}

void AutonomousMissionScheduler::collectSystemMetrics()
{
    // Collect current system metrics
    qDebug() << "[MissionScheduler] Collecting system metrics";
    // Metrics collection would happen here
}
