#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QDateTime>
#include <QMap>
#include <QMutex>

// IncidentResponseCoordinator tracks incidents, escalations, and resolution runbooks.
class IncidentResponseCoordinator : public QObject {
    Q_OBJECT
public:
    enum class Severity { Sev1, Sev2, Sev3, Sev4 };
    enum class Status { Open, Mitigating, Monitoring, Resolved, Cancelled };

    struct Incident {
        QString id;
        QString title;
        Severity severity{Severity::Sev3};
        Status status{Status::Open};
        QStringList tags;
        QString owner;
        QString summary;
        QString mitigation;
        QDateTime openedAt;
        QDateTime updatedAt;
        QDateTime resolvedAt;
        int escalations{0};
    };

    explicit IncidentResponseCoordinator(QObject* parent = nullptr);
    ~IncidentResponseCoordinator();

    QString openIncident(const QString& title, Severity severity, const QStringList& tags = {});
    bool updateSummary(const QString& id, const QString& summary);
    bool addMitigation(const QString& id, const QString& mitigation);
    bool escalate(const QString& id, const QString& newOwner);
    bool setStatus(const QString& id, Status status);
    Incident get(const QString& id) const;
    QList<Incident> listOpen() const;
    QList<Incident> listRecent(int limit = 50) const;

signals:
    void incidentOpened(const Incident& inc);
    void incidentUpdated(const Incident& inc);
    void incidentResolved(const Incident& inc);
    void incidentEscalated(const Incident& inc);

private:
    QString generateId() const;
    QString statusToString(Status s) const;
    void enforceBoundedHistory();

    QMap<QString, Incident> m_incidents;
    QList<QString> m_order; // newest last
    mutable QMutex m_mutex;
};
