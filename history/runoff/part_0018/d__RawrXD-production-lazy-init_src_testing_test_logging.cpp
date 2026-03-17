/**
 * @file test_logging.cpp
 * @brief Implementation of structured test logging
 */

#include "test_logging.hpp"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QTextStream>
#include <QMutex>
#include <iostream>

// ─────────────────────────────────────────────────────────────────────
// LogEntry Implementation
// ─────────────────────────────────────────────────────────────────────

QJsonObject LogEntry::toJSON() const
{
    QJsonObject obj;
    obj["level"] = static_cast<int>(level);
    obj["category"] = category;
    obj["message"] = message;
    obj["test_name"] = testName;
    obj["file"] = fileName;
    obj["line"] = lineNumber;
    obj["timestamp"] = timestamp.toString(Qt::ISODate);
    return obj;
}

// ─────────────────────────────────────────────────────────────────────
// TestLogger Implementation
// ─────────────────────────────────────────────────────────────────────

TestLogger& TestLogger::instance()
{
    static TestLogger logger;
    return logger;
}

TestLogger::TestLogger()
{
}

void TestLogger::setTestContext(const QString& testName)
{
    m_currentTest = testName;
}

void TestLogger::log(LogLevel level, const QString& category, const QString& message,
                     const QString& file, int line)
{
    LogEntry entry;
    entry.level = level;
    entry.category = category;
    entry.message = message;
    entry.testName = m_currentTest;
    entry.fileName = file;
    entry.lineNumber = line;
    entry.timestamp = QDateTime::currentDateTime();
    
    m_logs.push_back(entry);
    
    // Console output
    if (m_consoleOutput) {
        QString levelStr = levelToString(level);
        QString output = QString("[%1] %2: %3 (%4)")
            .arg(entry.testName)
            .arg(levelStr)
            .arg(category)
            .arg(message);
        
        if (!file.isEmpty()) {
            output += QString(" at %1:%2").arg(file).arg(line);
        }
        
        std::cout << output.toStdString() << std::endl;
    }
    
    // File output
    if (!m_logFilePath.isEmpty()) {
        QFile logFile(m_logFilePath);
        if (logFile.open(QIODevice::Append | QIODevice::Text)) {
            QTextStream stream(&logFile);
            stream << entry.timestamp.toString(Qt::ISODate) << " | "
                   << levelToString(level) << " | "
                   << category << " | "
                   << message << "\n";
            logFile.close();
        }
    }
}

void TestLogger::debug(const QString& category, const QString& message)
{
    log(LogLevel::DEBUG, category, message);
}

void TestLogger::info(const QString& category, const QString& message)
{
    log(LogLevel::INFO, category, message);
}

void TestLogger::warning(const QString& category, const QString& message)
{
    log(LogLevel::WARNING, category, message);
}

void TestLogger::error(const QString& category, const QString& message)
{
    log(LogLevel::ERROR, category, message);
}

void TestLogger::critical(const QString& category, const QString& message)
{
    log(LogLevel::CRITICAL, category, message);
}

std::vector<LogEntry> TestLogger::getLogsForTest(const QString& testName) const
{
    std::vector<LogEntry> result;
    for (const auto& entry : m_logs) {
        if (entry.testName == testName) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<LogEntry> TestLogger::getLogsByLevel(LogLevel level) const
{
    std::vector<LogEntry> result;
    for (const auto& entry : m_logs) {
        if (entry.level == level) {
            result.push_back(entry);
        }
    }
    return result;
}

std::vector<LogEntry> TestLogger::getLogsByCategory(const QString& category) const
{
    std::vector<LogEntry> result;
    for (const auto& entry : m_logs) {
        if (entry.category == category) {
            result.push_back(entry);
        }
    }
    return result;
}

QString TestLogger::exportJSON() const
{
    QJsonArray array;
    for (const auto& entry : m_logs) {
        array.append(entry.toJSON());
    }
    
    QJsonDocument doc(array);
    return QString::fromUtf8(doc.toJson());
}

QString TestLogger::exportText() const
{
    QString text = "=== Test Logs ===\n\n";
    
    for (const auto& entry : m_logs) {
        text += QString("[%1] %2 - %3\n")
            .arg(entry.timestamp.toString("HH:mm:ss.zzz"))
            .arg(levelToString(entry.level))
            .arg(entry.message);
        
        if (!entry.fileName.isEmpty()) {
            text += QString("    Location: %1:%2\n").arg(entry.fileName).arg(entry.lineNumber);
        }
    }
    
    return text;
}

QString TestLogger::exportHTML() const
{
    QString html = R"(
    <!DOCTYPE html>
    <html>
    <head>
        <title>Test Execution Logs</title>
        <style>
            body { font-family: monospace; margin: 20px; background: #f5f5f5; }
            .log-entry { background: white; margin: 5px 0; padding: 10px; border-radius: 3px; }
            .DEBUG { border-left: 4px solid #666; }
            .INFO { border-left: 4px solid #2196F3; }
            .WARNING { border-left: 4px solid #FF9800; }
            .ERROR { border-left: 4px solid #F44336; }
            .CRITICAL { border-left: 4px solid #9C27B0; background: #fce4ec; }
            .timestamp { color: #666; font-size: 0.9em; }
            .level { font-weight: bold; }
        </style>
    </head>
    <body>
        <h1>Test Execution Logs</h1>
    )";
    
    for (const auto& entry : m_logs) {
        QString levelStr = levelToString(entry.level);
        html += QString(R"(
        <div class="log-entry %1">
            <div class="timestamp">%2</div>
            <div class="level">%3</div>
            <div class="message">%4</div>
        </div>
        )")
            .arg(levelStr)
            .arg(entry.timestamp.toString("HH:mm:ss.zzz"))
            .arg(levelStr)
            .arg(entry.message);
    }
    
    html += R"(
    </body>
    </html>
    )";
    
    return html;
}

void TestLogger::clear()
{
    m_logs.clear();
}

int TestLogger::count() const
{
    return m_logs.size();
}

void TestLogger::setConsoleOutput(bool enabled)
{
    m_consoleOutput = enabled;
}

void TestLogger::setLogFile(const QString& filePath)
{
    m_logFilePath = filePath;
    
    // Clear existing log file
    QFile logFile(filePath);
    if (logFile.exists()) {
        logFile.remove();
    }
}

TestLogger::Statistics TestLogger::getStatistics() const
{
    Statistics stats = {0, 0, 0, 0, 0, 0};
    
    for (const auto& entry : m_logs) {
        stats.totalLogs++;
        
        switch (entry.level) {
        case LogLevel::DEBUG:
            stats.debugCount++;
            break;
        case LogLevel::INFO:
            stats.infoCount++;
            break;
        case LogLevel::WARNING:
            stats.warningCount++;
            break;
        case LogLevel::ERROR:
            stats.errorCount++;
            break;
        case LogLevel::CRITICAL:
            stats.criticalCount++;
            break;
        }
    }
    
    return stats;
}

QString TestLogger::levelToString(LogLevel level) const
{
    switch (level) {
    case LogLevel::DEBUG:
        return "DEBUG";
    case LogLevel::INFO:
        return "INFO";
    case LogLevel::WARNING:
        return "WARN";
    case LogLevel::ERROR:
        return "ERROR";
    case LogLevel::CRITICAL:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

LogLevel TestLogger::stringToLevel(const QString& str) const
{
    if (str == "DEBUG") return LogLevel::DEBUG;
    if (str == "INFO") return LogLevel::INFO;
    if (str == "WARN") return LogLevel::WARNING;
    if (str == "ERROR") return LogLevel::ERROR;
    if (str == "CRITICAL") return LogLevel::CRITICAL;
    return LogLevel::INFO;
}
