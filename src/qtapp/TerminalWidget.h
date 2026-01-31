#pragma once


#include <memory>
#include "TerminalManager.h"


class TerminalWidget : public void
{

public:
    explicit TerminalWidget(void* parent = nullptr);
    ~TerminalWidget() override;
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Creates all Qt widgets and sets up connections
     */
    void initialize();

    void startShell(TerminalManager::ShellType type);
    void stopShell();
    bool isRunning() const;
    qint64 pid() const;

private:
    void onUserCommand();
    void onOutputReady(const std::vector<uint8_t>& data);
    void onErrorReady(const std::vector<uint8_t>& data);
    void onStarted();
    void onFinished(int exitCode, QProcess::ExitStatus status);

private:
    TerminalManager* m_manager;
    QPlainTextEdit* m_output;
    QLineEdit* m_input;
    QComboBox* m_shellSelect;
    QPushButton* m_startStopBtn;
    void appendOutput(const std::string& text);
};

