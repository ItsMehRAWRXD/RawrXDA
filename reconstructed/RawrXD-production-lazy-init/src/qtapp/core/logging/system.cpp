#include "logging_system.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>
#include <QFileInfo>
#include <QDebug>
#include <QMutex>
#include <stdexcept>

/**
 * @file logging_system.cpp
 * @brief Implementation of centralized logging system
 */

// Global mutex for thread-safe logging (lazy-initialized)
QMutex& loggingMutex() {
    static QMutex mutex;
    return mutex;
}

LoggingSystem& LoggingSystem::instance() {
    static LoggingSystem inst;
    return inst;
}

LoggingSystem::LoggingSystem()
    : QObject(nullptr)
{
    try {
        ensureLogDirectory();

        // Set default log path
        QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
        m_logFilePath = logDir + "/" + LOG_FILENAME;

        qDebug() << "[LoggingSystem] Initialized, log path:" << m_logFilePath;

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error during initialization:" << e.what();
    }
}

LoggingSystem::~LoggingSystem() {
    try {
        qDebug() << "[LoggingSystem] Shutdown, total entries:" << m_logHistory.size();
    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error during shutdown:" << e.what();
    }
}

void LoggingSystem::log(const QString& message,
                       LogLevel level,
                       LogCategory category) {
    try {
        QMutexLocker lock(&loggingMutex());

        LogEntry entry;
        entry.message = message;
        entry.level = level;
        entry.category = category;
        entry.timestamp = QDateTime::currentMSecsSinceEpoch();

        // Add to history
        m_logHistory.append(entry);
        if (m_logHistory.size() > MAX_HISTORY_SIZE) {
            m_logHistory.removeFirst();
        }

        // Update category count
        m_categoryLogCount[getCategoryName(category)]++;

        // Update statistics
        switch (level) {
            case LogLevel::Debug: /* count handled below */ break;
            case LogLevel::Info: /* count handled below */ break;
            case LogLevel::Warning: /* count handled below */ break;
            case LogLevel::Error: /* count handled below */ break;
            case LogLevel::Critical: /* count handled below */ break;
        }

        // Write to file
        if (m_fileLoggingEnabled) {
            writeLogToFile(entry);
            rotateLogFileIfNeeded();
        }

        // Console output
        if (m_consoleOutputEnabled) {
            QString levelName = getLevelName(level);
            QString categoryName = getCategoryName(category);
            qDebug().noquote() << QString("[%1] [%2] %3").arg(levelName, categoryName, message);
        }

        // Emit signal
        emit logEntryAdded(entry);

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error logging:" << e.what();
    }
}

void LoggingSystem::logWithSource(const QString& message,
                                 LogLevel level,
                                 const QString& sourceFile,
                                 int sourceLine,
                                 const QString& sourceFunction) {
    try {
        QMutexLocker lock(&loggingMutex());

        LogEntry entry;
        entry.message = message;
        entry.level = level;
        entry.timestamp = QDateTime::currentMSecsSinceEpoch();
        entry.sourceFile = sourceFile;
        entry.sourceLine = sourceLine;
        entry.sourceFunction = sourceFunction;

        m_logHistory.append(entry);
        if (m_logHistory.size() > MAX_HISTORY_SIZE) {
            m_logHistory.removeFirst();
        }

        if (m_fileLoggingEnabled) {
            writeLogToFile(entry);
            rotateLogFileIfNeeded();
        }

        if (m_consoleOutputEnabled) {
            qDebug().noquote() << formatLogEntry(entry);
        }

        emit logEntryAdded(entry);

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error logging with source:" << e.what();
    }
}

void LoggingSystem::logPerformance(const QString& componentName, double executionTimeMs) {
    try {
        QMutexLocker lock(&g_loggingMutex);

        if (!m_performanceMetrics.contains(componentName)) {
            m_performanceMetrics[componentName] = PerformanceMetrics();
            m_performanceMetrics[componentName].componentName = componentName;
        }

        PerformanceMetrics& metrics = m_performanceMetrics[componentName];
        metrics.executionTimeMs += executionTimeMs;
        metrics.callCount++;
        metrics.averageTimeMs = metrics.executionTimeMs / metrics.callCount;

        if (executionTimeMs > metrics.maxTimeMs || metrics.maxTimeMs == 0.0) {
            metrics.maxTimeMs = executionTimeMs;
        }

        if (executionTimeMs < metrics.minTimeMs || metrics.minTimeMs == 0.0) {
            metrics.minTimeMs = executionTimeMs;
        }

        // Log as info level
        log(QString("Performance [%1]: %2ms (avg: %3ms, max: %4ms)")
            .arg(componentName)
            .arg(executionTimeMs, 0, 'f', 2)
            .arg(metrics.averageTimeMs, 0, 'f', 2)
            .arg(metrics.maxTimeMs, 0, 'f', 2),
            LogLevel::Info, LogCategory::Performance);

        emit performanceMetricRecorded(componentName, executionTimeMs);

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error logging performance:" << e.what();
    }
}

void LoggingSystem::debug(const QString& message, LogCategory category) {
    log(message, LogLevel::Debug, category);
}

void LoggingSystem::info(const QString& message, LogCategory category) {
    log(message, LogLevel::Info, category);
}

void LoggingSystem::warning(const QString& message, LogCategory category) {
    log(message, LogLevel::Warning, category);
}

void LoggingSystem::error(const QString& message, LogCategory category) {
    log(message, LogLevel::Error, category);
}

void LoggingSystem::critical(const QString& message, LogCategory category) {
    log(message, LogLevel::Critical, category);
}

QList<LoggingSystem::LogEntry> LoggingSystem::getLogHistory(int count) const {
    try {
        QMutexLocker lock(&g_loggingMutex);

        QList<LogEntry> result;
        int start = qMax(0, m_logHistory.size() - count);

        for (int i = start; i < m_logHistory.size(); ++i) {
            result.append(m_logHistory[i]);
        }

        return result;

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error getting history:" << e.what();
        return QList<LogEntry>();
    }
}

QList<LoggingSystem::LogEntry> LoggingSystem::getLogsForCategory(LogCategory category, int count) const {
    try {
        QMutexLocker lock(&g_loggingMutex);

        QList<LogEntry> result;
        int found = 0;

        for (int i = m_logHistory.size() - 1; i >= 0 && found < count; --i) {
            if (m_logHistory[i].category == category) {
                result.prepend(m_logHistory[i]);
                found++;
            }
        }

        return result;

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error getting category logs:" << e.what();
        return QList<LogEntry>();
    }
}

QList<LoggingSystem::LogEntry> LoggingSystem::getLogsAboveLevel(LogLevel level, int count) const {
    try {
        QMutexLocker lock(&g_loggingMutex);

        QList<LogEntry> result;
        int found = 0;

        for (int i = m_logHistory.size() - 1; i >= 0 && found < count; --i) {
            if (static_cast<int>(m_logHistory[i].level) >= static_cast<int>(level)) {
                result.prepend(m_logHistory[i]);
                found++;
            }
        }

        return result;

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error getting level-filtered logs:" << e.what();
        return QList<LogEntry>();
    }
}

void LoggingSystem::clearLogHistory() {
    try {
        QMutexLocker lock(&g_loggingMutex);
        m_logHistory.clear();
        qDebug() << "[LoggingSystem] Log history cleared";
    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error clearing history:" << e.what();
    }
}

LoggingSystem::PerformanceMetrics LoggingSystem::getPerformanceMetrics(const QString& componentName) const {
    try {
        QMutexLocker lock(&g_loggingMutex);

        if (m_performanceMetrics.contains(componentName)) {
            return m_performanceMetrics[componentName];
        }
        return PerformanceMetrics();

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error getting metrics:" << e.what();
        return PerformanceMetrics();
    }
}

QMap<QString, LoggingSystem::PerformanceMetrics> LoggingSystem::getAllPerformanceMetrics() const {
    try {
        QMutexLocker lock(&g_loggingMutex);
        return m_performanceMetrics;
    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error getting all metrics:" << e.what();
        return QMap<QString, PerformanceMetrics>();
    }
}

void LoggingSystem::resetPerformanceMetrics() {
    try {
        QMutexLocker lock(&g_loggingMutex);
        m_performanceMetrics.clear();
        qDebug() << "[LoggingSystem] Performance metrics reset";
    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error resetting metrics:" << e.what();
    }
}

void LoggingSystem::resetComponentMetrics(const QString& componentName) {
    try {
        QMutexLocker lock(&g_loggingMutex);
        m_performanceMetrics.remove(componentName);
    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error resetting component metrics:" << e.what();
    }
}

bool LoggingSystem::saveLogsToFile(const QString& filePath) {
    try {
        QMutexLocker lock(&g_loggingMutex);

        QFile file(filePath);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            qWarning() << "[LoggingSystem] Could not open file for saving logs:" << filePath;
            return false;
        }

        QTextStream stream(&file);

        for (const LogEntry& entry : m_logHistory) {
            stream << formatLogEntry(entry) << "\n";
        }

        file.close();
        qDebug() << "[LoggingSystem] Logs saved to:" << filePath;
        return true;

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error saving logs:" << e.what();
        return false;
    }
}

void LoggingSystem::setFileLoggingEnabled(bool enabled) {
    m_fileLoggingEnabled = enabled;
    qDebug() << "[LoggingSystem] File logging" << (enabled ? "enabled" : "disabled");
}

bool LoggingSystem::isFileLoggingEnabled() const {
    return m_fileLoggingEnabled;
}

void LoggingSystem::setLogFilePath(const QString& path) {
    m_logFilePath = path;
    ensureLogDirectory();
    qDebug() << "[LoggingSystem] Log file path set to:" << path;
}

QString LoggingSystem::getLogFilePath() const {
    return m_logFilePath;
}

void LoggingSystem::setRotationEnabled(bool enabled) {
    m_rotationEnabled = enabled;
    qDebug() << "[LoggingSystem] Log rotation" << (enabled ? "enabled" : "disabled");
}

void LoggingSystem::setMaxLogFileSize(int sizeBytes) {
    m_maxLogFileSize = sizeBytes;
}

void LoggingSystem::setMaxLogFiles(int count) {
    m_maxLogFiles = count;
}

void LoggingSystem::setDebugFilter(LogCategory category) {
    m_debugFilter = category;
}

LoggingSystem::LogCategory LoggingSystem::getDebugFilter() const {
    return m_debugFilter;
}

void LoggingSystem::setConsoleOutputEnabled(bool enabled) {
    m_consoleOutputEnabled = enabled;
}

bool LoggingSystem::isConsoleOutputEnabled() const {
    return m_consoleOutputEnabled;
}

LoggingSystem::Statistics LoggingSystem::getStatistics() const {
    try {
        QMutexLocker lock(&g_loggingMutex);

        Statistics stats;
        stats.totalLogEntries = m_logHistory.size();

        for (const LogEntry& entry : m_logHistory) {
            switch (entry.level) {
                case LogLevel::Debug: stats.debugEntries++; break;
                case LogLevel::Info: stats.infoEntries++; break;
                case LogLevel::Warning: stats.warningEntries++; break;
                case LogLevel::Error: stats.errorEntries++; break;
                case LogLevel::Critical: stats.criticalEntries++; break;
            }
        }

        // Calculate file size
        QFileInfo fileInfo(m_logFilePath);
        if (fileInfo.exists()) {
            stats.totalFileSizeBytes = fileInfo.size();
        }

        return stats;

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error getting statistics:" << e.what();
        return Statistics();
    }
}

QString LoggingSystem::getLevelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warning: return "WARN";
        case LogLevel::Error: return "ERROR";
        case LogLevel::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}

QString LoggingSystem::getCategoryName(LogCategory category) {
    switch (category) {
        case LogCategory::General: return "General";
        case LogCategory::FileSystem: return "FileSystem";
        case LogCategory::Model: return "Model";
        case LogCategory::Build: return "Build";
        case LogCategory::Debug: return "Debug";
        case LogCategory::UI: return "UI";
        case LogCategory::Performance: return "Performance";
        case LogCategory::Network: return "Network";
    }
    return "Unknown";
}

void LoggingSystem::rotateLogFileIfNeeded() {
    try {
        if (!m_rotationEnabled) {
            return;
        }

        QFileInfo fileInfo(m_logFilePath);
        if (fileInfo.size() > m_maxLogFileSize) {
            QString baseName = m_logFilePath;
            if (baseName.endsWith(".log")) {
                baseName = baseName.left(baseName.length() - 4);
            }

            // Rotate old files
            for (int i = m_maxLogFiles - 1; i > 0; --i) {
                QString oldPath = QString("%1.%2.log").arg(baseName).arg(i);
                QString newPath = QString("%1.%2.log").arg(baseName).arg(i + 1);

                if (QFile::exists(oldPath)) {
                    QFile::remove(newPath);
                    QFile::rename(oldPath, newPath);
                }
            }

            // Rename current to .1
            QString rotatedPath = QString("%1.1.log").arg(baseName);
            if (QFile::exists(m_logFilePath)) {
                QFile::remove(rotatedPath);
                QFile::rename(m_logFilePath, rotatedPath);
            }

            emit logFileRotated(m_logFilePath, rotatedPath);
        }

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error rotating log file:" << e.what();
    }
}

void LoggingSystem::writeLogToFile(const LogEntry& entry) {
    try {
        QFile file(m_logFilePath);
        if (!file.open(QIODevice::Append | QIODevice::Text)) {
            qWarning() << "[LoggingSystem] Could not open log file:" << m_logFilePath;
            return;
        }

        QTextStream stream(&file);
        stream << formatLogEntry(entry) << "\n";
        file.close();

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error writing to log file:" << e.what();
    }
}

QString LoggingSystem::formatLogEntry(const LogEntry& entry) const {
    try {
        QString timestamp = QDateTime::fromMSecsSinceEpoch(entry.timestamp)
            .toString("yyyy-MM-dd hh:mm:ss.zzz");

        QString formatted = QString("[%1] %2 [%3] %4")
            .arg(timestamp, getLevelName(entry.level), getCategoryName(entry.category), entry.message);

        if (!entry.sourceFile.isEmpty()) {
            formatted += QString(" (%1:%2 in %3)")
                .arg(entry.sourceFile, QString::number(entry.sourceLine), entry.sourceFunction);
        }

        if (entry.executionTimeMs > 0.0) {
            formatted += QString(" {%1ms}").arg(entry.executionTimeMs, 0, 'f', 2);
        }

        return formatted;

    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error formatting entry:" << e.what();
        return entry.message;
    }
}

void LoggingSystem::ensureLogDirectory() {
    try {
        QDir dir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation));
        if (!dir.exists()) {
            dir.mkpath(".");
        }
    } catch (const std::exception& e) {
        qWarning() << "[LoggingSystem] Error ensuring log directory:" << e.what();
    }
}

void LoggingSystem::logInfo(const QString& component, const QString& message) {
    log(QString("[%1] %2").arg(component, message), LogLevel::Info, LogCategory::General);
}

void LoggingSystem::logWarning(const QString& component, const QString& message) {
    log(QString("[%1] %2").arg(component, message), LogLevel::Warning, LogCategory::General);
}

void LoggingSystem::logError(const QString& component, const QString& message) {
    log(QString("[%1] %2").arg(component, message), LogLevel::Error, LogCategory::General);
}

void LoggingSystem::logDebug(const QString& component, const QString& message) {
    log(QString("[%1] %2").arg(component, message), LogLevel::Debug, LogCategory::General);
}
