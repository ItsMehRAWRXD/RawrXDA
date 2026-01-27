/**
 * @file thermal_dashboard_plugin.hpp
 * @brief Hot-injectable Thermal Dashboard Plugin Interface
 * 
 * Provides dynamic loading capability for thermal monitoring UI
 * into RawrXD IDE without restart.
 */

#pragma once

#include <QtPlugin>
#include <QWidget>
#include <QObject>
#include <memory>
#include <cstdint>

namespace rawrxd::thermal {

/**
 * @brief Thermal data snapshot structure
 */
struct ThermalSnapshot {
    float nvmeTemps[5];     // Up to 5 NVMe drives
    float gpuTemp;
    float cpuTemp;
    int activeDriveCount;
    int currentThrottle;    // 0-100%
    uint64_t timestamp;
};

/**
 * @brief Plugin interface for thermal dashboard injection
 */
class IThermalDashboardPlugin {
public:
    virtual ~IThermalDashboardPlugin() = default;
    
    // Plugin lifecycle
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual QString pluginName() const = 0;
    virtual QString pluginVersion() const = 0;
    
    // Widget creation
    virtual QWidget* createDashboardWidget(QWidget* parent = nullptr) = 0;
    virtual QWidget* createCompactWidget(QWidget* parent = nullptr) = 0;  // Toolbar widget
    
    // Runtime control
    virtual void startMonitoring() = 0;
    virtual void stopMonitoring() = 0;
    virtual bool isMonitoring() const = 0;
    
    // Burst mode control (interfaces with RawrXD-Hybrid.exe)
    virtual void setBurstMode(int mode) = 0;  // 0=sovereign, 1=thermal, 2=hybrid
    virtual int currentBurstMode() const = 0;
    
    // Thermal data access
    virtual ThermalSnapshot getCurrentSnapshot() const = 0;
};

} // namespace rawrxd::thermal

#define IThermalDashboardPlugin_iid "com.rawrxd.thermal.IThermalDashboardPlugin/1.0"
Q_DECLARE_INTERFACE(rawrxd::thermal::IThermalDashboardPlugin, IThermalDashboardPlugin_iid)
