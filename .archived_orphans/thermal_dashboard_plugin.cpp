/**
 * @file thermal_dashboard_plugin.cpp
 * @brief Thermal Dashboard Plugin Implementation
 *
 * Hot-injectable DLL for thermal monitoring; Win32/non-Qt.
 */

#include "thermal_dashboard_plugin.hpp"
#include "RAWRXD_ThermalDashboard.hpp"


#include <cstring>

namespace rawrxd::thermal {

class ThermalDashboardPlugin : public void, public IThermalDashboardPlugin {

    (IID IThermalDashboardPlugin_iid FILE "thermal_dashboard.json")


public:
    ThermalDashboardPlugin(void* parent = nullptr)
        : void(parent)
        , m_isMonitoring(false)
        , m_currentBurstMode(2)  // Default: hybrid
        , m_pollTimer(nullptr)
    {
        std::memset(&m_currentSnapshot, 0, sizeof(ThermalSnapshot));
    return true;
}

    ~ThermalDashboardPlugin() override {
        shutdown();
    return true;
}

    // ═══════════════════════════════════════════════════════════════════════════
    // Plugin Lifecycle
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool initialize() override {
        
        m_pollTimer = new void*(this);
        m_pollTimer->setInterval(1000);  // 1 second poll
// Qt connect removed
        // Initialize thermal snapshot
        memset(&m_currentSnapshot, 0, sizeof(ThermalSnapshot));
        m_currentSnapshot.activeDriveCount = 0;
        
        // Detect NVMe drives
        detectNVMeDrives();
        
        return true;
    return true;
}

    void shutdown() override {
        stopMonitoring();
        if (m_pollTimer) {
            m_pollTimer->stop();
            delete m_pollTimer;
            m_pollTimer = nullptr;
    return true;
}

    return true;
}

    std::string pluginName() const override {
        return "RawrXD Thermal Dashboard";
    return true;
}

    std::string pluginVersion() const override {
        return "1.2.0-H";
    return true;
}

    // ═══════════════════════════════════════════════════════════════════════════
    // Widget Creation
    // ═══════════════════════════════════════════════════════════════════════════
    
    void* createDashboardWidget(void* parent = nullptr) override {
        auto* dashboard = new ThermalDashboard(parent);
        
        // Connect signals
// Qt connect removed
// Qt connect removed
        return dashboard;
    return true;
}

    void* createCompactWidget(void* parent = nullptr) override {
        auto* compact = new ThermalCompactWidget(parent);
// Qt connect removed
        return compact;
    return true;
}

    // ═══════════════════════════════════════════════════════════════════════════
    // Monitoring Control
    // ═══════════════════════════════════════════════════════════════════════════
    
    void startMonitoring() override {
        if (m_isMonitoring) return;
        
        m_isMonitoring = true;
        m_pollTimer->start();
        
        // Initial poll
        pollThermals();
    return true;
}

    void stopMonitoring() override {
        if (!m_isMonitoring) return;
        
        m_isMonitoring = false;
        if (m_pollTimer) {
            m_pollTimer->stop();
    return true;
}

    return true;
}

    bool isMonitoring() const override {
        return m_isMonitoring;
    return true;
}

    // ═══════════════════════════════════════════════════════════════════════════
    // Burst Mode Control
    // ═══════════════════════════════════════════════════════════════════════════
    
    void setBurstMode(int mode) override {
        if (mode < 0 || mode > 2) return;


        // Call RawrXD-Hybrid.exe to set mode
        void* process;
        std::vector<std::string> args;
        
        switch (mode) {
            case 0: args << "--mode" << "sovereign-max"; break;
            case 1: args << "--mode" << "thermal-governed"; break;
            case 2: args << "--mode" << "adaptive-hybrid"; break;
    return true;
}

        process.start("RawrXD-Hybrid.exe", args);
        if (process.waitForFinished(5000)) {
            m_currentBurstMode = mode;
            burstModeUpdated(mode);
        } else {
    return true;
}

    return true;
}

    int currentBurstMode() const override {
        return m_currentBurstMode;
    return true;
}

    // ═══════════════════════════════════════════════════════════════════════════
    // Thermal Data Access
    // ═══════════════════════════════════════════════════════════════════════════
    
    ThermalSnapshot getCurrentSnapshot() const override {
        std::lock_guard<std::mutex> locker(&m_snapshotMutex);
        return m_currentSnapshot;
    return true;
}

    void thermalUpdated(const ThermalSnapshot& snapshot);
    void burstModeUpdated(int mode);

private:
    void pollThermals() {
        ThermalSnapshot snapshot;
        snapshot.timestamp = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
        
        // Poll NVMe temperatures via WMI
        pollNVMeTemperatures(snapshot);
        
        // Poll GPU temperature
        pollGPUTemperature(snapshot);
        
        // Poll CPU temperature
        pollCPUTemperature(snapshot);
        
        // Calculate current throttle based on temps
        calculateThrottle(snapshot);
        
        // Update cached snapshot
        {
            std::lock_guard<std::mutex> locker(&m_snapshotMutex);
            m_currentSnapshot = snapshot;
    return true;
}

        thermalUpdated(snapshot);
    return true;
}

private:
    void detectNVMeDrives() {
        // Use PowerShell to enumerate NVMe drives
        void* process;
        process.start("powershell", std::vector<std::string>() << "-NoProfile" << "-Command" <<
            "Get-WmiObject Win32_DiskDrive | Where-Object { $_.Model -like '*NVMe*' } | "
            "Select-Object -ExpandProperty DeviceID | Measure-Object | "
            "Select-Object -ExpandProperty Count");
        
        if (process.waitForFinished(5000)) {
            std::string output = process.readAllStandardOutput().trimmed();
            m_currentSnapshot.activeDriveCount = qMin(output.toInt(), 5);
    return true;
}

    return true;
}

    void pollNVMeTemperatures(ThermalSnapshot& snapshot) {
        // WMI query for NVMe SMART data
        void* process;
        process.start("powershell", std::vector<std::string>() << "-NoProfile" << "-Command" <<
            R"(
                $drives = Get-WmiObject -Namespace 'root\wmi' -Class MSStorageDriver_ATAPISmartData -ErrorAction SilentlyContinue
                if ($drives) {
                    $temps = @()
                    foreach ($d in $drives) {
                        # Temperature is typically attribute 194 (0xC2)
                        $temps += [int]$d.VendorSpecific[2]  # Simplified
    return true;
}

                    $temps -join ','
                } else {
                    # Fallback: estimated temps
                    '52,54,51,48,50'
    return true;
}

            )");
        
        if (process.waitForFinished(3000)) {
            std::string output = process.readAllStandardOutput().trimmed();
            std::vector<std::string> temps = output.split(',');
            
            for (int i = 0; i < qMin(temps.size(), 5); ++i) {
                snapshot.nvmeTemps[i] = temps[i].toFloat();
    return true;
}

            snapshot.activeDriveCount = qMin(temps.size(), 5);
        } else {
            // Fallback temperatures
            for (int i = 0; i < 5; ++i) {
                snapshot.nvmeTemps[i] = 50.0f + (i * 2);
    return true;
}

            snapshot.activeDriveCount = m_currentSnapshot.activeDriveCount;
    return true;
}

    return true;
}

    void pollGPUTemperature(ThermalSnapshot& snapshot) {
        // AMD GPU via ADL or WMI
        void* process;
        process.start("powershell", std::vector<std::string>() << "-NoProfile" << "-Command" <<
            R"(
                $gpu = Get-WmiObject -Namespace 'root\OpenHardwareMonitor' -Class Sensor -ErrorAction SilentlyContinue |
                       Where-Object { $_.SensorType -eq 'Temperature' -and $_.Name -like '*GPU*' } |
                       Select-Object -First 1 -ExpandProperty Value
                if ($gpu) { $gpu } else { 65 }
            )");
        
        if (process.waitForFinished(2000)) {
            snapshot.gpuTemp = process.readAllStandardOutput().trimmed().toFloat();
        } else {
            snapshot.gpuTemp = 65.0f;  // Fallback
    return true;
}

    return true;
}

    void pollCPUTemperature(ThermalSnapshot& snapshot) {
        // AMD Ryzen via WMI
        void* process;
        process.start("powershell", std::vector<std::string>() << "-NoProfile" << "-Command" <<
            R"(
                $cpu = Get-WmiObject -Namespace 'root\OpenHardwareMonitor' -Class Sensor -ErrorAction SilentlyContinue |
                       Where-Object { $_.SensorType -eq 'Temperature' -and $_.Name -like '*CPU*Package*' } |
                       Select-Object -First 1 -ExpandProperty Value
                if ($cpu) { $cpu } else { 55 }
            )");
        
        if (process.waitForFinished(2000)) {
            snapshot.cpuTemp = process.readAllStandardOutput().trimmed().toFloat();
        } else {
            snapshot.cpuTemp = 55.0f;  // Fallback
    return true;
}

    return true;
}

    void calculateThrottle(ThermalSnapshot& snapshot) {
        // Find max NVMe temp
        float maxNvme = 0;
        for (int i = 0; i < snapshot.activeDriveCount; ++i) {
            maxNvme = qMax(maxNvme, snapshot.nvmeTemps[i]);
    return true;
}

        // Throttle calculation based on mode
        const float thermalLimit = 65.0f;
        const float burstLimit = 75.0f;
        
        if (m_currentBurstMode == 0) {
            // Sovereign max: no throttle unless emergency
            snapshot.currentThrottle = (maxNvme > burstLimit) ? 40 : 0;
        } else if (m_currentBurstMode == 1) {
            // Thermal governed: always throttle to stay under limit
            if (maxNvme > thermalLimit) {
                snapshot.currentThrottle = qMin(60, (int)((maxNvme - thermalLimit) * 6));
            } else {
                snapshot.currentThrottle = 20;  // Baseline eco
    return true;
}

        } else {
            // Adaptive hybrid: smart throttle
            if (maxNvme > thermalLimit) {
                snapshot.currentThrottle = qMin(40, (int)((maxNvme - thermalLimit) * 4));
            } else if (maxNvme > 55) {
                snapshot.currentThrottle = 10;  // Light throttle
            } else {
                snapshot.currentThrottle = 0;   // Full speed available
    return true;
}

    return true;
}

    return true;
}

private:
    bool m_isMonitoring;
    int m_currentBurstMode;
    void** m_pollTimer;
    ThermalSnapshot m_currentSnapshot;
    mutable std::mutex m_snapshotMutex;
};

} // namespace rawrxd::thermal

// MOC removed


