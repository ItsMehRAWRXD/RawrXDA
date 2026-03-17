#pragma once

#include <QObject>
#include <QProcess>
#include <QString>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTimer>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#endif

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
    bool startElevated(ShellType shell);  // NEW: Start with UAC elevation
    void stop();
    qint64 pid() const;
    bool isRunning() const;
    bool isElevated() const;  // NEW: Check if running as admin
    void writeInput(const QByteArray& data);

signals:
    void outputReady(const QByteArray& data);
    void errorReady(const QByteArray& data);
    void started();
    void finished(int exitCode, QProcess::ExitStatus exitStatus);
    void elevationFailed(const QString& error);  // NEW: UAC elevation failed

private slots:
    void onStdoutReady();
    void onStderrReady();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onElevatedConnection();  // NEW: Elevated process connected via pipe
    void onElevatedDataReady();   // NEW: Data from elevated process

private:
    bool checkElevationStatus();  // Check if current process is admin
    bool launchElevatedProcess(ShellType shell);  // Use ShellExecuteEx
    void setupNamedPipe();        // Create IPC channel
    
    QProcess* m_process;
    ShellType m_shellType;
    bool m_isElevated;
    
    // For elevated process IPC
    QLocalServer* m_pipeServer;
    QLocalSocket* m_pipeClient;
    QString m_pipeName;
    QTimer* m_connectionTimeout;
};

#ifdef Q_OS_WIN
// Helper: Check if current process has admin privileges
inline bool IsProcessElevated() {
    BOOL isElevated = FALSE;
    HANDLE token = nullptr;
    
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(TOKEN_ELEVATION);
        
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &size)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(token);
    }
    
    return isElevated != FALSE;
}
#endif
