// autonomous_observability_system.cpp - Production observability for autonomous systems
#include "autonomous_observability_system.h"
#include <QDateTime>
#include <QDebug>
#include <QJsonDocument>
#include <QUuid>
#include <algorithm>
#include <cmath>

AutonomousObservabilitySystem::AutonomousObservabilitySystem(QObject* parent)
    : QObject(parent)
{
    // Set up timers
    connect(&m_metricsTimer, &QTimer::timeout, this, &AutonomousObservabilitySystem::onMetricsAggregationTimer);
    connect(&m_healthCheckTimer, &QTimer::timeout, this, &AutonomousObservabilitySystem::onHealthCheckTimer);
    
    m_metricsTimer.setInterval(m_metricsAggregationIntervalMs);
    m_healthCheckTimer.setInterval(10000);  // 10 seconds
    
    m_metricsTimer.start();
    m_healthCheckTimer.start();
    
    qInfo() << "[AutonomousObservabilitySystem] Initialized";
}

AutonomousObservabilitySystem::~AutonomousObservabilitySystem()
{
    m_metricsTimer.stop();
    m_healthCheckTimer.stop();
}

// ========== STRUCTURED LOGGING ==========

void AutonomousObservabilitySystem::logDebug(const QString& component, const QString& message, const QJsonObject& context)
{
    if (m_currentLogLevel != "DEBUG") return;
    
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "DEBUG";
    entry.component = component;
    entry.message = message;
    entry.context = context;
    entry.traceId = "";
    
    m_logHistory.append(entry);
    processLogEntry(entry);
    
    emit logEmitted(entry.level, entry.component, entry.message);
}

void AutonomousObservabilitySystem::logInfo(const QString& component, const QString& message, const QJsonObject& context)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "INFO";
    entry.component = component;
    entry.message = message;
    entry.context = context;
    entry.traceId = "";
    
    m_logHistory.append(entry);
    processLogEntry(entry);
    
    emit logEmitted(entry.level, entry.component, entry.message);
}

void AutonomousObservabilitySystem::logWarning(const QString& component, const QString& message, const QJsonObject& context)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "WARNING";
    entry.component = component;
    entry.message = message;
    entry.context = context;
    entry.traceId = "";
    
    m_logHistory.append(entry);
    processLogEntry(entry);
    
    emit logEmitted(entry.level, entry.component, entry.message);
    
    // Warnings may trigger health degradation
    m_healthScore = std::max(0.0, m_healthScore - 0.02);
}

void AutonomousObservabilitySystem::logError(const QString& component, const QString& message, const QJsonObject& context)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "ERROR";
    entry.component = component;
    entry.message = message;
    entry.context = context;
    entry.traceId = "";
    
    m_logHistory.append(entry);
    processLogEntry(entry);
    
    emit logEmitted(entry.level, entry.component, entry.message);
    
    // Errors degrade health
    m_healthScore = std::max(0.0, m_healthScore - 0.05);
}

void AutonomousObservabilitySystem::logCritical(const QString& component, const QString& message, const QJsonObject& context)
{
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = "CRITICAL";
    entry.component = component;
    entry.message = message;
    entry.context = context;
    entry.traceId = "";
    
    m_logHistory.append(entry);
    processLogEntry(entry);
    
    emit logEmitted(entry.level, entry.component, entry.message);
    
    // Critical errors degrade health significantly
    m_healthScore = std::max(0.0, m_healthScore - 0.1);
    m_currentHealthStatus = "degraded";
}

// ========== DISTRIBUTED TRACING ==========

QString AutonomousObservabilitySystem::startTrace(const QString& operationName, const QJsonObject& metadata)
{
    QString traceId = QUuid::createUuid().toString();
    
    TraceRecord trace;
    trace.traceId = traceId;
    trace.operationName = operationName;
    trace.startTime = QDateTime::currentDateTime();
    trace.metadata = metadata;
    
    m_activeTraces[traceId] = trace;
    
    emit traceCreated(traceId, operationName);
    
    if (m_distributedTracingEnabled) {
        logInfo("Tracing", "Trace started: " + operationName, QJsonObject{{"traceId", traceId}});
    }
    
    return traceId;
}

void AutonomousObservabilitySystem::recordTraceEvent(const QString& traceId, const QString& eventName, const QJsonObject& eventData)
{
    if (!m_activeTraces.contains(traceId)) {
        qWarning() << "[AutonomousObservabilitySystem] Trace not found:" << traceId;
        return;
    }
    
    QJsonObject event;
    event["name"] = eventName;
    event["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    event["data"] = eventData;
    
    m_activeTraces[traceId].events.append(event);
}

QJsonObject AutonomousObservabilitySystem::endTrace(const QString& traceId)
{
    if (!m_activeTraces.contains(traceId)) {
        qWarning() << "[AutonomousObservabilitySystem] Trace not found:" << traceId;
        return QJsonObject();
    }
    
    TraceRecord& trace = m_activeTraces[traceId];
    trace.endTime = QDateTime::currentDateTime();
    trace.durationMs = trace.startTime.msecsTo(trace.endTime);
    
    QJsonObject result;
    result["traceId"] = traceId;
    result["operationName"] = trace.operationName;
    result["durationMs"] = static_cast<int>(trace.durationMs);
    result["eventCount"] = trace.events.size();
    
    // Move to completed traces
    m_completedTraces.append(trace);
    m_activeTraces.remove(traceId);
    
    if (m_distributedTracingEnabled) {
        logInfo("Tracing", "Trace completed: " + trace.operationName, 
                QJsonObject{{"traceId", traceId}, {"durationMs", static_cast<int>(trace.durationMs)}});
    }
    
    return result;
}

QJsonArray AutonomousObservabilitySystem::getTraceHistory(const QString& operationName) const
{
    QJsonArray history;
    
    for (const auto& trace : m_completedTraces) {
        if (trace.operationName == operationName) {
            QJsonObject traceInfo;
            traceInfo["traceId"] = trace.traceId;
            traceInfo["timestamp"] = trace.startTime.toString(Qt::ISODate);
            traceInfo["durationMs"] = static_cast<int>(trace.durationMs);
            traceInfo["eventCount"] = trace.events.size();
            history.append(traceInfo);
        }
    }
    
    return history;
}

// ========== METRICS COLLECTION ==========

void AutonomousObservabilitySystem::recordMetric(const QString& metricName, double value, const QJsonObject& labels)
{
    MetricValue metric;
    metric.metricName = metricName;
    metric.value = value;
    metric.timestamp = QDateTime::currentDateTime();
    metric.labels = labels;
    metric.metricType = "gauge";
    
    m_metricsBuffer.append(metric);
    m_currentMetrics[metricName] = value;
    
    emit metricRecorded(metricName, value);
    
    // Check for alerts
    for (const auto& threshold : m_alertThresholds) {
        if (threshold.metricName == metricName && value > threshold.threshold) {
            Alert alert;
            alert.alertId = QUuid::createUuid().toString();
            alert.metricName = metricName;
            alert.severity = threshold.severity;
            alert.message = "Metric " + metricName + " exceeded threshold: " + QString::number(value);
            alert.triggeredAt = QDateTime::currentDateTime();
            
            m_activeAlerts.append(alert);
            emit alertTriggered(alert.alertId, alert.severity, alert.message);
        }
    }
}

void AutonomousObservabilitySystem::recordHistogram(const QString& metricName, double value)
{
    MetricValue metric;
    metric.metricName = metricName;
    metric.value = value;
    metric.timestamp = QDateTime::currentDateTime();
    metric.metricType = "histogram";
    
    m_metricsBuffer.append(metric);
}

void AutonomousObservabilitySystem::incrementCounter(const QString& metricName, int increment)
{
    double currentValue = m_currentMetrics.value(metricName, 0.0);
    recordMetric(metricName, currentValue + increment);
}

void AutonomousObservabilitySystem::recordGauge(const QString& metricName, double value)
{
    recordMetric(metricName, value);
}

// ========== PERFORMANCE PROFILING ==========

void AutonomousObservabilitySystem::startProfiling(const QString& functionName)
{
    m_profilingStartTimes[functionName] = QDateTime::currentMSecsSinceEpoch();
}

void AutonomousObservabilitySystem::endProfiling(const QString& functionName)
{
    if (!m_profilingStartTimes.contains(functionName)) {
        qWarning() << "[AutonomousObservabilitySystem] Profiling not started for:" << functionName;
        return;
    }
    
    qint64 duration = QDateTime::currentMSecsSinceEpoch() - m_profilingStartTimes[functionName];
    m_profilingStartTimes.remove(functionName);
    
    if (!m_performanceProfiles.contains(functionName)) {
        m_performanceProfiles[functionName] = PerformanceProfile();
    }
    
    PerformanceProfile& profile = m_performanceProfiles[functionName];
    profile.functionName = functionName;
    profile.callCount++;
    profile.totalDurationMs += duration;
    profile.minDurationMs = std::min(profile.minDurationMs, duration);
    profile.maxDurationMs = std::max(profile.maxDurationMs, duration);
    profile.lastCalled = QDateTime::currentDateTime();
    
    recordMetric("performance_" + functionName + "_duration_ms", static_cast<double>(duration));
}

QJsonObject AutonomousObservabilitySystem::getProfilingReport() const
{
    QJsonObject report;
    
    for (auto it = m_performanceProfiles.begin(); it != m_performanceProfiles.end(); ++it) {
        const PerformanceProfile& profile = it.value();
        
        QJsonObject profileInfo;
        profileInfo["call_count"] = static_cast<int>(profile.callCount);
        profileInfo["total_duration_ms"] = static_cast<int>(profile.totalDurationMs);
        profileInfo["average_duration_ms"] = static_cast<double>(profile.totalDurationMs) / profile.callCount;
        profileInfo["min_duration_ms"] = static_cast<int>(profile.minDurationMs);
        profileInfo["max_duration_ms"] = static_cast<int>(profile.maxDurationMs);
        profileInfo["last_called"] = profile.lastCalled.toString(Qt::ISODate);
        
        report[profile.functionName] = profileInfo;
    }
    
    return report;
}

QString AutonomousObservabilitySystem::identifyPerformanceBottlenecks() const
{
    QString bottlenecks;
    
    // Find functions with high average duration
    for (auto it = m_performanceProfiles.begin(); it != m_performanceProfiles.end(); ++it) {
        const PerformanceProfile& profile = it.value();
        double avgDuration = static_cast<double>(profile.totalDurationMs) / profile.callCount;
        
        if (avgDuration > 1000) {  // More than 1 second
            bottlenecks += profile.functionName + ": " + 
                          QString::number(avgDuration, 'f', 2) + "ms average\n";
        }
    }
    
    return bottlenecks.isEmpty() ? "No significant bottlenecks detected" : bottlenecks;
}

// ========== HEALTH MONITORING ==========

QJsonObject AutonomousObservabilitySystem::getSystemHealth() const
{
    QJsonObject health;
    health["status"] = m_currentHealthStatus;
    health["score"] = m_healthScore;
    health["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    health["active_traces"] = static_cast<int>(m_activeTraces.size());
    health["active_alerts"] = static_cast<int>(m_activeAlerts.size());
    
    return health;
}

QJsonObject AutonomousObservabilitySystem::getComponentHealth(const QString& componentName) const
{
    QJsonObject health;
    health["component"] = componentName;
    health["status"] = "healthy";  // Would track per-component
    health["last_activity"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return health;
}

double AutonomousObservabilitySystem::getHealthScore() const
{
    return m_healthScore;
}

// ========== ANOMALY DETECTION ==========

bool AutonomousObservabilitySystem::detectAnomaly(const QString& metricName, double value)
{
    // Simple statistical anomaly detection
    double mean = 0.0;
    double sampleCount = 0.0;
    
    for (const auto& metric : m_metricsBuffer) {
        if (metric.metricName == metricName) {
            mean += metric.value;
            sampleCount++;
        }
    }
    
    if (sampleCount == 0) return false;
    
    mean /= sampleCount;
    
    // Calculate standard deviation
    double variance = 0.0;
    for (const auto& metric : m_metricsBuffer) {
        if (metric.metricName == metricName) {
            double diff = metric.value - mean;
            variance += diff * diff;
        }
    }
    
    double stdDev = std::sqrt(variance / sampleCount);
    
    // Detect if value is >3 standard deviations from mean
    if (std::abs(value - mean) > 3 * stdDev) {
        Anomaly anomaly;
        anomaly.metricName = metricName;
        anomaly.expectedValue = mean;
        anomaly.actualValue = value;
        anomaly.deviationPercent = (stdDev > 0) ? (100.0 * std::abs(value - mean) / stdDev) : 0.0;
        anomaly.detectedAt = QDateTime::currentDateTime();
        
        m_detectedAnomalies.append(anomaly);
        emit anomalyDetected(metricName, QJsonObject{
            {"expected", mean},
            {"actual", value},
            {"deviation_percent", anomaly.deviationPercent}
        });
        
        return true;
    }
    
    return false;
}

QJsonArray AutonomousObservabilitySystem::getDetectedAnomalies() const
{
    QJsonArray anomalies;
    
    for (const auto& anomaly : m_detectedAnomalies) {
        QJsonObject anomObj;
        anomObj["metric_name"] = anomaly.metricName;
        anomObj["expected_value"] = anomaly.expectedValue;
        anomObj["actual_value"] = anomaly.actualValue;
        anomObj["deviation_percent"] = anomaly.deviationPercent;
        anomObj["detected_at"] = anomaly.detectedAt.toString(Qt::ISODate);
        
        anomalies.append(anomObj);
    }
    
    return anomalies;
}

void AutonomousObservabilitySystem::clearAnomalies()
{
    m_detectedAnomalies.clear();
}

// ========== ALERTING ==========

void AutonomousObservabilitySystem::setAlertThreshold(const QString& metricName, double threshold, const QString& severity)
{
    Alert alert;
    alert.alertId = metricName + "_threshold";
    alert.metricName = metricName;
    alert.threshold = threshold;
    alert.severity = severity;
    
    m_alertThresholds.append(alert);
}

QJsonArray AutonomousObservabilitySystem::getActiveAlerts() const
{
    QJsonArray alerts;
    
    for (const auto& alert : m_activeAlerts) {
        if (!alert.acknowledged) {
            QJsonObject alertObj;
            alertObj["alert_id"] = alert.alertId;
            alertObj["metric_name"] = alert.metricName;
            alertObj["severity"] = alert.severity;
            alertObj["message"] = alert.message;
            alertObj["triggered_at"] = alert.triggeredAt.toString(Qt::ISODate);
            
            alerts.append(alertObj);
        }
    }
    
    return alerts;
}

void AutonomousObservabilitySystem::acknowledgeAlert(const QString& alertId)
{
    for (auto& alert : m_activeAlerts) {
        if (alert.alertId == alertId) {
            alert.acknowledged = true;
            logInfo("Alerts", "Alert acknowledged: " + alertId);
            return;
        }
    }
}

// ========== CONFIGURATION ==========

void AutonomousObservabilitySystem::setLogLevel(const QString& level)
{
    m_currentLogLevel = level;
    qInfo() << "[AutonomousObservabilitySystem] Log level set to:" << level;
}

void AutonomousObservabilitySystem::enableDetailedMetrics(bool enable)
{
    m_detailedMetricsEnabled = enable;
    qInfo() << "[AutonomousObservabilitySystem] Detailed metrics" << (enable ? "enabled" : "disabled");
}

void AutonomousObservabilitySystem::enableDistributedTracing(bool enable)
{
    m_distributedTracingEnabled = enable;
    qInfo() << "[AutonomousObservabilitySystem] Distributed tracing" << (enable ? "enabled" : "disabled");
}

void AutonomousObservabilitySystem::setMetricsAggregationInterval(int intervalMs)
{
    m_metricsAggregationIntervalMs = intervalMs;
    m_metricsTimer.setInterval(intervalMs);
}

// ========== EXPORT AND REPORTING ==========

QString AutonomousObservabilitySystem::exportMetricsAsPrometheus() const
{
    QString prometheus;
    
    // Export in Prometheus text format
    for (auto it = m_currentMetrics.begin(); it != m_currentMetrics.end(); ++it) {
        prometheus += it.key() + " " + QString::number(it.value()) + "\n";
    }
    
    return prometheus;
}

QString AutonomousObservabilitySystem::exportTracesAsJSON() const
{
    QJsonArray tracesArray;
    
    for (const auto& trace : m_completedTraces) {
        QJsonObject traceObj;
        traceObj["traceId"] = trace.traceId;
        traceObj["operationName"] = trace.operationName;
        traceObj["durationMs"] = static_cast<int>(trace.durationMs);
        traceObj["events"] = trace.events;
        
        tracesArray.append(traceObj);
    }
    
    return QJsonDocument(tracesArray).toJson(QJsonDocument::Indented);
}

QString AutonomousObservabilitySystem::generateHealthReport() const
{
    QString report;
    report += "=== SYSTEM HEALTH REPORT ===\n";
    report += "Status: " + m_currentHealthStatus + "\n";
    report += "Score: " + QString::number(m_healthScore, 'f', 2) + "\n";
    report += "Timestamp: " + QDateTime::currentDateTime().toString(Qt::ISODate) + "\n\n";
    
    report += "Active Traces: " + QString::number(m_activeTraces.size()) + "\n";
    report += "Active Alerts: " + QString::number(m_activeAlerts.size()) + "\n";
    report += "Detected Anomalies: " + QString::number(m_detectedAnomalies.size()) + "\n";
    
    return report;
}

QString AutonomousObservabilitySystem::generatePerformanceReport() const
{
    QString report;
    report += "=== PERFORMANCE REPORT ===\n";
    report += identifyPerformanceBottlenecks();
    report += "\n";
    
    for (auto it = m_performanceProfiles.begin(); it != m_performanceProfiles.end(); ++it) {
        const PerformanceProfile& profile = it.value();
        double avgDuration = static_cast<double>(profile.totalDurationMs) / profile.callCount;
        
        report += profile.functionName + ":\n";
        report += "  Calls: " + QString::number(profile.callCount) + "\n";
        report += "  Avg: " + QString::number(avgDuration, 'f', 2) + "ms\n";
        report += "  Min/Max: " + QString::number(profile.minDurationMs) + "/" + 
                 QString::number(profile.maxDurationMs) + "ms\n";
    }
    
    return report;
}

QString AutonomousObservabilitySystem::generateAuditLog() const
{
    QString log;
    log += "=== AUDIT LOG ===\n";
    log += "Total Log Entries: " + QString::number(m_logHistory.size()) + "\n\n";
    
    // Show last 20 entries
    int start = std::max(0, static_cast<int>(m_logHistory.size()) - 20);
    for (int i = start; i < m_logHistory.size(); ++i) {
        const LogEntry& entry = m_logHistory[i];
        log += "[" + entry.timestamp.toString(Qt::ISODate) + "] [" + entry.level + "] " +
               entry.component + ": " + entry.message + "\n";
    }
    
    return log;
}

// ========== PRIVATE SLOTS AND HELPERS ==========

void AutonomousObservabilitySystem::processLogEntry(const LogEntry& entry)
{
    // Process log entry for any special handling
    if (entry.level == "ERROR" || entry.level == "CRITICAL") {
        // Could trigger additional actions
    }
}

void AutonomousObservabilitySystem::aggregateMetrics()
{
    // Aggregate buffered metrics
    QMap<QString, QVector<double>> aggregated;
    
    for (const auto& metric : m_metricsBuffer) {
        aggregated[metric.metricName].append(metric.value);
    }
    
    // Could compute statistics here
    m_metricsBuffer.clear();
}

void AutonomousObservabilitySystem::checkHealthStatus()
{
    // Check if system is healthy based on metrics
    if (m_healthScore > 0.8) {
        m_currentHealthStatus = "healthy";
    } else if (m_healthScore > 0.5) {
        m_currentHealthStatus = "degraded";
    } else {
        m_currentHealthStatus = "unhealthy";
    }
    
    emit healthStatusChanged(m_currentHealthStatus, m_healthScore);
}

void AutonomousObservabilitySystem::detectAnomalies()
{
    // Check for anomalies in current metrics
    for (auto it = m_currentMetrics.begin(); it != m_currentMetrics.end(); ++it) {
        detectAnomaly(it.key(), it.value());
    }
}

void AutonomousObservabilitySystem::evaluateAlerts()
{
    // Check thresholds
    for (auto& threshold : m_alertThresholds) {
        if (m_currentMetrics.contains(threshold.metricName)) {
            double value = m_currentMetrics[threshold.metricName];
            if (value > threshold.threshold) {
                // Alert already triggered in recordMetric
            }
        }
    }
}

void AutonomousObservabilitySystem::onMetricsAggregationTimer()
{
    aggregateMetrics();
    detectAnomalies();
    evaluateAlerts();
}

void AutonomousObservabilitySystem::onHealthCheckTimer()
{
    // Health score recovery when no errors occur
    m_healthScore = std::min(1.0, m_healthScore + 0.01);
    checkHealthStatus();
}
