#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QMutex>
#include <QJsonObject>

// ResilienceValidator performs health checks (disk, config, latency budgets) and aggregates readiness.
class ResilienceValidator : public QObject {
    Q_OBJECT
public:
    struct CheckResult {
        QString name;
        bool passed{false};
        QString details;
        double value{0.0};
        double threshold{0.0};
    };

    explicit ResilienceValidator(QObject* parent = nullptr);
    ~ResilienceValidator();

    QList<CheckResult> runAll(const QString& workspaceRoot);
    bool overallPassed() const;

signals:
    void validationCompleted(const QList<CheckResult>& results, bool passed);

private:
    CheckResult diskSpace(const QString& workspaceRoot) const;
    CheckResult configExists(const QString& workspaceRoot) const;
    CheckResult latencyBudget() const;
    CheckResult backupFreshness(const QString& workspaceRoot) const;

    QList<CheckResult> m_last;
    mutable QMutex m_mutex;
};
