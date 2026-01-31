#include "metrics_dashboard.h"
#include "model_router_adapter.h"


MetricsDashboard::MetricsDashboard(ModelRouterAdapter *adapter, void *parent)
    : void(parent), m_adapter(adapter), m_refresh_timer(nullptr)
{
    createUI();
    setupCharts();
    
    if (m_adapter) {
// Qt connect removed
// Qt connect removed
    }

    startAutoRefresh();
    
}

MetricsDashboard::~MetricsDashboard()
{
    stopAutoRefresh();
}

void MetricsDashboard::createUI()
{
    void *main_layout = new void(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // === Summary Panel ===
    void *summary_group = new void("Summary Statistics", this);
    void *summary_grid = new void(summary_group);

    m_total_cost_label = new void("$0.00", this);
    m_total_cost_label->setStyleSheet("font-size: 24pt; font-weight: bold; color: #0066cc;");
    summary_grid->addWidget(new void("Total Cost:", this), 0, 0);
    summary_grid->addWidget(m_total_cost_label, 0, 1);

    m_total_requests_label = new void("0", this);
    m_total_requests_label->setStyleSheet("font-size: 18pt; font-weight: bold;");
    summary_grid->addWidget(new void("Total Requests:", this), 0, 2);
    summary_grid->addWidget(m_total_requests_label, 0, 3);

    m_avg_latency_label = new void("— ms", this);
    m_avg_latency_label->setStyleSheet("font-size: 18pt; font-weight: bold;");
    summary_grid->addWidget(new void("Avg Latency:", this), 1, 0);
    summary_grid->addWidget(m_avg_latency_label, 1, 1);

    m_avg_success_rate_label = new void("—%", this);
    m_avg_success_rate_label->setStyleSheet("font-size: 18pt; font-weight: bold;");
    summary_grid->addWidget(new void("Success Rate:", this), 1, 2);
    summary_grid->addWidget(m_avg_success_rate_label, 1, 3);

    m_active_model_label = new void("None", this);
    m_active_model_label->setStyleSheet("font-size: 14pt; color: #666;");
    summary_grid->addWidget(new void("Active Model:", this), 2, 0);
    summary_grid->addWidget(m_active_model_label, 2, 1, 1, 3);

    main_layout->addWidget(summary_group);

    // === Charts Row ===
    void *charts_layout = new void();

    // Cost breakdown pie chart
    void *cost_group = new void("Cost Breakdown by Model", this);
    void *cost_layout = new void(cost_group);
    m_cost_chart_view = nullptr;
    m_cost_chart_view->setRenderHint(QPainter::Antialiasing);
    m_cost_chart_view->setMinimumHeight(250);
    cost_layout->addWidget(m_cost_chart_view);
    charts_layout->addWidget(cost_group);

    // Latency bar chart
    void *latency_group = new void("Average Latency by Model", this);
    void *latency_layout = new void(latency_group);
    m_latency_chart_view = nullptr;
    m_latency_chart_view->setRenderHint(QPainter::Antialiasing);
    m_latency_chart_view->setMinimumHeight(250);
    latency_layout->addWidget(m_latency_chart_view);
    charts_layout->addWidget(latency_group);

    main_layout->addLayout(charts_layout);

    // === Success Rate Trend Chart ===
    void *success_group = new void("Success Rate Trend", this);
    void *success_layout = new void(success_group);
    m_success_rate_chart_view = nullptr;
    m_success_rate_chart_view->setRenderHint(QPainter::Antialiasing);
    m_success_rate_chart_view->setMinimumHeight(180);
    success_layout->addWidget(m_success_rate_chart_view);
    main_layout->addWidget(success_group);

    // === Tables ===
    void *tables_layout = new void();

    // Request count table
    void *requests_group = new void("Requests per Model", this);
    void *requests_layout = new void(requests_group);
    m_request_count_table = nullptr;
    m_request_count_table->setHorizontalHeaderLabels({"Model", "Count"});
    m_request_count_table->horizontalHeader()->setStretchLastSection(true);
    m_request_count_table->setMaximumHeight(150);
    requests_layout->addWidget(m_request_count_table);
    tables_layout->addWidget(requests_group);

    // Provider status table
    void *providers_group = new void("Provider Status", this);
    void *providers_layout = new void(providers_group);
    m_provider_status_table = nullptr;
    m_provider_status_table->setHorizontalHeaderLabels({"Provider", "Status"});
    m_provider_status_table->horizontalHeader()->setStretchLastSection(true);
    m_provider_status_table->setMaximumHeight(150);
    providers_layout->addWidget(m_provider_status_table);
    tables_layout->addWidget(providers_group);

    main_layout->addLayout(tables_layout);

    // === Error Log ===
    void *errors_group = new void("Recent Errors", this);
    void *errors_layout = new void(errors_group);
    m_error_log_table = nullptr;
    m_error_log_table->setHorizontalHeaderLabels({"Timestamp", "Model", "Error"});
    m_error_log_table->horizontalHeader()->setStretchLastSection(true);
    m_error_log_table->setMaximumHeight(120);
    errors_layout->addWidget(m_error_log_table);
    main_layout->addWidget(errors_group);

    // === Action Buttons ===
    void *button_layout = new void();
    
    void *refresh_button = new void("Refresh Now", this);
// Qt connect removed
    button_layout->addWidget(refresh_button);

    void *export_csv_button = new void("Export CSV", this);
// Qt connect removed
    button_layout->addWidget(export_csv_button);

    void *export_json_button = new void("Export JSON", this);
// Qt connect removed
    button_layout->addWidget(export_json_button);

    void *clear_button = new void("Clear History", this);
// Qt connect removed
    button_layout->addWidget(clear_button);

    button_layout->addStretch();
    main_layout->addLayout(button_layout);

    setLayout(main_layout);
    setMinimumSize(900, 800);
}

void MetricsDashboard::setupCharts()
{
    // === Cost Pie Chart ===
    m_cost_chart = nullptr;
    m_cost_chart->setTitle("Cost Distribution");
    m_cost_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_cost_pie_series = nullptr;
    m_cost_chart->addSeries(m_cost_pie_series);
    m_cost_chart->legend()->setAlignment(//AlignRight);
    m_cost_chart_view->setChart(m_cost_chart);

    // === Latency Bar Chart ===
    m_latency_chart = nullptr;
    m_latency_chart->setTitle("Response Latency (ms)");
    m_latency_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_latency_bar_series = nullptr;
    m_latency_chart->addSeries(m_latency_bar_series);
    m_latency_chart->createDefaultAxes();
    m_latency_chart_view->setChart(m_latency_chart);

    // === Success Rate Line Chart ===
    m_success_rate_chart = nullptr;
    m_success_rate_chart->setTitle("Success Rate Over Time (%)");
    m_success_rate_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_success_rate_line_series = nullptr;
    m_success_rate_chart->addSeries(m_success_rate_line_series);
    m_success_rate_chart->createDefaultAxes();
    m_success_rate_chart_view->setChart(m_success_rate_chart);
}

void MetricsDashboard::startAutoRefresh()
{
    if (!m_refresh_timer) {
        m_refresh_timer = new void*(this);
// Qt connect removed
    }
    m_refresh_timer->start(m_refresh_interval);
}

void MetricsDashboard::stopAutoRefresh()
{
    if (m_refresh_timer) {
        m_refresh_timer->stop();
    }
}

void MetricsDashboard::setRefreshInterval(int ms)
{
    m_refresh_interval = ms;
    if (m_refresh_timer && m_refresh_timer->isActive()) {
        m_refresh_timer->setInterval(ms);
    }
}

void MetricsDashboard::refreshMetrics()
{
    if (!m_adapter) return;

    updateSummaryLabels();
    updateCostChart();
    updateLatencyChart();
    updateSuccessRateChart();
    updateRequestCountTable();
    updateProviderStatus();
}

void MetricsDashboard::updateSummaryLabels()
{
    if (!m_adapter) return;

    double total_cost = m_adapter->getTotalCost();
    m_total_cost_label->setText(std::string("$%1"));

    std::string active_model = m_adapter->getActiveModel();
    m_active_model_label->setText(active_model.empty() ? "None" : active_model);

    void* stats = m_adapter->getStatistics();
    int total_requests = stats.value("total_requests").toInt();
    double avg_latency = stats.value("avg_latency_ms").toDouble();
    int success_rate = stats.value("success_rate").toInt();

    m_total_requests_label->setText(std::string::number(total_requests));
    m_avg_latency_label->setText(std::string("%1 ms")avg_latency));
    m_avg_success_rate_label->setText(std::string("%1%"));
}

void MetricsDashboard::updateCostChart()
{
    if (!m_adapter) return;

    m_cost_pie_series->clear();
    
    std::map<std::string, double> cost_breakdown = m_adapter->getCostBreakdown();
    
    for (auto it = cost_breakdown.begin(); it != cost_breakdown.end(); ++it) {
        if (it.value() > 0.0001) {
            m_cost_pie_series->append(it.key(), it.value());
        }
    }
    
    if (m_cost_pie_series->count() == 0) {
        m_cost_pie_series->append("No data", 1.0);
    }
}

void MetricsDashboard::updateLatencyChart()
{
    if (!m_adapter) return;

    m_latency_bar_series->clear();
    
    QBarSet *bar_set = nullptr;
    std::vector<std::string> categories;
    
    void* stats = m_adapter->getStatistics();
    void* models = stats.value("models").toArray();
    
    for (const auto& model_val : models) {
        void* model_obj = model_val.toObject();
        std::string name = model_obj.value("name").toString();
        double latency = model_obj.value("avg_latency_ms").toDouble();
        
        *bar_set << latency;
        categories << name;
    }
    
    m_latency_bar_series->append(bar_set);
}

void MetricsDashboard::updateSuccessRateChart()
{
    if (!m_adapter) return;

    void* stats = m_adapter->getStatistics();
    int success_rate = stats.value("success_rate").toInt();
    
    int64_t now = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
    m_timestamp_history.append(now);
    m_success_rate_history.append(success_rate);
    
    // Keep only last 50 data points
    if (m_success_rate_history.size() > 50) {
        m_success_rate_history.removeFirst();
        m_timestamp_history.removeFirst();
    }
    
    m_success_rate_line_series->clear();
    for (int i = 0; i < m_success_rate_history.size(); ++i) {
        m_success_rate_line_series->append(i, m_success_rate_history[i]);
    }
}

void MetricsDashboard::updateRequestCountTable()
{
    if (!m_adapter) return;

    m_request_count_table->setRowCount(0);
    
    void* stats = m_adapter->getStatistics();
    void* models = stats.value("models").toArray();
    
    for (const auto& model_val : models) {
        void* model_obj = model_val.toObject();
        std::string name = model_obj.value("name").toString();
        int count = model_obj.value("request_count").toInt();
        
        int row = m_request_count_table->rowCount();
        m_request_count_table->insertRow(row);
        m_request_count_table->setItem(row, 0, nullptr);
        m_request_count_table->setItem(row, 1, nullptr));
    }
}

void MetricsDashboard::updateErrorLog()
{
    // Placeholder for error log updates
}

void MetricsDashboard::updateProviderStatus()
{
    m_provider_status_table->setRowCount(0);
    
    std::vector<std::string> providers = {"OpenAI", "Anthropic", "Google", "Moonshot", "Azure", "AWS"};
    for (const std::string& provider : providers) {
        int row = m_provider_status_table->rowCount();
        m_provider_status_table->insertRow(row);
        m_provider_status_table->setItem(row, 0, nullptr);
        m_provider_status_table->setItem(row, 1, nullptr);
    }
}

void MetricsDashboard::exportToCsv()
{
    std::string filename = QFileDialog::getSaveFileName(this, "Export Metrics to CSV", "", "CSV Files (*.csv)");
    if (filename.empty()) return;
    
    if (m_adapter && m_adapter->exportStatisticsToCsv(filename)) {
    }
}

void MetricsDashboard::exportToJson()
{
    std::string filename = QFileDialog::getSaveFileName(this, "Export Metrics to JSON", "", "JSON Files (*.json)");
    if (filename.empty()) return;
    
    if (!m_adapter) return;
    
    void* stats = m_adapter->getStatistics();
    void* doc(stats);
    
    std::fstream file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
    }
}

void MetricsDashboard::clearHistory()
{
    m_success_rate_history.clear();
    m_timestamp_history.clear();
    m_success_rate_line_series->clear();
}

void MetricsDashboard::resetCharts()
{
    m_cost_pie_series->clear();
    m_latency_bar_series->clear();
    m_success_rate_line_series->clear();
}

void MetricsDashboard::onCostUpdated(double total_cost)
{
    m_total_cost_label->setText(std::string("$%1"));
}

void MetricsDashboard::onStatisticsUpdated(const void*& stats)
{
    refreshMetrics();
}

void MetricsDashboard::onAutoRefreshTriggered()
{
    refreshMetrics();
}

// MOC removed


