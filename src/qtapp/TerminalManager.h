#pragma once


class TerminalManager : public void
{

public:
    enum ShellType {
        PowerShell,
        CommandPrompt
    };

    explicit TerminalManager(void* parent = nullptr);
    ~TerminalManager() override;

    bool start(ShellType shell);
    void stop();
    qint64 pid() const;
    bool isRunning() const;
    void writeInput(const std::vector<uint8_t>& data);


    void outputReady(const std::vector<uint8_t>& data);
    void errorReady(const std::vector<uint8_t>& data);
    void started();
    void finished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    void onStdoutReady();
    void onStderrReady();
    void onProcessStarted();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess* m_process;
    ShellType m_shellType;
};

