#pragma once
#include <cstddef>

class ModelRouterAdapter;

class MetricsDashboard {
public:
    explicit MetricsDashboard(ModelRouterAdapter* adapter, void* parent = nullptr);
    ~MetricsDashboard();

    void createUI();
    void setupCharts();
    void startAutoRefresh();
    void stopAutoRefresh();
    void refresh();

private:
    ModelRouterAdapter* m_adapter = nullptr;
    void* m_refresh_timer = nullptr;
    void* m_total_cost_label = nullptr;
    void* m_total_requests_label = nullptr;
    void* m_avg_latency_label = nullptr;
    void* m_avg_success_rate_label = nullptr;
    void* m_active_model_label = nullptr;
    void* m_cost_chart_view = nullptr;
    void* m_latency_chart_view = nullptr;
};
