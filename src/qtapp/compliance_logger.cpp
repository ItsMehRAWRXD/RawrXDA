#include "compliance_logger.hpp"
#include <QStandardPaths>
#include <QDir>
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QHostInfo>
#include <QNetworkInterface>

ComplianceLogger& ComplianceLogger::instance() {
    static ComplianceLogger instance;
    return instance;
}

ComplianceLogger::ComplianceLogger()
    : QObject(nullptr)
{
}

ComplianceLogger::~ComplianceLogger() {
    stop();
}

void ComplianceLogger::start(const QString& logFilePath) {
    QMutexLocker locker(&m_mutex);
    
    if (m_running) {
        qInfo() << "[ComplianceLogger] Already running";
        return;
    }
    
    // Default log path
    if (logFilePath.isEmpty()) {
        QString appData = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        m_logFilePath = appData + "/logs/compliance.log";
    } else {
        m_logFilePath = logFilePath;
    }
    
    // Ensure log directory exists
    QFileInfo fi(m_logFilePath);
    QDir().mkpath(fi.absolutePath());
    
    // Open log file in append mode
    m_logFile = new QFile(m_logFilePath);
    if (!m_logFile->open(QIODevice::Append | QIODevice::Text)) {
        qCritical() << "[ComplianceLogger] Failed to open log file:" << m_logFilePath;
        delete m_logFile;
        m_logFile = nullptr;
        return;
    }
    
    m_logStream = new QTextStream(m_logFile);
    m_running = true;
    
    qInfo() << "[ComplianceLogger] Started compliance logging";
    qInfo() << "[ComplianceLogger] Log file:" << m_logFilePath;
    qInfo() << "[ComplianceLogger] Retention period:" << m_retentionDays << "days";
    
    // Log startup event
    logEvent(LogLvl_Aud4, EvType_Sys4, "system", "ComplianceLoggingStarted");
}

void ComplianceLogger::stop() {
    QMutexLocker locker(&m_mutex);
    
    if (!m_running) return;
    
    // Log shutdown event
    LogEntry shutdownEntry;
    shutdownEntry.timestamp = QDateTime::currentDateTime();
    shutdownEntry.level = LogLvl_Aud4;
    shutdownEntry.eventType = EvType_Sys4;
    shutdownEntry.userId = "system";
    shutdownEntry.action = "ComplianceLoggingStopped";
    writeLogEntry(shutdownEntry);
    
    if (m_logStream) {
        delete m_logStream;
        m_logStream = nullptr;
    }
    
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }
    
    m_running = false;
    qInfo() << "[ComplianceLogger] Stopped";
}

void ComplianceLogger::logEvent(LogLevel level, EventType eventType,
                                const QString& userId, const QString& action,
                                const QString& resourceId, const QString& details) {
    QMutexLocker locker(&m_mutex);
    
    if (!m_running) return;
    
    LogEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.eventType = eventType;
    entry.userId = userId;
    entry.action = action;
    entry.resourceId = resourceId;
    entry.details = details;
    
    // Get IP address (for audit trail)
    foreach (const QHostAddress &address, QNetworkInterface::allAddresses()) {
        if (address.protocol() == QAbstractSocket::IPv4Protocol && 
            !address.isLoopback()) {
            entry.ipAddress = address.toString();
            break;
        }
    }
    
    // Calculate tamper-evident checksum
    entry.checksum = calculateEntryChecksum(entry);
    
    writeLogEntry(entry);
    
    emit eventLogged(entry);
    
    // Emit security alerts for critical events
    if (level == LogLvl_Sec3 || eventType == EvType_SecViol5) {
        emit securityAlert(QString("%1: %2 by %3")
            .arg(eventTypeToString(eventType))
            .arg(action)
            .arg(userId));
    }
}

void ComplianceLogger::logModelAccess(const QString& userId, const QString& modelPath,
                                      const QString& action) {
    logEvent(LogLvl_Aud4, EvType_Model0, userId, action, modelPath,
             QString("Model: %1").arg(modelPath));
}

void ComplianceLogger::logDataAccess(const QString& userId, const QString& dataPath,
                                     const QString& action) {
    // HIPAA requirement: Log all PHI access
    logEvent(LogLvl_Aud4, EvType_Data1, userId, action, dataPath,
             QString("Data: %1").arg(dataPath));
}

void ComplianceLogger::logConfigChange(const QString& userId, const QString& setting,
                                       const QString& oldValue, const QString& newValue) {
    // SOC2 requirement: Track all configuration changes
    QString details = QString("Setting: %1, Old: %2, New: %3")
        .arg(setting)
        .arg(oldValue)
        .arg(newValue);
    
    logEvent(LogLvl_Aud4, EvType_Cfg3, userId, "ConfigurationModified", setting, details);
}

void ComplianceLogger::logSecurityViolation(const QString& userId, const QString& violation) {
    logEvent(LogLvl_Sec3, EvType_SecViol5, userId, violation, QString(),
             QString("Security violation: %1").arg(violation));
    
    emit complianceViolation(violation);
}

void ComplianceLogger::logUserLogin(const QString& userId, bool success, const QString& ipAddress) {
    QString action = success ? "LoginSuccess" : "LoginFailed";
    QString details = QString("Login %1 from %2").arg(success ? "successful" : "failed").arg(ipAddress);
    
    LogLevel level = success ? LogLvl_Info0 : LogLvl_Warn1;
    
    logEvent(level, EvType_User2, userId, action, QString(), details);
}

void ComplianceLogger::logSystemError(const QString& component, const QString& errorMessage) {
    logEvent(LogLvl_Err2, EvType_Sys4, "system", "SystemError", component, 
             QString("Component: %1, Error: %2").arg(component).arg(errorMessage));
}

QString ComplianceLogger::exportAuditLog(const QDateTime& startDate, const QDateTime& endDate) const {
    QMutexLocker locker(&m_mutex);
    
    QJsonArray entries;
    
    QFile file(m_logFilePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "[ComplianceLogger] Failed to open log for export";
        return QString();
    }
    
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        QString line = stream.readLine();
        
        // Parse log entry (simplified - actual parsing would be more robust)
        if (line.contains("[AUDIT]") || line.contains("[SECURITY]")) {
            QJsonObject entry;
            entry["logLine"] = line;
            entries.append(entry);
        }
    }
    
    file.close();
    
    QJsonObject root;
    root["exportDate"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["startDate"] = startDate.toString(Qt::ISODate);
    root["endDate"] = endDate.toString(Qt::ISODate);
    root["totalEntries"] = entries.size();
    root["entries"] = entries;
    
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

void ComplianceLogger::rotateLogs() {
    QMutexLocker locker(&m_mutex);
    
    if (!m_running) return;
    
    // Close current log
    if (m_logStream) {
        delete m_logStream;
        m_logStream = nullptr;
    }
    if (m_logFile) {
        m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }
    
    // Rename current log with timestamp
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString rotatedPath = m_logFilePath + "." + timestamp;
    
    QFile::rename(m_logFilePath, rotatedPath);
    
    // Open new log
    m_logFile = new QFile(m_logFilePath);
    m_logFile->open(QIODevice::Append | QIODevice::Text);
    m_logStream = new QTextStream(m_logFile);
    
    qInfo() << "[ComplianceLogger] Log rotated to:" << rotatedPath;
    
    // Log rotation event
    logEvent(LogLvl_Aud4, EvType_Sys4, "system", "LogRotated", rotatedPath);
}

void ComplianceLogger::setRetentionPeriod(int days) {
    QMutexLocker locker(&m_mutex);
    m_retentionDays = days;
    qInfo() << "[ComplianceLogger] Retention period set to:" << days << "days";
}

void ComplianceLogger::writeLogEntry(const LogEntry& entry) {
    if (!m_logStream) return;
    
    QString logLine = formatLogEntry(entry);
    *m_logStream << logLine << "\n";
    m_logStream->flush();
}

QString ComplianceLogger::calculateEntryChecksum(const LogEntry& entry) const {
    QCryptographicHash hash(QCryptographicHash::Sha256);
    
    QString data = QString("%1|%2|%3|%4|%5|%6|%7|%8")
        .arg(entry.timestamp.toString(Qt::ISODate))
        .arg(entry.level)
        .arg(entry.eventType)
        .arg(entry.userId)
        .arg(entry.action)
        .arg(entry.resourceId)
        .arg(entry.ipAddress)
        .arg(entry.details);
    
    hash.addData(data.toUtf8());
    return QString(hash.result().toHex());
}

QString ComplianceLogger::formatLogEntry(const LogEntry& entry) const {
    return QString("[%1] [%2] [%3] User=%4 Action=%5 Resource=%6 IP=%7 Details=%8 Checksum=%9")
        .arg(entry.timestamp.toString(Qt::ISODateWithMs))
        .arg(logLevelToString(entry.level))
        .arg(eventTypeToString(entry.eventType))
        .arg(entry.userId)
        .arg(entry.action)
        .arg(entry.resourceId)
        .arg(entry.ipAddress)
        .arg(entry.details)
        .arg(entry.checksum);
}

QString ComplianceLogger::eventTypeToString(EventType type) const {
    switch (type) {
        case EvType_Model0: return "MODEL_ACCESS";
        case EvType_Data1: return "DATA_ACCESS";
        case EvType_User2: return "USER_LOGIN";
        case EvType_Cfg3: return "CONFIG_CHANGE";
        case EvType_Sys4: return "SYSTEM_EVENT";
        case EvType_SecViol5: return "SECURITY_VIOLATION";
        default: return "UNKNOWN";
    }
}

QString ComplianceLogger::logLevelToString(LogLevel level) const {
    switch (level) {
        case LogLvl_Info0: return "INFO";
        case LogLvl_Warn1: return "WARNING";
        case LogLvl_Err2: return "ERROR";
        case LogLvl_Sec3: return "SECURITY";
        case LogLvl_Aud4: return "AUDIT";
        default: return "UNKNOWN";
    }
}
