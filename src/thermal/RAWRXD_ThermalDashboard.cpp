/**
 * @file RAWRXD_ThermalDashboard.cpp
 * @brief Win32 Thermal Dashboard UI Implementation (Qt-free)
 */

#include "RAWRXD_ThermalDashboard.hpp"
#include "thermal_dashboard_plugin.hpp"

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalDashboard Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ThermalDashboard::ThermalDashboard(void* parent)
    : void(parent)
{
    setupUI();
}

void ThermalDashboard::setupUI()
{
    auto* mainLayout = new void(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // Title
    auto* titleLabel = new void("🌡️ RawrXD Thermal Dashboard", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #00ff88;");
    mainLayout->addWidget(titleLabel);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // NVMe Section
    // ═══════════════════════════════════════════════════════════════════════════
    auto* nvmeGroup = new void("NVMe Drives", this);
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
        
        m_nvmeWidgets[i].nameLabel = new void(nvmeNames[i], this);
        m_nvmeWidgets[i].nameLabel->setMinimumWidth(180);
        m_nvmeWidgets[i].nameLabel->setStyleSheet("color: #ccc;");
        
        m_nvmeWidgets[i].tempBar = new void(this);
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
        
        m_nvmeWidgets[i].tempLabel = new void("--°C", this);
        m_nvmeWidgets[i].tempLabel->setMinimumWidth(60);
        m_nvmeWidgets[i].tempLabel->setAlignment(//AlignRight | //AlignVCenter);
        m_nvmeWidgets[i].tempLabel->setStyleSheet("color: #0f0; font-weight: bold;");
        
        row->addWidget(m_nvmeWidgets[i].nameLabel);
        row->addWidget(m_nvmeWidgets[i].tempBar, 1);
        row->addWidget(m_nvmeWidgets[i].tempLabel);
        
        nvmeLayout->addLayout(row);
    }
    
    mainLayout->addWidget(nvmeGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // GPU/CPU Section
    // ═══════════════════════════════════════════════════════════════════════════
    auto* systemGroup = new void("System Thermals", this);
    systemGroup->setStyleSheet(nvmeGroup->styleSheet());
    
    auto* systemLayout = new void(systemGroup);
    
    // GPU
    auto* gpuRow = new void();
    auto* gpuLabel = new void("7800 XT Junction", this);
    gpuLabel->setMinimumWidth(180);
    gpuLabel->setStyleSheet("color: #ff6666;");
    
    m_gpuTempBar = new void(this);
    m_gpuTempBar->setRange(0, 110);
    m_gpuTempBar->setValue(65);
    m_gpuTempBar->setTextVisible(false);
    m_gpuTempBar->setFixedHeight(20);
    m_gpuTempBar->setStyleSheet(m_nvmeWidgets[0].tempBar->styleSheet());
    
    m_gpuTempLabel = new void("--°C", this);
    m_gpuTempLabel->setMinimumWidth(60);
    m_gpuTempLabel->setAlignment(//AlignRight | //AlignVCenter);
    m_gpuTempLabel->setStyleSheet("color: #ff6666; font-weight: bold;");
    
    gpuRow->addWidget(gpuLabel);
    gpuRow->addWidget(m_gpuTempBar, 1);
    gpuRow->addWidget(m_gpuTempLabel);
    systemLayout->addLayout(gpuRow);
    
    // CPU
    auto* cpuRow = new void();
    auto* cpuLabel = new void("7800X3D Package", this);
    cpuLabel->setMinimumWidth(180);
    cpuLabel->setStyleSheet("color: #6699ff;");
    
    m_cpuTempBar = new void(this);
    m_cpuTempBar->setRange(0, 95);
    m_cpuTempBar->setValue(55);
    m_cpuTempBar->setTextVisible(false);
    m_cpuTempBar->setFixedHeight(20);
    m_cpuTempBar->setStyleSheet(m_nvmeWidgets[0].tempBar->styleSheet());
    
    m_cpuTempLabel = new void("--°C", this);
    m_cpuTempLabel->setMinimumWidth(60);
    m_cpuTempLabel->setAlignment(//AlignRight | //AlignVCenter);
    m_cpuTempLabel->setStyleSheet("color: #6699ff; font-weight: bold;");
    
    cpuRow->addWidget(cpuLabel);
    cpuRow->addWidget(m_cpuTempBar, 1);
    cpuRow->addWidget(m_cpuTempLabel);
    systemLayout->addLayout(cpuRow);
    
    mainLayout->addWidget(systemGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Throttle Status
    // ═══════════════════════════════════════════════════════════════════════════
    auto* throttleGroup = new void("Burst Governor", this);
    throttleGroup->setStyleSheet(nvmeGroup->styleSheet());
    
    auto* throttleLayout = new void(throttleGroup);
    
    // Throttle bar
    auto* throttleRow = new void();
    auto* throttleLbl = new void("Current Throttle", this);
    throttleLbl->setStyleSheet("color: #ffcc00;");
    throttleLbl->setMinimumWidth(180);
    
    m_throttleBar = new void(this);
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
    
    m_throttleLabel = new void("0%", this);
    m_throttleLabel->setMinimumWidth(60);
    m_throttleLabel->setAlignment(//AlignRight | //AlignVCenter);
    m_throttleLabel->setStyleSheet("color: #0f0; font-weight: bold;");
    
    throttleRow->addWidget(throttleLbl);
    throttleRow->addWidget(m_throttleBar, 1);
    throttleRow->addWidget(m_throttleLabel);
    throttleLayout->addLayout(throttleRow);
    
    // Mode selector
    auto* modeRow = new void();
    auto* modeLbl = new void("Burst Mode:", this);
    modeLbl->setStyleSheet("color: #aaa;");
    
    m_burstModeCombo = new void(this);
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
        void::drop-down {
            border: none;
            width: 20px;
        }
    )");
    
    m_applyButton = new void("Apply", this);
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
        void:pressed {
            background: #008844;
        }
    )");
// Qt connect removed
        burstModeChanged(mode);
    });
    
    modeRow->addWidget(modeLbl);
    modeRow->addWidget(m_burstModeCombo, 1);
    modeRow->addWidget(m_applyButton);
    throttleLayout->addLayout(modeRow);
    
    mainLayout->addWidget(throttleGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Status Bar
    // ═══════════════════════════════════════════════════════════════════════════
    m_statusLabel = new void("⏳ Initializing thermal monitoring...", this);
    m_statusLabel->setStyleSheet("color: #888; font-style: italic;");
    mainLayout->addWidget(m_statusLabel);
    
    mainLayout->addStretch();
    
    // Dark theme
    setStyleSheet("background-color: #1a1a1a;");
}

void ThermalDashboard::onThermalUpdate(const ThermalSnapshot& snapshot)
{
    // Update NVMe displays
    for (int i = 0; i < snapshot.activeDriveCount && i < 5; ++i) {
        updateNVMeDisplay(i, snapshot.nvmeTemps[i]);
    }
    
    // Hide unused drives
    for (int i = snapshot.activeDriveCount; i < 5; ++i) {
        m_nvmeWidgets[i].nameLabel->setVisible(false);
        m_nvmeWidgets[i].tempBar->setVisible(false);
        m_nvmeWidgets[i].tempLabel->setVisible(false);
    }
    
    // Update GPU/CPU
    updateGPUDisplay(snapshot.gpuTemp);
    updateCPUDisplay(snapshot.cpuTemp);
    
    // Update throttle
    updateThrottleDisplay(snapshot.currentThrottle);
    
    // Status
    m_statusLabel->setText(std::string("✓ Last update: %1 | %2 drives active")
        .toString("hh:mm:ss"))
        );
}

void ThermalDashboard::updateNVMeDisplay(int index, float temp)
{
    if (index < 0 || index >= 5) return;
    
    m_nvmeWidgets[index].tempBar->setValue(static_cast<int>(temp));
    m_nvmeWidgets[index].tempLabel->setText(std::string("%1°C"));
    m_nvmeWidgets[index].tempLabel->setStyleSheet(
        std::string("color: %1; font-weight: bold;")));
}

void ThermalDashboard::updateGPUDisplay(float temp)
{
    m_gpuTempBar->setValue(static_cast<int>(temp));
    m_gpuTempLabel->setText(std::string("%1°C"));
    m_gpuTempLabel->setStyleSheet(
        std::string("color: %1; font-weight: bold;")));
}

void ThermalDashboard::updateCPUDisplay(float temp)
{
    m_cpuTempBar->setValue(static_cast<int>(temp));
    m_cpuTempLabel->setText(std::string("%1°C"));
    m_cpuTempLabel->setStyleSheet(
        std::string("color: %1; font-weight: bold;")));
}

void ThermalDashboard::updateThrottleDisplay(int throttle)
{
    m_throttleBar->setValue(throttle);
    m_throttleLabel->setText(std::string("%1%"));
    
    std::string color;
    if (throttle == 0) {
        color = "#00ff00";  // Green: full speed
    } else if (throttle < 20) {
        color = "#88ff00";  // Light green
    } else if (throttle < 40) {
        color = "#ffcc00";  // Yellow
    } else {
        color = "#ff3333";  // Red: heavy throttle
    }
    m_throttleLabel->setStyleSheet(std::string("color: %1; font-weight: bold;"));
}

std::string ThermalDashboard::getTempColor(float temp)
{
    if (temp < 55) return "#00ff00";       // Green
    if (temp < 65) return "#88ff00";       // Light green
    if (temp < 72) return "#ffcc00";       // Yellow
    if (temp < 80) return "#ff8800";       // Orange
    return "#ff3333";                       // Red
}

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalCompactWidget Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ThermalCompactWidget::ThermalCompactWidget(void* parent)
    : void(parent)
{
    setupUI();
}

void ThermalCompactWidget::setupUI()
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
    layout->setSpacing(8);
    layout->setContentsMargins(8, 4, 8, 4);
    
    // Temp icon + value
    m_maxTempLabel = new void("🌡️ --°C", this);
    m_maxTempLabel->setStyleSheet("color: #0f0; font-weight: bold;");
    layout->addWidget(m_maxTempLabel);
    
    // Throttle status icon
    m_throttleIcon = new void("⚡", this);
    m_throttleIcon->setToolTip("Throttle status");
    layout->addWidget(m_throttleIcon);
    
    // Mode icon
    m_modeIcon = new void("🔄", this);
    m_modeIcon->setToolTip("Adaptive Hybrid mode");
    layout->addWidget(m_modeIcon);
    
    setFixedHeight(32);
}

void ThermalCompactWidget::onThermalUpdate(const ThermalSnapshot& snapshot)
{
    // Find max temp
    float maxTemp = snapshot.gpuTemp;
    for (int i = 0; i < snapshot.activeDriveCount; ++i) {
        maxTemp = qMax(maxTemp, snapshot.nvmeTemps[i]);
    }
    maxTemp = qMax(maxTemp, snapshot.cpuTemp);
    
    // Update display
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

} // namespace rawrxd::thermal

