/**
 * @file run_debug_widget.cpp
 * @brief Full Run/Debug integration implementation for RawrXD IDE
 * @author RawrXD Team
 */

#include "run_debug_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QRegularExpression>
#include <QApplication>
#include <QClipboard>
#include <QToolButton>
#include <QLabel>
#include <QGroupBox>
#include <QScrollArea>

// =============================================================================
// LaunchConfig JSON Serialization
// =============================================================================

QJsonObject LaunchConfig::toJson() const {
    QJsonObject obj;
    obj["name"] = name;
    obj["type"] = type;
    obj["request"] = request;
    obj["program"] = program;
    obj["cwd"] = cwd;
    obj["stopAtEntry"] = stopAtEntry;
    obj["processId"] = processId;
    obj["miDebuggerPath"] = miDebuggerPath;
    obj["miDebuggerArgs"] = miDebuggerArgs;
    
    QJsonArray argsArray;
    for (const QString& arg : args) {
        argsArray.append(arg);
    }
    obj["args"] = argsArray;
    
    QJsonObject envObj;
    for (auto it = env.begin(); it != env.end(); ++it) {
        envObj[it.key()] = it.value();
    }
    obj["env"] = envObj;
    
    QJsonArray setupArray;
    for (const QString& cmd : setupCommands) {
        setupArray.append(cmd);
    }
    obj["setupCommands"] = setupArray;
    
    return obj;
}

LaunchConfig LaunchConfig::fromJson(const QJsonObject& obj) {
    LaunchConfig config;
    config.name = obj["name"].toString();
    config.type = obj["type"].toString("cppdbg");
    config.request = obj["request"].toString("launch");
    config.program = obj["program"].toString();
    config.cwd = obj["cwd"].toString();
    config.stopAtEntry = obj["stopAtEntry"].toBool(false);
    config.processId = obj["processId"].toInt(0);
    config.miDebuggerPath = obj["miDebuggerPath"].toString();
    config.miDebuggerArgs = obj["miDebuggerArgs"].toString();
    
    QJsonArray argsArray = obj["args"].toArray();
    for (const QJsonValue& val : argsArray) {
        config.args.append(val.toString());
    }
    
    QJsonObject envObj = obj["env"].toObject();
    for (auto it = envObj.begin(); it != envObj.end(); ++it) {
        config.env[it.key()] = it.value().toString();
    }
    
    QJsonArray setupArray = obj["setupCommands"].toArray();
    for (const QJsonValue& val : setupArray) {
        config.setupCommands.append(val.toString());
    }
    
    return config;
}

// =============================================================================
// RunDebugWidget Implementation
// =============================================================================

RunDebugWidget::RunDebugWidget(QWidget* parent)
    : QWidget(parent)
    , m_settings(new QSettings("RawrXD", "IDE", this))
{
    setupUI();
    loadLaunchConfigs();
    
    // Set default debugger paths based on platform
#ifdef Q_OS_WIN
    if (m_launchConfigs.isEmpty()) {
        LaunchConfig defaultConfig;
        defaultConfig.name = "Debug Current Program";
        defaultConfig.type = "cppdbg";
        defaultConfig.request = "launch";
        defaultConfig.miDebuggerPath = "gdb";
        m_launchConfigs.append(defaultConfig);
    }
#else
    if (m_launchConfigs.isEmpty()) {
        LaunchConfig defaultConfig;
        defaultConfig.name = "Debug Current Program";
        defaultConfig.type = "lldb";
        defaultConfig.request = "launch";
        defaultConfig.miDebuggerPath = "lldb-mi";
        m_launchConfigs.append(defaultConfig);
    }
#endif
    
    // Update config selector
    for (const LaunchConfig& config : m_launchConfigs) {
        m_configSelector->addItem(config.name);
    }
}

RunDebugWidget::~RunDebugWidget() {
    if (m_debuggerProcess && m_debuggerProcess->state() != QProcess::NotRunning) {
        stopDebugging();
    }
    saveLaunchConfigs();
}

void RunDebugWidget::setupUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    setupToolbar();
    mainLayout->addWidget(m_toolbar);
    
    // Main splitter for panels
    m_mainSplitter = new QSplitter(Qt::Vertical, this);
    
    // Tab widget for different debug views
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabPosition(QTabWidget::South);
    
    // Setup all panels
    setupVariablesPanel();
    setupWatchPanel();
    setupCallStackPanel();
    setupBreakpointsPanel();
    setupThreadsPanel();
    setupRegistersPanel();
    setupMemoryPanel();
    setupDebugConsole();
    
    // Add tabs
    m_tabWidget->addTab(m_variablesTree, "Variables");
    m_tabWidget->addTab(m_watchTree, "Watch");
    m_tabWidget->addTab(m_callStackTree, "Call Stack");
    m_tabWidget->addTab(m_breakpointsTree, "Breakpoints");
    m_tabWidget->addTab(m_threadsTree, "Threads");
    m_tabWidget->addTab(m_registersTable, "Registers");
    m_tabWidget->addTab(m_memoryView, "Memory");
    
    m_mainSplitter->addWidget(m_tabWidget);
    
    // Debug console at bottom
    QWidget* consoleWidget = new QWidget(this);
    QVBoxLayout* consoleLayout = new QVBoxLayout(consoleWidget);
    consoleLayout->setContentsMargins(2, 2, 2, 2);
    consoleLayout->addWidget(new QLabel("Debug Console:"));
    consoleLayout->addWidget(m_debugConsole);
    
    QHBoxLayout* inputLayout = new QHBoxLayout();
    inputLayout->addWidget(new QLabel(">"));
    inputLayout->addWidget(m_consoleInput);
    consoleLayout->addLayout(inputLayout);
    
    m_mainSplitter->addWidget(consoleWidget);
    m_mainSplitter->setStretchFactor(0, 3);
    m_mainSplitter->setStretchFactor(1, 1);
    
    mainLayout->addWidget(m_mainSplitter);
}

void RunDebugWidget::setupToolbar() {
    m_toolbar = new QToolBar("Debug Toolbar", this);
    m_toolbar->setIconSize(QSize(16, 16));
    
    // Configuration selector
    m_configSelector = new QComboBox(this);
    m_configSelector->setMinimumWidth(150);
    m_toolbar->addWidget(new QLabel(" Config: "));
    m_toolbar->addWidget(m_configSelector);
    m_toolbar->addSeparator();
    
    connect(m_configSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &RunDebugWidget::onConfigurationChanged);
    
    // Debug control buttons
    m_startBtn = new QPushButton("▶ Start", this);
    m_startBtn->setToolTip("Start Debugging (F5)");
    m_startBtn->setShortcut(QKeySequence(Qt::Key_F5));
    connect(m_startBtn, &QPushButton::clicked, this, [this]() {
        if (m_configSelector->currentIndex() >= 0 && m_configSelector->currentIndex() < m_launchConfigs.size()) {
            startDebugging(m_launchConfigs[m_configSelector->currentIndex()]);
        }
    });
    m_toolbar->addWidget(m_startBtn);
    
    m_pauseBtn = new QPushButton("⏸ Pause", this);
    m_pauseBtn->setToolTip("Pause Debugging (F6)");
    m_pauseBtn->setEnabled(false);
    connect(m_pauseBtn, &QPushButton::clicked, this, &RunDebugWidget::pauseDebugging);
    m_toolbar->addWidget(m_pauseBtn);
    
    m_continueBtn = new QPushButton("▶ Continue", this);
    m_continueBtn->setToolTip("Continue (F5)");
    m_continueBtn->setEnabled(false);
    connect(m_continueBtn, &QPushButton::clicked, this, &RunDebugWidget::continueDebugging);
    m_toolbar->addWidget(m_continueBtn);
    
    m_stopBtn = new QPushButton("⏹ Stop", this);
    m_stopBtn->setToolTip("Stop Debugging (Shift+F5)");
    m_stopBtn->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F5));
    m_stopBtn->setEnabled(false);
    connect(m_stopBtn, &QPushButton::clicked, this, &RunDebugWidget::stopDebugging);
    m_toolbar->addWidget(m_stopBtn);
    
    m_toolbar->addSeparator();
    
    // Step buttons
    m_stepOverBtn = new QPushButton("⤵ Step Over", this);
    m_stepOverBtn->setToolTip("Step Over (F10)");
    m_stepOverBtn->setShortcut(QKeySequence(Qt::Key_F10));
    m_stepOverBtn->setEnabled(false);
    connect(m_stepOverBtn, &QPushButton::clicked, this, &RunDebugWidget::stepOver);
    m_toolbar->addWidget(m_stepOverBtn);
    
    m_stepIntoBtn = new QPushButton("↓ Step Into", this);
    m_stepIntoBtn->setToolTip("Step Into (F11)");
    m_stepIntoBtn->setShortcut(QKeySequence(Qt::Key_F11));
    m_stepIntoBtn->setEnabled(false);
    connect(m_stepIntoBtn, &QPushButton::clicked, this, &RunDebugWidget::stepInto);
    m_toolbar->addWidget(m_stepIntoBtn);
    
    m_stepOutBtn = new QPushButton("↑ Step Out", this);
    m_stepOutBtn->setToolTip("Step Out (Shift+F11)");
    m_stepOutBtn->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F11));
    m_stepOutBtn->setEnabled(false);
    connect(m_stepOutBtn, &QPushButton::clicked, this, &RunDebugWidget::stepOut);
    m_toolbar->addWidget(m_stepOutBtn);
    
    m_toolbar->addSeparator();
    
    m_restartBtn = new QPushButton("🔄 Restart", this);
    m_restartBtn->setToolTip("Restart Debugging (Ctrl+Shift+F5)");
    m_restartBtn->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F5));
    m_restartBtn->setEnabled(false);
    connect(m_restartBtn, &QPushButton::clicked, this, &RunDebugWidget::restartDebugging);
    m_toolbar->addWidget(m_restartBtn);
}

void RunDebugWidget::setupBreakpointsPanel() {
    m_breakpointsTree = new QTreeWidget(this);
    m_breakpointsTree->setHeaderLabels({"", "File", "Line", "Condition", "Hit Count"});
    m_breakpointsTree->setColumnWidth(0, 30);  // Enabled checkbox
    m_breakpointsTree->setColumnWidth(1, 200);
    m_breakpointsTree->setColumnWidth(2, 60);
    m_breakpointsTree->setColumnWidth(3, 150);
    m_breakpointsTree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_breakpointsTree->setRootIsDecorated(false);
    
    connect(m_breakpointsTree, &QTreeWidget::itemDoubleClicked,
            this, &RunDebugWidget::onBreakpointDoubleClicked);
    
    connect(m_breakpointsTree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        QMenu menu(this);
        QTreeWidgetItem* item = m_breakpointsTree->itemAt(pos);
        
        if (item) {
            int bpId = item->data(0, Qt::UserRole).toInt();
            
            QAction* removeAction = menu.addAction("Remove Breakpoint");
            connect(removeAction, &QAction::triggered, this, [this, bpId]() {
                removeBreakpoint(bpId);
            });
            
            QAction* conditionAction = menu.addAction("Edit Condition...");
            connect(conditionAction, &QAction::triggered, this, [this, bpId]() {
                bool ok;
                QString condition = QInputDialog::getText(this, "Breakpoint Condition",
                    "Enter condition:", QLineEdit::Normal, QString(), &ok);
                if (ok) {
                    setBreakpointCondition(bpId, condition);
                }
            });
            
            menu.addSeparator();
        }
        
        QAction* removeAllAction = menu.addAction("Remove All Breakpoints");
        connect(removeAllAction, &QAction::triggered, this, &RunDebugWidget::removeAllBreakpoints);
        
        menu.exec(m_breakpointsTree->mapToGlobal(pos));
    });
}

void RunDebugWidget::setupCallStackPanel() {
    m_callStackTree = new QTreeWidget(this);
    m_callStackTree->setHeaderLabels({"#", "Function", "File", "Line", "Address"});
    m_callStackTree->setColumnWidth(0, 30);
    m_callStackTree->setColumnWidth(1, 200);
    m_callStackTree->setColumnWidth(2, 200);
    m_callStackTree->setColumnWidth(3, 60);
    m_callStackTree->setRootIsDecorated(false);
    
    connect(m_callStackTree, &QTreeWidget::itemClicked,
            this, &RunDebugWidget::onStackFrameClicked);
}

void RunDebugWidget::setupVariablesPanel() {
    m_variablesTree = new QTreeWidget(this);
    m_variablesTree->setHeaderLabels({"Name", "Value", "Type"});
    m_variablesTree->setColumnWidth(0, 150);
    m_variablesTree->setColumnWidth(1, 200);
    m_variablesTree->setContextMenuPolicy(Qt::CustomContextMenu);
    
    connect(m_variablesTree, &QTreeWidget::itemExpanded,
            this, &RunDebugWidget::onVariableExpanded);
    
    connect(m_variablesTree, &QTreeWidget::customContextMenuRequested,
            this, [this](const QPoint& pos) {
        QTreeWidgetItem* item = m_variablesTree->itemAt(pos);
        if (!item) return;
        
        QMenu menu(this);
        
        QAction* copyValueAction = menu.addAction("Copy Value");
        connect(copyValueAction, &QAction::triggered, this, [item]() {
            QApplication::clipboard()->setText(item->text(1));
        });
        
        QAction* copyNameAction = menu.addAction("Copy Name");
        connect(copyNameAction, &QAction::triggered, this, [item]() {
            QApplication::clipboard()->setText(item->text(0));
        });
        
        QAction* addWatchAction = menu.addAction("Add to Watch");
        connect(addWatchAction, &QAction::triggered, this, [this, item]() {
            addWatchExpression(item->text(0));
        });
        
        menu.exec(m_variablesTree->mapToGlobal(pos));
    });
}

void RunDebugWidget::setupWatchPanel() {
    m_watchTree = new QTreeWidget(this);
    m_watchTree->setHeaderLabels({"Expression", "Value", "Type"});
    m_watchTree->setColumnWidth(0, 150);
    m_watchTree->setColumnWidth(1, 200);
    
    // Watch input at bottom
    QWidget* watchWidget = new QWidget(this);
    QVBoxLayout* watchLayout = new QVBoxLayout(watchWidget);
    watchLayout->setContentsMargins(0, 0, 0, 0);
    watchLayout->addWidget(m_watchTree);
    
    QHBoxLayout* inputLayout = new QHBoxLayout();
    m_watchInput = new QLineEdit(this);
    m_watchInput->setPlaceholderText("Add watch expression...");
    connect(m_watchInput, &QLineEdit::returnPressed, this, &RunDebugWidget::onWatchExpressionAdded);
    inputLayout->addWidget(m_watchInput);
    
    QPushButton* addWatchBtn = new QPushButton("+", this);
    addWatchBtn->setMaximumWidth(30);
    connect(addWatchBtn, &QPushButton::clicked, this, &RunDebugWidget::onWatchExpressionAdded);
    inputLayout->addWidget(addWatchBtn);
    
    watchLayout->addLayout(inputLayout);
    
    // Replace watchTree with watchWidget in tab
    // Note: The tab already adds m_watchTree, so we need to handle this specially
    connect(m_watchTree, &QTreeWidget::itemExpanded,
            this, &RunDebugWidget::onVariableExpanded);
}

void RunDebugWidget::setupDebugConsole() {
    m_debugConsole = new QTextEdit(this);
    m_debugConsole->setReadOnly(true);
    m_debugConsole->setFont(QFont("Consolas", 10));
    m_debugConsole->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #d4d4d4; }");
    
    m_consoleInput = new QLineEdit(this);
    m_consoleInput->setFont(QFont("Consolas", 10));
    m_consoleInput->setPlaceholderText("Enter GDB/LLDB command...");
    connect(m_consoleInput, &QLineEdit::returnPressed, this, &RunDebugWidget::onDebugConsoleCommand);
}

void RunDebugWidget::setupThreadsPanel() {
    m_threadsTree = new QTreeWidget(this);
    m_threadsTree->setHeaderLabels({"ID", "Name", "State", "Location"});
    m_threadsTree->setColumnWidth(0, 50);
    m_threadsTree->setColumnWidth(1, 150);
    m_threadsTree->setColumnWidth(2, 80);
    m_threadsTree->setRootIsDecorated(false);
    
    connect(m_threadsTree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int) {
        int threadId = item->data(0, Qt::UserRole).toInt();
        if (threadId > 0 && isRunning()) {
            sendGdbMiCommand(QString("-thread-select %1").arg(threadId));
            m_currentThreadId = threadId;
            // Refresh stack after thread switch
            sendGdbMiCommand("-stack-list-frames");
        }
    });
}

void RunDebugWidget::setupRegistersPanel() {
    m_registersTable = new QTableWidget(this);
    m_registersTable->setColumnCount(2);
    m_registersTable->setHorizontalHeaderLabels({"Register", "Value"});
    m_registersTable->horizontalHeader()->setStretchLastSection(true);
    m_registersTable->setAlternatingRowColors(true);
    m_registersTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
}

void RunDebugWidget::setupMemoryPanel() {
    m_memoryView = new QTextEdit(this);
    m_memoryView->setReadOnly(true);
    m_memoryView->setFont(QFont("Consolas", 10));
    m_memoryView->setLineWrapMode(QTextEdit::NoWrap);
}

// =============================================================================
// Debugger Control
// =============================================================================

void RunDebugWidget::startDebugging(const LaunchConfig& config) {
    if (isRunning()) {
        stopDebugging();
    }
    
    m_activeConfig = config;
    
    // Create debugger process
    m_debuggerProcess = new QProcess(this);
    
    connect(m_debuggerProcess, &QProcess::readyReadStandardOutput,
            this, &RunDebugWidget::processDebuggerOutput);
    connect(m_debuggerProcess, &QProcess::readyReadStandardError,
            this, &RunDebugWidget::processDebuggerError);
    connect(m_debuggerProcess, &QProcess::started,
            this, &RunDebugWidget::onDebuggerStarted);
    connect(m_debuggerProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &RunDebugWidget::onDebuggerFinished);
    
    // Set working directory
    QString cwd = config.cwd.isEmpty() ? m_workingDirectory : config.cwd;
    if (!cwd.isEmpty()) {
        m_debuggerProcess->setWorkingDirectory(cwd);
    }
    
    // Set environment
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    for (auto it = config.env.begin(); it != config.env.end(); ++it) {
        env.insert(it.key(), it.value());
    }
    m_debuggerProcess->setProcessEnvironment(env);
    
    // Find debugger
    QString debuggerPath = config.miDebuggerPath;
    if (debuggerPath.isEmpty()) {
        debuggerPath = findDebugger(config.type);
    }
    
    QStringList args;
    args << "--interpreter=mi2";  // Use MI2 protocol
    if (!config.miDebuggerArgs.isEmpty()) {
        args << config.miDebuggerArgs.split(' ', Qt::SkipEmptyParts);
    }
    
    appendOutput("system", QString("Starting debugger: %1 %2\n").arg(debuggerPath, args.join(" ")));
    
    m_debuggerProcess->start(debuggerPath, args);
    
    if (!m_debuggerProcess->waitForStarted(5000)) {
        appendOutput("error", QString("Failed to start debugger: %1\n").arg(m_debuggerProcess->errorString()));
        emit errorReceived("Failed to start debugger");
        return;
    }
}

void RunDebugWidget::stopDebugging() {
    if (!m_debuggerProcess) return;
    
    if (m_state == DebuggerState::Running || m_state == DebuggerState::Paused) {
        sendGdbCommand("-gdb-exit");
        
        if (!m_debuggerProcess->waitForFinished(3000)) {
            m_debuggerProcess->kill();
            m_debuggerProcess->waitForFinished(1000);
        }
    }
    
    setDebuggerState(DebuggerState::Stopped);
}

void RunDebugWidget::pauseDebugging() {
    if (m_state != DebuggerState::Running) return;
    
    sendGdbMiCommand("-exec-interrupt");
}

void RunDebugWidget::continueDebugging() {
    if (m_state != DebuggerState::Paused) return;
    
    sendGdbMiCommand("-exec-continue");
    setDebuggerState(DebuggerState::Running);
}

void RunDebugWidget::stepOver() {
    if (m_state != DebuggerState::Paused) return;
    
    sendGdbMiCommand("-exec-next");
    setDebuggerState(DebuggerState::SteppingOver);
}

void RunDebugWidget::stepInto() {
    if (m_state != DebuggerState::Paused) return;
    
    sendGdbMiCommand("-exec-step");
    setDebuggerState(DebuggerState::SteppingInto);
}

void RunDebugWidget::stepOut() {
    if (m_state != DebuggerState::Paused) return;
    
    sendGdbMiCommand("-exec-finish");
    setDebuggerState(DebuggerState::SteppingOut);
}

void RunDebugWidget::restartDebugging() {
    stopDebugging();
    QTimer::singleShot(500, this, [this]() {
        startDebugging(m_activeConfig);
    });
}

// =============================================================================
// Breakpoint Management
// =============================================================================

int RunDebugWidget::addBreakpoint(const QString& file, int line, const QString& condition) {
    // Check if breakpoint already exists
    for (const Breakpoint& bp : m_breakpoints) {
        if (bp.file == file && bp.line == line) {
            return bp.id;
        }
    }
    
    Breakpoint bp;
    bp.id = m_nextBreakpointId++;
    bp.file = file;
    bp.line = line;
    bp.condition = condition;
    bp.enabled = true;
    
    m_breakpoints.append(bp);
    
    // If debugger is running, set the breakpoint
    if (isRunning()) {
        QString cmd = QString("-break-insert %1:%2").arg(escapeGdbString(file)).arg(line);
        if (!condition.isEmpty()) {
            cmd += QString(" -c \"%1\"").arg(escapeGdbString(condition));
        }
        sendGdbMiCommand(cmd);
    }
    
    updateBreakpointsView();
    emit breakpointAdded(bp);
    
    return bp.id;
}

void RunDebugWidget::removeBreakpoint(int id) {
    for (int i = 0; i < m_breakpoints.size(); ++i) {
        if (m_breakpoints[i].id == id) {
            if (isRunning()) {
                sendGdbMiCommand(QString("-break-delete %1").arg(id));
            }
            m_breakpoints.removeAt(i);
            updateBreakpointsView();
            emit breakpointRemoved(id);
            return;
        }
    }
}

void RunDebugWidget::removeAllBreakpoints() {
    if (isRunning()) {
        for (const Breakpoint& bp : m_breakpoints) {
            sendGdbMiCommand(QString("-break-delete %1").arg(bp.id));
        }
    }
    
    QVector<int> ids;
    for (const Breakpoint& bp : m_breakpoints) {
        ids.append(bp.id);
    }
    
    m_breakpoints.clear();
    updateBreakpointsView();
    
    for (int id : ids) {
        emit breakpointRemoved(id);
    }
}

void RunDebugWidget::enableBreakpoint(int id, bool enabled) {
    for (Breakpoint& bp : m_breakpoints) {
        if (bp.id == id) {
            bp.enabled = enabled;
            if (isRunning()) {
                QString cmd = enabled ? "-break-enable" : "-break-disable";
                sendGdbMiCommand(QString("%1 %2").arg(cmd).arg(id));
            }
            updateBreakpointsView();
            emit breakpointChanged(bp);
            return;
        }
    }
}

void RunDebugWidget::setBreakpointCondition(int id, const QString& condition) {
    for (Breakpoint& bp : m_breakpoints) {
        if (bp.id == id) {
            bp.condition = condition;
            if (isRunning()) {
                sendGdbMiCommand(QString("-break-condition %1 %2").arg(id).arg(escapeGdbString(condition)));
            }
            updateBreakpointsView();
            emit breakpointChanged(bp);
            return;
        }
    }
}

QVector<Breakpoint> RunDebugWidget::getBreakpoints() const {
    return m_breakpoints;
}

Breakpoint* RunDebugWidget::findBreakpoint(const QString& file, int line) {
    for (Breakpoint& bp : m_breakpoints) {
        if (bp.file == file && bp.line == line) {
            return &bp;
        }
    }
    return nullptr;
}

// =============================================================================
// Watch/Variable Management
// =============================================================================

void RunDebugWidget::addWatchExpression(const QString& expr) {
    if (expr.isEmpty()) return;
    
    // Check if already watching
    for (const Variable& var : m_watches) {
        if (var.name == expr) return;
    }
    
    Variable watch;
    watch.name = expr;
    watch.evaluateName = expr;
    m_watches.append(watch);
    
    // Evaluate if debugger is paused
    if (isPaused()) {
        sendGdbMiCommand(QString("-data-evaluate-expression \"%1\"").arg(escapeGdbString(expr)),
            [this, expr](const QJsonObject& result) {
                QString value = result["value"].toString();
                for (Variable& w : m_watches) {
                    if (w.name == expr) {
                        w.value = value;
                        break;
                    }
                }
                // Update watch tree
                for (int i = 0; i < m_watchTree->topLevelItemCount(); ++i) {
                    QTreeWidgetItem* item = m_watchTree->topLevelItem(i);
                    if (item->text(0) == expr) {
                        item->setText(1, value);
                        break;
                    }
                }
            });
    }
    
    // Add to tree
    QTreeWidgetItem* item = new QTreeWidgetItem(m_watchTree);
    item->setText(0, expr);
    item->setText(1, "<not evaluated>");
    item->setFlags(item->flags() | Qt::ItemIsEditable);
}

void RunDebugWidget::removeWatchExpression(const QString& expr) {
    for (int i = 0; i < m_watches.size(); ++i) {
        if (m_watches[i].name == expr) {
            m_watches.removeAt(i);
            break;
        }
    }
    
    for (int i = 0; i < m_watchTree->topLevelItemCount(); ++i) {
        if (m_watchTree->topLevelItem(i)->text(0) == expr) {
            delete m_watchTree->takeTopLevelItem(i);
            break;
        }
    }
}

QString RunDebugWidget::evaluateExpression(const QString& expr) {
    if (!isPaused()) return QString();
    
    QString result;
    bool done = false;
    
    sendGdbMiCommand(QString("-data-evaluate-expression \"%1\"").arg(escapeGdbString(expr)),
        [&result, &done](const QJsonObject& r) {
            result = r["value"].toString();
            done = true;
        });
    
    // Wait for result (with timeout)
    QElapsedTimer timer;
    timer.start();
    while (!done && timer.elapsed() < 5000) {
        QApplication::processEvents();
    }
    
    return result;
}

QVector<Variable> RunDebugWidget::getLocals() {
    return m_locals;
}

// =============================================================================
// Configuration Management
// =============================================================================

void RunDebugWidget::loadLaunchConfigs() {
    m_launchConfigs.clear();
    
    QString configPath = m_workingDirectory + "/.vscode/launch.json";
    QFile file(configPath);
    
    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QJsonArray configs = doc.object()["configurations"].toArray();
        
        for (const QJsonValue& val : configs) {
            m_launchConfigs.append(LaunchConfig::fromJson(val.toObject()));
        }
        file.close();
    }
    
    // Also load from settings
    int count = m_settings->beginReadArray("LaunchConfigs");
    for (int i = 0; i < count; ++i) {
        m_settings->setArrayIndex(i);
        QJsonObject obj = QJsonDocument::fromJson(
            m_settings->value("config").toByteArray()).object();
        if (!obj.isEmpty()) {
            m_launchConfigs.append(LaunchConfig::fromJson(obj));
        }
    }
    m_settings->endArray();
}

void RunDebugWidget::saveLaunchConfigs() {
    m_settings->beginWriteArray("LaunchConfigs");
    for (int i = 0; i < m_launchConfigs.size(); ++i) {
        m_settings->setArrayIndex(i);
        QJsonDocument doc(m_launchConfigs[i].toJson());
        m_settings->setValue("config", doc.toJson(QJsonDocument::Compact));
    }
    m_settings->endArray();
}

QVector<LaunchConfig> RunDebugWidget::getLaunchConfigs() const {
    return m_launchConfigs;
}

void RunDebugWidget::addLaunchConfig(const LaunchConfig& config) {
    m_launchConfigs.append(config);
    m_configSelector->addItem(config.name);
    saveLaunchConfigs();
}

void RunDebugWidget::setWorkingDirectory(const QString& dir) {
    m_workingDirectory = dir;
    loadLaunchConfigs();
}

// =============================================================================
// GDB/MI Protocol
// =============================================================================

void RunDebugWidget::sendGdbCommand(const QString& cmd) {
    if (!m_debuggerProcess || m_debuggerProcess->state() != QProcess::Running) return;
    
    QString fullCmd = cmd + "\n";
    m_debuggerProcess->write(fullCmd.toUtf8());
    
    appendOutput("command", "> " + cmd + "\n");
}

void RunDebugWidget::sendGdbMiCommand(const QString& cmd, std::function<void(const QJsonObject&)> callback) {
    if (!m_debuggerProcess || m_debuggerProcess->state() != QProcess::Running) return;
    
    int token = ++m_commandToken;
    if (callback) {
        m_pendingCommands[token] = callback;
    }
    
    QString fullCmd = QString("%1%2\n").arg(token).arg(cmd);
    m_debuggerProcess->write(fullCmd.toUtf8());
    
    appendOutput("command", "> " + cmd + "\n");
}

void RunDebugWidget::initializeDebugger() {
    // Set up the inferior (program to debug)
    if (!m_activeConfig.program.isEmpty()) {
        sendGdbMiCommand(QString("-file-exec-and-symbols \"%1\"")
            .arg(escapeGdbString(m_activeConfig.program)));
    }
    
    // Run setup commands
    for (const QString& cmd : m_activeConfig.setupCommands) {
        sendGdbCommand(cmd);
    }
    
    // Set program arguments
    if (!m_activeConfig.args.isEmpty()) {
        sendGdbMiCommand(QString("-exec-arguments %1").arg(m_activeConfig.args.join(" ")));
    }
    
    // Setup breakpoints
    setupBreakpointsInDebugger();
    
    // Start or attach
    if (m_activeConfig.request == "attach") {
        sendGdbMiCommand(QString("-target-attach %1").arg(m_activeConfig.processId));
    } else {
        if (m_activeConfig.stopAtEntry) {
            sendGdbMiCommand("-exec-run --start");
        } else {
            sendGdbMiCommand("-exec-run");
        }
    }
    
    setDebuggerState(DebuggerState::Running);
    emit debuggingStarted();
}

void RunDebugWidget::setupBreakpointsInDebugger() {
    for (const Breakpoint& bp : m_breakpoints) {
        if (!bp.enabled) continue;
        
        QString cmd = QString("-break-insert %1:%2")
            .arg(escapeGdbString(bp.file))
            .arg(bp.line);
        
        if (!bp.condition.isEmpty()) {
            cmd += QString(" -c \"%1\"").arg(escapeGdbString(bp.condition));
        }
        
        if (bp.ignoreCount > 0) {
            cmd += QString(" -i %1").arg(bp.ignoreCount);
        }
        
        sendGdbMiCommand(cmd);
    }
}

// =============================================================================
// Process Slots
// =============================================================================

void RunDebugWidget::processDebuggerOutput() {
    if (!m_debuggerProcess) return;
    
    m_outputBuffer += QString::fromUtf8(m_debuggerProcess->readAllStandardOutput());
    
    // Process complete lines
    int newlineIndex;
    while ((newlineIndex = m_outputBuffer.indexOf('\n')) != -1) {
        QString line = m_outputBuffer.left(newlineIndex).trimmed();
        m_outputBuffer = m_outputBuffer.mid(newlineIndex + 1);
        
        if (!line.isEmpty()) {
            parseGdbMiOutput(line);
        }
    }
}

void RunDebugWidget::processDebuggerError() {
    if (!m_debuggerProcess) return;
    
    QString error = QString::fromUtf8(m_debuggerProcess->readAllStandardError());
    appendOutput("error", error);
    emit errorReceived(error);
}

void RunDebugWidget::onDebuggerStarted() {
    appendOutput("system", "Debugger started\n");
    QTimer::singleShot(100, this, &RunDebugWidget::initializeDebugger);
}

void RunDebugWidget::onDebuggerFinished(int exitCode, QProcess::ExitStatus status) {
    Q_UNUSED(status)
    
    appendOutput("system", QString("Debugger exited with code %1\n").arg(exitCode));
    
    setDebuggerState(DebuggerState::Stopped);
    emit debuggingStopped(exitCode);
    
    m_debuggerProcess->deleteLater();
    m_debuggerProcess = nullptr;
    m_pendingCommands.clear();
}

void RunDebugWidget::parseGdbMiOutput(const QString& output) {
    // GDB/MI output format:
    // ^result,... - result record
    // *async-class,... - exec async record
    // +async-class,... - status async record
    // =async-class,... - notify async record
    // ~"text" - console stream
    // @"text" - target stream
    // &"text" - log stream
    
    if (output.isEmpty()) return;
    
    QChar firstChar = output[0];
    
    // Check for token prefix
    int tokenEnd = 0;
    while (tokenEnd < output.length() && output[tokenEnd].isDigit()) {
        tokenEnd++;
    }
    
    int token = 0;
    QString remaining = output;
    if (tokenEnd > 0) {
        token = output.left(tokenEnd).toInt();
        remaining = output.mid(tokenEnd);
        firstChar = remaining.isEmpty() ? '\0' : remaining[0];
    }
    
    if (remaining.isEmpty()) return;
    
    // Parse based on record type
    switch (firstChar.toLatin1()) {
        case '^': {
            // Result record
            int commaPos = remaining.indexOf(',');
            QString resultClass = commaPos > 0 ? remaining.mid(1, commaPos - 1) : remaining.mid(1);
            QString results = commaPos > 0 ? remaining.mid(commaPos + 1) : QString();
            
            QJsonObject resultObj;
            // Simple parsing - full implementation would use proper MI parser
            if (!results.isEmpty()) {
                // Parse key=value pairs
                QRegularExpression re("(\\w+)=\"([^\"]*)\"");
                QRegularExpressionMatchIterator it = re.globalMatch(results);
                while (it.hasNext()) {
                    QRegularExpressionMatch match = it.next();
                    resultObj[match.captured(1)] = match.captured(2);
                }
            }
            
            handleResultRecord(resultClass, resultObj);
            
            // Call pending callback
            if (token > 0 && m_pendingCommands.contains(token)) {
                m_pendingCommands[token](resultObj);
                m_pendingCommands.remove(token);
            }
            break;
        }
        
        case '*': {
            // Exec async record
            int commaPos = remaining.indexOf(',');
            QString asyncClass = commaPos > 0 ? remaining.mid(1, commaPos - 1) : remaining.mid(1);
            QString results = commaPos > 0 ? remaining.mid(commaPos + 1) : QString();
            
            QJsonObject resultObj;
            QRegularExpression re("(\\w+)=\"([^\"]*)\"");
            QRegularExpressionMatchIterator it = re.globalMatch(results);
            while (it.hasNext()) {
                QRegularExpressionMatch match = it.next();
                resultObj[match.captured(1)] = match.captured(2);
            }
            
            handleAsyncRecord(asyncClass, resultObj);
            break;
        }
        
        case '~': {
            // Console output
            QString text = remaining.mid(2, remaining.length() - 3);  // Remove ~" and "
            text.replace("\\n", "\n").replace("\\t", "\t").replace("\\\"", "\"");
            handleStreamRecord("console", text);
            break;
        }
        
        case '@': {
            // Target output
            QString text = remaining.mid(2, remaining.length() - 3);
            text.replace("\\n", "\n").replace("\\t", "\t");
            handleStreamRecord("target", text);
            break;
        }
        
        case '&': {
            // Log output
            QString text = remaining.mid(2, remaining.length() - 3);
            text.replace("\\n", "\n").replace("\\t", "\t");
            handleStreamRecord("log", text);
            break;
        }
        
        case '=': {
            // Notify async record
            int commaPos = remaining.indexOf(',');
            QString asyncClass = commaPos > 0 ? remaining.mid(1, commaPos - 1) : remaining.mid(1);
            appendOutput("notify", QString("[%1]\n").arg(asyncClass));
            break;
        }
        
        default:
            appendOutput("unknown", output + "\n");
            break;
    }
}

void RunDebugWidget::handleAsyncRecord(const QString& asyncClass, const QJsonObject& results) {
    if (asyncClass == "stopped") {
        QString reason = results["reason"].toString();
        QString file = results["file"].toString();
        int line = results["line"].toString().toInt();
        
        setDebuggerState(DebuggerState::Paused);
        
        if (!file.isEmpty() && line > 0) {
            m_currentSourceFile = file;
            m_currentSourceLine = line;
            emit navigateToSource(file, line);
        }
        
        emit debuggingPaused(reason, file, line);
        
        // Refresh locals, stack, threads
        refreshAll();
        
        // Check if hit breakpoint
        if (reason == "breakpoint-hit") {
            QString bkptno = results["bkptno"].toString();
            for (Breakpoint& bp : m_breakpoints) {
                if (QString::number(bp.id) == bkptno) {
                    bp.hitCount++;
                    emit breakpointHit(bp);
                    break;
                }
            }
        }
        
        appendOutput("system", QString("Stopped: %1 at %2:%3\n").arg(reason, file).arg(line));
        
    } else if (asyncClass == "running") {
        setDebuggerState(DebuggerState::Running);
        emit debuggingContinued();
        
    } else if (asyncClass == "thread-created") {
        sendGdbMiCommand("-thread-info");
        
    } else if (asyncClass == "thread-exited") {
        sendGdbMiCommand("-thread-info");
    }
}

void RunDebugWidget::handleResultRecord(const QString& resultClass, const QJsonObject& results) {
    Q_UNUSED(results)
    
    if (resultClass == "done") {
        // Command completed successfully
    } else if (resultClass == "running") {
        setDebuggerState(DebuggerState::Running);
    } else if (resultClass == "error") {
        QString msg = results["msg"].toString();
        appendOutput("error", "Error: " + msg + "\n");
        emit errorReceived(msg);
    } else if (resultClass == "exit") {
        setDebuggerState(DebuggerState::Stopped);
    }
}

void RunDebugWidget::handleStreamRecord(const QString& streamType, const QString& text) {
    appendOutput(streamType, text);
    emit outputReceived(streamType, text);
}

// =============================================================================
// UI Update Methods
// =============================================================================

void RunDebugWidget::updateBreakpointsView() {
    m_breakpointsTree->clear();
    
    for (const Breakpoint& bp : m_breakpoints) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_breakpointsTree);
        item->setCheckState(0, bp.enabled ? Qt::Checked : Qt::Unchecked);
        item->setText(1, QFileInfo(bp.file).fileName());
        item->setText(2, QString::number(bp.line));
        item->setText(3, bp.condition);
        item->setText(4, QString::number(bp.hitCount));
        item->setData(0, Qt::UserRole, bp.id);
        item->setToolTip(1, bp.file);
        
        if (!bp.verified && isRunning()) {
            item->setForeground(1, Qt::gray);
        }
    }
}

void RunDebugWidget::updateCallStackView(const QVector<StackFrame>& frames) {
    m_callStackTree->clear();
    m_callStack = frames;
    
    for (const StackFrame& frame : frames) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_callStackTree);
        item->setText(0, QString::number(frame.level));
        item->setText(1, frame.function);
        item->setText(2, QFileInfo(frame.file).fileName());
        item->setText(3, QString::number(frame.line));
        item->setText(4, frame.address);
        item->setData(0, Qt::UserRole, frame.level);
        item->setToolTip(2, frame.file);
    }
    
    emit callStackUpdated(frames);
}

void RunDebugWidget::updateVariablesView(const QVector<Variable>& vars) {
    m_variablesTree->clear();
    m_locals = vars;
    
    for (const Variable& var : vars) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_variablesTree);
        item->setText(0, var.name);
        item->setText(1, var.value);
        item->setText(2, var.type);
        
        if (var.hasChildren) {
            item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        }
        
        item->setData(0, Qt::UserRole, var.evaluateName);
        item->setData(0, Qt::UserRole + 1, var.variablesReference);
    }
    
    emit variablesUpdated(vars);
}

void RunDebugWidget::updateThreadsView(const QVector<ThreadInfo>& threads) {
    m_threadsTree->clear();
    m_threads = threads;
    
    for (const ThreadInfo& thread : threads) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_threadsTree);
        item->setText(0, QString::number(thread.id));
        item->setText(1, thread.name);
        item->setText(2, thread.state);
        
        if (thread.currentFrame) {
            item->setText(3, QString("%1:%2").arg(thread.currentFrame->function)
                .arg(thread.currentFrame->line));
        }
        
        item->setData(0, Qt::UserRole, thread.id);
        
        if (thread.id == m_currentThreadId) {
            item->setIcon(0, style()->standardIcon(QStyle::SP_ArrowRight));
        }
    }
    
    emit threadsUpdated(threads);
}

void RunDebugWidget::updateRegistersView(const QMap<QString, QString>& regs) {
    m_registers = regs;
    m_registersTable->setRowCount(regs.size());
    
    int row = 0;
    for (auto it = regs.begin(); it != regs.end(); ++it, ++row) {
        m_registersTable->setItem(row, 0, new QTableWidgetItem(it.key()));
        m_registersTable->setItem(row, 1, new QTableWidgetItem(it.value()));
    }
}

void RunDebugWidget::setDebuggerState(DebuggerState newState) {
    DebuggerState oldState = m_state;
    m_state = newState;
    
    // Update button states
    bool stopped = (newState == DebuggerState::Stopped);
    bool running = (newState == DebuggerState::Running);
    bool paused = (newState == DebuggerState::Paused);
    
    m_startBtn->setEnabled(stopped);
    m_stopBtn->setEnabled(!stopped);
    m_pauseBtn->setEnabled(running);
    m_continueBtn->setEnabled(paused);
    m_stepOverBtn->setEnabled(paused);
    m_stepIntoBtn->setEnabled(paused);
    m_stepOutBtn->setEnabled(paused);
    m_restartBtn->setEnabled(!stopped);
    
    if (oldState != newState) {
        emit stateChanged(newState);
    }
}

void RunDebugWidget::appendOutput(const QString& category, const QString& text) {
    QTextCharFormat format;
    
    if (category == "error") {
        format.setForeground(QColor("#f44747"));
    } else if (category == "command") {
        format.setForeground(QColor("#569cd6"));
    } else if (category == "system") {
        format.setForeground(QColor("#4ec9b0"));
    } else if (category == "console") {
        format.setForeground(QColor("#d4d4d4"));
    } else {
        format.setForeground(QColor("#9cdcfe"));
    }
    
    QTextCursor cursor = m_debugConsole->textCursor();
    cursor.movePosition(QTextCursor::End);
    cursor.insertText(text, format);
    m_debugConsole->setTextCursor(cursor);
    m_debugConsole->ensureCursorVisible();
}

// =============================================================================
// Slots
// =============================================================================

void RunDebugWidget::onConfigurationChanged(int index) {
    if (index >= 0 && index < m_launchConfigs.size()) {
        m_activeConfig = m_launchConfigs[index];
    }
}

void RunDebugWidget::onBreakpointDoubleClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)
    
    int bpId = item->data(0, Qt::UserRole).toInt();
    for (const Breakpoint& bp : m_breakpoints) {
        if (bp.id == bpId) {
            emit navigateToSource(bp.file, bp.line);
            break;
        }
    }
}

void RunDebugWidget::onStackFrameClicked(QTreeWidgetItem* item, int column) {
    Q_UNUSED(column)
    
    int level = item->data(0, Qt::UserRole).toInt();
    
    if (level >= 0 && level < m_callStack.size()) {
        const StackFrame& frame = m_callStack[level];
        m_currentFrameLevel = level;
        
        if (!frame.file.isEmpty() && frame.line > 0) {
            emit navigateToSource(frame.file, frame.line);
        }
        
        // Request locals for this frame
        if (isPaused()) {
            sendGdbMiCommand(QString("-stack-select-frame %1").arg(level));
            sendGdbMiCommand("-stack-list-locals --all-values");
        }
    }
}

void RunDebugWidget::onVariableExpanded(QTreeWidgetItem* item) {
    if (item->childCount() > 0) return;  // Already expanded
    
    int varRef = item->data(0, Qt::UserRole + 1).toInt();
    if (varRef <= 0) return;
    
    QString expr = item->data(0, Qt::UserRole).toString();
    
    // Request children from debugger
    if (isPaused()) {
        sendGdbMiCommand(QString("-var-list-children --all-values \"%1\"").arg(expr),
            [item](const QJsonObject& result) {
                // Parse children and add to tree
                // Implementation depends on exact MI output format
                Q_UNUSED(result)
            });
    }
}

void RunDebugWidget::onWatchExpressionAdded() {
    QString expr = m_watchInput->text().trimmed();
    if (!expr.isEmpty()) {
        addWatchExpression(expr);
        m_watchInput->clear();
    }
}

void RunDebugWidget::onDebugConsoleCommand() {
    QString cmd = m_consoleInput->text().trimmed();
    if (!cmd.isEmpty()) {
        sendGdbCommand(cmd);
        m_consoleInput->clear();
    }
}

void RunDebugWidget::refreshAll() {
    if (!isPaused()) return;
    
    // Refresh call stack
    sendGdbMiCommand("-stack-list-frames", [this](const QJsonObject& result) {
        QVector<StackFrame> frames;
        // Parse frames from result
        // Format: stack=[frame={level="0",addr="0x...",func="main",...},...]
        Q_UNUSED(result)
        // Simplified - full implementation would parse MI output
        updateCallStackView(frames);
    });
    
    // Refresh locals
    sendGdbMiCommand("-stack-list-locals --all-values", [this](const QJsonObject& result) {
        QVector<Variable> locals;
        Q_UNUSED(result)
        updateVariablesView(locals);
    });
    
    // Refresh threads
    sendGdbMiCommand("-thread-info", [this](const QJsonObject& result) {
        QVector<ThreadInfo> threads;
        Q_UNUSED(result)
        updateThreadsView(threads);
    });
    
    // Refresh registers
    sendGdbMiCommand("-data-list-register-values x", [this](const QJsonObject& result) {
        QMap<QString, QString> regs;
        Q_UNUSED(result)
        updateRegistersView(regs);
    });
    
    // Refresh watches
    for (int i = 0; i < m_watchTree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_watchTree->topLevelItem(i);
        QString expr = item->text(0);
        
        sendGdbMiCommand(QString("-data-evaluate-expression \"%1\"").arg(escapeGdbString(expr)),
            [item](const QJsonObject& result) {
                item->setText(1, result["value"].toString());
            });
    }
}

// =============================================================================
// Utility Methods
// =============================================================================

QString RunDebugWidget::findDebugger(const QString& type) {
    if (type == "lldb") {
#ifdef Q_OS_WIN
        return "C:/Program Files/LLVM/bin/lldb-mi.exe";
#else
        return "/usr/bin/lldb-mi";
#endif
    } else {
        // Default to GDB
#ifdef Q_OS_WIN
        return "C:/msys64/mingw64/bin/gdb.exe";
#else
        return "/usr/bin/gdb";
#endif
    }
}

QString RunDebugWidget::escapeGdbString(const QString& str) {
    QString result = str;
    result.replace("\\", "\\\\");
    result.replace("\"", "\\\"");
    return result;
}

void RunDebugWidget::parseVariableChildren(Variable& parent, const QJsonArray& children) {
    for (const QJsonValue& val : children) {
        QJsonObject childObj = val.toObject();
        Variable child;
        child.name = childObj["name"].toString();
        child.value = childObj["value"].toString();
        child.type = childObj["type"].toString();
        child.hasChildren = childObj["numchild"].toString().toInt() > 0;
        parent.children.append(child);
    }
}
