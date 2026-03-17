// Distributed Tracing System - Real-time execution monitoring and visualization
#pragma once

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariantMap>
#include <QStringList>
#include <QDateTime>
#include <QTimer>
#include <QElapsedTimer>
#include <QMutex>
#include <QReadWriteLock>
#include <QQueue>
#include <QSet>
#include <QHash>
#include <QUuid>
#include <QThread>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>
#include <atomic>

// Forward declarations
class AgenticExecutor;
class AdvancedPlanningEngine;
class ToolCompositionFramework;

/**
 * @brief Trace span representing a unit of work
 */
struct TraceSpan {
    QString spanId;
    QString traceId;
    QString parentSpanId;
    QString operationName;
    QString serviceName;
    QDateTime startTime;
    QDateTime endTime;
    qint64 durationMicros = 0;
    
    // Context and metadata
    QJsonObject tags;
    QJsonObject logs;
    QVariantMap baggage;
    QString status;              // "ok", "error", "timeout"
    QString statusMessage;
    
    // Performance metrics
    qint64 memoryUsedBytes = 0;
    double cpuUsagePercent = 0.0;
    int threadCount = 0;
    QJsonObject customMetrics;
    
    // Relationships
    QStringList childSpanIds;
    QStringList references;      // "follows", "child_of", "caused_by"
    
    QJsonObject toJson() const;
    void fromJson(const QJsonObject& json);
};

/**
 * @brief Distributed trace containing multiple spans
 */
struct DistributedTrace {
    QString traceId;
    QString rootSpanId;
    QDateTime startTime;
    QDateTime endTime;
    qint64 totalDurationMicros = 0;
    
    // Trace metadata
    QString operation;
    QString service;
    QStringList services;        // All services involved
    QJsonObject traceContext;
    int spanCount = 0;
    int errorCount = 0;
    
    // Performance summary
    QJsonObject performanceSummary;
    QStringList criticalPath;
    QStringList bottlenecks;
    double parallelismRatio = 0.0;
    
    std::unordered_map<QString, TraceSpan> spans;
    
    QJsonObject toJson() const;
    bool isComplete() const;
    void calculateMetrics();
};

/**
 * @brief Memory Persistence - State management and intelligent caching
 */
class MemoryPersistence : public QObject {
    Q_OBJECT

public:
    explicit MemoryPersistence(QObject* parent = nullptr);
    ~MemoryPersistence();

    // Snapshot management
    bool saveSnapshot(const QString& key, const QJsonObject& data);
    QJsonObject loadSnapshot(const QString& key);
    bool removeSnapshot(const QString& key);
    QStringList getSnapshotKeys() const;
    
    // State persistence
    void setState(const QString& component, const QVariantMap& state);
    QVariantMap getState(const QString& component) const;
    void clearState(const QString& component);
    
    // Cache management
    void setCacheValue(const QString& key, const QVariant& value, int ttlSeconds = 3600);
    QVariant getCacheValue(const QString& key) const;
    void removeCacheValue(const QString& key);
    void clearCache();
    QJsonObject getCacheStatistics() const;

signals:
    void snapshotSaved(const QString& key);
    void snapshotRemoved(const QString& key);
    void cacheUpdated(const QString& key);

private:
    mutable QReadWriteLock m_lock;
    QHash<QString, QJsonObject> m_snapshots;
    QHash<QString, QVariantMap> m_componentStates;
    QHash<QString, QPair<QVariant, QDateTime>> m_cache;
};

/**
 * @brief Distributed Tracing System
 */
class DistributedTracer : public QObject {
    Q_OBJECT

public:
    explicit DistributedTracer(QObject* parent = nullptr);
    ~DistributedTracer();

    // Initialization
    void initialize(AgenticExecutor* executor, AdvancedPlanningEngine* planner,
                   ToolCompositionFramework* toolFramework);
    bool isInitialized() const { return m_initialized; }

    // Trace lifecycle
    QString startTrace(const QString& operation, const QString& serviceName = "AI-Toolkit");
    QString startSpan(const QString& traceId, const QString& operationName,
                     const QString& parentSpanId = QString());
    void finishSpan(const QString& spanId, const QString& status = "ok",
                   const QString& statusMessage = QString());
    void finishTrace(const QString& traceId);

    // Span manipulation
    void addSpanTag(const QString& spanId, const QString& key, const QVariant& value);
    void addSpanLog(const QString& spanId, const QString& event, const QJsonObject& data = QJsonObject());
    void setSpanError(const QString& spanId, const QString& error);
    void setBaggage(const QString& spanId, const QString& key, const QVariant& value);

    // Trace retrieval
    DistributedTrace getTrace(const QString& traceId) const;
    TraceSpan getSpan(const QString& spanId) const;
    QStringList getActiveTraces() const;
    QStringList getCompletedTraces() const;
    QStringList getTracesForOperation(const QString& operation) const;

    // Performance analysis
    QJsonObject analyzeTrace(const QString& traceId) const;
    QStringList findBottlenecks(const QString& traceId) const;
    QStringList findCriticalPath(const QString& traceId) const;
    QJsonObject getPerformanceMetrics(const QString& traceId) const;
    double calculateParallelism(const QString& traceId) const;

    // Visualization data
    QJsonObject generateVisualizationData(const QString& traceId) const;
    QString generateFlameGraph(const QString& traceId) const;
    QJsonObject generateTimeline(const QString& traceId) const;
    QJsonObject generateServiceMap() const;

    // Configuration
    void setTracingEnabled(bool enabled) { m_tracingEnabled = enabled; }
    void setSamplingRate(double rate) { m_samplingRate = qBound(0.0, rate, 1.0); }
    void setMaxTraceHistory(int maxTraces) { m_maxTraceHistory = maxTraces; }

signals:
    void traceStarted(const QString& traceId);
    void traceCompleted(const QString& traceId);
    void spanStarted(const QString& spanId, const QString& traceId);
    void spanCompleted(const QString& spanId, const QString& traceId);
    void errorDetected(const QString& spanId, const QString& error);
    void performanceAlert(const QString& traceId, const QString& alert);

private slots:
    void cleanupOldTraces();
    void updateMetrics();

private:
    // Core components
    AgenticExecutor* m_agenticExecutor = nullptr;
    AdvancedPlanningEngine* m_planningEngine = nullptr;
    ToolCompositionFramework* m_toolFramework = nullptr;

    // Trace storage
    std::unordered_map<QString, DistributedTrace> m_traces;
    std::unordered_map<QString, TraceSpan> m_spans;
    QSet<QString> m_activeTraces;
    mutable QReadWriteLock m_tracesLock;

    // Configuration
    bool m_initialized = false;
    bool m_tracingEnabled = true;
    double m_samplingRate = 1.0;
    int m_maxTraceHistory = 1000;

    // Performance tracking
    std::atomic<int> m_totalSpans{0};
    std::atomic<int> m_totalTraces{0};
    QElapsedTimer m_uptimeTimer;

    // Timers
    QTimer* m_cleanupTimer;
    QTimer* m_metricsTimer;

    // Internal methods
    void setupTimers();
    QString generateTraceId();
    QString generateSpanId();
    bool shouldSample() const;
    void updateSpanMetrics(TraceSpan& span);
    void analyzeTraceCompletion(DistributedTrace& trace);
    QStringList calculateCriticalPath(const DistributedTrace& trace) const;
    QStringList identifyBottlenecks(const DistributedTrace& trace) const;
};