#pragma once
#include <QObject>
#include <QString>
#include <QDateTime>

class ComplianceLoggerTest : public QObject {
    Q_OBJECT
public:
    struct LogEntry {
        QDateTime timestamp;
        QString userId;
    };
    static ComplianceLoggerTest& instance();
signals:
    void eventLogged(const LogEntry& entry);
private:
    ComplianceLoggerTest() : QObject(nullptr) {}
};
