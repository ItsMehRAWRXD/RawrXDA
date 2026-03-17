#include "MainWindow.h"
#include "ui/AISuggestionOverlay.h"
#include "StreamerClient.h"
#include "ui/TaskProposalWidget.h"
#include "orchestrator/AgentOrchestrator.h"
#include "widgets/CloudRunnerWidget.h"
#include "widgets/PluginManagerWidget.h"
#include <QJsonDocument>
#include <QRegularExpression>

#include <QAction>
#include <QCloseEvent>
#include <QColor>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMainWindow>
#include <QMenu>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QUrl>
#include <QFile>
#include <QIODevice>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent) {
    setAcceptDrops(true);  // Enable drag-drop for files
    
    // Top Goal Bar
    QWidget* goalBar = createGoalBar();
    QWidget* goalBarContainer = new QWidget(this);
    QVBoxLayout* topLayout = new QVBoxLayout(goalBarContainer);
    topLayout->setContentsMargins(0,0,0,0);
    topLayout->addWidget(goalBar);

    // Center editor area with tabs
    QWidget* editor = createEditorArea();
    topLayout->addWidget(editor);
    setCentralWidget(goalBarContainer);

    // Left Agent Panel dock
    QDockWidget* agentDock = new QDockWidget(tr("Agent Control Panel"), this);
    agentDock->setObjectName("AgentControlPanelDock");
    agentDock->setWidget(createAgentPanel());
    addDockWidget(Qt::LeftDockWidgetArea, agentDock);

    // Right Proposal Review dock
    QDockWidget* proposalDock = new QDockWidget(tr("Proposal Review"), this);
    proposalDock->setObjectName("ProposalReviewDock");
    proposalDock->setWidget(createProposalReview());
    addDockWidget(Qt::RightDockWidgetArea, proposalDock);

    // Bottom Terminal dock
    terminalDock_ = new QDockWidget(tr("Terminal"), this);
    terminalDock_->setObjectName("TerminalDock");
    terminalDock_->setWidget(createTerminalPanel());
    addDockWidget(Qt::BottomDockWidgetArea, terminalDock_);

    // Debug/Logging dock (right side)
    debugDock_ = new QDockWidget(tr("Debug & Logs"), this);
    debugDock_->setObjectName("DebugDock");
    debugDock_->setWidget(createDebugPanel());
    addDockWidget(Qt::RightDockWidgetArea, debugDock_);

    // Cloud Runner dock (bottom area)
    cloudRunner_ = new CloudRunnerWidget(this);
    QDockWidget* cloudDock = new QDockWidget(tr("⚡ Cloud Runner"), this);
    cloudDock->setObjectName("CloudRunnerDock");
    cloudDock->setWidget(cloudRunner_);
    addDockWidget(Qt::BottomDockWidgetArea, cloudDock);
    
    connect(cloudRunner_, &CloudRunnerWidget::jobCompleted, this, [this](bool success, const QString& url) {
        if (success && qshellOutput_) {
            qshellOutput_->append(QString("[CLOUD] Job completed! Artifacts: %1").arg(url));
        }
    });
    
    // Plugin Manager dock (right area)
    pluginMgr_ = new PluginManagerWidget(this);
    QDockWidget* pluginDock = new QDockWidget(tr("🧩 Plugins"), this);
    pluginDock->setObjectName("PluginManagerDock");
    pluginDock->setWidget(pluginMgr_);
    addDockWidget(Qt::RightDockWidgetArea, pluginDock);
    
    connect(pluginMgr_, &PluginManagerWidget::logMessage, this, [this](const QString& msg) {
        if (qshellOutput_) {
            qshellOutput_->append(QString("[PLUGIN] %1").arg(msg));
        }
    });

    // Bottom QShell tab (as additional tab in editor)
    editorTabs_->addTab(createQShellTab(), tr("QShell"));

    // Connect signals
    connect(this, &MainWindow::onGoalSubmitted, this, &MainWindow::handleAgentMockProgress);

    // Suggestion overlay geometry management
    if (codeView_) {
        codeView_->installEventFilter(this);
        // TEMPORARILY DISABLED: overlay_ = new AISuggestionOverlay(codeView_);
        // overlay_->setOpacity(0.35);
        // overlay_->setGeometry(codeView_->geometry());
    }

    // Enable Enter-to-send on goal input
    if (goalInput_) {
        goalInput_->installEventFilter(this);
    }

    // Initialize StreamerClient for AI model communication
    streamer_ = new StreamerClient(streamerUrl_, this);

    // Streamer wiring for suggestions and visible chat output
    connect(streamer_, &StreamerClient::chunkReceived, this, &MainWindow::updateSuggestion);
    connect(streamer_, &StreamerClient::chunkReceived, this, &MainWindow::appendModelChunk);
    connect(streamer_, &StreamerClient::completed, this, &MainWindow::handleGenerationFinished);

    // Instantiate orchestrator and bind to streamer
    orchestrator_ = new AgentOrchestrator(this);
    orchestrator_->setStreamer(streamer_);
    
    // Connect orchestrator signals
    connect(orchestrator_, &AgentOrchestrator::taskStatusUpdated,
            this, &MainWindow::handleTaskStatusUpdate);
    connect(orchestrator_, &AgentOrchestrator::orchestrationFinished,
            this, &MainWindow::handleWorkflowFinished);
    connect(orchestrator_, &AgentOrchestrator::taskChunk,
            this, &MainWindow::handleTaskStreaming);
    
    // Forward StreamerClient task completion into orchestrator
    connect(streamer_, &StreamerClient::taskCompleted,
            this, [this](bool success, const QString& taskId, const QString& modelName, const QString& fullOutput){
                Q_UNUSED(modelName);
                Q_UNUSED(fullOutput);
                if (orchestrator_) orchestrator_->handleTaskCompletion(taskId, success);
            });

    // QShell input handler for config toggles
    if (qshellInput_) {
        disconnect(qshellInput_, nullptr, nullptr, nullptr);
        connect(qshellInput_, &QLineEdit::returnPressed, this, &MainWindow::handleQShellReturn);
    }

    // Start terminals
    if (pwshOutput_) pwshOutput_->appendPlainText("PowerShell initialized (pending start)\n");
    if (cmdOutput_) cmdOutput_->appendPlainText("CMD initialized (pending start)\n");
    
    resize(1400, 900);
    setWindowTitle(tr("RawrXD - AI-First IDE (Kitchen Sink Edition)"));
}

MainWindow::~MainWindow() = default;

/* ============================================================
   EVENT HANDLERS
   ============================================================ */

void MainWindow::closeEvent(QCloseEvent* event) {
    saveSession();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event) {
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        for (const QUrl& url : mimeData->urls()) {
            QString filePath = url.toLocalFile();
            if (!filePath.isEmpty()) {
                loadContextItemIntoEditor(new QListWidgetItem(QStringLiteral("📄 ") + filePath));
            }
        }
    }
}

/* ============================================================
   SESSION MANAGEMENT STUBS
   ============================================================ */

void MainWindow::setupMenuBar() {}
void MainWindow::setupToolBars() {}
void MainWindow::setupDockWidgets() {}
void MainWindow::setupStatusBar() {}
void MainWindow::setupSystemTray() {}
void MainWindow::setupShortcuts() {}
void MainWindow::restoreSession() {}
void MainWindow::saveSession() {}

/* ============================================================
   ORIGINAL UI CREATORS
   ============================================================ */

QWidget* MainWindow::createGoalBar() {
    QWidget* w = new QWidget(this);
    QHBoxLayout* layout = new QHBoxLayout(w);
    layout->setContentsMargins(6,6,6,6);

    // Agent selector
    agentSelector_ = new QComboBox(w);
    agentSelector_->addItems({
        tr("Auto Select"),
        tr("Feature Agent"),
        tr("Security Agent"),
        tr("Performance Agent"),
        tr("Debug Agent"),
        tr("Refactor Agent"),
        tr("Documentation Agent")
    });

    goalInput_ = new QLineEdit(w);
    goalInput_->setPlaceholderText(tr("Ask a question or describe what you want to build..."));

    QPushButton* submitBtn = new QPushButton(tr("Send"), w);
    connect(submitBtn, &QPushButton::clicked, this, &MainWindow::handleGoalSubmit);

    layout->addWidget(new QLabel(tr("Agent:"), w));
    layout->addWidget(agentSelector_);
    layout->addWidget(new QLabel(tr("Chat:"), w));
    layout->addWidget(goalInput_, 1);
    layout->addWidget(submitBtn);

    // Mock Architect status badge
    if (!mockStatusBadge_) {
        mockStatusBadge_ = new QLabel(this);
        mockStatusBadge_->setText(tr("MOCK ARCHITECT: ON"));
        mockStatusBadge_->setStyleSheet("color: red; font-weight: bold; margin-left: 10px;");
        mockStatusBadge_->setVisible(forceMockArchitect_);
    }
    layout->addWidget(mockStatusBadge_);
    return w;
}

QWidget* MainWindow::createAgentPanel() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* v = new QVBoxLayout(w);
    v->setContentsMargins(6,6,6,6);

    QLabel* heading = new QLabel(tr("📝 Chat History"), w);
    heading->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    chatHistory_ = new QListWidget(w);
    chatHistory_->setStyleSheet("QListWidget { background: #2b2b2b; color: #e0e0e0; border: 1px solid #555; }");
    
    QPushButton* newChatBtn = new QPushButton(tr("+ New Chat"), w);
    QPushButton* newEditorBtn = new QPushButton(tr("+ New Editor"), w);
    QPushButton* newWindowBtn = new QPushButton(tr("+ New Window"), w);
    
    connect(newChatBtn, &QPushButton::clicked, this, &MainWindow::handleNewChat);
    connect(newEditorBtn, &QPushButton::clicked, this, &MainWindow::handleNewEditor);
    connect(newWindowBtn, &QPushButton::clicked, this, &MainWindow::handleNewWindow);
    
    v->addWidget(heading);
    v->addWidget(chatHistory_, 1);
    v->addWidget(newChatBtn);
    v->addWidget(newEditorBtn);
    v->addWidget(newWindowBtn);
    v->addSpacing(12);
    
    // Persistence controls
    QLabel* persistTitle = new QLabel(tr("📁 Workflow State"), w);
    persistTitle->setStyleSheet("font-weight: bold;");
    QPushButton* saveStateBtn = new QPushButton(tr("💾 Save State"), w);
    QPushButton* loadStateBtn = new QPushButton(tr("📂 Load State"), w);
    connect(saveStateBtn, &QPushButton::clicked, this, &MainWindow::handleSaveState);
    connect(loadStateBtn, &QPushButton::clicked, this, &MainWindow::handleLoadState);
    
    v->addWidget(persistTitle);
    v->addWidget(saveStateBtn);
    v->addWidget(loadStateBtn);
    v->addStretch();
    return w;
}

QWidget* MainWindow::createProposalReview() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* v = new QVBoxLayout(w);
    v->setContentsMargins(6,6,6,6);

    QLabel* title = new QLabel(tr("🔧 Context & Tools"), w);
    title->setStyleSheet("font-weight: bold; font-size: 14px;");
    
    contextList_ = new QListWidget(w);
    contextList_->setStyleSheet("QListWidget { background: #2b2b2b; color: #e0e0e0; border: 1px solid #555; }");
    contextList_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(contextList_, &QListWidget::customContextMenuRequested, this, &MainWindow::showContextMenu);
    connect(contextList_, &QListWidget::itemDoubleClicked, this, &MainWindow::loadContextItemIntoEditor);
    
    QPushButton* addFileBtn = new QPushButton(tr("+ Add File"), w);
    QPushButton* addFolderBtn = new QPushButton(tr("+ Add Folder"), w);
    QPushButton* addSymbolBtn = new QPushButton(tr("+ Add Symbol"), w);
    
    connect(addFileBtn, &QPushButton::clicked, this, &MainWindow::handleAddFile);
    connect(addFolderBtn, &QPushButton::clicked, this, &MainWindow::handleAddFolder);
    connect(addSymbolBtn, &QPushButton::clicked, this, &MainWindow::handleAddSymbol);
    
    v->addWidget(title);
    v->addWidget(contextList_, 1);
    v->addWidget(addFileBtn);
    v->addWidget(addFolderBtn);
    v->addWidget(addSymbolBtn);
    return w;
}

QWidget* MainWindow::createEditorArea() {
    editorTabs_ = new QTabWidget(this);
    codeView_ = new QTextEdit(editorTabs_);
    codeView_->setReadOnly(false);
    codeView_->setPlaceholderText(tr("Chat with AI model here...\n\nYou can:\n• Ask questions about code\n• Request code generation\n• Debug issues\n• Explain concepts\n\nPress Ctrl+Enter to send or use the Send button above."));
    codeView_->setText("");

    // Attach Copilot context menu
    codeView_->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(codeView_, &QTextEdit::customContextMenuRequested, this, &MainWindow::showEditorContextMenu);

    // Make tabs closable
    editorTabs_->setTabsClosable(true);
    connect(editorTabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::handleTabClose);

    QWidget* editorContainer = new QWidget(editorTabs_);
    QVBoxLayout* v = new QVBoxLayout(editorContainer);
    v->setContentsMargins(6,6,6,6);
    v->addWidget(codeView_);

    // OverlayWidget will be created in constructor and parented to codeView_

    editorTabs_->addTab(editorContainer, tr("Chat"));
    return editorTabs_;
}

QWidget* MainWindow::createQShellTab() {
    QWidget* w = new QWidget(editorTabs_);
    QVBoxLayout* v = new QVBoxLayout(w);
    v->setContentsMargins(6,6,6,6);
    qshellOutput_ = new QTextEdit(w);
    qshellOutput_->setReadOnly(true);
    qshellOutput_->setText(tr(
        "=== RawrXD QShell - Agentic Command Interface ===\n\n"
        "CONTEXT COMMANDS (prefix with /):\n"
        "  /ask <question>        - Ask AI a question\n"
        "  /fix <code>            - Fix code issues\n"
        "  /explain <code>        - Explain code\n"
        "  /refactor <code>       - Refactor code\n"
        "  /test <code>           - Generate tests\n"
        "  /doc <code>            - Generate documentation\n\n"
        "AGENTIC COMMANDS:\n"
        "  Invoke-QAgent -Agent Feature|Security|Performance \"prompt\"\n"
        "  Retry-Blocked          - Retry blocked tasks\n"
        "  Set-MockArchitect On|Off\n\n"
        "WORKFLOW COMMANDS:\n"
        "  Start-Swarm            - Start local swarm engine\n"
        "  Stop-Swarm             - Stop swarm engine\n"
        "  Build-Model <name>     - Compile new model\n"
        "  Harvest-LLM            - Start LLM data harvesting\n\n"
        "Type 'help' for more commands\n\n"
    ));
    qshellInput_ = new QLineEdit(w);
    qshellInput_->setPlaceholderText(tr("QShell> Type commands like: Invoke-QAgent -Goal \"...\""));
    v->addWidget(qshellOutput_, 1);
    v->addWidget(qshellInput_);

    // Connected in constructor to handleQShellReturn
    return w;
}

void MainWindow::handleGoalSubmit() {
    const QString goal = goalInput_ ? goalInput_->text() : QString();
    if (goal.isEmpty()) return;
    emit onGoalSubmitted(goal);
    
    // Display in chat
    if (codeView_) {
        codeView_->append(QString("\n**You:** %1\n").arg(goal));
    }
    
    // Backend generation via StreamerClient
    QString agentModel = QStringLiteral("quantumide-feature");
    if (agentSelector_) {
        const QString sel = agentSelector_->currentText().toLower();
        if (sel.contains("performance")) agentModel = QStringLiteral("quantumide-performance");
        else if (sel.contains("security")) agentModel = QStringLiteral("quantumide-security");
        else if (sel.contains("debug")) agentModel = QStringLiteral("quantumide-debug");
        else if (sel.contains("refactor")) agentModel = QStringLiteral("quantumide-refactor");
        else if (sel.contains("documentation")) agentModel = QStringLiteral("quantumide-docs");
        else if (sel.contains("feature")) agentModel = QStringLiteral("quantumide-feature");
    }

    suggestionBuffer_.clear();
    if (overlay_) overlay_->clear();
    if (qshellOutput_) qshellOutput_->append(QString("[Invoke] Model=%1 Prompt=\"%2\"").arg(agentModel, goal));
    if (streamer_) streamer_->startGeneration(agentModel, goal);
}

void MainWindow::handleArchitectChunk(const QString& chunk) {
    architectBuffer_ += chunk;
    // Display in chat
    if (codeView_) {
        codeView_->moveCursor(QTextCursor::End);
        codeView_->insertPlainText(chunk);
    }
}

void MainWindow::handleArchitectFinished() {
    // Restore suggestion wiring
    if (streamer_) {
        disconnect(streamer_, &StreamerClient::chunkReceived, this, &MainWindow::handleArchitectChunk);
        disconnect(streamer_, &StreamerClient::completed, this, &MainWindow::handleArchitectFinished);
        connect(streamer_, &StreamerClient::chunkReceived, this, &MainWindow::updateSuggestion);
        connect(streamer_, &StreamerClient::completed, this, &MainWindow::handleGenerationFinished);
    }

    architectRunning_ = false;

    // Parse JSON and hand off to orchestrator, with mock fallback and force toggle
    QJsonDocument architectDoc;
    bool usedMock = false;
    if (forceMockArchitect_) {
        architectDoc = getMockArchitectJson();
        usedMock = true;
        if (qshellOutput_) qshellOutput_->append("[Architect] Using mock task graph (forced).");
    } else {
        if (!architectBuffer_.isEmpty()) {
            QJsonParseError err;
            architectDoc = QJsonDocument::fromJson(architectBuffer_.toUtf8(), &err);
            if (err.error != QJsonParseError::NoError || architectDoc.isNull()) {
                if (qshellOutput_) qshellOutput_->append(QString("[Architect] JSON parse error: %1").arg(err.errorString()));
            }
        }
        if (architectDoc.isNull() || !architectDoc.isObject()) {
            architectDoc = getMockArchitectJson();
            usedMock = true;
            if (qshellOutput_) qshellOutput_->append("[Architect] Using mock task graph fallback.");
        }
    }

        if (orchestrator_) orchestrator_->startWorkflow(QString::fromUtf8(architectDoc.toJson(QJsonDocument::Compact)));
    architectBuffer_.clear();
}

QJsonDocument MainWindow::getMockArchitectJson() const {
    const QString jsonString = R"({
        "goal": "Implement user auth service",
        "task_graph": [
            {
                "task_id": "T1_FEAT",
                "agent": "feature",
                "prompt": "Implement the core login function using email and password.",
                "dependencies": []
            },
            {
                "task_id": "T2_SEC",
                "agent": "security",
                "prompt": "Review T1's login function for password hashing flaws.",
                "dependencies": ["T1_FEAT"]
            },
            {
                "task_id": "T3_PERF",
                "agent": "performance",
                "prompt": "Profile the login latency and suggest indexing optimizations.",
                "dependencies": ["T1_FEAT"]
            }
        ]
    })";
    return QJsonDocument::fromJson(jsonString.toUtf8());
}

void MainWindow::handleTaskStatusUpdate(const QString& taskId, const QString& status, const QString& agentType) {
    // Display task status updates in QShell
    if (qshellOutput_) {
        qshellOutput_->append(QString("[%1] %2: %3").arg(taskId, agentType, status));
    }
}

void MainWindow::handleTaskCompleted(const QString& agentType, const QString& summary) {
    if (qshellOutput_) {
        qshellOutput_->append(QString("[%1] Completed: %2").arg(agentType, summary));
    }
}

void MainWindow::handleWorkflowFinished(bool success) {
    if (qshellOutput_) {
        qshellOutput_->append(success ? "[✅ SUCCESS] Workflow completed" : "[❌ FAILED] Workflow completed with issues");
    }
}

void MainWindow::handleTaskStreaming(const QString& taskId, const QString& chunk, const QString& agentType) {
    // Display streaming task output in QShell
    if (qshellOutput_) {
        qshellOutput_->append(QString("[%1] %2: %3").arg(taskId, agentType, chunk));
    }
}

void MainWindow::handleAgentMockProgress() {
    // Update chat history with new session
    if (chatHistory_) {
        chatHistory_->addItem(QString("Session: %1").arg(goalInput_->text()));
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == codeView_) {
        if (event->type() == QEvent::Resize || event->type() == QEvent::Move) {
            if (overlay_) overlay_->setGeometry(codeView_->geometry());
        }
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if ((keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) &&
                (keyEvent->modifiers() & Qt::ControlModifier)) {
                handleGoalSubmit();
                return true;
            }
        }
    }
    if (watched == goalInput_) {
        if (event->type() == QEvent::KeyPress) {
            auto* keyEvent = static_cast<QKeyEvent*>(event);
            if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
                handleGoalSubmit();
                return true;
            }
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::updateSuggestion(const QString& chunk) {
    if (!suggestionEnabled_) return;
    suggestionBuffer_ += chunk;
    if (overlay_) overlay_->setGhostText(suggestionBuffer_);
}

void MainWindow::appendModelChunk(const QString& chunk) {
    if (codeView_) {
        codeView_->moveCursor(QTextCursor::End);
        codeView_->insertPlainText(chunk);
    }
}

void MainWindow::handleGenerationFinished() {
    suggestionBuffer_.clear();
    if (overlay_) overlay_->clear();
}

void MainWindow::handleQShellReturn() {
    const QString cmd = qshellInput_ ? qshellInput_->text() : QString();
    if (cmd.isEmpty()) return;
    auto echo = [this, cmd](const QString& tag){ qshellOutput_->append(tag + " " + cmd); };

    if (cmd.startsWith("Set-QConfig", Qt::CaseInsensitive)) {
        // Example: Set-QConfig -Suggest on|off
        if (cmd.contains("-Suggest", Qt::CaseInsensitive)) {
            if (cmd.contains("on", Qt::CaseInsensitive)) {
                suggestionEnabled_ = true;
                qshellOutput_->append("[Set-QConfig] Suggest enabled");
            } else if (cmd.contains("off", Qt::CaseInsensitive)) {
                suggestionEnabled_ = false;
                qshellOutput_->append("[Set-QConfig] Suggest disabled");
                if (overlay_) overlay_->clear();
                suggestionBuffer_.clear();
            } else {
                qshellOutput_->append("[Set-QConfig] Usage: Set-QConfig -Suggest on|off");
            }
        } else if (cmd.contains("-UseMockArchitect", Qt::CaseInsensitive)) {
            QRegularExpression reMockConfig("^\\s*Set-QConfig\\s+-UseMockArchitect\\s+(on|off)\\s*$", QRegularExpression::CaseInsensitiveOption);
            const auto mm = reMockConfig.match(cmd);
            if (mm.hasMatch()) {
                const QString state = mm.captured(1).toLower();
                forceMockArchitect_ = (state == QStringLiteral("on"));
                qshellOutput_->append(forceMockArchitect_ ? "[System] Mock Architect usage forced ON. Live output will be ignored." : "[System] Mock Architect usage forced OFF. Live output will be used.");
                if (mockStatusBadge_) mockStatusBadge_->setVisible(forceMockArchitect_);
            } else {
                qshellOutput_->append("[Set-QConfig] Usage: Set-QConfig -UseMockArchitect on|off");
            }
        } else {
            echo("[Set-QConfig]");
        }
    } else if (cmd.startsWith("Invoke-QAgent", Qt::CaseInsensitive)) {
        // Parse: Invoke-QAgent -Agent <Feature|Security|Performance> "<prompt>"
        QRegularExpression reAgent(
            "^\\s*Invoke-QAgent\\s+-Agent\\s+([A-Za-z]+)\\s+\"([^\"]+)\"\\s*$",
            QRegularExpression::CaseInsensitiveOption);
        const auto match = reAgent.match(cmd);
        if (match.hasMatch()) {
            const QString agentType = match.captured(1).toLower();
            const QString prompt = match.captured(2);
            QString agentModel = QStringLiteral("quantumide-feature");
            if (agentType.startsWith("perf")) agentModel = QStringLiteral("quantumide-performance");
            else if (agentType.startsWith("sec")) agentModel = QStringLiteral("quantumide-security");
            else if (agentType.startsWith("feat")) agentModel = QStringLiteral("quantumide-feature");

            // Reset suggestion state
            suggestionBuffer_.clear();
            if (overlay_) overlay_->clear();

            if (streamer_) {
                qshellOutput_->append(QString("[Invoke-QAgent] Model=%1 Prompt=\"%2\"").arg(agentModel, prompt));
                streamer_->startGeneration(agentModel, prompt);
            } else {
                qshellOutput_->append("[ERROR] Streamer Client not initialized.");
            }
        } else {
            qshellOutput_->append("[Invoke-QAgent] Usage: Invoke-QAgent -Agent Feature|Security|Performance \"prompt\"");
        }
    } else if (cmd.startsWith("Retry-Blocked", Qt::CaseInsensitive)) {
        if (orchestrator_) {
            orchestrator_->retryBlockedTasks();
            qshellOutput_->append("[Control] Retry-Blocked invoked: re-evaluating blocked tasks.");
        }
    } else if (cmd.startsWith("Get-QContext", Qt::CaseInsensitive)) {
        echo("[Get-QContext]");
    } else if (cmd.startsWith("Set-QConfig", Qt::CaseInsensitive)) {
        // Already handled above; extend for -Retries <n>
        QRegularExpression reRetries("^\\s*Set-QConfig\\s+-Retries\\s+(\\d+)\\s*$", QRegularExpression::CaseInsensitiveOption);
        const auto mRet = reRetries.match(cmd);
        if (mRet.hasMatch()) {
            int n = mRet.captured(1).toInt();
            if (orchestrator_) orchestrator_->setMaxRetries(n);
            qshellOutput_->append(QString("[Set-QConfig] Max retries set to %1").arg(n));
        }
    } else {
        qshellOutput_->append("Unknown command. Try Invoke-QAgent / Get-QContext / Set-QConfig");
    }

    if (qshellInput_) qshellInput_->clear();
}

void MainWindow::handleSaveState() {
    if (!orchestrator_) {
        qshellOutput_->append("[ERROR] Orchestrator not initialized");
        return;
    }
    
    QString defaultPath = QDir::homePath() + "/orchestration_state.json";
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Orchestration State"),
        defaultPath,
        tr("JSON Files (*.json);;All Files (*)")
    );
    
    if (filePath.isEmpty()) {
        return; // User cancelled
    }
    
    bool success = orchestrator_->saveOrchestrationState(filePath);
    if (success) {
        qshellOutput_->append(QString("[💾 SAVED] Orchestration state saved to: %1").arg(filePath));
        mockStatusBadge_->setText(tr("STATE SAVED ✓"));
        mockStatusBadge_->setStyleSheet("color: green; font-weight: bold; margin-left: 10px;");
        mockStatusBadge_->setVisible(true);
    } else {
        qshellOutput_->append(QString("[ERROR] Failed to save state to: %1").arg(filePath));
    }
}

void MainWindow::handleLoadState() {
    if (!orchestrator_) {
        qshellOutput_->append("[ERROR] Orchestrator not initialized");
        return;
    }
    
    QString defaultPath = QDir::homePath();
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Load Orchestration State"),
        defaultPath,
        tr("JSON Files (*.json);;All Files (*)")
    );
    
    if (filePath.isEmpty()) {
        return; // User cancelled
    }
    
    bool success = orchestrator_->loadOrchestrationState(filePath);
    if (success) {
        qshellOutput_->append(QString("[📂 LOADED] Orchestration state loaded from: %1").arg(filePath));
        qshellOutput_->append("[INFO] Workflow resumed automatically if tasks were pending");
        mockStatusBadge_->setText(tr("STATE LOADED ✓"));
        mockStatusBadge_->setStyleSheet("color: blue; font-weight: bold; margin-left: 10px;");
        mockStatusBadge_->setVisible(true);
    } else {
        qshellOutput_->append(QString("[ERROR] Failed to load state from: %1").arg(filePath));
    }
}

void MainWindow::handleNewChat() {
    QString chatName = QString("Chat %1").arg(editorTabs_->count() + 1);
    QTextEdit* newChat = new QTextEdit(editorTabs_);
    newChat->setPlaceholderText(tr("New chat session...\n\nAsk questions, request code, or discuss ideas."));
    editorTabs_->addTab(newChat, chatName);
    editorTabs_->setCurrentWidget(newChat);
    
    if (chatHistory_) {
        chatHistory_->addItem(chatName);
    }
}

void MainWindow::handleNewEditor() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Open File"));
    if (!fileName.isEmpty()) {
        QTextEdit* editor = new QTextEdit(editorTabs_);
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            editor->setText(file.readAll());
            file.close();
        }
        editorTabs_->addTab(editor, QFileInfo(fileName).fileName());
        editorTabs_->setCurrentWidget(editor);
    }
}

void MainWindow::handleNewWindow() {
    // Create a new detached window for chat
    QWidget* window = new QWidget(nullptr, Qt::Window);
    window->setWindowTitle(tr("RawrXD - New Chat Window"));
    window->resize(800, 600);
    
    QVBoxLayout* layout = new QVBoxLayout(window);
    QTextEdit* chat = new QTextEdit(window);
    chat->setPlaceholderText(tr("Detached chat window...\n\nIndependent chat session."));
    QLineEdit* input = new QLineEdit(window);
    input->setPlaceholderText(tr("Type your message..."));
    
    layout->addWidget(chat, 1);
    layout->addWidget(input);
    
    window->show();
}

void MainWindow::handleAddFile() {
    QString fileName = QFileDialog::getOpenFileName(this, tr("Add File to Context"));
    if (!fileName.isEmpty() && contextList_) {
        contextList_->addItem(QString("📄 %1").arg(QFileInfo(fileName).fileName()));
    }
}

void MainWindow::handleAddFolder() {
    QString dirName = QFileDialog::getExistingDirectory(this, tr("Add Folder to Context"));
    if (!dirName.isEmpty() && contextList_) {
        contextList_->addItem(QString("📁 %1").arg(QFileInfo(dirName).fileName()));
    }
}

void MainWindow::handleAddSymbol() {
    // Placeholder for symbol picker dialog
    if (contextList_) {
        contextList_->addItem(QString("🔣 Symbol (placeholder)"));
    }
}

QWidget* MainWindow::createTerminalPanel() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);
    terminalTabs_ = new QTabWidget(w);

    // PowerShell tab
    QWidget* pwshTab = new QWidget(terminalTabs_);
    QVBoxLayout* pwshLayout = new QVBoxLayout(pwshTab);
    pwshOutput_ = new QPlainTextEdit(pwshTab);
    pwshOutput_->setReadOnly(true);
    pwshOutput_->setStyleSheet("background: #012456; color: #eee; font-family: 'Consolas', monospace;");
    pwshInput_ = new QLineEdit(pwshTab);
    pwshInput_->setStyleSheet("background: #012456; color: #eee; font-family: 'Consolas', monospace;");
    pwshInput_->setPlaceholderText("PS> ");
    connect(pwshInput_, &QLineEdit::returnPressed, this, &MainWindow::handlePwshCommand);
    pwshLayout->addWidget(pwshOutput_);
    pwshLayout->addWidget(pwshInput_);
    terminalTabs_->addTab(pwshTab, tr("PowerShell"));

    // CMD tab
    QWidget* cmdTab = new QWidget(terminalTabs_);
    QVBoxLayout* cmdLayout = new QVBoxLayout(cmdTab);
    cmdOutput_ = new QPlainTextEdit(cmdTab);
    cmdOutput_->setReadOnly(true);
    cmdOutput_->setStyleSheet("background: #000; color: #fff; font-family: 'Consolas', monospace;");
    cmdInput_ = new QLineEdit(cmdTab);
    cmdInput_->setStyleSheet("background: #000; color: #fff; font-family: 'Consolas', monospace;");
    cmdInput_->setPlaceholderText("C:\\> ");
    connect(cmdInput_, &QLineEdit::returnPressed, this, &MainWindow::handleCmdCommand);
    cmdLayout->addWidget(cmdOutput_);
    cmdLayout->addWidget(cmdInput_);
    terminalTabs_->addTab(cmdTab, tr("CMD"));

    layout->addWidget(terminalTabs_);

    // Start processes
    pwshProcess_ = new QProcess(this);
    connect(pwshProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::readPwshOutput);
    connect(pwshProcess_, &QProcess::readyReadStandardError, this, &MainWindow::readPwshOutput);
    pwshProcess_->start("powershell.exe", QStringList() << "-NoExit" << "-Command" << "-");
    if (pwshOutput_) pwshOutput_->appendPlainText("PowerShell started\n");

    cmdProcess_ = new QProcess(this);
    connect(cmdProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::readCmdOutput);
    connect(cmdProcess_, &QProcess::readyReadStandardError, this, &MainWindow::readCmdOutput);
    cmdProcess_->start("cmd.exe");
    if (cmdOutput_) cmdOutput_->appendPlainText("Command Prompt started\n");

    return w;
}

void MainWindow::handlePwshCommand() {
    const QString cmd = pwshInput_ ? pwshInput_->text() : QString();
    if (cmd.isEmpty() || !pwshProcess_) return;
    pwshOutput_->appendPlainText("PS> " + cmd);
    pwshProcess_->write((cmd + "\n").toUtf8());
    pwshInput_->clear();
}

void MainWindow::handleCmdCommand() {
    const QString cmd = cmdInput_ ? cmdInput_->text() : QString();
    if (cmd.isEmpty() || !cmdProcess_) return;
    cmdOutput_->appendPlainText("C:\\> " + cmd);
    cmdProcess_->write((cmd + "\r\n").toUtf8());
    cmdInput_->clear();
}

void MainWindow::readPwshOutput() {
    const QString out = QString::fromLocal8Bit(pwshProcess_->readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(pwshProcess_->readAllStandardError());
    if (!out.isEmpty()) pwshOutput_->appendPlainText(out);
    if (!err.isEmpty()) pwshOutput_->appendPlainText("[ERROR] " + err);
}

void MainWindow::readCmdOutput() {
    const QString out = QString::fromLocal8Bit(cmdProcess_->readAllStandardOutput());
    const QString err = QString::fromLocal8Bit(cmdProcess_->readAllStandardError());
    if (!out.isEmpty()) cmdOutput_->appendPlainText(out);
    if (!err.isEmpty()) cmdOutput_->appendPlainText("[ERROR] " + err);
}

QWidget* MainWindow::createDebugPanel() {
    QWidget* w = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(w);

    QHBoxLayout* topBar = new QHBoxLayout();
    logLevelFilter_ = new QComboBox(w);
    logLevelFilter_->addItems({tr("ALL"), tr("DEBUG"), tr("INFO"), tr("WARN"), tr("ERROR")});
    connect(logLevelFilter_, &QComboBox::currentTextChanged, this, &MainWindow::filterLogLevel);

    QPushButton* clearLogBtn = new QPushButton(tr("Clear"), w);
    QPushButton* saveLogBtn = new QPushButton(tr("Save Log"), w);
    connect(clearLogBtn, &QPushButton::clicked, this, &MainWindow::clearDebugLog);
    connect(saveLogBtn, &QPushButton::clicked, this, &MainWindow::saveDebugLog);

    topBar->addWidget(new QLabel(tr("Level:"), w));
    topBar->addWidget(logLevelFilter_);
    topBar->addStretch();
    topBar->addWidget(clearLogBtn);
    topBar->addWidget(saveLogBtn);

    debugOutput_ = new QPlainTextEdit(w);
    debugOutput_->setReadOnly(true);
    debugOutput_->setStyleSheet("background: #1e1e1e; color: #d4d4d4; font-family: 'Consolas', monospace;");
    debugOutput_->setMaximumBlockCount(10000);

    layout->addLayout(topBar);
    layout->addWidget(debugOutput_);

    // Seed messages
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    debugOutput_->appendPlainText("[" + ts + "] [INFO] Debug panel initialized");
    debugOutput_->appendPlainText("[" + ts + "] [INFO] Real-time logging active");
    return w;
}

void MainWindow::clearDebugLog() {
    if (debugOutput_) debugOutput_->clear();
}

void MainWindow::saveDebugLog() {
    const QString fileName = QFileDialog::getSaveFileName(this, tr("Save Debug Log"),
                                                         "debug_log.txt",
                                                         tr("Text Files (*.txt);;All Files (*)"));
    if (!fileName.isEmpty() && debugOutput_) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << debugOutput_->toPlainText();
            file.close();
        }
    }
}

void MainWindow::filterLogLevel(const QString& level) {
    // Placeholder: Future filtering can buffer all logs and apply selection
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    if (debugOutput_) debugOutput_->appendPlainText("[" + ts + "] [INFO] Log level set to: " + level);
}

void MainWindow::showEditorContextMenu(const QPoint& pos) {
    QMenu* menu = codeView_->createStandardContextMenu();
    menu->addSeparator();
    QMenu* copilotMenu = menu->addMenu(tr("🤖 Copilot"));
    copilotMenu->addAction(tr("Explain Code"), this, &MainWindow::explainCode);
    copilotMenu->addAction(tr("Fix Issues"), this, &MainWindow::fixCode);
    copilotMenu->addAction(tr("Refactor"), this, &MainWindow::refactorCode);
    copilotMenu->addAction(tr("Generate Tests"), this, &MainWindow::generateTests);
    copilotMenu->addAction(tr("Generate Docs"), this, &MainWindow::generateDocs);
    menu->exec(codeView_->mapToGlobal(pos));
}

static QString selectedOrAllText(QTextEdit* edit) {
    QString t = edit->textCursor().selectedText();
    if (t.isEmpty()) t = edit->toPlainText();
    return t;
}

void MainWindow::explainCode() {
    const QString sel = selectedOrAllText(codeView_);
    if (qshellOutput_) qshellOutput_->append("[COPILOT] Explaining code...");
    goalInput_->setText("/explain " + sel.left(100) + "...");
    handleGoalSubmit();
}

void MainWindow::fixCode() {
    const QString sel = codeView_->textCursor().selectedText();
    if (sel.isEmpty()) return;
    if (qshellOutput_) qshellOutput_->append("[COPILOT] Analyzing code for fixes...");
    goalInput_->setText("/fix " + sel.left(100) + "...");
    handleGoalSubmit();
}

void MainWindow::refactorCode() {
    const QString sel = codeView_->textCursor().selectedText();
    if (sel.isEmpty()) return;
    if (qshellOutput_) qshellOutput_->append("[COPILOT] Refactoring code...");
    goalInput_->setText("/refactor " + sel.left(100) + "...");
    handleGoalSubmit();
}

void MainWindow::generateTests() {
    const QString sel = codeView_->textCursor().selectedText();
    if (sel.isEmpty()) return;
    if (qshellOutput_) qshellOutput_->append("[COPILOT] Generating tests...");
    goalInput_->setText("/test " + sel.left(100) + "...");
    handleGoalSubmit();
}

void MainWindow::generateDocs() {
    const QString sel = codeView_->textCursor().selectedText();
    if (sel.isEmpty()) return;
    if (qshellOutput_) qshellOutput_->append("[COPILOT] Generating documentation...");
    goalInput_->setText("/doc " + sel.left(100) + "...");
    handleGoalSubmit();
}

void MainWindow::showContextMenu(const QPoint& pos) {
    if (!contextList_) return;
    QListWidgetItem* item = contextList_->itemAt(pos);
    if (!item) return;

    QMenu* menu = new QMenu(this);
    QAction* loadAction = menu->addAction(tr("Load in Editor"));
    QAction* removeAction = menu->addAction(tr("Remove from Context"));

    connect(loadAction, &QAction::triggered, this, [this, item]() {
        loadContextItemIntoEditor(item);
    });

    connect(removeAction, &QAction::triggered, this, [this, item]() {
        delete item;
    });

    menu->exec(contextList_->mapToGlobal(pos));
}

void MainWindow::loadContextItemIntoEditor(QListWidgetItem* item) {
    if (!item || !editorTabs_) return;
    const QString text = item->text();
    const int spaceIdx = text.indexOf(' ');
    const QString fileName = (spaceIdx > 0) ? text.mid(spaceIdx + 1) : QString();

    if (text.startsWith(QString::fromUtf8("📄")) && !fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextEdit* editor = new QTextEdit(editorTabs_);
            editor->setText(QString::fromUtf8(file.readAll()));
            file.close();
            editorTabs_->addTab(editor, QFileInfo(fileName).fileName());
            editorTabs_->setCurrentWidget(editor);
        }
    } else if (text.startsWith(QString::fromUtf8("📁")) && !fileName.isEmpty()) {
        QTreeWidget* tree = new QTreeWidget(editorTabs_);
        tree->setHeaderLabel(fileName);
        populateFolderTree(tree->invisibleRootItem(), fileName);
        editorTabs_->addTab(tree, QString::fromUtf8("📁 ") + QFileInfo(fileName).fileName());
        editorTabs_->setCurrentWidget(tree);
    }
}

void MainWindow::populateFolderTree(QTreeWidgetItem* parent, const QString& path) {
    QDir dir(path);
    const QFileInfoList entries = dir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& entry : entries) {
        QTreeWidgetItem* item = new QTreeWidgetItem(parent);
        item->setText(0, entry.fileName());
        item->setData(0, Qt::UserRole, entry.absoluteFilePath());
        if (entry.isDir()) {
            item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
            populateFolderTree(item, entry.absoluteFilePath());
        } else {
            item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
        }
    }
}

void MainWindow::handleTabClose(int index) {
    if (editorTabs_ && index >= 0) {
        QWidget* widget = editorTabs_->widget(index);
        editorTabs_->removeTab(index);
        delete widget;
    }
}

/* ============================================================
   NEW IDE-WIDE SLOT STUBS
   These are placeholder implementations for all new subsystems.
   Each can be expanded as the corresponding widget is implemented.
   ============================================================ */

void MainWindow::onProjectOpened(const QString& path) { Q_UNUSED(path); }
void MainWindow::onBuildStarted() {}
void MainWindow::onBuildFinished(bool success) { Q_UNUSED(success); }
void MainWindow::onVcsStatusChanged() {}
void MainWindow::onDebuggerStateChanged(bool running) { Q_UNUSED(running); }
void MainWindow::onTestRunStarted() {}
void MainWindow::onTestRunFinished() {}
void MainWindow::onDatabaseConnected() {}
void MainWindow::onDockerContainerListed() {}
void MainWindow::onCloudResourceListed() {}
void MainWindow::onPackageInstalled(const QString& pkg) { Q_UNUSED(pkg); }
void MainWindow::onDocumentationQueried(const QString& keyword) { Q_UNUSED(keyword); }
void MainWindow::onUMLGenerated(const QString& plantUml) { Q_UNUSED(plantUml); }
void MainWindow::onImageEdited(const QString& path) { Q_UNUSED(path); }
void MainWindow::onTranslationChanged(const QString& lang) { Q_UNUSED(lang); }
void MainWindow::onDesignImported(const QString& file) { Q_UNUSED(file); }
void MainWindow::onAIChatMessage(const QString& msg) { Q_UNUSED(msg); }
void MainWindow::onNotebookExecuted() {}
void MainWindow::onMarkdownRendered() {}
void MainWindow::onSheetCalculated() {}
void MainWindow::onTerminalCommand(const QString& cmd) { Q_UNUSED(cmd); }
void MainWindow::onSnippetInserted(const QString& id) { Q_UNUSED(id); }
void MainWindow::onRegexTested(const QString& pattern) { Q_UNUSED(pattern); }
void MainWindow::onDiffMerged() {}
void MainWindow::onColorPicked(const QColor& c) { Q_UNUSED(c); }
void MainWindow::onIconSelected(const QString& name) { Q_UNUSED(name); }
void MainWindow::onPluginLoaded(const QString& name) { Q_UNUSED(name); }
void MainWindow::onSettingsSaved() {}
void MainWindow::onNotificationClicked(const QString& id) { Q_UNUSED(id); }
void MainWindow::onShortcutChanged(const QString& id, const QKeySequence& key) { Q_UNUSED(id); Q_UNUSED(key); }
void MainWindow::onTelemetryReady() {}
void MainWindow::onUpdateAvailable(const QString& version) { Q_UNUSED(version); }
void MainWindow::onWelcomeProjectChosen(const QString& path) { Q_UNUSED(path); }
void MainWindow::onCommandPaletteTriggered(const QString& cmd) { Q_UNUSED(cmd); }
void MainWindow::onProgressCancelled(const QString& taskId) { Q_UNUSED(taskId); }
void MainWindow::onQuickFixApplied(const QString& fix) { Q_UNUSED(fix); }
void MainWindow::onMinimapClicked(qreal ratio) { Q_UNUSED(ratio); }
void MainWindow::onBreadcrumbClicked(const QString& symbol) { Q_UNUSED(symbol); }
void MainWindow::onStatusFieldClicked(const QString& field) { Q_UNUSED(field); }
void MainWindow::onTerminalEmulatorCommand(const QString& cmd) { Q_UNUSED(cmd); }
void MainWindow::onSearchResultActivated(const QString& file, int line) { Q_UNUSED(file); Q_UNUSED(line); }
void MainWindow::onBookmarkToggled(const QString& file, int line) { Q_UNUSED(file); Q_UNUSED(line); }
void MainWindow::onTodoClicked(const QString& file, int line) { Q_UNUSED(file); Q_UNUSED(line); }
void MainWindow::onMacroReplayed() {}
void MainWindow::onCompletionCacheHit(const QString& key) { Q_UNUSED(key); }
void MainWindow::onLSPDiagnostic(const QString& file, const QJsonArray& diags) { Q_UNUSED(file); Q_UNUSED(diags); }
void MainWindow::onCodeLensClicked(const QString& command) { Q_UNUSED(command); }
void MainWindow::onInlayHintShown(const QString& file) { Q_UNUSED(file); }
void MainWindow::onInlineChatRequested(const QString& text) { Q_UNUSED(text); }
void MainWindow::onAIReviewComment(const QString& comment) { Q_UNUSED(comment); }
void MainWindow::onCodeStreamEdit(const QString& patch) { Q_UNUSED(patch); }
void MainWindow::onAudioCallStarted() {}
void MainWindow::onScreenShareStarted() {}
void MainWindow::onWhiteboardDraw(const QByteArray& svg) { Q_UNUSED(svg); }
void MainWindow::onTimeEntryAdded(const QString& task) { Q_UNUSED(task); }
void MainWindow::onKanbanMoved(const QString& taskId) { Q_UNUSED(taskId); }
void MainWindow::onPomodoroTick(int remaining) { Q_UNUSED(remaining); }
void MainWindow::onWallpaperChanged(const QString& path) { Q_UNUSED(path); }
void MainWindow::onAccessibilityToggled(bool on) { Q_UNUSED(on); }


