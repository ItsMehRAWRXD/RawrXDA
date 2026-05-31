#pragma once

#include <QWidget>
#include <QString>
#include <QJsonObject>
#include <memory>

// Forward declarations
class QTabWidget;
class QLabel;
class QGridLayout;
class Profiler;
class QChart;
class QChartView;
class QLineSeries;
class QValueAxis;
class QDateTimeAxis;

/**
 * @class ObservabilityDashboard
 * @brief Real-time observability dashboard for training metrics
 *
 * Displays live charts and metrics including:
 * - CPU, GPU, Memory utilization over time
 * - Training throughput (samples/sec, tokens/sec)
 * - Phase-specific latencies
 * - Batch processing times with latency percentiles
 * - Performance warnings and anomaly detection
 *
 * Uses Qt Charts for visualization.
 */
class ObservabilityDashboard : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructor
     * @param profiler Pointer to Profiler instance (non-owning)
     * @param parent Parent widget for Qt ownership
     */
    explicit ObservabilityDashboard(Profiler* profiler, QWidget* parent = nullptr);
    ~ObservabilityDashboard() override;

    // Two-phase initialization: call after QApplication exists
    void initialize();

public slots:
    /**
     * @brief Called when new profiling metrics available
     * @param cpuPercent CPU usage 0-100%
     * @param memoryMB Memory in MB
     * @param gpuPercent GPU usage 0-100%
     * @param gpuMemoryMB GPU memory in MB
     */
    void onMetricsUpdated(float cpuPercent, float memoryMB, float gpuPercent, float gpuMemoryMB);

    /**
     * @brief Called when new batch latency data available
     * @param batchLatencyMs Batch processing time
     * @param throughputSamples Samples per second
     */
    void onThroughputUpdated(float batchLatencyMs, float throughputSamples);

    /**
     * @brief Called when performance warning detected
     * @param warning Warning message
     */
    void onPerformanceWarning(const QString& warning);

    /**
     * @brief Clear all charts and reset data
     */
    void clearCharts();

    /**
     * @brief Export dashboard data to file
     * @param filePath Path to export to (CSV or JSON)
     * @return true if export successful
     */
    bool exportData(const QString& filePath);

signals:
    /**
     * @brief Dashboard is requesting profiler data update
     */
    void requestMetricsRefresh();

private:
    void setupUI();
    void setupConnections();
    void createResourceChart();
    void createThroughputChart();
    void createLatencyChart();
    void createMetricsPanel();

    // ===== UI Components =====
    QTabWidget* m_tabWidget;
    
    // Resource usage tab
    QChartView* m_resourceChartView;
    QChart* m_resourceChart;
    QLineSeries* m_cpuSeries;
    QLineSeries* m_memorySeries;
    QLineSeries* m_gpuSeries;
    QDateTimeAxis* m_resourceAxisX;
    QValueAxis* m_resourceAxisY;

    // Throughput tab
    QChartView* m_throughputChartView;
    QChart* m_throughputChart;
    QLineSeries* m_samplesPerSecSeries;
    QLineSeries* m_tokensPerSecSeries;
    QDateTimeAxis* m_throughputAxisX;
    QValueAxis* m_throughputAxisY;

    // Latency tab
    QChartView* m_latencyChartView;
    QChart* m_latencyChart;
    QLineSeries* m_batchLatencySeries;
    QLineSeries* m_p95LatencySeries;
    QLineSeries* m_p99LatencySeries;
    QDateTimeAxis* m_latencyAxisX;
    QValueAxis* m_latencyAxisY;

    // Metrics panel tab
    QWidget* m_metricsPanel;
    QLabel* m_currentCpuLabel;
    QLabel* m_currentMemoryLabel;
    QLabel* m_currentGpuLabel;
    QLabel* m_currentThroughputLabel;
    QLabel* m_peakMemoryLabel;
    QLabel* m_warningsLabel;

    // ===== State =====
    Profiler* m_profiler;
    int m_dataPointCount = 0;
    int m_maxDataPoints = 300; // Keep last 300 data points
    std::vector<QString> m_warnings;
};

