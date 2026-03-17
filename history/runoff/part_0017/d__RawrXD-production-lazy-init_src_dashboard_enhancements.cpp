/**
 * @file dashboard_enhancements.cpp
 * @brief Enhanced dashboard implementation
 */

#include "dashboard_enhancements.h"
#include <QDebug>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QFile>
#include <QScrollArea>
#include <algorithm>
#include <cmath>

// ============================================================================
// DashboardMetricsProvider Implementation
// ============================================================================

DashboardMetricsProvider::DashboardMetricsProvider(QObject* parent)
    : QObject(parent)
{
    qInfo() << "[DashboardMetricsProvider] Initialized";
}

DashboardMetricsProvider::SystemMetrics DashboardMetricsProvider::getMetrics() const
{
    QMutexLocker locker(&m_mutex);
    return m_currentMetrics;
}

std::vector<DashboardMetricsProvider::SystemMetrics> DashboardMetricsProvider::getMetricsHistory(int samples) const
{
    QMutexLocker locker(&m_mutex);
    std::vector<SystemMetrics> result;
    
    int startIdx = std::max(0, (int)m_history.size() - samples);
    result.assign(m_history.begin() + startIdx, m_history.end());
    
    return result;
}

DashboardMetricsProvider::MetricStats DashboardMetricsProvider::getMetricStats(const QString& metric) const
{
    QMutexLocker locker(&m_mutex);
    MetricStats stats;
    
    std::vector<double> values;
    
    // Extract metric values from history
    if (metric == "cpuUsage") {
        for (const auto& m : m_history) {
            values.push_back(m.cpuUsage);
        }
    } else if (metric == "memoryUsage") {
        for (const auto& m : m_history) {
            values.push_back(m.memoryUsage);
        }
    }
    
    if (values.empty()) {
        return stats;
    }
    
    // Calculate statistics
    stats.minimum = *std::min_element(values.begin(), values.end());
    stats.maximum = *std::max_element(values.begin(), values.end());
    
    double sum = 0.0;
    for (double v : values) {
        sum += v;
    }
    stats.average = sum / values.size();
    
    // Standard deviation
    double sumSquaredDiff = 0.0;
    for (double v : values) {
        double diff = v - stats.average;
        sumSquaredDiff += diff * diff;
    }
    stats.stdDev = std::sqrt(sumSquaredDiff / values.size());
    
    return stats;
}

void DashboardMetricsProvider::resetMetrics()
{
    QMutexLocker locker(&m_mutex);
    m_currentMetrics = SystemMetrics();
    m_history.clear();
    qInfo() << "[DashboardMetricsProvider] Metrics reset";
}

// ============================================================================
// AlertManagerWidget Implementation
// ============================================================================

AlertManagerWidget::AlertManagerWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Alerts table
    QTableWidget* alertTable = new QTableWidget();
    alertTable->setColumnCount(5);
    alertTable->setHorizontalHeaderLabels({"Time", "Alert", "Message", "Severity", "Actions"});
    alertTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    layout->addWidget(alertTable);
    
    // Control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* clearBtn = new QPushButton("Clear All");
    buttonLayout->addWidget(clearBtn);
    layout->addLayout(buttonLayout);
    
    setLayout(layout);
    
    qInfo() << "[AlertManagerWidget] Initialized";
}

void AlertManagerWidget::addAlert(const QString& alertName, const QString& message, int severity)
{
    QMutexLocker locker(&m_mutex);
    
    AlertItem item;
    item.id = QString::number(m_alerts.size());
    item.name = alertName;
    item.message = message;
    item.severity = severity;
    item.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    
    m_alerts.push_back(item);
    
    emit alertCountChanged(m_alerts.size());
    qInfo() << "[AlertManagerWidget] Alert added:" << alertName;
}

void AlertManagerWidget::removeAlert(const QString& alertId)
{
    QMutexLocker locker(&m_mutex);
    
    auto it = std::find_if(m_alerts.begin(), m_alerts.end(),
        [&alertId](const AlertItem& item) { return item.id == alertId; });
    
    if (it != m_alerts.end()) {
        m_alerts.erase(it);
        emit alertCountChanged(m_alerts.size());
    }
}

void AlertManagerWidget::clearAlerts()
{
    QMutexLocker locker(&m_mutex);
    m_alerts.clear();
    emit alertCountChanged(0);
    qInfo() << "[AlertManagerWidget] All alerts cleared";
}

int AlertManagerWidget::getActiveAlertCount() const
{
    QMutexLocker locker(&m_mutex);
    return m_alerts.size();
}

void AlertManagerWidget::acknowledgeAlert(const QString& alertId)
{
    QMutexLocker locker(&m_mutex);
    
    for (auto& item : m_alerts) {
        if (item.id == alertId) {
            item.acknowledged = true;
            emit alertAcknowledged(alertId);
            break;
        }
    }
}

void AlertManagerWidget::dismissAlert(const QString& alertId)
{
    removeAlert(alertId);
    emit alertDismissed(alertId);
}

QJsonArray AlertManagerWidget::getSuppressionRules() const
{
    QJsonArray rulesArray;
    // Would return suppression rules from alert manager
    return rulesArray;
}

void AlertManagerWidget::addSuppressionRule(const QString& alertPattern, int durationSeconds)
{
    qInfo() << "[AlertManagerWidget] Added suppression rule for" << alertPattern 
           << "duration:" << durationSeconds << "seconds";
}

void AlertManagerWidget::removeSuppressionRule(const QString& ruleId)
{
    qInfo() << "[AlertManagerWidget] Removed suppression rule:" << ruleId;
}

void AlertManagerWidget::updateUI()
{
    // Update UI with current alerts
    qInfo() << "[AlertManagerWidget] UI updated with" << m_alerts.size() << "alerts";
}

// ============================================================================
// MetricsVisualizationWidget Implementation
// ============================================================================

MetricsVisualizationWidget::MetricsVisualizationWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Placeholder for chart
    QLabel* chartLabel = new QLabel("Metrics Visualization Area");
    layout->addWidget(chartLabel);
    
    setLayout(layout);
    qInfo() << "[MetricsVisualizationWidget] Initialized";
}

void MetricsVisualizationWidget::setMetric(const QString& metricName, ChartType type)
{
    MetricData data;
    data.name = metricName;
    data.type = type;
    m_metrics.push_back(data);
    
    qInfo() << "[MetricsVisualizationWidget] Set metric:" << metricName;
}

void MetricsVisualizationWidget::updateMetricValue(const QString& metricName, double value)
{
    for (auto& metric : m_metrics) {
        if (metric.name == metricName) {
            metric.values.push_back(value);
            
            // Check thresholds
            if (value >= metric.criticalThreshold) {
                emit thresholdExceeded(metricName, value);
            }
            
            break;
        }
    }
    
    drawChart();
}

void MetricsVisualizationWidget::setThresholds(double warning, double critical)
{
    for (auto& metric : m_metrics) {
        metric.warningThreshold = warning;
        metric.criticalThreshold = critical;
    }
}

void MetricsVisualizationWidget::clearData()
{
    for (auto& metric : m_metrics) {
        metric.values.clear();
    }
}

void MetricsVisualizationWidget::drawChart()
{
    // Would draw actual chart visualization
    qInfo() << "[MetricsVisualizationWidget] Chart drawn";
}

// ============================================================================
// ResourceMonitorWidget Implementation
// ============================================================================

ResourceMonitorWidget::ResourceMonitorWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // CPU gauge
    QLabel* cpuLabel = new QLabel("CPU: 0%");
    layout->addWidget(cpuLabel);
    
    // Memory gauge
    QLabel* memLabel = new QLabel("Memory: 0%");
    layout->addWidget(memLabel);
    
    // Network gauge
    QLabel* netLabel = new QLabel("Network: 0 bytes/sec");
    layout->addWidget(netLabel);
    
    // Enforcement toggle
    QPushButton* enforceBtn = new QPushButton("Enable Enforcement");
    layout->addWidget(enforceBtn);
    
    setLayout(layout);
    qInfo() << "[ResourceMonitorWidget] Initialized";
}

void ResourceMonitorWidget::updateResources(double cpuUsage, double memoryUsage, uint64_t networkBytes)
{
    qInfo() << "[ResourceMonitorWidget] Updated: CPU=" << cpuUsage 
           << "% Memory=" << memoryUsage << "% Network=" << networkBytes << "bytes";
}

ResourceMonitorWidget::ResourceLimits ResourceMonitorWidget::getLimits() const
{
    return m_limits;
}

void ResourceMonitorWidget::setLimits(const ResourceLimits& limits)
{
    m_limits = limits;
    updateLimitIndicators();
}

bool ResourceMonitorWidget::isEnforcementEnabled() const
{
    return m_enforcementEnabled;
}

void ResourceMonitorWidget::setEnforcementEnabled(bool enabled)
{
    m_enforcementEnabled = enabled;
    emit enforcementStatusChanged(enabled);
    qInfo() << "[ResourceMonitorWidget] Enforcement" << (enabled ? "enabled" : "disabled");
}

void ResourceMonitorWidget::updateLimitIndicators()
{
    qInfo() << "[ResourceMonitorWidget] Updated limit indicators";
}

// ============================================================================
// FirewallRuleWidget Implementation
// ============================================================================

FirewallRuleWidget::FirewallRuleWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Rules table
    QTableWidget* ruleTable = new QTableWidget();
    ruleTable->setColumnCount(7);
    ruleTable->setHorizontalHeaderLabels({"ID", "Name", "Source", "Port", "Destination", "Port", "Action"});
    layout->addWidget(ruleTable);
    
    // Control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* addBtn = new QPushButton("Add Rule");
    QPushButton* deleteBtn = new QPushButton("Delete");
    buttonLayout->addWidget(addBtn);
    buttonLayout->addWidget(deleteBtn);
    layout->addLayout(buttonLayout);
    
    setLayout(layout);
    qInfo() << "[FirewallRuleWidget] Initialized";
}

void FirewallRuleWidget::addRule(const QString& ruleId, const QString& ruleName,
                                 const QString& srcIP, int srcPort, const QString& dstIP, int dstPort,
                                 const QString& action)
{
    Rule rule;
    rule.id = ruleId;
    rule.name = ruleName;
    rule.srcIP = srcIP;
    rule.srcPort = srcPort;
    rule.dstIP = dstIP;
    rule.dstPort = dstPort;
    rule.action = action;
    
    m_rules.push_back(rule);
    
    QJsonObject ruleJson;
    ruleJson["id"] = ruleId;
    ruleJson["name"] = ruleName;
    ruleJson["srcIP"] = srcIP;
    ruleJson["srcPort"] = srcPort;
    ruleJson["dstIP"] = dstIP;
    ruleJson["dstPort"] = dstPort;
    ruleJson["action"] = action;
    
    emit ruleAdded(ruleJson);
    
    qInfo() << "[FirewallRuleWidget] Added rule:" << ruleName;
}

void FirewallRuleWidget::removeRule(const QString& ruleId)
{
    auto it = std::find_if(m_rules.begin(), m_rules.end(),
        [&ruleId](const Rule& r) { return r.id == ruleId; });
    
    if (it != m_rules.end()) {
        m_rules.erase(it);
        emit ruleRemoved(ruleId);
    }
}

void FirewallRuleWidget::updateRule(const QString& ruleId, const QString& ruleName, bool enabled)
{
    for (auto& rule : m_rules) {
        if (rule.id == ruleId) {
            rule.name = ruleName;
            rule.enabled = enabled;
            
            QJsonObject ruleJson;
            ruleJson["id"] = ruleId;
            ruleJson["name"] = ruleName;
            ruleJson["enabled"] = enabled;
            
            emit ruleUpdated(ruleId, ruleJson);
            break;
        }
    }
}

QJsonArray FirewallRuleWidget::getAllRules() const
{
    QJsonArray rulesArray;
    
    for (const auto& rule : m_rules) {
        QJsonObject ruleJson;
        ruleJson["id"] = rule.id;
        ruleJson["name"] = rule.name;
        ruleJson["srcIP"] = rule.srcIP;
        ruleJson["srcPort"] = rule.srcPort;
        ruleJson["dstIP"] = rule.dstIP;
        ruleJson["dstPort"] = rule.dstPort;
        ruleJson["action"] = rule.action;
        ruleJson["enabled"] = rule.enabled;
        rulesArray.append(ruleJson);
    }
    
    return rulesArray;
}

void FirewallRuleWidget::createNewRule()
{
    qInfo() << "[FirewallRuleWidget] Create new rule dialog opened";
}

void FirewallRuleWidget::deleteSelectedRule()
{
    qInfo() << "[FirewallRuleWidget] Delete selected rule";
}

FirewallRuleWidget::BlockStats FirewallRuleWidget::getBlockStats() const
{
    return m_blockStats;
}

// ============================================================================
// ConversationSessionWidget Implementation
// ============================================================================

ConversationSessionWidget::ConversationSessionWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Sessions list
    QTableWidget* sessionTable = new QTableWidget();
    sessionTable->setColumnCount(4);
    sessionTable->setHorizontalHeaderLabels({"ID", "Title", "Messages", "Tokens"});
    layout->addWidget(sessionTable);
    
    // Chat area (placeholder)
    QLabel* chatLabel = new QLabel("Chat messages will appear here");
    layout->addWidget(chatLabel);
    
    // Control buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* newBtn = new QPushButton("New Session");
    QPushButton* deleteBtn = new QPushButton("Delete");
    buttonLayout->addWidget(newBtn);
    buttonLayout->addWidget(deleteBtn);
    layout->addLayout(buttonLayout);
    
    setLayout(layout);
    qInfo() << "[ConversationSessionWidget] Initialized";
}

void ConversationSessionWidget::addSession(const QString& sessionId, const QString& title, 
                                          int messageCount, int tokenCount)
{
    SessionInfo session;
    session.id = sessionId;
    session.title = title;
    session.messageCount = messageCount;
    session.tokenCount = tokenCount;
    
    m_sessions.push_back(session);
    
    qInfo() << "[ConversationSessionWidget] Added session:" << title;
}

void ConversationSessionWidget::removeSession(const QString& sessionId)
{
    auto it = std::find_if(m_sessions.begin(), m_sessions.end(),
        [&sessionId](const SessionInfo& s) { return s.id == sessionId; });
    
    if (it != m_sessions.end()) {
        m_sessions.erase(it);
        emit sessionDeleted(sessionId);
    }
}

void ConversationSessionWidget::updateSession(const QString& sessionId, int messageCount, int tokenCount)
{
    for (auto& session : m_sessions) {
        if (session.id == sessionId) {
            session.messageCount = messageCount;
            session.tokenCount = tokenCount;
            break;
        }
    }
}

void ConversationSessionWidget::loadSession(const QString& sessionId)
{
    m_currentSessionId = sessionId;
    emit sessionLoaded(sessionId);
}

void ConversationSessionWidget::displayMessage(const QString& role, const QString& content)
{
    qInfo() << "[ConversationSessionWidget] Displayed message from" << role;
}

ConversationSessionWidget::SessionStats ConversationSessionWidget::getStats() const
{
    SessionStats stats;
    stats.totalSessions = m_sessions.size();
    stats.activeSessions = 1;  // Current session
    
    for (const auto& session : m_sessions) {
        stats.totalTokensUsed += session.tokenCount;
        stats.totalMessages += session.messageCount;
    }
    
    return stats;
}

// ============================================================================
// SecurityAuditWidget Implementation
// ============================================================================

SecurityAuditWidget::SecurityAuditWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Risk gauge
    QLabel* riskLabel = new QLabel("Security Risk: 0.0/1.0");
    layout->addWidget(riskLabel);
    
    // Vulnerability list
    QTableWidget* vulnTable = new QTableWidget();
    vulnTable->setColumnCount(4);
    vulnTable->setHorizontalHeaderLabels({"Test", "Status", "Severity", "Message"});
    layout->addWidget(vulnTable);
    
    // Audit button
    QPushButton* auditBtn = new QPushButton("Run Security Audit");
    layout->addWidget(auditBtn);
    
    // Report button
    QPushButton* reportBtn = new QPushButton("Export Report");
    layout->addWidget(reportBtn);
    
    setLayout(layout);
    qInfo() << "[SecurityAuditWidget] Initialized";
}

void SecurityAuditWidget::displayAuditResults(const QJsonObject& results)
{
    m_lastAuditResults = results;
    m_riskScore = results["riskScore"].toDouble(0.0);
    
    // Parse vulnerability counts
    m_vulnCount.critical = results["criticalVulnerabilities"].toInt(0);
    m_vulnCount.high = results["highVulnerabilities"].toInt(0);
    
    emit riskScoreChanged(m_riskScore);
    
    qInfo() << "[SecurityAuditWidget] Displayed results with risk score:" << m_riskScore;
}

SecurityAuditWidget::VulnerabilityCount SecurityAuditWidget::getVulnerabilityCount() const
{
    return m_vulnCount;
}

QString SecurityAuditWidget::generateReport() const
{
    QString report = "SECURITY AUDIT REPORT\n";
    report += QString("Risk Score: %1/1.0\n").arg(m_riskScore);
    report += QString("Critical: %1, High: %2, Medium: %3, Low: %4\n")
        .arg(m_vulnCount.critical).arg(m_vulnCount.high)
        .arg(m_vulnCount.medium).arg(m_vulnCount.low);
    
    return report;
}

void SecurityAuditWidget::runSecurityAudit()
{
    emit auditStarted();
    qInfo() << "[SecurityAuditWidget] Security audit started";
}

// ============================================================================
// PerformanceGraphWidget Implementation
// ============================================================================

PerformanceGraphWidget::PerformanceGraphWidget(QWidget* parent)
    : QWidget(parent)
{
    QVBoxLayout* layout = new QVBoxLayout(this);
    
    // Performance summary
    QLabel* summaryLabel = new QLabel("Performance Metrics");
    layout->addWidget(summaryLabel);
    
    // Graph placeholder
    QLabel* graphLabel = new QLabel("Performance graph will appear here");
    layout->addWidget(graphLabel);
    
    setLayout(layout);
    qInfo() << "[PerformanceGraphWidget] Initialized";
}

void PerformanceGraphWidget::addSample(const QString& metric, double latencyMs, uint64_t throughput, double errorRate)
{
    PerformanceSample sample;
    sample.metric = metric;
    sample.latencyMs = latencyMs;
    sample.throughput = throughput;
    sample.errorRate = errorRate;
    sample.timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    
    m_samples.push_back(sample);
    
    // Keep only latest samples
    if ((int)m_samples.size() > MAX_SAMPLES) {
        m_samples.erase(m_samples.begin());
    }
    
    if (latencyMs > 1000.0) {  // > 1 second
        emit performanceThresholdExceeded(metric, latencyMs);
    }
}

PerformanceGraphWidget::PerformanceSummary PerformanceGraphWidget::getSummary() const
{
    PerformanceSummary summary;
    
    if (m_samples.empty()) {
        return summary;
    }
    
    std::vector<double> latencies;
    uint64_t totalThroughput = 0;
    double sumErrorRate = 0.0;
    
    for (const auto& sample : m_samples) {
        latencies.push_back(sample.latencyMs);
        totalThroughput += sample.throughput;
        sumErrorRate += sample.errorRate;
    }
    
    std::sort(latencies.begin(), latencies.end());
    
    summary.minLatencyMs = latencies.front();
    summary.maxLatencyMs = latencies.back();
    
    // Calculate average
    double sum = 0.0;
    for (double l : latencies) {
        sum += l;
    }
    summary.avgLatencyMs = sum / latencies.size();
    
    // Calculate percentiles
    int p95Idx = (latencies.size() * 95) / 100;
    int p99Idx = (latencies.size() * 99) / 100;
    summary.p95LatencyMs = latencies[p95Idx];
    summary.p99LatencyMs = latencies[p99Idx];
    
    summary.totalThroughput = totalThroughput;
    summary.avgErrorRate = sumErrorRate / m_samples.size();
    
    return summary;
}

void PerformanceGraphWidget::clearData()
{
    m_samples.clear();
}

// ============================================================================
// EnhancedDashboard Implementation
// ============================================================================

EnhancedDashboard::EnhancedDashboard(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    qInfo() << "[EnhancedDashboard] Initialized";
}

EnhancedDashboard::~EnhancedDashboard()
{
}

void EnhancedDashboard::initialize()
{
    m_metricsProvider = std::make_unique<DashboardMetricsProvider>();
    m_alertWidget = std::make_unique<AlertManagerWidget>();
    m_metricsWidget = std::make_unique<MetricsVisualizationWidget>();
    m_resourceWidget = std::make_unique<ResourceMonitorWidget>();
    m_firewallWidget = std::make_unique<FirewallRuleWidget>();
    m_conversationWidget = std::make_unique<ConversationSessionWidget>();
    m_securityWidget = std::make_unique<SecurityAuditWidget>();
    m_performanceWidget = std::make_unique<PerformanceGraphWidget>();
    
    m_updateTimer = std::make_unique<QTimer>();
    connect(m_updateTimer.get(), &QTimer::timeout, this, &EnhancedDashboard::onUpdateTimer);
    m_updateTimer->start(1000);  // Update every second
    
    setupConnections();
    
    emit dashboardInitialized();
    qInfo() << "[EnhancedDashboard] Fully initialized";
}

QJsonObject EnhancedDashboard::exportState() const
{
    QJsonObject state;
    state["metrics"] = m_metricsProvider->getMetrics().cpuUsage;
    state["alerts"] = m_alertWidget->getActiveAlertCount();
    state["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    return state;
}

bool EnhancedDashboard::importState(const QJsonObject& state)
{
    qInfo() << "[EnhancedDashboard] Imported state";
    return true;
}

void EnhancedDashboard::saveLayout(const QString& filename)
{
    QJsonObject layout = exportState();
    QJsonDocument doc(layout);
    
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        qInfo() << "[EnhancedDashboard] Layout saved to:" << filename;
    }
}

bool EnhancedDashboard::loadLayout(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isObject()) {
        return importState(doc.object());
    }
    
    return false;
}

void EnhancedDashboard::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // Tab widget for different views
    QTabWidget* tabWidget = new QTabWidget();
    
    // Placeholder tabs
    QWidget* overviewTab = new QWidget();
    QWidget* alertsTab = new QWidget();
    QWidget* metricsTab = new QWidget();
    QWidget* resourcesTab = new QWidget();
    QWidget* firewallTab = new QWidget();
    QWidget* securityTab = new QWidget();
    
    tabWidget->addTab(overviewTab, "Overview");
    tabWidget->addTab(alertsTab, "Alerts");
    tabWidget->addTab(metricsTab, "Metrics");
    tabWidget->addTab(resourcesTab, "Resources");
    tabWidget->addTab(firewallTab, "Firewall");
    tabWidget->addTab(securityTab, "Security");
    
    mainLayout->addWidget(tabWidget);
    setLayout(mainLayout);
    
    qInfo() << "[EnhancedDashboard] UI setup complete";
}

void EnhancedDashboard::setupConnections()
{
    // Connect signals from widgets
    if (m_alertWidget) {
        connect(m_alertWidget.get(), &AlertManagerWidget::alertCountChanged,
                this, [this](int count) {
                    calculateSystemHealth();
                });
    }
    
    qInfo() << "[EnhancedDashboard] Connections setup complete";
}

void EnhancedDashboard::onUpdateTimer()
{
    calculateSystemHealth();
}

void EnhancedDashboard::calculateSystemHealth()
{
    if (!m_metricsProvider) return;
    
    auto metrics = m_metricsProvider->getMetrics();
    
    // Calculate health based on metrics (0.0 to 1.0)
    m_systemHealth = 1.0;
    m_systemHealth -= (metrics.cpuUsage / 100.0) * 0.3;       // CPU impact
    m_systemHealth -= (metrics.memoryUsage / 100.0) * 0.3;    // Memory impact
    m_systemHealth -= std::min(1.0, metrics.metricsDropRate / 100.0) * 0.2;  // Metric drop impact
    m_systemHealth -= std::min(1.0, (double)metrics.criticalAlerts / 10.0) * 0.2;  // Alert impact
    
    m_systemHealth = std::max(0.0, m_systemHealth);
    
    emit systemHealthChanged(m_systemHealth);
}
