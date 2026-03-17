#include "IncidentResponseCoordinator.h"

#include <QLoggingCategory>
#include <QMutexLocker>
#include <QRandomGenerator>

namespace {
constexpr int kMaxIncidents = 500;
}

Q_LOGGING_CATEGORY(opsIncidentLog, "ops.incident")

IncidentResponseCoordinator::IncidentResponseCoordinator(QObject* parent) : QObject(parent) {}
IncidentResponseCoordinator::~IncidentResponseCoordinator() {}

QString IncidentResponseCoordinator::generateId() const {
    return QString::number(QDateTime::currentMSecsSinceEpoch()) + "-" + QString::number(QRandomGenerator::global()->bounded(1000, 9999));
}

QString IncidentResponseCoordinator::statusToString(Status s) const {
    switch (s) {
    case Status::Open: return "open";
    case Status::Mitigating: return "mitigating";
    case Status::Monitoring: return "monitoring";
    case Status::Resolved: return "resolved";
    case Status::Cancelled: return "cancelled";
    }
    return "unknown";
}

QString IncidentResponseCoordinator::openIncident(const QString& title, Severity severity, const QStringList& tags) {
    Incident inc;
    inc.id = generateId();
    inc.title = title;
    inc.severity = severity;
    inc.tags = tags;
    inc.openedAt = QDateTime::currentDateTimeUtc();
    inc.updatedAt = inc.openedAt;

    {
        QMutexLocker locker(&m_mutex);
        m_incidents[inc.id] = inc;
        m_order.append(inc.id);
        enforceBoundedHistory();
    }
    qInfo(opsIncidentLog) << "incident opened" << inc.id << "severity" << static_cast<int>(severity) << "title" << title;
    emit incidentOpened(inc);
    return inc.id;
}

bool IncidentResponseCoordinator::updateSummary(const QString& id, const QString& summary) {
    QMutexLocker locker(&m_mutex);
    if (!m_incidents.contains(id)) return false;
    Incident& inc = m_incidents[id];
    inc.summary = summary;
    inc.updatedAt = QDateTime::currentDateTimeUtc();
    qInfo(opsIncidentLog) << "incident summary updated" << inc.id;
    emit incidentUpdated(inc);
    return true;
}

bool IncidentResponseCoordinator::addMitigation(const QString& id, const QString& mitigation) {
    QMutexLocker locker(&m_mutex);
    if (!m_incidents.contains(id)) return false;
    Incident& inc = m_incidents[id];
    inc.mitigation = mitigation;
    inc.updatedAt = QDateTime::currentDateTimeUtc();
    qInfo(opsIncidentLog) << "incident mitigation added" << inc.id;
    emit incidentUpdated(inc);
    return true;
}

bool IncidentResponseCoordinator::escalate(const QString& id, const QString& newOwner) {
    QMutexLocker locker(&m_mutex);
    if (!m_incidents.contains(id)) return false;
    Incident& inc = m_incidents[id];
    inc.owner = newOwner;
    inc.escalations += 1;
    inc.updatedAt = QDateTime::currentDateTimeUtc();
    qInfo(opsIncidentLog) << "incident escalated" << inc.id << "owner" << newOwner << "count" << inc.escalations;
    emit incidentEscalated(inc);
    return true;
}

bool IncidentResponseCoordinator::setStatus(const QString& id, Status status) {
    QMutexLocker locker(&m_mutex);
    if (!m_incidents.contains(id)) return false;
    Incident& inc = m_incidents[id];
    inc.status = status;
    inc.updatedAt = QDateTime::currentDateTimeUtc();
    if (status == Status::Resolved || status == Status::Cancelled) {
        inc.resolvedAt = inc.updatedAt;
        qInfo(opsIncidentLog) << "incident closed" << inc.id << "status" << statusToString(status);
        emit incidentResolved(inc);
    } else {
        qInfo(opsIncidentLog) << "incident status updated" << inc.id << "status" << statusToString(status);
        emit incidentUpdated(inc);
    }
    return true;
}

IncidentResponseCoordinator::Incident IncidentResponseCoordinator::get(const QString& id) const {
    QMutexLocker locker(&m_mutex);
    return m_incidents.value(id);
}

QList<IncidentResponseCoordinator::Incident> IncidentResponseCoordinator::listOpen() const {
    QMutexLocker locker(&m_mutex);
    QList<Incident> res;
    for (const auto& id : m_order) {
        const Incident& inc = m_incidents[id];
        if (inc.status == Status::Open || inc.status == Status::Mitigating || inc.status == Status::Monitoring) {
            res.append(inc);
        }
    }
    return res;
}

void IncidentResponseCoordinator::enforceBoundedHistory() {
    if (m_order.size() <= kMaxIncidents) {
        return;
    }
    const int toTrim = m_order.size() - kMaxIncidents;
    for (int i = 0; i < toTrim; ++i) {
        const QString evict = m_order.takeFirst();
        m_incidents.remove(evict);
    }
}

QList<IncidentResponseCoordinator::Incident> IncidentResponseCoordinator::listRecent(int limit) const {
    QMutexLocker locker(&m_mutex);
    QList<Incident> res;
    int start = qMax(0, m_order.size() - limit);
    for (int i = start; i < m_order.size(); ++i) {
        res.append(m_incidents.value(m_order[i]));
    }
    return res;
}
