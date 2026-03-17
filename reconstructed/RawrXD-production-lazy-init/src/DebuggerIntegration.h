#ifndef DEBUGGERINTEGRATION_H
#define DEBUGGERINTEGRATION_H

#include <QObject>
#include <QString>
#include <QVector>
#include <QMap>
#include "qtapp/DebuggerPanel.h"

namespace RawrXD {

class DebuggerIntegration : public QObject {
    Q_OBJECT

public:
    static DebuggerIntegration& instance();

    void initialize(DebuggerPanel* panel);
    
    // Debug control
    void startDebugging(const QString& target);
    void stopDebugging();
    void pause();
    void resume();
    void stepOver();
    void stepInto();
    void stepOut();

    // Breakpoint management
    void toggleBreakpoint(const QString& file, int line);
    void clearAllBreakpoints();

    // Variable management
    void updateVariables();
    void updateCallStack();
    void evaluateExpression(const QString& expression);

signals:
    void targetStarted(const QString& target);
    void targetStopped(const QString& target);
    void pausedAt(const QString& file, int line);
    void exceptionOccurred(const QString& message);

private slots:
    void onContinueRequested();
    void onStepOverRequested();
    void onStepIntoRequested();
    void onStepOutRequested();
    void onStopRequested();

private:
    DebuggerIntegration();
    ~DebuggerIntegration() = default;

    DebuggerPanel* m_panel = nullptr;
    bool m_isRunning = false;
    bool m_isPaused = false;
    QString m_currentTarget;
    
    struct Breakpoint {
        QString file;
        int line;
        bool enabled;
    };
    QVector<Breakpoint> m_breakpoints;
};

} // namespace RawrXD

#endif // DEBUGGERINTEGRATION_H
