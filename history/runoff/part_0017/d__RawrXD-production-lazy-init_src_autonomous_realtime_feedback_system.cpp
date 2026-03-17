// autonomous_realtime_feedback_system.cpp - Real-time feedback for autonomous execution
#include "autonomous_realtime_feedback_system.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <algorithm>

AutonomousRealtimeFeedbackSystem::AutonomousRealtimeFeedbackSystem(QObject* parent)
    : QObject(parent)
{
    connect(&m_updateTimer, &QTimer::timeout, this, &AutonomousRealtimeFeedbackSystem::onLiveUpdateTimer);
    
    qInfo() << "[AutonomousRealtimeFeedbackSystem] Initialized";
}

AutonomousRealtimeFeedbackSystem::~AutonomousRealtimeFeedbackSystem()
{
    stopLiveUpdates();
}

// ========== REAL-TIME UPDATES ==========

void AutonomousRealtimeFeedbackSystem::updateExecutionProgress(const QString& taskId, int current, int total)
{
    if (!m_taskProgress.contains(taskId)) {
        TaskProgress progress;
        progress.taskId = taskId;
        progress.startTime = QDateTime::currentDateTime();
        progress.status = "running";
        m_taskProgress[taskId] = progress;
    }
    
    TaskProgress& progress = m_taskProgress[taskId];
    progress.currentStep = current;
    progress.totalSteps = total;
    progress.lastUpdateTime = QDateTime::currentDateTime();
    
    emit progressUpdated(taskId, current, total);
    
    // Calculate ETA
    if (current > 0) {
        qint64 elapsedMs = progress.startTime.msecsTo(progress.lastUpdateTime);
        qint64 estimatedTotalMs = (elapsedMs * total) / current;
        qint64 remainingMs = estimatedTotalMs - elapsedMs;
        
        qDebug() << "[AutonomousRealtimeFeedbackSystem] Task" << taskId << ":"
                 << current << "/" << total
                 << "- ETA:" << (remainingMs / 1000) << "seconds";
    }
}

void AutonomousRealtimeFeedbackSystem::updateAgentStatus(const QString& agentId, const QString& status, const QJsonObject& details)
{
    if (!m_agentStatus.contains(agentId)) {
        AgentStatusInfo info;
        info.agentId = agentId;
        m_agentStatus[agentId] = info;
    }
    
    AgentStatusInfo& info = m_agentStatus[agentId];
    info.currentStatus = status;
    info.details = details;
    info.lastUpdated = QDateTime::currentDateTime();
    
    // Extract resource usage if available
    if (details.contains("cpu_usage")) {
        info.cpuUsage = details["cpu_usage"].toDouble();
    }
    if (details.contains("memory_usage")) {
        info.memoryUsage = details["memory_usage"].toDouble();
    }
    
    emit statusChanged(agentId, status);
    
    qInfo() << "[AutonomousRealtimeFeedbackSystem] Agent" << agentId << "status:" << status;
}

void AutonomousRealtimeFeedbackSystem::updateMetrics(const QString& metricName, double value)
{
    if (!m_metricHistory.contains(metricName)) {
        m_metricHistory[metricName] = QVector<double>();
    }
    
    m_metricHistory[metricName].append(value);
    
    // Keep last 100 data points
    if (m_metricHistory[metricName].size() > 100) {
        m_metricHistory[metricName].removeFirst();
    }
    
    emit metricsUpdated(metricName, value);
}

void AutonomousRealtimeFeedbackSystem::displayTaskResult(const QString& taskId, const QJsonObject& result)
{
    qInfo() << "[AutonomousRealtimeFeedbackSystem] Displaying result for task:" << taskId;
    
    if (m_taskProgress.contains(taskId)) {
        m_taskProgress[taskId].status = "completed";
    }
    
    emit resultDisplayed(taskId, result);
}

// ========== LIVE VISUALIZATION ==========

QJsonObject AutonomousRealtimeFeedbackSystem::getExecutionDashboard() const
{
    QJsonObject dashboard;
    dashboard["type"] = "execution";
    dashboard["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray tasksArray;
    for (auto it = m_taskProgress.begin(); it != m_taskProgress.end(); ++it) {
        const TaskProgress& progress = it.value();
        
        QJsonObject taskObj;
        taskObj["task_id"] = progress.taskId;
        taskObj["current_step"] = progress.currentStep;
        taskObj["total_steps"] = progress.totalSteps;
        taskObj["progress_percent"] = (progress.totalSteps > 0) ? 
            (100.0 * progress.currentStep / progress.totalSteps) : 0.0;
        taskObj["status"] = progress.status;
        taskObj["current_action"] = progress.currentAction;
        
        tasksArray.append(taskObj);
    }
    
    dashboard["tasks"] = tasksArray;
    dashboard["active_tasks"] = tasksArray.size();
    
    return dashboard;
}

QJsonObject AutonomousRealtimeFeedbackSystem::getPerformanceDashboard() const
{
    QJsonObject dashboard;
    dashboard["type"] = "performance";
    dashboard["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonObject metrics;
    for (auto it = m_metricHistory.begin(); it != m_metricHistory.end(); ++it) {
        const QVector<double>& values = it.value();
        if (!values.isEmpty()) {
            double sum = 0.0;
            for (double v : values) {
                sum += v;
            }
            double average = sum / values.size();
            
            QJsonObject metricObj;
            metricObj["current"] = values.last();
            metricObj["average"] = average;
            metricObj["min"] = *std::min_element(values.begin(), values.end());
            metricObj["max"] = *std::max_element(values.begin(), values.end());
            metricObj["data_points"] = values.size();
            
            metrics[it.key()] = metricObj;
        }
    }
    
    dashboard["metrics"] = metrics;
    
    return dashboard;
}

QJsonObject AutonomousRealtimeFeedbackSystem::getHealthDashboard() const
{
    QJsonObject dashboard;
    dashboard["type"] = "health";
    dashboard["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray agentsArray;
    for (auto it = m_agentStatus.begin(); it != m_agentStatus.end(); ++it) {
        const AgentStatusInfo& info = it.value();
        
        QJsonObject agentObj;
        agentObj["agent_id"] = info.agentId;
        agentObj["status"] = info.currentStatus;
        agentObj["cpu_usage_percent"] = info.cpuUsage;
        agentObj["memory_usage_percent"] = info.memoryUsage;
        agentObj["last_updated"] = info.lastUpdated.toString(Qt::ISODate);
        
        agentsArray.append(agentObj);
    }
    
    dashboard["agents"] = agentsArray;
    dashboard["total_agents"] = agentsArray.size();
    
    return dashboard;
}

QJsonObject AutonomousRealtimeFeedbackSystem::getAgentStatus(const QString& agentId) const
{
    if (m_agentStatus.contains(agentId)) {
        const AgentStatusInfo& info = m_agentStatus[agentId];
        
        QJsonObject status;
        status["agent_id"] = agentId;
        status["current_status"] = info.currentStatus;
        status["details"] = info.details;
        status["cpu_usage"] = info.cpuUsage;
        status["memory_usage"] = info.memoryUsage;
        status["last_updated"] = info.lastUpdated.toString(Qt::ISODate);
        
        return status;
    }
    
    return QJsonObject{{"error", "Agent not found"}};
}

// ========== STREAMING UPDATES ==========

void AutonomousRealtimeFeedbackSystem::startLiveUpdates(int intervalMs)
{
    m_updateTimer.setInterval(intervalMs);
    m_updateTimer.start();
    
    qInfo() << "[AutonomousRealtimeFeedbackSystem] Live updates started, interval:" << intervalMs << "ms";
    emit liveUpdatesStarted();
}

void AutonomousRealtimeFeedbackSystem::stopLiveUpdates()
{
    m_updateTimer.stop();
    
    qInfo() << "[AutonomousRealtimeFeedbackSystem] Live updates stopped";
    emit liveUpdatesStopped();
}

bool AutonomousRealtimeFeedbackSystem::isLiveUpdating() const
{
    return m_updateTimer.isActive();
}

// ========== ALERT DISPLAY ==========

void AutonomousRealtimeFeedbackSystem::displayAlert(const QString& severity, const QString& message, const QJsonObject& details)
{
    qWarning() << "[AutonomousRealtimeFeedbackSystem] Alert [" << severity << "]:" << message;
    
    emit alertDisplayed(severity, message);
}

void AutonomousRealtimeFeedbackSystem::displayNotification(const QString& message, const QString& type)
{
    qInfo() << "[AutonomousRealtimeFeedbackSystem] Notification [" << type << "]:" << message;
    
    emit notificationDisplayed(message);
}

// ========== LOGGING VIEW ==========

void AutonomousRealtimeFeedbackSystem::addLogEntry(const QString& level, const QString& component, const QString& message)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.component = component;
    entry.message = message;
    entry.displayColor = (level == "ERROR" || level == "CRITICAL") ? "#ff0000" :
                        (level == "WARNING") ? "#ffaa00" :
                        (level == "DEBUG") ? "#0088ff" : "#00aa00";
    
    m_logHistory.append(entry);
    
    // Keep only recent entries
    if (m_logHistory.size() > m_maxDisplayItems) {
        m_logHistory.removeFirst();
    }
}

QJsonArray AutonomousRealtimeFeedbackSystem::getRecentLogs(int count) const
{
    QJsonArray logs;
    
    int startIdx = std::max(0, static_cast<int>(m_logHistory.size()) - count);
    for (int i = startIdx; i < m_logHistory.size(); ++i) {
        const LogEntry& entry = m_logHistory[i];
        
        QJsonObject logObj;
        logObj["timestamp"] = entry.timestamp.toString(Qt::ISODate);
        logObj["level"] = entry.level;
        logObj["component"] = entry.component;
        logObj["message"] = entry.message;
        logObj["color"] = entry.displayColor;
        
        logs.append(logObj);
    }
    
    return logs;
}

// ========== ANALYTICS ==========

QJsonObject AutonomousRealtimeFeedbackSystem::generateExecutionSummary() const
{
    QJsonObject summary;
    summary["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    int totalTasks = m_taskProgress.size();
    int completedTasks = 0;
    qint64 totalDuration = 0;
    
    for (auto it = m_taskProgress.begin(); it != m_taskProgress.end(); ++it) {
        const TaskProgress& progress = it.value();
        if (progress.status == "completed") {
            completedTasks++;
            totalDuration += progress.startTime.msecsTo(progress.lastUpdateTime);
        }
    }
    
    summary["total_tasks"] = totalTasks;
    summary["completed_tasks"] = completedTasks;
    summary["completion_rate_percent"] = (totalTasks > 0) ? (100.0 * completedTasks / totalTasks) : 0.0;
    summary["total_execution_time_ms"] = static_cast<int>(totalDuration);
    summary["active_agents"] = static_cast<int>(m_agentStatus.size());
    summary["total_log_entries"] = static_cast<int>(m_logHistory.size());
    
    return summary;
}

QJsonArray AutonomousRealtimeFeedbackSystem::getTaskHistory() const
{
    QJsonArray history;
    
    for (auto it = m_taskProgress.begin(); it != m_taskProgress.end(); ++it) {
        const TaskProgress& progress = it.value();
        
        QJsonObject taskObj;
        taskObj["task_id"] = progress.taskId;
        taskObj["status"] = progress.status;
        taskObj["start_time"] = progress.startTime.toString(Qt::ISODate);
        taskObj["last_update"] = progress.lastUpdateTime.toString(Qt::ISODate);
        taskObj["progress"] = (progress.totalSteps > 0) ? 
            (100.0 * progress.currentStep / progress.totalSteps) : 0.0;
        
        history.append(taskObj);
    }
    
    return history;
}

QString AutonomousRealtimeFeedbackSystem::generateVisualizationData() const
{
    QJsonObject vizData;
    
    vizData["dashboard"] = getExecutionDashboard();
    vizData["performance"] = getPerformanceDashboard();
    vizData["health"] = getHealthDashboard();
    vizData["recent_logs"] = getRecentLogs(20);
    vizData["summary"] = generateExecutionSummary();
    
    return QJsonDocument(vizData).toJson(QJsonDocument::Indented);
}

// ========== CONFIGURATION ==========

void AutonomousRealtimeFeedbackSystem::setUpdateFrequency(int frequencyMs)
{
    m_updateFrequencyMs = frequencyMs;
    if (m_updateTimer.isActive()) {
        m_updateTimer.setInterval(frequencyMs);
    }
    
    qInfo() << "[AutonomousRealtimeFeedbackSystem] Update frequency set to:" << frequencyMs << "ms";
}

void AutonomousRealtimeFeedbackSystem::enableAutoScroll(bool enable)
{
    m_autoScroll = enable;
}

void AutonomousRealtimeFeedbackSystem::setMaxDisplayItems(int max)
{
    m_maxDisplayItems = max;
}

void AutonomousRealtimeFeedbackSystem::setTheme(const QString& theme)
{
    m_currentTheme = theme;
    qInfo() << "[AutonomousRealtimeFeedbackSystem] Theme set to:" << theme;
}

// ========== PRIVATE SLOTS ==========

void AutonomousRealtimeFeedbackSystem::onLiveUpdateTimer()
{
    updateDashboard();
    refreshMetricsDisplay();
    refreshAgentStatusDisplay();
}

void AutonomousRealtimeFeedbackSystem::updateDashboard()
{
    m_cachedDashboard = getExecutionDashboard();
    m_lastDashboardUpdate = QDateTime::currentDateTime();
    
    emit dashboardRefreshed("execution");
}

void AutonomousRealtimeFeedbackSystem::refreshMetricsDisplay()
{
    // Metrics already updated in updateMetrics()
}

void AutonomousRealtimeFeedbackSystem::refreshAgentStatusDisplay()
{
    // Status already updated in updateAgentStatus()
}

// ========== HELPER METHODS ==========

QString AutonomousRealtimeFeedbackSystem::formatMetricValue(const QString& metricName, double value)
{
    if (metricName.contains("percent") || metricName.contains("rate")) {
        return QString::number(value, 'f', 2) + "%";
    } else if (metricName.contains("ms") || metricName.contains("time")) {
        return QString::number(value, 'f', 0) + "ms";
    } else if (metricName.contains("memory") || metricName.contains("size")) {
        return QString::number(value / 1024, 'f', 2) + "MB";
    }
    
    return QString::number(value, 'f', 2);
}

QString AutonomousRealtimeFeedbackSystem::getStatusColor(const QString& status)
{
    if (status == "running") {
        return "#00aa00";  // Green
    } else if (status == "completed") {
        return "#00ff00";  // Bright green
    } else if (status == "error") {
        return "#ff0000";  // Red
    } else if (status == "warning") {
        return "#ffaa00";  // Orange
    }
    
    return "#ffffff";  // White
}

QJsonObject AutonomousRealtimeFeedbackSystem::buildExecutionGraph() const
{
    QJsonObject graph;
    graph["type"] = "execution_timeline";
    
    QJsonArray nodes;
    for (auto it = m_taskProgress.begin(); it != m_taskProgress.end(); ++it) {
        const TaskProgress& progress = it.value();
        
        QJsonObject node;
        node["id"] = progress.taskId;
        node["label"] = progress.currentAction;
        node["status"] = progress.status;
        node["progress"] = (progress.totalSteps > 0) ? 
            (100.0 * progress.currentStep / progress.totalSteps) : 0.0;
        
        nodes.append(node);
    }
    
    graph["nodes"] = nodes;
    
    return graph;
}

QJsonObject AutonomousRealtimeFeedbackSystem::buildPerformanceMetrics() const
{
    QJsonObject metrics;
    
    for (auto it = m_metricHistory.begin(); it != m_metricHistory.end(); ++it) {
        const QVector<double>& values = it.value();
        
        if (!values.isEmpty()) {
            QJsonArray dataArray;
            for (double val : values) {
                dataArray.append(val);
            }
            
            metrics[it.key()] = dataArray;
        }
    }
    
    return metrics;
}
