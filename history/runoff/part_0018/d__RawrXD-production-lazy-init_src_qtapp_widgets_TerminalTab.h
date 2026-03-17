// TerminalTab.h - Individual Terminal Tab
// Part of RawrXD Agentic IDE - Phase 6 (Terminal Integration)
// Manages single terminal session with shell process

#ifndef TERMINALTAB_H
#define TERMINALTAB_H

#include <QWidget>
#include <QProcess>
#include <QTextEdit>
#include <QString>
#include <QStringList>
#include <QMap>
#include <QTimer>
#include "ANSIColorParser.h"

class QLineEdit;
class QPushButton;
class QLabel;
class QComboBox;
class QMenu;

// ============================================================================
// Terminal Configuration Structure
// ============================================================================

struct TerminalConfig {
    QString shell;              // Shell executable (cmd.exe, powershell.exe, bash, etc.)
    QString workingDirectory;   // Initial working directory
    QStringList initialArgs;    // Initial shell arguments
    bool inheritParentEnv = true;
    QMap<QString, QString> environment;
    int bufferSize = 10000;     // Max lines to keep in history
    int updateInterval = 50;    // Output update interval (ms)
    QString name;               // Display name for tab
};

// ============================================================================
// Terminal Tab Class
// ============================================================================

class TerminalTab : public QWidget
{
    Q_OBJECT

public:
    explicit TerminalTab(const TerminalConfig& config, QWidget* parent = nullptr);
    ~TerminalTab();

    // Terminal Control
    void start();
    void stop();
    void kill();
    bool isRunning() const;
    
    // Input/Output
    void sendCommand(const QString& command);
    void sendInput(const QString& input);
    void sendKey(Qt::Key key);
    QString getOutput() const;
    QString getSelectedText() const;
    
    // Configuration
    void setWorkingDirectory(const QString& dir);
    QString getWorkingDirectory() const { return m_workingDirectory; }
    void setEnvironmentVariable(const QString& name, const QString& value);
    QString getShell() const { return m_config.shell; }
    
    // History
    void clearHistory();
    void clearOutput();
    QStringList getCommandHistory() const { return m_commandHistory; }
    void setCommandHistory(const QStringList& history) { m_commandHistory = history; }
    
    // Display
    void setFontSize(int size);
    int getFontSize() const;
    void setFontFamily(const QString& family);
    QString getFontFamily() const;
    void clear();
    
    // State
    QString getCurrentCommand() const { return m_currentCommand; }
    int getLineCount() const;
    QString getLastLine() const;
    
    // Copy/Paste
    void copy();
    void paste();
    void selectAll();
    
signals:
    void outputReceived(const QString& output);
    void processStarted();
    void processFinished(int exitCode);
    void processError(const QString& error);
    void commandExecuted(const QString& command);
    void workingDirectoryChanged(const QString& dir);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private slots:
    // Process events
    void onProcessReadyRead();
    void onProcessError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);
    void onProcessStarted();
    
    // UI events
    void onInputLineEnter();
    void onClearClicked();
    void onCopyClicked();
    void onPasteClicked();
    void onHistoryUp();
    void onHistoryDown();
    
    // Timer
    void onOutputTimeout();
    
    // Context menu
    void onContextMenuRequested(const QPoint& pos);

private:
    // Helper methods
    void setupUI();
    void connectSignals();
    void appendOutput(const QString& text, bool parsed = true);
    void parseAndAppendOutput(const QString& text);
    void updatePrompt();
    void executeCommand(const QString& cmd);
    QString getPrompt() const;
    QString extractWorkingDirectory(const QString& output);
    void updateButtonStates();
    void scrollToBottom();
    void handleCommandHistory(bool up);
    
    // UI Components
    QTextEdit* m_outputDisplay;
    QLineEdit* m_inputLine;
    QPushButton* m_clearBtn;
    QPushButton* m_copyBtn;
    QPushButton* m_pasteBtn;
    QPushButton* m_stopBtn;
    QLabel* m_shellLabel;
    QLabel* m_dirLabel;
    
    // Process management
    QProcess* m_shellProcess;
    TerminalConfig m_config;
    QString m_workingDirectory;
    QString m_currentCommand;
    QString m_outputBuffer;
    QTimer* m_outputTimer;
    int m_historyIndex;
    
    // Command history
    QStringList m_commandHistory;
    static const int MAX_HISTORY = 1000;
    
    // Output buffer
    QStringList m_outputLines;
    static const int OUTPUT_BUFFER_SIZE = 10000;
    
    // ANSI Parser
    ANSIColorParser m_ansiParser;
    
    // State tracking
    bool m_isRunning;
    bool m_isWaitingForPrompt;
    int m_commandStartLine;
    
    // Font settings
    QString m_fontFamily;
    int m_fontSize;
};

#endif // TERMINALTAB_H
