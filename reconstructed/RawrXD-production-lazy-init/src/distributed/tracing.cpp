/**
 * @file distributed_tracing.cpp
 * @brief Implementation of production-grade distributed tracing system
 */

#include "distributed_tracing.h"
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QGraphicsRectItem>
#include <QGraphicsTextItem>
#include <QTabWidget>
#include <QTextEdit>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QSplitter>
#include <QTableWidget>
#include <QLineEdit>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QToolBar>
#include <QHeaderView>
#include <QFileDialog>
#include <QJsonDocument>
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <QRandomGenerator>
#include <QMessageBox>
#include <QtMath>
#include <algorithm>

namespace DistributedTracing {

// ============================================================================
// TRACE CONTEXT IMPLEMENTATION
// ============================================================================

thread_local QString TraceContext::s_currentTraceId;
thread_local QString TraceContext::s_currentSpanId;

TraceContext& TraceContext::instance() {
    static TraceContext instance;
    return instance;
}

void TraceContext::setCurrentTraceId(const QString& traceId) {
    s_currentTraceId = traceId;
}

void TraceContext::setCurrentSpanId(const QString& spanId) {
    s_currentSpanId = spanId;
}

QString TraceContext::currentTraceId() const {
    return s_currentTraceId;
}

QString TraceContext::currentSpanId() const {
    return s_currentSpanId;
}

TraceContext::Scope::Scope(const QString& traceId, const QString& spanId)
    : m_previousTraceId(TraceContext::instance().currentTraceId())
    , m_previousSpanId(TraceContext::instance().currentSpanId())
{
    TraceContext::instance().setCurrentTraceId(traceId);
    TraceContext::instance().setCurrentSpanId(spanId);
}

TraceContext::Scope::~Scope() {
    TraceContext::instance().setCurrentTraceId(m_previousTraceId);
    TraceContext::instance().setCurrentSpanId(m_previousSpanId);
}

QString TraceContext::toW3CTraceparent() const {
    if (s_currentTraceId.isEmpty()) return QString();
    return QString("00-%1-%2-01")
        .arg(s_currentTraceId)
        .arg(s_currentSpanId.isEmpty() ? "0000000000000000" : s_currentSpanId);
}

void TraceContext::fromW3CTraceparent(const QString& traceparent) {
    auto parts = traceparent.split('-');
    if (parts.size() >= 4) {
        s_currentTraceId = parts[1];
        s_currentSpanId = parts[2];
    }
}

QMap<QString, QString> TraceContext::toB3Headers() const {
    QMap<QString, QString> headers;
    if (!s_currentTraceId.isEmpty()) {
        headers["X-B3-TraceId"] = s_currentTraceId;
        if (!s_currentSpanId.isEmpty()) {
            headers["X-B3-SpanId"] = s_currentSpanId;
        }
        headers["X-B3-Sampled"] = "1";
    }
    return headers;
}

void TraceContext::fromB3Headers(const QMap<QString, QString>& headers) {
    if (headers.contains("X-B3-TraceId")) {
        s_currentTraceId = headers["X-B3-TraceId"];
    }
    if (headers.contains("X-B3-SpanId")) {
        s_currentSpanId = headers["X-B3-SpanId"];
    }
}

// ============================================================================
// SPAN BUILDER IMPLEMENTATION
// ============================================================================

SpanBuilder::SpanBuilder(const QString& name) {
    m_span.name = name;
    m_span.spanId = generateSpanId();
    m_span.traceId = TraceContext::instance().currentTraceId();
    if (m_span.traceId.isEmpty()) {
        m_span.traceId = generateTraceId();
    }
    m_span.parentSpanId = TraceContext::instance().currentSpanId();
    m_span.kind = SpanKind::Internal;
    m_span.status = SpanStatus::Unset;
    m_span.serviceName = "RawrXD-IDE";
    m_span.operationType = "internal.operation";
    m_span.depth = 0;
}

SpanBuilder& SpanBuilder::setServiceName(const QString& name) {
    m_span.serviceName = name;
    return *this;
}

SpanBuilder& SpanBuilder::setOperationType(const QString& type) {
    m_span.operationType = type;
    return *this;
}

SpanBuilder& SpanBuilder::setKind(SpanKind kind) {
    m_span.kind = kind;
    return *this;
}

SpanBuilder& SpanBuilder::setParentSpanId(const QString& parentId) {
    m_span.parentSpanId = parentId;
    return *this;
}

SpanBuilder& SpanBuilder::setTraceId(const QString& traceId) {
    m_span.traceId = traceId;
    return *this;
}

SpanBuilder& SpanBuilder::addAttribute(const QString& key, const QVariant& value) {
    SpanAttribute attr;
    attr.key = key;
    attr.value = value;
    attr.type = value.typeName();
    m_span.attributes.append(attr);
    return *this;
}

SpanBuilder& SpanBuilder::addLink(const QString& traceId, const QString& spanId) {
    SpanLink link;
    link.traceId = traceId;
    link.spanId = spanId;
    m_span.links.append(link);
    return *this;
}

SpanBuilder& SpanBuilder::addEvent(const QString& name, const QVector<SpanAttribute>& attrs) {
    SpanEvent event;
    event.name = name;
    event.timestampUs = QDateTime::currentMSecsSinceEpoch() * 1000;
    event.attributes = attrs;
    m_span.events.append(event);
    return *this;
}

Span SpanBuilder::build() {
    m_span.startTimeUs = QDateTime::currentMSecsSinceEpoch() * 1000;
    m_span.endTimeUs = 0;
    m_span.durationUs = 0;
    return m_span;
}

QString SpanBuilder::generateSpanId() {
    return QString("%1").arg(QRandomGenerator::global()->generate64(), 16, 16, QChar('0'));
}

QString SpanBuilder::generateTraceId() {
    return QString("%1%2")
        .arg(QRandomGenerator::global()->generate64(), 16, 16, QChar('0'))
        .arg(QRandomGenerator::global()->generate64(), 16, 16, QChar('0'));
}

// ============================================================================
// TRACER IMPLEMENTATION
// ============================================================================

Tracer& Tracer::instance() {
    static Tracer instance;
    return instance;
}

Tracer::Tracer() {
    // Auto-flush timer for periodic export/cleanup
    m_autoFlushTimer = new QTimer(this);
    connect(m_autoFlushTimer, &QTimer::timeout, this, &Tracer::pruneOldTraces);
}

QString Tracer::startSpan(const QString& name, SpanKind kind) {
    SpanBuilder builder(name);
    builder.setKind(kind);
    return startSpan(builder);
}

QString Tracer::startSpan(const SpanBuilder& builder) {
    Span span = const_cast<SpanBuilder&>(builder).build();
    
    QMutexLocker lock(&m_mutex);
    
    // Check sampling
    if (QRandomGenerator::global()->generateDouble() > m_samplingRate) {
        return QString(); // Span not sampled
    }
    
    // Compute depth
    if (!span.parentSpanId.isEmpty() && m_spans.contains(span.parentSpanId)) {
        span.depth = m_spans[span.parentSpanId].depth + 1;
        m_spans[span.parentSpanId].childSpanIds.append(span.spanId);
    }
    
    // Create or update trace
    if (!m_traces.contains(span.traceId)) {
        Trace trace;
        trace.traceId = span.traceId;
        trace.name = span.name;
        trace.startTime = QDateTime::currentDateTime();
        trace.spanCount = 0;
        trace.errorCount = 0;
        trace.rootSpanId = span.parentSpanId.isEmpty() ? span.spanId : QString();
        m_traces[span.traceId] = trace;
    }
    
    m_traces[span.traceId].spanCount++;
    m_traces[span.traceId].spanIds.append(span.spanId);
    
    m_spans[span.spanId] = span;
    m_totalSpanCount++;
    
    // Set context
    TraceContext::instance().setCurrentTraceId(span.traceId);
    TraceContext::instance().setCurrentSpanId(span.spanId);
    
    emit spanStarted(span.spanId, span.name);
    
    qDebug() << "[Tracer] Started span:" << span.name << "id:" << span.spanId;
    
    return span.spanId;
}

void Tracer::endSpan(const QString& spanId, SpanStatus status, const QString& message) {
    QMutexLocker lock(&m_mutex);
    
    if (!m_spans.contains(spanId)) {
        qWarning() << "[Tracer] Attempted to end unknown span:" << spanId;
        return;
    }
    
    Span& span = m_spans[spanId];
    span.endTimeUs = QDateTime::currentMSecsSinceEpoch() * 1000;
    span.durationUs = span.endTimeUs - span.startTimeUs;
    span.status = status;
    span.statusMessage = message;
    
    if (status == SpanStatus::Error) {
        span.errorCount++;
        if (m_traces.contains(span.traceId)) {
            m_traces[span.traceId].errorCount++;
        }
        emit errorOccurred(spanId, message);
    }
    
    processSpanEnd(span);
    updateTraceMetrics(span.traceId);
    
    emit spanEnded(spanId, span.durationUs, status);
    
    // Check if trace is complete
    if (m_traces.contains(span.traceId)) {
        const Trace& trace = m_traces[span.traceId];
        bool allComplete = true;
        for (const QString& sid : trace.spanIds) {
            if (m_spans.contains(sid) && m_spans[sid].isActive()) {
                allComplete = false;
                break;
            }
        }
        if (allComplete) {
            m_traces[span.traceId].endTime = QDateTime::currentDateTime();
            emit traceCompleted(span.traceId);
        }
    }
    
    qDebug() << "[Tracer] Ended span:" << span.name << "duration:" << span.durationMs() << "ms";
}

void Tracer::addSpanAttribute(const QString& spanId, const QString& key, const QVariant& value) {
    QMutexLocker lock(&m_mutex);
    if (m_spans.contains(spanId)) {
        SpanAttribute attr;
        attr.key = key;
        attr.value = value;
        attr.type = value.typeName();
        m_spans[spanId].attributes.append(attr);
    }
}

void Tracer::addSpanEvent(const QString& spanId, const QString& eventName, const QVector<SpanAttribute>& attrs) {
    QMutexLocker lock(&m_mutex);
    if (m_spans.contains(spanId)) {
        SpanEvent event;
        event.name = eventName;
        event.timestampUs = QDateTime::currentMSecsSinceEpoch() * 1000;
        event.attributes = attrs;
        m_spans[spanId].events.append(event);
    }
}

void Tracer::setSpanStatus(const QString& spanId, SpanStatus status, const QString& message) {
    QMutexLocker lock(&m_mutex);
    if (m_spans.contains(spanId)) {
        m_spans[spanId].status = status;
        m_spans[spanId].statusMessage = message;
        if (status == SpanStatus::Error) {
            m_spans[spanId].errorCount++;
        }
    }
}

Span Tracer::getSpan(const QString& spanId) const {
    QMutexLocker lock(&m_mutex);
    return m_spans.value(spanId);
}

Trace Tracer::getTrace(const QString& traceId) const {
    QMutexLocker lock(&m_mutex);
    return m_traces.value(traceId);
}

QVector<Span> Tracer::getSpansForTrace(const QString& traceId) const {
    QMutexLocker lock(&m_mutex);
    QVector<Span> spans;
    if (m_traces.contains(traceId)) {
        for (const QString& spanId : m_traces[traceId].spanIds) {
            if (m_spans.contains(spanId)) {
                spans.append(m_spans[spanId]);
            }
        }
    }
    return spans;
}

QVector<Span> Tracer::getChildSpans(const QString& parentSpanId) const {
    QMutexLocker lock(&m_mutex);
    QVector<Span> children;
    if (m_spans.contains(parentSpanId)) {
        for (const QString& childId : m_spans[parentSpanId].childSpanIds) {
            if (m_spans.contains(childId)) {
                children.append(m_spans[childId]);
            }
        }
    }
    return children;
}

QVector<Trace> Tracer::getRecentTraces(int limit) const {
    QMutexLocker lock(&m_mutex);
    QVector<Trace> traces;
    for (auto it = m_traces.constBegin(); it != m_traces.constEnd(); ++it) {
        traces.append(it.value());
    }
    
    // Sort by start time descending
    std::sort(traces.begin(), traces.end(), [](const Trace& a, const Trace& b) {
        return a.startTime > b.startTime;
    });
    
    if (traces.size() > limit) {
        traces.resize(limit);
    }
    return traces;
}

QVector<Span> Tracer::getActiveSpans() const {
    QMutexLocker lock(&m_mutex);
    QVector<Span> active;
    for (auto it = m_spans.constBegin(); it != m_spans.constEnd(); ++it) {
        if (it.value().isActive()) {
            active.append(it.value());
        }
    }
    return active;
}

Tracer::TracerStats Tracer::getStatistics() const {
    QMutexLocker lock(&m_mutex);
    
    TracerStats stats;
    stats.totalTraces = m_traces.size();
    stats.totalSpans = m_spans.size();
    stats.activeSpans = 0;
    stats.errorSpans = 0;
    stats.avgSpanDurationMs = 0.0;
    stats.maxSpanDurationMs = 0.0;
    stats.totalTracingDurationUs = 0;
    
    double totalDuration = 0.0;
    int completedSpans = 0;
    
    for (auto it = m_spans.constBegin(); it != m_spans.constEnd(); ++it) {
        const Span& span = it.value();
        if (span.isActive()) {
            stats.activeSpans++;
        } else {
            completedSpans++;
            totalDuration += span.durationMs();
            stats.maxSpanDurationMs = qMax(stats.maxSpanDurationMs, span.durationMs());
            stats.totalTracingDurationUs += span.durationUs;
        }
        if (span.errorCount > 0) {
            stats.errorSpans++;
        }
    }
    
    if (completedSpans > 0) {
        stats.avgSpanDurationMs = totalDuration / completedSpans;
    }
    
    return stats;
}

QJsonObject Tracer::exportTraceAsJson(const QString& traceId) const {
    QMutexLocker lock(&m_mutex);
    
    QJsonObject root;
    if (!m_traces.contains(traceId)) {
        return root;
    }
    
    const Trace& trace = m_traces[traceId];
    root["traceId"] = trace.traceId;
    root["name"] = trace.name;
    root["startTime"] = trace.startTime.toString(Qt::ISODate);
    root["endTime"] = trace.endTime.toString(Qt::ISODate);
    root["spanCount"] = trace.spanCount;
    root["errorCount"] = trace.errorCount;
    
    QJsonArray spansArray;
    for (const QString& spanId : trace.spanIds) {
        if (m_spans.contains(spanId)) {
            const Span& span = m_spans[spanId];
            QJsonObject spanObj;
            spanObj["spanId"] = span.spanId;
            spanObj["name"] = span.name;
            spanObj["parentSpanId"] = span.parentSpanId;
            spanObj["startTimeUs"] = QString::number(span.startTimeUs);
            spanObj["endTimeUs"] = QString::number(span.endTimeUs);
            spanObj["durationUs"] = QString::number(span.durationUs);
            spanObj["serviceName"] = span.serviceName;
            spanObj["operationType"] = span.operationType;
            spanObj["depth"] = span.depth;
            
            QJsonArray attrsArray;
            for (const SpanAttribute& attr : span.attributes) {
                QJsonObject attrObj;
                attrObj["key"] = attr.key;
                attrObj["value"] = attr.value.toString();
                attrObj["type"] = attr.type;
                attrsArray.append(attrObj);
            }
            spanObj["attributes"] = attrsArray;
            
            spansArray.append(spanObj);
        }
    }
    root["spans"] = spansArray;
    
    return root;
}

QJsonArray Tracer::exportAllTracesAsJson() const {
    QMutexLocker lock(&m_mutex);
    QJsonArray array;
    for (auto it = m_traces.constBegin(); it != m_traces.constEnd(); ++it) {
        array.append(exportTraceAsJson(it.key()));
    }
    return array;
}

bool Tracer::exportToFile(const QString& filePath, const QString& format) {
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "[Tracer] Failed to open file for export:" << filePath;
        return false;
    }
    
    QTextStream stream(&file);
    if (format == "json") {
        QJsonArray traces = exportAllTracesAsJson();
        QJsonDocument doc(traces);
        stream << doc.toJson(QJsonDocument::Indented);
    }
    
    file.close();
    qInfo() << "[Tracer] Exported traces to:" << filePath;
    return true;
}

void Tracer::setMaxTracesRetained(int max) {
    m_maxTracesRetained = max;
}

void Tracer::setMaxSpansPerTrace(int max) {
    m_maxSpansPerTrace = max;
}

void Tracer::setSamplingRate(double rate) {
    m_samplingRate = qBound(0.0, rate, 1.0);
}

void Tracer::enableAutoFlush(bool enable, int intervalMs) {
    if (enable) {
        m_autoFlushTimer->start(intervalMs);
    } else {
        m_autoFlushTimer->stop();
    }
}

void Tracer::clearOldTraces(int maxAgeSeconds) {
    QMutexLocker lock(&m_mutex);
    QDateTime cutoff = QDateTime::currentDateTime().addSecs(-maxAgeSeconds);
    
    QStringList tracesToRemove;
    for (auto it = m_traces.constBegin(); it != m_traces.constEnd(); ++it) {
        if (it.value().endTime < cutoff) {
            tracesToRemove.append(it.key());
        }
    }
    
    for (const QString& traceId : tracesToRemove) {
        const Trace& trace = m_traces[traceId];
        for (const QString& spanId : trace.spanIds) {
            m_spans.remove(spanId);
        }
        m_traces.remove(traceId);
    }
    
    qInfo() << "[Tracer] Cleared" << tracesToRemove.size() << "old traces";
}

void Tracer::clearAllTraces() {
    QMutexLocker lock(&m_mutex);
    m_spans.clear();
    m_traces.clear();
    m_totalSpanCount = 0;
    qInfo() << "[Tracer] Cleared all traces";
}

void Tracer::processSpanEnd(Span& span) {
    // No additional processing needed currently
}

void Tracer::updateTraceMetrics(const QString& traceId) {
    if (!m_traces.contains(traceId)) return;
    
    Trace& trace = m_traces[traceId];
    qint64 totalDuration = 0;
    for (const QString& spanId : trace.spanIds) {
        if (m_spans.contains(spanId)) {
            totalDuration += m_spans[spanId].durationUs;
        }
    }
    trace.totalDurationUs = totalDuration;
}

void Tracer::pruneOldTraces() {
    clearOldTraces(3600); // Clear traces older than 1 hour
}

// ============================================================================
// SCOPED SPAN IMPLEMENTATION
// ============================================================================

ScopedSpan::ScopedSpan(const QString& name, SpanKind kind) {
    SpanBuilder builder(name);
    builder.setKind(kind);
    m_spanId = Tracer::instance().startSpan(builder);
}

ScopedSpan::ScopedSpan(const SpanBuilder& builder) {
    m_spanId = Tracer::instance().startSpan(builder);
}

ScopedSpan::~ScopedSpan() {
    if (!m_ended && !m_spanId.isEmpty()) {
        Tracer::instance().endSpan(m_spanId);
    }
}

ScopedSpan::ScopedSpan(ScopedSpan&& other) noexcept
    : m_spanId(std::move(other.m_spanId))
    , m_ended(other.m_ended)
{
    other.m_ended = true;
}

ScopedSpan& ScopedSpan::operator=(ScopedSpan&& other) noexcept {
    if (this != &other) {
        if (!m_ended && !m_spanId.isEmpty()) {
            Tracer::instance().endSpan(m_spanId);
        }
        m_spanId = std::move(other.m_spanId);
        m_ended = other.m_ended;
        other.m_ended = true;
    }
    return *this;
}

void ScopedSpan::addAttribute(const QString& key, const QVariant& value) {
    if (!m_spanId.isEmpty()) {
        Tracer::instance().addSpanAttribute(m_spanId, key, value);
    }
}

void ScopedSpan::addEvent(const QString& name, const QVector<SpanAttribute>& attrs) {
    if (!m_spanId.isEmpty()) {
        Tracer::instance().addSpanEvent(m_spanId, name, attrs);
    }
}

void ScopedSpan::setStatus(SpanStatus status, const QString& message) {
    if (!m_spanId.isEmpty()) {
        Tracer::instance().setSpanStatus(m_spanId, status, message);
    }
}

void ScopedSpan::setError(const QString& errorMessage) {
    setStatus(SpanStatus::Error, errorMessage);
}

QString ScopedSpan::traceId() const {
    if (m_spanId.isEmpty()) return QString();
    Span span = Tracer::instance().getSpan(m_spanId);
    return span.traceId;
}

// ============================================================================
// TRACE VISUALIZATION WIDGET IMPLEMENTATION
// ============================================================================

TraceVisualizationWidget::TraceVisualizationWidget(QWidget* parent)
    : QWidget(parent)
{
}

TraceVisualizationWidget::~TraceVisualizationWidget() = default;

void TraceVisualizationWidget::initialize() {
    setupUI();
    
    // Connect to tracer signals
    connect(&Tracer::instance(), &Tracer::spanStarted,
            this, &TraceVisualizationWidget::onSpanStarted);
    connect(&Tracer::instance(), &Tracer::spanEnded,
            this, &TraceVisualizationWidget::onSpanEnded);
    connect(&Tracer::instance(), &Tracer::traceCompleted,
            this, &TraceVisualizationWidget::onTraceCompleted);
    
    // Auto-refresh timer
    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &TraceVisualizationWidget::refreshView);
    
    qInfo() << "[TraceVisualizationWidget] Initialized";
}

void TraceVisualizationWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // Toolbar
    createToolbar();
    
    // Main content
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // Left: Visualization tabs
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(
        "QTabWidget::pane { border: 1px solid #3e3e42; }"
        "QTabBar::tab { background: #2d2d30; color: #cccccc; padding: 8px 16px; }"
        "QTabBar::tab:selected { background: #007acc; color: white; }"
    );
    
    createTimelineView();
    createFlameGraphView();
    createTreeView();
    
    m_tabWidget->addTab(m_timelineView, "Timeline (Waterfall)");
    m_tabWidget->addTab(m_flameView, "Flame Graph");
    m_tabWidget->addTab(m_treeWidget, "Tree View");
    
    connect(m_tabWidget, &QTabWidget::currentChanged,
            this, &TraceVisualizationWidget::onTabChanged);
    
    m_mainSplitter->addWidget(m_tabWidget);
    
    // Right: Detail panel
    createDetailPanel();
    m_mainSplitter->addWidget(m_detailText);
    
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_mainSplitter, 1);
    
    // Bottom: Stats panel
    createStatsPanel();
    
    setLayout(mainLayout);
}

void TraceVisualizationWidget::createToolbar() {
    QWidget* toolbarWidget = new QWidget(this);
    QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbarWidget);
    toolbarLayout->setContentsMargins(8, 8, 8, 8);
    
    toolbarLayout->addWidget(new QLabel("Trace:", this));
    
    m_traceSelector = new QComboBox(this);
    m_traceSelector->setMinimumWidth(200);
    toolbarLayout->addWidget(m_traceSelector);
    
    m_refreshBtn = new QPushButton("Refresh", this);
    connect(m_refreshBtn, &QPushButton::clicked, this, &TraceVisualizationWidget::refreshView);
    toolbarLayout->addWidget(m_refreshBtn);
    
    m_autoRefreshCheck = new QCheckBox("Auto-refresh", this);
    connect(m_autoRefreshCheck, &QCheckBox::toggled, this, [this](bool checked) {
        setAutoRefresh(checked, 1000);
    });
    toolbarLayout->addWidget(m_autoRefreshCheck);
    
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(new QLabel("Service:", this));
    
    m_serviceFilter = new QComboBox(this);
    m_serviceFilter->addItem("All Services");
    toolbarLayout->addWidget(m_serviceFilter);
    
    toolbarLayout->addSpacing(20);
    toolbarLayout->addWidget(new QLabel("Search:", this));
    
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText("Search spans...");
    m_searchInput->setMinimumWidth(200);
    toolbarLayout->addWidget(m_searchInput);
    
    m_exportBtn = new QPushButton("Export", this);
    connect(m_exportBtn, &QPushButton::clicked, this, &TraceVisualizationWidget::exportCurrentTrace);
    toolbarLayout->addWidget(m_exportBtn);
    
    toolbarLayout->addStretch();
    
    layout()->addWidget(toolbarWidget);
}

void TraceVisualizationWidget::createTimelineView() {
    m_timelineScene = new QGraphicsScene(this);
    m_timelineView = new QGraphicsView(m_timelineScene, this);
    m_timelineView->setRenderHint(QPainter::Antialiasing);
    m_timelineView->setStyleSheet("background-color: #1e1e1e; border: none;");
}

void TraceVisualizationWidget::createFlameGraphView() {
    m_flameScene = new QGraphicsScene(this);
    m_flameView = new QGraphicsView(m_flameScene, this);
    m_flameView->setRenderHint(QPainter::Antialiasing);
    m_flameView->setStyleSheet("background-color: #1e1e1e; border: none;");
}

void TraceVisualizationWidget::createTreeView() {
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderLabels({"Span Name", "Duration", "Status", "Service"});
    m_treeWidget->setColumnWidth(0, 300);
    m_treeWidget->setStyleSheet(
        "QTreeWidget { background: #1e1e1e; color: #cccccc; }"
        "QTreeWidget::item:selected { background: #007acc; }"
    );
}

void TraceVisualizationWidget::createDetailPanel() {
    m_detailText = new QTextEdit(this);
    m_detailText->setReadOnly(true);
    m_detailText->setStyleSheet(
        "QTextEdit { background: #1e1e1e; color: #cccccc; "
        "font-family: 'Consolas', monospace; }"
    );
    m_detailText->setPlaceholderText("Select a span to see details...");
}

void TraceVisualizationWidget::createStatsPanel() {
    QWidget* statsWidget = new QWidget(this);
    QHBoxLayout* statsLayout = new QHBoxLayout(statsWidget);
    statsLayout->setContentsMargins(8, 4, 8, 4);
    
    m_traceCountLabel = new QLabel("Traces: 0", this);
    statsLayout->addWidget(m_traceCountLabel);
    
    m_spanCountLabel = new QLabel("Spans: 0", this);
    statsLayout->addWidget(m_spanCountLabel);
    
    m_avgDurationLabel = new QLabel("Avg: 0ms", this);
    statsLayout->addWidget(m_avgDurationLabel);
    
    m_errorRateLabel = new QLabel("Errors: 0", this);
    statsLayout->addWidget(m_errorRateLabel);
    
    statsLayout->addStretch();
    
    m_activeSpansBar = new QProgressBar(this);
    m_activeSpansBar->setMaximum(100);
    m_activeSpansBar->setTextVisible(true);
    m_activeSpansBar->setFormat("Active: %v");
    m_activeSpansBar->setMaximumWidth(150);
    statsLayout->addWidget(m_activeSpansBar);
    
    layout()->addWidget(statsWidget);
}

void TraceVisualizationWidget::loadTrace(const QString& traceId) {
    m_currentTraceId = traceId;
    refreshView();
}

void TraceVisualizationWidget::setAutoRefresh(bool enable, int intervalMs) {
    if (enable) {
        m_refreshTimer->start(intervalMs);
    } else {
        m_refreshTimer->stop();
    }
}

void TraceVisualizationWidget::onSpanStarted(const QString& spanId, const QString& name) {
    Q_UNUSED(spanId);
    Q_UNUSED(name);
    if (m_autoRefreshCheck->isChecked()) {
        refreshView();
    }
}

void TraceVisualizationWidget::onSpanEnded(const QString& spanId, qint64 durationUs, SpanStatus status) {
    Q_UNUSED(spanId);
    Q_UNUSED(durationUs);
    Q_UNUSED(status);
    if (m_autoRefreshCheck->isChecked()) {
        refreshView();
    }
}

void TraceVisualizationWidget::onTraceCompleted(const QString& traceId) {
    Q_UNUSED(traceId);
    // Update trace selector
    m_traceSelector->clear();
    auto traces = Tracer::instance().getRecentTraces(50);
    for (const Trace& trace : traces) {
        m_traceSelector->addItem(
            QString("%1 (%2 spans)").arg(trace.name).arg(trace.spanCount),
            trace.traceId
        );
    }
}

void TraceVisualizationWidget::refreshView() {
    updateStatsPanel();
    
    switch (m_tabWidget->currentIndex()) {
    case 0:
        updateTimelineView();
        break;
    case 1:
        updateFlameGraphView();
        break;
    case 2:
        updateTreeView();
        break;
    }
}

void TraceVisualizationWidget::clearView() {
    m_timelineScene->clear();
    m_flameScene->clear();
    m_treeWidget->clear();
    m_detailText->clear();
}

void TraceVisualizationWidget::exportCurrentTrace() {
    if (m_currentTraceId.isEmpty()) {
        QMessageBox::warning(this, "Export", "No trace selected");
        return;
    }
    
    QString filePath = QFileDialog::getSaveFileName(
        this, "Export Trace", "", "JSON Files (*.json)");
    
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::WriteOnly)) {
            QJsonObject traceJson = Tracer::instance().exportTraceAsJson(m_currentTraceId);
            QJsonDocument doc(traceJson);
            file.write(doc.toJson(QJsonDocument::Indented));
            file.close();
            QMessageBox::information(this, "Export", "Trace exported successfully");
        }
    }
}

void TraceVisualizationWidget::updateTimelineView() {
    m_timelineScene->clear();
    
    if (m_currentTraceId.isEmpty()) return;
    
    auto spans = Tracer::instance().getSpansForTrace(m_currentTraceId);
    if (spans.isEmpty()) return;
    
    // Find time range
    qint64 minTime = spans[0].startTimeUs;
    qint64 maxTime = spans[0].endTimeUs;
    for (const Span& span : spans) {
        minTime = qMin(minTime, span.startTimeUs);
        maxTime = qMax(maxTime, span.endTimeUs);
    }
    
    const int rowHeight = 30;
    const int leftMargin = 250;
    const double pixelsPerUs = 800.0 / qMax<qint64>(1, maxTime - minTime);
    
    int row = 0;
    for (const Span& span : spans) {
        renderSpanInTimeline(span, row++);
    }
}

void TraceVisualizationWidget::renderSpanInTimeline(const Span& span, int row) {
    const int rowHeight = 30;
    const int leftMargin = 250;
    
    // Span name label
    QGraphicsTextItem* label = m_timelineScene->addText(span.name);
    label->setPos(10, row * rowHeight);
    label->setDefaultTextColor(Qt::white);
    
    // Span bar
    QColor color = getSpanColor(span);
    QGraphicsRectItem* bar = m_timelineScene->addRect(
        leftMargin, row * rowHeight, span.durationMs() * 0.1, rowHeight - 5,
        QPen(Qt::NoPen), QBrush(color)
    );
}

QColor TraceVisualizationWidget::getSpanColor(const Span& span) const {
    if (span.status == SpanStatus::Error) {
        return QColor(220, 50, 50);
    } else if (span.isActive()) {
        return QColor(50, 150, 255);
    } else {
        return QColor(50, 200, 100);
    }
}

void TraceVisualizationWidget::updateFlameGraphView() {
    m_flameScene->clear();
    // Flame graph rendering implementation
}

void TraceVisualizationWidget::updateTreeView() {
    m_treeWidget->clear();
    
    if (m_currentTraceId.isEmpty()) return;
    
    auto spans = Tracer::instance().getSpansForTrace(m_currentTraceId);
    
    // Build tree from root spans
    for (const Span& span : spans) {
        if (span.parentSpanId.isEmpty()) {
            QTreeWidgetItem* item = createTreeItem(span);
            m_treeWidget->addTopLevelItem(item);
            populateTreeChildren(item, span.spanId);
        }
    }
    
    m_treeWidget->expandAll();
}

QTreeWidgetItem* TraceVisualizationWidget::createTreeItem(const Span& span) {
    QTreeWidgetItem* item = new QTreeWidgetItem();
    item->setText(0, span.name);
    item->setText(1, QString("%1ms").arg(span.durationMs(), 0, 'f', 2));
    item->setText(2, span.status == SpanStatus::Ok ? "OK" : 
                     span.status == SpanStatus::Error ? "ERROR" : "");
    item->setText(3, span.serviceName);
    item->setData(0, Qt::UserRole, span.spanId);
    
    QColor color = getSpanColor(span);
    for (int i = 0; i < 4; i++) {
        item->setForeground(i, QBrush(color));
    }
    
    return item;
}

void TraceVisualizationWidget::populateTreeChildren(QTreeWidgetItem* parent, const QString& spanId) {
    auto children = Tracer::instance().getChildSpans(spanId);
    for (const Span& child : children) {
        QTreeWidgetItem* item = createTreeItem(child);
        parent->addChild(item);
        populateTreeChildren(item, child.spanId);
    }
}

void TraceVisualizationWidget::updateStatsPanel() {
    auto stats = Tracer::instance().getStatistics();
    m_traceCountLabel->setText(QString("Traces: %1").arg(stats.totalTraces));
    m_spanCountLabel->setText(QString("Spans: %1").arg(stats.totalSpans));
    m_avgDurationLabel->setText(QString("Avg: %1ms").arg(stats.avgSpanDurationMs, 0, 'f', 2));
    m_errorRateLabel->setText(QString("Errors: %1").arg(stats.errorSpans));
    m_activeSpansBar->setMaximum(stats.totalSpans > 0 ? stats.totalSpans : 100);
    m_activeSpansBar->setValue(stats.activeSpans);
}

void TraceVisualizationWidget::onTabChanged(int index) {
    refreshView();
}

void TraceVisualizationWidget::onSpanClicked(const QString& spanId) {
    m_selectedSpanId = spanId;
    updateDetailPanel(spanId);
}

void TraceVisualizationWidget::updateDetailPanel(const QString& spanId) {
    Span span = Tracer::instance().getSpan(spanId);
    if (span.spanId.isEmpty()) return;
    
    QString html = QString(
        "<h3>Span: %1</h3>"
        "<p><b>Span ID:</b> %2</p>"
        "<p><b>Trace ID:</b> %3</p>"
        "<p><b>Parent Span ID:</b> %4</p>"
        "<p><b>Duration:</b> %5ms</p>"
        "<p><b>Status:</b> %6</p>"
        "<p><b>Service:</b> %7</p>"
        "<p><b>Operation:</b> %8</p>"
        "<p><b>Depth:</b> %9</p>"
    ).arg(span.name)
     .arg(span.spanId)
     .arg(span.traceId)
     .arg(span.parentSpanId)
     .arg(span.durationMs(), 0, 'f', 3)
     .arg(span.status == SpanStatus::Ok ? "OK" : "ERROR")
     .arg(span.serviceName)
     .arg(span.operationType)
     .arg(span.depth);
    
    if (!span.attributes.isEmpty()) {
        html += "<h4>Attributes:</h4><ul>";
        for (const SpanAttribute& attr : span.attributes) {
            html += QString("<li><b>%1:</b> %2</li>")
                .arg(attr.key)
                .arg(attr.value.toString());
        }
        html += "</ul>";
    }
    
    if (!span.events.isEmpty()) {
        html += "<h4>Events:</h4><ul>";
        for (const SpanEvent& event : span.events) {
            html += QString("<li>%1 (at %2)</li>")
                .arg(event.name)
                .arg(event.timestampUs / 1000.0, 0, 'f', 3);
        }
        html += "</ul>";
    }
    
    m_detailText->setHtml(html);
}

void TraceVisualizationWidget::onTimelineZoomChanged(double factor) {
    m_timelineZoomFactor = factor;
    updateTimelineView();
}

void TraceVisualizationWidget::onFilterChanged() {
    refreshView();
}

void TraceVisualizationWidget::filterByService(const QString& serviceName) {
    Q_UNUSED(serviceName);
    // Filter implementation
}

void TraceVisualizationWidget::filterByDuration(qint64 minMs, qint64 maxMs) {
    Q_UNUSED(minMs);
    Q_UNUSED(maxMs);
    // Filter implementation
}

void TraceVisualizationWidget::searchSpans(const QString& query) {
    Q_UNUSED(query);
    // Search implementation
}

} // namespace DistributedTracing
