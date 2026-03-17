#include "ChaosExperimentEngine.h"

#include <QRandomGenerator>
#include <QThread>

ChaosExperimentEngine::ChaosExperimentEngine(QObject* parent) : QObject(parent) {}
ChaosExperimentEngine::~ChaosExperimentEngine() {}

QString ChaosExperimentEngine::typeToString(ExperimentType t) const {
    switch (t) {
    case ExperimentType::LatencyInjection: return "latency";
    case ExperimentType::CpuStress: return "cpu";
    case ExperimentType::MemoryPressure: return "memory";
    case ExperimentType::DiskPressure: return "disk";
    case ExperimentType::NetworkDrop: return "network";
    case ExperimentType::KillProcess: return "kill";
    case ExperimentType::RestartService: return "restart";
    }
    return "unknown";
}

QString ChaosExperimentEngine::schedule(const Experiment& exp) {
    QMutexLocker locker(&m_mutex);
    Experiment copy = exp;
    copy.id = copy.id.isEmpty() ? QString::number(QDateTime::currentMSecsSinceEpoch()) : copy.id;
    copy.scheduledAt = QDateTime::currentDateTimeUtc();
    copy.status = ExperimentStatus::Scheduled;
    m_queue[copy.id] = copy;
    return copy.id;
}

bool ChaosExperimentEngine::cancel(const QString& id) {
    QMutexLocker locker(&m_mutex);
    if (!m_queue.contains(id)) return false;
    Experiment exp = m_queue.take(id);
    exp.status = ExperimentStatus::Cancelled;
    recordHistory(exp);
    return true;
}

ChaosExperimentEngine::Experiment ChaosExperimentEngine::get(const QString& id) const {
    QMutexLocker locker(&m_mutex);
    if (m_queue.contains(id)) return m_queue.value(id);
    for (const auto& e : m_history) if (e.id == id) return e;
    return {};
}

QList<ChaosExperimentEngine::Experiment> ChaosExperimentEngine::listScheduled() const {
    QMutexLocker locker(&m_mutex);
    return m_queue.values();
}

QList<ChaosExperimentEngine::Experiment> ChaosExperimentEngine::listHistory(int limit) const {
    QMutexLocker locker(&m_mutex);
    if (limit > 0 && m_history.size() > limit) return m_history.mid(m_history.size() - limit);
    return m_history;
}

bool ChaosExperimentEngine::runNext() {
    QMutexLocker locker(&m_mutex);
    if (m_queue.isEmpty()) return false;
    QString id = m_queue.firstKey();
    Experiment exp = m_queue.take(id);
    locker.unlock();
    return run(id);
}

bool ChaosExperimentEngine::run(const QString& id) {
    Experiment exp;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_queue.contains(id)) return false;
        exp = m_queue.take(id);
    }

    exp.startedAt = QDateTime::currentDateTimeUtc();
    exp.status = ExperimentStatus::Running;
    emit experimentStarted(exp);

    bool ok = execute(exp);

    exp.completedAt = QDateTime::currentDateTimeUtc();
    if (ok) {
        exp.status = ExperimentStatus::Completed;
        emit experimentCompleted(exp);
    } else {
        exp.status = ExperimentStatus::Failed;
        emit experimentFailed(exp, exp.result);
    }

    recordHistory(exp);
    return ok;
}

bool ChaosExperimentEngine::execute(Experiment& exp) {
    QElapsedTimer timer;
    timer.start();
    const int durationMs = qMax(100, exp.durationSec * 1000);

    switch (exp.type) {
    case ExperimentType::LatencyInjection: {
        int jitter = QRandomGenerator::global()->bounded(10, 100);
        QThread::msleep(durationMs + jitter + exp.intensity);
        exp.result = QString("Injected %1ms latency with jitter %2ms").arg(durationMs).arg(jitter);
        break;
    }
    case ExperimentType::CpuStress: {
        // busy-loop for duration with intensity scaling
        qint64 target = durationMs * 1000;
        qint64 acc = 0;
        while (acc < target) {
            for (int i = 0; i < 1000 * exp.intensity; ++i) {
                acc += i % 7;
            }
        }
        exp.result = QString("CPU stress level %1 for %2ms").arg(exp.intensity).arg(durationMs);
        break;
    }
    case ExperimentType::MemoryPressure: {
        QByteArray blob;
        blob.resize(qMin(50, exp.intensity) * 1024 * 10); // up to ~500KB
        for (int i = 0; i < blob.size(); ++i) blob[i] = static_cast<char>(i % 256);
        QThread::msleep(durationMs / 2);
        exp.result = QString("Memory pressure %1KB").arg(blob.size() / 1024);
        break;
    }
    case ExperimentType::DiskPressure: {
        int ops = qMax(1, exp.intensity);
        for (int i = 0; i < ops; ++i) {
            volatile int x = i * i;
            Q_UNUSED(x);
        }
        QThread::msleep(durationMs / 3);
        exp.result = QString("Disk pressure ops=%1 simulated").arg(ops);
        break;
    }
    case ExperimentType::NetworkDrop: {
        QThread::msleep(durationMs / 2 + exp.intensity);
        exp.result = "Simulated packet loss window";
        break;
    }
    case ExperimentType::KillProcess: {
        exp.result = QString("Would kill process %1 (simulation)").arg(exp.target);
        QThread::msleep(50);
        break;
    }
    case ExperimentType::RestartService: {
        QThread::msleep(200 + exp.intensity);
        exp.result = QString("Restarted service %1 (simulated)").arg(exp.target);
        break;
    }
    default:
        exp.result = "Unknown experiment";
        return false;
    }

    exp.latencyMs = timer.elapsed();
    return true;
}

void ChaosExperimentEngine::recordHistory(const Experiment& exp) {
    QMutexLocker locker(&m_mutex);
    m_history.append(exp);
    if (m_history.size() > 500) m_history.removeFirst();
}
