#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QMutex>

// MacroAutomationRunner executes scripted macro steps (run command, insert text, open file).
class MacroAutomationRunner : public QObject {
    Q_OBJECT
public:
    struct Step {
        enum Type { InsertText, OpenFile, RunCommand };
        Type type{InsertText};
        QString arg1;
        QString arg2;
    };

    struct Macro {
        QString id;
        QString name;
        QString description;
        QList<Step> steps;
    };

    explicit MacroAutomationRunner(QObject* parent = nullptr);
    ~MacroAutomationRunner();

    bool registerMacro(const Macro& macro);
    bool removeMacro(const QString& id);
    QList<Macro> listMacros() const;

    bool execute(const QString& id);

signals:
    void insertTextRequested(const QString& text);
    void openFileRequested(const QString& path);
    void runCommandRequested(const QString& command, const QStringList& args);
    void macroCompleted(const QString& id);

private:
    QMap<QString, Macro> m_macros;
    mutable QMutex m_mutex;
};
