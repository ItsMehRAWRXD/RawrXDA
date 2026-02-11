/**
 * @file RAWRXD_ThermalDashboard_Enhanced.cpp
 * @brief Enhanced Thermal Dashboard Implementation with Predictive Visualization
 * 
// REMOVED_QT:  * Full production implementation of the enhanced dashboard with QtCharts
 * predicted path visualization and integrated thermal management.
 * 
 * @copyright RawrXD IDE 2026
 */

#include "RAWRXD_ThermalDashboard_Enhanced.hpp"


namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalDashboardEnhanced Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ThermalDashboardEnhanced::ThermalDashboardEnhanced(void* parent)
    : void(parent)
    , m_tabWidget(nullptr)
    , m_tempChartView(nullptr)
    , m_tempChart(nullptr)
    , m_loadChartView(nullptr)
    , m_loadChart(nullptr)
    , m_predChartView(nullptr)
    , m_predChart(nullptr)
    , m_startTime(std::chrono::system_clock::time_point::currentMSecsSinceEpoch())
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
    auto* mainLayout = new void(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    
    // Title
    auto* titleLabel = new void("🌡️ RawrXD Thermal Dashboard Pro", this);
    titleLabel->setStyleSheet("font-size: 20px; font-weight: bold; color: #00ff88;");
    mainLayout->addWidget(titleLabel);
    
    // Tab widget
    m_tabWidget = new void(this);
    m_tabWidget->setStyleSheet(R"(
        void::pane {
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
    m_statusLabel = new void("⏳ Initializing thermal monitoring...", this);
    m_statusLabel->setStyleSheet("color: #888; font-style: italic;");
    mainLayout->addWidget(m_statusLabel);
    
    // Dark theme
    setStyleSheet("background-color: #1a1a1a;");
}

void ThermalDashboardEnhanced::setupMainTab()
{
    auto* mainWidget = new void();
    auto* layout = new void(mainWidget);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // NVMe Section
    // ═══════════════════════════════════════════════════════════════════════════
    auto* nvmeGroup = new void("NVMe Drives", mainWidget);
    nvmeGroup->setStyleSheet(R"(
        void {
            border: 2px solid #444;
            border-radius: 8px;
            margin-top: 10px;
            padding-top: 10px;
            font-weight: bold;
            color: #aaa;
        }
        void::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px;
        }
    )");
    
    auto* nvmeLayout = new void(nvmeGroup);
    
    const char* nvmeNames[] = {
        "NVMe0 (SK hynix P41)",
        "NVMe1 (Samsung 990 PRO)",
        "NVMe2 (WD Black SN850X)",
        "NVMe3 (Samsung 990 PRO 4TB)",
        "NVMe4 (Crucial T705 4TB)"
    };
    
    for (int i = 0; i < 5; ++i) {
        auto* row = new void();
        
        m_nvmeWidgets[i].nameLabel = new void(nvmeNames[i], nvmeGroup);
        m_nvmeWidgets[i].nameLabel->setMinimumWidth(180);
        m_nvmeWidgets[i].nameLabel->setStyleSheet("color: #ccc;");
        
        m_nvmeWidgets[i].tempBar = new void(nvmeGroup);
        m_nvmeWidgets[i].tempBar->setRange(0, 100);
        m_nvmeWidgets[i].tempBar->setValue(50);
        m_nvmeWidgets[i].tempBar->setTextVisible(false);
        m_nvmeWidgets[i].tempBar->setFixedHeight(20);
        m_nvmeWidgets[i].tempBar->setStyleSheet(R"(
            void {
                border: 1px solid #555;
                border-radius: 4px;
                background: #222;
            }
            void::chunk {
                border-radius: 3px;
                background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                    stop:0 #00cc66, stop:0.6 #ffcc00, stop:1 #ff3333);
            }
        )");
        
        m_nvmeWidgets[i].tempLabel = new void("--°C", nvmeGroup);
        m_nvmeWidgets[i].tempLabel->setMinimumWidth(60);
        m_nvmeWidgets[i].tempLabel->setAlignment(//AlignRight | //AlignVCenter);
        m_nvmeWidgets[i].tempLabel->setStyleSheet("color: #0f0; font-weight: bold;");
        
        m_nvmeWidgets[i].headroomLabel = new void("(--°C)", nvmeGroup);
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
    auto* systemGroup = new void("System Thermals", mainWidget);
    systemGroup->setStyleSheet(nvmeGroup->styleSheet());
    
    auto* systemLayout = new void(systemGroup);
    
    // GPU
    auto* gpuRow = new void();
    auto* gpuLabel = new void("7800 XT Junction", systemGroup);
    gpuLabel->setMinimumWidth(180);
    gpuLabel->setStyleSheet("color: #ff6666;");
    
    m_gpuTempBar = new void(systemGroup);
    m_gpuTempBar->setRange(0, 110);
    m_gpuTempBar->setValue(65);
    m_gpuTempBar->setTextVisible(false);
    m_gpuTempBar->setFixedHeight(20);
    m_gpuTempBar->setStyleSheet(m_nvmeWidgets[0].tempBar->styleSheet());
    
    m_gpuTempLabel = new void("--°C", systemGroup);
    m_gpuTempLabel->setMinimumWidth(60);
    m_gpuTempLabel->setAlignment(//AlignRight | //AlignVCenter);
    m_gpuTempLabel->setStyleSheet("color: #ff6666; font-weight: bold;");
    
    gpuRow->addWidget(gpuLabel);
    gpuRow->addWidget(m_gpuTempBar, 1);
    gpuRow->addWidget(m_gpuTempLabel);
    systemLayout->addLayout(gpuRow);
    
    // CPU
    auto* cpuRow = new void();
    auto* cpuLabel = new void("7800X3D Package", systemGroup);
    cpuLabel->setMinimumWidth(180);
    cpuLabel->setStyleSheet("color: #6699ff;");
    
    m_cpuTempBar = new void(systemGroup);
    m_cpuTempBar->setRange(0, 95);
    m_cpuTempBar->setValue(55);
    m_cpuTempBar->setTextVisible(false);
    m_cpuTempBar->setFixedHeight(20);
    m_cpuTempBar->setStyleSheet(m_nvmeWidgets[0].tempBar->styleSheet());
    
    m_cpuTempLabel = new void("--°C", systemGroup);
    m_cpuTempLabel->setMinimumWidth(60);
    m_cpuTempLabel->setAlignment(//AlignRight | //AlignVCenter);
    m_cpuTempLabel->setStyleSheet("color: #6699ff; font-weight: bold;");
    
    cpuRow->addWidget(cpuLabel);
    cpuRow->addWidget(m_cpuTempBar, 1);
    cpuRow->addWidget(m_cpuTempLabel);
    systemLayout->addLayout(cpuRow);
    
    layout->addWidget(systemGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Prediction & Throttle Section
    // ═══════════════════════════════════════════════════════════════════════════
    auto* predGroup = new void("Predictive Throttling", mainWidget);
    predGroup->setStyleSheet(nvmeGroup->styleSheet());
    
    auto* predLayout = new void(predGroup);
    
    // Prediction status
    m_predictionStatusLabel = new void("📊 Prediction: Initializing...", predGroup);
    m_predictionStatusLabel->setStyleSheet("color: #00ccff; font-weight: bold;");
    predLayout->addWidget(m_predictionStatusLabel);
    
    // Throttle action
    m_throttleActionLabel = new void("⚡ Action: NONE", predGroup);
    m_throttleActionLabel->setStyleSheet("color: #00ff00; font-weight: bold;");
    predLayout->addWidget(m_throttleActionLabel);
    
    // Throttle bar
    auto* throttleRow = new void();
    auto* throttleLbl = new void("Current Throttle", predGroup);
    throttleLbl->setStyleSheet("color: #ffcc00;");
    throttleLbl->setMinimumWidth(120);
    
    m_throttleBar = new void(predGroup);
    m_throttleBar->setRange(0, 100);
    m_throttleBar->setValue(0);
    m_throttleBar->setTextVisible(false);
    m_throttleBar->setFixedHeight(20);
    m_throttleBar->setStyleSheet(R"(
        void {
            border: 1px solid #555;
            border-radius: 4px;
            background: #222;
        }
        void::chunk {
            border-radius: 3px;
            background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                stop:0 #00ff00, stop:0.5 #ffff00, stop:1 #ff0000);
        }
    )");
    
    m_throttleLabel = new void("0%", predGroup);
    m_throttleLabel->setMinimumWidth(50);
    m_throttleLabel->setAlignment(//AlignRight | //AlignVCenter);
    m_throttleLabel->setStyleSheet("color: #0f0; font-weight: bold;");
    
    throttleRow->addWidget(throttleLbl);
    throttleRow->addWidget(m_throttleBar, 1);
    throttleRow->addWidget(m_throttleLabel);
    predLayout->addLayout(throttleRow);
    
    // Mode selector
    auto* modeRow = new void();
    auto* modeLbl = new void("Burst Mode:", predGroup);
    modeLbl->setStyleSheet("color: #aaa;");
    
    m_burstModeCombo = new void(predGroup);
    m_burstModeCombo->addItem("🚀 SOVEREIGN-MAX (142μs)", 0);
    m_burstModeCombo->addItem("🌡️ THERMAL-GOVERNED (237μs)", 1);
    m_burstModeCombo->addItem("⚡ ADAPTIVE-HYBRID (dynamic)", 2);
    m_burstModeCombo->setCurrentIndex(2);
    m_burstModeCombo->setStyleSheet(R"(
        void {
            background: #333;
            color: #fff;
            border: 1px solid #555;
            border-radius: 4px;
            padding: 5px 10px;
            min-width: 250px;
        }
        void:hover {
            border-color: #00ff88;
        }
    )");
    
    m_applyButton = new void("Apply", predGroup);
    m_applyButton->setStyleSheet(R"(
        void {
            background: #00aa55;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 6px 20px;
            font-weight: bold;
        }
        void:hover {
            background: #00cc66;
        }
    )");
// Qt connect removed
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
    auto* chartsWidget = new void();
    auto* layout = new void(chartsWidget);
    
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
    m_tempChart = nullptr;
    m_tempChart->setTitle("Temperature History & Prediction");
    m_tempChart->setTheme(QChart::ChartThemeDark);
    m_tempChart->setBackgroundBrush(QBrush(uint32_t(26, 26, 26)));
    m_tempChart->setAnimationOptions(QChart::NoAnimation);
    
    // Create series for each sensor
    const uint32_t nvmeColors[] = {
        uint32_t(0, 255, 136),    // Green
        uint32_t(0, 204, 255),    // Cyan
        uint32_t(255, 204, 0),    // Yellow
        uint32_t(255, 102, 102),  // Red
        uint32_t(204, 153, 255)   // Purple
    };
    
    for (int i = 0; i < 5; ++i) {
        m_nvmeTempSeries[i] = nullptr;
        m_nvmeTempSeries[i]->setName(std::string("NVMe%1"));
        m_nvmeTempSeries[i]->setColor(nvmeColors[i]);
        m_tempChart->addSeries(m_nvmeTempSeries[i]);
    }
    
    m_gpuTempSeries = nullptr;
    m_gpuTempSeries->setName("GPU");
    m_gpuTempSeries->setColor(uint32_t(255, 100, 100));
    m_tempChart->addSeries(m_gpuTempSeries);
    
    m_cpuTempSeries = nullptr;
    m_cpuTempSeries->setName("CPU");
    m_cpuTempSeries->setColor(uint32_t(100, 150, 255));
    m_tempChart->addSeries(m_cpuTempSeries);
    
    // Predicted path (dotted line)
    m_predictedTempSeries = nullptr;
    m_predictedTempSeries->setName("Predicted");
    QPen predPen(uint32_t(255, 255, 0));
    predPen.setStyle(//DashLine);
    predPen.setWidth(2);
    m_predictedTempSeries->setPen(predPen);
    m_tempChart->addSeries(m_predictedTempSeries);
    
    // Threshold markers
    m_thresholdMarkers = nullptr;
    m_thresholdMarkers->setName("Threshold");
    m_thresholdMarkers->setMarkerShape(QScatterSeries::MarkerShapeTriangle);
    m_thresholdMarkers->setMarkerSize(10);
    m_thresholdMarkers->setColor(uint32_t(255, 0, 0));
    m_tempChart->addSeries(m_thresholdMarkers);
    
    // Create axes
    m_tempXAxis = nullptr;
    m_tempXAxis->setTitleText("Time (seconds)");
    m_tempXAxis->setRange(0, 300);
    m_tempXAxis->setLabelFormat("%d");
    m_tempChart->addAxis(m_tempXAxis, //AlignBottom);
    
    m_tempYAxis = nullptr;
    m_tempYAxis->setTitleText("Temperature (°C)");
    m_tempYAxis->setRange(30, 90);
    m_tempYAxis->setLabelFormat("%d");
    m_tempChart->addAxis(m_tempYAxis, //AlignLeft);
    
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
    
    m_tempChartView = nullptr;
    m_tempChartView->setRenderHint(QPainter::Antialiasing);
}

void ThermalDashboardEnhanced::createPredictionChart()
{
    m_predChart = nullptr;
    m_predChart->setTitle("Prediction Metrics");
    m_predChart->setTheme(QChart::ChartThemeDark);
    m_predChart->setBackgroundBrush(QBrush(uint32_t(26, 26, 26)));
    m_predChart->setAnimationOptions(QChart::NoAnimation);
    
    m_ewmaSeries = nullptr;
    m_ewmaSeries->setName("EWMA");
    m_ewmaSeries->setColor(uint32_t(0, 255, 136));
    m_predChart->addSeries(m_ewmaSeries);
    
    m_slopeSeries = nullptr;
    m_slopeSeries->setName("Slope (×10)");
    m_slopeSeries->setColor(uint32_t(255, 204, 0));
    m_predChart->addSeries(m_slopeSeries);
    
    m_confidenceSeries = nullptr;
    m_confidenceSeries->setName("Confidence (×100)");
    m_confidenceSeries->setColor(uint32_t(0, 204, 255));
    m_predChart->addSeries(m_confidenceSeries);
    
    m_predChart->createDefaultAxes();
    m_predChart->legend()->setVisible(true);
    
    m_predChartView = nullptr;
    m_predChartView->setRenderHint(QPainter::Antialiasing);
}

void ThermalDashboardEnhanced::setupConfigTab()
{
    auto* configWidget = new void();
    auto* layout = new void(configWidget);
    
    auto* group = new void("Predictive Throttling Configuration", configWidget);
    auto* formLayout = new void(group);
    
    // Alpha (EWMA smoothing factor)
    auto* alphaRow = new void();
    alphaRow->addWidget(new void("EWMA Alpha (responsiveness):"));
    m_alphaSpinBox = nullptr;
    m_alphaSpinBox->setRange(0.1, 0.9);
    m_alphaSpinBox->setSingleStep(0.05);
    m_alphaSpinBox->setValue(0.3);
    alphaRow->addWidget(m_alphaSpinBox);
    formLayout->addLayout(alphaRow);
    
    // History size
    auto* historyRow = new void();
    historyRow->addWidget(new void("History Size (samples):"));
    m_historySizeSpinBox = nullptr;
    m_historySizeSpinBox->setRange(5, 100);
    m_historySizeSpinBox->setValue(20);
    historyRow->addWidget(m_historySizeSpinBox);
    formLayout->addLayout(historyRow);
    
    // Thermal threshold
    auto* thresholdRow = new void();
    thresholdRow->addWidget(new void("Thermal Threshold (°C):"));
    m_thermalThresholdSpinBox = nullptr;
    m_thermalThresholdSpinBox->setRange(40.0, 80.0);
    m_thermalThresholdSpinBox->setSingleStep(1.0);
    m_thermalThresholdSpinBox->setValue(60.0);
    thresholdRow->addWidget(m_thermalThresholdSpinBox);
    formLayout->addLayout(thresholdRow);
    
    // Emergency threshold
    auto* emergencyRow = new void();
    emergencyRow->addWidget(new void("Emergency Threshold (°C):"));
    m_emergencyThresholdSpinBox = nullptr;
    m_emergencyThresholdSpinBox->setRange(60.0, 90.0);
    m_emergencyThresholdSpinBox->setSingleStep(1.0);
    m_emergencyThresholdSpinBox->setValue(75.0);
    emergencyRow->addWidget(m_emergencyThresholdSpinBox);
    formLayout->addLayout(emergencyRow);
    
    // Prediction horizon
    auto* horizonRow = new void();
    horizonRow->addWidget(new void("Prediction Horizon (ms):"));
    m_predictionHorizonSpinBox = nullptr;
    m_predictionHorizonSpinBox->setRange(1000, 30000);
    m_predictionHorizonSpinBox->setSingleStep(500);
    m_predictionHorizonSpinBox->setValue(5000);
// Qt connect removed
    horizonRow->addWidget(m_predictionHorizonSpinBox);
    formLayout->addLayout(horizonRow);
    
    // Predictive enabled
    m_predictiveEnabledCheck = nullptr;
    m_predictiveEnabledCheck->setChecked(true);
    formLayout->addWidget(m_predictiveEnabledCheck);
    
    layout->addWidget(group);
    layout->addStretch();
    
    m_tabWidget->addTab(configWidget, "⚙️ Config");
}

void ThermalDashboardEnhanced::setupControlsTab()
{
    auto* controlsWidget = new void();
    auto* layout = new void(controlsWidget);
    
    // Manual throttle control
    auto* throttleGroup = new void("Manual Throttle Override", controlsWidget);
    auto* throttleLayout = new void(throttleGroup);
    
    m_manualThrottleEnabled = nullptr;
// Qt connect removed
        m_manualThrottleSlider->setEnabled(checked);
    });
    throttleLayout->addWidget(m_manualThrottleEnabled);
    
    auto* sliderRow = new void();
    m_manualThrottleSlider = nullptr;
    m_manualThrottleSlider->setRange(0, 100);
    m_manualThrottleSlider->setValue(0);
    m_manualThrottleSlider->setEnabled(false);
// Qt connect removed
    m_manualThrottleLabel = new void("0%");
    m_manualThrottleLabel->setMinimumWidth(50);
    
    sliderRow->addWidget(m_manualThrottleSlider, 1);
    sliderRow->addWidget(m_manualThrottleLabel);
    throttleLayout->addLayout(sliderRow);
    
    layout->addWidget(throttleGroup);
    
    // Drive override
    auto* driveGroup = new void("Drive Selection Override", controlsWidget);
    auto* driveLayout = new void(driveGroup);
    
    m_driveOverrideCheck = nullptr;
// Qt connect removed
    driveLayout->addWidget(m_driveOverrideCheck);
    
    m_driveOverrideCombo = new void();
    m_driveOverrideCombo->addItem("NVMe0 (SK hynix P41)", 0);
    m_driveOverrideCombo->addItem("NVMe1 (Samsung 990 PRO)", 1);
    m_driveOverrideCombo->addItem("NVMe2 (WD Black SN850X)", 2);
    m_driveOverrideCombo->addItem("NVMe3 (Samsung 990 PRO 4TB)", 3);
    m_driveOverrideCombo->addItem("NVMe4 (Crucial T705 4TB)", 4);
    m_driveOverrideCombo->setEnabled(false);
// Qt connect removed
    driveLayout->addWidget(m_driveOverrideCombo);
    
    // Drive selection table
    m_driveSelectionTable = nullptr;
    m_driveSelectionTable->setHorizontalHeaderLabels({"Drive", "Temp", "Headroom", "Score"});
    m_driveSelectionTable->horizontalHeader()->setStretchLastSection(true);
    driveLayout->addWidget(m_driveSelectionTable);
    
    layout->addWidget(driveGroup);
    
    // Emergency controls
    auto* emergencyGroup = new void("Emergency Controls", controlsWidget);
    auto* emergencyLayout = new void(emergencyGroup);
    
    m_emergencyStopButton = new void("🛑 EMERGENCY STOP");
    m_emergencyStopButton->setStyleSheet(R"(
        void {
            background: #cc0000;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 10px 20px;
            font-weight: bold;
            font-size: 14px;
        }
        void:hover {
            background: #ff0000;
        }
    )");
    
    m_clearEmergencyButton = new void("✓ Clear Emergency");
    m_clearEmergencyButton->setStyleSheet(R"(
        void {
            background: #00aa55;
            color: white;
            border: none;
            border-radius: 4px;
            padding: 10px 20px;
            font-weight: bold;
        }
        void:hover {
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
    m_statusLabel->setText(std::string("✓ Last update: %1 | %2 drives active")
        .toString("hh:mm:ss"))
        );
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
    
    m_startTime = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
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
    
    burstModeChanged(mode);
}

void ThermalDashboardEnhanced::onThrottleSliderChanged(int value)
{
    m_manualThrottleLabel->setText(std::string("%1%"));
    
    if (m_manualThrottleActive) {
        if (m_sharedMemory && m_sharedMemory->isOpen()) {
            m_sharedMemory->setThrottlePercent(value);
        }
        throttleAdjusted(value);
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
        driveSelected(index);
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
    int64_t now = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
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
    m_nvmeWidgets[index].tempLabel->setText(std::string("%1°C"));
    m_nvmeWidgets[index].tempLabel->setStyleSheet(
        std::string("color: %1; font-weight: bold;")));
    
    // Calculate headroom
    double headroom = 70.0 - temp;  // Assuming 70°C max
    m_nvmeWidgets[index].headroomLabel->setText(std::string("(%1°C)"));
    
    std::string headroomColor = (headroom > 15) ? "#00ff00" : (headroom > 5) ? "#ffcc00" : "#ff3333";
    m_nvmeWidgets[index].headroomLabel->setStyleSheet(std::string("color: %1;"));
}

void ThermalDashboardEnhanced::updateGPUDisplay(float temp)
{
    m_gpuTempBar->setValue(static_cast<int>(temp));
    m_gpuTempLabel->setText(std::string("%1°C"));
    m_gpuTempLabel->setStyleSheet(
        std::string("color: %1; font-weight: bold;")));
}

void ThermalDashboardEnhanced::updateCPUDisplay(float temp)
{
    m_cpuTempBar->setValue(static_cast<int>(temp));
    m_cpuTempLabel->setText(std::string("%1°C"));
    m_cpuTempLabel->setStyleSheet(
        std::string("color: %1; font-weight: bold;")));
}

void ThermalDashboardEnhanced::updateThrottleDisplay(int throttle)
{
    m_throttleBar->setValue(throttle);
    m_throttleLabel->setText(std::string("%1%"));
    
    std::string color;
    if (throttle == 0) {
        color = "#00ff00";
    } else if (throttle < 20) {
        color = "#88ff00";
    } else if (throttle < 40) {
        color = "#ffcc00";
    } else {
        color = "#ff3333";
    }
    m_throttleLabel->setStyleSheet(std::string("color: %1; font-weight: bold;"));
}

void ThermalDashboardEnhanced::updatePredictionDisplay()
{
    PredictionResult prediction = m_predictor->getPrediction();
    
    std::string statusText = std::string("📊 Prediction: %1°C (±%2°C) | Confidence: %3% | Slope: %4°C/s")
        
         * 5.0, 0, 'f', 1)  // Uncertainty estimate
        
        ;
    
    m_predictionStatusLabel->setText(statusText);
    
    // Update action label
    ThrottleAction action = m_predictor->getRecommendedAction(prediction.predictedTemp);
    m_throttleActionLabel->setText(std::string("⚡ Action: %1")));
    m_throttleActionLabel->setStyleSheet(
        std::string("color: %1; font-weight: bold;").name()));
}

void ThermalDashboardEnhanced::updateLoadBalancerDisplay()
{
    DriveSelectionResult result = m_loadBalancer->selectOptimalDriveDetailed();
    
    // Update table
    for (int i = 0; i < 5; ++i) {
        auto info = m_loadBalancer->getDriveInfo(i);
        if (!info.has_value()) continue;
        
        m_driveSelectionTable->setItem(i, 0, nullptr));
        m_driveSelectionTable->setItem(i, 1, nullptr));
        m_driveSelectionTable->setItem(i, 2, nullptr));
        m_driveSelectionTable->setItem(i, 3, nullptr));
        
        // Highlight selected drive
        if (i == result.selectedDrive) {
            for (int j = 0; j < 4; ++j) {
                m_driveSelectionTable->item(i, j)->setBackground(uint32_t(0, 100, 50));
            }
        }
    }
}

void ThermalDashboardEnhanced::updateTemperatureChart(const ThermalSnapshot& snapshot)
{
    int64_t now = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
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

std::string ThermalDashboardEnhanced::getTempColor(float temp)
{
    if (temp < 55) return "#00ff00";
    if (temp < 65) return "#88ff00";
    if (temp < 72) return "#ffcc00";
    if (temp < 80) return "#ff8800";
    return "#ff3333";
}

std::string ThermalDashboardEnhanced::getThrottleActionString(ThrottleAction action)
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

uint32_t ThermalDashboardEnhanced::getThrottleActionColor(ThrottleAction action)
{
    switch (action) {
        case ThrottleAction::NONE:      return uint32_t(0, 255, 0);
        case ThrottleAction::LIGHT:     return uint32_t(136, 255, 0);
        case ThrottleAction::MODERATE:  return uint32_t(255, 204, 0);
        case ThrottleAction::HEAVY:     return uint32_t(255, 136, 0);
        case ThrottleAction::EMERGENCY: return uint32_t(255, 51, 51);
        default:                        return uint32_t(128, 128, 128);
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalCompactWidgetEnhanced Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ThermalCompactWidgetEnhanced::ThermalCompactWidgetEnhanced(void* parent)
    : void(parent)
{
    setupUI();
}

void ThermalCompactWidgetEnhanced::setupUI()
{
    setFrameStyle(void::StyledPanel | void::Raised);
    setStyleSheet(R"(
        void {
            background: #2a2a2a;
            border: 1px solid #444;
            border-radius: 4px;
            padding: 4px;
        }
    )");
    
    auto* layout = new void(this);
    layout->setSpacing(10);
    layout->setContentsMargins(8, 4, 8, 4);
    
    // Current temp
    m_maxTempLabel = new void("🌡️ --°C", this);
    m_maxTempLabel->setStyleSheet("color: #0f0; font-weight: bold;");
    layout->addWidget(m_maxTempLabel);
    
    // Predicted temp
    m_predictedTempLabel = new void("→ --°C", this);
    m_predictedTempLabel->setToolTip("Predicted temperature");
    m_predictedTempLabel->setStyleSheet("color: #ff0; font-weight: bold;");
    layout->addWidget(m_predictedTempLabel);
    
    // Throttle icon
    m_throttleIcon = new void("⚡", this);
    m_throttleIcon->setToolTip("Throttle status");
    layout->addWidget(m_throttleIcon);
    
    // Mode icon
    m_modeIcon = new void("🔄", this);
    m_modeIcon->setToolTip("Adaptive Hybrid mode");
    layout->addWidget(m_modeIcon);
    
    // Drive icon
    m_driveIcon = new void("💾0", this);
    m_driveIcon->setToolTip("Selected drive");
    layout->addWidget(m_driveIcon);
    
    // Expand button
    m_expandButton = new void("⊞", this);
    m_expandButton->setFixedSize(24, 24);
    m_expandButton->setStyleSheet(R"(
        void {
            background: #444;
            color: #fff;
            border: none;
            border-radius: 4px;
        }
        void:hover {
            background: #555;
        }
    )");
// Qt connect removed
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
    
    std::string color = (maxTemp < 65) ? "#00ff00" : (maxTemp < 75) ? "#ffcc00" : "#ff3333";
    m_maxTempLabel->setText(std::string("🌡️ %1°C"));
    m_maxTempLabel->setStyleSheet(std::string("color: %1; font-weight: bold;"));
    
    // Throttle icon
    if (snapshot.currentThrottle == 0) {
        m_throttleIcon->setText("⚡");
        m_throttleIcon->setToolTip("Full speed");
    } else if (snapshot.currentThrottle < 30) {
        m_throttleIcon->setText("🔋");
        m_throttleIcon->setToolTip(std::string("Light throttle: %1%"));
    } else {
        m_throttleIcon->setText("🐢");
        m_throttleIcon->setToolTip(std::string("Heavy throttle: %1%"));
    }
}

void ThermalCompactWidgetEnhanced::onPredictionUpdate(const PredictionResult& prediction)
{
    std::string color = (prediction.predictedTemp < 65) ? "#ffff00" : 
                    (prediction.predictedTemp < 75) ? "#ff8800" : "#ff3333";
    
    m_predictedTempLabel->setText(std::string("→ %1°C"));
    m_predictedTempLabel->setStyleSheet(std::string("color: %1; font-weight: bold;"));
    m_predictedTempLabel->setToolTip(std::string("Predicted in %1ms (confidence: %2%)")
        
        );
}

} // namespace rawrxd::thermal


