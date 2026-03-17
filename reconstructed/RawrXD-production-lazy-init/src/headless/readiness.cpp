#include "headless_readiness.h"
#include "logging/structured_logger.h"
#include "error_handler.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace RawrXD {

HeadlessReadiness& HeadlessReadiness::instance() {
    static HeadlessReadiness instance;
    return instance;
}

void HeadlessReadiness::initialize(const HeadlessConfig& config) {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        return;
    }
    
    config_ = config;
    
    // Create health check timer
    healthCheckTimer_ = new QTimer(this);
    healthCheckTimer_->setInterval(DEFAULT_HEALTH_CHECK_INTERVAL * 1000);
    connect(healthCheckTimer_, &QTimer::timeout, this, &HeadlessReadiness::onHealthCheckTimeout);
    
    // Create resource monitor timer
    resourceMonitorTimer_ = new QTimer(this);
    resourceMonitorTimer_->setInterval(DEFAULT_RESOURCE_CHECK_INTERVAL * 1000);
    connect(resourceMonitorTimer_, &QTimer::timeout, this, [this]() {
        checkResourceLimits();
    });
    
    initialized_ = true;
    
    LOG_INFO("Headless readiness initialized", {
        {"enabled", config_.enabled},
        {"timeout_minutes", config_.timeoutMinutes},
        {"max_memory_mb", config_.maxMemoryMB},
        {"max_cpu_percent", config_.maxCpuPercent}
    });
}

void HeadlessReadiness::shutdown() {
    QMutexLocker lock(&mutex_);
    
    if (initialized_) {
        if (headlessRunning_) {
            stopHeadlessMode();
        }
        
        if (healthCheckTimer_) {
            healthCheckTimer_->stop();
            delete healthCheckTimer_;
            healthCheckTimer_ = nullptr;
        }
        
        if (resourceMonitorTimer_) {
            resourceMonitorTimer_->stop();
            delete resourceMonitorTimer_;
            resourceMonitorTimer_ = nullptr;
        }
        
        removePidFile();
        
        initialized_ = false;
        
        LOG_INFO("Headless readiness shut down");
    }
}

bool HeadlessReadiness::startHeadlessMode() {
    QMutexLocker lock(&mutex_);
    
    if (!initialized_) {
        ERROR_HANDLE("Headless readiness not initialized", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::CONFIGURATION)
            .setOperation("HeadlessReadiness startHeadlessMode"));
        return false;
    }
    
    if (headlessRunning_) {
        LOG_WARN("Headless mode already running");
        return true;
    }
    
    // Create headless process
    headlessProcess_ = new QProcess(this);
    
    // Set working directory
    if (!config_.workingDirectory.isEmpty()) {
        headlessProcess_->setWorkingDirectory(config_.workingDirectory);
    }
    
    // Set process environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("RAWRXD_HEADLESS_MODE", "1");
    env.insert("RAWRXD_LOG_FILE", config_.logFile);
    headlessProcess_->setProcessEnvironment(env);
    
    // Connect signals
    connect(headlessProcess_, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), 
            this, &HeadlessReadiness::onProcessFinished);
    connect(headlessProcess_, &QProcess::errorOccurred, this, &HeadlessReadiness::onProcessError);
    
    // Start the process
    QString program = QCoreApplication::applicationFilePath();
    QStringList arguments = {"--headless", "--no-gui"};
    
    headlessProcess_->start(program, arguments);
    
    if (!headlessProcess_->waitForStarted()) {
        ERROR_HANDLE("Failed to start headless process", ErrorContext()
            .setSeverity(ErrorSeverity::HIGH)
            .setCategory(ErrorCategory::EXECUTION)
            .setOperation("HeadlessReadiness startHeadlessMode"));
        
        delete headlessProcess_;
        headlessProcess_ = nullptr;
        return false;
    }
    
    headlessRunning_ = true;
    
    // Start monitoring
    startMonitoring();
    
    // Write PID file
    writePidFile();
    
    // Log startup
    logMessage("Headless mode started at " + QDateTime::currentDateTime().toString(Qt::ISODate));
    
    emit headlessStarted();
    
    LOG_INFO("Headless mode started", {{"pid", headlessProcess_->processId()}});
    
    return true;
}

bool HeadlessReadiness::stopHeadlessMode() {
    QMutexLocker lock(&mutex_);
    
    if (!headlessRunning_ || !headlessProcess_) {
        return true;
    }
    
    // Stop monitoring
    stopMonitoring();
    
    // Graceful shutdown
    headlessProcess_->kill();
    
    if (!headlessProcess_->waitForFinished(10000)) { // 10 second timeout
        headlessProcess_->terminate();
        headlessProcess_->waitForFinished(5000); // 5 second timeout
    }
    
    delete headlessProcess_;
    headlessProcess_ = nullptr;
    headlessRunning_ = false;
    
    // Remove PID file
    removePidFile();
    
    // Log shutdown
    logMessage("Headless mode stopped at " + QDateTime::currentDateTime().toString(Qt::ISODate));
    
    emit headlessStopped();
    
    LOG_INFO("Headless mode stopped");
    
    return true;
}

bool HeadlessReadiness::isHeadlessRunning() const {
    QMutexLocker lock(&mutex_);
    return headlessRunning_;
}

qint64 HeadlessReadiness::getMemoryUsage() const {
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS_EX pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*) &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize / (1024 * 1024); // Convert to MB
    }
#else
    // Linux/macOS implementation
    QFile file("/proc/self/status");
    if (file.open(QIODevice::ReadOnly)) {
        QTextStream in(&file);
        while (!in.atEnd()) {
            QString line = in.readLine();
            if (line.startsWith("VmRSS:")) {
                QStringList parts = line.split(' ', QString::SkipEmptyParts);
                if (parts.size() >= 2) {
                    return parts[1].toLongLong() / 1024; // Convert from KB to MB
                }
            }
        }
    }
#endif
    return 0;
}

int HeadlessReadiness::getCpuUsage() const {
    // CPU usage monitoring is complex and platform-specific
    // For simplicity, return a placeholder value
    // In production, you'd implement proper CPU monitoring
    return 0;
}

bool HeadlessReadiness::isWithinLimits() const {
    qint64 memoryUsage = getMemoryUsage();
    int cpuUsage = getCpuUsage();
    
    return memoryUsage <= config_.maxMemoryMB && cpuUsage <= config_.maxCpuPercent;
}

bool HeadlessReadiness::performHealthCheck() {
    QMutexLocker lock(&mutex_);
    
    if (!headlessRunning_) {
        emit healthCheckFailed("Headless mode not running");
        return false;
    }
    
    if (!isProcessAlive()) {
        emit healthCheckFailed("Headless process not alive");
        return false;
    }
    
    if (!isWithinLimits()) {
        qint64 memoryUsage = getMemoryUsage();
        int cpuUsage = getCpuUsage();
        
        QString warning = QString("Resource limits exceeded: Memory %1/%2MB, CPU %3/%4%")
            .arg(memoryUsage).arg(config_.maxMemoryMB)
            .arg(cpuUsage).arg(config_.maxCpuPercent);
        
        emit resourceWarning("limits", memoryUsage, config_.maxMemoryMB);
        emit healthCheckFailed(warning);
        
        return false;
    }
    
    // Additional health checks can be added here
    
    return true;
}

void HeadlessReadiness::setHealthCheckInterval(int seconds) {
    QMutexLocker lock(&mutex_);
    
    if (healthCheckTimer_) {
        healthCheckTimer_->setInterval(seconds * 1000);
    }
    
    LOG_DEBUG("Health check interval updated", {{"seconds", seconds}});
}

void HeadlessReadiness::updateConfig(const HeadlessConfig& config) {
    QMutexLocker lock(&mutex_);
    
    bool wasRunning = headlessRunning_;
    
    if (wasRunning) {
        stopHeadlessMode();
    }
    
    config_ = config;
    
    if (wasRunning) {
        startHeadlessMode();
    }
    
    LOG_INFO("Headless configuration updated");
}

HeadlessConfig HeadlessReadiness::getConfig() const {
    QMutexLocker lock(&mutex_);
    return config_;
}

bool HeadlessReadiness::restartHeadlessProcess() {
    QMutexLocker lock(&mutex_);
    
    if (!headlessRunning_) {
        return startHeadlessMode();
    }
    
    stopHeadlessMode();
    QThread::msleep(1000); // Wait 1 second
    return startHeadlessMode();
}

bool HeadlessReadiness::isProcessAlive() const {
    QMutexLocker lock(&mutex_);
    
    if (!headlessProcess_) {
        return false;
    }
    
    return headlessProcess_->state() == QProcess::Running;
}

QString HeadlessReadiness::getLogContent() const {
    if (config_.logFile.isEmpty()) {
        return QString();
    }
    
    QFile file(config_.logFile);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    
    QTextStream in(&file);
    return in.readAll();
}

void HeadlessReadiness::clearLog() {
    if (config_.logFile.isEmpty()) {
        return;
    }
    
    QFile file(config_.logFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.close();
    }
    
    LOG_DEBUG("Headless log cleared");
}

void HeadlessReadiness::emergencyShutdown() {
    QMutexLocker lock(&mutex_);
    
    if (headlessProcess_) {
        headlessProcess_->kill();
        headlessProcess_->waitForFinished(1000);
        delete headlessProcess_;
        headlessProcess_ = nullptr;
    }
    
    headlessRunning_ = false;
    removePidFile();
    
    LOG_WARN("Emergency shutdown performed");
}

void HeadlessReadiness::gracefulShutdown() {
    stopHeadlessMode();
}

void HeadlessReadiness::onHealthCheckTimeout() {
    if (!performHealthCheck()) {
        LOG_WARN("Health check failed");
    }
}

void HeadlessReadiness::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    Q_UNUSED(exitStatus);
    
    QMutexLocker lock(&mutex_);
    
    headlessRunning_ = false;
    removePidFile();
    
    logMessage("Headless process finished with exit code: " + QString::number(exitCode));
    
    emit processCrashed(exitCode);
    emit headlessStopped();
    
    LOG_WARN("Headless process finished", {{"exit_code", exitCode}});
}

void HeadlessReadiness::onProcessError(QProcess::ProcessError error) {
    QString errorMsg;
    
    switch (error) {
        case QProcess::FailedToStart:
            errorMsg = "Failed to start";
            break;
        case QProcess::Crashed:
            errorMsg = "Crashed";
            break;
        case QProcess::Timedout:
            errorMsg = "Timed out";
            break;
        case QProcess::WriteError:
            errorMsg = "Write error";
            break;
        case QProcess::ReadError:
            errorMsg = "Read error";
            break;
        default:
            errorMsg = "Unknown error";
            break;
    }
    
    ERROR_HANDLE("Headless process error", ErrorContext()
        .setSeverity(ErrorSeverity::HIGH)
        .setCategory(ErrorCategory::EXECUTION)
        .setOperation("HeadlessReadiness processError")
        .addMetadata("error", errorMsg));
    
    logMessage("Headless process error: " + errorMsg);
}

bool HeadlessReadiness::startMonitoring() {
    if (healthCheckTimer_) {
        healthCheckTimer_->start();
    }
    
    if (resourceMonitorTimer_) {
        resourceMonitorTimer_->start();
    }
    
    return true;
}

void HeadlessReadiness::stopMonitoring() {
    if (healthCheckTimer_) {
        healthCheckTimer_->stop();
    }
    
    if (resourceMonitorTimer_) {
        resourceMonitorTimer_->stop();
    }
}

void HeadlessReadiness::checkResourceLimits() {
    if (!isWithinLimits()) {
        qint64 memoryUsage = getMemoryUsage();
        int cpuUsage = getCpuUsage();
        
        emit resourceWarning("memory", memoryUsage, config_.maxMemoryMB);
        
        LOG_WARN("Resource limits exceeded", {
            {"memory_usage_mb", memoryUsage},
            {"memory_limit_mb", config_.maxMemoryMB},
            {"cpu_usage_percent", cpuUsage},
            {"cpu_limit_percent", config_.maxCpuPercent}
        });
    }
}

void HeadlessReadiness::logMessage(const QString& message) {
    if (config_.logFile.isEmpty()) {
        return;
    }
    
    QFile file(config_.logFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        QTextStream out(&file);
        out << QDateTime::currentDateTime().toString(Qt::ISODate) << " - " << message << "\n";
        file.close();
    }
}

void HeadlessReadiness::writePidFile() {
    if (config_.pidFile.isEmpty()) {
        return;
    }
    
    QFile file(config_.pidFile);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << headlessProcess_->processId();
        file.close();
    }
}

void HeadlessReadiness::removePidFile() {
    if (config_.pidFile.isEmpty()) {
        return;
    }
    
    QFile::remove(config_.pidFile);
}

HeadlessReadiness::~HeadlessReadiness() {
    shutdown();
}

} // namespace RawrXD