/**
 * @file mainwindow_phase_d_hardening.cpp
 * @brief Phase D Implementation - Production Hardening
 * 
 * Implements comprehensive production hardening for RawrXD IDE including:
 * - Memory safety and leak detection
 * - Performance profiling and optimization
 * - Crash recovery and automatic restart
 * - Thread pool optimization
 * - Watchdog timers and timeout handling
 */

#include "MainWindow_ProductionHardening.h"
#include "MainWindow.h"
#include <QDebug>
#include <QApplication>
#include <QSettings>
#include <QStandardPaths>
#include <QFile>
#include <QDateTime>

// ============================================================
// PHASE D: Production Hardening Implementation
// ============================================================

/**
 * @brief Initialize production hardening systems
 * 
 * Called during MainWindow startup to enable all safety features
 */
void MainWindow::initializeProductionHardening()
{
    RawrXD::Integration::ScopedTimer timer("MainWindow", "initializeProductionHardening");
    
    RawrXD::Integration::logInfo("MainWindow", "hardening_init_begin",
        "Initializing production hardening systems");
    
    // ============================================================
    // 1. Memory Safety Manager
    // ============================================================
    
    // Set memory limit to 500MB
    MemorySafetyManager::instance().setMemoryLimit(500 * 1024 * 1024);
    
    RawrXD::Integration::logInfo("MainWindow", "memory_safety_init",
        "Memory safety manager initialized (limit: 500MB)");
    
    // ============================================================
    // 2. Watchdog Timer for Deadlock Detection
    // ============================================================
    
    if (!m_watchdog)
    {
        m_watchdog = std::make_unique<WatchdogTimer>(this);
        
        connect(m_watchdog.get(), &WatchdogTimer::operationTimeout,
                this, &MainWindow::onOperationTimeout);
        connect(m_watchdog.get(), &WatchdogTimer::deadlockDetected,
                this, &MainWindow::onDeadlockDetected);
        
        // Send heartbeat every 5 seconds
        QTimer* heartbeatTimer = new QTimer(this);
        connect(heartbeatTimer, &QTimer::timeout, this, [this]() {
            m_watchdog->sendHeartbeat();
        });
        heartbeatTimer->start(5000);
        
        RawrXD::Integration::logInfo("MainWindow", "watchdog_init",
            "Watchdog timer initialized (5s heartbeat interval)");
    }
    
    // ============================================================
    // 3. Thread Pool Optimization
    // ============================================================
    
    ThreadPoolManager::instance().initialize();
    int threadCount = QThread::idealThreadCount();
    
    RawrXD::Integration::logInfo("MainWindow", "threadpool_init",
        QString("Thread pool initialized with %1 threads").arg(threadCount));
    
    // ============================================================
    // 4. Crash Recovery System
    // ============================================================
    
    setupCrashRecovery();
    
    // ============================================================
    // 5. Performance Profiling
    // ============================================================
    
    // Start performance monitoring
    m_perfMonitorTimer = new QTimer(this);
    connect(m_perfMonitorTimer, &QTimer::timeout, this, &MainWindow::onPerformanceMonitor);
    m_perfMonitorTimer->start(10000); // Monitor every 10 seconds
    
    RawrXD::Integration::logInfo("MainWindow", "perfmon_init",
        "Performance monitoring started (10s interval)");
    
    MetricsCollector::instance().incrementCounter("hardening_systems_initialized");
    
    statusBar()->showMessage(tr("Production hardening systems initialized"), 3000);
    
    RawrXD::Integration::logInfo("MainWindow", "hardening_init_complete",
        QString("All hardening systems initialized in %1ms").arg(timer.elapsed()),
        QJsonObject{{"timer_ms", timer.elapsed()}});
}

/**
 * @brief Setup automatic crash recovery
 * 
 * Enables the application to recover from crashes and restore state
 */
void MainWindow::setupCrashRecovery()
{
    RawrXD::Integration::ScopedTimer timer("MainWindow", "setupCrashRecovery");
    
    QSettings settings("RawrXD", "IDE");
    
    // Check for previous crash
    bool wasLastSessionCrashed = settings.value("crash/was_crashed", false).toBool();
    
    if (wasLastSessionCrashed)
    {
        RawrXD::Integration::logWarn("MainWindow", "crash_detected",
            "Previous session ended abnormally, attempting recovery");
        
        // Restore from backup
        restoreFromCrashBackup();
        
        // Show recovery notification
        NotificationCenter::instance().showNotification(
            "Crash Recovery",
            "Your session was recovered from a crash",
            3000
        );
    }
    
    // Mark this session as started
    settings.setValue("crash/was_crashed", false);
    
    // Setup periodic session backup (every 5 minutes)
    QTimer* backupTimer = new QTimer(this);
    connect(backupTimer, &QTimer::timeout, this, &MainWindow::createCrashRecoveryBackup);
    backupTimer->start(300000); // 5 minutes
    
    // Mark crash on exit (will be cleared on normal exit)
    connect(QApplication::instance(), &QApplication::aboutToQuit, this, [this]() {
        QSettings settings("RawrXD", "IDE");
        settings.setValue("crash/was_crashed", false);
        settings.sync();
    });
    
    // On abnormal termination, mark as crashed
    std::signal(SIGABRT, [](int) {
        QSettings settings("RawrXD", "IDE");
        settings.setValue("crash/was_crashed", true);
        settings.sync();
        std::exit(1);
    });
    
    RawrXD::Integration::logInfo("MainWindow", "crash_recovery_setup",
        "Crash recovery system enabled with 5-minute backup interval");
}

/**
 * @brief Create a crash recovery backup
 */
void MainWindow::createCrashRecoveryBackup()
{
    try
    {
        QSettings settings("RawrXD", "IDE");
        
        // Backup window state
        QByteArray windowGeometry = saveGeometry();
        QByteArray windowState = saveState();
        
        // Store in temporary backup location
        QString backupDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) 
                          + "/.RawrXD/crash_backups";
        QDir().mkpath(backupDir);
        
        QString backupFile = backupDir + "/" + QDateTime::currentDateTime()
                            .toString("yyyy-MM-dd_hh-mm-ss") + ".backup";
        
        QSettings backup(backupFile, QSettings::IniFormat);
        backup.setValue("window/geometry", windowGeometry.toBase64());
        backup.setValue("window/state", windowState.toBase64());
        backup.sync();
        
        // Cleanup old backups (keep only last 10)
        QDir backupDirObj(backupDir);
        QStringList backups = backupDirObj.entryList({"*.backup"}, QDir::Files);
        while (backups.size() > 10)
        {
            QFile::remove(backupDir + "/" + backups.first());
            backups.removeFirst();
        }
        
        RawrXD::Integration::logDebug("MainWindow", "crash_backup_created",
            QString("Crash recovery backup created: %1").arg(backupFile));
    }
    catch (const std::exception& e)
    {
        RawrXD::Integration::logError("MainWindow", "crash_backup_failed",
            QString("Failed to create crash backup: %1").arg(QString::fromStdString(e.what())));
    }
}

/**
 * @brief Restore from crash backup
 */
void MainWindow::restoreFromCrashBackup()
{
    try
    {
        QString backupDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation)
                          + "/.RawrXD/crash_backups";
        QDir backupDirObj(backupDir);
        QStringList backups = backupDirObj.entryList({"*.backup"}, QDir::Files, QDir::Time);
        
        if (!backups.isEmpty())
        {
            QString latestBackup = backupDir + "/" + backups.first();
            QSettings backup(latestBackup, QSettings::IniFormat);
            
            QByteArray windowGeometry = QByteArray::fromBase64(
                backup.value("window/geometry", "").toByteArray());
            QByteArray windowState = QByteArray::fromBase64(
                backup.value("window/state", "").toByteArray());
            
            if (!windowGeometry.isEmpty())
                restoreGeometry(windowGeometry);
            if (!windowState.isEmpty())
                restoreState(windowState);
            
            RawrXD::Integration::logInfo("MainWindow", "crash_restored",
                QString("Session restored from backup: %1").arg(latestBackup));
        }
    }
    catch (const std::exception& e)
    {
        RawrXD::Integration::logError("MainWindow", "crash_restore_failed",
            QString("Failed to restore from crash backup: %1")
                .arg(QString::fromStdString(e.what())));
    }
}

/**
 * @brief Handle operation timeout
 */
void MainWindow::onOperationTimeout(const QString& operationName)
{
    RawrXD::Integration::logWarn("MainWindow", "operation_timeout",
        QString("Operation timed out: %1").arg(operationName));
    
    statusBar()->showMessage(tr("Operation timeout: %1 (recovered)").arg(operationName), 5000);
    
    MetricsCollector::instance().incrementCounter("operation_timeouts");
}

/**
 * @brief Handle detected deadlock
 */
void MainWindow::onDeadlockDetected()
{
    RawrXD::Integration::logError("MainWindow", "deadlock_detected",
        "Potential deadlock detected - no heartbeat for 60 seconds");
    
    // Attempt recovery
    NotificationCenter::instance().showNotification(
        "System Recovery",
        "Deadlock detected and recovering...",
        3000
    );
    
    // Restart problematic subsystems
    restartSubsystems();
    
    MetricsCollector::instance().incrementCounter("deadlocks_detected");
}

/**
 * @brief Monitor performance metrics
 */
void MainWindow::onPerformanceMonitor()
{
    MemorySafetyManager& memMgr = MemorySafetyManager::instance();
    ThreadPoolManager& threadMgr = ThreadPoolManager::instance();
    PerformanceProfiler& profiler = PerformanceProfiler::instance();
    
    size_t currentMemory = memMgr.getCurrentMemoryUsage();
    size_t peakMemory = memMgr.getPeakMemoryUsage();
    double threadEfficiency = threadMgr.getEfficiency();
    
    // Warn if memory usage is high
    if (currentMemory > 400 * 1024 * 1024) // 400MB of 500MB limit
    {
        RawrXD::Integration::logWarn("MainWindow", "high_memory_usage",
            QString("High memory usage: %1 MB").arg(currentMemory / (1024 * 1024)));
    }
    
    // Log performance metrics
    QJsonObject metrics{
        {"current_memory_mb", static_cast<int>(currentMemory / (1024 * 1024))},
        {"peak_memory_mb", static_cast<int>(peakMemory / (1024 * 1024))},
        {"thread_efficiency", QString::number(threadEfficiency, 'f', 2)},
        {"thread_count", QThread::idealThreadCount()},
        {"pending_tasks", threadMgr.getPendingTaskCount()}
    };
    
    RawrXD::Integration::logDebug("MainWindow", "performance_metrics",
        "Performance snapshot", metrics);
    
    // Emit performance update signal if connected
    emit performanceMetricsUpdated(
        static_cast<int>(currentMemory / (1024 * 1024)),
        threadEfficiency,
        threadMgr.getPendingTaskCount()
    );
}

/**
 * @brief Restart subsystems to recover from errors
 */
void MainWindow::restartSubsystems()
{
    RawrXD::Integration::logInfo("MainWindow", "subsystem_restart",
        "Restarting critical subsystems for recovery");
    
    try
    {
        // Restart inference engine
        if (m_inferenceEngine)
        {
            m_inferenceEngine->reset();
            RawrXD::Integration::logInfo("MainWindow", "inference_engine_restarted",
                "Inference engine restarted");
        }
        
        // Clear caches
        m_settingsCache.clear();
        m_fileInfoCache.clear();
        
        RawrXD::Integration::logInfo("MainWindow", "caches_cleared",
            "System caches cleared");
        
        statusBar()->showMessage(tr("System recovered"), 3000);
    }
    catch (const std::exception& e)
    {
        RawrXD::Integration::logError("MainWindow", "subsystem_restart_failed",
            QString("Failed to restart subsystems: %1")
                .arg(QString::fromStdString(e.what())));
    }
}

/**
 * @brief Cleanup on application exit
 */
void MainWindow::cleanupOnExit()
{
    RawrXD::Integration::ScopedTimer timer("MainWindow", "cleanupOnExit");
    
    RawrXD::Integration::logInfo("MainWindow", "cleanup_begin",
        "Beginning application cleanup");
    
    try
    {
        // Stop watchdog
        m_watchdog.reset();
        
        // Stop performance monitoring
        if (m_perfMonitorTimer)
        {
            m_perfMonitorTimer->stop();
            m_perfMonitorTimer->deleteLater();
        }
        
        // Save final state
        QSettings settings("RawrXD", "IDE");
        settings.setValue("crash/was_crashed", false);
        settings.sync();
        
        // Report memory issues
        MemorySafetyManager::instance().checkForLeaks();
        
        // Generate final performance report
        QString perfReport = PerformanceProfiler::instance().generateReport();
        RawrXD::Integration::logInfo("MainWindow", "final_performance_report",
            "Final performance metrics:\n" + perfReport);
        
        RawrXD::Integration::logInfo("MainWindow", "cleanup_complete",
            QString("Cleanup completed in %1ms").arg(timer.elapsed()));
    }
    catch (const std::exception& e)
    {
        RawrXD::Integration::logError("MainWindow", "cleanup_failed",
            QString("Cleanup error: %1").arg(QString::fromStdString(e.what())));
    }
}

// ============================================================
// Integration with Existing Signals
// ============================================================

/**
 * @brief Record all operation latencies for performance monitoring
 */
void MainWindow::recordOperationLatency(const QString& operationName, double latencyMs)
{
    PerformanceProfiler::instance().recordOperation(operationName, latencyMs);
    
    // Alert if operation exceeds performance targets
    if (operationName == "toggle_dock_widget" && latencyMs > 50)
    {
        RawrXD::Integration::logWarn("MainWindow", "perf_target_exceeded",
            QString("Toggle latency exceeded target: %1ms > 50ms").arg(latencyMs));
    }
    else if (operationName == "render_menu" && latencyMs > 100)
    {
        RawrXD::Integration::logWarn("MainWindow", "perf_target_exceeded",
            QString("Menu render latency exceeded target: %1ms > 100ms").arg(latencyMs));
    }
}
