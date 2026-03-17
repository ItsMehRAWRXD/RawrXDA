#pragma once

#include <QString>
#include <QObject>
#include <QMap>
#include <QStringList>
#include <memory>
#include <functional>

/**
 * @file logging_system.h
 * @brief Centralized logging with file rotation and telemetry
 * 
 * Provides:
 * - Structured logging dispatch
 * - File rotation and archiving
 * - Performance monitoring
 * - Telemetry collection
 * - Debug output filtering
 */

class LoggingSystem : public QObject {
    Q_OBJECT

public:
    enum class LogLevel {
        Debug,     // Detailed information for debugging
        Info,      // General informational messages
        Warning,   // Warning messages
        Error,     // Error messages
        Critical   // Critical messages
    };

    enum class LogCategory {
        General,
        FileSystem,
        Model,
        Build,
        Debug,
        UI,
        Performance,
        Network
    };

    struct LogEntry {
        QString message;
        LogLevel level = LogLevel::Info;
        LogCategory category = LogCategory::General;
        long long timestamp = 0;  // Unix timestamp in ms
        QString sourceFile;
        int sourceLine = 0;
        QString sourceFunction;
        double executionTimeMs = 0.0;
    };

    struct PerformanceMetrics {
        QString componentName;
        double executionTimeMs = 0.0;
        int callCount = 0;
        double averageTimeMs = 0.0;
        double maxTimeMs = 0.0;
        double minTimeMs = 0.0;
    };

    static LoggingSystem& instance();

    /**
     * Log message
     */
    void log(const QString& message,
            LogLevel level = LogLevel::Info,
            LogCategory category = LogCategory::General);

    /**
     * Log with source information
     */
    void logWithSource(const QString& message,
                      LogLevel level,
                      const QString& sourceFile,
                      int sourceLine,
                      const QString& sourceFunction = "");

    /**
     * Log performance metrics
     */
    void logPerformance(const QString& componentName, double executionTimeMs);

    // Convenience methods
    void logInfo(const QString& component, const QString& message);
    void logWarning(const QString& component, const QString& message);
    void logError(const QString& component, const QString& message);
    void logDebug(const QString& component, const QString& message);

    /**
     * Debug-level logging (optimized out in release)
     */
    void debug(const QString& message, LogCategory category = LogCategory::General);

    /**
     * Info-level logging
     */
    void info(const QString& message, LogCategory category = LogCategory::General);

    /**
     * Warning-level logging
     */
    void warning(const QString& message, LogCategory category = LogCategory::General);

    /**
     * Error-level logging
     */
    void error(const QString& message, LogCategory category = LogCategory::General);

    /**
     * Critical-level logging
     */
    void critical(const QString& message, LogCategory category = LogCategory::General);

    /**
     * Get log history
     */
    QList<LogEntry> getLogHistory(int count = 100) const;

    /**
     * Get logs for category
     */
    QList<LogEntry> getLogsForCategory(LogCategory category, int count = 50) const;

    /**
     * Get logs above level
     */
    QList<LogEntry> getLogsAboveLevel(LogLevel level, int count = 50) const;

    /**
     * Clear log history
     */
    void clearLogHistory();

    /**
     * Get performance metrics
     */
    PerformanceMetrics getPerformanceMetrics(const QString& componentName) const;

    /**
     * Get all performance metrics
     */
    QMap<QString, PerformanceMetrics> getAllPerformanceMetrics() const;

    /**
     * Reset performance metrics
     */
    void resetPerformanceMetrics();

    /**
     * Reset specific component metrics
     */
    void resetComponentMetrics(const QString& componentName);

    /**
     * Save logs to file
     */
    bool saveLogsToFile(const QString& filePath);

    /**
     * Enable/disable file logging
     */
    void setFileLoggingEnabled(bool enabled);

    /**
     * Check if file logging is enabled
     */
    bool isFileLoggingEnabled() const;

    /**
     * Set log file path
     */
    void setLogFilePath(const QString& path);

    /**
     * Get log file path
     */
    QString getLogFilePath() const;

    /**
     * Enable/disable rotation
     */
    void setRotationEnabled(bool enabled);

    /**
     * Set max log file size (bytes) before rotation
     */
    void setMaxLogFileSize(int sizeBytes);

    /**
     * Set max log files to keep
     */
    void setMaxLogFiles(int count);

    /**
     * Set debug output filter (category)
     */
    void setDebugFilter(LogCategory category);

    /**
     * Get debug filter
     */
    LogCategory getDebugFilter() const;

    /**
     * Enable/disable console output
     */
    void setConsoleOutputEnabled(bool enabled);

    /**
     * Check if console output enabled
     */
    bool isConsoleOutputEnabled() const;

    /**
     * Get statistics
     */
    struct Statistics {
        int totalLogEntries = 0;
        int debugEntries = 0;
        int infoEntries = 0;
        int warningEntries = 0;
        int errorEntries = 0;
        int criticalEntries = 0;
        double totalFileSizeBytes = 0;
    };

    Statistics getStatistics() const;

    /**
     * Get level name
     */
    static QString getLevelName(LogLevel level);

    /**
     * Get category name
     */
    static QString getCategoryName(LogCategory category);

signals:
    /**
     * Log entry added
     */
    void logEntryAdded(const LogEntry& entry);

    /**
     * Log file rotated
     */
    void logFileRotated(const QString& oldPath, const QString& newPath);

    /**
     * Performance metric recorded
     */
    void performanceMetricRecorded(const QString& componentName, double timeMs);

private:
    LoggingSystem();
    ~LoggingSystem() override;

    QList<LogEntry> m_logHistory;
    QMap<QString, PerformanceMetrics> m_performanceMetrics;
    QMap<QString, int> m_categoryLogCount;

    bool m_fileLoggingEnabled = true;
    bool m_consoleOutputEnabled = true;
    bool m_rotationEnabled = true;

    QString m_logFilePath;
    int m_maxLogFileSize = 10 * 1024 * 1024;  // 10 MB
    int m_maxLogFiles = 5;
    LogCategory m_debugFilter = LogCategory::General;

    static constexpr int MAX_HISTORY_SIZE = 5000;
    static constexpr const char* LOG_FILENAME = "rawrxd.log";

    void rotateLogFileIfNeeded();
    void writeLogToFile(const LogEntry& entry);
    QString formatLogEntry(const LogEntry& entry) const;
    void ensureLogDirectory();
};
