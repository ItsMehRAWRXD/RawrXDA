#pragma once

#include <QObject>
#include <QProcess>
#include <QString>

class TerminalManager : public QObject
{
    Q_OBJECT
public:
    enum ShellType {
        PowerShell,
        CommandPrompt
    };

    explicit TerminalManager(QObject* parent = nullptr);
    ~TerminalManager() override;

    bool start(ShellType shell);
    void stop();
    qint64 pid() const;
    bool isRunning() const;
    void writeInput(const QByteArray& data);

signals:
    void outputReady(const QByteArray& data);
    void errorReady(const QByteArray& data);
    void started();
    void finished(int exitCode, QProcess::ExitStatus exitStatus);

private slots:
    void onStdoutReady();
    void onStderrReady();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess* m_process;
    ShellType m_shellType;
};
