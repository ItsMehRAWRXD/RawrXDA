#pragma once

#include <QObject>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QMutex>

class PostmortemGenerator : public QObject {
    Q_OBJECT
public:
    struct Event {
        QString description;
        QDateTime timestamp;
    };

    struct ActionItem {
        QString owner;
        QString detail;
        QDateTime due;
    };

    struct Report {
        QString incidentId;
        QString summary;
        QString rootCause;
        QList<Event> timeline;
        QList<ActionItem> actions;
        QString lessons;
        QDateTime created;
    };

    explicit PostmortemGenerator(QObject* parent = nullptr);

    void startIncident(const QString& incidentId, const QString& summary);
    void addEvent(const QString& incidentId, const QString& description, const QDateTime& timestamp = QDateTime::currentDateTimeUtc());
    void setRootCause(const QString& incidentId, const QString& rootCause);
    void addActionItem(const QString& incidentId, const QString& owner, const QString& detail, const QDateTime& due);
    void finalize(const QString& incidentId, const QString& lessonsLearned);
    Report report(const QString& incidentId) const;

signals:
    void postmortemReady(const PostmortemGenerator::Report& report);

private:
    mutable QMutex m_mutex;
    QMap<QString, Report> m_reports;
};
