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
{
    qDebug() << "[MainWindow] Lightweight constructor - deferring all initialization";
    
    // Basic window setup only
    setWindowTitle("RawrXD Agentic IDE v5.0 - Production Ready");
    resize(1400, 900);
    
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
    
    try {
        // Create central editor (lightweight - just QTabWidget wrapper)
        m_multiTabEditor = new MultiTabEditor(this);
        m_multiTabEditor->initialize();  // Deferred widget creation
        setCentralWidget(m_multiTabEditor);
        
        // Schedule next initialization phase
        QTimer::singleShot(100, this, &MainWindow::initializePhase2);
        
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] Phase 1 error:" << e.what();
        QMessageBox::critical(this, "Initialization Error", 
            QString("Failed to initialize editor: %1").arg(e.what()));
    }
}

void MainWindow::initializePhase2()
{
    qDebug() << "[MainWindow] Phase 2: Initializing AI components";
    
    try {
        // Initialize agentic engine (deferred inference loading)
        m_agenticEngine = new AgenticEngine(this);
        m_agenticEngine->initialize();
        
        // Initialize LSP client (deferred clangd startup)
        RawrXD::LSPServerConfig config;
        config.language = "cpp";
        config.command = "clangd";
        config.arguments = QStringList{"--background-index", "--clang-tidy"};
        config.workspaceRoot = QDir::currentPath();
        config.autoStart = false;
        
        m_lspClient = new RawrXD::LSPClient(config, this);
        m_lspClient->initialize();
        
        // Initialize PlanOrchestrator (for /refactor commands)
        m_planOrchestrator = new RawrXD::PlanOrchestrator(this);
        m_planOrchestrator->initialize();
        m_planOrchestrator->setLSPClient(m_lspClient);
        
        // Schedule next phase
        QTimer::singleShot(100, this, &MainWindow::initializePhase3);
        
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] Phase 2 error:" << e.what();
        statusBar()->showMessage(QString("AI initialization warning: %1").arg(e.what()));
        // Continue anyway - IDE can work without AI
        QTimer::singleShot(100, this, &MainWindow::initializePhase3);
    }
}

void MainWindow::initializePhase3()
{
    qDebug() << "[MainWindow] Phase 3: Creating UI docks";
    
    try {
        // Create chat interface dock
        m_chatInterface = new ChatInterface(this);
        m_chatInterface->initialize();
        m_chatInterface->setAgenticEngine(m_agenticEngine);
        m_chatInterface->setPlanOrchestrator(m_planOrchestrator);
        
        m_chatDock = new QDockWidget("AI Chat & Commands", this);
        m_chatDock->setWidget(m_chatInterface);
        addDockWidget(Qt::RightDockWidgetArea, m_chatDock);
        
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
        
        // Create terminal pool dock
        m_terminalPool = new TerminalPool(3, this);
        m_terminalPool->initialize();
        
        m_terminalDock = new QDockWidget("Terminals", this);
        m_terminalDock->setWidget(m_terminalPool);
        addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
        
        // Create TODO dock
        m_todoManager = new TodoManager(this);
        m_todoDock = new TodoDock(m_todoManager, this);
        
        m_todoDockWidget = new QDockWidget("TODO List", this);
        m_todoDockWidget->setWidget(m_todoDock);
        addDockWidget(Qt::RightDockWidgetArea, m_todoDockWidget);
        
        // Schedule next phase
        QTimer::singleShot(100, this, &MainWindow::initializePhase4);
        
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] Phase 3 error:" << e.what();
        statusBar()->showMessage(QString("Dock creation error: %1").arg(e.what()));
        QTimer::singleShot(100, this, &MainWindow::initializePhase4);
    }
}

void MainWindow::initializePhase4()
{
    qDebug() << "[MainWindow] Phase 4: Creating menus and toolbars";
    
    try {
        setupMenuBar();
        setupToolBars();
        setupStatusBar();
        loadSettings();
        
        qDebug() << "[MainWindow] ✅ All phases complete - IDE ready";
        statusBar()->showMessage("Ready - Type /refactor <prompt> in chat to start", 5000);
        
    } catch (const std::exception& e) {
        qCritical() << "[MainWindow] Phase 4 error:" << e.what();
        statusBar()->showMessage("Ready (with warnings)");
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
    
    // View menu
    QMenu *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("Toggle &File Browser", this, &MainWindow::toggleFileBrowser);
    viewMenu->addAction("Toggle &Chat", this, &MainWindow::toggleChat);
    viewMenu->addAction("Toggle &Terminals", this, &MainWindow::toggleTerminals);
    viewMenu->addAction("Toggle &TODOs", this, &MainWindow::toggleTodos);
    
    // AI menu
    QMenu *aiMenu = menuBar->addMenu("&AI");
    aiMenu->addAction("Start &Chat", this, &MainWindow::startChat);
    aiMenu->addAction("&Analyze Code", this, &MainWindow::analyzeCode);
    aiMenu->addAction("&Generate Code", this, &MainWindow::generateCode);
    aiMenu->addAction("&Refactor (Multi-file)", this, &MainWindow::refactorCode);
    
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
    aiToolBar->addAction("Analyze", this, &MainWindow::analyzeCode);
    aiToolBar->addAction("Refactor", this, &MainWindow::refactorCode);
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
    // TODO: Implement undo
}

void MainWindow::redo()
{
    // TODO: Implement redo
}

void MainWindow::find()
{
    // TODO: Implement find dialog
}

void MainWindow::replace()
{
    // TODO: Implement replace dialog
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
        m_chatInterface->sendMessage("@analyze current file");
    }
}

void MainWindow::generateCode()
{
    if (m_chatInterface) {
        m_chatInterface->sendMessage("@generate ");
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

} // namespace RawrXD
