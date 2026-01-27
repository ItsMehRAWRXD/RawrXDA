/**
 * @file RAWRXD_ThermalDashboard.cpp
 * @brief Qt6 Thermal Dashboard UI Implementation
 */

#include "RAWRXD_ThermalDashboard.hpp"
#include "thermal_dashboard_plugin.hpp"
#include <QStyle>
#include <QApplication>

namespace rawrxd::thermal {

// ═══════════════════════════════════════════════════════════════════════════════
// ThermalDashboard Implementation
// ═══════════════════════════════════════════════════════════════════════════════

ThermalDashboard::ThermalDashboard(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
}

void ThermalDashboard::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    
    // Title
    auto* titleLabel = new QLabel("🌡️ RawrXD Thermal Dashboard", this);
    titleLabel->setStyleSheet("font-size: 18px; font-weight: bold; color: #00ff88;");
    mainLayout->addWidget(titleLabel);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // NVMe Section
    // ═══════════════════════════════════════════════════════════════════════════
    auto* nvmeGroup = new QGroupBox("NVMe Drives", this);
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
        
        m_nvmeWidgets[i].nameLabel = new QLabel(nvmeNames[i], this);
        m_nvmeWidgets[i].nameLabel->setMinimumWidth(180);
        m_nvmeWidgets[i].nameLabel->setStyleSheet("color: #ccc;");
        
        m_nvmeWidgets[i].tempBar = new QProgressBar(this);
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
        
        m_nvmeWidgets[i].tempLabel = new QLabel("--°C", this);
        m_nvmeWidgets[i].tempLabel->setMinimumWidth(60);
        m_nvmeWidgets[i].tempLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
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
    auto* systemGroup = new QGroupBox("System Thermals", this);
    systemGroup->setStyleSheet(nvmeGroup->styleSheet());
    
    auto* systemLayout = new QVBoxLayout(systemGroup);
    
    // GPU
    auto* gpuRow = new QHBoxLayout();
    auto* gpuLabel = new QLabel("7800 XT Junction", this);
    gpuLabel->setMinimumWidth(180);
    gpuLabel->setStyleSheet("color: #ff6666;");
    
    m_gpuTempBar = new QProgressBar(this);
    m_gpuTempBar->setRange(0, 110);
    m_gpuTempBar->setValue(65);
    m_gpuTempBar->setTextVisible(false);
    m_gpuTempBar->setFixedHeight(20);
    m_gpuTempBar->setStyleSheet(m_nvmeWidgets[0].tempBar->styleSheet());
    
    m_gpuTempLabel = new QLabel("--°C", this);
    m_gpuTempLabel->setMinimumWidth(60);
    m_gpuTempLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_gpuTempLabel->setStyleSheet("color: #ff6666; font-weight: bold;");
    
    gpuRow->addWidget(gpuLabel);
    gpuRow->addWidget(m_gpuTempBar, 1);
    gpuRow->addWidget(m_gpuTempLabel);
    systemLayout->addLayout(gpuRow);
    
    // CPU
    auto* cpuRow = new QHBoxLayout();
    auto* cpuLabel = new QLabel("7800X3D Package", this);
    cpuLabel->setMinimumWidth(180);
    cpuLabel->setStyleSheet("color: #6699ff;");
    
    m_cpuTempBar = new QProgressBar(this);
    m_cpuTempBar->setRange(0, 95);
    m_cpuTempBar->setValue(55);
    m_cpuTempBar->setTextVisible(false);
    m_cpuTempBar->setFixedHeight(20);
    m_cpuTempBar->setStyleSheet(m_nvmeWidgets[0].tempBar->styleSheet());
    
    m_cpuTempLabel = new QLabel("--°C", this);
    m_cpuTempLabel->setMinimumWidth(60);
    m_cpuTempLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_cpuTempLabel->setStyleSheet("color: #6699ff; font-weight: bold;");
    
    cpuRow->addWidget(cpuLabel);
    cpuRow->addWidget(m_cpuTempBar, 1);
    cpuRow->addWidget(m_cpuTempLabel);
    systemLayout->addLayout(cpuRow);
    
    mainLayout->addWidget(systemGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Throttle Status
    // ═══════════════════════════════════════════════════════════════════════════
    auto* throttleGroup = new QGroupBox("Burst Governor", this);
    throttleGroup->setStyleSheet(nvmeGroup->styleSheet());
    
    auto* throttleLayout = new QVBoxLayout(throttleGroup);
    
    // Throttle bar
    auto* throttleRow = new QHBoxLayout();
    auto* throttleLbl = new QLabel("Current Throttle", this);
    throttleLbl->setStyleSheet("color: #ffcc00;");
    throttleLbl->setMinimumWidth(180);
    
    m_throttleBar = new QProgressBar(this);
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
    
    m_throttleLabel = new QLabel("0%", this);
    m_throttleLabel->setMinimumWidth(60);
    m_throttleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_throttleLabel->setStyleSheet("color: #0f0; font-weight: bold;");
    
    throttleRow->addWidget(throttleLbl);
    throttleRow->addWidget(m_throttleBar, 1);
    throttleRow->addWidget(m_throttleLabel);
    throttleLayout->addLayout(throttleRow);
    
    // Mode selector
    auto* modeRow = new QHBoxLayout();
    auto* modeLbl = new QLabel("Burst Mode:", this);
    modeLbl->setStyleSheet("color: #aaa;");
    
    m_burstModeCombo = new QComboBox(this);
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
        QComboBox::drop-down {
            border: none;
            width: 20px;
        }
    )");
    
    m_applyButton = new QPushButton("Apply", this);
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
        QPushButton:pressed {
            background: #008844;
        }
    )");
    
    connect(m_applyButton, &QPushButton::clicked, this, [this]() {
        int mode = m_burstModeCombo->currentData().toInt();
        emit burstModeChanged(mode);
    });
    
    modeRow->addWidget(modeLbl);
    modeRow->addWidget(m_burstModeCombo, 1);
    modeRow->addWidget(m_applyButton);
    throttleLayout->addLayout(modeRow);
    
    mainLayout->addWidget(throttleGroup);
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Status Bar
    // ═══════════════════════════════════════════════════════════════════════════
    m_statusLabel = new QLabel("⏳ Initializing thermal monitoring...", this);
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
    m_statusLabel->setText(QString("✓ Last update: %1 | %2 drives active")
        .arg(QDateTime::fromMSecsSinceEpoch(snapshot.timestamp).toString("hh:mm:ss"))
        .arg(snapshot.activeDriveCount));
}

void ThermalDashboard::updateNVMeDisplay(int index, float temp)
{
    if (index < 0 || index >= 5) return;
    
    m_nvmeWidgets[index].tempBar->setValue(static_cast<int>(temp));
    m_nvmeWidgets[index].tempLabel->setText(QString("%1°C").arg(temp, 0, 'f', 1));
    m_nvmeWidgets[index].tempLabel->setStyleSheet(
        QString("color: %1; font-weight: bold;").arg(getTempColor(temp)));
}

void ThermalDashboard::updateGPUDisplay(float temp)
{
    m_gpuTempBar->setValue(static_cast<int>(temp));
    m_gpuTempLabel->setText(QString("%1°C").arg(temp, 0, 'f', 1));
    m_gpuTempLabel->setStyleSheet(
        QString("color: %1; font-weight: bold;").arg(getTempColor(temp)));
}

void ThermalDashboard::updateCPUDisplay(float temp)
{
    m_cpuTempBar->setValue(static_cast<int>(temp));
    m_cpuTempLabel->setText(QString("%1°C").arg(temp, 0, 'f', 1));
    m_cpuTempLabel->setStyleSheet(
        QString("color: %1; font-weight: bold;").arg(getTempColor(temp)));
}

void ThermalDashboard::updateThrottleDisplay(int throttle)
{
    m_throttleBar->setValue(throttle);
    m_throttleLabel->setText(QString("%1%").arg(throttle));
    
    QString color;
    if (throttle == 0) {
        color = "#00ff00";  // Green: full speed
    } else if (throttle < 20) {
        color = "#88ff00";  // Light green
    } else if (throttle < 40) {
        color = "#ffcc00";  // Yellow
    } else {
        color = "#ff3333";  // Red: heavy throttle
    }
    m_throttleLabel->setStyleSheet(QString("color: %1; font-weight: bold;").arg(color));
}

QString ThermalDashboard::getTempColor(float temp)
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

ThermalCompactWidget::ThermalCompactWidget(QWidget* parent)
    : QFrame(parent)
{
    setupUI();
}

void ThermalCompactWidget::setupUI()
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
    layout->setSpacing(8);
    layout->setContentsMargins(8, 4, 8, 4);
    
    // Temp icon + value
    m_maxTempLabel = new QLabel("🌡️ --°C", this);
    m_maxTempLabel->setStyleSheet("color: #0f0; font-weight: bold;");
    layout->addWidget(m_maxTempLabel);
    
    // Throttle status icon
    m_throttleIcon = new QLabel("⚡", this);
    m_throttleIcon->setToolTip("Throttle status");
    layout->addWidget(m_throttleIcon);
    
    // Mode icon
    m_modeIcon = new QLabel("🔄", this);
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

} // namespace rawrxd::thermal
