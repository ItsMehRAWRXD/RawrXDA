#include "MainWindow_v5.h"
#include "ai_digestion_panel.hpp"
#include "ai_chat_panel.hpp"
#include "ThemedCodeEditor.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QSettings>
#include <QIcon>
#include <QDockWidget>
#include <QSplitter>
#include <QTreeWidget>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QToolBar>
#include <QAction>
#include <QComboBox>
#include <QPushButton>
#include <QProgressBar>
#include <QLabel>
#include <QTextEdit>
#include <QScrollBar>
#include <QApplication>
#include <QJsonArray>
#include "agentic_executor.h"
#include "agentic_engine.h"
#include "inference_engine.h"
#include "autonomous_intelligence_orchestrator.h"

MainWindow_v5::MainWindow_v5(QWidget* parent)
    : QMainWindow(parent)
    , m_digestionPanel(nullptr)
    , m_aiChatPanel(nullptr)
    , m_codeEditor(nullptr)
    , m_agenticExecutor(nullptr)
    , m_autonomousOrchestrator(nullptr)
{
    setWindowTitle("RawrXD Agentic IDE v5.0 - Full IDE");
    setGeometry(100, 100, 1600, 900);
    
    // Initialize AgenticExecutor and Orchestrator
    m_agenticExecutor = new AgenticExecutor(this);
    m_autonomousOrchestrator = new AutonomousIntelligenceOrchestrator(this);
    
    setupUI();
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupDockWidgets();
    setupWorkflowPanel();
    
    if (m_agenticExecutor) {
        connect(m_agenticExecutor, &AgenticExecutor::workflowStatusChanged,
            this, &MainWindow_v5::onWorkflowStatusUpdate);
        connect(m_agenticExecutor, &AgenticExecutor::workflowCompleted,
            this, &MainWindow_v5::onWorkflowComplete);
        connect(m_agenticExecutor, &AgenticExecutor::logMessage,
            this, &MainWindow_v5::appendWorkflowLog);
    }
    
    applyDarkTheme();
    restoreWindowState();
    
    // Initial orchestrator startup
    m_autonomousOrchestrator->initialize(QDir::currentPath());
}

void MainWindow_v5::setupUI()
{
    // Create central tab widget for main content
    m_tabWidget = new QTabWidget(this);
    m_tabWidget->setTabsClosable(true);
    m_tabWidget->setMovable(true);
    
    // Create code editor as the main tab (using QPlainTextEdit directly)
    m_codeEditor = new QPlainTextEdit(this);
    m_codeEditor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_tabWidget->addTab(m_codeEditor, "Untitled");
    
    // Create AI digestion panel as a tab
    m_digestionPanel = new AIDigestionPanel(this);
    m_tabWidget->addTab(m_digestionPanel, "AI Digestion & Training");
    
    setCentralWidget(m_tabWidget);
}

void MainWindow_v5::setupMenuBar()
{
    // File Menu
    auto fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New File", this, &MainWindow_v5::onNewFile, QKeySequence::New);
    fileMenu->addAction("&Open File...", this, &MainWindow_v5::onOpenFile, QKeySequence::Open);
    fileMenu->addAction("&Save", this, &MainWindow_v5::onSaveFile, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &QWidget::close, QKeySequence::Quit);
    
    // Edit Menu
    auto editMenu = menuBar()->addMenu("&Edit");
    editMenu->addAction("&Undo", m_codeEditor, &QPlainTextEdit::undo, QKeySequence::Undo);
    editMenu->addAction("&Redo", m_codeEditor, &QPlainTextEdit::redo, QKeySequence::Redo);
    editMenu->addSeparator();
    editMenu->addAction("Cu&t", m_codeEditor, &QPlainTextEdit::cut, QKeySequence::Cut);
    editMenu->addAction("&Copy", m_codeEditor, &QPlainTextEdit::copy, QKeySequence::Copy);
    editMenu->addAction("&Paste", m_codeEditor, &QPlainTextEdit::paste, QKeySequence::Paste);
    
    // View Menu
    auto viewMenu = menuBar()->addMenu("&View");
    viewMenu->addAction("Show AI Chat", this, &MainWindow_v5::toggleAIChatPanel);
    viewMenu->addAction("Show File Browser", this, &MainWindow_v5::toggleFileBrowser);
    viewMenu->addAction("Show Terminal", this, &MainWindow_v5::toggleTerminal);
    
    // AI Menu
    auto aiMenu = menuBar()->addMenu("&AI");
    aiMenu->addAction("&Generate Code", this, &MainWindow_v5::generateCode);
    
    // Help Menu
    auto helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, [this]() {
        statusBar()->showMessage("RawrXD Agentic IDE v5.0 - AI-Powered Development Environment", 3000);
    });
}

void MainWindow_v5::setupToolBar()
{
    auto toolbar = addToolBar("Main Toolbar");
    toolbar->addAction("New", this, &MainWindow_v5::onNewFile);
    toolbar->addAction("Open", this, &MainWindow_v5::onOpenFile);
    toolbar->addAction("Save", this, &MainWindow_v5::onSaveFile);
    toolbar->addSeparator();
    toolbar->addAction("AI Chat", this, &MainWindow_v5::toggleAIChatPanel);
}

void MainWindow_v5::setupStatusBar()
{
    statusBar()->showMessage("Ready - AI-Powered Development Environment");
}

void MainWindow_v5::setupDockWidgets()
{
    // AI Chat Panel (right side)
    m_aiChatPanel = new AIChatPanel(this);
    QDockWidget* chatDock = new QDockWidget("AI Chat", this);
    chatDock->setWidget(m_aiChatPanel);
    chatDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, chatDock);
    
    // File Browser (left side)
    m_fileBrowser = new QTreeWidget(this);
    m_fileBrowser->setHeaderLabel("Project Files");
    QDockWidget* browserDock = new QDockWidget("Explorer", this);
    browserDock->setWidget(m_fileBrowser);
    browserDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::LeftDockWidgetArea, browserDock);
    
    // Terminal (bottom)
    m_terminal = new QPlainTextEdit(this);
    m_terminal->setReadOnly(true);
    m_terminal->setPlaceholderText("Terminal output will appear here...");
    QDockWidget* terminalDock = new QDockWidget("Terminal", this);
    terminalDock->setWidget(m_terminal);
    terminalDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, terminalDock);
}

void MainWindow_v5::setupWorkflowPanel()
{
    if (m_workflowDock) {
        return;
    }

    m_workflowDock = new QDockWidget("Agentic Workflow", this);
    QWidget* container = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(container);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(6);

    QLabel* specLabel = new QLabel("Specification", this);
    layout->addWidget(specLabel);

    m_workflowSpec = new QPlainTextEdit(this);
    m_workflowSpec->setPlaceholderText("Describe the feature, script, or tool you want to automate...");
    m_workflowSpec->setMinimumHeight(120);
    layout->addWidget(m_workflowSpec);

    layout->addWidget(new QLabel("Output Path", this));
    QHBoxLayout* pathLayout = new QHBoxLayout();
    m_workflowOutputPath = new QLineEdit(this);
    m_workflowBrowseButton = new QPushButton("Browse", this);
    pathLayout->addWidget(m_workflowOutputPath);
    pathLayout->addWidget(m_workflowBrowseButton);
    layout->addLayout(pathLayout);

    layout->addWidget(new QLabel("Compiler", this));
    m_workflowCompiler = new QComboBox(this);
    layout->addWidget(m_workflowCompiler);

    m_workflowStartButton = new QPushButton("Run Workflow", this);
    layout->addWidget(m_workflowStartButton);

    m_workflowProgress = new QProgressBar(this);
    m_workflowProgress->setRange(0, 5);
    m_workflowProgress->setValue(0);
    layout->addWidget(m_workflowProgress);

    m_workflowStatus = new QLabel("Idle", this);
    layout->addWidget(m_workflowStatus);

    m_workflowLog = new QTextEdit(this);
    m_workflowLog->setReadOnly(true);
    m_workflowLog->setFixedHeight(140);
    layout->addWidget(m_workflowLog);

    container->setLayout(layout);
    m_workflowDock->setWidget(container);
    m_workflowDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_workflowDock);

    connect(m_workflowBrowseButton, &QPushButton::clicked, this, &MainWindow_v5::onWorkflowBrowseOutput);
    connect(m_workflowStartButton, &QPushButton::clicked, this, &MainWindow_v5::onRunWorkflow);

    refreshCompilerCombo();
}

void MainWindow_v5::refreshCompilerCombo()
{
    if (!m_workflowCompiler) {
        return;
    }

    m_workflowCompiler->clear();
    m_workflowCompiler->addItem("Auto Select", "auto");

    if (m_agenticExecutor) {
        for (const QString& compiler : m_agenticExecutor->listAvailableCompilers()) {
            m_workflowCompiler->addItem(compiler, compiler);
        }
    }

    m_workflowCompiler->setCurrentIndex(0);
}

void MainWindow_v5::applyDarkTheme()
{
    // Apply VS Code dark theme
    setStyleSheet(
        "QMainWindow { background-color: #1e1e1e; }"
        "QTabWidget::pane { border: none; background-color: #1e1e1e; }"
        "QTabBar::tab { background-color: #2d2d2d; color: #cccccc; padding: 8px 16px; }"
        "QTabBar::tab:selected { background-color: #1e1e1e; border-bottom: 2px solid #007acc; }"
        "QMenuBar { background-color: #2d2d2d; color: #cccccc; }"
        "QMenuBar::item:selected { background-color: #3e3e42; }"
        "QMenu { background-color: #2d2d2d; color: #cccccc; }"
        "QMenu::item:selected { background-color: #3e3e42; }"
        "QStatusBar { background-color: #007acc; color: white; }"
        "QDockWidget { color: #cccccc; }"
        "QDockWidget::title { background-color: #2d2d2d; padding: 4px; }"
        "QTreeWidget { background-color: #252526; color: #cccccc; }"
        "QPlainTextEdit { background-color: #1e1e1e; color: #d4d4d4; font-family: 'Consolas', monospace; }"
    );
}

void MainWindow_v5::onNewFile()
{
    QPlainTextEdit* editor = new QPlainTextEdit(this);
    editor->setLineWrapMode(QPlainTextEdit::NoWrap);
    m_tabWidget->addTab(editor, "Untitled");
    m_tabWidget->setCurrentWidget(editor);
    statusBar()->showMessage("New file created", 2000);
}

void MainWindow_v5::onOpenFile()
{
    statusBar()->showMessage("Open file dialog (not implemented in v5.0)", 2000);
}

void MainWindow_v5::onSaveFile()
{
    statusBar()->showMessage("Save file (not implemented in v5.0)", 2000);
}

void MainWindow_v5::toggleAIChatPanel()
{
    // Find the AI Chat dock widget and toggle its visibility
    for (QObject* obj : findChildren<QDockWidget*>()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(obj);
        if (dock && dock->windowTitle() == "AI Chat") {
            dock->setVisible(!dock->isVisible());
            break;
        }
    }
}

void MainWindow_v5::toggleFileBrowser()
{
    for (QObject* obj : findChildren<QDockWidget*>()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(obj);
        if (dock && dock->windowTitle() == "Explorer") {
            dock->setVisible(!dock->isVisible());
            break;
        }
    }
}

void MainWindow_v5::toggleTerminal()
{
    for (QObject* obj : findChildren<QDockWidget*>()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(obj);
        if (dock && dock->windowTitle() == "Terminal") {
            dock->setVisible(!dock->isVisible());
            break;
        }
    }
}

void MainWindow_v5::generateCode()
{
    // Show dialog for code generation specification
    QString specification = QInputDialog::getMultiLineText(
        this, 
        "Generate Code", 
        "Describe the code you want to generate:", 
        "Create a React server that handles HTTP requests and serves static files"
    );
    
    if (specification.isEmpty()) return;
    
    // Get output path
    QString outputPath = QFileDialog::getSaveFileName(
        this, 
        "Save Generated Code", 
        QDir::currentPath(), 
        "C++ Files (*.cpp);;Header Files (*.h);;All Files (*.*)"
    );
    
    if (outputPath.isEmpty()) return;
    
    // Select compiler
    QStringList compilers = {"clang", "msvc", "masm"};
    QString compiler = QInputDialog::getItem(
        this, 
        "Select Compiler", 
        "Choose compiler:", 
        compilers, 
        0, 
        false
    );
    
    // Execute workflow
    if (m_agenticExecutor) {
        statusBar()->showMessage("Generating code and compiling...");
        m_terminal->appendPlainText(">>> Starting unified workflow: " + specification);
        m_terminal->appendPlainText(">>> Target: " + outputPath);
        m_terminal->appendPlainText(">>> Compiler: " + compiler);
        
        QJsonObject result = m_agenticExecutor->executeFullWorkflow(
            specification, outputPath, compiler
        );
        
        // Show compilation output in terminal
        if (result.contains("compilation")) {
            QJsonObject comp = result["compilation"].toObject();
            if (comp.contains("build_output") && !comp["build_output"].toString().isEmpty()) {
                m_terminal->appendPlainText(comp["build_output"].toString());
            }
            if (comp.contains("build_error") && !comp["build_error"].toString().isEmpty()) {
                m_terminal->appendHtml("<font color='red'>" + comp["build_error"].toString() + "</font>");
            }
        }
        
        if (result["success"].toBool()) {
            statusBar()->showMessage("✅ Workflow completed successfully!");
            m_terminal->appendPlainText(">>> SUCCESS: Workflow complete.");
            
            // Open generated file in editor
            QFile file(outputPath);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                m_codeEditor->setPlainText(file.readAll());
                file.close();
                m_tabWidget->setCurrentIndex(0);
                m_tabWidget->setTabText(0, QFileInfo(outputPath).fileName());
            }
            
            // Show completion notification
            QMessageBox::information(
                this, 
                "Workflow Complete", 
                QString("Code generation and compilation completed successfully!\n\n"
                       "Generated: %1\n"
                       "Output: %2\n"
                       "Compiler: %3")
                    .arg(result["generated_code_length"].toInt())
                    .arg(outputPath)
                    .arg(compiler)
            );
        } else {
            statusBar()->showMessage("❌ Workflow failed");
            m_terminal->appendHtml("<font color='red'>>>> ERROR: Workflow failed: " + result["error"].toString() + "</font>");
            QMessageBox::critical(this, "Workflow Failed", result["error"].toString());
        }
    } else {
        statusBar()->showMessage("❌ Agentic executor not available");
    }
}

void MainWindow_v5::closeEvent(QCloseEvent* event)
{
    saveWindowState();
    QMainWindow::closeEvent(event);
}

void MainWindow_v5::restoreWindowState()
{
    QSettings settings;
    restoreGeometry(settings.value("MainWindow/geometry", saveGeometry()).toByteArray());
    restoreState(settings.value("MainWindow/windowState", saveState()).toByteArray());
}

void MainWindow_v5::saveWindowState()
{
    QSettings settings;
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/windowState", saveState());
}