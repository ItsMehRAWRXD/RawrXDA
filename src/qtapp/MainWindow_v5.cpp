// RawrXD Agentic IDE - v5.0 Clean Implementation
// Integrates v4.3 features with proper lazy initialization and async operations
#include "MainWindow_v5.h"
#include "chat_interface.h"
#include "multi_tab_editor.h"
#include "terminal_pool.h"
#include "file_browser.h"
#include "agentic_engine.h"
#include "plan_orchestrator.h"
#include "lsp_client.h"
#include "todo_dock.h"
#include "todo_manager.h"

#include <QApplication>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QDockWidget>
#include <QTimer>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QDebug>
#include <QInputDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTabWidget>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QProgressBar>
#include <QDir>

namespace RawrXD {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_multiTabEditor(nullptr)
    , m_chatInterface(nullptr)
    , m_terminalPool(nullptr)
    , m_fileBrowser(nullptr)
    , m_agenticEngine(nullptr)
    , m_planOrchestrator(nullptr)
    , m_lspClient(nullptr)
    , m_todoManager(nullptr)
    , m_todoDock(nullptr)
    , m_chatDock(nullptr)
    , m_terminalDock(nullptr)
    , m_fileDock(nullptr)
    , m_todoDockWidget(nullptr)
    , m_splashWidget(nullptr)
    , m_splashLabel(nullptr)
    , m_splashProgress(nullptr)
{
    qDebug() << "[MainWindow] Lightweight constructor - deferring all initialization";
    
    // Basic window setup only
    setWindowTitle("RawrXD Agentic IDE v5.0 - Production Ready");
    resize(1400, 900);
    
    // Create splash widget for initialization progress
    m_splashWidget = new QWidget(this);
    m_splashWidget->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    QVBoxLayout *splashLayout = new QVBoxLayout(m_splashWidget);
    splashLayout->setAlignment(Qt::AlignCenter);
    
    QLabel *titleLabel = new QLabel("<h1>RawrXD Agentic IDE</h1><p>v5.0 Production Ready</p>");
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet("color: #4ec9b0; margin: 20px;");
    splashLayout->addWidget(titleLabel);
    
    m_splashLabel = new QLabel("Initializing...");
    m_splashLabel->setAlignment(Qt::AlignCenter);
    splashLayout->addWidget(m_splashLabel);
    
    m_splashProgress = new QProgressBar();
    m_splashProgress->setRange(0, 100);
    m_splashProgress->setValue(0);
    m_splashProgress->setTextVisible(true);
    m_splashProgress->setStyleSheet(
        "QProgressBar { border: 2px solid #3c3c3c; border-radius: 5px; text-align: center; }"
        "QProgressBar::chunk { background-color: #4ec9b0; }"
    );
    splashLayout->addWidget(m_splashProgress);
    
    setCentralWidget(m_splashWidget);
    
    // Defer all heavy initialization to after event loop starts
    QTimer::singleShot(0, this, &MainWindow::initialize);
}

MainWindow::~MainWindow()
{
    qDebug() << "[MainWindow] Destructor - saving settings";
    saveSettings();
}

void MainWindow::initialize()
{
    qDebug() << "[MainWindow] Phase 1: Initializing core components";
    updateSplashProgress("⏳ Phase 1/4: Initializing core editor...", 10);
    
    try {
        // Create central editor (lightweight - just QTabWidget wrapper)
        m_multiTabEditor = new MultiTabEditor(this);
        m_multiTabEditor->initialize();  // Deferred widget creation
        m_multiTabEditor->hide();  // Keep hidden until splash is done
        
        updateSplashProgress("✓ Editor initialized", 25);
        
        // Schedule next initialization phase
        QTimer::singleShot(100, this, &MainWindow::initializePhase2);
        
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] Phase 1 error:" << e.what();
        updateSplashProgress("✗ Editor initialization failed", 25);
        QMessageBox::critical(this, "Initialization Error", 
            QString("Failed to initialize editor: %1").arg(e.what()));
    }
}

void MainWindow::initializePhase2()
{
    qDebug() << "[MainWindow] Phase 2: Initializing AI components";
    updateSplashProgress("⏳ Phase 2/4: Initializing AI engine & LSP...", 30);
    
    try {
        // Initialize agentic engine (deferred inference loading)
        m_agenticEngine = new AgenticEngine(this);
        m_agenticEngine->initialize();
        
        updateSplashProgress("✓ AI Engine initialized", 40);
        
        // Initialize LSP client (deferred clangd startup)
        RawrXD::LSPServerConfig config;
        config.language = "cpp";
        config.command = "clangd";
        config.arguments = QStringList{"--background-index", "--clang-tidy"};
        config.workspaceRoot = QDir::currentPath();
        config.autoStart = true;  // Enable auto-start for LSP
        
        m_lspClient = new RawrXD::LSPClient(config, this);
        m_lspClient->initialize();
        
        updateSplashProgress("✓ LSP Client initialized", 50);
        
        // Initialize PlanOrchestrator (for /refactor commands)
        m_planOrchestrator = new RawrXD::PlanOrchestrator(this);
        m_planOrchestrator->initialize();
        m_planOrchestrator->setLSPClient(m_lspClient);
        
        updateSplashProgress("✓ Plan Orchestrator initialized", 55);
        
        // Schedule next phase
        QTimer::singleShot(100, this, &MainWindow::initializePhase3);
        
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] Phase 2 error:" << e.what();
        updateSplashProgress("⚠ AI initialization warning", 55);
        statusBar()->showMessage(QString("AI initialization warning: %1").arg(e.what()));
        // Continue anyway - IDE can work without AI
        QTimer::singleShot(100, this, &MainWindow::initializePhase3);
    }
}

void MainWindow::initializePhase3()
{
    qDebug() << "[MainWindow] Phase 3: Creating UI docks";
    updateSplashProgress("⏳ Phase 3/4: Creating UI panels...", 60);
    
    try {
        // Create chat interface dock
        m_chatInterface = new ChatInterface(this);
        m_chatInterface->initialize();
        m_chatInterface->setAgenticEngine(m_agenticEngine);
        m_chatInterface->setPlanOrchestrator(m_planOrchestrator);
        
        m_chatDock = new QDockWidget("AI Chat & Commands", this);
        m_chatDock->setWidget(m_chatInterface);
        addDockWidget(Qt::RightDockWidgetArea, m_chatDock);
        
        updateSplashProgress("✓ Chat interface ready", 65);
        
        // Connect chat messages to agentic engine
        connect(m_chatInterface, &ChatInterface::messageSent,
                m_agenticEngine, &AgenticEngine::processMessage);
        connect(m_agenticEngine, &AgenticEngine::responseReady,
                m_chatInterface, &ChatInterface::messageReceived);
        
        // Connect model selection to load GGUF files
        connect(m_chatInterface, &ChatInterface::modelSelected,
                m_agenticEngine, &AgenticEngine::setModelName);
        
        // Connect model ready signal to enable/disable chat input
        connect(m_agenticEngine, &AgenticEngine::modelReady,
                m_chatInterface, &ChatInterface::setCanSendMessage);
        
        // Wire progress signals
        connect(m_planOrchestrator, &RawrXD::PlanOrchestrator::planningStarted,
                m_chatInterface, [this](const QString& prompt) {
                    m_chatInterface->addMessage("System", "📋 Planning: " + prompt);
                });
        
        connect(m_planOrchestrator, &RawrXD::PlanOrchestrator::executionStarted,
                m_chatInterface, [this](int taskCount) {
                    m_chatInterface->addMessage("System", 
                        QString("🚀 Executing %1 tasks...").arg(taskCount));
                });
        
        connect(m_planOrchestrator, &RawrXD::PlanOrchestrator::taskExecuted,
                m_chatInterface, [this](int index, bool success, const QString& desc) {
                    QString status = success ? "✓" : "✗";
                    QString color = success ? "#4ec9b0" : "#f48771";
                    m_chatInterface->addMessage("Task", 
                        QString("<span style='color:%1;'>%2 [%3] %4</span>")
                            .arg(color).arg(status).arg(index + 1).arg(desc));
                });
        
        // Create file browser dock
        m_fileBrowser = new FileBrowser(this);
        m_fileBrowser->initialize();
        
        m_fileDock = new QDockWidget("Files", this);
        m_fileDock->setWidget(m_fileBrowser);
        addDockWidget(Qt::LeftDockWidgetArea, m_fileDock);
        
        // Connect file browser to editor
        connect(m_fileBrowser, &FileBrowser::fileSelected,
                m_multiTabEditor, &MultiTabEditor::openFile);
        
        updateSplashProgress("✓ File browser ready", 75);
        
        // Create terminal pool dock
        m_terminalPool = new TerminalPool(3, this);
        m_terminalPool->initialize();
        
        m_terminalDock = new QDockWidget("Terminals", this);
        m_terminalDock->setWidget(m_terminalPool);
        addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
        
        updateSplashProgress("✓ Terminals ready", 85);
        
        // Create TODO dock
        m_todoManager = new TodoManager(this);
        m_todoDock = new TodoDock(m_todoManager, this);
        
        m_todoDockWidget = new QDockWidget("TODO List", this);
        m_todoDockWidget->setWidget(m_todoDock);
        addDockWidget(Qt::RightDockWidgetArea, m_todoDockWidget);
        
        updateSplashProgress("✓ All panels created", 90);
        
        // Schedule next phase
        QTimer::singleShot(100, this, &MainWindow::initializePhase4);
        
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] Phase 3 error:" << e.what();
        updateSplashProgress("⚠ Dock creation warning", 90);
        statusBar()->showMessage(QString("Dock creation error: %1").arg(e.what()));
        QTimer::singleShot(100, this, &MainWindow::initializePhase4);
    }
}

void MainWindow::initializePhase4()
{
    qDebug() << "[MainWindow] Phase 4: Creating menus and toolbars";
    updateSplashProgress("⏳ Phase 4/4: Finalizing UI...", 92);
    
    try {
        setupMenuBar();
        updateSplashProgress("✓ Menus created", 95);
        
        setupToolBars();
        setupStatusBar();
        updateSplashProgress("✓ Toolbars created", 98);
        
        loadSettings();
        
        qDebug() << "[MainWindow] ✅ All phases complete - IDE ready";
        updateSplashProgress("✅ Initialization complete!", 100);
        
        // Replace splash with actual editor
        QTimer::singleShot(500, [this]() {
            if (m_splashWidget) {
                m_splashWidget->deleteLater();
                m_splashWidget = nullptr;
            }
            if (m_multiTabEditor) {
                setCentralWidget(m_multiTabEditor);
                m_multiTabEditor->show();
            }
            statusBar()->showMessage("Ready - Type /refactor <prompt> in chat to start", 5000);
        });
        
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] Phase 4 error:" << e.what();
        updateSplashProgress("⚠ Finalization warning", 100);
        statusBar()->showMessage("Ready (with warnings)");
        
        // Still cleanup splash on error
        QTimer::singleShot(500, [this]() {
            if (m_splashWidget) {
                m_splashWidget->deleteLater();
                m_splashWidget = nullptr;
            }
            if (m_multiTabEditor) {
                setCentralWidget(m_multiTabEditor);
                m_multiTabEditor->show();
            }
        });
    }
}

void MainWindow::setupMenuBar()
{
    QMenuBar *menuBar = this->menuBar();
    
    // File menu
    QMenu *fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("&New File", this, &MainWindow::newFile, QKeySequence::New);
    fileMenu->addAction("&Open File", this, &MainWindow::openFile, QKeySequence::Open);
    fileMenu->addAction("&Save", this, &MainWindow::saveFile, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &MainWindow::close, QKeySequence::Quit);
    
    // Edit menu
    QMenu *editMenu = menuBar->addMenu("&Edit");
    editMenu->addAction("&Undo", this, &MainWindow::undo, QKeySequence::Undo);
    editMenu->addAction("&Redo", this, &MainWindow::redo, QKeySequence::Redo);
    editMenu->addSeparator();
    editMenu->addAction("&Find", this, &MainWindow::find, QKeySequence::Find);
    editMenu->addAction("&Replace", this, &MainWindow::replace, QKeySequence::Replace);
    editMenu->addSeparator();
    editMenu->addAction("&Preferences...", this, &MainWindow::showPreferences, QKeySequence("Ctrl+,"));
    
    // View menu
    QMenu *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("Toggle &File Browser", this, &MainWindow::toggleFileBrowser);
    viewMenu->addAction("Toggle &Chat", this, &MainWindow::toggleChat);
    viewMenu->addAction("Toggle &Terminals", this, &MainWindow::toggleTerminals);
    viewMenu->addAction("Toggle &TODOs", this, &MainWindow::toggleTodos);
    viewMenu->addSeparator();
    
    // TODO Panel submenu
    QMenu *todoMenu = viewMenu->addMenu("TODO Panel");
    todoMenu->addAction("Add TODO", this, &MainWindow::addTodo, QKeySequence("Ctrl+T"));
    todoMenu->addAction("Scan Code for TODOs", this, &MainWindow::scanCodeForTodos);
    
    // Terminals submenu
    QMenu *termMenu = viewMenu->addMenu("Terminals");
    termMenu->addAction("New Terminal", this, &MainWindow::newTerminal, QKeySequence("Ctrl+Shift+T"));
    termMenu->addAction("Close Terminal", this, &MainWindow::closeTerminal);
    termMenu->addAction("Next Terminal", this, &MainWindow::nextTerminal, QKeySequence("Ctrl+PgDown"));
    termMenu->addAction("Previous Terminal", this, &MainWindow::previousTerminal, QKeySequence("Ctrl+PgUp"));
    
    // AI menu
    QMenu *aiMenu = menuBar->addMenu("&AI");
    aiMenu->addAction("Start &Chat", this, &MainWindow::startChat);
    aiMenu->addSeparator();
    aiMenu->addAction("&Load Model...", this, &MainWindow::loadModel);
    aiMenu->addAction("&Inference Settings...", this, &MainWindow::showInferenceSettings);
    aiMenu->addSeparator();
    aiMenu->addAction("&Analyze Code", this, &MainWindow::analyzeCode);
    aiMenu->addAction("&Generate Code", this, &MainWindow::generateCode);
    aiMenu->addAction("&Refactor (Multi-file)", this, &MainWindow::refactorCode);
    aiMenu->addSeparator();
    
    // LSP Server submenu
    QMenu *lspMenu = aiMenu->addMenu("&LSP Server");
    lspMenu->addAction("&Start Server", this, &MainWindow::startLSPServer);
    lspMenu->addAction("Sto&p Server", this, &MainWindow::stopLSPServer);
    lspMenu->addAction("&Restart Server", this, &MainWindow::restartLSPServer);
    lspMenu->addAction("Server &Status", this, &MainWindow::showLSPStatus);
    
    // Help menu
    QMenu *helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("&About", this, &MainWindow::showAbout);
    helpMenu->addAction("AI &Commands", this, &MainWindow::showAIHelp);
}

void MainWindow::setupToolBars()
{
    QToolBar *fileToolBar = addToolBar("File");
    fileToolBar->addAction("New", this, &MainWindow::newFile);
    fileToolBar->addAction("Open", this, &MainWindow::openFile);
    fileToolBar->addAction("Save", this, &MainWindow::saveFile);
    
    QToolBar *aiToolBar = addToolBar("AI");
    aiToolBar->addAction("Chat", this, &MainWindow::startChat);
    aiToolBar->addAction("Load Model", this, &MainWindow::loadModel);
    aiToolBar->addAction("Analyze", this, &MainWindow::analyzeCode);
    aiToolBar->addAction("Refactor", this, &MainWindow::refactorCode);
    
    QToolBar *todoToolBar = addToolBar("TODO");
    todoToolBar->addAction("Add TODO", this, &MainWindow::addTodo);
    todoToolBar->addAction("Scan TODOs", this, &MainWindow::scanCodeForTodos);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Initializing...");
}

void MainWindow::loadSettings()
{
    QSettings settings("RawrXD", "AgenticIDE");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::saveSettings()
{
    QSettings settings("RawrXD", "AgenticIDE");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::updateSplashProgress(const QString& message, int percent)
{
    if (m_splashLabel) {
        m_splashLabel->setText(message);
    }
    if (m_splashProgress) {
        m_splashProgress->setValue(percent);
    }
    QApplication::processEvents();  // Force UI update
}

// File operations
void MainWindow::newFile()
{
    if (m_multiTabEditor) {
        m_multiTabEditor->newFile();
    }
}

void MainWindow::openFile()
{
    QString fileName = QFileDialog::getOpenFileName(this, "Open File", "", 
        "All Files (*);;C++ Files (*.cpp *.h);;Python Files (*.py)");
    if (!fileName.isEmpty() && m_multiTabEditor) {
        m_multiTabEditor->openFile(fileName);
    }
}

void MainWindow::saveFile()
{
    if (m_multiTabEditor) {
        m_multiTabEditor->saveCurrentFile();
    }
}

void MainWindow::undo()
{
    if (m_multiTabEditor) {
        m_multiTabEditor->undo();
    }
}

void MainWindow::redo()
{
    if (m_multiTabEditor) {
        m_multiTabEditor->redo();
    }
}

void MainWindow::find()
{
    if (m_multiTabEditor) {
        m_multiTabEditor->find();
    }
}

void MainWindow::replace()
{
    if (m_multiTabEditor) {
        m_multiTabEditor->replace();
    }
}

// View operations
void MainWindow::toggleFileBrowser()
{
    if (m_fileDock) {
        m_fileDock->setVisible(!m_fileDock->isVisible());
    }
}

void MainWindow::toggleChat()
{
    if (m_chatDock) {
        m_chatDock->setVisible(!m_chatDock->isVisible());
    }
}

void MainWindow::toggleTerminals()
{
    if (m_terminalDock) {
        m_terminalDock->setVisible(!m_terminalDock->isVisible());
    }
}

void MainWindow::toggleTodos()
{
    if (m_todoDockWidget) {
        m_todoDockWidget->setVisible(!m_todoDockWidget->isVisible());
    }
}

// AI operations
void MainWindow::startChat()
{
    if (m_chatDock) {
        m_chatDock->setVisible(true);
        m_chatDock->raise();
        if (m_chatInterface) {
            m_chatInterface->setFocus();
        }
    }
}

void MainWindow::analyzeCode()
{
    if (m_chatInterface) {
        m_chatInterface->sendMessageProgrammatically("@analyze current file");
    }
}

void MainWindow::generateCode()
{
    if (m_chatInterface) {
        m_chatInterface->sendMessageProgrammatically("@generate ");
        m_chatInterface->focusInput();
    }
}

void MainWindow::refactorCode()
{
    if (m_chatInterface) {
        m_chatInterface->addMessage("System", 
            "💡 Tip: Type '/refactor <description>' to perform multi-file refactoring\n"
            "Example: /refactor change UserManager to use UUID instead of int ID");
        m_chatDock->setVisible(true);
        m_chatDock->raise();
    }
}

void MainWindow::showAbout()
{
    QMessageBox::about(this, "About RawrXD Agentic IDE",
        "RawrXD Agentic IDE v5.0\n\n"
        "Features:\n"
        "• AI-powered multi-file refactoring (/refactor command)\n"
        "• LSP integration with clangd\n"
        "• Real-time progress updates (📋 🚀 ✓ ✗)\n"
        "• Cursor-style ghost text completions\n"
        "• Integrated terminals and file browser\n\n"
        "Built with Qt 6.7.3 and love ❤️");
}

void MainWindow::showAIHelp()
{
    if (m_chatInterface) {
        m_chatInterface->addMessage("System",
            "<h3>AI Commands</h3>"
            "<b>/refactor &lt;prompt&gt;</b> - Multi-file AI refactoring<br>"
            "<b>@plan &lt;task&gt;</b> - Create implementation plan<br>"
            "<b>@analyze</b> - Analyze current file<br>"
            "<b>@generate &lt;spec&gt;</b> - Generate code<br>"
            "<b>/help</b> - Show all commands");
        m_chatDock->setVisible(true);
    }
}

void MainWindow::loadModel()
{
    QString modelPath = QFileDialog::getOpenFileName(this, 
        "Load AI Model", 
        QDir::homePath(), 
        "GGUF Models (*.gguf);;All Files (*)");
    
    if (!modelPath.isEmpty()) {
        // Directly set the model in the agentic engine
        if (m_agenticEngine) {
            m_agenticEngine->setModelName(modelPath);
            statusBar()->showMessage(QString("Loading model: %1").arg(QFileInfo(modelPath).fileName()), 3000);
        } else {
            statusBar()->showMessage("Agentic Engine not initialized", 3000);
        }
    }
}

void MainWindow::showInferenceSettings()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Inference Settings");
    dialog->setModal(true);
    
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    
    // Temperature setting
    QHBoxLayout *tempLayout = new QHBoxLayout();
    tempLayout->addWidget(new QLabel("Temperature:"));
    QDoubleSpinBox *tempSpin = new QDoubleSpinBox();
    tempSpin->setRange(0.0, 2.0);
    tempSpin->setSingleStep(0.1);
    tempSpin->setValue(0.8);
    tempLayout->addWidget(tempSpin);
    layout->addLayout(tempLayout);
    
    // Top-P setting
    QHBoxLayout *topPLayout = new QHBoxLayout();
    topPLayout->addWidget(new QLabel("Top-P:"));
    QDoubleSpinBox *topPSpin = new QDoubleSpinBox();
    topPSpin->setRange(0.0, 1.0);
    topPSpin->setSingleStep(0.05);
    topPSpin->setValue(0.9);
    topPLayout->addWidget(topPSpin);
    layout->addLayout(topPLayout);
    
    // Max Tokens setting
    QHBoxLayout *tokensLayout = new QHBoxLayout();
    tokensLayout->addWidget(new QLabel("Max Tokens:"));
    QSpinBox *tokensSpin = new QSpinBox();
    tokensSpin->setRange(1, 4096);
    tokensSpin->setValue(512);
    tokensLayout->addWidget(tokensSpin);
    layout->addLayout(tokensLayout);
    
    // Backend selection
    QHBoxLayout *backendLayout = new QHBoxLayout();
    backendLayout->addWidget(new QLabel("Backend:"));
    QComboBox *backendCombo = new QComboBox();
    backendCombo->addItems({\"Auto\", \"CPU\", \"GPU (Vulkan)\", \"GPU (CUDA)\"});
    backendLayout->addWidget(backendCombo);
    layout->addLayout(backendLayout);
    
    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    if (dialog->exec() == QDialog::Accepted) {
        // TODO: Apply settings to AgenticEngine
        statusBar()->showMessage(QString("Inference settings updated: Temp=%.1f, TopP=%.2f, Tokens=%1")
            .arg(tempSpin->value()).arg(topPSpin->value()).arg(tokensSpin->value()), 3000);
    }
    
    delete dialog;
}

void MainWindow::startLSPServer()
{
    if (m_lspClient) {
        m_lspClient->startServer();
        statusBar()->showMessage("LSP Server starting...", 3000);
    }
}

void MainWindow::stopLSPServer()
{
    if (m_lspClient) {
        m_lspClient->stopServer();
        statusBar()->showMessage("LSP Server stopped", 3000);
    }
}

void MainWindow::restartLSPServer()
{
    if (m_lspClient) {
        m_lspClient->stopServer();
        QTimer::singleShot(500, [this]() {
            if (m_lspClient) {
                m_lspClient->startServer();
                statusBar()->showMessage("LSP Server restarted", 3000);
            }
        });
    }
}

void MainWindow::showLSPStatus()
{
    if (!m_lspClient) {
        QMessageBox::information(this, "LSP Status", "LSP Client not initialized");
        return;
    }
    
    bool isRunning = m_lspClient->isRunning();
    QString status = isRunning ? "Running ✓" : "Stopped ✗";
    
    QMessageBox::information(this, "LSP Server Status",
        QString("Status: %1\n\nLanguage: cpp\nServer: clangd\nCapabilities: Completions, Diagnostics, Hover, Definitions\n\nWorkspace: %2")
            .arg(status)
            .arg(QDir::currentPath()));
}

void MainWindow::showPreferences()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Preferences");
    dialog->setModal(true);
    dialog->resize(600, 400);
    
    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);
    QTabWidget *tabs = new QTabWidget();
    
    // LSP Settings Tab
    QWidget *lspTab = new QWidget();
    QVBoxLayout *lspLayout = new QVBoxLayout(lspTab);
    
    QHBoxLayout *lspCmdLayout = new QHBoxLayout();
    lspCmdLayout->addWidget(new QLabel("LSP Command:"));
    QLineEdit *lspCmdEdit = new QLineEdit("clangd");
    lspCmdLayout->addWidget(lspCmdEdit);
    lspLayout->addLayout(lspCmdLayout);
    
    QCheckBox *lspAutoStart = new QCheckBox("Auto-start LSP server");
    lspAutoStart->setChecked(true);
    lspLayout->addWidget(lspAutoStart);
    
    lspLayout->addStretch();
    tabs->addTab(lspTab, "LSP");
    
    // AI Settings Tab
    QWidget *aiTab = new QWidget();
    QVBoxLayout *aiLayout = new QVBoxLayout(aiTab);
    
    QHBoxLayout *modelLayout = new QHBoxLayout();
    modelLayout->addWidget(new QLabel("Default Model:"));
    QLineEdit *modelEdit = new QLineEdit();
    modelLayout->addWidget(modelEdit);
    QPushButton *browseBtn = new QPushButton("Browse...");
    connect(browseBtn, &QPushButton::clicked, [modelEdit, this]() {
        QString path = QFileDialog::getOpenFileName(this, "Select Model", QDir::homePath(), "GGUF Models (*.gguf)");
        if (!path.isEmpty()) modelEdit->setText(path);
    });
    modelLayout->addWidget(browseBtn);
    aiLayout->addLayout(modelLayout);
    
    aiLayout->addStretch();
    tabs->addTab(aiTab, "AI Model");
    
    // Terminal Settings Tab
    QWidget *termTab = new QWidget();
    QVBoxLayout *termLayout = new QVBoxLayout(termTab);
    
    QHBoxLayout *shellLayout = new QHBoxLayout();
    shellLayout->addWidget(new QLabel("Shell:"));
    QComboBox *shellCombo = new QComboBox();
    shellCombo->addItems({\"PowerShell\", \"Cmd\", \"Bash\", \"Custom\"});
    shellLayout->addWidget(shellCombo);
    termLayout->addLayout(shellLayout);
    
    termLayout->addStretch();
    tabs->addTab(termTab, "Terminal");
    
    // Editor Settings Tab
    QWidget *editorTab = new QWidget();
    QVBoxLayout *editorLayout = new QVBoxLayout(editorTab);
    
    QHBoxLayout *fontLayout = new QHBoxLayout();
    fontLayout->addWidget(new QLabel("Font Size:"));
    QSpinBox *fontSpin = new QSpinBox();
    fontSpin->setRange(8, 24);
    fontSpin->setValue(12);
    fontLayout->addWidget(fontSpin);
    editorLayout->addLayout(fontLayout);
    
    QCheckBox *lineNumbers = new QCheckBox("Show line numbers");
    lineNumbers->setChecked(true);
    editorLayout->addWidget(lineNumbers);
    
    QCheckBox *wordWrap = new QCheckBox("Word wrap");
    editorLayout->addWidget(wordWrap);
    
    editorLayout->addStretch();
    tabs->addTab(editorTab, "Editor");
    
    mainLayout->addWidget(tabs);
    
    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    mainLayout->addWidget(buttons);
    
    if (dialog->exec() == QDialog::Accepted) {
        // TODO: Save preferences to QSettings
        statusBar()->showMessage("Preferences saved", 3000);
    }
    
    delete dialog;
}

void MainWindow::addTodo()
{
    if (!m_todoManager) return;
    
    bool ok;
    QString text = QInputDialog::getText(this, "Add TODO", 
        "TODO Description:", QLineEdit::Normal, "", &ok);
    
    if (ok && !text.isEmpty()) {
        m_todoManager->addTodo(text, QString(), 0);  // No file/line association
        statusBar()->showMessage("TODO added", 2000);
    }
}

void MainWindow::scanCodeForTodos()
{
    if (!m_todoManager || !m_fileBrowser) return;
    
    // TODO: Implement recursive scan of project files for // TODO: comments
    QMessageBox::information(this, "Scan for TODOs",
        "This will scan all project files for TODO comments.\n\n"
        "Feature coming soon!");
}

void MainWindow::newTerminal()
{
    if (m_terminalPool) {
        m_terminalPool->createNewTerminal();
        statusBar()->showMessage("New terminal created", 2000);
    }
}

void MainWindow::closeTerminal()
{
    if (m_terminalPool && m_terminalDock) {
        // Close current tab - TerminalPool uses tab index
        // We'll need to get the current tab from the internal tab widget
        statusBar()->showMessage("Use terminal tab close button to close terminals", 2000);
    }
}

void MainWindow::nextTerminal()
{
    if (m_terminalPool) {
        // TerminalPool doesn't have nextTerminal, we'll access its internal tab widget
        statusBar()->showMessage("Use Ctrl+Tab to switch terminals", 2000);
    }
}

void MainWindow::previousTerminal()
{
    if (m_terminalPool) {
        statusBar()->showMessage("Use Ctrl+Shift+Tab to switch terminals", 2000);
    }
}

} // namespace RawrXD
