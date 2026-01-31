/**
 * @file RAWRXD_ThermalDashboard_Enhanced.hpp
 * @brief Enhanced Qt6 Thermal Dashboard with Predictive Visualization
 * 
 * Adds QtCharts predicted temperature path visualization and
 * integration with PredictiveThrottling engine.
 * 
 * @copyright RawrXD IDE 2026
 */

#pragma once

#include "thermal_dashboard_plugin.hpp"
#include "PredictiveThrottling.h"
#include "DynamicLoadBalancer.h"
#include "SovereignControlBlock.h"


// QtCharts


#include <memory>
#include <vector>
#include <deque>

namespace rawrxd::thermal {

// Forward declarations
class PredictiveThrottling;
class DynamicLoadBalancer;
class SharedMemoryManager;

/**
 * @brief Data point for temperature history
 */
struct TemperatureDataPoint {
    qint64 timestamp;
    double temperature;
};

/**
 * @brief Enhanced Thermal Dashboard with Predictive Visualization
 */
class ThermalDashboardEnhanced : public void {

public:
    explicit ThermalDashboardEnhanced(void* parent = nullptr);
    ~ThermalDashboardEnhanced() override;

public:
    /**
     * @brief Handle thermal snapshot update
     * @param snapshot Current thermal data
     */
    void onThermalUpdate(const ThermalSnapshot& snapshot);
    
    /**
     * @brief Force chart refresh
     */
    void refreshCharts();
    
    /**
     * @brief Clear all historical data
     */
    void clearHistory();

    /**
     * @brief Emitted when burst mode changes
     * @param mode New burst mode (0=sovereign, 1=thermal, 2=hybrid)
     */
    void burstModeChanged(int mode);
    
    /**
     * @brief Emitted when throttle level manually adjusted
     * @param percent New throttle percentage
     */
    void throttleAdjusted(int percent);
    
    /**
     * @brief Emitted when drive selection is manually overridden
     * @param driveIndex Selected drive index
     */
    void driveSelected(int driveIndex);

private:
    void onBurstModeApply();
    void onThrottleSliderChanged(int value);
    void onDriveOverrideToggled(bool checked);
    void onManualDriveSelected(int index);
    void onPredictionHorizonChanged(int value);
    void updatePredictedPath();

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // UI Setup
    // ═══════════════════════════════════════════════════════════════════════════
    
    void setupUI();
    void setupMainTab();
    void setupChartsTab();
    void setupConfigTab();
    void setupControlsTab();
    void createTemperatureChart();
    void createLoadBalancerChart();
    void createPredictionChart();
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Display Updates
    // ═══════════════════════════════════════════════════════════════════════════
    
    void updateNVMeDisplay(int index, float temp);
    void updateGPUDisplay(float temp);
    void updateCPUDisplay(float temp);
    void updateThrottleDisplay(int throttle);
    void updatePredictionDisplay();
    void updateLoadBalancerDisplay();
    void updateTemperatureChart(const ThermalSnapshot& snapshot);
    void updateDriveSelectionTable();
    
    std::string getTempColor(float temp);
    std::string getThrottleActionString(ThrottleAction action);
    QColor getThrottleActionColor(ThrottleAction action);

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // UI Components - Main Tab
    // ═══════════════════════════════════════════════════════════════════════════
    
    QTabWidget* m_tabWidget;
    
    // NVMe displays
    struct NVMeWidget {
        QLabel* nameLabel;
        QProgressBar* tempBar;
        QLabel* tempLabel;
        QLabel* headroomLabel;
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
    QLabel* m_throttleActionLabel;
    
    // Burst mode control
    QComboBox* m_burstModeCombo;
    QPushButton* m_applyButton;
    
    // Status
    QLabel* m_statusLabel;
    QLabel* m_predictionStatusLabel;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UI Components - Charts Tab
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Temperature history chart
    QChartView* m_tempChartView;
    QChart* m_tempChart;
    QLineSeries* m_nvmeTempSeries[5];
    QLineSeries* m_gpuTempSeries;
    QLineSeries* m_cpuTempSeries;
    QLineSeries* m_predictedTempSeries;     // Dotted prediction line
    QAreaSeries* m_predictionRangeSeries;   // Confidence band
    QScatterSeries* m_thresholdMarkers;
    QValueAxis* m_tempXAxis;
    QValueAxis* m_tempYAxis;
    
    // Load balancer chart
    QChartView* m_loadChartView;
    QChart* m_loadChart;
    QLineSeries* m_loadSeries[5];
    QLineSeries* m_selectedDriveSeries;
    
    // Prediction chart
    QChartView* m_predChartView;
    QChart* m_predChart;
    QLineSeries* m_ewmaSeries;
    QLineSeries* m_slopeSeries;
    QLineSeries* m_confidenceSeries;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UI Components - Config Tab
    // ═══════════════════════════════════════════════════════════════════════════
    
    QDoubleSpinBox* m_alphaSpinBox;
    QSpinBox* m_historySizeSpinBox;
    QDoubleSpinBox* m_thermalThresholdSpinBox;
    QDoubleSpinBox* m_emergencyThresholdSpinBox;
    QSpinBox* m_predictionHorizonSpinBox;
    QCheckBox* m_predictiveEnabledCheck;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UI Components - Controls Tab
    // ═══════════════════════════════════════════════════════════════════════════
    
    QSlider* m_manualThrottleSlider;
    QLabel* m_manualThrottleLabel;
    QCheckBox* m_manualThrottleEnabled;
    
    QCheckBox* m_driveOverrideCheck;
    QComboBox* m_driveOverrideCombo;
    
    QTableWidget* m_driveSelectionTable;
    
    QPushButton* m_emergencyStopButton;
    QPushButton* m_clearEmergencyButton;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // Backend Components
    // ═══════════════════════════════════════════════════════════════════════════
    
    std::unique_ptr<PredictiveThrottling> m_predictor;
    std::unique_ptr<DynamicLoadBalancer> m_loadBalancer;
    std::unique_ptr<SharedMemoryManager> m_sharedMemory;
    
    // Temperature history (for charts)
    static constexpr size_t MAX_HISTORY_POINTS = 300;  // 5 minutes at 1 sec interval
    std::deque<TemperatureDataPoint> m_nvmeHistory[5];
    std::deque<TemperatureDataPoint> m_gpuHistory;
    std::deque<TemperatureDataPoint> m_cpuHistory;
    
    // State
    ThermalSnapshot m_lastSnapshot;
    qint64 m_startTime;
    bool m_manualThrottleActive;
    bool m_driveOverrideActive;
    int m_selectedDriveOverride;
};

/**
 * @brief Compact toolbar widget for thermal status (enhanced)
 */
class ThermalCompactWidgetEnhanced : public QFrame {

public:
    explicit ThermalCompactWidgetEnhanced(void* parent = nullptr);
    ~ThermalCompactWidgetEnhanced() override = default;

public:
    void onThermalUpdate(const ThermalSnapshot& snapshot);
    void onPredictionUpdate(const PredictionResult& prediction);

    void expandRequested();

private:
    void setupUI();
    
    QLabel* m_maxTempLabel;
    QLabel* m_predictedTempLabel;
    QLabel* m_throttleIcon;
    QLabel* m_modeIcon;
    QLabel* m_driveIcon;
    QPushButton* m_expandButton;
};

} // namespace rawrxd::thermal

