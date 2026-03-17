#include "RunbookExecutor.h"

#include <QElapsedTimer>
#include <QLoggingCategory>
#include <QMutexLocker>
#include <QRandomGenerator>
#include <QThread>

Q_LOGGING_CATEGORY(opsRunbookLog, "ops.runbook")

RunbookExecutor::RunbookExecutor(QObject* parent) : QObject(parent) {}
RunbookExecutor::~RunbookExecutor() {}

bool RunbookExecutor::registerRunbook(const Runbook& rb) {
    QMutexLocker locker(&m_mutex);
    if (rb.id.isEmpty() || m_runbooks.contains(rb.id) || rb.steps.isEmpty()) return false;
    m_runbooks[rb.id] = rb;
    qInfo(opsRunbookLog) << "runbook registered" << rb.id << "steps" << rb.steps.size();
    return true;
}

bool RunbookExecutor::removeRunbook(const QString& id) {
    QMutexLocker locker(&m_mutex);
    const bool removed = m_runbooks.remove(id) > 0;
    if (removed) {
        qInfo(opsRunbookLog) << "runbook removed" << id;
    }
    return removed;
}

QList<RunbookExecutor::Runbook> RunbookExecutor::list() const {
    QMutexLocker locker(&m_mutex);
    return m_runbooks.values();
}

QString RunbookExecutor::simulateStep(const Step& step) const {
    switch (step.kind) {
    case Step::Command:
        return QString("Executed command: %1").arg(step.payload);
    case Step::HttpCheck:
        return QString("HTTP 200 OK from %1").arg(step.payload);
    case Step::Note:
        return QString("Note: %1").arg(step.payload);
    }
    return "";
}

bool RunbookExecutor::execute(const QString& id) {
    QList<Step> steps;
    {
        QMutexLocker locker(&m_mutex);
        if (!m_runbooks.contains(id)) return false;
        steps = m_runbooks[id].steps;
    }

    bool allOk = true;
    QElapsedTimer totalTimer;
    totalTimer.start();
    qInfo(opsRunbookLog) << "runbook start" << id << "steps" << steps.size();
    for (int i = 0; i < steps.size(); ++i) {
        const Step& s = steps[i];
        emit stepStarted(id, i, s);
        QElapsedTimer stepTimer;
        stepTimer.start();
        // simulate work respecting timeout
        int work = qMin(s.timeoutMs, 2000) + QRandomGenerator::global()->bounded(0, 200);
        QThread::msleep(work);
        bool ok = QRandomGenerator::global()->bounded(0, 100) > 5; // 95% success rate
        QString details = simulateStep(s);
        allOk = allOk && ok;
        const qint64 stepElapsed = stepTimer.elapsed();
        qInfo(opsRunbookLog) << "step completed" << id << "index" << i << "ok" << ok << "elapsed_ms" << stepElapsed;
        emit stepCompleted(id, i, s, ok, details);
        if (!ok) break;
    }

    qInfo(opsRunbookLog) << "runbook completed" << id << "ok" << allOk << "elapsed_ms" << totalTimer.elapsed();
    emit runbookCompleted(id, allOk);
    return allOk;
}
