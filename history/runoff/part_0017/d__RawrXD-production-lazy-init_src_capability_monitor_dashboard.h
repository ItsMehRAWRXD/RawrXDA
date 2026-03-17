/**
 * @file capability_monitor_dashboard.h
 * @brief Visual Dashboard for Capability & Performance Monitoring
 * 
 * Complete performance monitoring UI with:
 * - Real-time capability metrics (CPU, GPU, Memory, Disk I/O)
 * - Component performance visualization
 * - Agent execution tracking
 * - Resource utilization charts
 * - Alert system for anomalies
 * - Historical data visualization
 */

#pragma once

#include <QWidget>
#include <QString>
#include <QMap>
#include <QVector>
#include <QDateTime>
#include "observability_dashboard.h"

class QTabWidget;
class QChartView;
class QChart;
class QLineSeries;
class QTableWidget;
class QLabel;
class QProgressBar;
class QPushButton;
class Profiler;

/**
 * @class CapabilityMonitorDashboard
 * @brief Comprehensive capability and performance monitoring dashboard
 */
class CapabilityMonitorDashboard : public QWidget {
    Q_OBJECT

public:
    explicit CapabilityMonitorDashboard(Profiler* profiler = nullptr, QWidget* parent = nullptr);
    ~CapabilityMonitorDashboard() override;

    void initialize();
    void startMonitoring();
    void stopMonitoring();

public slots:
    void onCapabilityMetricsUpdated(const QJsonObject& metrics);
    void onComponentPerformanceChanged(const QString& component, double performance);
    void onAgentExecutionStarted(const QString& agentId);
    void onAgentExecutionCompleted(const QString& agentId, qint64 durationMs);
    void refreshDashboard();

signals:
    void alertTriggered(const QString& alertType, const QString& message);
    void performanceThresholdExceeded(const QString& metric, double value);

private:
    void setupUI();
    void createCapabilityPanel();
    void createComponentPanel();
    void createAgentPanel();
    void createHistoricalPanel();
    void updateCharts();
    
    Profiler* m_profiler = nullptr;
    ObservabilityDashboard* m_observabilityDashboard = nullptr;
    
    QTabWidget* m_tabWidget = nullptr;
    QTableWidget* m_capabilityTable = nullptr;
    QTableWidget* m_componentTable = nullptr;
    QTableWidget* m_agentTable = nullptr;
    
    QChartView* m_capabilityChart = nullptr;
    QChartView* m_componentChart = nullptr;
    
    QVector<QLineSeries*> m_capabilitySeries;
    QMap<QString, double> m_currentMetrics;
    
    QLabel* m_cpuLabel = nullptr;
    QLabel* m_gpuLabel = nullptr;
    QLabel* m_memoryLabel = nullptr;
    QProgressBar* m_cpuBar = nullptr;
    QProgressBar* m_gpuBar = nullptr;
    QProgressBar* m_memoryBar = nullptr;
    
    bool m_monitoring = false;
};
