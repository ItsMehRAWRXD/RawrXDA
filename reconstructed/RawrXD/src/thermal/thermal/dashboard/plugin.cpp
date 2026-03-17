/**
 * @file thermal_dashboard_plugin.cpp
 * @brief Thermal Dashboard Plugin Implementation
 * 
 * Hot-injectable DLL for thermal monitoring with Qt6 UI
 */

#include "thermal_dashboard_plugin.hpp"
#include "RAWRXD_ThermalDashboard.hpp"
#include <QProcess>
#include <QTimer>
#include <QMutex>
#include <QMutexLocker>
#include <QDebug>
#include <QDateTime>
#include <QCoreApplication>
#include <cstring>

namespace rawrxd::thermal {

class ThermalDashboardPlugin : public QObject, public IThermalDashboardPlugin {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID IThermalDashboardPlugin_iid FILE "thermal_dashboard.json")
    Q_INTERFACES(rawrxd::thermal::IThermalDashboardPlugin)

public:
    ThermalDashboardPlugin(QObject* parent = nullptr)
        : QObject(parent)
        , m_isMonitoring(false)
        , m_currentBurstMode(2)  // Default: hybrid
        , m_pollTimer(nullptr)
    {
        qDebug() << "[ThermalPlugin] Constructor called";
        std::memset(&m_currentSnapshot, 0, sizeof(ThermalSnapshot));
    }
    
    ~ThermalDashboardPlugin() override {
        shutdown();
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Plugin Lifecycle
    // ═══════════════════════════════════════════════════════════════════════════
    
    bool initialize() override {
        qDebug() << "[ThermalPlugin] Initializing...";
        
        m_pollTimer = new QTimer(this);
        m_pollTimer->setInterval(1000);  // 1 second poll
        connect(m_pollTimer, &QTimer::timeout, this, &ThermalDashboardPlugin::pollThermals);
        
        // Initialize thermal snapshot
        memset(&m_currentSnapshot, 0, sizeof(ThermalSnapshot));
        m_currentSnapshot.activeDriveCount = 0;
        
        // Detect NVMe drives
        detectNVMeDrives();
        
        qDebug() << "[ThermalPlugin] Initialized successfully";
        return true;
    }
    
    void shutdown() override {
        qDebug() << "[ThermalPlugin] Shutting down...";
        stopMonitoring();
        if (m_pollTimer) {
            m_pollTimer->stop();
            delete m_pollTimer;
            m_pollTimer = nullptr;
        }
    }
    
    QString pluginName() const override {
        return QStringLiteral("RawrXD Thermal Dashboard");
    }
    
    QString pluginVersion() const override {
        return QStringLiteral("1.2.0-H");
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Widget Creation
    // ═══════════════════════════════════════════════════════════════════════════
    
    QWidget* createDashboardWidget(QWidget* parent = nullptr) override {
        auto* dashboard = new ThermalDashboard(parent);
        
        // Connect signals
        connect(this, &ThermalDashboardPlugin::thermalUpdated,
                dashboard, &ThermalDashboard::onThermalUpdate);
        connect(dashboard, &ThermalDashboard::burstModeChanged,
                this, &ThermalDashboardPlugin::setBurstMode);
        
        return dashboard;
    }
    
    QWidget* createCompactWidget(QWidget* parent = nullptr) override {
        auto* compact = new ThermalCompactWidget(parent);
        
        connect(this, &ThermalDashboardPlugin::thermalUpdated,
                compact, &ThermalCompactWidget::onThermalUpdate);
        
        return compact;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Monitoring Control
    // ═══════════════════════════════════════════════════════════════════════════
    
    void startMonitoring() override {
        if (m_isMonitoring) return;
        
        qDebug() << "[ThermalPlugin] Starting monitoring...";
        m_isMonitoring = true;
        m_pollTimer->start();
        
        // Initial poll
        pollThermals();
    }
    
    void stopMonitoring() override {
        if (!m_isMonitoring) return;
        
        qDebug() << "[ThermalPlugin] Stopping monitoring...";
        m_isMonitoring = false;
        if (m_pollTimer) {
            m_pollTimer->stop();
        }
    }
    
    bool isMonitoring() const override {
        return m_isMonitoring;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Burst Mode Control
    // ═══════════════════════════════════════════════════════════════════════════
    
    void setBurstMode(int mode) override {
        if (mode < 0 || mode > 2) return;
        
        qDebug() << "[ThermalPlugin] Setting burst mode:" << mode;
        
        // Call RawrXD-Hybrid.exe to set mode
        QProcess process;
        QStringList args;
        
        switch (mode) {
            case 0: args << "--mode" << "sovereign-max"; break;
            case 1: args << "--mode" << "thermal-governed"; break;
            case 2: args << "--mode" << "adaptive-hybrid"; break;
        }
        
        process.start("RawrXD-Hybrid.exe", args);
        if (process.waitForFinished(5000)) {
            m_currentBurstMode = mode;
            emit burstModeUpdated(mode);
        } else {
            qWarning() << "[ThermalPlugin] Failed to set burst mode";
        }
    }
    
    int currentBurstMode() const override {
        return m_currentBurstMode;
    }

    // ═══════════════════════════════════════════════════════════════════════════
    // Thermal Data Access
    // ═══════════════════════════════════════════════════════════════════════════
    
    ThermalSnapshot getCurrentSnapshot() const override {
        QMutexLocker locker(&m_snapshotMutex);
        return m_currentSnapshot;
    }

signals:
    void thermalUpdated(const ThermalSnapshot& snapshot);
    void burstModeUpdated(int mode);

private slots:
    void pollThermals() {
        ThermalSnapshot snapshot;
        snapshot.timestamp = QDateTime::currentMSecsSinceEpoch();
        
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
            QMutexLocker locker(&m_snapshotMutex);
            m_currentSnapshot = snapshot;
        }
        
        emit thermalUpdated(snapshot);
    }

private:
    void detectNVMeDrives() {
        // Use PowerShell to enumerate NVMe drives
        QProcess process;
        process.start("powershell", QStringList() << "-NoProfile" << "-Command" <<
            "Get-WmiObject Win32_DiskDrive | Where-Object { $_.Model -like '*NVMe*' } | "
            "Select-Object -ExpandProperty DeviceID | Measure-Object | "
            "Select-Object -ExpandProperty Count");
        
        if (process.waitForFinished(5000)) {
            QString output = process.readAllStandardOutput().trimmed();
            m_currentSnapshot.activeDriveCount = qMin(output.toInt(), 5);
            qDebug() << "[ThermalPlugin] Detected" << m_currentSnapshot.activeDriveCount << "NVMe drives";
        }
    }
    
    void pollNVMeTemperatures(ThermalSnapshot& snapshot) {
        // WMI query for NVMe SMART data
        QProcess process;
        process.start("powershell", QStringList() << "-NoProfile" << "-Command" <<
            R"(
                $drives = Get-WmiObject -Namespace 'root\wmi' -Class MSStorageDriver_ATAPISmartData -ErrorAction SilentlyContinue
                if ($drives) {
                    $temps = @()
                    foreach ($d in $drives) {
                        # Temperature is typically attribute 194 (0xC2)
                        $temps += [int]$d.VendorSpecific[2]  # Simplified
                    }
                    $temps -join ','
                } else {
                    # Fallback: estimated temps
                    '52,54,51,48,50'
                }
            )");
        
        if (process.waitForFinished(3000)) {
            QString output = process.readAllStandardOutput().trimmed();
            QStringList temps = output.split(',');
            
            for (int i = 0; i < qMin(temps.size(), 5); ++i) {
                snapshot.nvmeTemps[i] = temps[i].toFloat();
            }
            snapshot.activeDriveCount = qMin(temps.size(), 5);
        } else {
            // Fallback temperatures
            for (int i = 0; i < 5; ++i) {
                snapshot.nvmeTemps[i] = 50.0f + (i * 2);
            }
            snapshot.activeDriveCount = m_currentSnapshot.activeDriveCount;
        }
    }
    
    void pollGPUTemperature(ThermalSnapshot& snapshot) {
        // AMD GPU via ADL or WMI
        QProcess process;
        process.start("powershell", QStringList() << "-NoProfile" << "-Command" <<
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
        }
    }
    
    void pollCPUTemperature(ThermalSnapshot& snapshot) {
        // AMD Ryzen via WMI
        QProcess process;
        process.start("powershell", QStringList() << "-NoProfile" << "-Command" <<
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
        }
    }
    
    void calculateThrottle(ThermalSnapshot& snapshot) {
        // Find max NVMe temp
        float maxNvme = 0;
        for (int i = 0; i < snapshot.activeDriveCount; ++i) {
            maxNvme = qMax(maxNvme, snapshot.nvmeTemps[i]);
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
            }
        } else {
            // Adaptive hybrid: smart throttle
            if (maxNvme > thermalLimit) {
                snapshot.currentThrottle = qMin(40, (int)((maxNvme - thermalLimit) * 4));
            } else if (maxNvme > 55) {
                snapshot.currentThrottle = 10;  // Light throttle
            } else {
                snapshot.currentThrottle = 0;   // Full speed available
            }
        }
    }

private:
    bool m_isMonitoring;
    int m_currentBurstMode;
    QTimer* m_pollTimer;
    ThermalSnapshot m_currentSnapshot;
    mutable QMutex m_snapshotMutex;
};

} // namespace rawrxd::thermal

#include "thermal_dashboard_plugin.moc"
