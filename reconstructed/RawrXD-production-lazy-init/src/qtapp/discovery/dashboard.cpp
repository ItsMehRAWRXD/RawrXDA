#include "discovery_dashboard.h"
#include <QApplication>
#include <QScreen>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTimer>
#include <QJsonObject>
#include <QJsonArray>
#include <QTabWidget>
#include <QListWidget>
#include <QListWidgetItem>
#include <QGroupBox>
#include <QFrame>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QDateTime>

DiscoveryDashboard::DiscoveryDashboard(QWidget* parent)
    : QDockWidget(parent)
    , m_mainTabs(new QTabWidget(this))
    , m_refreshTimer(new QTimer(this))
    , m_refreshButton(new QPushButton("Refresh", this))
    , m_exportButton(new QPushButton("Export", this))
{
    // Set dock widget properties
    setWindowTitle("Autonomous Capabilities Dashboard");
    setObjectName("DiscoveryDashboard");
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
    
    // Initialize dashboard content
    setupUI();
    
    // Initialize capability status
    m_capabilityStatus["Advanced Planning"] = false;
    m_capabilityStatus["Error Analysis"] = false;
    m_capabilityStatus["Real-time Refactoring"] = false;
    m_capabilityStatus["Memory Persistence"] = false;
    m_capabilityStatus["Metrics Dashboard"] = false;
    m_capabilityStatus["Test Generation"] = false;
    m_capabilityStatus["Alert System"] = false;
    m_capabilityStatus["AI Chat Integration"] = true;
    m_capabilityStatus["Model Management"] = true;
    m_capabilityStatus["Terminal Integration"] = true;
    
    // Initialize metrics
    m_performanceMetrics["active_tasks"] = 0;
    m_performanceMetrics["completed_tasks"] = 15;
    m_performanceMetrics["success_rate"] = 0.92;
    m_performanceMetrics["average_response_time"] = 1.2;
    m_performanceMetrics["memory_usage_mb"] = 512;
    m_performanceMetrics["cpu_usage_percent"] = 25;
    
    // Connect signals
    connect(m_refreshButton, &QPushButton::clicked, this, &DiscoveryDashboard::onRefreshRequested);
    connect(m_exportButton, &QPushButton::clicked, this, &DiscoveryDashboard::onExportMetrics);
    connect(m_refreshTimer, &QTimer::timeout, this, &DiscoveryDashboard::onAutoRefresh);
    
    // Start auto-refresh
    m_refreshTimer->start(5000); // 5 seconds
    updateAllDisplays();
}

DiscoveryDashboard::~DiscoveryDashboard() = default;

void DiscoveryDashboard::setupUI() {
    // Set window title and properties
    setWindowTitle("Autonomous Capabilities Dashboard");
    setMinimumSize(600, 400);
    
    // Create central widget for dock widget
    QWidget* centralWidget = new QWidget(this);
    setWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);
    
    // Header
    QHBoxLayout* headerLayout = new QHBoxLayout();
    
    QLabel* titleLabel = new QLabel("🤖 Autonomous Capabilities Dashboard", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(16);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    titleLabel->setStyleSheet("color: #ffffff; margin-bottom: 10px;");
    
    m_refreshButton->setStyleSheet("QPushButton { background-color: #007acc; color: white; padding: 8px 16px; border-radius: 4px; }"
                                   "QPushButton:hover { background-color: #005a9e; }");
    m_exportButton->setStyleSheet("QPushButton { background-color: #4ec9b0; color: white; padding: 8px 16px; border-radius: 4px; }"
                                  "QPushButton:hover { background-color: #3aa898; }");
    
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(m_refreshButton);
    headerLayout->addWidget(m_exportButton);
    
    mainLayout->addLayout(headerLayout);
    
    // Main tabs
    mainLayout->addWidget(m_mainTabs);
    
    // Setup individual tabs
    setupCapabilityGrid();
    setupMetricsPanel();
    setupActivityFeed();
    setupSystemHealth();
    
    // Style
    setStyleSheet("QWidget { background-color: #1e1e1e; color: #e0e0e0; }"
                  "QTabWidget::pane { border: 1px solid #3a3a3a; background-color: #2d2d30; }"
                  "QTabBar::tab { background-color: #2d2d30; color: #e0e0e0; padding: 8px 16px; margin-right: 2px; }"
                  "QTabBar::tab:selected { background-color: #007acc; }"
                  "QTabBar::tab:hover { background-color: #3a3a3a; }");
}

void DiscoveryDashboard::setupCapabilityGrid() {
    m_capabilitiesTab = new QWidget();
    QVBoxLayout* tabLayout = new QVBoxLayout(m_capabilitiesTab);
    
    // Capability grid
    m_capabilityGrid = new QGridLayout();
    m_capabilityGrid->setSpacing(10);
    
    // Create capability cards
    QStringList capabilities = {
        "Advanced Planning",
        "Error Analysis", 
        "Real-time Refactoring",
        "Memory Persistence",
        "Metrics Dashboard",
        "Test Generation",
        "Alert System",
        "AI Chat Integration",
        "Model Management",
        "Terminal Integration"
    };
    
    QStringList descriptions = {
        "Recursive task decomposition and intelligent planning",
        "AI-powered error diagnosis and fix generation",
        "Automatic code improvement and refactoring",
        "Context persistence and retrieval system",
        "Performance monitoring and analytics",
        "Automated test case generation",
        "Proactive issue detection and notifications",
        "Integrated AI assistance and chat",
        "GGUF model loading and management",
        "Integrated terminal with AI suggestions"
    };
    
    for (int i = 0; i < capabilities.size(); ++i) {
        QFrame* card = createCapabilityCard(capabilities[i], descriptions[i]);
        m_capabilityCards[capabilities[i]] = card;
        
        int row = i / 3;
        int col = i % 3;
        m_capabilityGrid->addWidget(card, row, col);
    }
    
    tabLayout->addLayout(m_capabilityGrid);
    tabLayout->addStretch();
    
    m_mainTabs->addTab(m_capabilitiesTab, "Capabilities");
}

void DiscoveryDashboard::setupMetricsPanel() {
    m_metricsTab = new QWidget();
    QVBoxLayout* tabLayout = new QVBoxLayout(m_metricsTab);
    
    // Overall health
    QGroupBox* healthGroup = new QGroupBox("System Health", m_metricsTab);
    QVBoxLayout* healthLayout = new QVBoxLayout(healthGroup);
    
    m_overallHealthBar = new QProgressBar(healthGroup);
    m_overallHealthBar->setRange(0, 100);
    m_overallHealthBar->setValue(85);
    m_overallHealthBar->setStyleSheet("QProgressBar { border: 1px solid #3a3a3a; text-align: center; }"
                                      "QProgressBar::chunk { background-color: #4ec9b0; }");
    
    QLabel* healthLabel = new QLabel("Overall System Health", healthGroup);
    healthLayout->addWidget(healthLabel);
    healthLayout->addWidget(m_overallHealthBar);
    
    // Task metrics
    QGroupBox* taskGroup = new QGroupBox("Task Metrics", m_metricsTab);
    QVBoxLayout* taskLayout = new QVBoxLayout(taskGroup);
    
    m_activeTasksLabel = new QLabel("Active Tasks: 3", taskGroup);
    m_completedTasksLabel = new QLabel("Completed Tasks: 15", taskGroup);
    m_successRateLabel = new QLabel("Success Rate: 92%", taskGroup);
    
    taskLayout->addWidget(m_activeTasksLabel);
    taskLayout->addWidget(m_completedTasksLabel);
    taskLayout->addWidget(m_successRateLabel);
    
    // Performance metrics
    QGroupBox* perfGroup = new QGroupBox("Performance", m_metricsTab);
    QVBoxLayout* perfLayout = new QVBoxLayout(perfGroup);
    
    QLabel* responseTimeLabel = new QLabel("Avg Response Time: 1.2s", perfGroup);
    QLabel* memoryLabel = new QLabel("Memory Usage: 512 MB", perfGroup);
    QLabel* cpuLabel = new QLabel("CPU Usage: 25%", perfGroup);
    
    perfLayout->addWidget(responseTimeLabel);
    perfLayout->addWidget(memoryLabel);
    perfLayout->addWidget(cpuLabel);
    
    tabLayout->addWidget(healthGroup);
    tabLayout->addWidget(taskGroup);
    tabLayout->addWidget(perfGroup);
    tabLayout->addStretch();
    
    m_mainTabs->addTab(m_metricsTab, "Metrics");
}

void DiscoveryDashboard::setupActivityFeed() {
    m_activityTab = new QWidget();
    QVBoxLayout* tabLayout = new QVBoxLayout(m_activityTab);
    
    // Activity list
    m_activityList = new QListWidget(m_activityTab);
    m_activityList->setStyleSheet("QListWidget { background-color: #2d2d30; border: none; }"
                                  "QListWidget::item { padding: 8px; border-bottom: 1px solid #3a3a3a; }"
                                  "QListWidget::item:selected { background-color: #094771; }");
    
    // Add sample activities
    addActivityItem("planning", "Advanced planning engine initialized", QDateTime::currentDateTime().toString());
    addActivityItem("analysis", "Error analysis module started", QDateTime::currentDateTime().addSecs(-30).toString());
    addActivityItem("refactoring", "Real-time refactoring active", QDateTime::currentDateTime().addSecs(-60).toString());
    addActivityItem("alert", "System health check completed", QDateTime::currentDateTime().addSecs(-90).toString());
    
    tabLayout->addWidget(m_activityList);
    
    m_mainTabs->addTab(m_activityTab, "Activity");
}

void DiscoveryDashboard::setupSystemHealth() {
    m_systemTab = new QWidget();
    QVBoxLayout* tabLayout = new QVBoxLayout(m_systemTab);
    
    QGroupBox* statusGroup = new QGroupBox("Agent Status", m_systemTab);
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);
    
    QLabel* planningStatus = new QLabel("🧠 Planning Engine: Active", statusGroup);
    QLabel* analysisStatus = new QLabel("🔍 Error Analysis: Active", statusGroup);
    QLabel* refactorStatus = new QLabel("⚡ Refactoring: Ready", statusGroup);
    QLabel* memoryStatus = new QLabel("💾 Memory System: Connected", statusGroup);
    
    planningStatus->setStyleSheet("color: #4ec9b0;");
    analysisStatus->setStyleSheet("color: #4ec9b0;");
    refactorStatus->setStyleSheet("color: #4ec9b0;");
    memoryStatus->setStyleSheet("color: #4ec9b0;");
    
    statusLayout->addWidget(planningStatus);
    statusLayout->addWidget(analysisStatus);
    statusLayout->addWidget(refactorStatus);
    statusLayout->addWidget(memoryStatus);
    
    tabLayout->addWidget(statusGroup);
    tabLayout->addStretch();
    
    m_mainTabs->addTab(m_systemTab, "System");
}

QFrame* DiscoveryDashboard::createCapabilityCard(const QString& name, const QString& description) {
    QFrame* card = new QFrame();
    card->setFrameStyle(QFrame::Box);
    card->setMinimumSize(200, 120);
    
    QVBoxLayout* layout = new QVBoxLayout(card);
    layout->setContentsMargins(10, 10, 10, 10);
    
    // Name and status
    QHBoxLayout* headerLayout = new QHBoxLayout();
    QLabel* nameLabel = new QLabel(name, card);
    QFont nameFont = nameLabel->font();
    nameFont.setBold(true);
    nameLabel->setFont(nameFont);
    
    QLabel* statusLabel = new QLabel("●", card);
    statusLabel->setStyleSheet("color: #666; font-size: 16px;");
    
    headerLayout->addWidget(nameLabel);
    headerLayout->addStretch();
    headerLayout->addWidget(statusLabel);
    
    // Description
    QLabel* descLabel = new QLabel(description, card);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #ccc; font-size: 11px;");
    
    // Action button
    QPushButton* actionButton = new QPushButton("Activate", card);
    actionButton->setMaximumWidth(80);
    actionButton->setStyleSheet("QPushButton { background-color: #666; color: white; border: none; padding: 4px 8px; border-radius: 3px; }"
                               "QPushButton:hover { background-color: #777; }"
                               "QPushButton:checked { background-color: #007acc; }");
    
    connect(actionButton, &QPushButton::clicked, this, [this, name, statusLabel, actionButton]() {
        bool current = m_capabilityStatus[name];
        m_capabilityStatus[name] = !current;
        updateCapabilityCard(name, m_capabilityStatus[name], QString());
        actionButton->setText(m_capabilityStatus[name] ? "Deactivate" : "Activate");
        emit capabilitySelected(name);
    });
    
    layout->addLayout(headerLayout);
    layout->addWidget(descLabel);
    layout->addWidget(actionButton);
    
    // Store reference for updates
    card->setProperty("name", name);
    card->setProperty("statusLabel", QVariant::fromValue(statusLabel));
    card->setProperty("actionButton", QVariant::fromValue(actionButton));
    
    return card;
}

void DiscoveryDashboard::updateCapabilityCard(const QString& name, bool active, const QString& status) {
    if (!m_capabilityCards.contains(name)) return;
    
    QFrame* card = m_capabilityCards[name];
    QLabel* statusLabel = card->property("statusLabel").value<QLabel*>();
    QPushButton* actionButton = card->property("actionButton").value<QPushButton*>();
    
    if (statusLabel) {
        statusLabel->setText("●");
        statusLabel->setStyleSheet(QString("color: %1; font-size: 16px;").arg(active ? "#4ec9b0" : "#666"));
    }
    
    if (actionButton) {
        actionButton->setText(active ? "Deactivate" : "Activate");
        actionButton->setChecked(active);
    }
}

void DiscoveryDashboard::updateCapabilityCards() {
    for (auto it = m_capabilityStatus.begin(); it != m_capabilityStatus.end(); ++it) {
        updateCapabilityCard(it.key(), it.value(), QString());
    }
}

void DiscoveryDashboard::updateMetricsDisplay() {
    // Update health bar
    int healthScore = int(m_performanceMetrics["success_rate"].toDouble() * 100);
    m_overallHealthBar->setValue(healthScore);
    
    // Update task metrics
    m_activeTasksLabel->setText(QString("Active Tasks: %1").arg(m_performanceMetrics["active_tasks"].toInt()));
    m_completedTasksLabel->setText(QString("Completed Tasks: %1").arg(m_performanceMetrics["completed_tasks"].toInt()));
    m_successRateLabel->setText(QString("Success Rate: %1%").arg(int(m_performanceMetrics["success_rate"].toDouble() * 100)));
}

void DiscoveryDashboard::updateActivityDisplay() {
    // Activity list is updated via addActivityItem
}

void DiscoveryDashboard::updateSystemHealthDisplay() {
    // System health is static for now, could be made dynamic
}

void DiscoveryDashboard::updateAllDisplays() {
    updateCapabilityCards();
    updateMetricsDisplay();
    updateActivityDisplay();
    updateSystemHealthDisplay();
}

void DiscoveryDashboard::refreshAll() {
    qDebug() << "[DiscoveryDashboard] Refreshing all displays";
    updateAllDisplays();
    emit dashboardRefreshed();
}

void DiscoveryDashboard::updateCapabilityStatus(const QString& capability, bool active, const QString& status) {
    m_capabilityStatus[capability] = active;
    updateCapabilityCard(capability, active, status);
}

void DiscoveryDashboard::updatePerformanceMetrics(const QJsonObject& metrics) {
    m_performanceMetrics = metrics;
    updateMetricsDisplay();
}

void DiscoveryDashboard::addExecutionEvent(const QString& eventType, const QString& description) {
    QString timestamp = QDateTime::currentDateTime().toString();
    addActivityItem(eventType, description, timestamp);
}

void DiscoveryDashboard::addActivityItem(const QString& type, const QString& message, const QString& timestamp) {
    QString icon;
    if (type == "planning") icon = "🧠";
    else if (type == "analysis") icon = "🔍";
    else if (type == "refactoring") icon = "⚡";
    else if (type == "alert") icon = "🚨";
    else if (type == "memory") icon = "💾";
    else icon = "ℹ️";
    
    QString displayText = QString("%1 %2").arg(icon).arg(message);
    QListWidgetItem* item = new QListWidgetItem(displayText, m_activityList);
    item->setData(Qt::UserRole, type);
    item->setData(Qt::UserRole + 1, timestamp);
    
    // Add to activity events
    QJsonObject event;
    event["type"] = type;
    event["message"] = message;
    event["timestamp"] = timestamp;
    m_activityEvents.append(event);
    
    // Keep only last 50 items
    while (m_activityList->count() > 50) {
        delete m_activityList->takeItem(0);
    }
}

void DiscoveryDashboard::setRefreshInterval(int milliseconds) {
    m_refreshTimer->setInterval(milliseconds);
}

void DiscoveryDashboard::enableAutoRefresh(bool enable) {
    if (enable) {
        m_refreshTimer->start();
    } else {
        m_refreshTimer->stop();
    }
}

void DiscoveryDashboard::setCapabilitiesToMonitor(const QStringList& capabilities) {
    m_monitoredCapabilities = capabilities;
}

void DiscoveryDashboard::onCapabilityClicked(const QString& capability) {
    qDebug() << "[DiscoveryDashboard] Capability clicked:" << capability;
    emit capabilitySelected(capability);
}

void DiscoveryDashboard::onRefreshRequested()
{
    refreshAll();
}

void DiscoveryDashboard::onAutoRefresh() {
    updateMetricsDisplay();
}

void DiscoveryDashboard::onExportMetrics() {
    qDebug() << "[DiscoveryDashboard] Exporting metrics";
    
    QJsonObject exportData;
    exportData["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Convert QMap<QString, bool> to QVariantMap for JSON serialization
    QVariantMap capabilityStatusMap;
    for (auto it = m_capabilityStatus.begin(); it != m_capabilityStatus.end(); ++it) {
        capabilityStatusMap[it.key()] = it.value();
    }
    exportData["capabilities"] = QJsonObject::fromVariantMap(capabilityStatusMap);
    exportData["performance_metrics"] = m_performanceMetrics;
    exportData["activity_events"] = m_activityEvents;
    
    QString fileName = QString("dashboard_export_%1.json")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    
    emit metricsExported(fileName);
}

void DiscoveryDashboard::initialize() {
    refreshAll();
}

void DiscoveryDashboard::refresh() {
    refreshAll();
}
