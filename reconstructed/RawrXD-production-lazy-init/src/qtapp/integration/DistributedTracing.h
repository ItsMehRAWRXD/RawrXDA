#pragma once
#include "ProdIntegration.h"
#include <QObject>
#include <QString>
#include <QDateTime>
#include <QUuid>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QStack>
#include <QMap>

namespace RawrXD {
namespace Integration {
namespace Tracing {

// Distributed trace context
struct TraceContext {
    QString traceId;
    QString spanId;
    QString parentSpanId;
    QString serviceName;
    QString operationName;
    QDateTime startTime;
    QDateTime endTime;
    qint64 durationMs;
    QJsonObject tags;
    QJsonArray logs;
    bool isError;
    QString errorMessage;
    
    QJsonObject toJson() const {
        QJsonObject obj;
        obj.insert(QStringLiteral("trace_id"), traceId);
        obj.insert(QStringLiteral("span_id"), spanId);
        obj.insert(QStringLiteral("parent_span_id"), parentSpanId);
        obj.insert(QStringLiteral("service_name"), serviceName);
        obj.insert(QStringLiteral("operation_name"), operationName);
        obj.insert(QStringLiteral("start_time"), startTime.toString(Qt::ISODate));
        obj.insert(QStringLiteral("end_time"), endTime.toString(Qt::ISODate));
        obj.insert(QStringLiteral("duration_ms"), durationMs);
        obj.insert(QStringLiteral("tags"), tags);
        obj.insert(QStringLiteral("logs"), logs);
        obj.insert(QStringLiteral("is_error"), isError);
        obj.insert(QStringLiteral("error_message"), errorMessage);
        return obj;
    }
};

// Distributed tracing manager
class DistributedTracer {
public:
    static DistributedTracer& instance() {
        static DistributedTracer tracer;
        return tracer;
    }

    // Start a new trace (root span)
    QString startTrace(const QString& serviceName, const QString& operationName) {
        if (!Config::tracingEnabled()) return QString();
        
        QMutexLocker lock(&m_mutex);
        
        const QString traceId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        const QString spanId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        
        TraceContext context;
        context.traceId = traceId;
        context.spanId = spanId;
        context.serviceName = serviceName;
        context.operationName = operationName;
        context.startTime = QDateTime::currentDateTime();
        context.isError = false;
        
        m_activeSpans[spanId] = context;
        m_traceRoots[traceId] = spanId;
        
        if (Config::loggingEnabled()) {
            logDebug(QStringLiteral("DistributedTracer"), QStringLiteral("start_trace"),
                    QStringLiteral("Trace started: %1 (span: %2, op: %3)")
                    .arg(traceId, spanId, operationName));
        }
        
        traceEvent(serviceName.toUtf8().constData(), operationName.toUtf8().constData());
        
        return spanId;
    }

    // Start a child span
    QString startSpan(const QString& parentSpanId, const QString& serviceName, const QString& operationName) {
        if (!Config::tracingEnabled()) return QString();
        
        QMutexLocker lock(&m_mutex);
        
        auto parentIt = m_activeSpans.find(parentSpanId);
        if (parentIt == m_activeSpans.end()) return QString();
        
        const QString spanId = QUuid::createUuid().toString(QUuid::WithoutBraces);
        
        TraceContext context;
        context.traceId = parentIt->traceId;
        context.spanId = spanId;
        context.parentSpanId = parentSpanId;
        context.serviceName = serviceName;
        context.operationName = operationName;
        context.startTime = QDateTime::currentDateTime();
        context.isError = false;
        
        m_activeSpans[spanId] = context;
        
        if (Config::loggingEnabled()) {
            logDebug(QStringLiteral("DistributedTracer"), QStringLiteral("start_span"),
                    QStringLiteral("Span started: %1 (parent: %2, op: %3)")
                    .arg(spanId, parentSpanId, operationName));
        }
        
        return spanId;
    }

    // Finish a span
    void finishSpan(const QString& spanId, bool success = true, const QString& errorMessage = QString()) {
        if (!Config::tracingEnabled()) return;
        
        QMutexLocker lock(&m_mutex);
        
        auto it = m_activeSpans.find(spanId);
        if (it != m_activeSpans.end()) {
            it->endTime = QDateTime::currentDateTime();
            it->durationMs = it->startTime.msecsTo(it->endTime);
            it->isError = !success;
            it->errorMessage = errorMessage;
            
            // Move to completed spans
            m_completedSpans.append(*it);
            
            // Keep only last N completed spans
            while (m_completedSpans.size() > 1000) {
                m_completedSpans.removeFirst();
            }
            
            m_activeSpans.erase(it);
            
            if (Config::loggingEnabled()) {
                logInfo(QStringLiteral("DistributedTracer"), QStringLiteral("finish_span"),
                        QStringLiteral("Span finished: %1 (%2) in %3ms")
                        .arg(spanId).arg(success ? "success" : "error").arg(it->durationMs));
            }
            
            if (Config::metricsEnabled()) {
                recordMetric("trace_span_completed", 1);
                recordMetric("trace_span_duration_ms", it->durationMs);
                if (!success) {
                    recordMetric("trace_span_error", 1);
                }
            }
        }
    }

    // Add tags to span
    void addSpanTag(const QString& spanId, const QString& key, const QString& value) {
        if (!Config::tracingEnabled()) return;
        
        QMutexLocker lock(&m_mutex);
        
        auto it = m_activeSpans.find(spanId);
        if (it != m_activeSpans.end()) {
            it->tags.insert(key, value);
        }
    }

    // Add log entry to span
    void addSpanLog(const QString& spanId, const QString& message, const QJsonObject& fields = QJsonObject()) {
        if (!Config::tracingEnabled()) return;
        
        QMutexLocker lock(&m_mutex);
        
        auto it = m_activeSpans.find(spanId);
        if (it != m_activeSpans.end()) {
            QJsonObject logEntry;
            logEntry.insert(QStringLiteral("timestamp"), QDateTime::currentDateTime().toString(Qt::ISODate));
            logEntry.insert(QStringLiteral("message"), message);
            logEntry.insert(QStringLiteral("fields"), fields);
            it->logs.append(logEntry);
        }
    }

    // Get trace by ID
    QJsonArray getTrace(const QString& traceId) const {
        QMutexLocker lock(&m_mutex);
        
        QJsonArray trace;
        for (const auto& span : m_completedSpans) {
            if (span.traceId == traceId) {
                trace.append(span.toJson());
            }
        }
        
        return trace;
    }

    // Get all active spans
    QJsonArray getActiveSpans() const {
        QMutexLocker lock(&m_mutex);
        
        QJsonArray result;
        for (const auto& span : m_activeSpans) {
            result.append(span.toJson());
        }
        
        return result;
    }

    // Get recent completed spans
    QJsonArray getRecentSpans(int maxCount = 100) const {
        QMutexLocker lock(&m_mutex);
        
        QJsonArray result;
        int count = 0;
        
        for (auto it = m_completedSpans.rbegin(); 
             it != m_completedSpans.rend() && count < maxCount; 
             ++it, ++count) {
            result.append(it->toJson());
        }
        
        return result;
    }

private:
    DistributedTracer() = default;
    
    mutable QMutex m_mutex;
    QMap<QString, TraceContext> m_activeSpans;
    QMap<QString, QString> m_traceRoots; // traceId -> root spanId
    QVector<TraceContext> m_completedSpans;
};

// RAII span wrapper
class ScopedSpan {
public:
    ScopedSpan(const QString& serviceName, const QString& operationName)
        : m_isRoot(true) {
        m_spanId = DistributedTracer::instance().startTrace(serviceName, operationName);
    }

    ScopedSpan(const QString& parentSpanId, const QString& serviceName, const QString& operationName)
        : m_isRoot(false) {
        m_spanId = DistributedTracer::instance().startSpan(parentSpanId, serviceName, operationName);
    }

    ~ScopedSpan() {
        if (!m_finished && !m_spanId.isEmpty()) {
            DistributedTracer::instance().finishSpan(m_spanId, true);
        }
    }

    void finish(bool success = true, const QString& errorMessage = QString()) {
        if (!m_spanId.isEmpty()) {
            DistributedTracer::instance().finishSpan(m_spanId, success, errorMessage);
            m_finished = true;
        }
    }

    void addTag(const QString& key, const QString& value) {
        if (!m_spanId.isEmpty()) {
            DistributedTracer::instance().addSpanTag(m_spanId, key, value);
        }
    }

    void addLog(const QString& message, const QJsonObject& fields = QJsonObject()) {
        if (!m_spanId.isEmpty()) {
            DistributedTracer::instance().addSpanLog(m_spanId, message, fields);
        }
    }

    QString spanId() const { return m_spanId; }

private:
    QString m_spanId;
    bool m_isRoot;
    bool m_finished = false;
};

// Trace propagation helper (for cross-service calls)
class TracePropagation {
public:
    static QString injectTraceContext(const QString& spanId) {
        if (!Config::tracingEnabled()) return QString();
        
        // Simple base64-encoded trace context
        // In production, use W3C Trace Context or similar standard
        return QString("trace-%1").arg(spanId);
    }

    static QString extractTraceContext(const QString& traceHeader) {
        if (!Config::tracingEnabled()) return QString();
        
        if (traceHeader.startsWith(QStringLiteral("trace-"))) {
            return traceHeader.mid(6); // Remove "trace-" prefix
        }
        
        return QString();
    }
};

} // namespace Tracing
} // namespace Integration
} // namespace RawrXD
