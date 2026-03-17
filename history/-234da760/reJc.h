#ifndef METRICS_DASHBOARD_H
#define METRICS_DASHBOARD_H

#include <QWidget>
#include <QLabel>
#include <QTableWidget>
#include <QChartView>
#include <QPieSeries>
#include <QBarSeries>
#include <QLineSeries>
#include <QChart>
#include <QTimer>
#include <QVector>
#include <QMap>
#include <memory>
#include <QHeaderView>
#include "model_router_adapter.h"

;
QT_CHARTS_USE_NAMESPACE

class ModelRouterAdapter;

/**
 * @class MetricsDashboard
 * @brief Real-time metrics and statistics visualization dashboard
 * 
 * Displays:
 * - Total cost across all models
 * - Cost breakdown by model (pie chart)
 * - Latency histogram (bar chart)
 * - Success rate trend (line chart)
 * - Request count statistics
 * - Recent error logs
 * - Provider health status
 */
class MetricsDashboard : public QWidget {
    Q_OBJECT

public:
    explicit MetricsDashboard(ModelRouterAdapter *adapter, QWidget *parent = nullptr);
    ~MetricsDashboard();

    /**
     * Start auto-refresh timer (every 500ms)
     */
    void startAutoRefresh();

    /**
     * Stop auto-refresh timer
     */
    void stopAutoRefresh();

    /**
     * Set refresh interval in milliseconds
     */
    void setRefreshInterval(int ms);

    /**
     * Get current refresh interval
     */
    int getRefreshInterval() const { return m_refresh_interval; }

public slots:
    /**
     * Refresh all metrics from adapter
     */
    void refreshMetrics();

    /**
     * Export current metrics to CSV
     */
    void exportToCsv();

    /**
     * Export current metrics to JSON
     */
    void exportToJson();

    /**
     * Clear all historical data
     */
    void clearHistory();

    /**
     * Reset all charts
     */
    void resetCharts();

private slots:
    void onCostUpdated(double total_cost);
    void onStatisticsUpdated(const QJsonObject& stats);
    void onAutoRefreshTriggered();

private:
    void createUI();
    void setupCharts();
    void updateCostChart();
    void updateLatencyChart();
    void updateSuccessRateChart();
    void updateRequestCountTable();
    void updateErrorLog();
    void updateSummaryLabels();
    void updateProviderStatus();

    ModelRouterAdapter *m_adapter;

    // Refresh timer
    QTimer *m_refresh_timer;
    int m_refresh_interval = 500;  // ms

    // Summary labels
    QLabel *m_total_cost_label;
    QLabel *m_total_requests_label;
    QLabel *m_avg_latency_label;
    QLabel *m_avg_success_rate_label;
    QLabel *m_active_model_label;

    // Charts
    QChartView *m_cost_chart_view;
    QChart *m_cost_chart;
    QPieSeries *m_cost_pie_series;

    QChartView *m_latency_chart_view;
    QChart *m_latency_chart;
    QBarSeries *m_latency_bar_series;

    QChartView *m_success_rate_chart_view;
    QChart *m_success_rate_chart;
    QLineSeries *m_success_rate_line_series;

    // Tables
    QTableWidget *m_request_count_table;
    QTableWidget *m_error_log_table;
    QTableWidget *m_provider_status_table;

    // Historical data for trend charts
    QVector<double> m_success_rate_history;
    QVector<qint64> m_timestamp_history;

    // State
    QMap<QString, double> m_cost_by_model;
    QMap<QString, int> m_request_count_by_model;
    QMap<QString, double> m_latency_by_model;
};

#endif // METRICS_DASHBOARD_H
