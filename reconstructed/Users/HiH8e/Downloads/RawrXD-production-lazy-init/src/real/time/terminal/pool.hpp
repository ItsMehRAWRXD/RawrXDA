#ifndef REAL_TIME_TERMINAL_POOL_HPP
#define REAL_TIME_TERMINAL_POOL_HPP

#include <QObject>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QJsonObject>
#include <QMutex>
#include <memory>
#include <unordered_map>

class TerminalSession : public QObject {
    Q_OBJECT

public:
    enum ShellType {
        PowerShell = 0,
        CommandPrompt = 1,
        Bash = 2,
        WSL = 3
    };

    enum State {
        Idle = 0,
        Running = 1,
        Suspended = 2,
        Completed = 3,
        Error = 4
    };

    explicit TerminalSession(
        int sessionId,
        ShellType shellType,
        const QString& workingDir = QString(),
        QObject* parent = nullptr);

    ~TerminalSession() override;

    bool start();
    bool isRunning() const;
    void executeCommand(const QString& command);
    void stop();
    void kill();

    QString getOutput() const;
    QString getError() const;
    int getExitCode() const;
    State getState() const;

    QJsonObject getStatistics() const;

signals:
    void outputReceived(const QString& output);
    void errorReceived(const QString& error);
    void started();
    void finished(int exitCode);
    void stateChanged(int newState);

private slots:
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);
    void onProcessError(QProcess::ProcessError error);

private:
    int m_sessionId;
    ShellType m_shellType;
    QString m_workingDir;
    State m_state;
    
    std::unique_ptr<QProcess> m_process;
    mutable QMutex m_mutex;
    
    QString m_output;
    QString m_error;
    int m_exitCode;
    QStringList m_outputHistory;
    int m_maxHistoryLines;
};

class RealTimeTerminalPool : public QObject {
    Q_OBJECT

public:
    explicit RealTimeTerminalPool(int poolSize = 4, QObject* parent = nullptr);
    ~RealTimeTerminalPool() override;

    int createTerminal(TerminalSession::ShellType shellType, const QString& workingDir = QString());
    bool removeTerminal(int terminalId);
    
    int getActiveTerminal() const;
    void setActiveTerminal(int terminalId);
    
    bool executeCommand(int terminalId, const QString& command);
    bool executeCommandSync(int terminalId, const QString& command, QString& output);
    
    QString getTerminalOutput(int terminalId) const;
    QString getTerminalError(int terminalId) const;
    
    int getTerminalCount() const;
    QJsonObject getPoolStatistics() const;
    
    void closeAllTerminals();

signals:
    void terminalOutput(int terminalId, const QString& output);
    void terminalError(int terminalId, const QString& error);
    void terminalCreated(int terminalId);
    void terminalClosed(int terminalId);
    void commandStarted(int terminalId, const QString& command);
    void commandCompleted(int terminalId, int exitCode);

private:
    std::unordered_map<int, std::unique_ptr<TerminalSession>> m_terminals;
    mutable QMutex m_terminalsMutex;
    int m_nextTerminalId;
    int m_activeTerminalId;
    int m_poolSize;
};

#endif // REAL_TIME_TERMINAL_POOL_HPP
