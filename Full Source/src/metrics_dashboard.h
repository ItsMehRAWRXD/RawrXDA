#ifndef METRICS_DASHBOARD_H
#define METRICS_DASHBOARD_H


#include <memory>

#include "model_router_adapter.h"

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
class MetricsDashboard : public void {

public:
    explicit MetricsDashboard(ModelRouterAdapter *adapter, void *parent = nullptr);
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

public:
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

private:
    void onCostUpdated(double total_cost);
    void onStatisticsUpdated(const void*& stats);
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
    void* *m_refresh_timer;
    int m_refresh_interval = 500;  // ms

    // Summary labels
    void *m_total_cost_label;
    void *m_total_requests_label;
    void *m_avg_latency_label;
    void *m_avg_success_rate_label;
    void *m_active_model_label;

    // Charts
// REMOVED_QT:     QChartView *m_cost_chart_view;
    QChart *m_cost_chart;
    QPieSeries *m_cost_pie_series;

// REMOVED_QT:     QChartView *m_latency_chart_view;
    QChart *m_latency_chart;
    QBarSeries *m_latency_bar_series;

// REMOVED_QT:     QChartView *m_success_rate_chart_view;
    QChart *m_success_rate_chart;
    QLineSeries *m_success_rate_line_series;

    // Tables
    QTableWidget *m_request_count_table;
    QTableWidget *m_error_log_table;
    QTableWidget *m_provider_status_table;

    // Historical data for trend charts
    std::vector<double> m_success_rate_history;
    std::vector<int64_t> m_timestamp_history;

    // State
    std::map<std::string, double> m_cost_by_model;
    std::map<std::string, int> m_request_count_by_model;
    std::map<std::string, double> m_latency_by_model;
};

#endif // METRICS_DASHBOARD_H


