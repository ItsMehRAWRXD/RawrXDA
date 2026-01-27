/**
 * @file RAWRXD_ThermalDashboard_Enhanced.cpp
 * @brief Enhanced Thermal Dashboard Implementation with Predictive Visualization
 * 
 * Full production implementation of the enhanced dashboard with QtCharts
 * predicted path visualization and integrated thermal management.
 * 
 * @copyright RawrXD IDE 2026
 */

#include "RAWRXD_ThermalDashboard_Enhanced.hpp"
#include <QApplication>
#include <QStyle>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <QHeaderView>

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalDashboardEnhanced Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ThermalDashboardEnhanced::ThermalDashboardEnhanced(QWidget* parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_tempChartView(nullptr)
    , m_tempChart(nullptr)
    , m_loadChartView(nullptr)
    , m_loadChart(nullptr)
    , m_predChartView(nullptr)
    , m_predChart(nullptr)
    , m_startTime(QDateTime::currentMSecsSinceEpoch())
    , m_manualThrottleActive(false)
    , m_driveOverrideActive(false)
    , m_selectedDriveOverride(-1)
{
    // Initialize backend components
    PredictiveConfig predConfig;
    predConfig.alpha = 0.3;
    predConfig.historySize = 20;
    predConfig.thermalThreshold = 60.0;
    predConfig.emergencyThreshold = 75.0;
    predConfig.predictionHorizonMs = 5000;
    
    m_predictor = std::make_unique<PredictiveThrottling>(predConfig);
    m_loadBalancer = std::make_unique<DynamicLoadBalancer>();
    m_sharedMemory = std::make_unique<SharedMemoryManager>();
    
    // Try to connect to shared memory
    if (!m_sharedMemory->open()) {
        m_sharedMemory->create();
    }
    
    // Set control block for predictor
    if (m_sharedMemory->isOpen()) {
        m_predictor->setControlBlock(m_sharedMemory->getControlBlock());
    }
    
    // Initialize arrays
    std::memset(&m_lastSnapshot, 0, sizeof(ThermalSnapshot));
    for (int i = 0; i < 5; ++i) {
        m_nvmeTempSeries[i] = nullptr;
        m_loadSeries[i] = nullptr;
    }
    
    setupUI();
}

ThermalDashboardEnhanced::~ThermalDashboardEnhanced()
{
    // Cleanup handled by unique_ptrs and Qt parent-child relationships
}

void ThermalDashboardEnhanced::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Title
    auto* titleLabel = new QLabel("🌡️ RawrXD Thermal Dashboard Pro", this);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #00ff88;");
    mainLayout->addWidget(titleLabel);
    
    // Tab widget
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #444;
            background: #1a1a1a;
        }
        QTabBar::tab {
            background: #2a2a2a;
            color: #aaa;
            padding: 8px 16px;
            margin-right: 2px;
            border-top-left-radius: 4px;
            border-top-right-radius: 4px;
        }
        QTabBar::tab:selected {
            background: #333;
            color: #00ff88;
        }
        QTabBar::tab:hover {
            background: #3a3a3a;
        }
    )");
    
    setupMainTab();
    setupChartsTab();
    setupConfigTab();
    setupControlsTab();
    
    mainLayout->addWidget(m_tabWidget, 1);
    
    // Status bar
    m_statusLabel = new QLabel("⏳ Initializing thermal monitoring...", this);
    m_statusLabel->setStyleSheet("color: #888; font-style: italic;");
    mainLayout->addWidget(m_statusLabel);
    
    // Dark theme
    setStyleSheet("background-color: #1a1a1a;");
}

void ThermalDashboardEnhanced::setupMainTab()
{
    auto* mainWidget = new QWidget();
    auto* layout = new QVBoxLayout(mainWidget);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // NVMe Section
    // ═══════════════════════════════════════════════════════════════════════════
    auto* nvmeGroup = new QGroupBox("NVMe Drives", mainWidget);
    nvmeGroup->setStyleSheet(R"(
        QGroupBox {
            border: 2px solid #444;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
            color: #aaa;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
    )");
    
    auto* nvmeLayout = new QVBoxLayout(nvmeGroup);
    
    const char* nvmeNames[] = {
        "NVMe0 (SK hynix P41)",
        "NVMe1 (Samsung 990 PRO)",
        "NVMe2 (WD Black SN850X)",
        "NVMe3 (Samsung 990 PRO 4TB)",
        "NVMe4 (Crucial T705 4TB)"
    };
    
    for (int i = 0; i < 5; ++i) {
        auto* row = new QHBoxLayout();
        
        m_nvmeWidgets[i].nameLabel = new QLabel(nvmeNames[i], nvmeGroup);
        m_nvmeWidgets[i].nameLabel->setMinimumWidth(180);
        m_nvmeWidgets[i].nameLabel->setStyleSheet("color: #ccc;");
        
        m_nvmeWidgets[i].tempBar = new QProgressBar(nvmeGroup);
        m_nvmeWidgets[i].tempBar->setRange(0, 100);
        m_nvmeWidgets[i].tempBar->setValue(50);
        m_nvmeWidgets[i].tempBar->setTextVisible(false);
        m_nvmeWidgets[i].tempBar->setFixedHeight(20);
        m_nvmeWidgets[i].tempBar->setStyleSheet(R"(
            QProgressBar {
                border: 1px solid #555;
                border-radius: 4px;
                background: #222;
            }
            QProgressBar::chunk {
                border-radius: 3px;
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #00cc66, stop:0.6 #ffcc00, stop:1 #ff3333);
            }
        )");
        
        m_nvmeWidgets[i].tempLabel = new QLabel("--°C", nvmeGroup);
        m_nvmeWidgets[i].tempLabel->setMinimumWidth(60);
        m_nvmeWidgets[i].tempLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_nvmeWidgets[i].tempLabel->setStyleSheet("color: #0f0; font-weight: bold;");
        
        m_nvmeWidgets[i].headroomLabel = new QLabel("(--°C)", nvmeGroup);
        m_nvmeWidgets[i].headroomLabel->setMinimumWidth(60);
        m_nvmeWidgets[i].headroomLabel->setStyleSheet("color: #888;");
        
        row->addWidget(m_nvmeWidgets[i].nameLabel);
        row->addWidget(m_nvmeWidgets[i].tempBar, 1);
        row->addWidget(m_nvmeWidgets[i].tempLabel);
        row->addWidget(m_nvmeWidgets[i].headroomLabel);
        
        nvmeLayout->addLayout(row);
    }
    
    layout->addWidget(nvmeGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // System Thermals Section
    // ═══════════════════════════════════════════════════════════════════════════
    auto* systemGroup = new QGroupBox("System Thermals", mainWidget);
    systemGroup->setStyleSheet(nvmeGroup->styleSheet());
    
    auto* systemLayout = new QVBoxLayout(systemGroup);
    
    // GPU
    auto* gpuRow = new QHBoxLayout();
    auto* gpuLabel = new QLabel("7800 XT Junction", systemGroup);
    gpuLabel->setMinimumWidth(180);
    gpuLabel->setStyleSheet("color: #ff6666;");
    
    m_gpuTempBar = new QProgressBar(systemGroup);
    m_gpuTempBar->setRange(0, 110);
    m_gpuTempBar->setValue(65);
    m_gpuTempBar->setTextVisible(false);
    m_gpuTempBar->setFixedHeight(20);
    m_gpuTempBar->setStyleSheet(m_nvmeWidgets[0].tempBar->styleSheet());
    
    m_gpuTempLabel = new QLabel("--°C", systemGroup);
    m_gpuTempLabel->setMinimumWidth(60);
    m_gpuTempLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_gpuTempLabel->setStyleSheet("color: #ff6666; font-weight: bold;");
    
    gpuRow->addWidget(gpuLabel);
    gpuRow->addWidget(m_gpuTempBar, 1);
    gpuRow->addWidget(m_gpuTempLabel);
    systemLayout->addLayout(gpuRow);
    
    // CPU
    auto* cpuRow = new QHBoxLayout();
    auto* cpuLabel = new QLabel("7800X3D Package", systemGroup);
    cpuLabel->setMinimumWidth(180);
    cpuLabel->setStyleSheet("color: #6699ff;");
    
    m_cpuTempBar = new QProgressBar(systemGroup);
    m_cpuTempBar->setRange(0, 95);
    m_cpuTempBar->setValue(55);
    m_cpuTempBar->setTextVisible(false);
    m_cpuTempBar->setFixedHeight(20);
    m_cpuTempBar->setStyleSheet(m_nvmeWidgets[0].tempBar->styleSheet());
    
    m_cpuTempLabel = new QLabel("--°C", systemGroup);
    m_cpuTempLabel->setMinimumWidth(60);
    m_cpuTempLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_cpuTempLabel->setStyleSheet("color: #6699ff; font-weight: bold;");
    
    cpuRow->addWidget(cpuLabel);
    cpuRow->addWidget(m_cpuTempBar, 1);
    cpuRow->addWidget(m_cpuTempLabel);
    systemLayout->addLayout(cpuRow);
    
    layout->addWidget(systemGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Prediction & Throttle Section
    // ═══════════════════════════════════════════════════════════════════════════
    auto* predGroup = new QGroupBox("Predictive Throttling", mainWidget);
    predGroup->setStyleSheet(nvmeGroup->styleSheet());
    
    auto* predLayout = new QVBoxLayout(predGroup);
    
    // Prediction status
    m_predictionStatusLabel = new QLabel("📊 Prediction: Initializing...", predGroup);
    m_predictionStatusLabel->setStyleSheet("color: #00ccff; font-weight: bold;");
    predLayout->addWidget(m_predictionStatusLabel);
    
    // Throttle action
    m_throttleActionLabel = new QLabel("⚡ Action: NONE", predGroup);
    m_throttleActionLabel->setStyleSheet("color: #00ff00; font-weight: bold;");
    predLayout->addWidget(m_throttleActionLabel);
    
    // Throttle bar
    auto* throttleRow = new QHBoxLayout();
    auto* throttleLbl = new QLabel("Current Throttle", predGroup);
    throttleLbl->setStyleSheet("color: #ffcc00;");
    throttleLbl->setMinimumWidth(120);
    
    m_throttleBar = new QProgressBar(predGroup);
    m_throttleBar->setRange(0, 100);
    m_throttleBar->setValue(0);
    m_throttleBar->setTextVisible(false);
    m_throttleBar->setFixedHeight(20);
    m_throttleBar->setStyleSheet(R"(
        QProgressBar {
            border: 1px solid #555;
            border-radius: 4px;
            background: #222;
        }
        QProgressBar::chunk {
            border-radius: 3px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #00ff00, stop:0.5 #ffff00, stop:1 #ff0000);
        }
    )");
    
    m_throttleLabel = new QLabel("0%", predGroup);
    m_throttleLabel->setMinimumWidth(50);
    m_throttleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_throttleLabel->setStyleSheet("color: #0f0; font-weight: bold;");
    
    throttleRow->addWidget(throttleLbl);
    throttleRow->addWidget(m_throttleBar, 1);
    throttleRow->addWidget(m_throttleLabel);
    predLayout->addLayout(throttleRow);
    
    // Mode selector
    auto* modeRow = new QHBoxLayout();
    auto* modeLbl = new QLabel("Burst Mode:", predGroup);
    modeLbl->setStyleSheet("color: #aaa;");
    
    m_burstModeCombo = new QComboBox(predGroup);
    m_burstModeCombo->addItem("🚀 SOVEREIGN-MAX (142μs)", 0);
    m_burstModeCombo->addItem("🌡️ THERMAL-GOVERNED (237μs)", 1);
    m_burstModeCombo->addItem("⚡ ADAPTIVE-HYBRID (dynamic)", 2);
    m_burstModeCombo->setCurrentIndex(2);
    m_burstModeCombo->setStyleSheet(R"(
        QComboBox {
            background: #333;
            color: #fff;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 5px 10px;
            min-width: 250px;
        }
        QComboBox:hover {
            border-color: #00ff88;
        }
    )");
    
    m_applyButton = new QPushButton("Apply", predGroup);
    m_applyButton->setStyleSheet(R"(
        QPushButton {
            background: #00aa55;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 20px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: #00cc66;
        }
    )");
    connect(m_applyButton, &QPushButton::clicked, this, &ThermalDashboardEnhanced::onBurstModeApply);
    
    modeRow->addWidget(modeLbl);
    modeRow->addWidget(m_burstModeCombo, 1);
    modeRow->addWidget(m_applyButton);
    predLayout->addLayout(modeRow);
    
    layout->addWidget(predGroup);
    layout->addStretch();
    
    m_tabWidget->addTab(mainWidget, "📊 Dashboard");
}

void ThermalDashboardEnhanced::setupChartsTab()
{
    auto* chartsWidget = new QWidget();
    auto* layout = new QVBoxLayout(chartsWidget);
    
    createTemperatureChart();
    createPredictionChart();
    
    if (m_tempChartView) {
        layout->addWidget(m_tempChartView, 2);
    }
    if (m_predChartView) {
        layout->addWidget(m_predChartView, 1);
    }
    
    m_tabWidget->addTab(chartsWidget, "📈 Charts");
}

void ThermalDashboardEnhanced::createTemperatureChart()
{
    m_tempChart = new QChart();
    m_tempChart->setTitle("Temperature History & Prediction");
    m_tempChart->setTheme(QChart::ChartThemeDark);
    m_tempChart->setBackgroundBrush(QBrush(QColor(26, 26, 26)));
    m_tempChart->setAnimationOptions(QChart::NoAnimation);
    
    // Create series for each sensor
    const QColor nvmeColors[] = {
        QColor(0, 255, 136),    // Green
        QColor(0, 204, 255),    // Cyan
        QColor(255, 204, 0),    // Yellow
        QColor(255, 102, 102),  // Red
        QColor(204, 153, 255)   // Purple
    };
    
    for (int i = 0; i < 5; ++i) {
        m_nvmeTempSeries[i] = new QLineSeries();
        m_nvmeTempSeries[i]->setName(QString("NVMe%1").arg(i));
        m_nvmeTempSeries[i]->setColor(nvmeColors[i]);
        m_tempChart->addSeries(m_nvmeTempSeries[i]);
    }
    
    m_gpuTempSeries = new QLineSeries();
    m_gpuTempSeries->setName("GPU");
    m_gpuTempSeries->setColor(QColor(255, 100, 100));
    m_tempChart->addSeries(m_gpuTempSeries);
    
    m_cpuTempSeries = new QLineSeries();
    m_cpuTempSeries->setName("CPU");
    m_cpuTempSeries->setColor(QColor(100, 150, 255));
    m_tempChart->addSeries(m_cpuTempSeries);
    
    // Predicted path (dotted line)
    m_predictedTempSeries = new QLineSeries();
    m_predictedTempSeries->setName("Predicted");
    QPen predPen(QColor(255, 255, 0));
    predPen.setStyle(Qt::DashLine);
    predPen.setWidth(2);
    m_predictedTempSeries->setPen(predPen);
    m_tempChart->addSeries(m_predictedTempSeries);
    
    // Threshold markers
    m_thresholdMarkers = new QScatterSeries();
    m_thresholdMarkers->setName("Threshold");
    m_thresholdMarkers->setMarkerShape(QScatterSeries::MarkerShapeTriangle);
    m_thresholdMarkers->setMarkerSize(10);
    m_thresholdMarkers->setColor(QColor(255, 0, 0));
    m_tempChart->addSeries(m_thresholdMarkers);
    
    // Create axes
    m_tempXAxis = new QValueAxis();
    m_tempXAxis->setTitleText("Time (seconds)");
    m_tempXAxis->setRange(0, 300);
    m_tempXAxis->setLabelFormat("%d");
    m_tempChart->addAxis(m_tempXAxis, Qt::AlignBottom);
    
    m_tempYAxis = new QValueAxis();
    m_tempYAxis->setTitleText("Temperature (°C)");
    m_tempYAxis->setRange(30, 90);
    m_tempYAxis->setLabelFormat("%d");
    m_tempChart->addAxis(m_tempYAxis, Qt::AlignLeft);
    
    // Attach series to axes
    for (int i = 0; i < 5; ++i) {
        m_nvmeTempSeries[i]->attachAxis(m_tempXAxis);
        m_nvmeTempSeries[i]->attachAxis(m_tempYAxis);
    }
    m_gpuTempSeries->attachAxis(m_tempXAxis);
    m_gpuTempSeries->attachAxis(m_tempYAxis);
    m_cpuTempSeries->attachAxis(m_tempXAxis);
    m_cpuTempSeries->attachAxis(m_tempYAxis);
    m_predictedTempSeries->attachAxis(m_tempXAxis);
    m_predictedTempSeries->attachAxis(m_tempYAxis);
    m_thresholdMarkers->attachAxis(m_tempXAxis);
    m_thresholdMarkers->attachAxis(m_tempYAxis);
    
    m_tempChartView = new QChartView(m_tempChart);
    m_tempChartView->setRenderHint(QPainter::Antialiasing);
}

void ThermalDashboardEnhanced::createPredictionChart()
{
    m_predChart = new QChart();
    m_predChart->setTitle("Prediction Metrics");
    m_predChart->setTheme(QChart::ChartThemeDark);
    m_predChart->setBackgroundBrush(QBrush(QColor(26, 26, 26)));
    m_predChart->setAnimationOptions(QChart::NoAnimation);
    
    m_ewmaSeries = new QLineSeries();
    m_ewmaSeries->setName("EWMA");
    m_ewmaSeries->setColor(QColor(0, 255, 136));
    m_predChart->addSeries(m_ewmaSeries);
    
    m_slopeSeries = new QLineSeries();
    m_slopeSeries->setName("Slope (×10)");
    m_slopeSeries->setColor(QColor(255, 204, 0));
    m_predChart->addSeries(m_slopeSeries);
    
    m_confidenceSeries = new QLineSeries();
    m_confidenceSeries->setName("Confidence (×100)");
    m_confidenceSeries->setColor(QColor(0, 204, 255));
    m_predChart->addSeries(m_confidenceSeries);
    
    m_predChart->createDefaultAxes();
    m_predChart->legend()->setVisible(true);
    
    m_predChartView = new QChartView(m_predChart);
    m_predChartView->setRenderHint(QPainter::Antialiasing);
}

void ThermalDashboardEnhanced::setupConfigTab()
{
    auto* configWidget = new QWidget();
    auto* layout = new QVBoxLayout(configWidget);
    
    auto* group = new QGroupBox("Predictive Throttling Configuration", configWidget);
    auto* formLayout = new QVBoxLayout(group);
    
    // Alpha (EWMA smoothing factor)
    auto* alphaRow = new QHBoxLayout();
    alphaRow->addWidget(new QLabel("EWMA Alpha (responsiveness):"));
    m_alphaSpinBox = new QDoubleSpinBox();
    m_alphaSpinBox->setRange(0.1, 0.9);
    m_alphaSpinBox->setSingleStep(0.05);
    m_alphaSpinBox->setValue(0.3);
    alphaRow->addWidget(m_alphaSpinBox);
    formLayout->addLayout(alphaRow);
    
    // History size
    auto* historyRow = new QHBoxLayout();
    historyRow->addWidget(new QLabel("History Size (samples):"));
    m_historySizeSpinBox = new QSpinBox();
    m_historySizeSpinBox->setRange(5, 100);
    m_historySizeSpinBox->setValue(20);
    historyRow->addWidget(m_historySizeSpinBox);
    formLayout->addLayout(historyRow);
    
    // Thermal threshold
    auto* thresholdRow = new QHBoxLayout();
    thresholdRow->addWidget(new QLabel("Thermal Threshold (°C):"));
    m_thermalThresholdSpinBox = new QDoubleSpinBox();
    m_thermalThresholdSpinBox->setRange(40.0, 80.0);
    m_thermalThresholdSpinBox->setSingleStep(1.0);
    m_thermalThresholdSpinBox->setValue(60.0);
    thresholdRow->addWidget(m_thermalThresholdSpinBox);
    formLayout->addLayout(thresholdRow);
    
    // Emergency threshold
    auto* emergencyRow = new QHBoxLayout();
    emergencyRow->addWidget(new QLabel("Emergency Threshold (°C):"));
    m_emergencyThresholdSpinBox = new QDoubleSpinBox();
    m_emergencyThresholdSpinBox->setRange(60.0, 90.0);
    m_emergencyThresholdSpinBox->setSingleStep(1.0);
    m_emergencyThresholdSpinBox->setValue(75.0);
    emergencyRow->addWidget(m_emergencyThresholdSpinBox);
    formLayout->addLayout(emergencyRow);
    
    // Prediction horizon
    auto* horizonRow = new QHBoxLayout();
    horizonRow->addWidget(new QLabel("Prediction Horizon (ms):"));
    m_predictionHorizonSpinBox = new QSpinBox();
    m_predictionHorizonSpinBox->setRange(1000, 30000);
    m_predictionHorizonSpinBox->setSingleStep(500);
    m_predictionHorizonSpinBox->setValue(5000);
    connect(m_predictionHorizonSpinBox, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &ThermalDashboardEnhanced::onPredictionHorizonChanged);
    horizonRow->addWidget(m_predictionHorizonSpinBox);
    formLayout->addLayout(horizonRow);
    
    // Predictive enabled
    m_predictiveEnabledCheck = new QCheckBox("Enable Predictive Throttling");
    m_predictiveEnabledCheck->setChecked(true);
    formLayout->addWidget(m_predictiveEnabledCheck);
    
    layout->addWidget(group);
    layout->addStretch();
    
    m_tabWidget->addTab(configWidget, "⚙️ Config");
}

void ThermalDashboardEnhanced::setupControlsTab()
{
    auto* controlsWidget = new QWidget();
    auto* layout = new QVBoxLayout(controlsWidget);
    
    // Manual throttle control
    auto* throttleGroup = new QGroupBox("Manual Throttle Override", controlsWidget);
    auto* throttleLayout = new QVBoxLayout(throttleGroup);
    
    m_manualThrottleEnabled = new QCheckBox("Enable Manual Throttle");
    connect(m_manualThrottleEnabled, &QCheckBox::toggled, [this](bool checked) {
        m_manualThrottleActive = checked;
        m_manualThrottleSlider->setEnabled(checked);
    });
    throttleLayout->addWidget(m_manualThrottleEnabled);
    
    auto* sliderRow = new QHBoxLayout();
    m_manualThrottleSlider = new QSlider(Qt::Horizontal);
    m_manualThrottleSlider->setRange(0, 100);
    m_manualThrottleSlider->setValue(0);
    m_manualThrottleSlider->setEnabled(false);
    connect(m_manualThrottleSlider, &QSlider::valueChanged,
            this, &ThermalDashboardEnhanced::onThrottleSliderChanged);
    
    m_manualThrottleLabel = new QLabel("0%");
    m_manualThrottleLabel->setMinimumWidth(50);
    
    sliderRow->addWidget(m_manualThrottleSlider, 1);
    sliderRow->addWidget(m_manualThrottleLabel);
    throttleLayout->addLayout(sliderRow);
    
    layout->addWidget(throttleGroup);
    
    // Drive override
    auto* driveGroup = new QGroupBox("Drive Selection Override", controlsWidget);
    auto* driveLayout = new QVBoxLayout(driveGroup);
    
    m_driveOverrideCheck = new QCheckBox("Override Automatic Drive Selection");
    connect(m_driveOverrideCheck, &QCheckBox::toggled,
            this, &ThermalDashboardEnhanced::onDriveOverrideToggled);
    driveLayout->addWidget(m_driveOverrideCheck);
    
    m_driveOverrideCombo = new QComboBox();
    m_driveOverrideCombo->addItem("NVMe0 (SK hynix P41)", 0);
    m_driveOverrideCombo->addItem("NVMe1 (Samsung 990 PRO)", 1);
    m_driveOverrideCombo->addItem("NVMe2 (WD Black SN850X)", 2);
    m_driveOverrideCombo->addItem("NVMe3 (Samsung 990 PRO 4TB)", 3);
    m_driveOverrideCombo->addItem("NVMe4 (Crucial T705 4TB)", 4);
    m_driveOverrideCombo->setEnabled(false);
    connect(m_driveOverrideCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ThermalDashboardEnhanced::onManualDriveSelected);
    driveLayout->addWidget(m_driveOverrideCombo);
    
    // Drive selection table
    m_driveSelectionTable = new QTableWidget(5, 4);
    m_driveSelectionTable->setHorizontalHeaderLabels({"Drive", "Temp", "Headroom", "Score"});
    m_driveSelectionTable->horizontalHeader()->setStretchLastSection(true);
    driveLayout->addWidget(m_driveSelectionTable);
    
    layout->addWidget(driveGroup);
    
    // Emergency controls
    auto* emergencyGroup = new QGroupBox("Emergency Controls", controlsWidget);
    auto* emergencyLayout = new QHBoxLayout(emergencyGroup);
    
    m_emergencyStopButton = new QPushButton("🛑 EMERGENCY STOP");
    m_emergencyStopButton->setStyleSheet(R"(
        QPushButton {
            background: #cc0000;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 10px 20px;
            font-weight: bold;
            font-size: 14px;
        }
        QPushButton:hover {
            background: #ff0000;
        }
    )");
    
    m_clearEmergencyButton = new QPushButton("✓ Clear Emergency");
    m_clearEmergencyButton->setStyleSheet(R"(
        QPushButton {
            background: #00aa55;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 10px 20px;
            font-weight: bold;
        }
        QPushButton:hover {
            background: #00cc66;
        }
    )");
    
    emergencyLayout->addWidget(m_emergencyStopButton);
    emergencyLayout->addWidget(m_clearEmergencyButton);
    
    layout->addWidget(emergencyGroup);
    layout->addStretch();
    
    m_tabWidget->addTab(controlsWidget, "🎛️ Controls");
}

// ═══════════════════════════════════════════════════════════════════════════════
// Public Slots
// ═══════════════════════════════════════════════════════════════════════════════

void ThermalDashboardEnhanced::onThermalUpdate(const ThermalSnapshot& snapshot)
{
    m_lastSnapshot = snapshot;
    
    // Update NVMe displays
    for (int i = 0; i < snapshot.activeDriveCount && i < 5; ++i) {
        updateNVMeDisplay(i, snapshot.nvmeTemps[i]);
        m_nvmeWidgets[i].nameLabel->setVisible(true);
        m_nvmeWidgets[i].tempBar->setVisible(true);
        m_nvmeWidgets[i].tempLabel->setVisible(true);
        m_nvmeWidgets[i].headroomLabel->setVisible(true);
    }
    
    // Hide unused drives
    for (int i = snapshot.activeDriveCount; i < 5; ++i) {
        m_nvmeWidgets[i].nameLabel->setVisible(false);
        m_nvmeWidgets[i].tempBar->setVisible(false);
        m_nvmeWidgets[i].tempLabel->setVisible(false);
        m_nvmeWidgets[i].headroomLabel->setVisible(false);
    }
    
    // Update GPU/CPU
    updateGPUDisplay(snapshot.gpuTemp);
    updateCPUDisplay(snapshot.cpuTemp);
    
    // Update throttle
    updateThrottleDisplay(snapshot.currentThrottle);
    
    // Feed predictor
    m_predictor->addFromSnapshot(snapshot, 0);  // Use max temp
    
    // Update load balancer
    m_loadBalancer->updateFromSnapshot(snapshot);
    
    // Update charts
    updateTemperatureChart(snapshot);
    
    // Update prediction
    updatePredictionDisplay();
    updatePredictedPath();
    
    // Update load balancer display
    updateLoadBalancerDisplay();
    
    // Status
    m_statusLabel->setText(QString("✓ Last update: %1 | %2 drives active")
        .arg(QDateTime::fromMSecsSinceEpoch(snapshot.timestamp).toString("hh:mm:ss"))
        .arg(snapshot.activeDriveCount));
}

void ThermalDashboardEnhanced::refreshCharts()
{
    if (m_tempChartView) {
        m_tempChartView->update();
    }
    if (m_predChartView) {
        m_predChartView->update();
    }
}

void ThermalDashboardEnhanced::clearHistory()
{
    m_predictor->clearHistory();
    
    for (int i = 0; i < 5; ++i) {
        m_nvmeHistory[i].clear();
        if (m_nvmeTempSeries[i]) {
            m_nvmeTempSeries[i]->clear();
        }
    }
    m_gpuHistory.clear();
    m_cpuHistory.clear();
    
    if (m_gpuTempSeries) m_gpuTempSeries->clear();
    if (m_cpuTempSeries) m_cpuTempSeries->clear();
    if (m_predictedTempSeries) m_predictedTempSeries->clear();
    if (m_ewmaSeries) m_ewmaSeries->clear();
    if (m_slopeSeries) m_slopeSeries->clear();
    if (m_confidenceSeries) m_confidenceSeries->clear();
    
    m_startTime = QDateTime::currentMSecsSinceEpoch();
}

// ═══════════════════════════════════════════════════════════════════════════════
// Private Slots
// ═══════════════════════════════════════════════════════════════════════════════

void ThermalDashboardEnhanced::onBurstModeApply()
{
    int mode = m_burstModeCombo->currentData().toInt();
    
    if (m_sharedMemory && m_sharedMemory->isOpen()) {
        m_sharedMemory->setBurstMode(static_cast<BurstMode>(mode));
    }
    
    emit burstModeChanged(mode);
}

void ThermalDashboardEnhanced::onThrottleSliderChanged(int value)
{
    m_manualThrottleLabel->setText(QString("%1%").arg(value));
    
    if (m_manualThrottleActive) {
        if (m_sharedMemory && m_sharedMemory->isOpen()) {
            m_sharedMemory->setThrottlePercent(value);
        }
        emit throttleAdjusted(value);
    }
}

void ThermalDashboardEnhanced::onDriveOverrideToggled(bool checked)
{
    m_driveOverrideActive = checked;
    m_driveOverrideCombo->setEnabled(checked);
    
    if (!checked) {
        m_selectedDriveOverride = -1;
    }
}

void ThermalDashboardEnhanced::onManualDriveSelected(int index)
{
    if (m_driveOverrideActive) {
        m_selectedDriveOverride = index;
        emit driveSelected(index);
    }
}

void ThermalDashboardEnhanced::onPredictionHorizonChanged(int value)
{
    PredictiveConfig config = m_predictor->config();
    config.predictionHorizonMs = value;
    m_predictor->setConfig(config);
}

void ThermalDashboardEnhanced::updatePredictedPath()
{
    if (!m_predictedTempSeries) return;
    
    m_predictedTempSeries->clear();
    
    PredictionResult prediction = m_predictor->getPrediction();
    if (!prediction.isValid) return;
    
    // Get current time relative to start
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double currentTimeSec = (now - m_startTime) / 1000.0;
    double currentTemp = m_predictor->getLastReading();
    
    // Add current point
    m_predictedTempSeries->append(currentTimeSec, currentTemp);
    
    // Add predicted point
    double futureTimeSec = currentTimeSec + (prediction.predictionHorizonMs / 1000.0);
    m_predictedTempSeries->append(futureTimeSec, prediction.predictedTemp);
}

// ═══════════════════════════════════════════════════════════════════════════════
// Private Methods
// ═══════════════════════════════════════════════════════════════════════════════

void ThermalDashboardEnhanced::updateNVMeDisplay(int index, float temp)
{
    if (index < 0 || index >= 5) return;
    
    m_nvmeWidgets[index].tempBar->setValue(static_cast<int>(temp));
    m_nvmeWidgets[index].tempLabel->setText(QString("%1°C").arg(temp, 0, 'f', 1));
    m_nvmeWidgets[index].tempLabel->setStyleSheet(
        QString("color: %1; font-weight: bold;").arg(getTempColor(temp)));
    
    // Calculate headroom
    double headroom = 70.0 - temp;  // Assuming 70°C max
    m_nvmeWidgets[index].headroomLabel->setText(QString("(%1°C)").arg(headroom, 0, 'f', 1));
    
    QString headroomColor = (headroom > 15) ? "#00ff00" : (headroom > 5) ? "#ffcc00" : "#ff3333";
    m_nvmeWidgets[index].headroomLabel->setStyleSheet(QString("color: %1;").arg(headroomColor));
}

void ThermalDashboardEnhanced::updateGPUDisplay(float temp)
{
    m_gpuTempBar->setValue(static_cast<int>(temp));
    m_gpuTempLabel->setText(QString("%1°C").arg(temp, 0, 'f', 1));
    m_gpuTempLabel->setStyleSheet(
        QString("color: %1; font-weight: bold;").arg(getTempColor(temp)));
}

void ThermalDashboardEnhanced::updateCPUDisplay(float temp)
{
    m_cpuTempBar->setValue(static_cast<int>(temp));
    m_cpuTempLabel->setText(QString("%1°C").arg(temp, 0, 'f', 1));
    m_cpuTempLabel->setStyleSheet(
        QString("color: %1; font-weight: bold;").arg(getTempColor(temp)));
}

void ThermalDashboardEnhanced::updateThrottleDisplay(int throttle)
{
    m_throttleBar->setValue(throttle);
    m_throttleLabel->setText(QString("%1%").arg(throttle));
    
    QString color;
    if (throttle == 0) {
        color = "#00ff00";
    } else if (throttle < 20) {
        color = "#88ff00";
    } else if (throttle < 40) {
        color = "#ffcc00";
    } else {
        color = "#ff3333";
    }
    m_throttleLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
}

void ThermalDashboardEnhanced::updatePredictionDisplay()
{
    PredictionResult prediction = m_predictor->getPrediction();
    
    QString statusText = QString("📊 Prediction: %1°C (±%2°C) | Confidence: %3% | Slope: %4°C/s")
        .arg(prediction.predictedTemp, 0, 'f', 1)
        .arg((1.0 - prediction.confidence) * 5.0, 0, 'f', 1)  // Uncertainty estimate
        .arg(prediction.confidence * 100.0, 0, 'f', 0)
        .arg(prediction.slope, 0, 'f', 2);
    
    m_predictionStatusLabel->setText(statusText);
    
    // Update action label
    ThrottleAction action = m_predictor->getRecommendedAction(prediction.predictedTemp);
    m_throttleActionLabel->setText(QString("⚡ Action: %1").arg(getThrottleActionString(action)));
    m_throttleActionLabel->setStyleSheet(
        QString("color: %1; font-weight: bold;").arg(getThrottleActionColor(action).name()));
}

void ThermalDashboardEnhanced::updateLoadBalancerDisplay()
{
    DriveSelectionResult result = m_loadBalancer->selectOptimalDriveDetailed();
    
    // Update table
    for (int i = 0; i < 5; ++i) {
        auto info = m_loadBalancer->getDriveInfo(i);
        if (!info.has_value()) continue;
        
        m_driveSelectionTable->setItem(i, 0, new QTableWidgetItem(QString("NVMe%1").arg(i)));
        m_driveSelectionTable->setItem(i, 1, new QTableWidgetItem(QString("%1°C").arg(info->currentTemp, 0, 'f', 1)));
        m_driveSelectionTable->setItem(i, 2, new QTableWidgetItem(QString("%1°C").arg(info->thermalHeadroom, 0, 'f', 1)));
        m_driveSelectionTable->setItem(i, 3, new QTableWidgetItem(QString("%1").arg(result.allScores[i], 0, 'f', 3)));
        
        // Highlight selected drive
        if (i == result.selectedDrive) {
            for (int j = 0; j < 4; ++j) {
                m_driveSelectionTable->item(i, j)->setBackground(QColor(0, 100, 50));
            }
        }
    }
}

void ThermalDashboardEnhanced::updateTemperatureChart(const ThermalSnapshot& snapshot)
{
    qint64 now = QDateTime::currentMSecsSinceEpoch();
    double timeSec = (now - m_startTime) / 1000.0;
    
    // Update NVMe series
    for (int i = 0; i < snapshot.activeDriveCount && i < 5; ++i) {
        if (m_nvmeTempSeries[i]) {
            m_nvmeTempSeries[i]->append(timeSec, snapshot.nvmeTemps[i]);
            
            // Limit points
            while (m_nvmeTempSeries[i]->count() > static_cast<int>(MAX_HISTORY_POINTS)) {
                m_nvmeTempSeries[i]->remove(0);
            }
        }
    }
    
    // Update GPU/CPU
    if (m_gpuTempSeries) {
        m_gpuTempSeries->append(timeSec, snapshot.gpuTemp);
        while (m_gpuTempSeries->count() > static_cast<int>(MAX_HISTORY_POINTS)) {
            m_gpuTempSeries->remove(0);
        }
    }
    
    if (m_cpuTempSeries) {
        m_cpuTempSeries->append(timeSec, snapshot.cpuTemp);
        while (m_cpuTempSeries->count() > static_cast<int>(MAX_HISTORY_POINTS)) {
            m_cpuTempSeries->remove(0);
        }
    }
    
    // Update prediction metrics chart
    if (m_ewmaSeries) {
        m_ewmaSeries->append(timeSec, m_predictor->getCurrentEWMA());
        while (m_ewmaSeries->count() > static_cast<int>(MAX_HISTORY_POINTS)) {
            m_ewmaSeries->remove(0);
        }
    }
    
    if (m_slopeSeries) {
        m_slopeSeries->append(timeSec, m_predictor->getTemperatureSlope() * 10 + 50);  // Scale and offset
        while (m_slopeSeries->count() > static_cast<int>(MAX_HISTORY_POINTS)) {
            m_slopeSeries->remove(0);
        }
    }
    
    // Update X axis range
    if (m_tempXAxis && timeSec > 300) {
        m_tempXAxis->setRange(timeSec - 300, timeSec + 10);
    }
}

QString ThermalDashboardEnhanced::getTempColor(float temp)
{
    if (temp < 55) return "#00ff00";
    if (temp < 65) return "#88ff00";
    if (temp < 72) return "#ffcc00";
    if (temp < 80) return "#ff8800";
    return "#ff3333";
}

QString ThermalDashboardEnhanced::getThrottleActionString(ThrottleAction action)
{
    switch (action) {
        case ThrottleAction::NONE:      return "NONE";
        case ThrottleAction::LIGHT:     return "LIGHT";
        case ThrottleAction::MODERATE:  return "MODERATE";
        case ThrottleAction::HEAVY:     return "HEAVY";
        case ThrottleAction::EMERGENCY: return "EMERGENCY";
        default:                        return "UNKNOWN";
    }
}

QColor ThermalDashboardEnhanced::getThrottleActionColor(ThrottleAction action)
{
    switch (action) {
        case ThrottleAction::NONE:      return QColor(0, 255, 0);
        case ThrottleAction::LIGHT:     return QColor(136, 255, 0);
        case ThrottleAction::MODERATE:  return QColor(255, 204, 0);
        case ThrottleAction::HEAVY:     return QColor(255, 136, 0);
        case ThrottleAction::EMERGENCY: return QColor(255, 51, 51);
        default:                        return QColor(128, 128, 128);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalCompactWidgetEnhanced Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ThermalCompactWidgetEnhanced::ThermalCompactWidgetEnhanced(QWidget* parent)
    : QFrame(parent)
{
    setupUI();
}

void ThermalCompactWidgetEnhanced::setupUI()
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setStyleSheet(R"(
        QFrame {
            background: #2a2a2a;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 4px;
        }
    )");
    
    auto* layout = new QHBoxLayout(this);
    layout->setSpacing(10);
    layout->setContentsMargins(8, 4, 8, 4);
    
    // Current temp
    m_maxTempLabel = new QLabel("🌡️ --°C", this);
    m_maxTempLabel->setStyleSheet("color: #0f0; font-weight: bold;");
    layout->addWidget(m_maxTempLabel);
    
    // Predicted temp
    m_predictedTempLabel = new QLabel("→ --°C", this);
    m_predictedTempLabel->setToolTip("Predicted temperature");
    m_predictedTempLabel->setStyleSheet("color: #ff0; font-weight: bold;");
    layout->addWidget(m_predictedTempLabel);
    
    // Throttle icon
    m_throttleIcon = new QLabel("⚡", this);
    m_throttleIcon->setToolTip("Throttle status");
    layout->addWidget(m_throttleIcon);
    
    // Mode icon
    m_modeIcon = new QLabel("🔄", this);
    m_modeIcon->setToolTip("Adaptive Hybrid mode");
    layout->addWidget(m_modeIcon);
    
    // Drive icon
    m_driveIcon = new QLabel("💾0", this);
    m_driveIcon->setToolTip("Selected drive");
    layout->addWidget(m_driveIcon);
    
    // Expand button
    m_expandButton = new QPushButton("⊞", this);
    m_expandButton->setFixedSize(24, 24);
    m_expandButton->setStyleSheet(R"(
        QPushButton {
            background: #444;
            color: #fff;
            border: none;
            border-radius: 4px;
        }
        QPushButton:hover {
            background: #555;
        }
    )");
    connect(m_expandButton, &QPushButton::clicked, this, &ThermalCompactWidgetEnhanced::expandRequested);
    layout->addWidget(m_expandButton);
    
    setFixedHeight(36);
}

void ThermalCompactWidgetEnhanced::onThermalUpdate(const ThermalSnapshot& snapshot)
{
    // Find max temp
    float maxTemp = snapshot.gpuTemp;
    for (int i = 0; i < snapshot.activeDriveCount; ++i) {
        maxTemp = qMax(maxTemp, snapshot.nvmeTemps[i]);
    }
    maxTemp = qMax(maxTemp, snapshot.cpuTemp);
    
    QString color = (maxTemp < 65) ? "#00ff00" : (maxTemp < 75) ? "#ffcc00" : "#ff3333";
    m_maxTempLabel->setText(QString("🌡️ %1°C").arg(maxTemp, 0, 'f', 0));
    m_maxTempLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
    
    // Throttle icon
    if (snapshot.currentThrottle == 0) {
        m_throttleIcon->setText("⚡");
        m_throttleIcon->setToolTip("Full speed");
    } else if (snapshot.currentThrottle < 30) {
        m_throttleIcon->setText("🔋");
        m_throttleIcon->setToolTip(QString("Light throttle: %1%").arg(snapshot.currentThrottle));
    } else {
        m_throttleIcon->setText("🐢");
        m_throttleIcon->setToolTip(QString("Heavy throttle: %1%").arg(snapshot.currentThrottle));
    }
}

void ThermalCompactWidgetEnhanced::onPredictionUpdate(const PredictionResult& prediction)
{
    QString color = (prediction.predictedTemp < 65) ? "#ffff00" : 
                    (prediction.predictedTemp < 75) ? "#ff8800" : "#ff3333";
    
    m_predictedTempLabel->setText(QString("→ %1°C").arg(prediction.predictedTemp, 0, 'f', 0));
    m_predictedTempLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
    m_predictedTempLabel->setToolTip(QString("Predicted in %1ms (confidence: %2%)")
        .arg(prediction.predictionHorizonMs)
        .arg(prediction.confidence * 100, 0, 'f', 0));
}

} // namespace rawrxd::thermal
