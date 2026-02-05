#include "metrics_dashboard.h"
#include "model_router_adapter.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QPushButton>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QBarSet>
#include <QBarCategoryAxis>
#include <QValueAxis>
#include <QDateTimeAxis>

MetricsDashboard::MetricsDashboard(ModelRouterAdapter *adapter, QWidget *parent)
    : QWidget(parent), m_adapter(adapter), m_refresh_timer(nullptr)
{
    createUI();
    setupCharts();
    
    if (m_adapter) {
        connect(m_adapter, &ModelRouterAdapter::costUpdated,
                this, &MetricsDashboard::onCostUpdated);
        connect(m_adapter, &ModelRouterAdapter::statisticsUpdated,
                this, &MetricsDashboard::onStatisticsUpdated);
    }

    startAutoRefresh();
    
    qDebug() << "[MetricsDashboard] Constructed with auto-refresh every" << m_refresh_interval << "ms";
}

MetricsDashboard::~MetricsDashboard()
{
    stopAutoRefresh();
    qDebug() << "[MetricsDashboard] Destroyed";
}

void MetricsDashboard::createUI()
{
    QVBoxLayout *main_layout = new QVBoxLayout(this);
    main_layout->setContentsMargins(8, 8, 8, 8);
    main_layout->setSpacing(8);

    // === Summary Panel ===
    QGroupBox *summary_group = new QGroupBox("Summary Statistics", this);
    QGridLayout *summary_grid = new QGridLayout(summary_group);

    m_total_cost_label = new QLabel("$0.00", this);
    m_total_cost_label->setStyleSheet("font-size: 24pt; font-weight: bold; color: #0066cc;");
    summary_grid->addWidget(new QLabel("Total Cost:", this), 0, 0);
    summary_grid->addWidget(m_total_cost_label, 0, 1);

    m_total_requests_label = new QLabel("0", this);
    m_total_requests_label->setStyleSheet("font-size: 18pt; font-weight: bold;");
    summary_grid->addWidget(new QLabel("Total Requests:", this), 0, 2);
    summary_grid->addWidget(m_total_requests_label, 0, 3);

    m_avg_latency_label = new QLabel("— ms", this);
    m_avg_latency_label->setStyleSheet("font-size: 18pt; font-weight: bold;");
    summary_grid->addWidget(new QLabel("Avg Latency:", this), 1, 0);
    summary_grid->addWidget(m_avg_latency_label, 1, 1);

    m_avg_success_rate_label = new QLabel("—%", this);
    m_avg_success_rate_label->setStyleSheet("font-size: 18pt; font-weight: bold;");
    summary_grid->addWidget(new QLabel("Success Rate:", this), 1, 2);
    summary_grid->addWidget(m_avg_success_rate_label, 1, 3);

    m_active_model_label = new QLabel("None", this);
    m_active_model_label->setStyleSheet("font-size: 14pt; color: #666;");
    summary_grid->addWidget(new QLabel("Active Model:", this), 2, 0);
    summary_grid->addWidget(m_active_model_label, 2, 1, 1, 3);

    main_layout->addWidget(summary_group);

    // === Charts Row ===
    QHBoxLayout *charts_layout = new QHBoxLayout();

    // Cost breakdown pie chart
    QGroupBox *cost_group = new QGroupBox("Cost Breakdown by Model", this);
    QVBoxLayout *cost_layout = new QVBoxLayout(cost_group);
    m_cost_chart_view = new QChartView(this);
    m_cost_chart_view->setRenderHint(QPainter::Antialiasing);
    m_cost_chart_view->setMinimumHeight(250);
    cost_layout->addWidget(m_cost_chart_view);
    charts_layout->addWidget(cost_group);

    // Latency bar chart
    QGroupBox *latency_group = new QGroupBox("Average Latency by Model", this);
    QVBoxLayout *latency_layout = new QVBoxLayout(latency_group);
    m_latency_chart_view = new QChartView(this);
    m_latency_chart_view->setRenderHint(QPainter::Antialiasing);
    m_latency_chart_view->setMinimumHeight(250);
    latency_layout->addWidget(m_latency_chart_view);
    charts_layout->addWidget(latency_group);

    main_layout->addLayout(charts_layout);

    // === Success Rate Trend Chart ===
    QGroupBox *success_group = new QGroupBox("Success Rate Trend", this);
    QVBoxLayout *success_layout = new QVBoxLayout(success_group);
    m_success_rate_chart_view = new QChartView(this);
    m_success_rate_chart_view->setRenderHint(QPainter::Antialiasing);
    m_success_rate_chart_view->setMinimumHeight(180);
    success_layout->addWidget(m_success_rate_chart_view);
    main_layout->addWidget(success_group);

    // === Tables ===
    QHBoxLayout *tables_layout = new QHBoxLayout();

    // Request count table
    QGroupBox *requests_group = new QGroupBox("Requests per Model", this);
    QVBoxLayout *requests_layout = new QVBoxLayout(requests_group);
    m_request_count_table = new QTableWidget(0, 2, this);
    m_request_count_table->setHorizontalHeaderLabels({"Model", "Count"});
    m_request_count_table->horizontalHeader()->setStretchLastSection(true);
    m_request_count_table->setMaximumHeight(150);
    requests_layout->addWidget(m_request_count_table);
    tables_layout->addWidget(requests_group);

    // Provider status table
    QGroupBox *providers_group = new QGroupBox("Provider Status", this);
    QVBoxLayout *providers_layout = new QVBoxLayout(providers_group);
    m_provider_status_table = new QTableWidget(0, 2, this);
    m_provider_status_table->setHorizontalHeaderLabels({"Provider", "Status"});
    m_provider_status_table->horizontalHeader()->setStretchLastSection(true);
    m_provider_status_table->setMaximumHeight(150);
    providers_layout->addWidget(m_provider_status_table);
    tables_layout->addWidget(providers_group);

    main_layout->addLayout(tables_layout);

    // === Error Log ===
    QGroupBox *errors_group = new QGroupBox("Recent Errors", this);
    QVBoxLayout *errors_layout = new QVBoxLayout(errors_group);
    m_error_log_table = new QTableWidget(0, 3, this);
    m_error_log_table->setHorizontalHeaderLabels({"Timestamp", "Model", "Error"});
    m_error_log_table->horizontalHeader()->setStretchLastSection(true);
    m_error_log_table->setMaximumHeight(120);
    errors_layout->addWidget(m_error_log_table);
    main_layout->addWidget(errors_group);

    // === Action Buttons ===
    QHBoxLayout *button_layout = new QHBoxLayout();
    
    QPushButton *refresh_button = new QPushButton("Refresh Now", this);
    connect(refresh_button, &QPushButton::clicked, this, &MetricsDashboard::refreshMetrics);
    button_layout->addWidget(refresh_button);

    QPushButton *export_csv_button = new QPushButton("Export CSV", this);
    connect(export_csv_button, &QPushButton::clicked, this, &MetricsDashboard::exportToCsv);
    button_layout->addWidget(export_csv_button);

    QPushButton *export_json_button = new QPushButton("Export JSON", this);
    connect(export_json_button, &QPushButton::clicked, this, &MetricsDashboard::exportToJson);
    button_layout->addWidget(export_json_button);

    QPushButton *clear_button = new QPushButton("Clear History", this);
    connect(clear_button, &QPushButton::clicked, this, &MetricsDashboard::clearHistory);
    button_layout->addWidget(clear_button);

    button_layout->addStretch();
    main_layout->addLayout(button_layout);

    setLayout(main_layout);
    setMinimumSize(900, 800);
}

void MetricsDashboard::setupCharts()
{
    // === Cost Pie Chart ===
    m_cost_chart = new QChart();
    m_cost_chart->setTitle("Cost Distribution");
    m_cost_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_cost_pie_series = new QPieSeries();
    m_cost_chart->addSeries(m_cost_pie_series);
    m_cost_chart->legend()->setAlignment(Qt::AlignRight);
    m_cost_chart_view->setChart(m_cost_chart);

    // === Latency Bar Chart ===
    m_latency_chart = new QChart();
    m_latency_chart->setTitle("Response Latency (ms)");
    m_latency_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_latency_bar_series = new QBarSeries();
    m_latency_chart->addSeries(m_latency_bar_series);
    m_latency_chart->createDefaultAxes();
    m_latency_chart_view->setChart(m_latency_chart);

    // === Success Rate Line Chart ===
    m_success_rate_chart = new QChart();
    m_success_rate_chart->setTitle("Success Rate Over Time (%)");
    m_success_rate_chart->setAnimationOptions(QChart::SeriesAnimations);
    m_success_rate_line_series = new QLineSeries();
    m_success_rate_chart->addSeries(m_success_rate_line_series);
    m_success_rate_chart->createDefaultAxes();
    m_success_rate_chart_view->setChart(m_success_rate_chart);
}

void MetricsDashboard::startAutoRefresh()
{
    if (!m_refresh_timer) {
        m_refresh_timer = new QTimer(this);
        connect(m_refresh_timer, &QTimer::timeout, this, &MetricsDashboard::onAutoRefreshTriggered);
    }
    m_refresh_timer->start(m_refresh_interval);
    qDebug() << "[MetricsDashboard] Auto-refresh started";
}

void MetricsDashboard::stopAutoRefresh()
{
    if (m_refresh_timer) {
        m_refresh_timer->stop();
        qDebug() << "[MetricsDashboard] Auto-refresh stopped";
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
    m_total_cost_label->setText(QString("$%1").arg(total_cost, 0, 'f', 4));

    QString active_model = m_adapter->getActiveModel();
    m_active_model_label->setText(active_model.isEmpty() ? "None" : active_model);

    QJsonObject stats = m_adapter->getStatistics();
    int total_requests = stats.value("total_requests").toInt();
    double avg_latency = stats.value("avg_latency_ms").toDouble();
    int success_rate = stats.value("success_rate").toInt();

    m_total_requests_label->setText(QString::number(total_requests));
    m_avg_latency_label->setText(QString("%1 ms").arg((int)avg_latency));
    m_avg_success_rate_label->setText(QString("%1%").arg(success_rate));
}

void MetricsDashboard::updateCostChart()
{
    if (!m_adapter) return;

    m_cost_pie_series->clear();
    
    QMap<QString, double> cost_breakdown = m_adapter->getCostBreakdown();
    
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
    
    QBarSet *bar_set = new QBarSet("Latency");
    QStringList categories;
    
    QJsonObject stats = m_adapter->getStatistics();
    QJsonArray models = stats.value("models").toArray();
    
    for (const auto& model_val : models) {
        QJsonObject model_obj = model_val.toObject();
        QString name = model_obj.value("name").toString();
        double latency = model_obj.value("avg_latency_ms").toDouble();
        
        *bar_set << latency;
        categories << name;
    }
    
    m_latency_bar_series->append(bar_set);
}

void MetricsDashboard::updateSuccessRateChart()
{
    if (!m_adapter) return;

    QJsonObject stats = m_adapter->getStatistics();
    int success_rate = stats.value("success_rate").toInt();
    
    qint64 now = QDateTime::currentMSecsSinceEpoch();
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
    
    QJsonObject stats = m_adapter->getStatistics();
    QJsonArray models = stats.value("models").toArray();
    
    for (const auto& model_val : models) {
        QJsonObject model_obj = model_val.toObject();
        QString name = model_obj.value("name").toString();
        int count = model_obj.value("request_count").toInt();
        
        int row = m_request_count_table->rowCount();
        m_request_count_table->insertRow(row);
        m_request_count_table->setItem(row, 0, new QTableWidgetItem(name));
        m_request_count_table->setItem(row, 1, new QTableWidgetItem(QString::number(count)));
    }
}

void MetricsDashboard::updateErrorLog()
{
    // Placeholder for error log updates
}

void MetricsDashboard::updateProviderStatus()
{
    m_provider_status_table->setRowCount(0);
    
    QStringList providers = {"OpenAI", "Anthropic", "Google", "Moonshot", "Azure", "AWS"};
    for (const QString& provider : providers) {
        int row = m_provider_status_table->rowCount();
        m_provider_status_table->insertRow(row);
        m_provider_status_table->setItem(row, 0, new QTableWidgetItem(provider));
        m_provider_status_table->setItem(row, 1, new QTableWidgetItem("Active"));
    }
}

void MetricsDashboard::exportToCsv()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Metrics to CSV", "", "CSV Files (*.csv)");
    if (filename.isEmpty()) return;
    
    if (m_adapter && m_adapter->exportStatisticsToCsv(filename)) {
        qDebug() << "[MetricsDashboard] Exported to CSV:" << filename;
    }
}

void MetricsDashboard::exportToJson()
{
    QString filename = QFileDialog::getSaveFileName(this, "Export Metrics to JSON", "", "JSON Files (*.json)");
    if (filename.isEmpty()) return;
    
    if (!m_adapter) return;
    
    QJsonObject stats = m_adapter->getStatistics();
    QJsonDocument doc(stats);
    
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        qDebug() << "[MetricsDashboard] Exported to JSON:" << filename;
    }
}

void MetricsDashboard::clearHistory()
{
    m_success_rate_history.clear();
    m_timestamp_history.clear();
    m_success_rate_line_series->clear();
    qDebug() << "[MetricsDashboard] History cleared";
}

void MetricsDashboard::resetCharts()
{
    m_cost_pie_series->clear();
    m_latency_bar_series->clear();
    m_success_rate_line_series->clear();
}

void MetricsDashboard::onCostUpdated(double total_cost)
{
    m_total_cost_label->setText(QString("$%1").arg(total_cost, 0, 'f', 4));
}

void MetricsDashboard::onStatisticsUpdated(const QJsonObject& stats)
{
    refreshMetrics();
}

void MetricsDashboard::onAutoRefreshTriggered()
{
    refreshMetrics();
}

#include "metrics_dashboard.moc"
