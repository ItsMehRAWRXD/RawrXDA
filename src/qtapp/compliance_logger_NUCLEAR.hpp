#pragma once

#include <QObject>
#include <QString>

class ComplianceLogger : public QObject {
    Q_OBJECT

public:
    static ComplianceLogger& instance();
    ~ComplianceLogger();

signals:
    void testSignal(const QString& message);

private:
    ComplianceLogger();
};
