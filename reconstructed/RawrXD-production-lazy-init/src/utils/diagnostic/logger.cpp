// Diagnostic Logger Implementation
#include "diagnostic_logger.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QDebug>

namespace RawrXD {

DiagnosticLogger::DiagnosticLogger() {
    // Create logs directory
    QString logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/logs";
    QDir().mkpath(logDir);
    
    QString fileName = QString("%1/diagnostic_%2.log")
        .arg(logDir)
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
    
    m_logFile.setFileName(fileName);
    if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        m_stream.setDevice(&m_logFile);
        log("DiagnosticLogger", "Logging started", "INFO");
    }
}

DiagnosticLogger::~DiagnosticLogger() {
    if (m_logFile.isOpen()) {
        log("DiagnosticLogger", "Logging stopped", "INFO");
        m_logFile.close();
    }
}

DiagnosticLogger& DiagnosticLogger::instance() {
    static DiagnosticLogger instance;
    return instance;
}

void DiagnosticLogger::log(const QString& component, const QString& message, const QString& level) {
    QMutexLocker locker(&m_mutex);
    
    QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
    QString logEntry = QString("[%1] [%2] [%3] %4")
        .arg(timestamp)
        .arg(level)
        .arg(component)
        .arg(message);
    
    // Add to recent logs
    m_recentLogs.append(logEntry);
    if (m_recentLogs.size() > m_maxRecentLogs) {
        m_recentLogs.removeFirst();
    }
    
    // Write to file
    if (m_stream.device()) {
        m_stream << logEntry << "\n";
        m_stream.flush();
    }
    
    // Also output to debug console
    qDebug() << logEntry;
}

void DiagnosticLogger::error(const QString& component, const QString& message) {
    log(component, message, "ERROR");
}

void DiagnosticLogger::warn(const QString& component, const QString& message) {
    log(component, message, "WARN");
}

void DiagnosticLogger::info(const QString& component, const QString& message) {
    log(component, message, "INFO");
}

void DiagnosticLogger::debug(const QString& component, const QString& message) {
    log(component, message, "DEBUG");
}

QString DiagnosticLogger::getRecentLogs(int maxLines) const {
    QMutexLocker locker(&m_mutex);
    
    int start = qMax(0, m_recentLogs.size() - maxLines);
    QStringList recent = m_recentLogs.mid(start);
    return recent.join("\n");
}

void DiagnosticLogger::clearLogs() {
    QMutexLocker locker(&m_mutex);
    m_recentLogs.clear();
}

QString DiagnosticLogger::logFilePath() const {
    return m_logFile.fileName();
}

} // namespace RawrXD