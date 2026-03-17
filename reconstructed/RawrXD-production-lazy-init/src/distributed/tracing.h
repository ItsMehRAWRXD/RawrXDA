/**
 * @file distributed_tracing.h
 * @brief Production-Grade Distributed Tracing System with OpenTelemetry-compatible spans
 * 
 * Provides comprehensive execution trace visualization with:
 * - Span-based trace hierarchy (parent/child relationships)
 * - OpenTelemetry-compatible trace/span IDs
 * - Real-time trace visualization with flame graphs
 * - Trace context propagation for cross-component tracking
 * - Performance bottleneck detection
 * - Trace export (JSON, OTLP, Jaeger formats)
 * 
 * @author RawrXD Team
 * @date 2026
 * @version 1.0
 */

#pragma once

#include <QObject>
#include <QWidget>
#include <QString>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include <QMutex>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QColor>
#include <QUuid>
#include <memory>
#include <functional>
#include <atomic>

// Forward declarations
class QTreeWidget;
class QTreeWidgetItem;
class QGraphicsScene;
class QGraphicsView;
class QTabWidget;
class QTextEdit;
class QComboBox;
class QPushButton;
class QLabel;
class QProgressBar;
class QSplitter;
class QTableWidget;
class QLineEdit;
class QCheckBox;

namespace DistributedTracing {

// ============================================================================
// SPAN AND TRACE STRUCTURES
// ============================================================================

/**
 * @enum SpanKind
 * @brief Type of span in the trace
 */
enum class SpanKind {
    Internal,       // Internal operation
    Client,         // Client-side request
    Server,         // Server-side handling
    Producer,       // Message producer
    Consumer        // Message consumer
};

/**
 * @enum SpanStatus
 * @brief Current status of a span
 */
enum class SpanStatus {
    Unset,
    Ok,
    Error
};

/**
 * @struct SpanAttribute
 * @brief Key-value attribute attached to a span
 */
struct SpanAttribute {
    QString key;
    QVariant value;
    QString type; // "string", "int", "double", "bool", "array"
};

/**
 * @struct SpanEvent
 * @brief Event that occurred during span execution
 */
struct SpanEvent {
    QString name;
    qint64 timestampUs;
    QVector<SpanAttribute> attributes;
};

/**
 * @struct SpanLink
 * @brief Link to another span (cross-trace reference)
 */
struct SpanLink {
    QString traceId;
    QString spanId;
    QVector<SpanAttribute> attributes;
};

/**
 * @struct Span
 * @brief Complete span representation with OpenTelemetry compatibility
 */
struct Span {
    // Identity
    QString spanId;         // 16 hex chars (64-bit)
    QString traceId;        // 32 hex chars (128-bit)
    QString parentSpanId;   // Empty for root spans
    
    // Naming
    QString name;
    QString serviceName;
    QString operationType;  // "ai.inference", "file.read", "db.query", etc.
    
    // Timing (microseconds since epoch)
    qint64 startTimeUs;
    qint64 endTimeUs;
    qint64 durationUs;
    
    // Metadata
    SpanKind kind;
    SpanStatus status;
    QString statusMessage;
    
    // Context
    QVector<SpanAttribute> attributes;
    QVector<SpanEvent> events;
    QVector<SpanLink> links;
    
    // Hierarchy tracking
    int depth;  // 0 = root
    QStringList childSpanIds;
    
    // Performance metrics
    double cpuUsagePercent;
    qint64 memoryDeltaBytes;
    int errorCount;
    
    // Computed
    bool isActive() const { return endTimeUs == 0; }
    double durationMs() const { return durationUs / 1000.0; }
};

/**
 * @struct Trace
 * @brief Complete trace containing multiple spans
 */
struct Trace {
    QString traceId;
    QString name;
    QDateTime startTime;
    QDateTime endTime;
    qint64 totalDurationUs;
    int spanCount;
    int errorCount;
    QVector<QString> spanIds;  // All spans in this trace
    QString rootSpanId;
    QMap<QString, QString> tags;  // Trace-level tags
    
    bool isComplete() const { return endTime.isValid(); }
};

// ============================================================================
// TRACE CONTEXT
// ============================================================================

/**
 * @class TraceContext
 * @brief Thread-safe trace context propagation
 * 
 * Manages the current trace/span context for automatic parent linking.
 * Uses thread-local storage for cross-thread safety.
 */
class TraceContext {
public:
    static TraceContext& instance();
    
    // Context manipulation
    void setCurrentTraceId(const QString& traceId);
    void setCurrentSpanId(const QString& spanId);
    QString currentTraceId() const;
    QString currentSpanId() const;
    
    // Scope-based context (for RAII patterns)
    struct Scope {
        Scope(const QString& traceId, const QString& spanId);
        ~Scope();
    private:
        QString m_previousTraceId;
        QString m_previousSpanId;
    };
    
    // W3C Trace Context propagation
    QString toW3CTraceparent() const;
    void fromW3CTraceparent(const QString& traceparent);
    
    // B3 propagation (Zipkin compatibility)
    QMap<QString, QString> toB3Headers() const;
    void fromB3Headers(const QMap<QString, QString>& headers);
    
private:
    TraceContext() = default;
    static thread_local QString s_currentTraceId;
    static thread_local QString s_currentSpanId;
};

// ============================================================================
// SPAN BUILDER
// ============================================================================

/**
 * @class SpanBuilder
 * @brief Fluent builder for creating spans
 */
class SpanBuilder {
public:
    SpanBuilder(const QString& name);
    
    SpanBuilder& setServiceName(const QString& name);
    SpanBuilder& setOperationType(const QString& type);
    SpanBuilder& setKind(SpanKind kind);
    SpanBuilder& setParentSpanId(const QString& parentId);
    SpanBuilder& setTraceId(const QString& traceId);
    SpanBuilder& addAttribute(const QString& key, const QVariant& value);
    SpanBuilder& addLink(const QString& traceId, const QString& spanId);
    SpanBuilder& addEvent(const QString& name, const QVector<SpanAttribute>& attrs = {});
    
    Span build();
    
private:
    Span m_span;
    static QString generateSpanId();
    static QString generateTraceId();
};

// ============================================================================
// TRACER - Core tracing engine
// ============================================================================

/**
 * @class Tracer
 * @brief Central tracing manager and span repository
 * 
 * Thread-safe tracer that manages all spans and traces with:
 * - Automatic parent-child linking
 * - Span lifecycle management
 * - Performance metric collection
 * - Export capabilities
 */
class Tracer : public QObject {
    Q_OBJECT
    
public:
    static Tracer& instance();
    
    // Span lifecycle
    QString startSpan(const QString& name, SpanKind kind = SpanKind::Internal);
    QString startSpan(const SpanBuilder& builder);
    void endSpan(const QString& spanId, SpanStatus status = SpanStatus::Ok, const QString& message = "");
    
    // Span manipulation
    void addSpanAttribute(const QString& spanId, const QString& key, const QVariant& value);
    void addSpanEvent(const QString& spanId, const QString& eventName, const QVector<SpanAttribute>& attrs = {});
    void setSpanStatus(const QString& spanId, SpanStatus status, const QString& message = "");
    
    // Queries
    Span getSpan(const QString& spanId) const;
    Trace getTrace(const QString& traceId) const;
    QVector<Span> getSpansForTrace(const QString& traceId) const;
    QVector<Span> getChildSpans(const QString& parentSpanId) const;
    QVector<Trace> getRecentTraces(int limit = 100) const;
    QVector<Span> getActiveSpans() const;
    
    // Statistics
    struct TracerStats {
        int totalTraces;
        int totalSpans;
        int activeSpans;
        int errorSpans;
        double avgSpanDurationMs;
        double maxSpanDurationMs;
        qint64 totalTracingDurationUs;
    };
    TracerStats getStatistics() const;
    
    // Export
    QJsonObject exportTraceAsJson(const QString& traceId) const;
    QJsonArray exportAllTracesAsJson() const;
    QString exportTraceAsOTLP(const QString& traceId) const;
    bool exportToFile(const QString& filePath, const QString& format = "json");
    
    // Configuration
    void setMaxTracesRetained(int max);
    void setMaxSpansPerTrace(int max);
    void setSamplingRate(double rate);  // 0.0 - 1.0
    void enableAutoFlush(bool enable, int intervalMs = 5000);
    
    // Cleanup
    void clearOldTraces(int maxAgeSeconds);
    void clearAllTraces();
    
signals:
    void spanStarted(const QString& spanId, const QString& name);
    void spanEnded(const QString& spanId, qint64 durationUs, SpanStatus status);
    void traceCompleted(const QString& traceId);
    void errorOccurred(const QString& spanId, const QString& error);
    void statisticsUpdated(const TracerStats& stats);
    
private:
    Tracer();
    ~Tracer() = default;
    
    void processSpanEnd(Span& span);
    void updateTraceMetrics(const QString& traceId);
    void pruneOldTraces();
    
    mutable QMutex m_mutex;
    QMap<QString, Span> m_spans;
    QMap<QString, Trace> m_traces;
    int m_maxTracesRetained = 1000;
    int m_maxSpansPerTrace = 1000;
    double m_samplingRate = 1.0;
    std::atomic<int> m_totalSpanCount{0};
    QTimer* m_autoFlushTimer = nullptr;
};

// ============================================================================
// SCOPED SPAN - RAII wrapper for automatic span lifecycle
// ============================================================================

/**
 * @class ScopedSpan
 * @brief RAII wrapper for automatic span start/end
 * 
 * Usage:
 *   void myFunction() {
 *       ScopedSpan span("myFunction");
 *       span.addAttribute("param1", value1);
 *       // ... work ...
 *   } // span automatically ends here
 */
class ScopedSpan {
public:
    explicit ScopedSpan(const QString& name, SpanKind kind = SpanKind::Internal);
    ScopedSpan(const SpanBuilder& builder);
    ~ScopedSpan();
    
    // Non-copyable
    ScopedSpan(const ScopedSpan&) = delete;
    ScopedSpan& operator=(const ScopedSpan&) = delete;
    
    // Movable
    ScopedSpan(ScopedSpan&& other) noexcept;
    ScopedSpan& operator=(ScopedSpan&& other) noexcept;
    
    // Manipulation
    void addAttribute(const QString& key, const QVariant& value);
    void addEvent(const QString& name, const QVector<SpanAttribute>& attrs = {});
    void setStatus(SpanStatus status, const QString& message = "");
    void setError(const QString& errorMessage);
    
    QString spanId() const { return m_spanId; }
    QString traceId() const;
    
private:
    QString m_spanId;
    bool m_ended = false;
};

// ============================================================================
// TRACE VISUALIZATION WIDGET
// ============================================================================

/**
 * @class TraceVisualizationWidget
 * @brief Complete trace visualization UI with multiple view modes
 * 
 * Features:
 * - Timeline view (waterfall diagram)
 * - Flame graph view
 * - Tree hierarchy view
 * - Span detail panel
 * - Performance analytics
 * - Export capabilities
 */
class TraceVisualizationWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit TraceVisualizationWidget(QWidget* parent = nullptr);
    ~TraceVisualizationWidget() override;
    
    void initialize();
    
    // Data loading
    void loadTrace(const QString& traceId);
    void loadMultipleTraces(const QVector<QString>& traceIds);
    void setAutoRefresh(bool enable, int intervalMs = 1000);
    
public slots:
    // Real-time updates
    void onSpanStarted(const QString& spanId, const QString& name);
    void onSpanEnded(const QString& spanId, qint64 durationUs, SpanStatus status);
    void onTraceCompleted(const QString& traceId);
    
    // Actions
    void refreshView();
    void clearView();
    void exportCurrentTrace();
    void filterByService(const QString& serviceName);
    void filterByDuration(qint64 minMs, qint64 maxMs);
    void searchSpans(const QString& query);
    
signals:
    void spanSelected(const QString& spanId);
    void traceSelected(const QString& traceId);
    void exportRequested(const QString& format);
    
private slots:
    void onTabChanged(int index);
    void onSpanClicked(const QString& spanId);
    void onTimelineZoomChanged(double factor);
    void onFilterChanged();
    
private:
    void setupUI();
    void createTimelineView();
    void createFlameGraphView();
    void createTreeView();
    void createDetailPanel();
    void createToolbar();
    void createStatsPanel();
    
    void updateTimelineView();
    void updateFlameGraphView();
    void updateTreeView();
    void updateDetailPanel(const QString& spanId);
    void updateStatsPanel();
    
    // Timeline rendering
    void renderSpanInTimeline(const Span& span, int row);
    QColor getSpanColor(const Span& span) const;
    
    // Flame graph rendering
    void buildFlameGraphNode(const Span& span, int depth, qint64 offsetUs);
    
    // Tree rendering
    QTreeWidgetItem* createTreeItem(const Span& span);
    void populateTreeChildren(QTreeWidgetItem* parent, const QString& spanId);
    
    // UI Components
    QTabWidget* m_tabWidget = nullptr;
    
    // Timeline view
    QGraphicsScene* m_timelineScene = nullptr;
    QGraphicsView* m_timelineView = nullptr;
    double m_timelineZoomFactor = 1.0;
    
    // Flame graph view
    QGraphicsScene* m_flameScene = nullptr;
    QGraphicsView* m_flameView = nullptr;
    
    // Tree view
    QTreeWidget* m_treeWidget = nullptr;
    
    // Detail panel
    QSplitter* m_mainSplitter = nullptr;
    QTextEdit* m_detailText = nullptr;
    QTableWidget* m_attributeTable = nullptr;
    QTableWidget* m_eventTable = nullptr;
    
    // Stats panel
    QLabel* m_traceCountLabel = nullptr;
    QLabel* m_spanCountLabel = nullptr;
    QLabel* m_avgDurationLabel = nullptr;
    QLabel* m_errorRateLabel = nullptr;
    QProgressBar* m_activeSpansBar = nullptr;
    
    // Toolbar
    QComboBox* m_traceSelector = nullptr;
    QLineEdit* m_searchInput = nullptr;
    QComboBox* m_serviceFilter = nullptr;
    QPushButton* m_refreshBtn = nullptr;
    QPushButton* m_exportBtn = nullptr;
    QCheckBox* m_autoRefreshCheck = nullptr;
    
    // State
    QString m_currentTraceId;
    QString m_selectedSpanId;
    QVector<QString> m_loadedTraceIds;
    QTimer* m_refreshTimer = nullptr;
    QMap<QString, QColor> m_serviceColorMap;
};

// ============================================================================
// PERFORMANCE ANALYZER
// ============================================================================

/**
 * @class TracePerformanceAnalyzer
 * @brief Analyzes traces to detect performance issues
 */
class TracePerformanceAnalyzer : public QObject {
    Q_OBJECT
    
public:
    explicit TracePerformanceAnalyzer(QObject* parent = nullptr);
    
    struct PerformanceIssue {
        QString spanId;
        QString traceId;
        QString issueType;  // "slow_span", "deep_nesting", "high_latency", "error_chain"
        QString description;
        QString recommendation;
        double severity;  // 0.0 - 1.0
        qint64 impactMs;
    };
    
    QVector<PerformanceIssue> analyzeTrace(const QString& traceId);
    QVector<PerformanceIssue> findSlowSpans(qint64 thresholdMs = 100);
    QVector<PerformanceIssue> findDeepNesting(int maxDepth = 10);
    QVector<PerformanceIssue> findErrorChains();
    QJsonObject generatePerformanceReport(const QString& traceId);
    
signals:
    void issueDetected(const PerformanceIssue& issue);
    void analysisComplete(const QString& traceId, int issueCount);
    
private:
    void analyzeSpanLatency(const Span& span, QVector<PerformanceIssue>& issues);
    void analyzeNesting(const Span& span, int depth, QVector<PerformanceIssue>& issues);
};

} // namespace DistributedTracing

// ============================================================================
// CONVENIENCE MACROS
// ============================================================================

#define TRACE_SPAN(name) \
    DistributedTracing::ScopedSpan _trace_span_##__LINE__(name)

#define TRACE_SPAN_WITH_ATTRS(name, ...) \
    DistributedTracing::ScopedSpan _trace_span_##__LINE__(name); \
    __VA_ARGS__

#define TRACE_EVENT(name) \
    DistributedTracing::Tracer::instance().addSpanEvent( \
        DistributedTracing::TraceContext::instance().currentSpanId(), name)

#define TRACE_ATTRIBUTE(key, value) \
    DistributedTracing::Tracer::instance().addSpanAttribute( \
        DistributedTracing::TraceContext::instance().currentSpanId(), key, value)

#define TRACE_ERROR(message) \
    DistributedTracing::Tracer::instance().setSpanStatus( \
        DistributedTracing::TraceContext::instance().currentSpanId(), \
        DistributedTracing::SpanStatus::Error, message)

#endif // DISTRIBUTED_TRACING_H
