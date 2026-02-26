/**
 * @file RAWRXD_ThermalDashboard.hpp
 * @brief Win32 Thermal Dashboard UI (Qt-free)
 */

#pragma once

#include "thermal_dashboard_plugin.hpp"
#include "RawrXD_Window.h"


namespace rawrxd::thermal {

/**
 * @brief Full thermal dashboard widget
 */
class ThermalDashboard : public RawrXD::Window {

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
        void* nameLabel;
        void* tempBar;
        void* tempLabel;
    };
    NVMeWidget m_nvmeWidgets[5];
    
    // GPU/CPU displays
    void* m_gpuTempBar;
    void* m_gpuTempLabel;
    void* m_cpuTempBar;
    void* m_cpuTempLabel;
    
    // Throttle display
    void* m_throttleBar;
    void* m_throttleLabel;
    
    // Burst mode control
    void* m_burstModeCombo;
    void* m_applyButton;
    
    // Status
    void* m_statusLabel;
};

/**
 * @brief Compact toolbar widget for thermal status
 */
class ThermalCompactWidget : public RawrXD::Window {

public:
    explicit ThermalCompactWidget(void* parent = nullptr);
    ~ThermalCompactWidget() override = default;

public:
    void onThermalUpdate(const ThermalSnapshot& snapshot);

private:
    void setupUI();

private:
    void* m_maxTempLabel;
    void* m_throttleIcon;
    void* m_modeIcon;
};

} // namespace rawrxd::thermal

