/**
 * @file windows_resource_enforcer.h
 * @brief Windows Job Objects for CPU and memory resource enforcement
 * @author RawrXD Team
 * @date 2026
 * 
 * Provides:
 * - Windows Job Object creation and management
 * - CPU affinity and usage limits
 * - Memory limits (working set and commit)
 * - Process priority management
 * - Resource monitoring and enforcement
 */

#pragma once

#include <QString>
#include <QObject>
#include <QList>
#include <QJsonObject>
#include <windows.h>
#include <memory>

/**
 * @class ResourceLimits
 * @brief Defines resource constraints for a process or job
 */
struct ResourceLimits {
    // CPU limits
    int cpuCount;                          // Number of CPUs available
    int cpuAffinity;                       // Bitmask for CPU affinity
    int cpuPercentageLimit;                // CPU usage limit (1-100)
    
    // Memory limits
    uint64_t workingSetLimitMB;            // Max working set in MB
    uint64_t commitLimitMB;                // Max commit in MB
    uint64_t peakWorkingSetMB;             // Peak working set tracking
    
    // Process limits
    int maxProcessCount;                   // Max number of processes
    int priorityClass;                     // NORMAL, HIGH, REALTIME, etc.
    int priorityBoost;                     // Priority boost on foreground
    
    // Enforcement
    bool enforceMemoryLimit;
    bool enforceCpuLimit;
    bool terminateOnLimitExceeded;
    
    ResourceLimits()
        : cpuCount(1), cpuAffinity(0), cpuPercentageLimit(50),
          workingSetLimitMB(512), commitLimitMB(1024), peakWorkingSetMB(0),
          maxProcessCount(1), priorityClass(NORMAL_PRIORITY_CLASS, priorityBoost(0),
          enforceMemoryLimit(true), enforceCpuLimit(true), terminateOnLimitExceeded(false)
    {
    }
};

/**
 * @class WindowsJobObject
 * @brief Wrapper for Windows Job Objects
 * 
 * Manages process resource limits using Windows Job Objects API
 */
class WindowsJobObject {
public:
    explicit WindowsJobObject(const QString& jobName);
    ~WindowsJobObject();
    
    // Job management
    bool create();
    bool assignProcess(DWORD processId);
    bool assignProcessHandle(HANDLE processHandle);
    void close();
    
    // Resource configuration
    void setResourceLimits(const ResourceLimits& limits);
    void setMemoryLimit(uint64_t limitMB);
    void setCpuAffinity(int affinityMask);
    void setPriorityClass(int priorityClass);
    
    // Monitoring
    struct JobStats {
        uint64_t totalProcesses;
        uint64_t totalUserTime;
        uint64_t totalKernelTime;
        uint64_t peakProcessCount;
        uint64_t peakWorkingSetBytes;
        uint64_t totalCommittedMemory;
    };
    
    JobStats getStatistics() const;
    bool isValid() const;
    HANDLE getHandle() const { return m_jobHandle; }
    
private:
    bool configureBasicLimits(const ResourceLimits& limits);
    bool configureExtendedLimits(const ResourceLimits& limits);
    bool configureNotificationLimit(const ResourceLimits& limits);
    
    HANDLE m_jobHandle;
    QString m_jobName;
    ResourceLimits m_currentLimits;
};

/**
 * @class ProcessResourceManager
 * @brief Manages resource constraints for sandboxed processes
 */
class ProcessResourceManager : public QObject {
    Q_OBJECT
    
public:
    explicit ProcessResourceManager(QObject* parent = nullptr);
    ~ProcessResourceManager();
    
    // Process registration and constraint
    QString createSandboxedProcess(const QString& exePath, const QString& args,
                                  const ResourceLimits& limits);
    
    bool terminateProcess(const QString& jobId);
    bool updateLimits(const QString& jobId, const ResourceLimits& limits);
    
    // Monitoring
    QJsonObject getProcessStats(const QString& jobId) const;
    QJsonArray getAllProcessStats() const;
    
    // Validation
    bool validateResourceLimits(const ResourceLimits& limits) const;
    QString getLastError() const { return m_lastError; }
    
signals:
    void resourceLimitExceeded(const QString& jobId, const QString& limitType);
    void processTerminated(const QString& jobId);
    void resourceAlert(const QString& jobId, double cpuPercent, uint64_t memoryMB);
    
private slots:
    void onMonitoringTimer();
    
private:
    struct ProcessEntry {
        QString jobId;
        std::shared_ptr<WindowsJobObject> jobObject;
        DWORD processId;
        QDateTime creationTime;
        ResourceLimits limits;
    };
    
    void monitorProcesses();
    
    QMap<QString, ProcessEntry> m_processes;
    QTimer* m_monitoringTimer;
    QString m_lastError;
    QMutex m_mutex;
};

#endif // WINDOWS_RESOURCE_ENFORCER_H
