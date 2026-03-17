#include "PerformanceMonitor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDebug>
#include <QPainter>
#include <QDateTime>

PerformanceMonitor::PerformanceMonitor(QWidget* parent)
    : QWidget(parent) {
    
    setupUI();
    
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &PerformanceMonitor::updateDisplay);
    m_updateTimer->start(500); // Update every 500ms
    
    setWindowTitle("Performance Monitor");
    setMinimumSize(600, 300);
}

PerformanceMonitor::~PerformanceMonitor() {
    m_updateTimer->stop();
}

void PerformanceMonitor::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout;
    
    // Model label
    m_modelLabel = new QLabel("Active Model: None");
    mainLayout->addWidget(m_modelLabel);
    
    // Metrics labels
    m_tpsLabel = new QLabel("Tokens/Sec: 0.0");
    m_tokenCountLabel = new QLabel("Total Tokens: 0");
    m_latencyLabel = new QLabel("Avg Latency: 0.0ms");
    m_memoryLabel = new QLabel("Memory Usage: 0 MB");
    
    QHBoxLayout* metricsLayout = new QHBoxLayout;
    metricsLayout->addWidget(m_tpsLabel);
    metricsLayout->addWidget(m_tokenCountLabel);
    metricsLayout->addWidget(m_latencyLabel);
    metricsLayout->addWidget(m_memoryLabel);
    
    mainLayout->addLayout(metricsLayout);
    mainLayout->addStretch();
    
    setLayout(mainLayout);
}

PerformanceMonitor::MetricsSnapshot PerformanceMonitor::getCurrentMetrics() const {
    return m_metrics;
}

void PerformanceMonitor::recordTokenGenerated(const QString& modelName) {
    m_modelTokenCounts[modelName]++;
    m_metrics.totalTokensGenerated++;
    m_metrics.activeModel = modelName;
}

void PerformanceMonitor::recordInferenceComplete(const QString& modelName, double latencyMs) {
    // Update average latency
    m_metrics.averageLatencyMs = (m_metrics.averageLatencyMs + latencyMs) / 2.0;
    
    // Calculate tokens per second based on latency
    if (latencyMs > 0) {
        m_metrics.tokensPerSecond = 1000.0 / latencyMs;
    }
}

void PerformanceMonitor::updateMemoryUsage(uint64_t usageMB) {
    m_metrics.peakMemoryUsageMB = usageMB;
    m_memoryLabel->setText(QString("Memory Usage: %1 MB").arg(usageMB));
}

void PerformanceMonitor::updateDisplay() {
    m_tpsLabel->setText(QString("Tokens/Sec: %1").arg(m_metrics.tokensPerSecond, 0, 'f', 2));
    m_tokenCountLabel->setText(QString("Total Tokens: %1").arg(m_metrics.totalTokensGenerated));
    m_latencyLabel->setText(QString("Avg Latency: %1ms").arg(m_metrics.averageLatencyMs, 0, 'f', 2));
    m_modelLabel->setText(QString("Active Model: %1").arg(m_metrics.activeModel.isEmpty() ? "None" : m_metrics.activeModel));
    
    emit metricsUpdated(m_metrics);
}

void PerformanceMonitor::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    painter.fillRect(rect(), Qt::white);
    
    // Draw metrics summary box
    QRect summaryBox(10, 80, width() - 20, height() - 100);
    painter.drawRect(summaryBox);
    
    painter.setPen(Qt::black);
    painter.setFont(QFont("Arial", 12));
    painter.drawText(summaryBox, Qt::AlignCenter, m_metrics.activeModel);
}
