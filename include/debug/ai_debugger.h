#ifndef AI_DEBUGGER_H
#define AI_DEBUGGER_H

#include <QObject>
#include <QProcess>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMap>
#include <QTemporaryFile>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QDebug>

// Breakpoint hit → collect locals, stack, registers → prompt → model → diff → Keep/Undo dialog → apply patch → continue.
class AIDebugger : public QObject
{
    Q_OBJECT

public:
    explicit AIDebugger(QObject *parent = nullptr);
    ~AIDebugger();

    // Start debugging session
    bool startDebugging(const QString &executablePath, const QStringList &arguments = QStringList());

    // Set a breakpoint
    void setBreakpoint(const QString &filePath, int lineNumber);

    // Continue execution
    void continueExecution();

    // Stop debugging session
    void stopDebugging();

signals:
    // Emitted when a breakpoint is hit
    void breakpointHit(const QString &filePath, int lineNumber, const QJsonObject &debugInfo);

    // Emitted when a suggested fix is ready
    void fixSuggested(const QString &diff);

    // Emitted when debugging session ends
    void debuggingFinished();

private slots:
    void onGdbReadyRead();
    void onGdbFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_gdbProcess;
    QString m_executablePath;
    bool m_isRunning;
    QMap<QString, int> m_breakpoints; // filePath -> lineNumber

    // Parse GDB output
    void parseGdbOutput(const QString &output);

    // Send command to GDB
    void sendGdbCommand(const QString &command);

    // Collect debug information
    QJsonObject collectDebugInfo();

    // Request fix from model
    void requestFixFromModel(const QJsonObject &debugInfo);
};

#endif // AI_DEBUGGER_H