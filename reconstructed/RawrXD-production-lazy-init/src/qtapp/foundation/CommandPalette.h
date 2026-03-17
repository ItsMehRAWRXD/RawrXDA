#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QDateTime>
#include <QMutex>

// CommandPalette supports registration, fuzzy search, and execution hooks.
class CommandPalette : public QObject {
    Q_OBJECT
public:
    struct Command {
        QString id;
        QString title;
        QString description;
        QString category;
        int usageCount{0};
        QDateTime lastUsed;
    };

    explicit CommandPalette(QObject* parent = nullptr);
    ~CommandPalette();

    bool registerCommand(const Command& cmd);
    bool unregisterCommand(const QString& id);
    QList<Command> listCommands(const QString& category = QString()) const;

    QList<Command> search(const QString& query, int maxResults = 10) const;
    bool execute(const QString& id);

signals:
    void commandExecuted(const QString& id);
    void commandRegistered(const QString& id);
    void commandRemoved(const QString& id);

private:
    double score(const QString& query, const Command& cmd) const;

    QMap<QString, Command> m_commands;
    mutable QMutex m_mutex;
};
