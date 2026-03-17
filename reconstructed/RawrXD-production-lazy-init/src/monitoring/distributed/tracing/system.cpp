// Distributed Tracing System Implementation
#include "distributed_tracing_system.h"
#include <QDebug>
#include <QRandomGenerator>
#include <QJsonDocument>
#include <QFile>
#include <QDir>
#include <QElapsedTimer>
#include <QTimer>

namespace Tracing {

// ============================================================================
// Span Implementation
// ============================================================================

QJsonObject Span::toJson() const {
    QJsonObject json;
    json["spanId"] = spanId;
    json["traceId"] = traceId;
    json["parentSpanId"] = parentSpanId;
    json["operationName"] = operationName;
    json["serviceName"] = serviceName;
    json["startTime"] = startTime.toString(Qt::ISODate);
    json["endTime"] = endTime.toString(Qt::ISODate);
    json["durationMicros"] = durationMicros;
    json["tags"] = tags;
    json["events"] = events;
    json["baggage"] = baggage;
    json["isError"] = isError;
    json["errorMessage"] = errorMessage;
    json["errorDetails"] = errorDetails;
    return json;
}

Span Span::fromJson(const QJsonObject& json) {
    Span span;
    span.spanId = json["spanId"].toString();
    span.traceId = json["traceId"].toString();
    span.parentSpanId = json["parentSpanId"].toString();
    span.operationName = json["operationName"].toString();
    span.serviceName = json["serviceName"].toString();
    span.startTime = QDateTime::fromString(json["startTime"].toString(), Qt::ISODate);
    span.endTime = QDateTime::fromString(json["endTime"].toString(), Qt::ISODate);
    span.durationMicros = json["durationMicros"].toVariant().toLongLong();
    span.tags = json["tags"].toObject();
    span.events = json["events"].toArray();
    span.baggage = json["baggage"].toObject();
    span.isError = json["isError"].toBool();
    span.errorMessage = json["errorMessage"].toString();
    span.errorDetails = json["errorDetails"].toObject();
    return span;
}

// ============================================================================
// Trace Implementation
// ============================================================================

QJsonArray Trace::toJson() const {
    QJsonArray array;
    for (const Span& span : spans) {
        array.append(span.toJson());
    }
    return array;
}

QVector<Span> Trace::getRootSpans() const {
    QVector<Span> roots;
    for (const Span& span : spans) {
        if (span.parentSpanId.isEmpty()) {
            roots.append(span);
        }
    }
    return roots;
}

QVector<Span> Trace::getChildSpans(const QString& parentSpanId) const {
    QVector<Span> children;
    for (const Span& span : spans) {
        if (span.parentSpanId == parentSpanId) {
            children.append(span);
        }
    }
    return children;
}

Span Trace::getCriticalPath() const {
    if (spans.isEmpty()) {
        return Span();
    }
    
    // Return the span with the longest duration
    Span longest = spans.first();
    for (const Span& span : spans) {
        if (span.durationMicros > longest.durationMicros) {
            longest = span;
        }
    }
    return longest;
}

double Trace::calculateServiceTime(const QString& serviceName) const {
    double totalTime = 0.0;
    for (const Span& span : spans) {
        if (span.serviceName == serviceName) {
            totalTime += span.durationMicros / 1000.0; // Convert to ms
        }
    }
    return totalTime;
}

// ============================================================================
// TraceContext Implementation
// ============================================================================

QString TraceContext::toHeader() const {
    return QString("00-%1-%2-%3").arg(traceId, spanId, sampled ? "01" : "00");
}

TraceContext TraceContext::fromHeader(const QString& header) {
    TraceContext ctx;
    QStringList parts = header.split('-');
    if (parts.size() >= 4) {
        ctx.traceId = parts[1];
        ctx.spanId = parts[2];
        ctx.sampled = parts[3] == "01";
    }
    return ctx;
}

// ============================================================================
// SpanBuilder Implementation
// ============================================================================

SpanBuilder::SpanBuilder(const QString& operationName, const QString& serviceName) {
    m_span.operationName = operationName;
    m_span.serviceName = serviceName;
}

SpanBuilder& SpanBuilder::setTraceId(const QString& traceId) {
    m_span.traceId = traceId;
    return *this;
}

SpanBuilder& SpanBuilder::setParentSpanId(const QString& parentSpanId) {
    m_span.parentSpanId = parentSpanId;
    return *this;
}

SpanBuilder& SpanBuilder::addTag(const QString& key, const QVariant& value) {
    m_span.tags[key] = QJsonValue::fromVariant(value);
    return *this;
}

SpanBuilder& SpanBuilder::addBaggage(const QString& key, const QString& value) {
    m_span.baggage[key] = value;
    return *this;
}

SpanBuilder& SpanBuilder::setError(bool isError) {
    m_span.isError = isError;
    return *this;
}

Span SpanBuilder::start() {
    m_span.startTime = QDateTime::currentDateTime();
    return m_span;
}

// ============================================================================
// DistributedTracingSystem Implementation
// ============================================================================

DistributedTracingSystem::DistributedTracingSystem(QObject* parent)
    : QObject(parent)
    , m_samplingRate(1.0)
    , m_maxTraceSize(1000)
    , m_traceRetentionDays(7)
    , m_autoFlushEnabled(false)
    , m_flushTimer(nullptr)
    , m_totalSpansRecorded(0)
    , m_totalTracesRecorded(0)
    , m_spansDropped(0)
{
    m_startTime = QDateTime::currentDateTime();
    qInfo() << "[DistributedTracingSystem] Created";
}

DistributedTracingSystem::~DistributedTracingSystem() {
    shutdown();
    qInfo() << "[DistributedTracingSystem] Destroyed";
}

void DistributedTracingSystem::initialize(const QString& serviceName, const QString& serviceVersion) {
    QMutexLocker lock(&m_mutex);
    
    m_serviceName = serviceName;
    m_serviceVersion = serviceVersion;
    
    qInfo() << "[DistributedTracingSystem] Initialized for service:" << serviceName 
            << "version:" << serviceVersion;
}

void DistributedTracingSystem::shutdown() {
    QMutexLocker lock(&m_mutex);
    
    if (m_flushTimer) {
        m_flushTimer->stop();
        delete m_flushTimer;
        m_flushTimer = nullptr;
    }
    
    // Flush pending spans
    flushTraces();
    
    qInfo() << "[DistributedTracingSystem] Shutdown complete. Total spans:" << m_totalSpansRecorded
            << "Total traces:" << m_totalTracesRecorded;
}

Span DistributedTracingSystem::startSpan(const QString& operationName, const TraceContext* parentContext) {
    Span span;
    span.operationName = operationName;
    span.serviceName = m_serviceName;
    span.startTime = QDateTime::currentDateTime();
    
    if (parentContext) {
        span.traceId = parentContext->traceId;
        span.parentSpanId = parentContext->spanId;
        span.baggage = parentContext->baggage;
    } else {
        span.traceId = generateTraceId();
    }
    
    span.spanId = generateSpanId();
    
    return span;
}

void DistributedTracingSystem::finishSpan(Span& span) {
    span.endTime = QDateTime::currentDateTime();
    span.durationMicros = span.startTime.msecsTo(span.endTime) * 1000;
    
    recordSpan(span);
}

void DistributedTracingSystem::recordSpan(const Span& span) {
    QMutexLocker lock(&m_mutex);
    
    if (!shouldSample()) {
        m_spansDropped++;
        return;
    }
    
    m_spansByTrace[span.traceId].append(span);
    m_totalSpansRecorded++;
    
    emit spanRecorded(span);
    
    // Check if trace is complete (root span finished)
    if (span.parentSpanId.isEmpty()) {
        buildTraceTree(m_completedTraces[span.traceId]);
        m_totalTracesRecorded++;
        emit traceCompleted(span.traceId);
    }
}

SpanBuilder DistributedTracingSystem::buildSpan(const QString& operationName) {
    return SpanBuilder(operationName, m_serviceName);
}

Trace DistributedTracingSystem::getTrace(const QString& traceId) const {
    QMutexLocker lock(&m_mutex);
    
    if (m_completedTraces.contains(traceId)) {
        return m_completedTraces[traceId];
    }
    
    // Build trace from pending spans
    Trace trace;
    trace.traceId = traceId;
    if (m_spansByTrace.contains(traceId)) {
        trace.spans = m_spansByTrace[traceId];
    }
    return trace;
}

QVector<Trace> DistributedTracingSystem::getTraces(const QDateTime& startTime, const QDateTime& endTime) const {
    QMutexLocker lock(&m_mutex);
    
    QVector<Trace> result;
    for (const Trace& trace : m_completedTraces.values()) {
        if (trace.startTime >= startTime && trace.endTime <= endTime) {
            result.append(trace);
        }
    }
    return result;
}

QVector<Trace> DistributedTracingSystem::getTracesByService(const QString& serviceName) const {
    QMutexLocker lock(&m_mutex);
    
    QVector<Trace> result;
    for (const Trace& trace : m_completedTraces.values()) {
        if (trace.rootServiceName == serviceName) {
            result.append(trace);
        }
    }
    return result;
}

QVector<Trace> DistributedTracingSystem::getTracesByOperation(const QString& operationName) const {
    QMutexLocker lock(&m_mutex);
    
    QVector<Trace> result;
    for (const Trace& trace : m_completedTraces.values()) {
        if (trace.rootOperationName == operationName) {
            result.append(trace);
        }
    }
    return result;
}

QJsonObject DistributedTracingSystem::analyzeTrace(const QString& traceId) {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject analysis;
    analysis["traceId"] = traceId;
    
    if (!m_completedTraces.contains(traceId)) {
        analysis["error"] = "Trace not found";
        return analysis;
    }
    
    const Trace& trace = m_completedTraces[traceId];
    
    analysis["spanCount"] = trace.spans.size();
    analysis["totalDurationMs"] = trace.totalDurationMicros / 1000.0;
    
    // Calculate service breakdown
    QJsonObject serviceBreakdown;
    QMap<QString, double> serviceTimes;
    for (const Span& span : trace.spans) {
        serviceTimes[span.serviceName] += span.durationMicros / 1000.0;
    }
    for (auto it = serviceTimes.begin(); it != serviceTimes.end(); ++it) {
        serviceBreakdown[it.key()] = it.value();
    }
    analysis["serviceBreakdown"] = serviceBreakdown;
    
    return analysis;
}

QJsonArray DistributedTracingSystem::findBottlenecks(const QString& traceId) {
    QMutexLocker lock(&m_mutex);
    
    QJsonArray bottlenecks;
    
    if (!m_completedTraces.contains(traceId)) {
        return bottlenecks;
    }
    
    const Trace& trace = m_completedTraces[traceId];
    
    // Find spans that took > 80% of total time
    double threshold = trace.totalDurationMicros * 0.8;
    for (const Span& span : trace.spans) {
        if (span.durationMicros >= threshold) {
            QJsonObject bottleneck;
            bottleneck["spanId"] = span.spanId;
            bottleneck["operationName"] = span.operationName;
            bottleneck["serviceName"] = span.serviceName;
            bottleneck["durationMs"] = span.durationMicros / 1000.0;
            bottleneck["percentage"] = (span.durationMicros * 100.0) / trace.totalDurationMicros;
            bottlenecks.append(bottleneck);
        }
    }
    
    return bottlenecks;
}

QJsonObject DistributedTracingSystem::getServiceDependencies() {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject dependencies;
    QMap<QString, QSet<QString>> depGraph;
    
    for (const Trace& trace : m_completedTraces.values()) {
        for (const Span& span : trace.spans) {
            if (!span.parentSpanId.isEmpty()) {
                // Find parent span
                for (const Span& parent : trace.spans) {
                    if (parent.spanId == span.parentSpanId) {
                        depGraph[parent.serviceName].insert(span.serviceName);
                        break;
                    }
                }
            }
        }
    }
    
    for (auto it = depGraph.begin(); it != depGraph.end(); ++it) {
        QJsonArray deps;
        for (const QString& dep : it.value()) {
            deps.append(dep);
        }
        dependencies[it.key()] = deps;
    }
    
    return dependencies;
}

QJsonObject DistributedTracingSystem::getOperationStatistics(const QString& operationName) {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject stats;
    stats["operationName"] = operationName;
    
    QVector<double> durations;
    int errorCount = 0;
    
    for (const Trace& trace : m_completedTraces.values()) {
        for (const Span& span : trace.spans) {
            if (span.operationName == operationName) {
                durations.append(span.durationMicros / 1000.0);
                if (span.isError) {
                    errorCount++;
                }
            }
        }
    }
    
    if (durations.isEmpty()) {
        stats["count"] = 0;
        return stats;
    }
    
    std::sort(durations.begin(), durations.end());
    
    double sum = 0;
    for (double d : durations) sum += d;
    
    stats["count"] = durations.size();
    stats["avgDurationMs"] = sum / durations.size();
    stats["minDurationMs"] = durations.first();
    stats["maxDurationMs"] = durations.last();
    stats["p50DurationMs"] = durations[durations.size() / 2];
    stats["p95DurationMs"] = durations[qMin((int)(durations.size() * 0.95), durations.size() - 1)];
    stats["p99DurationMs"] = durations[qMin((int)(durations.size() * 0.99), durations.size() - 1)];
    stats["errorCount"] = errorCount;
    stats["errorRate"] = (double)errorCount / durations.size();
    
    return stats;
}

QVector<Span> DistributedTracingSystem::getCriticalPath(const QString& traceId) {
    QMutexLocker lock(&m_mutex);
    
    QVector<Span> criticalPath;
    
    if (!m_completedTraces.contains(traceId)) {
        return criticalPath;
    }
    
    const Trace& trace = m_completedTraces[traceId];
    
    // Simple critical path: longest path through the trace
    // Start from root spans and follow longest child
    QVector<Span> rootSpans = trace.getRootSpans();
    
    for (const Span& root : rootSpans) {
        QVector<Span> path;
        path.append(root);
        
        QString currentSpanId = root.spanId;
        while (true) {
            QVector<Span> children = trace.getChildSpans(currentSpanId);
            if (children.isEmpty()) break;
            
            // Find longest child
            Span longest = children.first();
            for (const Span& child : children) {
                if (child.durationMicros > longest.durationMicros) {
                    longest = child;
                }
            }
            
            path.append(longest);
            currentSpanId = longest.spanId;
        }
        
        if (path.size() > criticalPath.size()) {
            criticalPath = path;
        }
    }
    
    return criticalPath;
}

double DistributedTracingSystem::calculateCriticalPathDuration(const QString& traceId) {
    QVector<Span> path = getCriticalPath(traceId);
    
    double totalDuration = 0;
    for (const Span& span : path) {
        totalDuration += span.durationMicros / 1000.0;
    }
    
    return totalDuration;
}

void DistributedTracingSystem::setSamplingRate(double rate) {
    QMutexLocker lock(&m_mutex);
    m_samplingRate = qBound(0.0, rate, 1.0);
}

bool DistributedTracingSystem::shouldSample() const {
    if (m_samplingRate >= 1.0) return true;
    if (m_samplingRate <= 0.0) return false;
    return QRandomGenerator::global()->bounded(1.0) < m_samplingRate;
}

bool DistributedTracingSystem::exportToJaeger(const QString& outputPath) {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject jaegerFormat;
    QJsonArray data;
    
    for (const Trace& trace : m_completedTraces.values()) {
        QJsonObject traceObj;
        traceObj["traceID"] = trace.traceId;
        traceObj["spans"] = trace.toJson();
        data.append(traceObj);
    }
    
    jaegerFormat["data"] = data;
    
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(QJsonDocument(jaegerFormat).toJson());
    file.close();
    
    qInfo() << "[DistributedTracingSystem] Exported" << m_completedTraces.size() 
            << "traces to Jaeger format:" << outputPath;
    
    return true;
}

bool DistributedTracingSystem::exportToZipkin(const QString& outputPath) {
    QMutexLocker lock(&m_mutex);
    
    QJsonArray zipkinFormat;
    
    for (const Trace& trace : m_completedTraces.values()) {
        for (const Span& span : trace.spans) {
            QJsonObject zipkinSpan;
            zipkinSpan["traceId"] = span.traceId;
            zipkinSpan["id"] = span.spanId;
            zipkinSpan["parentId"] = span.parentSpanId;
            zipkinSpan["name"] = span.operationName;
            zipkinSpan["timestamp"] = span.startTime.toMSecsSinceEpoch() * 1000;
            zipkinSpan["duration"] = span.durationMicros;
            
            QJsonObject localEndpoint;
            localEndpoint["serviceName"] = span.serviceName;
            zipkinSpan["localEndpoint"] = localEndpoint;
            
            zipkinSpan["tags"] = span.tags;
            
            zipkinFormat.append(zipkinSpan);
        }
    }
    
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(QJsonDocument(zipkinFormat).toJson());
    file.close();
    
    qInfo() << "[DistributedTracingSystem] Exported traces to Zipkin format:" << outputPath;
    
    return true;
}

bool DistributedTracingSystem::exportToJSON(const QString& outputPath) {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject output;
    QJsonArray traces;
    
    for (const Trace& trace : m_completedTraces.values()) {
        QJsonObject traceObj;
        traceObj["traceId"] = trace.traceId;
        traceObj["spans"] = trace.toJson();
        traceObj["startTime"] = trace.startTime.toString(Qt::ISODate);
        traceObj["endTime"] = trace.endTime.toString(Qt::ISODate);
        traceObj["totalDurationMicros"] = trace.totalDurationMicros;
        traces.append(traceObj);
    }
    
    output["traces"] = traces;
    output["serviceName"] = m_serviceName;
    output["serviceVersion"] = m_serviceVersion;
    output["exportTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QFile file(outputPath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    
    file.write(QJsonDocument(output).toJson());
    file.close();
    
    return true;
}

void DistributedTracingSystem::setMaxTraceSize(int maxSpans) {
    QMutexLocker lock(&m_mutex);
    m_maxTraceSize = maxSpans;
}

void DistributedTracingSystem::setTraceRetentionDays(int days) {
    QMutexLocker lock(&m_mutex);
    m_traceRetentionDays = days;
}

void DistributedTracingSystem::enableAutoFlush(bool enable, int intervalSeconds) {
    QMutexLocker lock(&m_mutex);
    
    m_autoFlushEnabled = enable;
    
    if (enable) {
        if (!m_flushTimer) {
            m_flushTimer = new QTimer(this);
            connect(m_flushTimer, &QTimer::timeout, this, [this]() {
                QMutexLocker lock(&m_mutex);
                flushTraces();
            });
        }
        m_flushTimer->start(intervalSeconds * 1000);
    } else if (m_flushTimer) {
        m_flushTimer->stop();
    }
}

QJsonObject DistributedTracingSystem::getTracingStatistics() const {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject stats;
    stats["serviceName"] = m_serviceName;
    stats["serviceVersion"] = m_serviceVersion;
    stats["totalSpansRecorded"] = static_cast<qint64>(m_totalSpansRecorded);
    stats["totalTracesRecorded"] = static_cast<qint64>(m_totalTracesRecorded);
    stats["spansDropped"] = static_cast<qint64>(m_spansDropped);
    stats["samplingRate"] = m_samplingRate;
    stats["pendingSpans"] = m_pendingSpans.size();
    stats["completedTraces"] = m_completedTraces.size();
    stats["uptimeSeconds"] = m_startTime.secsTo(QDateTime::currentDateTime());
    
    return stats;
}

qint64 DistributedTracingSystem::getTotalSpansRecorded() const {
    QMutexLocker lock(&m_mutex);
    return m_totalSpansRecorded;
}

qint64 DistributedTracingSystem::getTotalTracesRecorded() const {
    QMutexLocker lock(&m_mutex);
    return m_totalTracesRecorded;
}

void DistributedTracingSystem::buildTraceTree(Trace& trace) {
    if (!m_spansByTrace.contains(trace.traceId)) {
        return;
    }
    
    trace.spans = m_spansByTrace[trace.traceId];
    
    if (trace.spans.isEmpty()) {
        return;
    }
    
    // Calculate trace metadata
    trace.startTime = trace.spans.first().startTime;
    trace.endTime = trace.spans.first().endTime;
    
    for (const Span& span : trace.spans) {
        if (span.startTime < trace.startTime) {
            trace.startTime = span.startTime;
        }
        if (span.endTime > trace.endTime) {
            trace.endTime = span.endTime;
        }
        
        // Find root span
        if (span.parentSpanId.isEmpty()) {
            trace.rootServiceName = span.serviceName;
            trace.rootOperationName = span.operationName;
        }
    }
    
    trace.totalDurationMicros = trace.startTime.msecsTo(trace.endTime) * 1000;
    
    detectBottlenecks(trace);
}

void DistributedTracingSystem::detectBottlenecks(const Trace& trace) {
    double threshold = trace.totalDurationMicros * 0.5; // 50% of total
    
    for (const Span& span : trace.spans) {
        if (span.durationMicros >= threshold) {
            emit bottleneckDetected(trace.traceId, span.serviceName, span.durationMicros / 1000.0);
        }
    }
}

void DistributedTracingSystem::pruneOldTraces() {
    QDateTime cutoff = QDateTime::currentDateTime().addDays(-m_traceRetentionDays);
    
    QStringList toRemove;
    for (auto it = m_completedTraces.begin(); it != m_completedTraces.end(); ++it) {
        if (it.value().endTime < cutoff) {
            toRemove.append(it.key());
        }
    }
    
    for (const QString& traceId : toRemove) {
        m_completedTraces.remove(traceId);
        m_spansByTrace.remove(traceId);
    }
}

void DistributedTracingSystem::flushTraces() {
    // Move pending spans to completed traces
    for (const Span& span : m_pendingSpans) {
        m_spansByTrace[span.traceId].append(span);
    }
    m_pendingSpans.clear();
    
    // Prune old traces
    pruneOldTraces();
}

QString DistributedTracingSystem::generateTraceId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).replace("-", "");
}

QString DistributedTracingSystem::generateSpanId() const {
    return QUuid::createUuid().toString(QUuid::WithoutBraces).left(16);
}

// ============================================================================
// ScopedSpan Implementation
// ============================================================================

ScopedSpan::ScopedSpan(DistributedTracingSystem* tracer, const QString& operationName, 
                       const TraceContext* parentContext)
    : m_tracer(tracer)
{
    m_timer.start();
    m_span = m_tracer->startSpan(operationName, parentContext);
}

ScopedSpan::~ScopedSpan() {
    if (m_tracer) {
        m_tracer->finishSpan(m_span);
    }
}

void ScopedSpan::addTag(const QString& key, const QVariant& value) {
    m_span.tags[key] = QJsonValue::fromVariant(value);
}

void ScopedSpan::addEvent(const QString& name, const QJsonObject& attributes) {
    QJsonObject event;
    event["name"] = name;
    event["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    event["attributes"] = attributes;
    m_span.events.append(event);
}

void ScopedSpan::setError(const QString& errorMessage) {
    m_span.isError = true;
    m_span.errorMessage = errorMessage;
}

// ============================================================================
// TraceVisualizer Implementation
// ============================================================================

QString TraceVisualizer::generateFlamegraph(const Trace& trace) {
    QString output;
    QTextStream stream(&output);
    
    stream << "# Flamegraph for trace: " << trace.traceId << "\n";
    stream << "# Format: stack;duration_ms\n";
    
    for (const Span& span : trace.spans) {
        stream << span.serviceName << ";" << span.operationName << " " 
               << (span.durationMicros / 1000) << "\n";
    }
    
    return output;
}

QString TraceVisualizer::generateWaterfallChart(const Trace& trace) {
    QString output;
    QTextStream stream(&output);
    
    stream << "# Waterfall chart for trace: " << trace.traceId << "\n\n";
    
    qint64 baseTime = trace.startTime.toMSecsSinceEpoch();
    
    for (const Span& span : trace.spans) {
        qint64 offset = span.startTime.toMSecsSinceEpoch() - baseTime;
        qint64 duration = span.durationMicros / 1000;
        
        QString indent = span.parentSpanId.isEmpty() ? "" : "  ";
        stream << indent << span.operationName << " @ " << offset << "ms (" 
               << duration << "ms)\n";
    }
    
    return output;
}

QString TraceVisualizer::generateServiceMap(const QVector<Trace>& traces) {
    QString output;
    QTextStream stream(&output);
    
    stream << "# Service dependency map\n";
    
    QMap<QString, QSet<QString>> dependencies;
    
    for (const Trace& trace : traces) {
        for (const Span& span : trace.spans) {
            if (!span.parentSpanId.isEmpty()) {
                for (const Span& parent : trace.spans) {
                    if (parent.spanId == span.parentSpanId) {
                        dependencies[parent.serviceName].insert(span.serviceName);
                        break;
                    }
                }
            }
        }
    }
    
    for (auto it = dependencies.begin(); it != dependencies.end(); ++it) {
        stream << it.key() << " -> ";
        QStringList deps = it.value().values();
        stream << deps.join(", ") << "\n";
    }
    
    return output;
}

QJsonObject TraceVisualizer::generateD3Hierarchy(const Trace& trace) {
    QJsonObject root;
    root["name"] = trace.rootOperationName;
    root["service"] = trace.rootServiceName;
    
    // Build children array recursively
    QJsonArray children;
    
    QVector<Span> rootSpans = trace.getRootSpans();
    for (const Span& rootSpan : rootSpans) {
        QJsonObject node;
        node["name"] = rootSpan.operationName;
        node["service"] = rootSpan.serviceName;
        node["duration"] = rootSpan.durationMicros / 1000.0;
        
        // Add children recursively
        QVector<Span> childSpans = trace.getChildSpans(rootSpan.spanId);
        if (!childSpans.isEmpty()) {
            QJsonArray childNodes;
            for (const Span& child : childSpans) {
                QJsonObject childNode;
                childNode["name"] = child.operationName;
                childNode["service"] = child.serviceName;
                childNode["duration"] = child.durationMicros / 1000.0;
                childNodes.append(childNode);
            }
            node["children"] = childNodes;
        }
        
        children.append(node);
    }
    
    root["children"] = children;
    
    return root;
}

} // namespace Tracing
