#include "metrics_collector.hpp"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <algorithm>
#include <cmath>

MetricsCollector& MetricsCollector::instance() {
    static MetricsCollector instance;
    return instance;
}

MetricsCollector::MetricsCollector()
    : QObject(nullptr)
{
}

MetricsCollector::~MetricsCollector() {
}

void MetricsCollector::startRequest(qint64 requestId, const QString& modelName, int promptTokens) {
    if (!m_enabled) return;
    
    QMutexLocker locker(&m_mutex);
    
    RequestMetrics metrics;
    metrics.requestId = requestId;
    metrics.startTime = QDateTime::currentDateTime();
    metrics.modelName = modelName;
    metrics.promptTokens = promptTokens;
    metrics.tokensGenerated = 0;
    metrics.success = false;
    
    m_activeRequests[requestId] = metrics;
    
    QElapsedTimer timer;
    timer.start();
    m_timers[requestId] = timer;
    
    emit requestStarted(requestId);
}

void MetricsCollector::endRequest(qint64 requestId, int tokensGenerated, bool success, const QString& error) {
    if (!m_enabled) return;
    
    QMutexLocker locker(&m_mutex);
    
    if (!m_activeRequests.contains(requestId)) {
        qWarning() << "[MetricsCollector] Unknown request:" << requestId;
        return;
    }
    
    RequestMetrics& metrics = m_activeRequests[requestId];
    metrics.endTime = QDateTime::currentDateTime();
    metrics.tokensGenerated = tokensGenerated;
    metrics.success = success;
    metrics.errorMessage = error;
    metrics.memoryUsed = m_currentMemoryUsage;
    
    if (m_timers.contains(requestId)) {
        metrics.durationMs = m_timers[requestId].elapsed();
        m_timers.remove(requestId);
    }
    
    // Calculate tokens per second
    if (metrics.durationMs > 0 && tokensGenerated > 0) {
        metrics.tokensPerSecond = (tokensGenerated * 1000.0f) / metrics.durationMs;
    } else {
        metrics.tokensPerSecond = 0.0f;
    }
    
    // Move to completed requests
    m_completedRequests.append(metrics);
    m_activeRequests.remove(requestId);
    
    // Performance warnings
    if (metrics.tokensPerSecond < 10.0f && success) {
        emit performanceWarning(QString("Low tokens/sec: %1 for request %2")
            .arg(metrics.tokensPerSecond, 0, 'f', 2)
            .arg(requestId));
    }
    
    if (metrics.durationMs > 30000) { // 30 seconds
        emit performanceWarning(QString("High latency: %1ms for request %2")
            .arg(metrics.durationMs)
            .arg(requestId));
    }
    
    emit requestCompleted(requestId, metrics);
    emit metricsUpdated();
    
    qInfo() << "[MetricsCollector] Request" << requestId << "completed:"
            << tokensGenerated << "tokens in" << metrics.durationMs << "ms"
            << "(" << metrics.tokensPerSecond << "tok/s)";
}

void MetricsCollector::recordToken(qint64 requestId) {
    if (!m_enabled) return;
    
    QMutexLocker locker(&m_mutex);
    
    if (m_activeRequests.contains(requestId)) {
        m_activeRequests[requestId].tokensGenerated++;
    }
}

void MetricsCollector::recordMemoryUsage(size_t bytes) {
    if (!m_enabled) return;
    
    QMutexLocker locker(&m_mutex);
    m_currentMemoryUsage = bytes;
}

MetricsCollector::RequestMetrics MetricsCollector::getRequestMetrics(qint64 requestId) const {
    QMutexLocker locker(&m_mutex);
    
    // Check active requests
    if (m_activeRequests.contains(requestId)) {
        return m_activeRequests[requestId];
    }
    
    // Search completed requests
    for (const auto& metrics : m_completedRequests) {
        if (metrics.requestId == requestId) {
            return metrics;
        }
    }
    
    return RequestMetrics();
}

MetricsCollector::AggregateMetrics MetricsCollector::getAggregateMetrics() const {
    QMutexLocker locker(&m_mutex);
    
    AggregateMetrics agg;
    
    if (m_completedRequests.isEmpty()) {
        return agg;
    }
    
    agg.totalRequests = m_completedRequests.size();
    agg.firstRequest = m_completedRequests.first().startTime;
    agg.lastRequest = m_completedRequests.last().endTime;
    
    QList<qint64> latencies;
    qint64 totalLatency = 0;
    float totalTokensPerSec = 0;
    size_t totalMemory = 0;
    
    for (const auto& metrics : m_completedRequests) {
        if (metrics.success) {
            agg.successfulRequests++;
        } else {
            agg.failedRequests++;
        }
        
        latencies.append(metrics.durationMs);
        totalLatency += metrics.durationMs;
        
        agg.minLatencyMs = qMin(agg.minLatencyMs, metrics.durationMs);
        agg.maxLatencyMs = qMax(agg.maxLatencyMs, metrics.durationMs);
        
        totalTokensPerSec += metrics.tokensPerSecond;
        agg.minTokensPerSec = qMin(agg.minTokensPerSec, metrics.tokensPerSecond);
        agg.maxTokensPerSec = qMax(agg.maxTokensPerSec, metrics.tokensPerSecond);
        
        totalMemory += metrics.memoryUsed;
        agg.peakMemoryUsage = qMax(agg.peakMemoryUsage, metrics.memoryUsed);
    }
    
    // Calculate averages
    agg.avgLatencyMs = totalLatency / agg.totalRequests;
    agg.avgTokensPerSec = totalTokensPerSec / agg.totalRequests;
    agg.avgMemoryUsage = totalMemory / agg.totalRequests;
    
    // Calculate percentiles
    std::sort(latencies.begin(), latencies.end());
    int p50Idx = latencies.size() * 50 / 100;
    int p95Idx = latencies.size() * 95 / 100;
    int p99Idx = latencies.size() * 99 / 100;
    
    agg.p50LatencyMs = latencies[p50Idx];
    agg.p95LatencyMs = latencies[p95Idx];
    agg.p99LatencyMs = latencies[p99Idx];
    
    return agg;
}

QString MetricsCollector::exportToJson() const {
    QMutexLocker locker(&m_mutex);
    
    QJsonObject root;
    root["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Aggregate metrics
    AggregateMetrics agg = getAggregateMetrics();
    QJsonObject aggObj;
    aggObj["totalRequests"] = agg.totalRequests;
    aggObj["successfulRequests"] = agg.successfulRequests;
    aggObj["failedRequests"] = agg.failedRequests;
    aggObj["avgLatencyMs"] = (double)agg.avgLatencyMs;
    aggObj["p50LatencyMs"] = (double)agg.p50LatencyMs;
    aggObj["p95LatencyMs"] = (double)agg.p95LatencyMs;
    aggObj["p99LatencyMs"] = (double)agg.p99LatencyMs;
    aggObj["avgTokensPerSec"] = (double)agg.avgTokensPerSec;
    aggObj["peakMemoryMB"] = (double)(agg.peakMemoryUsage / (1024.0 * 1024.0));
    root["aggregate"] = aggObj;
    
    // Individual requests
    QJsonArray requestsArray;
    for (const auto& metrics : m_completedRequests) {
        QJsonObject reqObj;
        reqObj["requestId"] = (double)metrics.requestId;
        reqObj["modelName"] = metrics.modelName;
        reqObj["startTime"] = metrics.startTime.toString(Qt::ISODate);
        reqObj["durationMs"] = (double)metrics.durationMs;
        reqObj["tokensGenerated"] = metrics.tokensGenerated;
        reqObj["tokensPerSec"] = (double)metrics.tokensPerSecond;
        reqObj["success"] = metrics.success;
        if (!metrics.errorMessage.isEmpty()) {
            reqObj["error"] = metrics.errorMessage;
        }
        requestsArray.append(reqObj);
    }
    root["requests"] = requestsArray;
    
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

QString MetricsCollector::exportToCsv() const {
    QMutexLocker locker(&m_mutex);
    
    QString csv;
    csv += "RequestID,ModelName,StartTime,DurationMs,TokensGenerated,TokensPerSec,Success,Error\n";
    
    for (const auto& metrics : m_completedRequests) {
        csv += QString("%1,%2,%3,%4,%5,%6,%7,%8\n")
            .arg(metrics.requestId)
            .arg(metrics.modelName)
            .arg(metrics.startTime.toString(Qt::ISODate))
            .arg(metrics.durationMs)
            .arg(metrics.tokensGenerated)
            .arg(metrics.tokensPerSecond, 0, 'f', 2)
            .arg(metrics.success ? "true" : "false")
            .arg(metrics.errorMessage);
    }
    
    return csv;
}

void MetricsCollector::reset() {
    QMutexLocker locker(&m_mutex);
    
    m_activeRequests.clear();
    m_timers.clear();
    m_completedRequests.clear();
    m_currentMemoryUsage = 0;
    
    qInfo() << "[MetricsCollector] Metrics reset";
    emit metricsUpdated();
}

void MetricsCollector::setEnabled(bool enabled) {
    QMutexLocker locker(&m_mutex);
    m_enabled = enabled;
    qInfo() << "[MetricsCollector] Metrics collection" << (enabled ? "enabled" : "disabled");
}

bool MetricsCollector::isEnabled() const {
    QMutexLocker locker(&m_mutex);
    return m_enabled;
}

void MetricsCollector::calculatePercentiles() {
    // Already calculated in getAggregateMetrics()
}
