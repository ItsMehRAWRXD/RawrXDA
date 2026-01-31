/**
 * @file RAWRXD_ThermalDashboard.hpp
 * @brief Qt6 Thermal Dashboard UI Widgets
 */

#pragma once

#include "thermal_dashboard_plugin.hpp"


namespace rawrxd::thermal {

/**
 * @brief Full thermal dashboard widget
 */
class ThermalDashboard : public void {

public:
    explicit ThermalDashboard(void* parent = nullptr);
    ~ThermalDashboard() override = default;

public:
    void onThermalUpdate(const ThermalSnapshot& snapshot);

    void burstModeChanged(int mode);

private:
    void setupUI();
    void updateNVMeDisplay(int index, float temp);
    void updateGPUDisplay(float temp);
    void updateCPUDisplay(float temp);
    void updateThrottleDisplay(int throttle);
    std::string getTempColor(float temp);

private:
    // NVMe displays
    struct NVMeWidget {
        QLabel* nameLabel;
        QProgressBar* tempBar;
        QLabel* tempLabel;
    };
    NVMeWidget m_nvmeWidgets[5];
    
    // GPU/CPU displays
    QProgressBar* m_gpuTempBar;
    QLabel* m_gpuTempLabel;
    QProgressBar* m_cpuTempBar;
    QLabel* m_cpuTempLabel;
    
    // Throttle display
    QProgressBar* m_throttleBar;
    QLabel* m_throttleLabel;
    
    // Burst mode control
    QComboBox* m_burstModeCombo;
    QPushButton* m_applyButton;
    
    // Status
    QLabel* m_statusLabel;
};

/**
 * @brief Compact toolbar widget for thermal status
 */
class ThermalCompactWidget : public QFrame {

public:
    explicit ThermalCompactWidget(void* parent = nullptr);
    ~ThermalCompactWidget() override = default;

public:
    void onThermalUpdate(const ThermalSnapshot& snapshot);

private:
    void setupUI();

private:
    QLabel* m_maxTempLabel;
    QLabel* m_throttleIcon;
    QLabel* m_modeIcon;
};

} // namespace rawrxd::thermal

