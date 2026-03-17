#pragma once

#include <QWidget>
#include <QProcess>
#include <memory>
#include "TerminalManager.h"

class QPlainTextEdit;
class QLineEdit;
class QComboBox;
class QPushButton;

class TerminalWidget : public QWidget
{
    Q_OBJECT
public:
    explicit TerminalWidget(QWidget* parent = nullptr);
    ~TerminalWidget() override;
    
    /**
     * Two-phase initialization - call after QApplication is ready
     * Creates all Qt widgets and sets up connections
     */
    void initialize();

    void startShell(::TerminalManager::ShellType type);
    void stopShell();
    bool isRunning() const;
    qint64 pid() const;
    QString getTitle() const;

    /**
     * Execute a command in the terminal programmatically
     * @param command Command to execute
     * @param showInOutput Whether to show the command in the output
     */
    void executeCommand(const QString& command, bool showInOutput = true);

    /**
     * Get the current output text from the terminal
     */
    QString getOutput() const;

    /**
     * Clear the terminal output
     */
    void clearOutput();

    /**
     * Agentic capability: Analyze current terminal output for errors and suggest fixes
     */
    void askAIToFix();

signals:
    void errorDetected(const QString& errorText);
    void fixSuggested(const QString& fixCommand);
    void commandExecuted(const QString& command);
    void outputReceived(const QString& output);

private slots:
    void onUserCommand();
    void onOutputReady(const QByteArray& data);
    void onErrorReady(const QByteArray& data);
    void onStarted();
    void onFinished(int exitCode, QProcess::ExitStatus status);
    void checkForErrors(const QString& text);

public:
    void appendOutput(const QString& text);

private:
    ::TerminalManager* m_manager;
    QPlainTextEdit* m_output;
    QLineEdit* m_input;
    QComboBox* m_shellSelect;
    QPushButton* m_startStopBtn;
    QPushButton* m_fixBtn;
};
