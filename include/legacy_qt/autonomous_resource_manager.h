#pragma once

#include <QObject>
#include <QString>
#include <cstdint>

class QTimer;

/**
 * Legacy Qt-based resource monitor (quarantined under src/legacy/qt_support).
 * Production code uses Win32 paths under src/win32app/.
 */
class AutonomousResourceManager : public QObject
{
    Q_OBJECT

  public:
    struct SystemResources
    {
        uint64_t total_memory_bytes = 0;
        uint64_t available_memory_bytes = 0;
        uint32_t cpu_usage_percent = 0;
        uint32_t memory_usage_percent = 0;
        uint32_t gpu_usage_percent = 0;
        bool gpu_available = false;
        QString gpu_name;
        uint64_t disk_space_available_bytes = 0;

        double getMemoryUsagePercent() const
        {
            if (total_memory_bytes == 0)
            {
                return 0.0;
            }
            const double used = static_cast<double>(total_memory_bytes - available_memory_bytes);
            return 100.0 * used / static_cast<double>(total_memory_bytes);
        }

        bool isMemoryLow() const { return getMemoryUsagePercent() > 75.0; }
        bool isMemoryCritical() const { return getMemoryUsagePercent() > 90.0; }
    };

    explicit AutonomousResourceManager(QObject* parent = nullptr);
    ~AutonomousResourceManager() override;

    SystemResources getCurrentResources() const;

    bool canLoadModel(const QString& modelPath, const SystemResources& resources) const;
    uint32_t getOptimalThreadCount(const SystemResources& resources) const;
    bool shouldUseCompression(const SystemResources& resources) const;
    uint32_t getRecommendedCompressionLevel(const SystemResources& resources) const;

    void startMonitoring(int intervalMs);
    void stopMonitoring();
    void updateResources();

  signals:
    void resourcesUpdated(const SystemResources& resources);
    void resourcesCritical(const SystemResources& resources);
    void resourcesLow(const SystemResources& resources);
    void resourcesOptimal(const SystemResources& resources);

  private slots:
    void onMonitoringTimer();

  private:
    SystemResources gatherResources() const;

    uint64_t getAvailableMemory() const;
    uint64_t getTotalMemory() const;
    uint32_t getCpuUsage() const;
    void getGpuInfo(uint32_t& usage, bool& available, QString& name) const;
    uint64_t getAvailableDiskSpace(const QString& path) const;

    QTimer* monitoring_timer_;
    bool monitoring_active_;
    SystemResources current_resources_;
};

Q_DECLARE_METATYPE(AutonomousResourceManager::SystemResources)
