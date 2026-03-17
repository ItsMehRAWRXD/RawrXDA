#include "SLOTracker.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QMutexLocker>

Q_LOGGING_CATEGORY(opsSloLog, "ops.slo")

SLOTracker::SLOTracker(QObject* parent)
    : QObject(parent) {
}

void SLOTracker::defineSLO(const QString& service, double target, int windowMinutes) {
    QMutexLocker locker(&m_mutex);
    m_counts[service] = {0, 0, QDateTime::currentMSecsSinceEpoch(), windowMinutes};
    m_slos[service] = {target, 100.0, windowMinutes};
    qInfo(opsSloLog) << "SLO defined" << service << "target" << target << "window_min" << windowMinutes;
}

void SLOTracker::recordSuccess(const QString& service) {
    QMutexLocker locker(&m_mutex);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    prune(service, now);
    auto& wc = m_counts[service];
    wc.successes++;
    m_slos[service].current = availabilityLocked(service);
}

void SLOTracker::recordFailure(const QString& service) {
    QMutexLocker locker(&m_mutex);
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    prune(service, now);
    auto& wc = m_counts[service];
    wc.failures++;

    m_slos[service].current = availabilityLocked(service);
    const double target = m_slos.value(service, SLO{}).target;
    const double avail = m_slos[service].current;
    if (target > 0.0 && avail < target) {
        qWarning(opsSloLog) << "SLO breach" << service << "availability" << avail << "target" << target;
        emit sloBreached(service, avail, target);
    }
}

double SLOTracker::availability(const QString& service) const {
    QMutexLocker locker(&m_mutex);
    return availabilityLocked(service);
}

SLOTracker::SLO SLOTracker::slo(const QString& service) const {
    QMutexLocker locker(&m_mutex);
    SLO result = m_slos.value(service, SLO{});
    result.current = availabilityLocked(service);
    return result;
}

void SLOTracker::prune(const QString& service, qint64 nowMs) {
    auto& wc = m_counts[service];
    if (wc.windowMinutes <= 0) {
        wc.windowMinutes = 1440;
    }
    const qint64 windowMs = static_cast<qint64>(wc.windowMinutes) * 60 * 1000;
    if (wc.windowStart == 0) {
        wc.windowStart = nowMs;
    }
    if (nowMs - wc.windowStart > windowMs) {
        wc.successes = 0;
        wc.failures = 0;
        wc.windowStart = nowMs;
    }
}

double SLOTracker::availabilityLocked(const QString& service) const {
    if (!m_counts.contains(service)) {
        return 100.0;
    }
    const auto& wc = m_counts[service];
    const int total = wc.successes + wc.failures;
    if (total == 0) {
        return 100.0;
    }
    return (static_cast<double>(wc.successes) / total) * 100.0;
}
