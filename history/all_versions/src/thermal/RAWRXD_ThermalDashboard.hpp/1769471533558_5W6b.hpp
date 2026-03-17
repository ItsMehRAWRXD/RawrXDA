/**
 * @file RAWRXD_ThermalDashboard.hpp
 * @brief Qt6 Thermal Dashboard UI Widgets
 */

#pragma once

#include "thermal_dashboard_plugin.hpp"
#include <QWidget>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTimer>
#include <QFrame>
#include <QDateTime>

namespace rawrxd::thermal {

/**
 * @brief Full thermal dashboard widget
 */
class ThermalDashboard : public QWidget {
    Q_OBJECT

public:
    explicit ThermalDashboard(QWidget* parent = nullptr);
    ~ThermalDashboard() override = default;

public slots:
    void onThermalUpdate(const ThermalSnapshot& snapshot);

signals:
    void burstModeChanged(int mode);

private:
    void setupUI();
    void updateNVMeDisplay(int index, float temp);
    void updateGPUDisplay(float temp);
    void updateCPUDisplay(float temp);
    void updateThrottleDisplay(int throttle);
    QString getTempColor(float temp);

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
    Q_OBJECT

public:
    explicit ThermalCompactWidget(QWidget* parent = nullptr);
    ~ThermalCompactWidget() override = default;

public slots:
    void onThermalUpdate(const ThermalSnapshot& snapshot);

private:
    void setupUI();

private:
    QLabel* m_maxTempLabel;
    QLabel* m_throttleIcon;
    QLabel* m_modeIcon;
};

} // namespace rawrxd::thermal
