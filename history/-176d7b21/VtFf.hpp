/**
 * @file test_logging.hpp
 * @brief Structured logging for tests
 */

#ifndef TEST_LOGGING_HPP
#define TEST_LOGGING_HPP

#include <QString>
#include <QDebug>
#include <QDateTime>
#include <QJsonObject>
#include <QFile>
#include <memory>
#include <vector>

/**
 * @enum LogLevel
 * @brief Log severity levels
 */
enum class LogLevel
{
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

/**
 * @struct LogEntry
 * @brief Single log entry with metadata
 */
struct LogEntry
{
    LogLevel level;
    QString category;
    QString message;
    QString testName;
    QString fileName;
    int lineNumber;
    QDateTime timestamp;
    
    QJsonObject toJSON() const;
};

/**
 * @class TestLogger
 * @brief Structured logging for test execution
 * 
 * Features:
 * - Per-test logging context
 * - Multiple log levels
 * - Structured JSON format
 * - File and console output
 * - Category-based filtering
 * - Async log writing
 */
class TestLogger
{
public:
    /**
     * Get singleton instance
     */
    static TestLogger& instance();

    /**
     * Set current test context
     */
    void setTestContext(const QString& testName);

    /**
     * Log message at specified level
     */
    void log(LogLevel level, const QString& category, const QString& message,
             const QString& file = QString(), int line = 0);

    /**
     * Convenience methods
     */
    void debug(const QString& category, const QString& message);
    void info(const QString& category, const QString& message);
    void warning(const QString& category, const QString& message);
    void error(const QString& category, const QString& message);
    void critical(const QString& category, const QString& message);

    /**
     * Get logs for current test
     */
    std::vector<LogEntry> getLogsForTest(const QString& testName) const;

    /**
     * Get logs filtered by level
     */
    std::vector<LogEntry> getLogsByLevel(LogLevel level) const;

    /**
     * Get logs filtered by category
     */
    std::vector<LogEntry> getLogsByCategory(const QString& category) const;

    /**
     * Export logs as JSON
     */
    QString exportJSON() const;

    /**
     * Export logs as structured text
     */
    QString exportText() const;

    /**
     * Export logs as HTML
     */
    QString exportHTML() const;

    /**
     * Clear logs
     */
    void clear();

    /**
     * Get log count
     */
    int count() const;

    /**
     * Enable/disable console output
     */
    void setConsoleOutput(bool enabled);

    /**
     * Set log file path
     */
    void setLogFile(const QString& filePath);

    /**
     * Get summary statistics
     */
    struct Statistics {
        int totalLogs;
        int debugCount;
        int infoCount;
        int warningCount;
        int errorCount;
        int criticalCount;
    };
    
    Statistics getStatistics() const;

private:
    TestLogger();
    TestLogger(const TestLogger&) = delete;
    TestLogger& operator=(const TestLogger&) = delete;

    std::vector<LogEntry> m_logs;
    QString m_currentTest;
    QString m_logFilePath;
    bool m_consoleOutput = true;

    QString levelToString(LogLevel level) const;
    LogLevel stringToLevel(const QString& str) const;
};

#endif // TEST_LOGGING_HPP
