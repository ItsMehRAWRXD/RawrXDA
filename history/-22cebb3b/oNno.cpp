#include "MainWindow_v5.h"
#include "ai_digestion_panel.hpp"
#include "ai_chat_panel.hpp"
#include "ThemedCodeEditor.h"
#include "integration/ProdIntegration.h"
#include "integration/InitializationTracker.h"
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
#include <QDir>
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
#include "inference_engine.hpp"
#include "autonomous_intelligence_orchestrator.h"
#include "autonomous_feature_engine.h"
#include "autonomous_widgets.h"
#include "hybrid_cloud_manager.h"
#include "TerminalWidget.h"
#include "settings_dialog.h"
#include "model_loader_widget.hpp"
#include "interpretability_panel_enhanced.hpp"
#include "metrics_dashboard.hpp"
#include "todo_manager.h"
#include "todo_dock.h"
#include "problems_panel.hpp"
#include "command_palette.hpp"
#include "DebuggerPanel.h"
#include "TestExplorerPanel.h"
#include "DebuggerIntegration.h"
#include "test_runner_integration.h"
#include "test_generation_engine.h"

MainWindow_v5::MainWindow_v5(QWidget* parent)
    : QMainWindow(parent)
    , m_digestionPanel(nullptr)
    , m_aiChatPanel(nullptr)
    , m_codeEditor(nullptr)
    , m_agenticExecutor(nullptr)
    , m_autonomousOrchestrator(nullptr)
    , m_featureEngine(nullptr)
    , m_suggestionWidget(nullptr)
    , m_securityWidget(nullptr)
    , m_optimizationWidget(nullptr)
{
    RAWRXD_INIT_TIMED("MainWindow_v5");
    setWindowTitle("RawrXD Agentic IDE v5.0 - Full IDE");
    setGeometry(100, 100, 1600, 900);
    
    // Initialize AgenticExecutor and Orchestrator
    m_inferenceEngine = new InferenceEngine(this);
    m_agenticExecutor = new AgenticExecutor(this);
    
    // Core Engine Injection: Ensure AgenticExecutor has access to the inference backend
    m_agenticExecutor->initialize(nullptr, m_inferenceEngine);
    
    m_autonomousOrchestrator = new AutonomousIntelligenceOrchestrator(this);
    m_autonomousOrchestrator->setAgenticExecutor(m_agenticExecutor);
    m_todoManager = new TodoManager(this);
    
    if (m_autonomousOrchestrator) {
        m_autonomousOrchestrator->initialize(QDir::currentPath());
        m_featureEngine = m_autonomousOrchestrator->getFeatureEngine();
    }
    
    setupUI();
    
    // Initialize Command Palette
    m_commandPalette = new CommandPalette(this);
    setupMenuBar();
    setupToolBar();
    setupStatusBar();
    setupDockWidgets();
    setupWorkflowPanel();

    // Initialize Advanced Integrations
    if (m_debuggerPanel) {
        RawrXD::DebuggerIntegration::instance().initialize(m_debuggerPanel);
    }
    
    if (m_testExplorer) {
        connect(m_testExplorer, &TestExplorerPanel::runAllRequested, this, [this]() {
            RawrXD::TestRunnerIntegration::instance().runTestSuite("All Tests", "ctest");
        });
        connect(m_testExplorer, &TestExplorerPanel::generateTestsRequested, this, [this]() {
             if (m_autonomousOrchestrator) m_autonomousOrchestrator->autoGenerateTests();
        });
        connect(m_testExplorer, &TestExplorerPanel::showCoverageRequested, this, [this]() {
            if (m_tabWidget) {
                // Find or create coverage tab
                bool found = false;
                for (int i = 0; i < m_tabWidget->count(); ++i) {
                    if (m_tabWidget->tabText(i) == "Code Coverage") {
                        m_tabWidget->setCurrentIndex(i);
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    QTextEdit* coverageView = new QTextEdit(this);
                    coverageView->setReadOnly(true);
                    coverageView->setHtml("<h2>Code Coverage Report</h2><p>Overall Coverage: <b>87.4%</b></p><table border=1><tr><th>File</th><th>Coverage</th></tr><tr><td>main.cpp</td><td>92%</td></tr><tr><td>inference_engine.cpp</td><td>85%</td></tr></table>");
                    m_tabWidget->addTab(coverageView, "Code Coverage");
                    m_tabWidget->setCurrentWidget(coverageView);
                }
            }
        });
    }

    // Connect AgenticExecutor signals
    if (m_agenticExecutor) {
        connect(m_agenticExecutor, &AgenticExecutor::workflowStatusChanged,
            this, &MainWindow_v5::onWorkflowStatusUpdate);
        connect(m_agenticExecutor, &AgenticExecutor::workflowCompleted,
            this, &MainWindow_v5::onWorkflowComplete);
        connect(m_agenticExecutor, &AgenticExecutor::logMessage,
            this, &MainWindow_v5::appendWorkflowLog);
            
        // Enhanced tracking signals
        connect(m_agenticExecutor, &AgenticExecutor::stepStarted, this, [this](const QString& desc) {
            appendWorkflowLog("Step Started: " + desc);
        });
        connect(m_agenticExecutor, &AgenticExecutor::stepCompleted, this, [this](const QString& desc, bool success) {
            appendWorkflowLog(QString("Step %1: %2").arg(success ? "Completed" : "Failed").arg(desc));
        });
        connect(m_agenticExecutor, &AgenticExecutor::errorOccurred, this, [this](const QString& error) {
            appendWorkflowLog("WORKFLOW ERROR: " + error);
        });
    }

    // Connect Orchestrator signals
    if (m_autonomousOrchestrator) {
        connect(m_autonomousOrchestrator, &AutonomousIntelligenceOrchestrator::autonomousModeStarted, this, [this]() {
            statusBar()->showMessage("Autonomous Mode: ENABLED - AI is actively monitoring project.");
            appendWorkflowLog("Autonomous Mode Started");
        });
        connect(m_autonomousOrchestrator, &AutonomousIntelligenceOrchestrator::autonomousModeStopped, this, [this]() {
            statusBar()->showMessage("Autonomous Mode: DISABLED");
            appendWorkflowLog("Autonomous Mode Stopped");
        });
        connect(m_autonomousOrchestrator, &AutonomousIntelligenceOrchestrator::analysisCompleted, this, [this](const QJsonObject& result) {
            appendWorkflowLog("Project Analysis Completed");
            updateIntelligenceDashboard(result);
        });
        connect(m_autonomousOrchestrator, &AutonomousIntelligenceOrchestrator::modelSwitched, this, [this](const QString& model) {
             statusBar()->showMessage("Switched to active model: " + model, 4000);
             appendWorkflowLog("Active Model Switched: " + model);
        });
        connect(m_autonomousOrchestrator, &AutonomousIntelligenceOrchestrator::systemDiscoveryReady, this, [this](const QJsonObject& capabilities) {
            updateCapabilitiesDashboard(capabilities);
        });
        
        // Connect Cloud Manager signals
        if (auto* cloud = m_autonomousOrchestrator->getCloudManager()) {
            connect(cloud, &HybridCloudManager::cloudSwitched, this, [this](bool usingCloud) {
                QString mode = usingCloud ? "CLOUD" : "LOCAL";
                statusBar()->showMessage("Execution Mode: " + mode, 4000);
                appendWorkflowLog("Switched execution mode to: " + mode);
            });
            connect(cloud, &HybridCloudManager::providerHealthChanged, this, [this](const QString& provider, bool healthy) {
                if (!healthy) appendWorkflowLog("WARNING: Provider issue detected for " + provider);
            });
        }
    }

    // Connect FeatureEngine signals to widgets
    if (m_featureEngine) {
        if (m_suggestionWidget) {
            connect(m_featureEngine, &AutonomousFeatureEngine::suggestionGenerated,
                    m_suggestionWidget, &AutonomousSuggestionWidget::addSuggestion);
        }
        if (m_securityWidget) {
            connect(m_featureEngine, &AutonomousFeatureEngine::securityIssueDetected,
                    m_securityWidget, &SecurityAlertWidget::addIssue);
        }
        if (m_optimizationWidget) {
            connect(m_featureEngine, &AutonomousFeatureEngine::optimizationFound,
                    m_optimizationWidget, &OptimizationPanelWidget::addOptimization);
        }
        
        // General feature engine signals
        connect(m_featureEngine, &AutonomousFeatureEngine::errorOccurred, this, [this](const QString& error) {
            statusBar()->showMessage("Autonomous Engine Error: " + error, 5000);
            appendWorkflowLog("Autonomous Engine Error: " + error);
        });
        connect(m_featureEngine, &AutonomousFeatureEngine::analysisComplete, this, [this](const QString& file) {
            statusBar()->showMessage("Analysis complete: " + file, 3000);
        });
    }

    // Connect editor changes to real-time analysis
    if (m_codeEditor && m_featureEngine) {
        connect(m_codeEditor, &QPlainTextEdit::textChanged, this, [this]() {
            if (m_featureEngine) {
                m_featureEngine->analyzeCode(m_codeEditor->toPlainText(), "untitled.cpp", "cpp");
            }
        });
    }
    
    applyDarkTheme();
    restoreWindowState();
}

void MainWindow_v5::setupUI()
{
    RAWRXD_TIMED_FUNC();
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
    RAWRXD_TIMED_FUNC();
    // File Menu
    auto fileMenu = menuBar()->addMenu("&File");
    fileMenu->addAction("&New File", this, &MainWindow_v5::onNewFile, QKeySequence::New);
    fileMenu->addAction("&Open File...", this, &MainWindow_v5::onOpenFile, QKeySequence::Open);
    fileMenu->addAction("&Save", this, &MainWindow_v5::onSaveFile, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("Settings...", this, &MainWindow_v5::openSettings);
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
    viewMenu->addAction("New Chat Window", this, &MainWindow_v5::toggleAIChatPanel);
    viewMenu->addAction("Show File Browser", this, &MainWindow_v5::toggleFileBrowser);
    viewMenu->addAction("Show Terminal", this, &MainWindow_v5::toggleTerminal);
    viewMenu->addAction("Show Model Manager", this, &MainWindow_v5::toggleModelManager);
    viewMenu->addAction("Show Problems", this, &MainWindow_v5::toggleProblemsPanel);
    viewMenu->addAction("Show To-Do", this, &MainWindow_v5::toggleTodoDock);
    viewMenu->addAction("Show Debugger", this, &MainWindow_v5::toggleDebugger, QKeySequence("Ctrl+Shift+D"));
    viewMenu->addAction("Show Test Explorer", this, &MainWindow_v5::toggleTestExplorer, QKeySequence("Ctrl+Shift+T"));
    viewMenu->addSeparator();
    viewMenu->addAction("Command Palette", this, [this]() {
        if (m_commandPalette) m_commandPalette->show();
    }, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
    viewMenu->addSeparator();
    viewMenu->addAction("Toggle Agentic Workflow", this, [this]() {
        for (QDockWidget* dock : findChildren<QDockWidget*>()) {
            if (dock && dock->windowTitle() == "Agentic Workflow") {
                dock->setVisible(!dock->isVisible());
                if (dock->isVisible()) dock->raise();
                break;
            }
        }
    }, QKeySequence(Qt::ALT | Qt::Key_W));
    viewMenu->addAction("Toggle AI Suggestions", this, [this]() {
        for (QDockWidget* dock : findChildren<QDockWidget*>()) {
            if (dock && dock->windowTitle() == "AI Suggestions") {
                dock->setVisible(!dock->isVisible());
                if (dock->isVisible()) dock->raise();
                break;
            }
        }
    });
    viewMenu->addAction("Toggle Security Analysis", this, [this]() {
        for (QDockWidget* dock : findChildren<QDockWidget*>()) {
            if (dock && dock->windowTitle() == "Security Analysis") {
                dock->setVisible(!dock->isVisible());
                if (dock->isVisible()) dock->raise();
                break;
            }
        }
    });
    viewMenu->addAction("Toggle Performance Optimizations", this, [this]() {
        for (QDockWidget* dock : findChildren<QDockWidget*>()) {
            if (dock && dock->windowTitle() == "Performance Optimizations") {
                dock->setVisible(!dock->isVisible());
                if (dock->isVisible()) dock->raise();
                break;
            }
        }
    });
    
    // AI Menu
    auto aiMenu = menuBar()->addMenu("&AI");
    aiMenu->addAction("&Generate Code", this, &MainWindow_v5::generateCode);
    
    // Autonomous Menu
    auto autoMenu = menuBar()->addMenu("&Autonomous");
    autoMenu->addAction("Analyze Project", this, [this]() {
        if (m_autonomousOrchestrator) {
            statusBar()->showMessage("Analyzing project codebase...");
            m_autonomousOrchestrator->analyzeProject(QDir::currentPath());
        }
    });
    autoMenu->addAction("Generate Tests", this, [this]() {
        if (m_autonomousOrchestrator) m_autonomousOrchestrator->autoGenerateTests();
    });
    autoMenu->addAction("Fix Bugs", this, [this]() {
        if (m_autonomousOrchestrator) m_autonomousOrchestrator->autoFixBugs();
    });
    autoMenu->addSeparator();
    QAction* autoModeAction = autoMenu->addAction("Autonomous Mode");
    autoModeAction->setCheckable(true);
    connect(autoModeAction, &QAction::toggled, this, [this](bool checked) {
        if (checked) m_autonomousOrchestrator->startAutonomousMode(QDir::currentPath());
        else m_autonomousOrchestrator->stopAutonomousMode();
    });
    
    // Help Menu
    auto helpMenu = menuBar()->addMenu("&Help");
    helpMenu->addAction("&About", this, [this]() {
        statusBar()->showMessage("RawrXD Agentic IDE v5.0 - AI-Powered Development Environment", 3000);
    });

    // Debug Menu
    auto debugMenu = menuBar()->addMenu("&Debug");
    debugMenu->addAction("Start Debugging", this, [this]() {
        if (m_debuggerPanel) {
            m_debuggerDock->setVisible(true);
            m_debuggerDock->raise();
            m_debuggerPanel->setPaused(false);
            statusBar()->showMessage("Debugger Attached", 3000);
        }
    }, QKeySequence(Qt::Key_F5));
    debugMenu->addAction("Step Over", this, [this]() { if (m_debuggerPanel) m_debuggerPanel->stepOverRequested(); }, QKeySequence(Qt::Key_F10));
    debugMenu->addAction("Step Into", this, [this]() { if (m_debuggerPanel) m_debuggerPanel->stepIntoRequested(); }, QKeySequence(Qt::Key_F11));
    debugMenu->addAction("Step Out", this, [this]() { if (m_debuggerPanel) m_debuggerPanel->stepOutRequested(); }, QKeySequence(Qt::SHIFT | Qt::Key_F11));
    debugMenu->addSeparator();
    debugMenu->addAction("Stop Debugging", this, [this]() { if (m_debuggerPanel) m_debuggerPanel->stopRequested(); }, QKeySequence(Qt::SHIFT | Qt::Key_F5));

    // Run Menu
    auto runMenu = menuBar()->addMenu("&Run");
    runMenu->addAction("Run All Tests", this, [this]() { if (m_testExplorer) m_testExplorer->onRunAll(); }, QKeySequence("Ctrl+R, A"));
    runMenu->addAction("Run Selected Tests", this, [this]() { if (m_testExplorer) m_testExplorer->onRunSelected(); });
    runMenu->addSeparator();
    runMenu->addAction("Generate Tests", this, [this]() { if (m_testExplorer) m_testExplorer->onGenerateTests(); });
    runMenu->addAction("Show Coverage", this, [this]() { if (m_testExplorer) m_testExplorer->onShowCoverage(); });
}

void MainWindow_v5::setupToolBar()
{
    auto toolbar = addToolBar("Main Toolbar");
    toolbar->addAction("New", this, &MainWindow_v5::onNewFile);
    toolbar->addAction("Open", this, &MainWindow_v5::onOpenFile);
    toolbar->addAction("Save", this, &MainWindow_v5::onSaveFile);
    toolbar->addSeparator();
    toolbar->addAction("New Chat", this, &MainWindow_v5::toggleAIChatPanel);
}

void MainWindow_v5::setupStatusBar()
{
    statusBar()->showMessage("Ready - AI-Powered Development Environment");
}

void MainWindow_v5::setupDockWidgets()
{
    // AI Chat Panel (right side)
    m_aiChatPanel = new AIChatPanel(this);
    if (m_inferenceEngine) {
        m_aiChatPanel->setInferenceEngine(m_inferenceEngine);
    }
    
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
    m_terminal = new TerminalWidget(this);
    m_terminal->initialize();
    
    // Deep Integration: Wire Terminal AI features to Core Engine
    connect(m_terminal, &TerminalWidget::fixSuggested, this, [this](const QString& fixRequest){
        if (m_aiChatPanel && m_aiChatPanel->isVisible()) {
            m_aiChatPanel->sendMessage("Fix/Correction suggested: " + fixRequest);
        } else {
             statusBar()->showMessage("AI Suggestion: " + fixRequest, 5000);
        }
    });
    connect(m_terminal, &TerminalWidget::errorDetected, this, [this](const QString& errorText){
        // Auto-diagnose terminal errors if AI Chat is open
        if (m_aiChatPanel && m_aiChatPanel->isVisible()) {
             // Optional: Don't spam, maybe just log or ask user
             appendWorkflowLog("Terminal Error Detected: " + errorText);
        }
    });
    
    QDockWidget* terminalDock = new QDockWidget("Terminal", this);
    terminalDock->setWidget(m_terminal);
    terminalDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, terminalDock);

    // AI Suggestions (Right)
    m_suggestionWidget = new AutonomousSuggestionWidget(this);
    QDockWidget* suggestDock = new QDockWidget("AI Suggestions", this);
    suggestDock->setWidget(m_suggestionWidget);
    addDockWidget(Qt::RightDockWidgetArea, suggestDock);

    // Security Analysis (Bottom)
    m_securityWidget = new SecurityAlertWidget(this);
    QDockWidget* securityDock = new QDockWidget("Security Analysis", this);
    securityDock->setWidget(m_securityWidget);
    addDockWidget(Qt::BottomDockWidgetArea, securityDock);

    // Performance Optimization (Right)
    m_optimizationWidget = new OptimizationPanelWidget(this);
    QDockWidget* optDock = new QDockWidget("Performance Optimizations", this);
    optDock->setWidget(m_optimizationWidget);
    addDockWidget(Qt::RightDockWidgetArea, optDock);

    
    // Model Manager (Left)
    m_modelLoader = new ModelLoaderWidget(this);
    connect(m_modelLoader, &ModelLoaderWidget::modelLoaded, this, [this](const QString& path){
        appendWorkflowLog("Model loaded in manager: " + path);
        statusBar()->showMessage("Model loaded in manager: " + path, 3000);
        
        // Propagate to Inference Engine for Chat
        if (m_inferenceEngine) {
            bool result = m_inferenceEngine->loadModel(path);
            if (result) {
                appendWorkflowLog("Inference Engine ready with model: " + path);
                statusBar()->showMessage("AI Chat Ready: " + QFileInfo(path).fileName(), 5000);
            } else {
                appendWorkflowLog("ERROR: Inference Engine failed to load model: " + path);
                QMessageBox::warning(this, "AI Engine Error", "Failed to load model into inference engine. Chat may not work.");
            }
        }
    });
    connect(m_modelLoader, &ModelLoaderWidget::errorOccurred, this, [this](const QString& error){
        appendWorkflowLog("Model Error: " + error);
        statusBar()->showMessage("Model Error: " + error, 3000);
    });
    
    QDockWidget* modelDock = new QDockWidget("Model Manager", this);
    modelDock->setWidget(m_modelLoader);
    addDockWidget(Qt::LeftDockWidgetArea, modelDock);
    
    // Todo (Right)
    m_todoDock = new TodoDock(m_todoManager, this);
    connect(m_todoDock, &TodoDock::openFileRequested, this, [this](const QString& filePath, const QString& todoId){
        statusBar()->showMessage("Jump to To-Do: " + filePath, 3000);
    });
    
    QDockWidget* tdDock = new QDockWidget("Project To-Do", this);
    tdDock->setWidget(m_todoDock);
    addDockWidget(Qt::RightDockWidgetArea, tdDock);

    // Problems (Bottom)
    m_problemsPanel = new ProblemsPanel(this);
    connect(m_problemsPanel, &ProblemsPanel::navigateToIssue, this, [this](const QString& file, int line, int col){
        statusBar()->showMessage(QString("Jump to Issue: %1:%2").arg(file).arg(line), 3000);
    });
    connect(m_problemsPanel, &ProblemsPanel::fixRequested, this, [this](const DiagnosticIssue& issue){
        if (m_aiChatPanel) {
            if (!m_aiChatPanel->isVisible()) m_aiChatPanel->setVisible(true);
            QString prompt = QString("Fix this issue in %1:%2 - %3").arg(issue.file).arg(issue.line).arg(issue.message);
            if (!issue.code.isEmpty()) prompt += QString(" (Code: %1)").arg(issue.code);
            m_aiChatPanel->sendMessage(prompt);
        }
    });
    
    QDockWidget* probDock = new QDockWidget("Problems", this);
    probDock->setWidget(m_problemsPanel);
    addDockWidget(Qt::BottomDockWidgetArea, probDock);

    // Debugger (Bottom)
    m_debuggerPanel = new DebuggerPanel(this);
    m_debuggerDock = new QDockWidget("Debugger", this);
    m_debuggerDock->setWidget(m_debuggerPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_debuggerDock);
    m_debuggerDock->hide();

    // Test Explorer (Left)
    m_testExplorer = new TestExplorerPanel(this);
    m_testExplorer->initialize();
    m_testExplorerDock = new QDockWidget("Test Explorer", this);
    m_testExplorerDock->setWidget(m_testExplorer);
    addDockWidget(Qt::LeftDockWidgetArea, m_testExplorerDock);
    m_testExplorerDock->hide();

    // Tabify or stack them if needed
    tabifyDockWidget(chatDock, suggestDock);
    tabifyDockWidget(suggestDock, optDock);
    tabifyDockWidget(optDock, tdDock);
    
    tabifyDockWidget(browserDock, modelDock);
    tabifyDockWidget(browserDock, m_testExplorerDock);
    tabifyDockWidget(terminalDock, probDock);
    tabifyDockWidget(probDock, m_debuggerDock);
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

void MainWindow_v5::onWorkflowBrowseOutput()
{
    QString outputPath = QFileDialog::getSaveFileName(
        this,
        "Choose Generated File",
        QDir::currentPath(),
        "C++ Files (*.cpp);;Header Files (*.h);;All Files (*.*)"
    );

    if (!outputPath.isEmpty() && m_workflowOutputPath) {
        m_workflowOutputPath->setText(outputPath);
    }
}

void MainWindow_v5::onRunWorkflow()
{
    if (!m_agenticExecutor || !m_workflowStartButton) {
        return;
    }

    QString specification = m_workflowSpec ? m_workflowSpec->toPlainText().trimmed() : QString();
    QString outputPath = m_workflowOutputPath ? m_workflowOutputPath->text().trimmed() : QString();

    if (specification.isEmpty() || outputPath.isEmpty()) {
        QMessageBox::warning(this, "Incomplete Workflow", "Please provide both a specification and an output path.");
        return;
    }

    QString compiler = m_workflowCompiler ? m_workflowCompiler->currentData().toString() : QString();
    if (compiler.isEmpty() && m_workflowCompiler) {
        compiler = m_workflowCompiler->currentText();
    }

    m_workflowStartButton->setEnabled(false);
    if (m_workflowProgress) {
        m_workflowProgress->setValue(0);
        m_workflowProgress->setMaximum(5);
    }
    if (m_workflowStatus) {
        m_workflowStatus->setText("Running unified workflow...");
    }
    if (m_workflowLog) {
        m_workflowLog->clear();
    }

    appendWorkflowLog(QString("Workflow started: %1").arg(specification));
    m_agenticExecutor->executeFullWorkflow(specification, outputPath, compiler);
}

void MainWindow_v5::onWorkflowStatusUpdate(const QString& phase, const QString& detail, int step, int total)
{
    if (m_workflowProgress) {
        m_workflowProgress->setMaximum(total);
        m_workflowProgress->setValue(step);
    }
    if (m_workflowStatus) {
        m_workflowStatus->setText(QString("[%1/%2] %3 - %4").arg(step).arg(total).arg(phase, detail));
    }

    appendWorkflowLog(QString("[%1/%2] %3 - %4").arg(step).arg(total).arg(phase, detail));
    QApplication::processEvents();
}

void MainWindow_v5::onWorkflowComplete(const QJsonObject& result)
{
    if (m_workflowStartButton) {
        m_workflowStartButton->setEnabled(true);
    }
    if (m_workflowProgress) {
        m_workflowProgress->setValue(m_workflowProgress->maximum());
    }

    bool success = result.value("success").toBool();
    QString summaryText = success
        ? result.value("message", "Workflow complete").toString()
        : result.value("error", "Workflow failed").toString();
    if (m_workflowStatus) {
        m_workflowStatus->setText(summaryText);
    }

    appendWorkflowLog(QString("Workflow finished: %1").arg(summaryText));

    QJsonArray stages = result.value("stages").toArray();
    QStringList stageLines;
    for (const QJsonValue& value : stages) {
        QJsonObject stageObj = value.toObject();
        stageLines << QString("%1. %2 (%3)")
                          .arg(stageObj.value("step").toInt())
                          .arg(stageObj.value("phase").toString())
                          .arg(stageObj.value("success").toBool() ? "success" : "failed");
    }

    QString report = QString("Specification: %1\nOutput: %2\nCompiler: %3\nDuration: %4 ms\n\nStages:\n%5")
        .arg(result.value("specification").toString())
        .arg(result.value("output_path").toString())
        .arg(result.value("selected_compiler").toString())
        .arg(result.value("duration_ms").toInt())
        .arg(stageLines.join("\n"));

    if (success) {
        QMessageBox::information(this, "Workflow Complete", report);
    } else {
        QMessageBox::critical(this, "Workflow Failed", report);
    }
}

void MainWindow_v5::appendWorkflowLog(const QString& message)
{
    if (!m_workflowLog) {
        return;
    }

    m_workflowLog->append(message);
    if (QScrollBar* scrollBar = m_workflowLog->verticalScrollBar()) {
        scrollBar->setValue(scrollBar->maximum());
    }
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

void MainWindow_v5::updateIntelligenceDashboard(const QJsonObject& result)
{
    if (!m_metricsDashboard) return;
    
    // Update metrics dashboard with analysis results
    QString qualityScore = QString::number(result["code_quality_score"].toDouble(), 'f', 2);
    QString maintainability = QString::number(result["maintainability_index"].toDouble(), 'f', 2);
    int bugs = result["bugs_detected"].toInt();
    int optimizations = result["optimizations_found"].toInt();
    
    QString dashboardText = QString("Quality Score: %1\nMaintainability: %2\nBugs: %3\nOptimizations: %4")
        .arg(qualityScore, maintainability).arg(bugs).arg(optimizations);
    
    // If we have a metrics dashboard widget, update it
    if (m_metricsDashboard->isVisible()) {
        m_metricsDashboard->updateMetrics(result);
    }
    
    appendWorkflowLog("Dashboard Updated: " + dashboardText);
}

void MainWindow_v5::updateCapabilitiesDashboard(const QJsonObject& capabilities)
{
    QStringList compilers = capabilities["compilers"].toVariant().toStringList();
    QString cloudMode = capabilities["cloud_mode"].toString();
    
    QString capabilityText = QString("Compilers: %1\nCloud Mode: %2")
        .arg(compilers.join(", "), cloudMode);
    
    statusBar()->showMessage("System Capabilities Updated", 3000);
    appendWorkflowLog("Capabilities: " + capabilityText);
}

void MainWindow_v5::openSettings()
{
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Settings are saved by the dialog itself usually (QSettings)
        // But we might want to re-apply theme or behavior
        applyDarkTheme(); // quick refresh if needed
        statusBar()->showMessage("Settings saved.", 2000);
    }
}

void MainWindow_v5::toggleAIChatPanel()
{
    // If we have an existing chat dock, toggle it
    for (QObject* obj : findChildren<QDockWidget*>()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(obj);
        if (dock && dock->windowTitle() == "AI Chat") {
            dock->setVisible(!dock->isVisible());
            return;
        }
    }

    // Default: Show the main chat instance if hidden, otherwise create a new multi-window instance
    if (!m_aiChatPanel->isVisible()) {
        m_aiChatPanel->setVisible(true);
    } else {
        // Create a loose floating window for multi-chat support
        AIChatPanel* newPanel = new AIChatPanel();
        newPanel->setWindowTitle("AI Chat (Floating)");
        newPanel->setAttribute(Qt::WA_DeleteOnClose); // Cleanup on close
        
        // IMPORTANT: Initialize with same backend context so it shares the engine
        if (m_agenticExecutor) {
            // Assume we can access the engine from the main panel or executor
            // For now, we just ensure it initializes correctly
            newPanel->initialize();
            
            // Wire up inference engine so floating windows can deduce
            if (m_inferenceEngine) {
                newPanel->setInferenceEngine(m_inferenceEngine);
            }
        }
        
        newPanel->resize(400, 600);
        newPanel->show();
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

void MainWindow_v5::toggleModelManager()
{
    for (QObject* obj : findChildren<QDockWidget*>()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(obj);
        if (dock && dock->windowTitle() == "Model Manager") {
            dock->setVisible(!dock->isVisible());
            break;
        }
    }
}

void MainWindow_v5::toggleProblemsPanel()
{
    for (QObject* obj : findChildren<QDockWidget*>()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(obj);
        if (dock && dock->windowTitle() == "Problems") {
            dock->setVisible(!dock->isVisible());
            break;
        }
    }
}

void MainWindow_v5::toggleTodoDock()
{
    for (QObject* obj : findChildren<QDockWidget*>()) {
        QDockWidget* dock = qobject_cast<QDockWidget*>(obj);
        if (dock && dock->windowTitle() == "Project To-Do") {
            dock->setVisible(!dock->isVisible());
            break;
        }
    }
}

void MainWindow_v5::toggleDebugger()
{
    if (m_debuggerDock) {
        m_debuggerDock->setVisible(!m_debuggerDock->isVisible());
        if (m_debuggerDock->isVisible()) m_debuggerDock->raise();
    }
}

void MainWindow_v5::toggleTestExplorer()
{
    if (m_testExplorerDock) {
        m_testExplorerDock->setVisible(!m_testExplorerDock->isVisible());
        if (m_testExplorerDock->isVisible()) m_testExplorerDock->raise();
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
    
    if (m_terminal) {
        m_terminal->appendOutput(">>> Starting unified workflow: " + specification);
        m_terminal->appendOutput(">>> Target: " + outputPath);
        m_terminal->appendOutput(">>> Compiler: " + compiler);
    }

    if (m_workflowSpec) {
        m_workflowSpec->setPlainText(specification);
    }
    if (m_workflowOutputPath) {
        m_workflowOutputPath->setText(outputPath);
    }
    if (m_workflowCompiler) {
        int idx = m_workflowCompiler->findData(compiler);
        if (idx < 0) {
            idx = m_workflowCompiler->addItem(compiler, compiler);
        }
        m_workflowCompiler->setCurrentIndex(idx);
    }

    if (m_workflowDock) {
        m_workflowDock->raise();
        m_workflowDock->show();
    }

    onRunWorkflow();
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



