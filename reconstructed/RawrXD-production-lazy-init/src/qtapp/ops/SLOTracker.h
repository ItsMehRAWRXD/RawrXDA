#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QMutex>

class SLOTracker : public QObject {
    Q_OBJECT
public:
    struct SLO {
        double target{0.0}; // e.g., 99.9 for availability
        double current{0.0};
        int windowMinutes{1440};
    };

    explicit SLOTracker(QObject* parent = nullptr);

    void defineSLO(const QString& service, double target, int windowMinutes);
    void recordSuccess(const QString& service);
    void recordFailure(const QString& service);
    double availability(const QString& service) const;
    SLO slo(const QString& service) const;

signals:
    void sloBreached(const QString& service, double availability, double target);

private:
    void prune(const QString& service, qint64 nowMs);
    double availabilityLocked(const QString& service) const;

    struct WindowedCount {
        int successes{0};
        int failures{0};
        qint64 windowStart{0};
        int windowMinutes{1440};
    };

    mutable QMap<QString, WindowedCount> m_counts;
    mutable QMap<QString, SLO> m_slos;
    mutable QMutex m_mutex;
};
