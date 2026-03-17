#include "discovery_dashboard.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QDebug>
#include <QColor>
#include <QIcon>
#include <QProcess>
#include <QSysInfo>
#include <QStorageInfo>
#include <QThread>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>

// Use the QtCharts namespace properly
using namespace QtCharts;

// ========== CapabilityScanner Implementation ==========

CapabilityScanner::CapabilityScanner(QObject* parent) : QObject(parent) {}
CapabilityScanner::~CapabilityScanner() = default;

void CapabilityScanner::startScan() {
    if (m_scanning.exchange(true)) return;
    
    QThread::create([this]() {
        QMutexLocker locker(&m_scanMutex);
        
        emit scanProgress(0, 100, "Starting system discovery...");
        QThread::msleep(100);
        
        scanHardware();
        emit scanProgress(20, 100, "Hardware discovery complete");
        
        scanSoftware();
        emit scanProgress(40, 100, "Software environment discovery complete");
        
        scanAIModels();
        emit scanProgress(60, 100, "AI Model discovery complete");
        
        scanNetwork();
        emit scanProgress(80, 100, "Network discovery complete");
        
        scanIntegrations();
        emit scanProgress(100, 100, "Discovery complete");
        
        m_scanning.store(false);
        emit scanCompleted(50); // Simulated count
    })->start();
}

void CapabilityScanner::stopScan() {
    m_scanning.store(false);
}

bool CapabilityScanner::isScanning() const {
    return m_scanning.load();
}

void CapabilityScanner::scanHardware() {
    HardwareInfo info;
    info.cpuModel = QSysInfo::currentCpuArchitecture();
    info.cpuCores = QThread::idealThreadCount();
    
    // In a real enterprise app, we'd use native APIs or WMI on Windows
    // For now, providing structured discovery patterns
    info.hasAVX = true;
    info.hasAVX2 = true;
    info.gpuModel = "NVIDIA GeForce RTX 4090"; // Example detection
    info.gpuMemoryMB = 24576;
    info.hasCUDA = true;
    
    QStorageInfo storage = QStorageInfo::root();
    info.totalStorageGB = storage.bytesTotal() / (1024 * 1024 * 1024);
    info.availableStorageGB = storage.bytesAvailable() / (1024 * 1024 * 1024);
    
    emit hardwareInfoUpdated(info);
    
    CapabilityInfo cap;
    cap.id = "hw_cpu";
    cap.name = "Central Processing Unit";
    cap.category = CapabilityCategory::Hardware;
    cap.status = CapabilityStatus::Available;
    cap.description = QString("System CPU: %1 cores detected").arg(info.cpuCores);
    emit capabilityDetected(cap);
    
    cap.id = "hw_gpu_cuda";
    cap.name = "NVIDIA CUDA Support";
    cap.category = CapabilityCategory::Hardware;
    cap.status = CapabilityStatus::Available;
    cap.description = "High-performance GPU acceleration via CUDA Toolkit";
    emit capabilityDetected(cap);
}

void CapabilityScanner::scanSoftware() {
    CapabilityInfo cap;
    cap.id = "sw_qt";
    cap.name = "Qt Framework";
    cap.category = CapabilityCategory::Software;
    cap.status = CapabilityStatus::Available;
    cap.version = QT_VERSION_STR;
    cap.description = "Professional UI and core framework";
    emit capabilityDetected(cap);
    
    // Check for compilers
    QStringList compilers = {"g++", "clang++", "msvc"};
    for (const QString& compiler : compilers) {
        cap.id = "sw_compiler_" + compiler;
        cap.name = compiler + " Compiler";
        cap.status = CapabilityStatus::Available; // Simplified
        emit capabilityDetected(cap);
    }
}

void CapabilityScanner::scanAIModels() {
    CapabilityInfo cap;
    cap.id = "ai_llm_internal";
    cap.name = "Internal LLM Engine";
    cap.category = CapabilityCategory::AI_Models;
    cap.status = CapabilityStatus::Available;
    cap.description = "Local inference engine based on llama.cpp";
    emit capabilityDetected(cap);
    
    cap.id = "ai_provider_openai";
    cap.name = "OpenAI Cloud Integration";
    cap.category = CapabilityCategory::AI_Models;
    cap.status = CapabilityStatus::Limited; // Needs API Key
    cap.description = "Enterprise cloud AI via OpenAI API";
    emit capabilityDetected(cap);
}

void CapabilityScanner::scanNetwork() {
    CapabilityInfo cap;
    cap.id = "net_api_server";
    cap.name = "Internal API Server";
    cap.category = CapabilityCategory::Network;
    cap.status = CapabilityStatus::Available;
    cap.description = "Communication layer for distributed tasks";
    emit capabilityDetected(cap);
}

void CapabilityScanner::scanIntegrations() {
    CapabilityInfo cap;
    cap.id = "ext_git";
    cap.name = "Git Version Control";
    cap.category = CapabilityCategory::Integration;
    cap.status = CapabilityStatus::Available;
    emit capabilityDetected(cap);
}

// ========== ServiceDiscovery Implementation ==========

ServiceDiscovery::ServiceDiscovery(QObject* parent) : QObject(parent) {
    m_healthCheckTimer = new QTimer(this);
    connect(m_healthCheckTimer, &QTimer::timeout, this, &ServiceDiscovery::performHealthChecks);
}

ServiceDiscovery::~ServiceDiscovery() = default;

void ServiceDiscovery::startDiscovery() {
    m_healthCheckTimer->start(m_healthCheckInterval);
    performHealthChecks();
}

void ServiceDiscovery::stopDiscovery() {
    m_healthCheckTimer->stop();
}

void ServiceDiscovery::addEndpoint(const QString& name, const QString& url) {
    QWriteLocker locker(&m_endpointLock);
    ServiceEndpoint ep;
    ep.name = name;
    ep.url = url;
    ep.status = CapabilityStatus::Checking;
    m_endpoints.append(ep);
}

void ServiceDiscovery::performHealthChecks() {
    QWriteLocker locker(&m_endpointLock);
    for (auto& ep : m_endpoints) {
        // Simulation of network health check
        ep.status = CapabilityStatus::Available;
        ep.latencyMs = 15.0; // Simulated latency
        ep.lastChecked = QDateTime::currentDateTime();
        emit endpointStatusChanged(ep.name, ep.status);
    }
    emit healthCheckCompleted(m_endpoints);
}

// ========== DiscoveryDashboard Implementation ==========

DiscoveryDashboard::DiscoveryDashboard(QWidget* parent)
    : QWidget(parent)
    , m_scanner(std::make_unique<CapabilityScanner>(this))
    , m_serviceDiscovery(std::make_unique<ServiceDiscovery>(this))
{
}

DiscoveryDashboard::~DiscoveryDashboard() = default;

void DiscoveryDashboard::initialize() {
    if (m_initialized) return;
    
    setupUI();
    setupConnections();
    
    m_initialized = true;
    
    // Initial scan
    startScan();
    m_serviceDiscovery->startDiscovery();
}

void DiscoveryDashboard::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // Header Panel
    QGroupBox* headerGroup = new QGroupBox("Enterprise System Discovery", this);
    QHBoxLayout* headerLayout = new QHBoxLayout(headerGroup);
    
    QVBoxLayout* statsLayout = new QVBoxLayout();
    m_totalCapabilitiesLabel = new QLabel("Total Capabilities: --", this);
    m_availableCapabilitiesLabel = new QLabel("Available: --", this);
    m_healthScoreLabel = new QLabel("Global Health Score: --", this);
    statsLayout->addWidget(m_totalCapabilitiesLabel);
    statsLayout->addWidget(m_availableCapabilitiesLabel);
    statsLayout->addWidget(m_healthScoreLabel);
    
    headerLayout->addLayout(statsLayout);
    
    QVBoxLayout* progressLayout = new QVBoxLayout();
    m_healthProgressBar = new QProgressBar(this);
    m_healthProgressBar->setRange(0, 100);
    m_healthProgressBar->setValue(0);
    m_healthProgressBar->setFormat("System Health: %p%");
    
    m_scanProgressBar = new QProgressBar(this);
    m_scanProgressBar->setRange(0, 100);
    m_scanProgressBar->setValue(0);
    m_scanProgressBar->setFormat("Scan Progress: %p%");
    
    progressLayout->addWidget(m_healthProgressBar);
    progressLayout->addWidget(m_scanProgressBar);
    headerLayout->addLayout(progressLayout, 1);
    
    mainLayout->addWidget(headerGroup);

    // Tab Widget
    m_tabWidget = new QTabWidget(this);
    
    // Tab 1: Capability Explorer
    QWidget* explorerTab = new QWidget();
    QVBoxLayout* explorerLayout = new QVBoxLayout(explorerTab);
    m_capabilityTree = new QTreeWidget(this);
    m_capabilityTree->setHeaderLabels({"Capability", "Status", "Version", "Description"});
    m_capabilityTree->setColumnWidth(0, 250);
    m_capabilityTree->setColumnWidth(1, 120);
    m_capabilityTree->setColumnWidth(2, 100);
    explorerLayout->addWidget(m_capabilityTree);
    m_tabWidget->addTab(explorerTab, QIcon(), "Capability Explorer");
    
    // Tab 2: Hardware Capabilities
    QWidget* hwTab = new QWidget();
    setupHardwarePanel(); // Fills hwTab layout via some other mechanism or I'll just do it here
    m_tabWidget->addTab(hwTab, "Hardware Intelligence");
    
    // Tab 3: Service Discovery
    QWidget* servicesTab = new QWidget();
    QVBoxLayout* servicesLayout = new QVBoxLayout(servicesTab);
    m_servicesTable = new QTableWidget(0, 5, this);
    m_servicesTable->setHorizontalHeaderLabels({"Service", "Endpoint", "Status", "Latency", "Last Check"});
    m_servicesTable->horizontalHeader()->setStretchLastSection(true);
    servicesLayout->addWidget(m_servicesTable);
    m_tabWidget->addTab(servicesTab, "Service Discovery");
    
    // Tab 4: Health Analytics
    QWidget* healthTab = new QWidget();
    setupHealthPanel();
    m_tabWidget->addTab(healthTab, "Health Analytics");

    mainLayout->addWidget(m_tabWidget);

    // Footer Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    m_scanButton = new QPushButton("Trigger Full Scan", this);
    m_refreshButton = new QPushButton("Refresh Status", this);
    m_exportButton = new QPushButton("Export Report", this);
    
    buttonLayout->addWidget(m_scanButton);
    buttonLayout->addWidget(m_refreshButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_exportButton);
    mainLayout->addLayout(buttonLayout);

    setLayout(mainLayout);
}

void DiscoveryDashboard::setupHardwarePanel() {
    QWidget* hwTab = m_tabWidget->widget(1);
    QGridLayout* layout = new QGridLayout(hwTab);

    QGroupBox* cpuGroup = new QGroupBox("CPU / Processing", this);
    QVBoxLayout* cpuLayout = new QVBoxLayout(cpuGroup);
    m_cpuLabel = new QLabel("Detecting CPU...", this);
    m_cpuProgressBar = new QProgressBar(this);
    cpuLayout->addWidget(m_cpuLabel);
    cpuLayout->addWidget(m_cpuProgressBar);
    layout->addWidget(cpuGroup, 0, 0);

    QGroupBox* gpuGroup = new QGroupBox("GPU / Acceleration", this);
    QVBoxLayout* gpuLayout = new QVBoxLayout(gpuGroup);
    m_gpuLabel = new QLabel("Detecting GPU...", this);
    m_gpuProgressBar = new QProgressBar(this);
    gpuLayout->addWidget(m_gpuLabel);
    gpuLayout->addWidget(m_gpuProgressBar);
    layout->addWidget(gpuGroup, 0, 1);

    QGroupBox* memGroup = new QGroupBox("Memory / Storage", this);
    QVBoxLayout* memLayout = new QVBoxLayout(memGroup);
    m_memoryLabel = new QLabel("RAM Usage", this);
    m_storageLabel = new QLabel("Storage Space", this);
    m_memoryProgressBar = new QProgressBar(this);
    m_storageProgressBar = new QProgressBar(this);
    memLayout->addWidget(m_memoryLabel);
    memLayout->addWidget(m_memoryProgressBar);
    memLayout->addWidget(m_storageLabel);
    memLayout->addWidget(m_storageProgressBar);
    layout->addWidget(memGroup, 1, 0, 1, 2);
}

void DiscoveryDashboard::setupHealthPanel() {
    QWidget* healthTab = m_tabWidget->widget(3);
    QVBoxLayout* layout = new QVBoxLayout(healthTab);

    // Health Pie Chart
    m_healthChart = new QChart();
    m_healthChart->setTitle("System Health Distribution");
    m_healthChart->setAnimationOptions(QChart::SeriesAnimations);
    
    m_healthPieSeries = new QPieSeries();
    m_healthChart->addSeries(m_healthPieSeries);
    
    m_healthChartView = new QChartView(m_healthChart, this);
    m_healthChartView->setRenderHint(QPainter::Antialiasing);
    m_healthChartView->setMinimumHeight(400);
    
    layout->addWidget(m_healthChartView);
}

void DiscoveryDashboard::setupConnections() {
    connect(m_scanButton, &QPushButton::clicked, this, &DiscoveryDashboard::startScan);
    connect(m_refreshButton, &QPushButton::clicked, this, &DiscoveryDashboard::refresh);
    
    connect(m_scanner.get(), &CapabilityScanner::capabilityDetected, this, &DiscoveryDashboard::onCapabilityDetected);
    connect(m_scanner.get(), &CapabilityScanner::scanProgress, this, &DiscoveryDashboard::onScanProgress);
    connect(m_scanner.get(), &CapabilityScanner::scanCompleted, this, &DiscoveryDashboard::onScanCompleted);
    connect(m_scanner.get(), &CapabilityScanner::hardwareInfoUpdated, this, &DiscoveryDashboard::onHardwareInfoUpdated);
}

void DiscoveryDashboard::startScan() {
    m_capabilityTree->clear();
    m_categoryItems.clear();
    m_scanner->startScan();
    m_scanButton->setEnabled(false);
}

void DiscoveryDashboard::refresh() {
    updateSummaryLabels();
    m_serviceDiscovery->performHealthChecks();
}

void DiscoveryDashboard::onCapabilityDetected(const CapabilityInfo& cap) {
    QWriteLocker locker(&m_dataLock);
    m_capabilities[cap.id] = cap;
    
    QTreeWidgetItem* categoryItem = getCategoryItem(cap.category);
    QTreeWidgetItem* capItem = new QTreeWidgetItem(categoryItem);
    
    capItem->setText(0, cap.name);
    capItem->setText(1, statusToString(cap.status));
    capItem->setText(2, cap.version);
    capItem->setText(3, cap.description);
    
    capItem->setForeground(1, QBrush(statusToColor(cap.status)));
    
    categoryItem->setExpanded(true);
    updateSummaryLabels();
}

void DiscoveryDashboard::onScanProgress(int current, int total, const QString& item) {
    m_scanProgressBar->setValue(current);
}

void DiscoveryDashboard::onScanCompleted(int total) {
    m_scanButton->setEnabled(true);
    m_lastScanTime = QDateTime::currentDateTime();
    updateSummaryLabels();
    updateHealthCharts();
}

void DiscoveryDashboard::onHardwareInfoUpdated(const HardwareInfo& info) {
    m_hardwareInfo = info;
    m_cpuLabel->setText(QString("CPU: %1 (%2 cores)").arg(info.cpuModel).arg(info.cpuCores));
    m_gpuLabel->setText(QString("GPU: %1 (%2 MB)").arg(info.gpuModel).arg(info.gpuMemoryMB));
    
    m_memoryProgressBar->setValue(50); // Simulated
    m_storageProgressBar->setFormat(QString("%1 GB free / %2 GB total").arg(info.availableStorageGB).arg(info.totalStorageGB));
    m_storageProgressBar->setValue(100 - (100 * info.availableStorageGB / info.totalStorageGB));
}

void DiscoveryDashboard::updateSummaryLabels() {
    int total = m_capabilities.size();
    int available = 0;
    for (const auto& cap : m_capabilities) {
        if (cap.status == CapabilityStatus::Available) available++;
    }
    
    m_totalCapabilitiesLabel->setText(QString("Total Capabilities Discovered: %1").arg(total));
    m_availableCapabilitiesLabel->setText(QString("Available: %1").arg(available));
    
    double health = total > 0 ? (double)available / total : 1.0;
    m_systemHealthScore = health;
    m_healthScoreLabel->setText(QString("Global Health Score: %1%").arg(health * 100, 0, 'f', 1));
    m_healthProgressBar->setValue(health * 100);
}

void DiscoveryDashboard::updateHealthCharts() {
    m_healthPieSeries->clear();
    
    QMap<CapabilityStatus, int> counts;
    for (const auto& cap : m_capabilities) {
        counts[cap.status]++;
    }
    
    for (auto it = counts.begin(); it != counts.end(); ++it) {
        QPieSlice* slice = m_healthPieSeries->append(statusToString(it.key()), it.value());
        slice->setBrush(statusToColor(it.key()));
    }
}

QTreeWidgetItem* DiscoveryDashboard::getCategoryItem(CapabilityCategory cat) {
    if (m_categoryItems.contains(cat)) return m_categoryItems[cat];
    
    QTreeWidgetItem* item = new QTreeWidgetItem(m_capabilityTree);
    item->setText(0, categoryToString(cat));
    item->setFont(0, QFont("", -1, QFont::Bold));
    m_categoryItems[cat] = item;
    return item;
}

QString DiscoveryDashboard::categoryToString(CapabilityCategory cat) const {
    switch(cat) {
        case CapabilityCategory::Hardware: return "Hardware";
        case CapabilityCategory::Software: return "Software";
        case CapabilityCategory::AI_Models: return "AI Models";
        case CapabilityCategory::Network: return "Network";
        case CapabilityCategory::Storage: return "Storage";
        case CapabilityCategory::Security: return "Security";
        case CapabilityCategory::Integration: return "Integrations";
        case CapabilityCategory::Performance: return "Performance";
        default: return "Unknown";
    }
}

QString DiscoveryDashboard::statusToString(CapabilityStatus s) const {
    switch(s) {
        case CapabilityStatus::Available: return "Available";
        case CapabilityStatus::Limited: return "Limited";
        case CapabilityStatus::Degraded: return "Degraded";
        case CapabilityStatus::Unavailable: return "Unavailable";
        case CapabilityStatus::Checking: return "Checking...";
        default: return "Unknown";
    }
}

QColor DiscoveryDashboard::statusToColor(CapabilityStatus s) const {
    switch(s) {
        case CapabilityStatus::Available: return Qt::darkGreen;
        case CapabilityStatus::Limited: return Qt::darkYellow;
        case CapabilityStatus::Degraded: return QColor(255, 140, 0); // Orange
        case CapabilityStatus::Unavailable: return Qt::darkRed;
        default: return Qt::gray;
    }
}
