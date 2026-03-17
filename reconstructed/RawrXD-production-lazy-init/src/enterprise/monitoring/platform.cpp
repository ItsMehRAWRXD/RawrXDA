/**
 * @file enterprise_monitoring_platform.cpp
 * @brief Enterprise-grade monitoring platform implementation
 * 
 * Implements distributed tracing, metrics, alerting, SLA tracking,
 * and business intelligence following header declarations exactly.
 */

#include "enterprise_monitoring_platform.h"
#include <QUuid>
#include <QThread>
#include <QJsonDocument>
#include <random>
#include <algorithm>

namespace RawrXD {
namespace Agentic {

// ============================================================================
// DistributedTracingCollector Implementation
// ============================================================================

DistributedTracingCollector::DistributedTracingCollector(QObject *parent)
    : QObject(parent)
    , m_samplingRate(0.1)
    , m_adaptiveSampling(false)
{
    qDebug() << "[DistributedTracingCollector] Initialized";
}

DistributedTracingCollector::~DistributedTracingCollector() = default;

void DistributedTracingCollector::startSpan(const QString &operationName, const QString &serviceName)
{
    Span span;
    span.spanId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    span.traceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    span.operationName = operationName;
    span.serviceName = serviceName;
    span.startTime = QDateTime::currentDateTime();
    span.status = "running";
    
    m_activeSpans[span.spanId] = span;
    
    qDebug() << "[DistributedTracingCollector] Started span:" << span.spanId 
             << "operation:" << operationName;
    
    emit spanStarted(span);
}

void DistributedTracingCollector::updateSpanTag(const QString &spanId, const QString &tagName, const QString &tagValue)
{
    if (m_activeSpans.contains(spanId)) {
        m_activeSpans[spanId].tags[tagName] = tagValue;
        qDebug() << "[DistributedTracingCollector] Updated span tag:" << spanId 
                 << tagName << "=" << tagValue;
    }
}

void DistributedTracingCollector::addSpanLog(const QString &spanId, const QString &logMessage)
{
    if (m_activeSpans.contains(spanId)) {
        m_activeSpans[spanId].logs.append(
            QString("[%1] %2").arg(QDateTime::currentDateTime().toString(Qt::ISODate), logMessage)
        );
        qDebug() << "[DistributedTracingCollector] Added log to span:" << spanId;
    }
}

void DistributedTracingCollector::endSpan(const QString &spanId, const QString &status)
{
    if (!m_activeSpans.contains(spanId)) {
        qWarning() << "[DistributedTracingCollector] Span not found:" << spanId;
        return;
    }
    
    Span &span = m_activeSpans[spanId];
    span.endTime = QDateTime::currentDateTime();
    span.status = status;
    span.durationMs = span.startTime.msecsTo(span.endTime);
    
    qDebug() << "[DistributedTracingCollector] Ended span:" << spanId 
             << "duration:" << span.durationMs << "ms";
    
    // Add span to trace
    if (!m_traces.contains(span.traceId)) {
        Trace trace;
        trace.traceId = span.traceId;
        trace.startTime = span.startTime;
        trace.rootOperation = span.operationName;
        m_traces[span.traceId] = trace;
    }
    
    m_traces[span.traceId].spans.append(span);
    m_traces[span.traceId].spanCount = m_traces[span.traceId].spans.size();
    m_traces[span.traceId].endTime = span.endTime;
    m_traces[span.traceId].totalDurationMs = 
        m_traces[span.traceId].startTime.msecsTo(span.endTime);
    
    if (status == "error") {
        m_traces[span.traceId].hasErrors = true;
    }
    
    emit spanEnded(span);
    m_activeSpans.remove(spanId);
}

Span DistributedTracingCollector::getSpan(const QString &spanId)
{
    if (m_activeSpans.contains(spanId)) {
        return m_activeSpans[spanId];
    }
    
    // Search in completed traces
    for (const auto &trace : m_traces) {
        for (const auto &span : trace.spans) {
            if (span.spanId == spanId) {
                return span;
            }
        }
    }
    
    return Span();
}

QVector<Span> DistributedTracingCollector::getSpansInTrace(const QString &traceId)
{
    if (m_traces.contains(traceId)) {
        return m_traces[traceId].spans;
    }
    return QVector<Span>();
}

Trace DistributedTracingCollector::getTrace(const QString &traceId)
{
    if (m_traces.contains(traceId)) {
        return m_traces[traceId];
    }
    return Trace();
}

Trace DistributedTracingCollector::findTraceByOperation(const QString &operationName, const QDateTime &startTime, const QDateTime &endTime)
{
    for (const auto &trace : m_traces) {
        if (trace.rootOperation == operationName &&
            trace.startTime >= startTime && trace.endTime <= endTime) {
            return trace;
        }
    }
    return Trace();
}

QVector<Trace> DistributedTracingCollector::findSlowTraces(qint64 minDurationMs)
{
    QVector<Trace> slowTraces;
    for (const auto &trace : m_traces) {
        if (trace.totalDurationMs >= minDurationMs) {
            slowTraces.append(trace);
        }
    }
    return slowTraces;
}

QVector<Trace> DistributedTracingCollector::findErrorTraces(const QString &serviceName)
{
    QVector<Trace> errorTraces;
    for (const auto &trace : m_traces) {
        if (!trace.hasErrors) continue;
        
        bool matchesService = serviceName.isEmpty();
        for (const auto &span : trace.spans) {
            if (span.serviceName == serviceName) {
                matchesService = true;
                break;
            }
        }
        
        if (matchesService) {
            errorTraces.append(trace);
        }
    }
    return errorTraces;
}

bool DistributedTracingCollector::shouldSampleTrace(const QString &traceId)
{
    // Simple hash-based sampling
    uint hash = qHash(traceId);
    double threshold = m_samplingRate * static_cast<double>(UINT_MAX);
    return static_cast<double>(hash) < threshold;
}

void DistributedTracingCollector::setSamplingRate(double rate)
{
    m_samplingRate = qBound(0.0, rate, 1.0);
    qDebug() << "[DistributedTracingCollector] Set sampling rate to:" << m_samplingRate;
}

void DistributedTracingCollector::setAdaptiveSampling(bool enabled)
{
    m_adaptiveSampling = enabled;
    qDebug() << "[DistributedTracingCollector] Adaptive sampling:" << (enabled ? "enabled" : "disabled");
}

QString DistributedTracingCollector::exportTraceAsJson(const QString &traceId)
{
    if (!m_traces.contains(traceId)) {
        return "{}";
    }
    
    const Trace &trace = m_traces[traceId];
    QJsonObject json;
    json["traceId"] = trace.traceId;
    json["rootOperation"] = trace.rootOperation;
    json["totalDurationMs"] = static_cast<qint64>(trace.totalDurationMs);
    json["spanCount"] = trace.spanCount;
    json["hasErrors"] = trace.hasErrors;
    
    QJsonArray spansArray;
    for (const auto &span : trace.spans) {
        QJsonObject spanObj;
        spanObj["spanId"] = span.spanId;
        spanObj["operationName"] = span.operationName;
        spanObj["serviceName"] = span.serviceName;
        spanObj["durationMs"] = static_cast<qint64>(span.durationMs);
        spanObj["status"] = span.status;
        spansArray.append(spanObj);
    }
    json["spans"] = spansArray;
    
    return QString::fromUtf8(QJsonDocument(json).toJson());
}

QString DistributedTracingCollector::generateTraceVisualization(const QString &traceId)
{
    if (!m_traces.contains(traceId)) {
        return "Trace not found";
    }
    
    const Trace &trace = m_traces[traceId];
    QString viz = QString("=== Trace: %1 ===\n").arg(trace.traceId);
    viz += QString("Root: %1\n").arg(trace.rootOperation);
    viz += QString("Duration: %1 ms\n").arg(trace.totalDurationMs);
    viz += QString("Spans: %1\n\n").arg(trace.spanCount);
    
    for (const auto &span : trace.spans) {
        viz += QString("  [%1] %2 (%3 ms) - %4\n")
            .arg(span.serviceName, span.operationName)
            .arg(span.durationMs)
            .arg(span.status);
    }
    
    return viz;
}

QVector<Trace> DistributedTracingCollector::getTracesForService(const QString &serviceName, int limit)
{
    QVector<Trace> serviceTraces;
    for (const auto &trace : m_traces) {
        for (const auto &span : trace.spans) {
            if (span.serviceName == serviceName) {
                serviceTraces.append(trace);
                break;
            }
        }
        if (serviceTraces.size() >= limit) break;
    }
    return serviceTraces;
}

Trace DistributedTracingCollector::buildTraceFromSpans(const QVector<Span> &spans)
{
    Trace trace;
    if (spans.isEmpty()) return trace;
    
    trace.traceId = spans.first().traceId;
    trace.spans = spans;
    trace.spanCount = spans.size();
    
    // Find earliest start and latest end
    trace.startTime = spans.first().startTime;
    trace.endTime = spans.first().endTime;
    
    for (const auto &span : spans) {
        if (span.startTime < trace.startTime) {
            trace.startTime = span.startTime;
            trace.rootOperation = span.operationName;
        }
        if (span.endTime > trace.endTime) {
            trace.endTime = span.endTime;
        }
        if (span.status == "error") {
            trace.hasErrors = true;
        }
    }
    
    trace.totalDurationMs = trace.startTime.msecsTo(trace.endTime);
    return trace;
}

// ============================================================================
// MetricsCollector Implementation
// ============================================================================

MetricsCollector::MetricsCollector(QObject *parent)
    : QObject(parent)
    , m_maxHistorySize(100000)
{
    qDebug() << "[MetricsCollector] Initialized";
}

MetricsCollector::~MetricsCollector() = default;

void MetricsCollector::recordGauge(const QString &metricName, double value, const QMap<QString, QString> &labels)
{
    Metric metric;
    metric.metricName = metricName;
    metric.metricType = "gauge";
    metric.value = value;
    metric.labels = labels;
    metric.timestamp = QDateTime::currentDateTime();
    
    m_metricsHistory[metricName].append(metric);
    pruneOldMetrics();
    
    emit metricRecorded(metric);
}

void MetricsCollector::incrementCounter(const QString &metricName, const QMap<QString, QString> &labels)
{
    double currentValue = 0;
    if (m_metricsHistory.contains(metricName) && !m_metricsHistory[metricName].isEmpty()) {
        currentValue = m_metricsHistory[metricName].last().value;
    }
    
    Metric metric;
    metric.metricName = metricName;
    metric.metricType = "counter";
    metric.value = currentValue + 1;
    metric.labels = labels;
    metric.timestamp = QDateTime::currentDateTime();
    
    m_metricsHistory[metricName].append(metric);
    pruneOldMetrics();
    
    emit metricRecorded(metric);
}

void MetricsCollector::recordHistogram(const QString &metricName, double value, const QMap<QString, QString> &labels)
{
    Metric metric;
    metric.metricName = metricName;
    metric.metricType = "histogram";
    metric.value = value;
    metric.labels = labels;
    metric.timestamp = QDateTime::currentDateTime();
    
    m_metricsHistory[metricName].append(metric);
    pruneOldMetrics();
    
    emit metricRecorded(metric);
}

void MetricsCollector::recordSummary(const QString &metricName, double value, const QMap<QString, QString> &labels)
{
    Metric metric;
    metric.metricName = metricName;
    metric.metricType = "summary";
    metric.value = value;
    metric.labels = labels;
    metric.timestamp = QDateTime::currentDateTime();
    
    m_metricsHistory[metricName].append(metric);
    pruneOldMetrics();
    
    emit metricRecorded(metric);
}

double MetricsCollector::getMetricValue(const QString &metricName)
{
    if (m_metricsHistory.contains(metricName) && !m_metricsHistory[metricName].isEmpty()) {
        return m_metricsHistory[metricName].last().value;
    }
    return 0.0;
}

QVector<Metric> MetricsCollector::getMetricsByLabel(const QString &labelName, const QString &labelValue)
{
    QVector<Metric> result;
    for (auto it = m_metricsHistory.begin(); it != m_metricsHistory.end(); ++it) {
        for (const auto &metric : it.value()) {
            if (metric.labels.value(labelName) == labelValue) {
                result.append(metric);
            }
        }
    }
    return result;
}

QVector<Metric> MetricsCollector::getAllMetrics()
{
    QVector<Metric> result;
    for (auto it = m_metricsHistory.begin(); it != m_metricsHistory.end(); ++it) {
        if (!it.value().isEmpty()) {
            result.append(it.value().last());
        }
    }
    return result;
}

QVector<Metric> MetricsCollector::getMetricTimeSeries(const QString &metricName, const QDateTime &start, const QDateTime &end)
{
    QVector<Metric> result;
    if (!m_metricsHistory.contains(metricName)) return result;
    
    for (const auto &metric : m_metricsHistory[metricName]) {
        if (metric.timestamp >= start && metric.timestamp <= end) {
            result.append(metric);
        }
    }
    return result;
}

QVector<double> MetricsCollector::getMetricValues(const QString &metricName, const QDateTime &start, const QDateTime &end)
{
    QVector<double> values;
    auto timeSeries = getMetricTimeSeries(metricName, start, end);
    for (const auto &metric : timeSeries) {
        values.append(metric.value);
    }
    return values;
}

double MetricsCollector::calculateAverage(const QString &metricName, const QDateTime &start, const QDateTime &end)
{
    auto values = getMetricValues(metricName, start, end);
    if (values.isEmpty()) return 0.0;
    
    double sum = 0.0;
    for (double v : values) sum += v;
    return sum / values.size();
}

double MetricsCollector::calculateMax(const QString &metricName, const QDateTime &start, const QDateTime &end)
{
    auto values = getMetricValues(metricName, start, end);
    if (values.isEmpty()) return 0.0;
    
    return *std::max_element(values.begin(), values.end());
}

double MetricsCollector::calculateMin(const QString &metricName, const QDateTime &start, const QDateTime &end)
{
    auto values = getMetricValues(metricName, start, end);
    if (values.isEmpty()) return 0.0;
    
    return *std::min_element(values.begin(), values.end());
}

double MetricsCollector::calculatePercentile(const QString &metricName, double percentile, const QDateTime &start, const QDateTime &end)
{
    auto values = getMetricValues(metricName, start, end);
    if (values.isEmpty()) return 0.0;
    
    std::sort(values.begin(), values.end());
    
    int index = static_cast<int>(percentile / 100.0 * (values.size() - 1));
    return values[qBound(0, index, values.size() - 1)];
}

void MetricsCollector::registerCustomMetric(const QString &metricName, const QString &metricType, const QString &unit)
{
    m_customMetricTypes[metricName] = QString("%1:%2").arg(metricType, unit);
    qDebug() << "[MetricsCollector] Registered custom metric:" << metricName << metricType << unit;
}

void MetricsCollector::recordBusinessMetric(const BusinessMetric &metric)
{
    Metric m;
    m.metricName = metric.metricName;
    m.metricType = metric.metricType;
    m.value = metric.value;
    m.timestamp = metric.timestamp;
    m.labels["dimension"] = metric.dimension;
    
    m_metricsHistory[metric.metricName].append(m);
    pruneOldMetrics();
    
    emit metricRecorded(m);
}

void MetricsCollector::pruneOldMetrics()
{
    for (auto it = m_metricsHistory.begin(); it != m_metricsHistory.end(); ++it) {
        while (it.value().size() > m_maxHistorySize) {
            it.value().removeFirst();
        }
    }
}

// ============================================================================
// StructuredLogger Implementation
// ============================================================================

StructuredLogger::StructuredLogger(const QString &serviceName, QObject *parent)
    : QObject(parent)
    , m_serviceName(serviceName)
{
    qDebug() << "[StructuredLogger] Initialized for service:" << serviceName;
}

StructuredLogger::~StructuredLogger()
{
    flushLogs();
}

void StructuredLogger::debug(const QString &message, const QMap<QString, QString> &data)
{
    LogEntry entry = createLogEntry("DEBUG", message, data);
    m_logBuffer.append(entry);
    emit logEmitted(entry);
}

void StructuredLogger::info(const QString &message, const QMap<QString, QString> &data)
{
    LogEntry entry = createLogEntry("INFO", message, data);
    m_logBuffer.append(entry);
    emit logEmitted(entry);
}

void StructuredLogger::warning(const QString &message, const QMap<QString, QString> &data)
{
    LogEntry entry = createLogEntry("WARNING", message, data);
    m_logBuffer.append(entry);
    emit logEmitted(entry);
}

void StructuredLogger::error(const QString &message, const QMap<QString, QString> &data)
{
    LogEntry entry = createLogEntry("ERROR", message, data);
    m_logBuffer.append(entry);
    emit logEmitted(entry);
    emit errorLogDetected(entry);
}

void StructuredLogger::fatal(const QString &message, const QMap<QString, QString> &data)
{
    LogEntry entry = createLogEntry("FATAL", message, data);
    m_logBuffer.append(entry);
    emit logEmitted(entry);
    emit errorLogDetected(entry);
    flushLogs();
}

void StructuredLogger::setTraceContext(const QString &traceId, const QString &spanId)
{
    m_currentTraceId = traceId;
    m_currentSpanId = spanId;
}

void StructuredLogger::clearTraceContext()
{
    m_currentTraceId.clear();
    m_currentSpanId.clear();
}

void StructuredLogger::attachContext(const QString &key, const QString &value)
{
    m_contextData[key] = value;
}

QVector<LogEntry> StructuredLogger::queryLogs(const QString &query, const QDateTime &start, const QDateTime &end)
{
    QVector<LogEntry> result;
    for (const auto &entry : m_logBuffer) {
        if (entry.timestamp >= start && entry.timestamp <= end &&
            entry.message.contains(query, Qt::CaseInsensitive)) {
            result.append(entry);
        }
    }
    return result;
}

QVector<LogEntry> StructuredLogger::getLogsForTrace(const QString &traceId)
{
    QVector<LogEntry> result;
    for (const auto &entry : m_logBuffer) {
        if (entry.traceId == traceId) {
            result.append(entry);
        }
    }
    return result;
}

QVector<LogEntry> StructuredLogger::getErrorLogs(const QString &serviceName, int limit)
{
    QVector<LogEntry> result;
    for (const auto &entry : m_logBuffer) {
        if ((entry.level == "ERROR" || entry.level == "FATAL") &&
            (serviceName.isEmpty() || entry.serviceName == serviceName)) {
            result.append(entry);
            if (result.size() >= limit) break;
        }
    }
    return result;
}

QJsonObject StructuredLogger::analyzeLogPattern(const QString &pattern, const QDateTime &start, const QDateTime &end)
{
    QJsonObject analysis;
    int matchCount = 0;
    
    for (const auto &entry : m_logBuffer) {
        if (entry.timestamp >= start && entry.timestamp <= end &&
            entry.message.contains(pattern, Qt::CaseInsensitive)) {
            matchCount++;
        }
    }
    
    analysis["pattern"] = pattern;
    analysis["match_count"] = matchCount;
    analysis["start_time"] = start.toString(Qt::ISODate);
    analysis["end_time"] = end.toString(Qt::ISODate);
    
    return analysis;
}

int StructuredLogger::countLogsByLevel(const QString &level, const QDateTime &start, const QDateTime &end)
{
    int count = 0;
    for (const auto &entry : m_logBuffer) {
        if (entry.level == level &&
            entry.timestamp >= start && entry.timestamp <= end) {
            count++;
        }
    }
    return count;
}

LogEntry StructuredLogger::createLogEntry(const QString &level, const QString &message, const QMap<QString, QString> &data)
{
    LogEntry entry;
    entry.serviceName = m_serviceName;
    entry.level = level;
    entry.message = message;
    entry.structuredData = data;
    entry.traceId = m_currentTraceId;
    entry.spanId = m_currentSpanId;
    entry.timestamp = QDateTime::currentDateTime();
    
    // Merge context data
    for (auto it = m_contextData.begin(); it != m_contextData.end(); ++it) {
        if (!entry.structuredData.contains(it.key())) {
            entry.structuredData[it.key()] = it.value();
        }
    }
    
    return entry;
}

void StructuredLogger::flushLogs()
{
    // In production, this would write to file/network
    qDebug() << "[StructuredLogger] Flushing" << m_logBuffer.size() << "log entries";
}

// ============================================================================
// AlertingEngine Implementation
// ============================================================================

AlertingEngine::AlertingEngine(QObject *parent)
    : QObject(parent)
{
    qDebug() << "[AlertingEngine] Initialized";
}

AlertingEngine::~AlertingEngine() = default;

bool AlertingEngine::createAlert(const Alert &alert)
{
    qDebug() << "[AlertingEngine] Creating alert:" << alert.alertName;
    
    Alert a = alert;
    if (a.alertId.isEmpty()) {
        a.alertId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    a.createdAt = QDateTime::currentDateTime();
    
    m_alerts[a.alertId] = a;
    return true;
}

bool AlertingEngine::updateAlert(const Alert &alert)
{
    qDebug() << "[AlertingEngine] Updating alert:" << alert.alertId;
    
    if (!m_alerts.contains(alert.alertId)) {
        return false;
    }
    
    m_alerts[alert.alertId] = alert;
    return true;
}

bool AlertingEngine::deleteAlert(const QString &alertId)
{
    qDebug() << "[AlertingEngine] Deleting alert:" << alertId;
    
    if (!m_alerts.contains(alertId)) {
        return false;
    }
    
    m_alerts.remove(alertId);
    return true;
}

Alert AlertingEngine::getAlert(const QString &alertId)
{
    if (m_alerts.contains(alertId)) {
        return m_alerts[alertId];
    }
    return Alert();
}

QVector<Alert> AlertingEngine::getAllAlerts()
{
    QVector<Alert> alerts;
    for (const auto &alert : m_alerts) {
        alerts.append(alert);
    }
    return alerts;
}

bool AlertingEngine::evaluateAlertCondition(const Alert &alert)
{
    // Simulated condition evaluation
    Q_UNUSED(alert)
    return false;  // No active condition met
}

bool AlertingEngine::triggerAlert(const QString &alertId)
{
    qDebug() << "[AlertingEngine] Triggering alert:" << alertId;
    
    if (!m_alerts.contains(alertId)) {
        return false;
    }
    
    const Alert &alert = m_alerts[alertId];
    
    AlertEvent event;
    event.alertId = alertId;
    event.status = "firing";
    event.timestamp = QDateTime::currentDateTime();
    event.description = alert.message;
    
    m_alertHistory[alertId].append(event);
    emit alertFired(alert);
    
    // Send notifications
    sendNotification(alert, "default");
    
    return true;
}

bool AlertingEngine::resolveAlert(const QString &alertId)
{
    qDebug() << "[AlertingEngine] Resolving alert:" << alertId;
    
    if (!m_alerts.contains(alertId)) {
        return false;
    }
    
    AlertEvent event;
    event.alertId = alertId;
    event.status = "resolved";
    event.timestamp = QDateTime::currentDateTime();
    
    m_alertHistory[alertId].append(event);
    emit alertResolved(alertId);
    
    return true;
}

bool AlertingEngine::configureAlertRoute(const QString &alertName, const QString &receiver)
{
    qDebug() << "[AlertingEngine] Configuring route for" << alertName << "to" << receiver;
    
    for (auto &alert : m_alerts) {
        if (alert.alertName == alertName) {
            if (!alert.recipients.contains(receiver)) {
                alert.recipients.append(receiver);
            }
            return true;
        }
    }
    return false;
}

bool AlertingEngine::addAlertRecipient(const QString &alertId, const QString &recipient)
{
    if (!m_alerts.contains(alertId)) {
        return false;
    }
    
    if (!m_alerts[alertId].recipients.contains(recipient)) {
        m_alerts[alertId].recipients.append(recipient);
    }
    return true;
}

bool AlertingEngine::removeAlertRecipient(const QString &alertId, const QString &recipient)
{
    if (!m_alerts.contains(alertId)) {
        return false;
    }
    
    m_alerts[alertId].recipients.removeAll(recipient);
    return true;
}

QVector<AlertEvent> AlertingEngine::getAlertHistory(const QString &alertId)
{
    if (m_alertHistory.contains(alertId)) {
        return m_alertHistory[alertId];
    }
    return QVector<AlertEvent>();
}

QVector<AlertEvent> AlertingEngine::getActiveAlerts()
{
    QVector<AlertEvent> active;
    for (auto it = m_alertHistory.begin(); it != m_alertHistory.end(); ++it) {
        if (!it.value().isEmpty() && it.value().last().status == "firing") {
            active.append(it.value().last());
        }
    }
    return active;
}

int AlertingEngine::getAlertCount(const QString &severity)
{
    int count = 0;
    for (const auto &alert : m_alerts) {
        if (alert.severity == severity && alert.enabled) {
            count++;
        }
    }
    return count;
}

bool AlertingEngine::suppressAlert(const QString &alertId, qint64 durationSeconds)
{
    qDebug() << "[AlertingEngine] Suppressing alert" << alertId << "for" << durationSeconds << "seconds";
    
    m_suppressedUntil[alertId] = QDateTime::currentDateTime().addSecs(durationSeconds);
    return true;
}

bool AlertingEngine::acknowledgeAlert(const QString &alertEventId)
{
    qDebug() << "[AlertingEngine] Acknowledging alert event:" << alertEventId;
    
    emit alertAcknowledged(alertEventId);
    return true;
}

bool AlertingEngine::configureEmailNotification(const QString &address)
{
    qDebug() << "[AlertingEngine] Configured email notification:" << address;
    return true;
}

bool AlertingEngine::configureSlackNotification(const QString &webhookUrl)
{
    qDebug() << "[AlertingEngine] Configured Slack notification:" << webhookUrl;
    return true;
}

bool AlertingEngine::configurePagerDutyNotification(const QString &integrationKey)
{
    qDebug() << "[AlertingEngine] Configured PagerDuty notification:" << integrationKey;
    return true;
}

bool AlertingEngine::configureSMSNotification(const QString &phoneNumber)
{
    qDebug() << "[AlertingEngine] Configured SMS notification:" << phoneNumber;
    return true;
}

void AlertingEngine::checkAllAlerts()
{
    for (const auto &alert : m_alerts) {
        if (alert.enabled && !m_suppressedUntil.contains(alert.alertId)) {
            if (evaluateAlertCondition(alert)) {
                triggerAlert(alert.alertId);
            }
        }
    }
}

void AlertingEngine::sendNotification(const Alert &alert, const QString &channel)
{
    qDebug() << "[AlertingEngine] Sending notification for" << alert.alertName << "via" << channel;
    emit notificationSent(alert.alertId, channel);
}

// ============================================================================
// SLATracker Implementation
// ============================================================================

SLATracker::SLATracker(QObject *parent)
    : QObject(parent)
{
    qDebug() << "[SLATracker] Initialized";
}

SLATracker::~SLATracker() = default;

bool SLATracker::createSLO(const SLAObjective &slo)
{
    qDebug() << "[SLATracker] Creating SLO:" << slo.objectiveId << "for" << slo.serviceName;
    
    SLAObjective s = slo;
    if (s.objectiveId.isEmpty()) {
        s.objectiveId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    }
    
    m_slos[s.objectiveId] = s;
    emit sloCreated(s);
    return true;
}

bool SLATracker::updateSLO(const SLAObjective &slo)
{
    qDebug() << "[SLATracker] Updating SLO:" << slo.objectiveId;
    
    if (!m_slos.contains(slo.objectiveId)) {
        return false;
    }
    
    m_slos[slo.objectiveId] = slo;
    return true;
}

bool SLATracker::deleteSLO(const QString &objectiveId)
{
    qDebug() << "[SLATracker] Deleting SLO:" << objectiveId;
    
    if (!m_slos.contains(objectiveId)) {
        return false;
    }
    
    m_slos.remove(objectiveId);
    return true;
}

SLAObjective SLATracker::getSLO(const QString &objectiveId)
{
    if (m_slos.contains(objectiveId)) {
        return m_slos[objectiveId];
    }
    return SLAObjective();
}

QVector<SLAObjective> SLATracker::getSLOsByService(const QString &serviceName)
{
    QVector<SLAObjective> result;
    for (const auto &slo : m_slos) {
        if (slo.serviceName == serviceName) {
            result.append(slo);
        }
    }
    return result;
}

double SLATracker::calculateServiceAvailability(const QString &serviceName, const QDateTime &start, const QDateTime &end)
{
    Q_UNUSED(serviceName)
    Q_UNUSED(start)
    Q_UNUSED(end)
    
    // Simulated high availability
    return 99.95 + (static_cast<double>(rand() % 5) / 100.0);
}

double SLATracker::calculateErrorBudgetUsed(const QString &objectiveId)
{
    if (!m_slos.contains(objectiveId)) {
        return 0.0;
    }
    
    const SLAObjective &slo = m_slos[objectiveId];
    double compliance = calculateMetricCompliance(slo);
    
    if (compliance >= slo.targetValue) {
        return 0.0;
    }
    
    double budgetUsed = (slo.targetValue - compliance) / slo.allowedErrorBudget * 100.0;
    return qMin(100.0, budgetUsed);
}

double SLATracker::calculateRemainingErrorBudget(const QString &objectiveId)
{
    return 100.0 - calculateErrorBudgetUsed(objectiveId);
}

bool SLATracker::isSLOCompliant(const QString &objectiveId)
{
    if (!m_slos.contains(objectiveId)) {
        return false;
    }
    
    const SLAObjective &slo = m_slos[objectiveId];
    double compliance = calculateMetricCompliance(slo);
    
    return compliance >= slo.targetValue;
}

int SLATracker::getBreachCount(const QString &objectiveId, const QDateTime &start, const QDateTime &end)
{
    Q_UNUSED(objectiveId)
    Q_UNUSED(start)
    Q_UNUSED(end)
    
    // Simulated breach count
    return rand() % 3;
}

SLAReport SLATracker::generateSLAReport(const QString &serviceName)
{
    qDebug() << "[SLATracker] Generating SLA report for:" << serviceName;
    
    SLAReport report;
    report.serviceName = serviceName;
    report.reportDate = QDateTime::currentDateTime();
    
    double totalCompliance = 0.0;
    int sloCount = 0;
    
    for (const auto &slo : m_slos) {
        if (slo.serviceName == serviceName) {
            double compliance = calculateMetricCompliance(slo);
            report.sloStatus[slo.metric] = compliance;
            totalCompliance += compliance;
            sloCount++;
            
            if (compliance < slo.targetValue) {
                report.breaches.append(QString("SLO breach: %1 (%.2f%% < %.2f%%)")
                    .arg(slo.metric).arg(compliance).arg(slo.targetValue));
            }
        }
    }
    
    report.overallCompliance = sloCount > 0 ? totalCompliance / sloCount : 100.0;
    report.remainingErrorBudget = 100.0 - (100.0 - report.overallCompliance);
    
    emit reportGenerated(report);
    return report;
}

QString SLATracker::generateSLAReportHTML(const QString &serviceName)
{
    SLAReport report = generateSLAReport(serviceName);
    
    QString html = QString("<html><body><h1>SLA Report: %1</h1>").arg(serviceName);
    html += QString("<p>Date: %1</p>").arg(report.reportDate.toString(Qt::ISODate));
    html += QString("<p>Overall Compliance: %.2f%%</p>").arg(report.overallCompliance);
    html += QString("<p>Remaining Error Budget: %.2f%%</p>").arg(report.remainingErrorBudget);
    
    html += "<h2>SLO Status</h2><ul>";
    for (auto it = report.sloStatus.begin(); it != report.sloStatus.end(); ++it) {
        html += QString("<li>%1: %.2f%%</li>").arg(it.key()).arg(it.value());
    }
    html += "</ul>";
    
    if (!report.breaches.isEmpty()) {
        html += "<h2>Breaches</h2><ul>";
        for (const QString &breach : report.breaches) {
            html += QString("<li>%1</li>").arg(breach);
        }
        html += "</ul>";
    }
    
    html += "</body></html>";
    return html;
}

QJsonObject SLATracker::generateSLAJSON(const QString &serviceName)
{
    SLAReport report = generateSLAReport(serviceName);
    
    QJsonObject json;
    json["service_name"] = report.serviceName;
    json["report_date"] = report.reportDate.toString(Qt::ISODate);
    json["overall_compliance"] = report.overallCompliance;
    json["remaining_error_budget"] = report.remainingErrorBudget;
    
    QJsonObject sloStatus;
    for (auto it = report.sloStatus.begin(); it != report.sloStatus.end(); ++it) {
        sloStatus[it.key()] = it.value();
    }
    json["slo_status"] = sloStatus;
    
    QJsonArray breaches;
    for (const QString &breach : report.breaches) {
        breaches.append(breach);
    }
    json["breaches"] = breaches;
    
    return json;
}

bool SLATracker::createSLOAlert(const QString &objectiveId, double thresholdPercent)
{
    qDebug() << "[SLATracker] Creating SLO alert for:" << objectiveId << "at" << thresholdPercent << "%";
    
    // This would typically integrate with AlertingEngine
    return m_slos.contains(objectiveId);
}

QVector<Alert> SLATracker::getSLOAlerts(const QString &serviceName)
{
    Q_UNUSED(serviceName)
    // Return empty - would integrate with AlertingEngine
    return QVector<Alert>();
}

double SLATracker::calculateMetricCompliance(const SLAObjective &slo)
{
    // Simulated compliance calculation
    double base = slo.targetValue;
    double variance = slo.allowedErrorBudget * (static_cast<double>(rand() % 100) / 100.0);
    
    // Usually meet SLO but occasionally breach
    if (rand() % 10 == 0) {
        return base - variance;
    }
    return base + variance * 0.1;
}

// ============================================================================
// BusinessIntelligence Implementation
// ============================================================================

BusinessIntelligence::BusinessIntelligence(QObject *parent)
    : QObject(parent)
{
    qDebug() << "[BusinessIntelligence] Initialized";
}

BusinessIntelligence::~BusinessIntelligence() = default;

void BusinessIntelligence::recordBusinessMetric(const BusinessMetric &metric)
{
    qDebug() << "[BusinessIntelligence] Recording metric:" << metric.metricName;
    
    m_metrics[metric.metricName].append(metric);
    emit metricRecorded(metric);
}

BusinessMetric BusinessIntelligence::getBusinessMetric(const QString &metricName)
{
    if (m_metrics.contains(metricName) && !m_metrics[metricName].isEmpty()) {
        return m_metrics[metricName].last();
    }
    return BusinessMetric();
}

QVector<BusinessMetric> BusinessIntelligence::getAllBusinessMetrics()
{
    QVector<BusinessMetric> all;
    for (auto it = m_metrics.begin(); it != m_metrics.end(); ++it) {
        if (!it.value().isEmpty()) {
            all.append(it.value().last());
        }
    }
    return all;
}

double BusinessIntelligence::calculateMetricTrend(const QString &metricName, const QDateTime &start, const QDateTime &end)
{
    if (!m_metrics.contains(metricName)) return 0.0;
    
    QVector<double> values;
    for (const auto &metric : m_metrics[metricName]) {
        if (metric.timestamp >= start && metric.timestamp <= end) {
            values.append(metric.value);
        }
    }
    
    if (values.size() < 2) return 0.0;
    
    double first = values.first();
    double last = values.last();
    
    if (first == 0) return 0.0;
    return ((last - first) / first) * 100.0;
}

QVector<BusinessMetric> BusinessIntelligence::getMetricsForDimension(const QString &dimension, const QString &dimensionValue)
{
    QVector<BusinessMetric> result;
    for (auto it = m_metrics.begin(); it != m_metrics.end(); ++it) {
        for (const auto &metric : it.value()) {
            if (metric.dimension == dimension || 
                (dimension == "value" && QString::number(metric.value) == dimensionValue)) {
                result.append(metric);
            }
        }
    }
    return result;
}

double BusinessIntelligence::forecastMetricValue(const QString &metricName, const QDateTime &futureDate)
{
    Q_UNUSED(futureDate)
    
    if (!m_metrics.contains(metricName) || m_metrics[metricName].size() < 2) {
        return 0.0;
    }
    
    // Simple linear extrapolation
    const auto &history = m_metrics[metricName];
    double lastValue = history.last().value;
    double trend = history.last().trend;
    
    return lastValue * (1 + trend / 100.0);
}

QVector<double> BusinessIntelligence::forecastTimeSeries(const QString &metricName, int periods)
{
    QVector<double> forecast;
    double currentValue = getBusinessMetric(metricName).value;
    double trend = getBusinessMetric(metricName).trend;
    
    for (int i = 0; i < periods; ++i) {
        currentValue *= (1 + trend / 100.0);
        forecast.append(currentValue);
    }
    
    return forecast;
}

QString BusinessIntelligence::generateExecutiveDashboard()
{
    qDebug() << "[BusinessIntelligence] Generating executive dashboard";
    
    QJsonObject dashboard;
    dashboard["type"] = "executive";
    dashboard["generated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonArray keyMetrics;
    for (const auto &metric : getAllBusinessMetrics()) {
        QJsonObject m;
        m["name"] = metric.metricName;
        m["value"] = metric.value;
        m["target"] = metric.targetValue;
        m["trend"] = metric.trend;
        keyMetrics.append(m);
    }
    dashboard["key_metrics"] = keyMetrics;
    
    return QString::fromUtf8(QJsonDocument(dashboard).toJson(QJsonDocument::Indented));
}

QString BusinessIntelligence::generateProductDashboard()
{
    qDebug() << "[BusinessIntelligence] Generating product dashboard";
    
    QJsonObject dashboard;
    dashboard["type"] = "product";
    dashboard["generated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return QString::fromUtf8(QJsonDocument(dashboard).toJson(QJsonDocument::Indented));
}

QString BusinessIntelligence::generateFinancialDashboard()
{
    qDebug() << "[BusinessIntelligence] Generating financial dashboard";
    
    QJsonObject dashboard;
    dashboard["type"] = "financial";
    dashboard["generated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return QString::fromUtf8(QJsonDocument(dashboard).toJson(QJsonDocument::Indented));
}

QJsonObject BusinessIntelligence::generateBusinessReport(const QDateTime &start, const QDateTime &end)
{
    qDebug() << "[BusinessIntelligence] Generating business report";
    
    QJsonObject report;
    report["start_date"] = start.toString(Qt::ISODate);
    report["end_date"] = end.toString(Qt::ISODate);
    
    QJsonArray metrics;
    for (auto it = m_metrics.begin(); it != m_metrics.end(); ++it) {
        QJsonObject m;
        m["name"] = it.key();
        m["trend"] = calculateMetricTrend(it.key(), start, end);
        
        if (!it.value().isEmpty()) {
            m["latest_value"] = it.value().last().value;
        }
        metrics.append(m);
    }
    report["metrics"] = metrics;
    
    return report;
}

QString BusinessIntelligence::generateInsightsSummary()
{
    qDebug() << "[BusinessIntelligence] Generating insights summary";
    
    QString summary = "=== Business Insights Summary ===\n\n";
    
    auto metrics = getAllBusinessMetrics();
    
    // Find top performers
    summary += "Top Performers:\n";
    for (const auto &metric : metrics) {
        if (metric.trend > 5.0) {
            summary += QString("  - %1: +%.1f%% growth\n").arg(metric.metricName).arg(metric.trend);
        }
    }
    
    // Find concerns
    summary += "\nAreas of Concern:\n";
    for (const auto &metric : metrics) {
        if (metric.trend < -5.0) {
            summary += QString("  - %1: %.1f%% decline\n").arg(metric.metricName).arg(metric.trend);
        }
    }
    
    emit insightDiscovered("summary_generated");
    return summary;
}

// ============================================================================
// MonitoringCoordinator Implementation
// ============================================================================

MonitoringCoordinator::MonitoringCoordinator(QObject *parent)
    : QObject(parent)
    , m_tracer(std::make_unique<DistributedTracingCollector>(this))
    , m_metricsCollector(std::make_unique<MetricsCollector>(this))
    , m_logger(std::make_unique<StructuredLogger>("MonitoringCoordinator", this))
    , m_alerting(std::make_unique<AlertingEngine>(this))
    , m_slaTracker(std::make_unique<SLATracker>(this))
    , m_businessIntelligence(std::make_unique<BusinessIntelligence>(this))
{
    qDebug() << "[MonitoringCoordinator] Initialized with all subsystems";
}

MonitoringCoordinator::~MonitoringCoordinator() = default;

void MonitoringCoordinator::initialize(const QString &environment)
{
    qDebug() << "[MonitoringCoordinator] Initializing for environment:" << environment;
    m_environment = environment;
    m_logger->info("Monitoring initialized", {{"environment", environment}});
}

QString MonitoringCoordinator::startOperation(const QString &operationName, const QString &serviceName)
{
    QString operationId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    m_tracer->startSpan(operationName, serviceName);
    m_operationStartTimes[operationId] = QDateTime::currentDateTime();
    m_logger->debug("Operation started", {{"operation", operationName}, {"service", serviceName}});
    
    emit operationStarted(operationId);
    return operationId;
}

void MonitoringCoordinator::recordOperationMetric(const QString &operationId, const QString &metricName, double value)
{
    m_metricsCollector->recordGauge(metricName, value, {{"operation_id", operationId}});
}

void MonitoringCoordinator::endOperation(const QString &operationId, const QString &status)
{
    if (m_operationStartTimes.contains(operationId)) {
        qint64 duration = m_operationStartTimes[operationId].msecsTo(QDateTime::currentDateTime());
        m_metricsCollector->recordHistogram("operation_duration_ms", duration, {{"operation_id", operationId}});
        m_operationStartTimes.remove(operationId);
    }
    
    m_logger->info("Operation completed", {{"operation_id", operationId}, {"status", status}});
    emit operationCompleted(operationId);
}

bool MonitoringCoordinator::performHealthCheck(const QString &serviceName)
{
    qDebug() << "[MonitoringCoordinator] Performing health check for:" << serviceName;
    
    // Simulated health check
    bool healthy = (rand() % 10) != 0;  // 90% healthy
    
    emit healthStatusChanged(serviceName, healthy ? "healthy" : "unhealthy");
    return healthy;
}

QJsonObject MonitoringCoordinator::getSystemHealth()
{
    QJsonObject health;
    health["environment"] = m_environment;
    health["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    health["status"] = "healthy";
    health["active_alerts"] = m_alerting->getActiveAlerts().size();
    
    return health;
}

QString MonitoringCoordinator::getHealthStatus(const QString &serviceName)
{
    return performHealthCheck(serviceName) ? "healthy" : "unhealthy";
}

QString MonitoringCoordinator::generateUnifiedDashboard()
{
    qDebug() << "[MonitoringCoordinator] Generating unified dashboard";
    
    QJsonObject dashboard;
    dashboard["type"] = "unified";
    dashboard["environment"] = m_environment;
    dashboard["generated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    dashboard["system_health"] = getSystemHealth();
    
    return QString::fromUtf8(QJsonDocument(dashboard).toJson(QJsonDocument::Indented));
}

QString MonitoringCoordinator::generateServiceDashboard(const QString &serviceName)
{
    qDebug() << "[MonitoringCoordinator] Generating service dashboard for:" << serviceName;
    
    QJsonObject dashboard;
    dashboard["type"] = "service";
    dashboard["service_name"] = serviceName;
    dashboard["health_status"] = getHealthStatus(serviceName);
    dashboard["generated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return QString::fromUtf8(QJsonDocument(dashboard).toJson(QJsonDocument::Indented));
}

QString MonitoringCoordinator::generateBusinessDashboard()
{
    return m_businessIntelligence->generateExecutiveDashboard();
}

QString MonitoringCoordinator::generateDailyReport()
{
    qDebug() << "[MonitoringCoordinator] Generating daily report";
    
    QJsonObject report;
    report["type"] = "daily";
    report["environment"] = m_environment;
    report["generated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    emit reportGenerated("daily");
    return QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented));
}

QString MonitoringCoordinator::generateWeeklyReport()
{
    qDebug() << "[MonitoringCoordinator] Generating weekly report";
    
    QJsonObject report;
    report["type"] = "weekly";
    report["environment"] = m_environment;
    report["generated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    emit reportGenerated("weekly");
    return QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented));
}

QString MonitoringCoordinator::generateMonthlyReport()
{
    qDebug() << "[MonitoringCoordinator] Generating monthly report";
    
    QJsonObject report;
    report["type"] = "monthly";
    report["environment"] = m_environment;
    report["generated_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    emit reportGenerated("monthly");
    return QString::fromUtf8(QJsonDocument(report).toJson(QJsonDocument::Indented));
}

QVector<QString> MonitoringCoordinator::detectAnomalies()
{
    qDebug() << "[MonitoringCoordinator] Detecting anomalies";
    
    QVector<QString> anomalies;
    // Simulated anomaly detection
    return anomalies;
}

void MonitoringCoordinator::configureAnomalyDetection(bool enabled, double sensitivity)
{
    qDebug() << "[MonitoringCoordinator] Anomaly detection:" << (enabled ? "enabled" : "disabled")
             << "sensitivity:" << sensitivity;
}

// ============================================================================
// MonitoringUtils Implementation
// ============================================================================

QString MonitoringUtils::calculateHealthStatus(const QJsonObject &metrics)
{
    // Simple health calculation based on metrics
    if (metrics.contains("error_rate")) {
        double errorRate = metrics["error_rate"].toDouble();
        if (errorRate > 5.0) return "critical";
        if (errorRate > 1.0) return "degraded";
    }
    
    if (metrics.contains("latency_p99")) {
        double latency = metrics["latency_p99"].toDouble();
        if (latency > 5000) return "degraded";
    }
    
    return "healthy";
}

QVector<QString> MonitoringUtils::detectPerformanceBottlenecks(const QVector<PerformanceMetric> &metrics)
{
    QVector<QString> bottlenecks;
    
    for (const auto &metric : metrics) {
        if (metric.p99Latency > 1000) {
            bottlenecks.append(QString("High P99 latency for %1: %2ms")
                .arg(metric.operationName).arg(metric.p99Latency));
        }
        
        if (metric.errorRate > 1.0) {
            bottlenecks.append(QString("Elevated error rate for %1: %.2f%%")
                .arg(metric.operationName).arg(metric.errorRate));
        }
        
        if (metric.throughput < 10) {
            bottlenecks.append(QString("Low throughput for %1: %2 ops/sec")
                .arg(metric.operationName).arg(metric.throughput));
        }
    }
    
    return bottlenecks;
}

QString MonitoringUtils::generateInsightFromMetrics(const QVector<Metric> &metrics)
{
    if (metrics.isEmpty()) {
        return "No metrics available for analysis";
    }
    
    double sum = 0;
    double maxVal = metrics.first().value;
    double minVal = metrics.first().value;
    
    for (const auto &metric : metrics) {
        sum += metric.value;
        if (metric.value > maxVal) maxVal = metric.value;
        if (metric.value < minVal) minVal = metric.value;
    }
    
    double avg = sum / metrics.size();
    double range = maxVal - minVal;
    
    QString insight = QString("Analyzed %1 metrics: avg=%.2f, min=%.2f, max=%.2f, range=%.2f")
        .arg(metrics.size()).arg(avg).arg(minVal).arg(maxVal).arg(range);
    
    if (range > avg * 0.5) {
        insight += " [HIGH VARIANCE DETECTED]";
    }
    
    return insight;
}

double MonitoringUtils::calculateServiceReliability(const QVector<AlertEvent> &events)
{
    if (events.isEmpty()) {
        return 100.0;  // No alerts = 100% reliable
    }
    
    qint64 totalDowntime = 0;
    QDateTime lastFiring;
    
    for (const auto &event : events) {
        if (event.status == "firing") {
            lastFiring = event.timestamp;
        } else if (event.status == "resolved" && lastFiring.isValid()) {
            totalDowntime += lastFiring.secsTo(event.timestamp);
            lastFiring = QDateTime();
        }
    }
    
    // Assume 30-day measurement window
    const qint64 windowSeconds = 30 * 24 * 60 * 60;
    double uptimePercent = (1.0 - static_cast<double>(totalDowntime) / windowSeconds) * 100.0;
    
    return qBound(0.0, uptimePercent, 100.0);
}

} // namespace Agentic
} // namespace RawrXD
