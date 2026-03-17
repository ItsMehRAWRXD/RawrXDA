#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QMutex>

// RunbookExecutor executes ordered runbook steps with simulated results.
class RunbookExecutor : public QObject {
    Q_OBJECT
public:
    struct Step {
        enum Kind { Command, HttpCheck, Note };
        Kind kind{Command};
        QString payload;  // command string, URL, or note text
        int timeoutMs{5000};
    };

    struct Runbook {
        QString id;
        QString title;
        QString description;
        QList<Step> steps;
    };

    explicit RunbookExecutor(QObject* parent = nullptr);
    ~RunbookExecutor();

    bool registerRunbook(const Runbook& rb);
    bool removeRunbook(const QString& id);
    QList<Runbook> list() const;

    bool execute(const QString& id);

signals:
    void stepStarted(const QString& runbookId, int index, const Step& step);
    void stepCompleted(const QString& runbookId, int index, const Step& step, bool success, const QString& details);
    void runbookCompleted(const QString& runbookId, bool success);

private:
    QString simulateStep(const Step& step) const;

    QMap<QString, Runbook> m_runbooks;
    mutable QMutex m_mutex;
};
