#pragma once
#include <QObject>
#include <QString>

class ComplianceLoggerSimple : public QObject {
    Q_OBJECT
public:
    static ComplianceLoggerSimple& instance();
signals:
    void testSignal(const QString& message);
private:
    ComplianceLoggerSimple() : QObject(nullptr) {}
};
