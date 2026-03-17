// Diagnostic Logger - Enhanced logging for manual testing
#pragma once

#include <QString>
#include <QDateTime>
#include <QFile>
#include <QMutex>
#include <QTextStream>
#include <QStandardPaths>
#include <QDir>

namespace RawrXD {

class DiagnosticLogger {
public:
    static DiagnosticLogger& instance();
    
    void log(const QString& component, const QString& message, const QString& level = "INFO");
    void error(const QString& component, const QString& message);
    void warn(const QString& component, const QString& message);
    void info(const QString& component, const QString& message);
    void debug(const QString& component, const QString& message);
    
    QString getRecentLogs(int maxLines = 100) const;
    void clearLogs();
    QString logFilePath() const;
    
private:
    DiagnosticLogger();
    ~DiagnosticLogger();
    
    void writeToFile(const QString& logEntry);
    
    QFile m_logFile;
    QTextStream m_stream;
    mutable QMutex m_mutex;
    QStringList m_recentLogs;
    int m_maxRecentLogs = 1000;
};

// Convenience macros
#define LOG_ERROR(component, message) RawrXD::DiagnosticLogger::instance().error(component, message)
#define LOG_WARN(component, message) RawrXD::DiagnosticLogger::instance().warn(component, message)
#define LOG_INFO(component, message) RawrXD::DiagnosticLogger::instance().info(component, message)
#define LOG_DEBUG(component, message) RawrXD::DiagnosticLogger::instance().debug(component, message)

} // namespace RawrXD