#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>

class ComplianceLoggerTest2 : public QObject {
    Q_OBJECT
public:
    enum class LogLevel { INFO, WARNING, ERROR };
    Q_ENUM(LogLevel)
    
    struct LogEntry {
        QDateTime timestamp;
        QString userId;
        LogLevel level;
    };
    
    static ComplianceLoggerTest2& instance();
signals:
    void eventLogged(const LogEntry& entry);
private:
    ComplianceLoggerTest2() : QObject(nullptr) {}
};
