#include "ai_metrics_collector.hpp"
#include <QFile>
#include <QJsonDocument>
#include <QUuid>
#include <QDebug>
#include <algorithm>
#include <numeric>

AIMetricsCollector::AIMetricsCollector(QObject* parent)
    : QObject(parent)
{
    qInfo() << "[METRICS] Metrics collector initialized";
}

AIMetricsCollector::~AIMetricsCollector() {
    QMutexLocker locker(&m_mutex);
    
    // Structured logging: Report metrics on destruction
    int totalOps = 0;
    for (int count : m_operationCounts.values()) totalOps += count;
    
    qInfo() << "[METRICS] Metrics collector shutting down:"
            << "total_sessions=" << m_sessions.size()
            << "total_operations=" << totalOps;
}

QString AIMetricsCollector::startSession(const QString& sessionType) {
    QMutexLocker locker(&m_mutex);
    
    QString sessionId = generateSessionId();
    
    SessionData sessionData;
    sessionData.metrics.sessionId = sessionId;
    sessionData.metrics.sessionType = sessionType;
    sessionData.metrics.startTime = QDateTime::currentDateTime();
    sessionData.metrics.totalOperations = 0;
    sessionData.metrics.successfulOperations = 0;
    sessionData.metrics.failedOperations = 0;
    
    m_sessions[sessionId] = sessionData;
    m_currentSessionId = sessionId;
    
    // Structured logging: Session started
    qInfo() << "[METRICS] Session started:"
            << "session_id=" << sessionId
            << "type=" << sessionType
            << "timestamp=" << sessionData.metrics.startTime.toString(Qt::ISODate);
    
    return sessionId;
}

void AIMetricsCollector::endSession(const QString& sessionId) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessions.contains(sessionId)) {
        qWarning() << "[METRICS] Attempted to end non-existent session:" << sessionId;
        return;
    }
    
    SessionData& session = m_sessions[sessionId];
    session.metrics.endTime = QDateTime::currentDateTime();
    session.metrics.totalDurationMs = session.metrics.startTime.msecsTo(session.metrics.endTime);
    
    calculateSessionStatistics(session);
    
    // Structured logging: Session completed
    qInfo() << "[METRICS] Session completed:"
            << "session_id=" << sessionId
            << "type=" << session.metrics.sessionType
            << "duration_ms=" << session.metrics.totalDurationMs
            << "total_operations=" << session.metrics.totalOperations
            << "success_rate=" << (session.metrics.successRate * 100.0) << "%"
            << "avg_latency_ms=" << session.metrics.averageLatencyMs;
    
    emit sessionCompleted(sessionId, session.metrics);
}

void AIMetricsCollector::recordOperationStart(const QString& sessionId, const QString& operationType, const QString& operationName) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessions.contains(sessionId)) {
        qWarning() << "[METRICS] Operation start for non-existent session:" << sessionId;
        return;
    }
    
    SessionData& session = m_sessions[sessionId];
    session.operationStartTimes[operationName] = QDateTime::currentMSecsSinceEpoch();
    
    qDebug() << "[METRICS] Operation started:"
             << "session=" << sessionId
             << "type=" << operationType
             << "name=" << operationName;
}

void AIMetricsCollector::recordOperationEnd(const QString& sessionId, const QString& operationName, bool success, const QString& errorMessage) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessions.contains(sessionId)) {
        qWarning() << "[METRICS] Operation end for non-existent session:" << sessionId;
        return;
    }
    
    SessionData& session = m_sessions[sessionId];
    
    if (!session.operationStartTimes.contains(operationName)) {
        qWarning() << "[METRICS] Operation end without start:" << operationName;
        return;
    }
    
    qint64 startTime = session.operationStartTimes[operationName];
    qint64 endTime = QDateTime::currentMSecsSinceEpoch();
    qint64 latencyMs = endTime - startTime;
    
    OperationMetric metric;
    metric.operationName = operationName;
    metric.startTime = QDateTime::fromMSecsSinceEpoch(startTime);
    metric.endTime = QDateTime::fromMSecsSinceEpoch(endTime);
    metric.latencyMs = latencyMs;
    metric.success = success;
    metric.errorMessage = errorMessage;
    
    session.operations.append(metric);
    session.metrics.totalOperations++;
    
    if (success) {
        session.metrics.successfulOperations++;
    } else {
        session.metrics.failedOperations++;
    }
    
    // Update latency history
    if (!m_latencyHistory.contains(metric.operationType)) {
        m_latencyHistory[metric.operationType] = QVector<qint64>();
    }
    m_latencyHistory[metric.operationType].append(latencyMs);
    
    // Update operation counts
    m_operationCounts[metric.operationType]++;
    
    if (!success) {
        m_errorCounts[metric.operationType]++;
    }
    
    // Structured logging: Operation completed
    qInfo() << "[METRICS] Operation completed:"
            << "session=" << sessionId
            << "name=" << operationName
            << "latency_ms=" << latencyMs
            << "success=" << success
            << (success ? "" : QString("error=%1").arg(errorMessage));
    
    session.operationStartTimes.remove(operationName);
    
    emit operationCompleted(sessionId, metric);
    emit metricsUpdated(sessionId);
}

void AIMetricsCollector::recordOperationMetric(const QString& sessionId, const QString& key, double value) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessions.contains(sessionId)) {
        return;
    }
    
    SessionData& session = m_sessions[sessionId];
    session.customMetrics[key] = value;
}

void AIMetricsCollector::recordLatency(const QString& operationType, qint64 latencyMs) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_latencyHistory.contains(operationType)) {
        m_latencyHistory[operationType] = QVector<qint64>();
    }
    m_latencyHistory[operationType].append(latencyMs);
}

void AIMetricsCollector::recordThroughput(const QString& operationType, int itemsProcessed, qint64 durationMs) {
    if (durationMs == 0) return;
    
    double throughput = (itemsProcessed * 1000.0) / durationMs; // items per second
    
    qInfo() << "[METRICS] Throughput recorded:"
            << "type=" << operationType
            << "items=" << itemsProcessed
            << "duration_ms=" << durationMs
            << "throughput=" << throughput << "items/sec";
}

void AIMetricsCollector::recordError(const QString& operationType, const QString& errorType) {
    QMutexLocker locker(&m_mutex);
    
    QString errorKey = operationType + "_" + errorType;
    m_errorCounts[errorKey]++;
    
    qWarning() << "[METRICS] Error recorded:"
               << "type=" << operationType
               << "error=" << errorType
               << "total_count=" << m_errorCounts[errorKey];
}

AIMetricsCollector::SessionMetrics AIMetricsCollector::getSessionMetrics(const QString& sessionId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessions.contains(sessionId)) {
        return SessionMetrics();
    }
    
    return m_sessions[sessionId].metrics;
}

QJsonObject AIMetricsCollector::getMetricsReport() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject report;
    report["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    report["total_sessions"] = m_sessions.size();
    
    // Session summaries
    QJsonArray sessionsArray;
    for (auto it = m_sessions.begin(); it != m_sessions.end(); ++it) {
        sessionsArray.append(getSessionReport(it.key()));
    }
    report["sessions"] = sessionsArray;
    
    // Latency statistics by operation type
    QJsonObject latencyStats;
    for (auto it = m_latencyHistory.begin(); it != m_latencyHistory.end(); ++it) {
        QJsonObject stats;
        stats["average"] = getAverageLatency(it.key());
        stats["count"] = it.value().size();
        latencyStats[it.key()] = stats;
    }
    report["latency_statistics"] = latencyStats;
    
    // Error counts
    QJsonObject errorStats;
    for (auto it = m_errorCounts.begin(); it != m_errorCounts.end(); ++it) {
        errorStats[it.key()] = it.value();
    }
    report["error_statistics"] = errorStats;
    
    return report;
}

QJsonObject AIMetricsCollector::getSessionReport(const QString& sessionId) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_sessions.contains(sessionId)) {
        return QJsonObject();
    }
    
    const SessionData& session = m_sessions[sessionId];
    const SessionMetrics& metrics = session.metrics;
    
    QJsonObject report;
    report["session_id"] = sessionId;
    report["session_type"] = metrics.sessionType;
    report["start_time"] = metrics.startTime.toString(Qt::ISODate);
    report["end_time"] = metrics.endTime.toString(Qt::ISODate);
    report["duration_ms"] = static_cast<double>(metrics.totalDurationMs);
    report["total_operations"] = metrics.totalOperations;
    report["successful_operations"] = metrics.successfulOperations;
    report["failed_operations"] = metrics.failedOperations;
    report["success_rate"] = metrics.successRate;
    report["average_latency_ms"] = metrics.averageLatencyMs;
    report["min_latency_ms"] = metrics.minLatencyMs;
    report["max_latency_ms"] = metrics.maxLatencyMs;
    report["statistics"] = metrics.statistics;
    report["custom_metrics"] = session.customMetrics;
    
    return report;
}

QJsonArray AIMetricsCollector::getAllOperations(const QString& sessionId) const {
    QMutexLocker locker(&m_mutex);
    
    QJsonArray operations;
    
    if (!m_sessions.contains(sessionId)) {
        return operations;
    }
    
    const SessionData& session = m_sessions[sessionId];
    
    for (const OperationMetric& metric : session.operations) {
        QJsonObject op;
        op["name"] = metric.operationName;
        op["type"] = metric.operationType;
        op["start_time"] = metric.startTime.toString(Qt::ISODate);
        op["end_time"] = metric.endTime.toString(Qt::ISODate);
        op["latency_ms"] = static_cast<double>(metric.latencyMs);
        op["success"] = metric.success;
        if (!metric.errorMessage.isEmpty()) {
            op["error"] = metric.errorMessage;
        }
        op["additional_data"] = metric.additionalData;
        
        operations.append(op);
    }
    
    return operations;
}

double AIMetricsCollector::getAverageLatency(const QString& operationType) const {
    QMutexLocker locker(&m_mutex);
    
    if (!m_latencyHistory.contains(operationType) || m_latencyHistory[operationType].isEmpty()) {
        return 0.0;
    }
    
    const QVector<qint64>& latencies = m_latencyHistory[operationType];
    qint64 sum = std::accumulate(latencies.begin(), latencies.end(), 0LL);
    return static_cast<double>(sum) / latencies.size();
}

double AIMetricsCollector::getSuccessRate(const QString& operationType) const {
    QMutexLocker locker(&m_mutex);
    
    int total = m_operationCounts.value(operationType, 0);
    if (total == 0) return 1.0;
    
    int errors = m_errorCounts.value(operationType, 0);
    return static_cast<double>(total - errors) / total;
}

int AIMetricsCollector::getTotalOperations(const QString& operationType) const {
    QMutexLocker locker(&m_mutex);
    return m_operationCounts.value(operationType, 0);
}

int AIMetricsCollector::getErrorCount(const QString& operationType) const {
    QMutexLocker locker(&m_mutex);
    return m_errorCounts.value(operationType, 0);
}

bool AIMetricsCollector::exportMetrics(const QString& filePath) const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject report = getMetricsReport();
    QJsonDocument doc(report);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qCritical() << "[METRICS] Failed to export metrics:"
                    << "file=" << filePath
                    << "error=" << file.errorString();
        return false;
    }
    
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
    
    qInfo() << "[METRICS] Metrics exported:"
            << "file=" << filePath
            << "sessions=" << m_sessions.size();
    
    return true;
}

void AIMetricsCollector::clearMetrics() {
    QMutexLocker locker(&m_mutex);
    
    m_sessions.clear();
    m_latencyHistory.clear();
    m_errorCounts.clear();
    m_operationCounts.clear();
    m_currentSessionId.clear();
    
    qInfo() << "[METRICS] All metrics cleared";
}

void AIMetricsCollector::calculateSessionStatistics(SessionData& session) {
    if (session.operations.isEmpty()) {
        session.metrics.averageLatencyMs = 0.0;
        session.metrics.minLatencyMs = 0.0;
        session.metrics.maxLatencyMs = 0.0;
        session.metrics.successRate = 0.0;
        return;
    }
    
    // Calculate latency statistics
    qint64 totalLatency = 0;
    qint64 minLatency = std::numeric_limits<qint64>::max();
    qint64 maxLatency = 0;
    
    for (const OperationMetric& metric : session.operations) {
        totalLatency += metric.latencyMs;
        minLatency = std::min(minLatency, metric.latencyMs);
        maxLatency = std::max(maxLatency, metric.latencyMs);
    }
    
    session.metrics.averageLatencyMs = static_cast<double>(totalLatency) / session.operations.size();
    session.metrics.minLatencyMs = static_cast<double>(minLatency);
    session.metrics.maxLatencyMs = static_cast<double>(maxLatency);
    
    // Calculate success rate
    if (session.metrics.totalOperations > 0) {
        session.metrics.successRate = static_cast<double>(session.metrics.successfulOperations) / session.metrics.totalOperations;
    } else {
        session.metrics.successRate = 0.0;
    }
    
    // Build statistics object
    session.metrics.statistics["total_latency_ms"] = static_cast<double>(totalLatency);
    session.metrics.statistics["operation_count"] = session.operations.size();
}

QString AIMetricsCollector::generateSessionId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
