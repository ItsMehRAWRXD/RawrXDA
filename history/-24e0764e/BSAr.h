#ifndef HEADLESS_READINESS_H
#define HEADLESS_READINESS_H

#include <QObject>
#include <QString>
#include <QProcess>
#include <QTimer>
#include <QThread>
#include <QMutex>

namespace RawrXD {

struct HeadlessConfig {
    bool enabled;
    int timeoutMinutes;
    qint64 maxMemoryMB;
    int maxCpuPercent;
    QString workingDirectory;
    QString logFile;
    QString pidFile;
    
    HeadlessConfig() 
        : enabled(false), timeoutMinutes(60), maxMemoryMB(4096), maxCpuPercent(80) {}
};

class HeadlessReadiness : public QObject {
    Q_OBJECT

public:
    static HeadlessReadiness& instance();
    
    void initialize(const HeadlessConfig& config = HeadlessConfig());
    void shutdown();
    
    // Mode control
    bool startHeadlessMode();
    bool stopHeadlessMode();
    bool isHeadlessRunning() const;
    
    // Resource monitoring
    qint64 getMemoryUsage() const;
    int getCpuUsage() const;
    bool isWithinLimits() const;
    
    // Health checks
    bool performHealthCheck();
    void setHealthCheckInterval(int seconds);
    
    // Configuration
    void updateConfig(const HeadlessConfig& config);
    HeadlessConfig getConfig() const;
    
    // Process management
    bool restartHeadlessProcess();
    bool isProcessAlive() const;
    
    // Logging
    QString getLogContent() const;
    void clearLog();
    
    // Emergency procedures
    void emergencyShutdown();
    void gracefulShutdown();
    
signals:
    void headlessStarted();
    void headlessStopped();
    void resourceWarning(const QString& resource, qint64 usage, qint64 limit);
    void healthCheckFailed(const QString& reason);
    void processCrashed(int exitCode);

private slots:
    void onHealthCheckTimeout();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    HeadlessReadiness() = default;
    ~HeadlessReadiness();
    
    bool startMonitoring();
    void stopMonitoring();
    void checkResourceLimits();
    void logMessage(const QString& message);
    void writePidFile();
    void removePidFile();
    
    HeadlessConfig config_;
    QProcess* headlessProcess_ = nullptr;
    QTimer* healthCheckTimer_ = nullptr;
    QTimer* resourceMonitorTimer_ = nullptr;
    
    mutable QMutex mutex_;
    bool initialized_ = false;
    bool headlessRunning_ = false;
    
    static const int DEFAULT_HEALTH_CHECK_INTERVAL = 30; // 30 seconds
    static const int DEFAULT_RESOURCE_CHECK_INTERVAL = 10; // 10 seconds
};

// Convenience macros
#define HEADLESS_START() RawrXD::HeadlessReadiness::instance().startHeadlessMode()
#define HEADLESS_STOP() RawrXD::HeadlessReadiness::instance().stopHeadlessMode()
#define HEADLESS_RUNNING() RawrXD::HeadlessReadiness::instance().isHeadlessRunning()
#define HEADLESS_HEALTH_CHECK() RawrXD::HeadlessReadiness::instance().performHealthCheck()

} // namespace RawrXD

#endif // HEADLESS_READINESS_H