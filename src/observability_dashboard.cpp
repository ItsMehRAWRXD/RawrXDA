#include "observability_dashboard.h"
#include "profiler.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QLabel>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QDateTime>
#include <QDebug>
#include <QFile>
#include <QTextStream>
#include <QGroupBox>
#include <QProgressBar>

ObservabilityDashboard::ObservabilityDashboard(Profiler* profiler, QWidget* parent)
    : QWidget(parent)
    , m_tabWidget(new QTabWidget(this))
    , m_profiler(profiler)
{
    setWindowTitle("Observability Dashboard");
    setMinimumSize(1000, 700);

    setupUI();
    setupConnections();
}

ObservabilityDashboard::~ObservabilityDashboard()
{
    // Qt handles widget cleanup
}

void ObservabilityDashboard::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ===== Create Charts =====
    createResourceChart();
    createThroughputChart();
    createLatencyChart();
    createMetricsPanel();

    // ===== Add to Tab Widget =====
    m_resourceChartView = new QChartView(m_resourceChart, this);
    m_resourceChartView->setRenderHint(QPainter::Antialiasing);
    m_tabWidget->addTab(m_resourceChartView, "System Resources");

    m_throughputChartView = new QChartView(m_throughputChart, this);
    m_throughputChartView->setRenderHint(QPainter::Antialiasing);
    m_tabWidget->addTab(m_throughputChartView, "Training Throughput");

    m_latencyChartView = new QChartView(m_latencyChart, this);
    m_latencyChartView->setRenderHint(QPainter::Antialiasing);
    m_tabWidget->addTab(m_latencyChartView, "Latency Analysis");

    m_tabWidget->addTab(m_metricsPanel, "Live Metrics");

    mainLayout->addWidget(m_tabWidget);
    setLayout(mainLayout);
}

void ObservabilityDashboard::createResourceChart()
{
    m_resourceChart = new QChart();
    m_resourceChart->setTitle("System Resource Utilization");
    m_resourceChart->setAnimationOptions(QChart::SeriesAnimations);
    m_resourceChart->setBackgroundBrush(QBrush(Qt::white));

    // Create series
    m_cpuSeries = new QLineSeries();
    m_cpuSeries->setName("CPU %");
    m_cpuSeries->setColor(Qt::blue);

    m_memorySeries = new QLineSeries();
    m_memorySeries->setName("Memory MB");
    m_memorySeries->setColor(Qt::red);

    m_gpuSeries = new QLineSeries();
    m_gpuSeries->setName("GPU %");
    m_gpuSeries->setColor(Qt::green);

    m_resourceChart->addSeries(m_cpuSeries);
    m_resourceChart->addSeries(m_memorySeries);
    m_resourceChart->addSeries(m_gpuSeries);

    // Create axes
    m_resourceAxisX = new QDateTimeAxis();
    m_resourceAxisX->setFormat("hh:mm:ss");
    m_resourceAxisX->setTickCount(5);
    m_resourceChart->addAxis(m_resourceAxisX, Qt::AlignBottom);
    m_cpuSeries->attachAxis(m_resourceAxisX);
    m_memorySeries->attachAxis(m_resourceAxisX);
    m_gpuSeries->attachAxis(m_resourceAxisX);

    m_resourceAxisY = new QValueAxis();
    m_resourceAxisY->setTitleText("Value");
    m_resourceAxisY->setRange(0, 100);
    m_resourceChart->addAxis(m_resourceAxisY, Qt::AlignLeft);
    m_cpuSeries->attachAxis(m_resourceAxisY);
    m_memorySeries->attachAxis(m_resourceAxisY);
    m_gpuSeries->attachAxis(m_resourceAxisY);

    m_resourceChart->legend()->setVisible(true);
    m_resourceChart->legend()->setAlignment(Qt::AlignTop);
}

void ObservabilityDashboard::createThroughputChart()
{
    m_throughputChart = new QChart();
    m_throughputChart->setTitle("Training Throughput");
    m_throughputChart->setAnimationOptions(QChart::SeriesAnimations);
    m_throughputChart->setBackgroundBrush(QBrush(Qt::white));

    // Create series
    m_samplesPerSecSeries = new QLineSeries();
    m_samplesPerSecSeries->setName("Samples/sec");
    m_samplesPerSecSeries->setColor(Qt::darkMagenta);

    m_tokensPerSecSeries = new QLineSeries();
    m_tokensPerSecSeries->setName("Tokens/sec");
    m_tokensPerSecSeries->setColor(Qt::darkCyan);

    m_throughputChart->addSeries(m_samplesPerSecSeries);
    m_throughputChart->addSeries(m_tokensPerSecSeries);

    // Create axes
    m_throughputAxisX = new QDateTimeAxis();
    m_throughputAxisX->setFormat("hh:mm:ss");
    m_throughputAxisX->setTickCount(5);
    m_throughputChart->addAxis(m_throughputAxisX, Qt::AlignBottom);
    m_samplesPerSecSeries->attachAxis(m_throughputAxisX);
    m_tokensPerSecSeries->attachAxis(m_throughputAxisX);

    m_throughputAxisY = new QValueAxis();
    m_throughputAxisY->setTitleText("Throughput");
    m_throughputAxisY->setRange(0, 1000);
    m_throughputChart->addAxis(m_throughputAxisY, Qt::AlignLeft);
    m_samplesPerSecSeries->attachAxis(m_throughputAxisY);
    m_tokensPerSecSeries->attachAxis(m_throughputAxisY);

    m_throughputChart->legend()->setVisible(true);
    m_throughputChart->legend()->setAlignment(Qt::AlignTop);
}

void ObservabilityDashboard::createLatencyChart()
{
    m_latencyChart = new QChart();
    m_latencyChart->setTitle("Batch Latency Analysis");
    m_latencyChart->setAnimationOptions(QChart::SeriesAnimations);
    m_latencyChart->setBackgroundBrush(QBrush(Qt::white));

    // Create series for latency percentiles
    m_batchLatencySeries = new QLineSeries();
    m_batchLatencySeries->setName("Avg Latency");
    m_batchLatencySeries->setColor(Qt::darkBlue);

    m_p95LatencySeries = new QLineSeries();
    m_p95LatencySeries->setName("P95 Latency");
    m_p95LatencySeries->setColor(Qt::darkYellow);

    m_p99LatencySeries = new QLineSeries();
    m_p99LatencySeries->setName("P99 Latency");
    m_p99LatencySeries->setColor(Qt::darkRed);

    m_latencyChart->addSeries(m_batchLatencySeries);
    m_latencyChart->addSeries(m_p95LatencySeries);
    m_latencyChart->addSeries(m_p99LatencySeries);

    // Create axes
    m_latencyAxisX = new QDateTimeAxis();
    m_latencyAxisX->setFormat("hh:mm:ss");
    m_latencyAxisX->setTickCount(5);
    m_latencyChart->addAxis(m_latencyAxisX, Qt::AlignBottom);
    m_batchLatencySeries->attachAxis(m_latencyAxisX);
    m_p95LatencySeries->attachAxis(m_latencyAxisX);
    m_p99LatencySeries->attachAxis(m_latencyAxisX);

    m_latencyAxisY = new QValueAxis();
    m_latencyAxisY->setTitleText("Latency (ms)");
    m_latencyAxisY->setRange(0, 5000);
    m_latencyChart->addAxis(m_latencyAxisY, Qt::AlignLeft);
    m_batchLatencySeries->attachAxis(m_latencyAxisY);
    m_p95LatencySeries->attachAxis(m_latencyAxisY);
    m_p99LatencySeries->attachAxis(m_latencyAxisY);

    m_latencyChart->legend()->setVisible(true);
    m_latencyChart->legend()->setAlignment(Qt::AlignTop);
}

void ObservabilityDashboard::createMetricsPanel()
{
    m_metricsPanel = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(m_metricsPanel);

    // Current metrics group
    QGroupBox* currentMetricsGroup = new QGroupBox("Current Metrics", this);
    QGridLayout* gridLayout = new QGridLayout(currentMetricsGroup);

    QLabel* cpuLabel = new QLabel("CPU Usage:", this);
    m_currentCpuLabel = new QLabel("-- %", this);
    m_currentCpuLabel->setStyleSheet("font-weight: bold; color: blue;");
    gridLayout->addWidget(cpuLabel, 0, 0);
    gridLayout->addWidget(m_currentCpuLabel, 0, 1);

    QLabel* memoryLabel = new QLabel("Memory Usage:", this);
    m_currentMemoryLabel = new QLabel("-- MB", this);
    m_currentMemoryLabel->setStyleSheet("font-weight: bold; color: red;");
    gridLayout->addWidget(memoryLabel, 1, 0);
    gridLayout->addWidget(m_currentMemoryLabel, 1, 1);

    QLabel* gpuLabel = new QLabel("GPU Usage:", this);
    m_currentGpuLabel = new QLabel("-- %", this);
    m_currentGpuLabel->setStyleSheet("font-weight: bold; color: green;");
    gridLayout->addWidget(gpuLabel, 2, 0);
    gridLayout->addWidget(m_currentGpuLabel, 2, 1);

    QLabel* throughputLabel = new QLabel("Throughput:", this);
    m_currentThroughputLabel = new QLabel("-- samples/sec", this);
    m_currentThroughputLabel->setStyleSheet("font-weight: bold; color: purple;");
    gridLayout->addWidget(throughputLabel, 3, 0);
    gridLayout->addWidget(m_currentThroughputLabel, 3, 1);

    QLabel* peakMemoryLabel = new QLabel("Peak Memory:", this);
    m_peakMemoryLabel = new QLabel("-- MB", this);
    m_peakMemoryLabel->setStyleSheet("font-weight: bold; color: darkRed;");
    gridLayout->addWidget(peakMemoryLabel, 4, 0);
    gridLayout->addWidget(m_peakMemoryLabel, 4, 1);

    layout->addWidget(currentMetricsGroup);

    // Warnings group
    QGroupBox* warningsGroup = new QGroupBox("Performance Warnings", this);
    QVBoxLayout* warningsLayout = new QVBoxLayout(warningsGroup);
    m_warningsLabel = new QLabel("No warnings", this);
    m_warningsLabel->setStyleSheet("color: green;");
    m_warningsLabel->setWordWrap(true);
    warningsLayout->addWidget(m_warningsLabel);
    
    layout->addWidget(warningsGroup);
    layout->addStretch();
}

void ObservabilityDashboard::setupConnections()
{
    // Connect to profiler signals if available
    if (m_profiler) {
        // Note: These connections would be set up by AgenticIDE
        // This is here for reference
    }
}

void ObservabilityDashboard::onMetricsUpdated(float cpuPercent, float memoryMB, float gpuPercent, float gpuMemoryMB)
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 timestamp = now.toMSecsSinceEpoch();

    // Add data points to series
    m_cpuSeries->append(timestamp, cpuPercent);
    m_memorySeries->append(timestamp, memoryMB);
    m_gpuSeries->append(timestamp, gpuPercent);

    // Update metrics panel
    m_currentCpuLabel->setText(QString::number(cpuPercent, 'f', 1) + " %");
    m_currentMemoryLabel->setText(QString::number(memoryMB, 'f', 1) + " MB");
    m_currentGpuLabel->setText(QString::number(gpuPercent, 'f', 1) + " %");

    // Limit data points
    m_dataPointCount++;
    if (m_dataPointCount > m_maxDataPoints) {
        if (!m_cpuSeries->points().isEmpty()) {
            m_cpuSeries->removePoints(0, 1);
            m_memorySeries->removePoints(0, 1);
            m_gpuSeries->removePoints(0, 1);
        }
    }

    // Update axis ranges
    if (!m_cpuSeries->points().isEmpty()) {
        m_resourceAxisX->setRange(
            QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(m_cpuSeries->points().first().x())),
            QDateTime::fromMSecsSinceEpoch(timestamp)
        );
    }
}

void ObservabilityDashboard::onThroughputUpdated(float batchLatencyMs, float throughputSamples)
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 timestamp = now.toMSecsSinceEpoch();

    m_samplesPerSecSeries->append(timestamp, throughputSamples);
    m_batchLatencySeries->append(timestamp, batchLatencyMs);

    m_currentThroughputLabel->setText(QString::number(throughputSamples, 'f', 1) + " samples/sec");

    // Limit data points
    if (m_samplesPerSecSeries->points().size() > m_maxDataPoints) {
        if (!m_samplesPerSecSeries->points().isEmpty()) {
            m_samplesPerSecSeries->removePoints(0, 1);
            m_batchLatencySeries->removePoints(0, 1);
        }
    }
}

void ObservabilityDashboard::onPerformanceWarning(const QString& warning)
{
    m_warnings.push_back(warning);
    
    // Keep last 10 warnings
    if (m_warnings.size() > 10) {
        m_warnings.erase(m_warnings.begin());
    }

    // Update label
    QString warningText = m_warnings.empty() ? "No warnings" : "";
    for (const auto& w : m_warnings) {
        warningText += "⚠ " + w + "\n";
    }
    
    m_warningsLabel->setText(warningText);
    m_warningsLabel->setStyleSheet(m_warnings.empty() ? "color: green;" : "color: darkRed;");
}

void ObservabilityDashboard::clearCharts()
{
    m_cpuSeries->clear();
    m_memorySeries->clear();
    m_gpuSeries->clear();
    m_samplesPerSecSeries->clear();
    m_tokensPerSecSeries->clear();
    m_batchLatencySeries->clear();
    m_p95LatencySeries->clear();
    m_p99LatencySeries->clear();
    
    m_dataPointCount = 0;
    m_warnings.clear();
    
    m_currentCpuLabel->setText("-- %");
    m_currentMemoryLabel->setText("-- MB");
    m_currentGpuLabel->setText("-- %");
    m_currentThroughputLabel->setText("-- samples/sec");
    m_warningsLabel->setText("No warnings");
}

bool ObservabilityDashboard::exportData(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for export:" << filePath;
        return false;
    }

    QTextStream stream(&file);
    
    // Export CPU metrics
    stream << "Timestamp,CPU(%),Memory(MB),GPU(%)\n";
    
    for (const auto& point : m_cpuSeries->points()) {
        QDateTime time = QDateTime::fromMSecsSinceEpoch(static_cast<qint64>(point.x()));
        stream << time.toString("hh:mm:ss") << ","
               << point.y() << ",";
        
        // Find corresponding memory point
        for (const auto& memPoint : m_memorySeries->points()) {
            if (memPoint.x() == point.x()) {
                stream << memPoint.y() << ",";
                break;
            }
        }
        
        // Find corresponding GPU point
        for (const auto& gpuPoint : m_gpuSeries->points()) {
            if (gpuPoint.x() == point.x()) {
                stream << gpuPoint.y();
                break;
            }
        }
        
        stream << "\n";
    }

    file.close();
    qDebug() << "Dashboard data exported to:" << filePath;
    return true;
}

