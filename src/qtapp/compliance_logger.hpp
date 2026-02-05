#pragma once

#include <QObject>
#include <QString>
#include <QFile>
#include <QTextStream>
#include <QDateTime>
#include <QMutex>

// Enums defined outside class to avoid Windows header conflicts  
enum ComplianceLogLevel { LogLvl_Info0, LogLvl_Warn1, LogLvl_Err2, LogLvl_Sec3, LogLvl_Aud4 };
enum ComplianceEventType { EvType_Model0, EvType_Data1, EvType_User2, EvType_Cfg3, EvType_Sys4, EvType_SecViol5 };

class ComplianceLogger : public QObject {
    Q_OBJECT

public:
    using LogLevel = ComplianceLogLevel;
    using EventType = ComplianceEventType;

    struct LogEntry {
        QDateTime timestamp;
        LogLevel level;
        EventType eventType;
        QString userId;
        QString action;
        QString resourceId;
        QString ipAddress;
        QString details;
        QString checksum;
    };

    static ComplianceLogger& instance();
    ~ComplianceLogger();

    void start(const QString& logFilePath = QString());
    void stop();
    void logEvent(LogLevel level, EventType eventType, const QString& userId, const QString& action,
                  const QString& resourceId = QString(), const QString& details = QString());
    void logModelAccess(const QString& userId, const QString& modelPath, const QString& action);
    void logDataAccess(const QString& userId, const QString& dataPath, const QString& action);
    void logConfigChange(const QString& userId, const QString& setting, const QString& oldValue, const QString& newValue);
    void logSecurityViolation(const QString& userId, const QString& violation);
    void logUserLogin(const QString& userId, bool success, const QString& ipAddress);
    void logSystemError(const QString& component, const QString& errorMessage);
    QString exportAuditLog(const QDateTime& startDate, const QDateTime& endDate) const;
    void rotateLogs();
    void setRetentionPeriod(int days);

signals:
    void eventLogged(const LogEntry& entry);
    void securityAlert(const QString& message);
    void complianceViolation(const QString& violation);

private:
    ComplianceLogger();
    ComplianceLogger(const ComplianceLogger&) = delete;
    ComplianceLogger& operator=(const ComplianceLogger&) = delete;

    void writeLogEntry(const LogEntry& entry);
    QString calculateEntryChecksum(const LogEntry& entry) const;
    QString formatLogEntry(const LogEntry& entry) const;
    QString eventTypeToString(EventType type) const;
    QString logLevelToString(LogLevel level) const;

    mutable QMutex m_mutex;
    QFile* m_logFile = nullptr;
    QTextStream* m_logStream = nullptr;
    QString m_logFilePath;
    int m_retentionDays = 365;
    bool m_running = false;
};
