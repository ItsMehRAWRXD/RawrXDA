#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QElapsedTimer>
#include <QMap>
#include <QMutex>

// ChaosExperimentEngine schedules and executes controlled resilience experiments
// (latency injection, process stress simulation, fault toggles) with metrics capture.
class ChaosExperimentEngine : public QObject {
    Q_OBJECT
public:
    enum class ExperimentType {
        LatencyInjection,
        CpuStress,
        MemoryPressure,
        DiskPressure,
        NetworkDrop,
        KillProcess,
        RestartService
    };

    enum class ExperimentStatus {
        Scheduled,
        Running,
        Completed,
        Failed,
        Cancelled
    };

    struct Experiment {
        QString id;
        ExperimentType type;
        QString target;
        int intensity{0}; // percentage or level
        int durationSec{0};
        ExperimentStatus status{ExperimentStatus::Scheduled};
        QDateTime scheduledAt;
        QDateTime startedAt;
        QDateTime completedAt;
        QString result;
        double latencyMs{0.0};
    };

    explicit ChaosExperimentEngine(QObject* parent = nullptr);
    ~ChaosExperimentEngine();

    QString schedule(const Experiment& exp);
    bool cancel(const QString& id);
    bool runNext();
    bool run(const QString& id);

    QList<Experiment> listScheduled() const;
    QList<Experiment> listHistory(int limit = 50) const;
    Experiment get(const QString& id) const;

signals:
    void experimentStarted(const Experiment& exp);
    void experimentCompleted(const Experiment& exp);
    void experimentFailed(const Experiment& exp, const QString& reason);

private:
    bool execute(Experiment& exp);
    void recordHistory(const Experiment& exp);
    QString typeToString(ExperimentType t) const;

    QMap<QString, Experiment> m_queue;
    QList<Experiment> m_history;
    mutable QMutex m_mutex;
};
