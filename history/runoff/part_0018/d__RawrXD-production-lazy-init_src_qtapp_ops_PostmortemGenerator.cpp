#include "PostmortemGenerator.h"

#include <QLoggingCategory>
#include <QMutexLocker>

Q_LOGGING_CATEGORY(opsPostmortemLog, "ops.postmortem")

PostmortemGenerator::PostmortemGenerator(QObject* parent)
    : QObject(parent) {
}

void PostmortemGenerator::startIncident(const QString& incidentId, const QString& summary) {
    QMutexLocker locker(&m_mutex);
    Report r;
    r.incidentId = incidentId;
    r.summary = summary;
    r.created = QDateTime::currentDateTimeUtc();
    m_reports[incidentId] = r;
    qInfo(opsPostmortemLog) << "postmortem started" << incidentId;
}

void PostmortemGenerator::addEvent(const QString& incidentId, const QString& description, const QDateTime& timestamp) {
    QMutexLocker locker(&m_mutex);
    auto it = m_reports.find(incidentId);
    if (it == m_reports.end()) {
        return;
    }
    it->timeline.push_back({description, timestamp});
    qInfo(opsPostmortemLog) << "event added" << incidentId << description;
}

void PostmortemGenerator::setRootCause(const QString& incidentId, const QString& rootCause) {
    QMutexLocker locker(&m_mutex);
    auto it = m_reports.find(incidentId);
    if (it == m_reports.end()) {
        return;
    }
    it->rootCause = rootCause;
    qInfo(opsPostmortemLog) << "root cause set" << incidentId;
}

void PostmortemGenerator::addActionItem(const QString& incidentId, const QString& owner, const QString& detail, const QDateTime& due) {
    QMutexLocker locker(&m_mutex);
    auto it = m_reports.find(incidentId);
    if (it == m_reports.end()) {
        return;
    }
    it->actions.push_back({owner, detail, due});
    qInfo(opsPostmortemLog) << "action item added" << incidentId << "owner" << owner;
}

void PostmortemGenerator::finalize(const QString& incidentId, const QString& lessonsLearned) {
    QMutexLocker locker(&m_mutex);
    auto it = m_reports.find(incidentId);
    if (it == m_reports.end()) {
        return;
    }
    it->lessons = lessonsLearned;
    const Report ready = *it;
    locker.unlock();
    emit postmortemReady(ready);
    qInfo(opsPostmortemLog) << "postmortem finalized" << incidentId;
}

PostmortemGenerator::Report PostmortemGenerator::report(const QString& incidentId) const {
    QMutexLocker locker(&m_mutex);
    return m_reports.value(incidentId);
}
