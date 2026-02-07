#ifndef AGENTIC_COMMAND_EXECUTOR_H
#define AGENTIC_COMMAND_EXECUTOR_H

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QProgressBar>

// Command Execution Wiring - async process launcher with progress.
// Output captured → streamed to Copilot chat (use your HandleCopilotStreamUpdate).
// Auto-approve list: npm test, cargo check, pytest – everything else triggers Keep/Undo dialog.
class AgenticCommandExecutor : public QObject
{
    Q_OBJECT

public:
    explicit AgenticCommandExecutor(QObject *parent = nullptr);
    ~AgenticCommandExecutor();

    // Set the auto-approve list of commands
    void setAutoApproveList(const QStringList &commands);

    // Execute a command (with approval if not in auto-approve list)
    void executeCommand(const QString &command, const QStringList &arguments = QStringList(), bool requireApproval = true);

    // Get command output
    QString getOutput() const;

    // Cancel running command
    void cancelCommand();

signals:
    // Emitted when command output is received (for streaming to chat)
    void outputReceived(const QString &output);

    // Emitted when command execution starts
    void executionStarted(const QString &command);

    // Emitted when command execution finishes
    void executionFinished(bool success, int exitCode);

    // Emitted when command requires approval
    void approvalRequired(const QString &command);

private slots:
    void onProcessReadyReadStandardOutput();
    void onProcessReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess *m_process;
    QString m_output;
    QStringList m_autoApproveList;

    // Check if command is in auto-approve list
    bool isAutoApproved(const QString &command);

    // Request approval via dialog
    bool requestApproval(const QString &command);
};

#endif // AGENTIC_COMMAND_EXECUTOR_H