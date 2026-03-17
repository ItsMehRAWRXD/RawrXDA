/**
 * @file RAWRXD_ThermalDashboard_Enhanced.hpp
 * @brief Enhanced Win32 Thermal Dashboard with Predictive Visualization (Qt-free)
 *
 * Integrates with PredictiveThrottling engine; pure C++20/Win32.
 *
 * @copyright RawrXD IDE 2026
 */

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "thermal_dashboard_plugin.hpp"
#include "PredictiveThrottling.h"
#include "DynamicLoadBalancer.h"
#include "SovereignControlBlock.h"

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
    int64_t timestamp;
    double temperature;
};

/**
 * @brief Enhanced Thermal Dashboard with Predictive Visualization
 */
class ThermalDashboardEnhanced {

public:
    explicit ThermalDashboardEnhanced(void* parent = nullptr);
    ~ThermalDashboardEnhanced();

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
    uint32_t getThrottleActionColor(ThrottleAction action);

private:
    // ═══════════════════════════════════════════════════════════════════════════
    // UI Components - Main Tab
    // ═══════════════════════════════════════════════════════════════════════════
    
    void* m_tabWidget;
    
    // NVMe displays
    struct NVMeWidget {
        void* nameLabel;
        void* tempBar;
        void* tempLabel;
        void* headroomLabel;
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
    void* m_throttleActionLabel;
    
    // Burst mode control
    void* m_burstModeCombo;
    void* m_applyButton;
    
    // Status
    void* m_statusLabel;
    void* m_predictionStatusLabel;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UI Components - Charts Tab
    // ═══════════════════════════════════════════════════════════════════════════
    
    // Temperature history chart
    void* m_tempChartView;           // Chart view handle
    void* m_tempChart;               // Chart object handle
    void* m_nvmeTempSeries[5];       // Line series (was QLineSeries*)
    void* m_gpuTempSeries;           // Line series (was QLineSeries*)
    void* m_cpuTempSeries;           // Line series (was QLineSeries*)
    void* m_predictedTempSeries;     // Dotted prediction line (was QLineSeries*)
    void* m_predictionRangeSeries;   // Confidence band (was QAreaSeries*)
    void* m_thresholdMarkers;        // Scatter series (was QScatterSeries*)
    void* m_tempXAxis;               // Value axis (was QValueAxis*)
    void* m_tempYAxis;               // Value axis (was QValueAxis*)
    
    // Load balancer chart
    void* m_loadChartView;           // Chart view handle
    void* m_loadChart;               // Chart object handle
    void* m_loadSeries[5];           // Line series (was QLineSeries*)
    void* m_selectedDriveSeries;     // Line series (was QLineSeries*)
    
    // Prediction chart
    void* m_predChartView;           // Chart view handle
    void* m_predChart;               // Chart object handle
    void* m_ewmaSeries;              // Line series (was QLineSeries*)
    void* m_slopeSeries;             // Line series (was QLineSeries*)
    void* m_confidenceSeries;        // Line series (was QLineSeries*)
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UI Components - Config Tab
    // ═══════════════════════════════════════════════════════════════════════════
    
    void* m_alphaSpinBox;            // Double spin (was QDoubleSpinBox*)
    void* m_historySizeSpinBox;
    void* m_thermalThresholdSpinBox;  // Double spin (was QDoubleSpinBox*)
    void* m_emergencyThresholdSpinBox; // Double spin (was QDoubleSpinBox*)
    void* m_predictionHorizonSpinBox;
    void* m_predictiveEnabledCheck;
    
    // ═══════════════════════════════════════════════════════════════════════════
    // UI Components - Controls Tab
    // ═══════════════════════════════════════════════════════════════════════════
    
    void* m_manualThrottleSlider;
    void* m_manualThrottleLabel;
    void* m_manualThrottleEnabled;
    
    void* m_driveOverrideCheck;
    void* m_driveOverrideCombo;
    
    void* m_driveSelectionTable;  // HWND list-view control (was QTableWidget*)
    
    void* m_emergencyStopButton;
    void* m_clearEmergencyButton;
    
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
    int64_t m_startTime;
    bool m_manualThrottleActive;
    bool m_driveOverrideActive;
    int m_selectedDriveOverride;
};

/**
 * @brief Compact toolbar widget for thermal status (enhanced)
 */
class ThermalCompactWidgetEnhanced {

public:
    explicit ThermalCompactWidgetEnhanced(void* parent = nullptr);
    ~ThermalCompactWidgetEnhanced() = default;

public:
    void onThermalUpdate(const ThermalSnapshot& snapshot);
    void onPredictionUpdate(const PredictionResult& prediction);

    void expandRequested();

private:
    void setupUI();
    
    void* m_maxTempLabel;
    void* m_predictedTempLabel;
    void* m_throttleIcon;
    void* m_modeIcon;
    void* m_driveIcon;
    void* m_expandButton;
};

} // namespace rawrxd::thermal


