/**
 * @file run_debug_widget.h
 * @brief Full Run/Debug integration widget for RawrXD IDE
 * @author RawrXD Team
 * 
 * This widget provides complete debugging and execution functionality including:
 * - GDB/LLDB debugger integration
 * - Breakpoint management with conditions
 * - Variable inspection and watch expressions
 * - Call stack navigation
 * - Memory and register inspection
 * - Launch configuration management
 */

#pragma once

#include <QWidget>
#include <QDockWidget>
#include <QTreeWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QLineEdit>
#include <QComboBox>
#include <QPushButton>
#include <QToolBar>
#include <QSplitter>
#include <QTabWidget>
#include <QProcess>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QFileSystemWatcher>
#include <QMap>
#include <QSet>
#include <memory>
#include <functional>

/**
 * @brief Breakpoint information structure
 */
struct Breakpoint {
    int id = -1;
    QString file;
    int line = 0;
    QString condition;
    int hitCount = 0;
    int ignoreCount = 0;
    bool enabled = true;
    bool verified = false;
    QString logMessage;  // For logpoints
    
    bool isLogpoint() const { return !logMessage.isEmpty(); }
};

/**
 * @brief Stack frame information
 */
struct StackFrame {
    int level = 0;
    QString function;
    QString file;
    int line = 0;
    QString address;
    QString module;
    QMap<QString, QString> locals;
};

/**
 * @brief Variable information for watch/locals
 */
struct Variable {
    QString name;
    QString value;
    QString type;
    bool hasChildren = false;
    QVector<Variable> children;
    QString evaluateName;  // Expression to re-evaluate
    int variablesReference = 0;
};

/**
 * @brief Thread information
 */
struct ThreadInfo {
    int id = 0;
    QString name;
    QString state;
    StackFrame* currentFrame = nullptr;
};

/**
 * @brief Launch configuration for debugging
 */
struct LaunchConfig {
    QString name;
    QString type;           // "cppdbg", "lldb", "gdb", "python", "node"
    QString request;        // "launch" or "attach"
    QString program;
    QStringList args;
    QString cwd;
    QMap<QString, QString> env;
    bool stopAtEntry = false;
    int processId = 0;      // For attach
    QString miDebuggerPath; // GDB/LLDB path
    QString miDebuggerArgs;
    QStringList setupCommands;
    
    QJsonObject toJson() const;
    static LaunchConfig fromJson(const QJsonObject& obj);
};

/**
 * @enum DebuggerState
 * @brief Current state of the debugger
 */
enum class DebuggerState {
    Stopped,
    Running,
    Paused,
    SteppingOver,
    SteppingInto,
    SteppingOut
};

/**
 * @class RunDebugWidget
 * @brief Full-featured debugging widget with GDB/LLDB integration
 */
class RunDebugWidget : public QWidget {
    Q_OBJECT

public:
    explicit RunDebugWidget(QWidget* parent = nullptr);
    ~RunDebugWidget() override;

    // Debugger control
    void startDebugging(const LaunchConfig& config);
    void stopDebugging();
    void pauseDebugging();
    void continueDebugging();
    void stepOver();
    void stepInto();
    void stepOut();
    void restartDebugging();
    
    // Breakpoint management
    int addBreakpoint(const QString& file, int line, const QString& condition = QString());
    void removeBreakpoint(int id);
    void removeAllBreakpoints();
    void enableBreakpoint(int id, bool enabled);
    void setBreakpointCondition(int id, const QString& condition);
    QVector<Breakpoint> getBreakpoints() const;
    Breakpoint* findBreakpoint(const QString& file, int line);
    
    // Variable/Watch management
    void addWatchExpression(const QString& expr);
    void removeWatchExpression(const QString& expr);
    QString evaluateExpression(const QString& expr);
    QVector<Variable> getLocals();
    
    // Configuration management
    void loadLaunchConfigs();
    void saveLaunchConfigs();
    QVector<LaunchConfig> getLaunchConfigs() const;
    void addLaunchConfig(const LaunchConfig& config);
    void setWorkingDirectory(const QString& dir);
    
    // State queries
    DebuggerState state() const { return m_state; }
    bool isRunning() const { return m_state != DebuggerState::Stopped; }
    bool isPaused() const { return m_state == DebuggerState::Paused; }

signals:
    // State change signals
    void debuggingStarted();
    void debuggingStopped(int exitCode);
    void debuggingPaused(const QString& reason, const QString& file, int line);
    void debuggingContinued();
    void stateChanged(DebuggerState newState);
    
    // Breakpoint signals
    void breakpointAdded(const Breakpoint& bp);
    void breakpointRemoved(int id);
    void breakpointChanged(const Breakpoint& bp);
    void breakpointHit(const Breakpoint& bp);
    
    // Data signals
    void variablesUpdated(const QVector<Variable>& locals);
    void callStackUpdated(const QVector<StackFrame>& frames);
    void threadsUpdated(const QVector<ThreadInfo>& threads);
    void watchUpdated(const QString& expr, const QString& value);
    
    // Output signals
    void outputReceived(const QString& category, const QString& output);
    void errorReceived(const QString& error);
    
    // Navigation signal
    void navigateToSource(const QString& file, int line);

public slots:
    void onConfigurationChanged(int index);
    void onBreakpointDoubleClicked(QTreeWidgetItem* item, int column);
    void onStackFrameClicked(QTreeWidgetItem* item, int column);
    void onVariableExpanded(QTreeWidgetItem* item);
    void onWatchExpressionAdded();
    void onDebugConsoleCommand();
    void refreshAll();

private slots:
    void processDebuggerOutput();
    void processDebuggerError();
    void onDebuggerStarted();
    void onDebuggerFinished(int exitCode, QProcess::ExitStatus status);
    void parseGdbMiOutput(const QString& output);

private:
    void setupUI();
    void setupToolbar();
    void setupBreakpointsPanel();
    void setupCallStackPanel();
    void setupVariablesPanel();
    void setupWatchPanel();
    void setupDebugConsole();
    void setupThreadsPanel();
    void setupRegistersPanel();
    void setupMemoryPanel();
    
    // GDB/MI protocol
    void sendGdbCommand(const QString& cmd);
    void sendGdbMiCommand(const QString& cmd, std::function<void(const QJsonObject&)> callback = nullptr);
    void initializeDebugger();
    void setupBreakpointsInDebugger();
    
    // Output parsing
    void handleAsyncRecord(const QString& asyncClass, const QJsonObject& results);
    void handleResultRecord(const QString& resultClass, const QJsonObject& results);
    void handleStreamRecord(const QString& streamType, const QString& text);
    void parseVariableChildren(Variable& parent, const QJsonArray& children);
    
    // UI updates
    void updateBreakpointsView();
    void updateCallStackView(const QVector<StackFrame>& frames);
    void updateVariablesView(const QVector<Variable>& vars);
    void updateThreadsView(const QVector<ThreadInfo>& threads);
    void updateRegistersView(const QMap<QString, QString>& regs);
    void setDebuggerState(DebuggerState newState);
    void appendOutput(const QString& category, const QString& text);
    
    // Configuration
    QString findDebugger(const QString& type);
    QString escapeGdbString(const QString& str);

private:
    // UI Components
    QToolBar* m_toolbar = nullptr;
    QComboBox* m_configSelector = nullptr;
    QTabWidget* m_tabWidget = nullptr;
    QSplitter* m_mainSplitter = nullptr;
    
    // Panels
    QTreeWidget* m_breakpointsTree = nullptr;
    QTreeWidget* m_callStackTree = nullptr;
    QTreeWidget* m_variablesTree = nullptr;
    QTreeWidget* m_watchTree = nullptr;
    QTreeWidget* m_threadsTree = nullptr;
    QTableWidget* m_registersTable = nullptr;
    QTextEdit* m_memoryView = nullptr;
    QTextEdit* m_debugConsole = nullptr;
    QLineEdit* m_consoleInput = nullptr;
    QLineEdit* m_watchInput = nullptr;
    
    // Action buttons
    QPushButton* m_startBtn = nullptr;
    QPushButton* m_stopBtn = nullptr;
    QPushButton* m_pauseBtn = nullptr;
    QPushButton* m_continueBtn = nullptr;
    QPushButton* m_stepOverBtn = nullptr;
    QPushButton* m_stepIntoBtn = nullptr;
    QPushButton* m_stepOutBtn = nullptr;
    QPushButton* m_restartBtn = nullptr;
    
    // Debugger process
    QProcess* m_debuggerProcess = nullptr;
    QString m_outputBuffer;
    int m_commandToken = 0;
    QMap<int, std::function<void(const QJsonObject&)>> m_pendingCommands;
    
    // Data
    QVector<LaunchConfig> m_launchConfigs;
    QVector<Breakpoint> m_breakpoints;
    QVector<StackFrame> m_callStack;
    QVector<Variable> m_locals;
    QVector<Variable> m_watches;
    QVector<ThreadInfo> m_threads;
    QMap<QString, QString> m_registers;
    
    int m_nextBreakpointId = 1;
    int m_currentThreadId = 0;
    int m_currentFrameLevel = 0;
    
    // State
    DebuggerState m_state = DebuggerState::Stopped;
    LaunchConfig m_activeConfig;
    QString m_workingDirectory;
    QString m_currentSourceFile;
    int m_currentSourceLine = 0;
    
    // Settings
    QSettings* m_settings = nullptr;
};

