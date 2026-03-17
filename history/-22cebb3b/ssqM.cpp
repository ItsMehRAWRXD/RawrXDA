// RawrXD Agentic IDE - v5.0 Clean Implementation
// Integrates v4.3 features with proper lazy initialization and async operations
#include "MainWindow_v5.h"
#include "latency_monitor.h"
#include "latency_status_panel.h"
#include "model_loader_thread.hpp"
#include "chat_interface.h"
#include "multi_tab_editor.h"
#include "terminal_pool.h"
#include "file_browser.h"
#include "agentic_engine.h"
#include "inference_engine.hpp"
#include "gguf_loader.hpp"
#include "plan_orchestrator.h"
#include "lsp_client.h"
#include "todo_dock.h"
#include "todo_manager.h"
#include "chat_history_manager.h"
#include "database_manager.h"
#include "agentic_text_edit.h"
#include "../gui/ModelConversionDialog.h"
#include "TelemetryWindow.h"
#include <windows.h>
#include <psapi.h>
#if __has_include("widgets/masm_editor_widget.h")
#define RAWRXD_HAS_MASM_EDITOR 1
#include "widgets/masm_editor_widget.h"
#endif
#if __has_include("widgets/multi_file_search.h")
#define RAWRXD_HAS_MULTI_FILE_SEARCH 1
#include "widgets/multi_file_search.h"
#endif

// Additional IDE Components (viewable via View menu)
#include "widgets/masm_editor_widget.h"
#include "widgets/hotpatch_panel.h"
#include "widgets/multi_file_search.h"
#include "interpretability_panel_enhanced.hpp"
#include "model_loader_widget.hpp"
#include "enterprise_tools_panel.h"  // NEW: GitHub-style tools management
#include "diagnostic_panel.h"         // NEW: Diagnostic panel for manual testing
#include "utils/diagnostic_logger.h"   // NEW: Diagnostic logging

// Theme System
#include "ThemeManager.h"
#include "ThemeConfigurationPanel.h"
#include "TransparencyControlPanel.h"
#include "ThemedCodeEditor.h"

// Using RawrXD namespace for MultiFileSearchWidget
using RawrXD::MultiFileSearchWidget;

// Phase 2 Polish Features
#include "../ui/diff_dock.h"
#include "../ui/gpu_backend_selector.h"
#include "../ui/auto_model_downloader.h"
#include "../ui/model_download_dialog_new.h"
#include "../ui/telemetry_optin_dialog.h"

// ALL Q_OBJECT Headers - MUST be included to force AUTOMOC processing
// MOC doesn't discover these through other .cpp file includes
#include "agentic_ide.h"
#include "../agentic_executor.h"
#include "agentic_copilot_bridge.h"
#include "chat_workspace.h"
#include "planning_agent.h"
#include "ghost_text_renderer.h"
#include "scalar_server.h"
#include "telemetry.h"
#include "../telemetry_singleton.h"
#include <QJsonObject>
#include "transformer_block_scalar.h"
#include "training_dialog.h"
#include "training_progress_dock.h"
#include "model_registry.h"
#include "model_trainer.h"
#include "profiler.h"
#include "observability_dashboard.h"
#include "hardware_backend_selector.h"
#include "security_manager.h"
#include "settings_dialog.h"
#include "ci_cd_settings.h"
#include "tokenizer_selector.h"
#include "checkpoint_manager.h"
#include "qt_masm_bridge.h"

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
#include <QStandardPaths>
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
#include <QDialog>
#include <QFileInfo>
#include <QProgressBar>
#include <QDir>
#include <QDirIterator>
#include <QRegularExpression>
#include <QTextStream>
#include <QFile>

namespace RawrXD {

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_multiTabEditor(nullptr)
    , m_chatTabs(nullptr)
    , m_currentChatPanel(nullptr)
    , m_terminalPool(nullptr)
    , m_fileBrowser(nullptr)
    , m_agenticEngine(nullptr)
    , m_inferenceEngine(nullptr)
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
    , m_modelLoaderThread(nullptr)
    , m_loadingProgressDialog(nullptr)
    , m_loadProgressTimer(nullptr)
    , m_pendingModelPath()
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
    
    // Initialize theme system immediately (lightweight singleton)
    m_themeManager = &RawrXD::ThemeManager::instance();
    m_themeManager->setMainWindow(this);
    
    // Initialize theme system immediately (lightweight singleton)
    m_themeManager = &RawrXD::ThemeManager::instance();
    m_themeManager->setMainWindow(this);
}

MainWindow::~MainWindow()
{
    qDebug() << "[MainWindow] Destructor - saving settings";
    saveSettings();
}

void MainWindow::onSettingsApplied()
{
    qDebug() << "[MainWindow] Settings applied signal received";
    applyInferenceSettings();
    
    // Update model directory for Ollama blob detection
    auto& settings = SettingsManager::instance();
    QString defaultModelDir = settings.getValue("models/defaultPath", "").toString();
    if (m_inferenceEngine && !defaultModelDir.isEmpty()) {
        qInfo() << "[MainWindow] Updating InferenceEngine model directory:" << defaultModelDir;
        m_inferenceEngine->setModelDirectory(defaultModelDir);
        
        // Refresh model list in all open chat panels
        if (m_chatTabs) {
            for (int i = 0; i < m_chatTabs->count(); ++i) {
                if (auto* panel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(i))) {
                    // Trigger model list refresh
                    panel->refreshModelList();
                    qDebug() << "[MainWindow] Refreshed model list in chat panel" << i;
                }
            }
        }
    }
    
    // Update other components that might need refreshing
    if (m_multiTabEditor) {
        bool showLineNumbers = settings.getValue("editor/showLineNumbers", true).toBool();
        bool wordWrap = settings.getValue("editor/wordWrap", false).toBool();
        
        // Apply to all open editors
        if (auto* editor = m_multiTabEditor->getCurrentEditor()) {
            editor->setLineNumbersVisible(showLineNumbers);
            editor->setWordWrapMode(wordWrap ? QTextOption::WrapAtWordBoundaryOrAnywhere : QTextOption::NoWrap);
        }
        qDebug() << "[MainWindow] Editor settings updated: LineNumbers=" << showLineNumbers << "WordWrap=" << wordWrap;
    }
    
    statusBar()->showMessage("Settings applied", 2000);
}

void MainWindow::initialize()
{
    qDebug() << "[MainWindow] Phase 1: Initializing core components";
    updateSplashProgress("⏳ Phase 1/4: Initializing core editor...", 10);
    
    try {
        #ifdef TEST_BRUTAL
        // Trigger ModelLoaderWidget construction to run the brutal codec harness
        {
            ModelLoaderWidget tempHarness(nullptr);
        }
        #endif
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
        
        // Initialize Chat History Manager with database backend
        // Use QDir::appDataLocation() for persistent storage
        QString historyDbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(historyDbPath);  // Ensure directory exists

        auto dbManager = std::make_shared<RawrXD::Database::DatabaseManager>();
        RawrXD::Database::DatabaseConfig dbConfig;
        dbConfig.database = historyDbPath + "/chat_history.db";
        dbConfig.host = "";  // Local SQLite
        dbConfig.port = 0;
        if (!dbManager->initialize("sqlite", dbConfig)) {
            qWarning() << "[MainWindow] Chat history database initialization failed, continuing without history";
        } else {
            m_historyManager = new RawrXD::Database::ChatHistoryManager(dbManager);
            if (!m_historyManager->initialize()) {
                qWarning() << "[MainWindow] ChatHistoryManager initialization failed";
                delete m_historyManager;
                m_historyManager = nullptr;
            } else {
                qInfo() << "[MainWindow] ChatHistoryManager initialized successfully";
                updateSplashProgress("✓ Chat History initialized", 45);
            }
        }
        
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
        m_chatTabs = new QTabWidget(this);
        m_chatTabs->setTabsClosable(true);
        connect(m_chatTabs, &QTabWidget::tabCloseRequested, this, [this](int index){
            QWidget* w = m_chatTabs->widget(index);
            m_chatTabs->removeTab(index);
        });
        
        // Update current panel when tab changes
        connect(m_chatTabs, QOverload<int>::of(&QTabWidget::currentChanged), this, [this](int index){
            if (index >= 0) {
                m_currentChatPanel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(index));
            }
        });

        m_chatDock = new QDockWidget("AI Chat & Commands", this);
        m_chatDock->setWidget(m_chatTabs);
        addDockWidget(Qt::RightDockWidgetArea, m_chatDock);
        
        // ✨ NEW: Auto-create default chat panel on startup
        // This ensures there's always an AI chat available without requiring user click
        createNewChatPanel();
        updateSplashProgress("✓ Default chat panel created", 62);

        // Add "New Chat" action to menu bar
        QAction* newChatAct = new QAction(tr("New Chat"), this);
        connect(newChatAct, &QAction::triggered, this, [this](){
            createNewChatPanel();
        });
        menuBar()->addAction(newChatAct);
        
        updateSplashProgress("✓ Chat interface ready", 65);

        // Connect agentic engine -> UI once (panel signals are wired per-panel in createNewChatPanel())
        if (m_agenticEngine) {
            // Legacy non-streaming response
            connect(m_agenticEngine, &AgenticEngine::responseReady,
                this, [this](const QString& response) {
                    // If we're already streaming tokens for this request, avoid duplicating output.
                    if (m_streamingActive) {
                        return;
                    }

                    if (m_currentChatPanel) {
                        const QString trimmed = response.trimmed();
                        if (trimmed.isEmpty()) {
                            m_currentChatPanel->addAssistantMessage("⚠ Empty response received.", false);
                        } else {
                            m_currentChatPanel->addAssistantMessage(response, false);
                            
                            // FULLY RE-ENABLED: Save assistant response to history
                            if (m_historyManager && !m_currentSessionId.isEmpty()) {
                                m_historyManager->addMessage(m_currentSessionId, "assistant", response);
                                qDebug() << "[MainWindow] Saved assistant response to session:" << m_currentSessionId;
                            }
                        }
                    }
                });

            // Real-time streaming response
            connect(m_agenticEngine, &AgenticEngine::streamToken,
                this, [this](const QString& token) {
                    m_streamingActive = true;
                    if (m_currentChatPanel) {
                        // addAssistantMessage(streaming=true) creates the bubble on first token
                        m_currentChatPanel->addAssistantMessage(token, true);
                    }
                });

            connect(m_agenticEngine, &AgenticEngine::streamFinished,
                this, [this]() {
                    m_streamingActive = false;
                    if (m_currentChatPanel) {
                        QString fullResponse = m_currentChatPanel->finishStreaming();
                        
                        // FULLY RE-ENABLED: Save assistant response to history
                        if (m_historyManager && !m_currentSessionId.isEmpty() && !fullResponse.isEmpty()) {
                            m_historyManager->addMessage(m_currentSessionId, "assistant", fullResponse);
                            qDebug() << "[MainWindow] Saved assistant response to session:" << m_currentSessionId;
                        }
                    }
                });

            connect(m_agenticEngine, &AgenticEngine::errorOccurred,
                this, [this](const QString& error) {
                    m_streamingActive = false;
                    if (m_currentChatPanel) {
                        // If a streaming bubble exists, append to it; otherwise show a normal message.
                        m_currentChatPanel->addAssistantMessage("⚠ " + error, true);
                        m_currentChatPanel->finishStreaming();
                    }
                    statusBar()->showMessage("⚠ " + error, 8000);
                });
        }
        
        // Connect model selection to load GGUF files
        // Connect settings dialog to apply AI chat configuration
        connect(this, &MainWindow::settingsApplied, this, &MainWindow::onSettingsApplied);
        
        connect(this, &MainWindow::settingsApplied, this, [this](){
            if (m_currentChatPanel) {
                // Load settings from SettingsManager and apply to manager
                auto& settings = SettingsManager::instance();
                bool cloudEnabled = settings.getValue("aichat/enableCloud", false).toBool();
                QString cloudEndpoint = settings.getValue("aichat/cloudEndpoint", "https://api.openai.com/v1/chat/completions").toString();
                QString apiKey = settings.getValue("aichat/apiKey", "").toString();
                bool localEnabled = settings.getValue("aichat/enableLocal", true).toBool();
                QString localEndpoint = settings.getValue("aichat/localEndpoint", "http://localhost:11434/api/generate").toString();
                int timeout = settings.getValue("aichat/requestTimeout", 30000).toInt();
                
                // Apply settings to chat panel
                if (m_currentChatPanel) {
                    // Update chat panel with new settings
                    m_currentChatPanel->setCloudAIEnabled(cloudEnabled);
                    m_currentChatPanel->setLocalAIEnabled(localEnabled);
                    m_currentChatPanel->setCloudEndpoint(cloudEndpoint);
                    m_currentChatPanel->setLocalEndpoint(localEndpoint);
                    m_currentChatPanel->setApiKey(apiKey);
                    m_currentChatPanel->setRequestTimeout(timeout);
                    qDebug() << "[MainWindow] Settings applied to chat panel";
                }
            }
        });
        
        // Connect model ready signal to enable/disable chat input in all panels
        connect(m_agenticEngine, &AgenticEngine::modelReady, this, [this](bool ready){
            if ( m_chatTabs) {
                // Enable/disable all chat panels based on model readiness
                for (int i = 0; i < m_chatTabs->count(); ++i) {
                    if (auto* panel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(i))) {
                        // Enable/disable input field based on model readiness
                        panel->setInputEnabled(ready);
                        if (ready) {
                            qDebug() << "[MainWindow] Panel" << i << "input enabled - model ready";
                        } else {
                            qDebug() << "[MainWindow] Panel" << i << "input disabled - model loading";
                        }
                    }
                }
            }
        });
        
        // Wire progress signals
        connect(m_planOrchestrator, &RawrXD::PlanOrchestrator::planningStarted,
                this, [this](const QString& prompt) {
                    if (m_currentChatPanel) {
                        m_currentChatPanel->addAssistantMessage("📋 Planning: " + prompt, false);
                    }
                });
        
        connect(m_planOrchestrator, &RawrXD::PlanOrchestrator::executionStarted,
                this, [this](int taskCount) {
                    if (m_currentChatPanel) {
                        m_currentChatPanel->addAssistantMessage(
                            QString("🚀 Executing %1 tasks...").arg(taskCount), false);
                    }
                });
        
        connect(m_planOrchestrator, &RawrXD::PlanOrchestrator::taskExecuted,
                this, [this](int index, bool success, const QString& desc) {
                    if (m_currentChatPanel) {
                        QString status = success ? "✓" : "✗";
                        QString color = success ? "#4ec9b0" : "#f48771";
                        m_currentChatPanel->addAssistantMessage(
                            QString("<span style='color:%1;'>%2 [%3] %4</span>")
                                .arg(color).arg(status).arg(index + 1).arg(desc), false);
                    }
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
        
        // Connect inline edit signal from editor to MainWindow
        connect(m_multiTabEditor, &MultiTabEditor::inlineEditRequested,
                this, &MainWindow::onInlineEditRequested);
        
        updateSplashProgress("✓ File browser ready", 75);
        
        // Create terminal pool dock
        m_terminalPool = new TerminalPool(3, this);
        m_terminalPool->initialize();
        
        m_terminalDock = new QDockWidget("Terminals", this);
        m_terminalDock->setWidget(m_terminalPool);
        addDockWidget(Qt::BottomDockWidgetArea, m_terminalDock);
        
        updateSplashProgress("✓ Terminals ready", 85);
        
        // Create TODO dock (hidden by default)
        m_todoManager = new TodoManager(this);
        m_todoDockWidget = new QDockWidget("TODO List", this);
        m_todoDockWidget->setObjectName("todoDockWidget");
        addDockWidget(Qt::RightDockWidgetArea, m_todoDockWidget);
        m_todoDockWidget->hide();  // Hidden by default - viewable via View menu
        
        // ============================================================
        // Additional IDE Components (Lazy Loaded)
        // ============================================================
        
        // MASM Editor
        m_masmEditorDock = new QDockWidget("MASM Editor", this);
        m_masmEditorDock->setObjectName("masmEditorDock");
        addDockWidget(Qt::RightDockWidgetArea, m_masmEditorDock);
        m_masmEditorDock->hide();
        
        // Model Tuner (GGUF Model Loader with compression)
        m_modelTunerDock = new QDockWidget("Model Tuner", this);
        m_modelTunerDock->setObjectName("modelTunerDock");
        addDockWidget(Qt::RightDockWidgetArea, m_modelTunerDock);
        m_modelTunerDock->hide();
        
        // Interpretability Panel (Model analysis and visualization)
        m_interpretabilityDock = new QDockWidget("Interpretability Panel", this);
        m_interpretabilityDock->setObjectName("interpretabilityDock");
        addDockWidget(Qt::BottomDockWidgetArea, m_interpretabilityDock);
        m_interpretabilityDock->hide();
        
        // Hotpatch Panel (Real-time model corrections)
        m_hotpatchDock = new QDockWidget("Hotpatch Panel", this);
        m_hotpatchDock->setObjectName("hotpatchDock");
        addDockWidget(Qt::BottomDockWidgetArea, m_hotpatchDock);
        m_hotpatchDock->hide();
        
        // Multi-File Search
        m_multiFileSearchDock = new QDockWidget("Multi-File Search", this);
        m_multiFileSearchDock->setObjectName("multiFileSearchDock");
        addDockWidget(Qt::BottomDockWidgetArea, m_multiFileSearchDock);
        m_multiFileSearchDock->hide();
        
        // Enterprise Tools Panel
        m_toolsPanelDock = new QDockWidget("🛠️ Enterprise Tools (44)", this);
        m_toolsPanelDock->setObjectName("toolsPanelDock");
        addDockWidget(Qt::RightDockWidgetArea, m_toolsPanelDock);
        m_toolsPanelDock->hide();
        
        // Diagnostic Panel
        m_diagnosticDock = new QDockWidget("🔍 Diagnostics", this);
        m_diagnosticDock->setObjectName("diagnosticDock");
        addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticDock);
        m_diagnosticDock->hide();
        
        // Theme System Docks
        m_themeDock = new QDockWidget("Theme Configuration", this);
        m_themeDock->setObjectName("themeDock");
        m_themeDock->hide();
        addDockWidget(Qt::RightDockWidgetArea, m_themeDock);
        
        m_transparencyDock = new QDockWidget("Transparency Controls", this);
        m_transparencyDock->setObjectName("transparencyDock");
        m_transparencyDock->hide();
        addDockWidget(Qt::RightDockWidgetArea, m_transparencyDock);
        
        updateSplashProgress("✓ All panels registered (Lazy Loading enabled)", 90);
        
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

        // Initialize hardware telemetry and record IDE startup
        try {
            telemetry::InitializeHardware();
            QJsonObject meta;
            meta["phase"] = QString("startup_complete");
            meta["version"] = QString("v5.0");
            GetTelemetry().recordEvent("ide_initialized", meta);
        } catch (...) {
            qWarning() << "[MainWindow] Telemetry hardware init failed (continuing)";
        }
        
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
            
            // Initialize Phase 2 Polish Features
            initializePhase2Polish();
            
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
    editMenu->addAction("Cu&t", this, &MainWindow::cut, QKeySequence::Cut);
    editMenu->addAction("&Copy", this, &MainWindow::copy, QKeySequence::Copy);
    editMenu->addAction("&Paste", this, &MainWindow::paste, QKeySequence::Paste);
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
    m_telemetryAction = viewMenu->addAction("Telemetry &Monitor", this, &MainWindow::toggleTelemetryWindow);
    if (m_telemetryAction) {
        m_telemetryAction->setCheckable(true);
    }
    viewMenu->addSeparator();

    // Optional panels – hidden by default, toggled via View menu
    viewMenu->addAction("MASM &Editor", this, &MainWindow::toggleMASMEditor);
    viewMenu->addAction("Model &Tuner", this, &MainWindow::toggleModelTuner);
    
    // IDE Tools submenu (hidden panels viewable here)
    QMenu *toolsMenu = viewMenu->addMenu("&IDE Tools");
    toolsMenu->addAction("&MASM Editor", this, &MainWindow::toggleMASMEditor, QKeySequence("Ctrl+Shift+A"));
    toolsMenu->addAction("&Model Tuner", this, &MainWindow::toggleModelTuner, QKeySequence("Ctrl+Shift+M"));
    toolsMenu->addAction("&Interpretability Panel", this, &MainWindow::toggleInterpretabilityPanel, QKeySequence("Ctrl+Shift+I"));
    toolsMenu->addAction("&Hotpatch Panel", this, &MainWindow::toggleHotpatchPanel, QKeySequence("Ctrl+Shift+H"));
    toolsMenu->addAction("Multi-File &Search", this, &MainWindow::toggleMultiFileSearch, QKeySequence("Ctrl+Shift+F"));
    toolsMenu->addSeparator();
    toolsMenu->addAction("🛠️ Enterprise &Tools Panel", this, &MainWindow::toggleToolsPanel, QKeySequence("Ctrl+Shift+T"));  // NEW: 44-tool management
    toolsMenu->addAction("🔍 Diagnostic Panel", this, &MainWindow::toggleDiagnosticPanel, QKeySequence("Ctrl+Shift+D"));  // NEW: Diagnostic panel
    viewMenu->addSeparator();
    
    // TODO Panel submenu
    QMenu *todoMenu = viewMenu->addMenu("TODO Panel");
    todoMenu->addAction("Add TODO", this, &MainWindow::addTodo, QKeySequence("Ctrl+T"));
    todoMenu->addAction("Scan Code for TODOs", this, &MainWindow::scanCodeForTodos);
    todoMenu->addSeparator();
    todoMenu->addAction("Clear All TODOs", this, [this]() {
        if (m_todoManager) {
            m_todoManager->clearAllTodos();
            statusBar()->showMessage("✓ All TODOs cleared", 2000);
        }
    });
    
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
    
        // Chat History submenu
        QMenu *chatHistoryMenu = aiMenu->addMenu("Chat &History");
        chatHistoryMenu->addAction("&Browse Sessions", this, &MainWindow::showChatSessionBrowser, QKeySequence("Ctrl+H"));
        chatHistoryMenu->addAction("&New Chat Session", this, &MainWindow::startChat);
        chatHistoryMenu->addSeparator();
        chatHistoryMenu->addAction("&Clear All History", this, [this]() {
            if (!m_historyManager) {
                QMessageBox::information(this, "Chat History", "Chat history not initialized");
                return;
            }
        
            auto reply = QMessageBox::question(this, "Clear Chat History",
                "Delete all chat sessions and messages?\\n\\nThis cannot be undone.",
                QMessageBox::Yes | QMessageBox::No);
        
            if (reply == QMessageBox::Yes) {
                auto sessions = m_historyManager->getSessions();
                int deletedCount = 0;
            
                for (const auto& sessionVal : sessions) {
                    QJsonObject session = sessionVal.toObject();
                    if (m_historyManager->deleteSession(session[QStringLiteral("id")].toString())) {
                        deletedCount++;
                    }
                }
            
                statusBar()->showMessage(QString("✓ Deleted %1 sessions").arg(deletedCount), 3000);
                qInfo() << "[MainWindow] Chat history cleared:" << deletedCount << "sessions deleted";
            }
        });
    
        aiMenu->addSeparator();
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
    
    // Create latency monitor and status panel
    m_latencyMonitor = new RawrXD::LatencyMonitor(this);
    m_latencyPanel = new RawrXD::LatencyStatusPanel(m_latencyMonitor, this);
    m_latencyDock = new QDockWidget("Latency & Statistics", this);
    m_latencyDock->setWidget(m_latencyPanel);
    m_latencyDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::LeftDockWidgetArea);
    addDockWidget(Qt::BottomDockWidgetArea, m_latencyDock);
    
    // Initial status
    m_latencyMonitor->setStatus("idle");
    
    // FULLY RE-ENABLED: Phase 6 Qt-MASM bridge property binding
    QTimer::singleShot(300, [this]() {
        RawrXD::QtMasmBridge::instance().bindProperty(
            "main.status",
            [this](const QVariant& value) {
                statusBar()->showMessage(value.toString(), 4000);
            },
            "Phase 6 Qt-MASM bridge ready"
        );
    });
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

void MainWindow::cut()
{
    if (m_multiTabEditor) {
        AgenticTextEdit* editor = m_multiTabEditor->getCurrentEditor();
        if (editor) {
            editor->cut();
        }
    }
}

void MainWindow::copy()
{
    if (m_multiTabEditor) {
        AgenticTextEdit* editor = m_multiTabEditor->getCurrentEditor();
        if (editor) {
            editor->copy();
        }
    }
}

void MainWindow::paste()
{
    if (m_multiTabEditor) {
        AgenticTextEdit* editor = m_multiTabEditor->getCurrentEditor();
        if (editor) {
            editor->paste();
        }
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
        if (!m_todoDockWidget->widget()) {
            qDebug() << "[MainWindow] Lazy loading TODO Panel...";
            m_todoDock = new TodoDock(m_todoManager, this);
            m_todoDockWidget->setWidget(m_todoDock);
        }
        m_todoDockWidget->setVisible(!m_todoDockWidget->isVisible());
    }
}

// ------------------------------------------------------------
// Optional panel helpers
// ------------------------------------------------------------
void MainWindow::toggleTelemetryWindow()
{
    if (!m_telemetryWindow) {
        m_telemetryWindow = new RawrXD::TelemetryWindow(this);
        m_telemetryWindow->setLogDirectory(QDir::currentPath());
        connect(m_telemetryWindow, &QDialog::finished, this, [this](int) {
            if (m_telemetryAction) {
                m_telemetryAction->setChecked(false);
            }
        });
    }

    const bool shouldShow = !m_telemetryWindow->isVisible();
    if (shouldShow) {
        m_telemetryWindow->show();
        m_telemetryWindow->raise();
        m_telemetryWindow->activateWindow();
    } else {
        m_telemetryWindow->hide();
    }

    if (m_telemetryAction) {
        m_telemetryAction->setChecked(shouldShow);
    }
}

void MainWindow::toggleMASMEditor()
{
    if (m_masmEditorDock) {
        // Lazy initialization of the widget
        if (!m_masmEditor) {
            qDebug() << "[MainWindow] Lazy loading MASM Editor...";
            m_masmEditor = new MASMEditorWidget(this);
            m_masmEditorDock->setWidget(m_masmEditor);
        }
        
        m_masmEditorDock->setVisible(!m_masmEditorDock->isVisible());
        if (m_masmEditorDock->isVisible()) {
            m_masmEditorDock->raise();
            statusBar()->showMessage("MASM Editor opened", 2000);
        }
    }
}

void MainWindow::toggleModelTuner()
{
    if (m_modelTunerDock) {
        // Lazy initialization of the widget
        if (!m_modelTuner) {
            qDebug() << "[MainWindow] Lazy loading Model Tuner...";
            m_modelTuner = new ModelLoaderWidget(this);
            m_modelTunerDock->setWidget(m_modelTuner);
            
            // Wire GGUF model loader to chat panel model selection
            connect(m_modelTuner, &ModelLoaderWidget::backendChanged, this, [this](const QString& backend) {
                if (m_latencyMonitor) m_latencyMonitor->setBackend(backend);
            });

            connect(m_modelTuner, &ModelLoaderWidget::modelLoaded, this, [this](const QString& modelPath) {
                qDebug() << "[MainWindow] Model loaded via tuner:" << modelPath;
                applyChatModelSelection(modelPath);
                statusBar()->showMessage("✓ Model " + QFileInfo(modelPath).fileName() + " loaded and applied to all chat panels", 3000);
            });
            
            connect(m_modelTuner, &ModelLoaderWidget::errorOccurred, this, [this](const QString& error) {
                qWarning() << "[MainWindow] Model loading error:" << error;
                statusBar()->showMessage("⚠ Model loading failed: " + error, 5000);
            });
        }
        
        m_modelTunerDock->setVisible(!m_modelTunerDock->isVisible());
        if (m_modelTunerDock->isVisible()) {
            m_modelTunerDock->raise();
            statusBar()->showMessage("Model Tuner opened", 2000);
        }
    }
}

void MainWindow::toggleInterpretabilityPanel()
{
    if (m_interpretabilityDock) {
        // Lazy initialization of the widget
        if (!m_interpretabilityPanel) {
            qDebug() << "[MainWindow] Lazy loading Interpretability Panel...";
            m_interpretabilityPanel = new InterpretabilityPanelEnhanced(this);
            m_interpretabilityDock->setWidget(m_interpretabilityPanel);
        }
        
        m_interpretabilityDock->setVisible(!m_interpretabilityDock->isVisible());
        if (m_interpretabilityDock->isVisible()) {
            m_interpretabilityDock->raise();
            statusBar()->showMessage("Interpretability Panel opened", 2000);
        }
    }
}

void MainWindow::toggleHotpatchPanel()
{
    if (m_hotpatchDock) {
        // Lazy initialization of the widget
        if (!m_hotpatchPanel) {
            qDebug() << "[MainWindow] Lazy loading Hotpatch Panel...";
            m_hotpatchPanel = new HotpatchPanel(this);
            m_hotpatchDock->setWidget(m_hotpatchPanel);
        }
        
        m_hotpatchDock->setVisible(!m_hotpatchDock->isVisible());
        if (m_hotpatchDock->isVisible()) {
            m_hotpatchDock->raise();
            statusBar()->showMessage("Hotpatch Panel opened", 2000);
        }
    }
}

void MainWindow::toggleMultiFileSearch()
{
    if (m_multiFileSearchDock) {
        // Lazy initialization of the widget
        if (!m_multiFileSearch) {
            qDebug() << "[MainWindow] Lazy loading Multi-File Search...";
            m_multiFileSearch = new RawrXD::MultiFileSearchWidget(this);
            m_multiFileSearchDock->setWidget(m_multiFileSearch);
        }
        
        m_multiFileSearchDock->setVisible(!m_multiFileSearchDock->isVisible());
        if (m_multiFileSearchDock->isVisible()) {
            m_multiFileSearchDock->raise();
            statusBar()->showMessage("Multi-File Search opened", 2000);
        }
    }
}

void MainWindow::toggleToolsPanel()
{
    if (m_toolsPanelDock) {
        // Lazy initialization of the real panel
        if (!m_toolsPanel) {
            qDebug() << "[MainWindow] Lazy loading Enterprise Tools Panel...";
            m_toolsPanel = new EnterpriseToolsPanel(this);
            m_toolsPanel->initialize();
            m_toolsPanelDock->setWidget(m_toolsPanel);
        }
        
        m_toolsPanelDock->setVisible(!m_toolsPanelDock->isVisible());
        if (m_toolsPanelDock->isVisible()) {
            m_toolsPanelDock->raise();
            statusBar()->showMessage("🛠️ Enterprise Tools Panel: 44 tools available", 3000);
        } else {
            statusBar()->showMessage("Enterprise Tools Panel closed", 2000);
        }
    }
}

// AI operations
void MainWindow::startChat()
{
    if (m_chatDock) {
        m_chatDock->setVisible(true);
        m_chatDock->raise();
        if (m_currentChatPanel) {
            m_currentChatPanel->setFocus();
        }
    }
}

void MainWindow::showChatSessionBrowser()
{
    if (!m_historyManager) {
        QMessageBox::information(this, "Chat History", "Chat history not initialized");
        return;
    }
    
    // Get all sessions
    auto sessions = m_historyManager->getSessions();
    
    if (sessions.isEmpty()) {
        QMessageBox::information(this, "Chat History", "No previous chat sessions found.\n\nStart a new chat to create a session.");
        return;
    }
    
    // Create session selection dialog
    QDialog* dialog = new QDialog(this);
    dialog->setWindowTitle("Chat History - Select Session");
    dialog->setModal(true);
    dialog->resize(500, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    
    // Add label
    QLabel* label = new QLabel("Select a previous chat session to resume:");
    layout->addWidget(label);
    
    // Create list widget for sessions
    QListWidget* sessionList = new QListWidget();
    sessionList->setSelectionMode(QAbstractItemView::SingleSelection);
    
    for (const auto& session : sessions) {
        QJsonObject sessionObj = session.toObject();
        QString sessionId = sessionObj[QStringLiteral("id")].toString();
        QString title = sessionObj[QStringLiteral("title")].toString();
        qint64 timestamp = sessionObj[QStringLiteral("created_at")].toVariant().toLongLong();
        int messageCount = sessionObj[QStringLiteral("message_count")].toInt();

        // Format date for display
        QDateTime dt = QDateTime::fromMSecsSinceEpoch(timestamp);
        QString dateStr = dt.toString("yyyy-MM-dd hh:mm");
        
        QString displayText = QString("%1  [%2] (%3 messages)")
            .arg(title)
            .arg(dateStr)
            .arg(messageCount);
        
        QListWidgetItem* item = new QListWidgetItem(displayText);
        item->setData(Qt::UserRole, sessionId);
        sessionList->addItem(item);
    }
    
    layout->addWidget(sessionList);
    
    // Buttons
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* loadBtn = new QPushButton("Load Session");
    QPushButton* deleteBtn = new QPushButton("Delete Session");
    QPushButton* cancelBtn = new QPushButton("Cancel");
    
    buttonLayout->addWidget(loadBtn);
    buttonLayout->addWidget(deleteBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelBtn);
    layout->addLayout(buttonLayout);
    
    // Connect buttons
    connect(loadBtn, &QPushButton::clicked, [this, dialog, sessionList]() {
        if (sessionList->currentItem()) {
            QString sessionId = sessionList->currentItem()->data(Qt::UserRole).toString();
            
            // Load session in current panel
            if (m_currentChatPanel) {
                emit m_currentChatPanel->sessionSelected(sessionId);
                qInfo() << "[MainWindow] Session loaded:" << sessionId;
            }
            
            dialog->accept();
        }
    });
    
    connect(deleteBtn, &QPushButton::clicked, [this, dialog, sessionList]() {
        if (sessionList->currentItem()) {
            QString sessionId = sessionList->currentItem()->data(Qt::UserRole).toString();
            
            auto reply = QMessageBox::question(dialog, "Delete Session",
                QString("Delete session '%1'?\n\nThis cannot be undone.").arg(sessionList->currentItem()->text()),
                QMessageBox::Yes | QMessageBox::No);
            
            if (reply == QMessageBox::Yes) {
                if (m_historyManager->deleteSession(sessionId)) {
                    qInfo() << "[MainWindow] Session deleted:" << sessionId;
                    delete sessionList->takeItem(sessionList->row(sessionList->currentItem()));
                    statusBar()->showMessage("✓ Session deleted", 2000);
                } else {
                    QMessageBox::warning(dialog, "Delete Failed", "Could not delete session");
                }
            }
        }
    });
    
    connect(cancelBtn, &QPushButton::clicked, dialog, &QDialog::reject);
    
    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::analyzeCode()
{
    if (m_currentChatPanel) {
        m_currentChatPanel->addUserMessage("@analyze current file");
        onChatMessageSent("@analyze current file");
    }
}

void MainWindow::generateCode()
{
    if (m_currentChatPanel) {
        m_currentChatPanel->addUserMessage("@generate ");
        m_currentChatPanel->setFocus();
    }
}

void MainWindow::refactorCode()
{
    if (m_currentChatPanel) {
        m_currentChatPanel->addAssistantMessage(
            "💡 Tip: Type '/refactor <description>' to perform multi-file refactoring\n"
            "Example: /refactor change UserManager to use UUID instead of int ID", false);
        if (m_chatDock) {
            m_chatDock->setVisible(true);
            m_chatDock->raise();
        }
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
    if (m_currentChatPanel) {
        m_currentChatPanel->addAssistantMessage(
            "<h3>AI Commands</h3>"
            "<b>/refactor &lt;prompt&gt;</b> - Multi-file AI refactoring<br>"
            "<b>@plan &lt;task&gt;</b> - Create implementation plan<br>"
            "<b>@analyze</b> - Analyze current file<br>"
            "<b>@generate &lt;spec&gt;</b> - Generate code<br>"
            "<b>/help</b> - Show all commands", false);
        if (m_chatDock) {
            m_chatDock->setVisible(true);
            m_chatDock->raise();
        }
    }
}

void MainWindow::loadModel()
{
    QString modelPath = QFileDialog::getOpenFileName(this, 
        "Load AI Model", 
        QDir::homePath(), 
        "AI Models (*.gguf sha256-*);;All Files (*)");
    
    if (!modelPath.isEmpty()) {
        // Actually load the model through onModelSelected which does the real work
        statusBar()->showMessage(QString("Loading model: %1...").arg(QFileInfo(modelPath).fileName()), 5000);
        
        // Use QTimer to prevent blocking the UI during model loading
        QTimer::singleShot(100, this, [this, modelPath]() {
            onModelSelected(modelPath);
        });
    }
}

void MainWindow::onModelSelected(const QString &ggufPath)
{
    // Validate model path
    if (ggufPath.isEmpty() || !QFile::exists(ggufPath)) {
        QString errorMsg = QString("Model file not found: %1").arg(ggufPath);
        LOG_ERROR("ModelLoader", errorMsg);
        QMessageBox::critical(this, "Invalid Model", errorMsg);
        statusBar()->showMessage("❌ Model file not found", 3000);
        return;
    }
    
    LOG_INFO("ModelLoader", QString("Starting model load: %1").arg(ggufPath));
    
    qDebug() << "[MainWindow::onModelSelected] Loading model:" << ggufPath;
    
    // Check if this is an Ollama blob file
    if (m_inferenceEngine && m_inferenceEngine->isBlobPath(ggufPath)) {
        qInfo() << "[MainWindow] Detected Ollama blob file:" << ggufPath;
        statusBar()->showMessage("🔄 Loading Ollama blob model...", 0);
    } else {
        statusBar()->showMessage("🔄 Loading GGUF model...", 0);
    }
    
    // Record telemetry: model load requested
    {
        QJsonObject meta; meta["path"] = ggufPath; meta["event"] = QString("requested");
        GetTelemetry().recordEvent("model_load", meta);
    }
    QApplication::processEvents(); // Update UI
    
    // Create inference engine if it doesn't exist
    if (!m_inferenceEngine) {
        qDebug() << "[MainWindow] Creating new InferenceEngine";
        m_inferenceEngine = new ::InferenceEngine(QString(), this);  // Empty path - no immediate load
        
        // Set model directory for Ollama blob detection
        auto& settings = SettingsManager::instance();
        QString defaultModelDir = settings.getValue("models/defaultPath", "").toString();
        if (!defaultModelDir.isEmpty()) {
            qInfo() << "[MainWindow] Setting model directory for Ollama blob detection:" << defaultModelDir;
            m_inferenceEngine->setModelDirectory(defaultModelDir);
        } else {
            // Use current directory as fallback
            qWarning() << "[MainWindow] No default model directory set - using current directory for blob detection";
            m_inferenceEngine->setModelDirectory(QDir::currentPath());
        }
        
        // Connect signal for unsupported quantization type detection
        connect(m_inferenceEngine, &::InferenceEngine::unsupportedQuantizationTypeDetected,
                this, [this](const QStringList& unsupportedTypes, const QString& recommendedType, const QString& modelPath) {
            // Show conversion dialog when unsupported types are detected
            ModelConversionDialog* conversionDialog = new ModelConversionDialog(
                unsupportedTypes, recommendedType, modelPath, this);
            
            if (conversionDialog->exec() == QDialog::Accepted) {
                auto result = conversionDialog->conversionResult();
                if (result == ModelConversionDialog::ConversionSucceeded) {
                    QString convertedPath = conversionDialog->convertedModelPath();
                    qInfo() << "[MainWindow] Conversion succeeded, reloading model from:" << convertedPath;
                    
                    // Reload model from converted path
                    if (m_inferenceEngine) {
                        m_inferenceEngine->unloadModel();
                        m_inferenceEngine->loadModel(convertedPath);
                    }
                }
            }
            
            conversionDialog->deleteLater();
        });
    }
    
    // Store path
    m_pendingModelPath = ggufPath;
    
    // Create progress dialog
    if (!m_loadingProgressDialog) {
        m_loadingProgressDialog = new QProgressDialog(this);
        m_loadingProgressDialog->setWindowTitle("Loading Model");
        m_loadingProgressDialog->setWindowModality(Qt::WindowModal);
        m_loadingProgressDialog->setMinimumDuration(0);
        m_loadingProgressDialog->setAutoClose(false);
        m_loadingProgressDialog->setAutoReset(false);
        connect(m_loadingProgressDialog, &QProgressDialog::canceled, this, &MainWindow::onModelLoadCanceled);
    }
    
    QString modelName = QFileInfo(ggufPath).fileName();
    m_loadingProgressDialog->setLabelText(QString("Loading %1...\nInitializing...").arg(modelName));
    m_loadingProgressDialog->setRange(0, 0);  // Indeterminate
    m_loadingProgressDialog->setValue(0);
    m_loadingProgressDialog->show();
    
    qInfo() << "[MainWindow] Starting std::thread model load for:" << ggufPath;
    
    // Clean up existing thread if any
    if (m_modelLoaderThread) {
        m_modelLoaderThread->cancel();
        m_modelLoaderThread->wait(2000);
        delete m_modelLoaderThread;
        m_modelLoaderThread = nullptr;
    }
    
    // Create new loader thread (pure C++ std::thread)
    m_modelLoaderThread = new ModelLoaderThread(m_inferenceEngine, ggufPath.toStdString());
    
    // Set progress callback (called from background thread)
    m_modelLoaderThread->setProgressCallback([this](const std::string& msg) {
        // Post to main thread via QTimer
        QMetaObject::invokeMethod(this, [this, msg]() {
            if (m_loadingProgressDialog && m_loadingProgressDialog->isVisible()) {
                QString qmsg = QString::fromStdString(msg);
                m_loadingProgressDialog->setLabelText(qmsg);
            }
        }, Qt::QueuedConnection);
    });
    
    // Set completion callback (called from background thread)
    m_modelLoaderThread->setCompleteCallback([this](bool success, const std::string& errorMsg) {
        // Post to main thread
        QMetaObject::invokeMethod(this, [this, success, errorMsg]() {
            onModelLoadFinished(success, errorMsg);
        }, Qt::QueuedConnection);
    });
    
    // Start the thread
    m_modelLoaderThread->start();
    
    // Setup timer to check if thread is still alive
    if (!m_loadProgressTimer) {
        m_loadProgressTimer = new QTimer(this);
        connect(m_loadProgressTimer, &QTimer::timeout, this, &MainWindow::checkLoadProgress);
    }
    m_loadProgressTimer->start(500);  // Check every 500ms
}

void MainWindow::checkLoadProgress()
{
    // This runs on main thread, checking if background thread is still alive
    if (m_modelLoaderThread && !m_modelLoaderThread->isRunning()) {
        // Thread finished, stop timer
        if (m_loadProgressTimer) {
            m_loadProgressTimer->stop();
        }
    }
}

void MainWindow::applyChatModelSelection(const QString& modelIdentifier)
{
    const QString modelDisplay = QFileInfo(modelIdentifier).fileName().isEmpty()
        ? modelIdentifier
        : QFileInfo(modelIdentifier).fileName();

    qDebug() << "[MainWindow::applyChatModelSelection] Called with:" << modelIdentifier
             << ", display:" << modelDisplay;

    // Keep agentic engine in sync so downstream processing is enabled
    if (m_agenticEngine) {
        // DO NOT call setModel() - it tries to load the model again with just the filename
        // We already loaded the model via InferenceEngine, just mark it as loaded
        m_agenticEngine->markModelAsLoaded(modelIdentifier);
        qDebug() << "[MainWindow] Agentic engine model set to" << modelDisplay;
    }

    // Enable input on ALL existing chat panels and update their dropdowns
    if (m_chatTabs) {
        qDebug() << "[MainWindow::applyChatModelSelection] Enabling input on" << m_chatTabs->count() << "panels";
        for (int i = 0; i < m_chatTabs->count(); ++i) {
            if (auto* panel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(i))) {
                panel->setLocalModel(modelDisplay);
                panel->setSelectedModel(modelDisplay);
                panel->setInputEnabled(true);
                qDebug() << "[MainWindow::applyChatModelSelection] Enabled input on panel" << i 
                         << "with model:" << modelDisplay;
            }
        }
    }

    // If no current panel exists, create one with the loaded model enabled
    if (!m_currentChatPanel && m_chatTabs && m_chatTabs->count() == 0) {
        qDebug() << "[MainWindow::applyChatModelSelection] No chat panel exists, creating one";
        AIChatPanel* newPanel = createNewChatPanel();
        if (newPanel) {
            newPanel->setLocalModel(modelDisplay);
            newPanel->setSelectedModel(modelDisplay);
            newPanel->setInputEnabled(true);
            newPanel->setFocus();
            qDebug() << "[MainWindow::applyChatModelSelection] Created new panel with model:" << modelDisplay;
        }
    } else if (m_currentChatPanel) {
        m_currentChatPanel->setFocus();
    }
    
    // Update the global model state so new panels know a model is loaded
    m_pendingModelPath = modelIdentifier;
    qDebug() << "[MainWindow::applyChatModelSelection] Updated m_pendingModelPath to:" << modelIdentifier;
}

void MainWindow::onModelLoadFinished(bool loadSuccess, const std::string& errorMsgStr)
{
    QString ggufPath = m_pendingModelPath;
    
    // Stop progress timer
    if (m_loadProgressTimer) {
        m_loadProgressTimer->stop();
    }
    
    // Hide progress dialog
    if (m_loadingProgressDialog) {
        m_loadingProgressDialog->hide();
    }
    
    qInfo() << "[MainWindow::onModelLoadFinished] Result:" << (loadSuccess ? "SUCCESS" : "FAILED");
    
    if (loadSuccess) {
        LOG_INFO("ModelLoader", QString("Model loaded successfully: %1").arg(ggufPath));
        
        if (m_agenticEngine) {
            m_agenticEngine->setInferenceEngine(m_inferenceEngine);
            // Mark model as loaded so AgenticEngine uses inference instead of fallback stubs
            m_agenticEngine->markModelAsLoaded(ggufPath);
            qInfo() << "[MainWindow] AgenticEngine now using loaded model:" << ggufPath;
        }

        applyChatModelSelection(ggufPath);
        
        // Update status bar with comprehensive info
        QString modelName = QFileInfo(ggufPath).baseName();
        QString backend = "CPU";  // Default, could be read from settings
        QSettings settings("RawrXD", "AgenticIDE");
        QString savedBackend = settings.value("AI/backend", "Auto").toString();
        if (savedBackend.contains("Vulkan")) backend = "Vulkan";
        else if (savedBackend.contains("CUDA")) backend = "CUDA";
        
        QString lspStatus = (m_lspClient && m_lspClient->isRunning()) ? "✔" : "✘";
        
        statusBar()->showMessage(
            QString("Model: %1 | GPU: %2 | LSP: %3")
            .arg(modelName).arg(backend).arg(lspStatus));
        
        qInfo() << "[MainWindow] ✅ Model loaded successfully:" << modelName;
        // Telemetry: model load succeeded
        QJsonObject meta; meta["path"] = ggufPath; meta["status"] = QString("success");
        GetTelemetry().recordEvent("model_load", meta);
    } else {
        QString errorMsg = QString("Failed to load GGUF model: %1 - %2").arg(ggufPath).arg(QString::fromStdString(errorMsgStr));
        LOG_ERROR("ModelLoader", errorMsg);
        QMessageBox::critical(this, "Load Failed", 
            QString("Failed to load GGUF model: %1\n\nCheck the console for detailed error messages.").arg(ggufPath));
        statusBar()->showMessage(QString("❌ Model load failed: %1").arg(QFileInfo(ggufPath).fileName()), 5000);
        // Telemetry: model load failed
        QJsonObject meta; meta["path"] = ggufPath; meta["status"] = QString("failed"); meta["error"] = QString::fromStdString(errorMsgStr);
        GetTelemetry().recordEvent("model_load", meta);
    }
    
    m_pendingModelPath.clear();
}void MainWindow::onModelLoadCanceled()
{
    qWarning() << "[MainWindow] Model loading canceled by user";
    
    if (m_modelLoaderThread) {
        m_modelLoaderThread->cancel();
        // Don't wait here - let it finish asynchronously
    }
    
    if (m_loadProgressTimer) {
        m_loadProgressTimer->stop();
    }
    
    statusBar()->showMessage("❌ Model load canceled", 3000);
    m_pendingModelPath.clear();
}

void MainWindow::applyInferenceSettings()
{
    auto& settings = SettingsManager::instance();
    
    // Read settings or use defaults
    float temperature = settings.getValue("inference/temperature", 0.8f).toFloat();
    float topP = settings.getValue("AI/topP", 0.9f).toFloat(); // Keep AI/topP if not in dialog yet
    int maxTokens = settings.getValue("inference/maxTokens", 512).toInt();
    
    bool cloudEnabled = settings.getValue("aichat/enableCloud", false).toBool();
    QString provider = cloudEnabled ? "Cloud AI" : "Local (GGUF)";
    QString apiKey = settings.getValue("aichat/apiKey", "").toString();
    QString model = settings.getValue("ai/model", "").toString();
    
    qDebug() << "[MainWindow::applyInferenceSettings] Applying:"
             << "temp=" << temperature << "topP=" << topP << "maxTokens=" << maxTokens
             << "provider=" << provider;
    
    // Forward to AgenticEngine
    if (m_agenticEngine) {
        AgenticEngine::GenerationConfig cfg;
        cfg.temperature = temperature;
        cfg.topP = topP;
        cfg.maxTokens = maxTokens;
        
        m_agenticEngine->setGenerationConfig(cfg);
        
        if (!cloudEnabled) {
            m_agenticEngine->setModelSource(AgenticEngine::Local);
        } else {
            m_agenticEngine->setModelSource(AgenticEngine::External);
            
            // Default to OpenAI for now if cloud enabled
            AgenticEngine::Provider providerEnum = AgenticEngine::Provider::OpenAI;
            m_agenticEngine->configureExternalModel(providerEnum, apiKey, model, settings.getValue("aichat/cloudEndpoint", "").toString());
        }
        
        statusBar()->showMessage(
            QString("⚙️ Inference settings updated: %1").arg(provider), 3000);
    }
}

void MainWindow::onChatMessageSent(const QString& message)
{
    // This slot is called when ChatInterface emits messageSent
    // We enhance the message with editor context before sending to AgenticEngine
    
    AIChatPanel* panel = qobject_cast<AIChatPanel*>(sender());
    if (!panel) panel = m_currentChatPanel; // Fallback
    
    QString editorContext;
    if (m_multiTabEditor) {
        editorContext = m_multiTabEditor->getSelectedText();
        if (editorContext.isEmpty() && m_multiTabEditor->getCurrentEditor()) {
            // If no selection, use the whole file as context
            editorContext = m_multiTabEditor->getCurrentEditor()->toPlainText();
        }
    }
    
    // FULLY RE-ENABLED: Chat history persistence
    if (m_historyManager && panel) {
        QString sessionId = panel->property("sessionId").toString();
        if (sessionId.isEmpty()) {
            // Try to get from m_currentSessionId if it's the current panel
            if (panel == m_currentChatPanel) sessionId = m_currentSessionId;
        }
        
        if (!sessionId.isEmpty()) {
            m_historyManager->addMessage(sessionId, "user", message);
            qDebug() << "[MainWindow::onChatMessageSent] Saved user message to session:" << sessionId;
        }
    }
    
    // Forward to AgenticEngine with context
    if (m_agenticEngine) {
        // Reset streaming state for this request
        m_streamingActive = false;
        m_agenticEngine->processMessage(message, editorContext, true);
        qDebug() << "[MainWindow::onChatMessageSent] Sent message with"
                 << editorContext.length() << "chars of editor context (streaming=true)";
    } else {
        qWarning() << "[MainWindow::onChatMessageSent] AgenticEngine not initialized";
    }
}

void MainWindow::onInlineEditRequested(const QString& prompt, const QString& selectedCode)
{
    if (!m_agenticEngine) return;
    
    // Show chat dock if hidden
    if (m_chatDock) {
        m_chatDock->show();
        m_chatDock->raise();
    }
    
    // Add message to chat panel to show progress
    if (m_currentChatPanel) {
        m_currentChatPanel->addUserMessage("@inline " + prompt);
        m_currentChatPanel->addAssistantMessage("Processing inline edit...", true);
    }
    
    // Send to engine with @inline prefix
    m_agenticEngine->processMessage("@inline " + prompt, selectedCode, true);
    
    statusBar()->showMessage("Processing inline edit...", 3000);
}

void MainWindow::showInferenceSettings()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Inference Settings");
    dialog->setModal(true);
    dialog->setMinimumWidth(400);
    
    QVBoxLayout *layout = new QVBoxLayout(dialog);
    
    // Temperature setting
    QHBoxLayout *tempLayout = new QHBoxLayout();
    tempLayout->addWidget(new QLabel("Temperature:"));
    QDoubleSpinBox *tempSpin = new QDoubleSpinBox();
    tempSpin->setRange(0.0, 2.0);
    tempSpin->setSingleStep(0.1);
    QSettings settings("RawrXD", "AgenticIDE");
    tempSpin->setValue(settings.value("AI/temperature", 0.8).toDouble());
    tempLayout->addWidget(tempSpin);
    layout->addLayout(tempLayout);
    
    // Top-P setting
    QHBoxLayout *topPLayout = new QHBoxLayout();
    topPLayout->addWidget(new QLabel("Top-P:"));
    QDoubleSpinBox *topPSpin = new QDoubleSpinBox();
    topPSpin->setRange(0.0, 1.0);
    topPSpin->setSingleStep(0.05);
    topPSpin->setValue(settings.value("AI/topP", 0.9).toDouble());
    topPLayout->addWidget(topPSpin);
    layout->addLayout(topPLayout);
    
    // Max Tokens setting
    QHBoxLayout *tokensLayout = new QHBoxLayout();
    tokensLayout->addWidget(new QLabel("Max Tokens:"));
    QSpinBox *tokensSpin = new QSpinBox();
    tokensSpin->setRange(1, 4096);
    tokensSpin->setValue(settings.value("AI/maxTokens", 512).toInt());
    tokensLayout->addWidget(tokensSpin);
    layout->addLayout(tokensLayout);
    
    // Backend selection
    QHBoxLayout *backendLayout = new QHBoxLayout();
    backendLayout->addWidget(new QLabel("Backend:"));
    QComboBox *backendCombo = new QComboBox();
    backendCombo->addItems({"Auto", "CPU", "GPU (Vulkan)", "GPU (CUDA)"});
    QString savedBackend = settings.value("AI/backend", "Auto").toString();
    int backendIdx = backendCombo->findText(savedBackend);
    if (backendIdx >= 0) backendCombo->setCurrentIndex(backendIdx);
    backendLayout->addWidget(backendCombo);
    layout->addLayout(backendLayout);
    
    // Buttons
    QDialogButtonBox *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, [=]() {
        // Save settings
        QSettings s("RawrXD", "AgenticIDE");
        s.setValue("AI/temperature", tempSpin->value());
        s.setValue("AI/topP", topPSpin->value());
        s.setValue("AI/maxTokens", tokensSpin->value());
        s.setValue("AI/backend", backendCombo->currentText());
        
        applyInferenceSettings();
        dialog->accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::reject);
    layout->addWidget(buttons);
    
    dialog->exec();
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
    SettingsDialog dialog(this);
    connect(&dialog, &SettingsDialog::settingsApplied, this, &MainWindow::settingsApplied, Qt::UniqueConnection);
    dialog.initialize();
    dialog.exec();
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
    if (!m_todoManager) return;
    
    // Get current project directory (use current working directory)
    QString projectDir = QDir::currentPath();
    
    // Allow user to select directory
    QString selectedDir = QFileDialog::getExistingDirectory(
        this,
        "Select Project Directory to Scan",
        projectDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (selectedDir.isEmpty()) return;
    projectDir = selectedDir;
    // Confirm scan
    auto reply = QMessageBox::question(this, "Scan for TODOs",
        QString("Scan all source files in:\n%1\n\nfor TODO/FIXME/XXX comments?").arg(projectDir),
        QMessageBox::Yes | QMessageBox::Cancel);
    
    if (reply != QMessageBox::Yes) return;
    
    // Scan recursively
    int foundCount = 0;
    QStringList filters;
    filters << "*.cpp" << "*.h" << "*.hpp" << "*.c" << "*.cc" << "*.cxx"
            << "*.py" << "*.js" << "*.ts" << "*.java" << "*.cs" << "*.rs"
            << "*.go" << "*.rb" << "*.php" << "*.swift" << "*.kt" << "*.scala"
            << "*.md" << "*.txt" << "*.cmake" << "CMakeLists.txt";
    
    QDirIterator it(projectDir, filters, QDir::Files | QDir::NoSymLinks,
                    QDirIterator::Subdirectories);
    
    QRegularExpression todoRegex(
        R"((//|#|;|<!--|/\*)\s*(TODO|FIXME|XXX|HACK|NOTE|BUG)(:|\s+)(.*))",
        QRegularExpression::CaseInsensitiveOption
    );
    
    while (it.hasNext()) {
        QString filePath = it.next();
        if (filePath.contains("/build/") || filePath.contains("\\\\build\\\\") ||
            filePath.contains("/build_") || filePath.contains("\\\\build_\\") ||
            filePath.contains("/.git/") || filePath.contains("\\\\.git\\\\")) continue;
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QTextStream in(&file);
        int lineNum = 0;
        while (!in.atEnd()) {
            lineNum++;
            QString line = in.readLine();
            QRegularExpressionMatch match = todoRegex.match(line);
            if (match.hasMatch()) {
                QString todoType = match.captured(2).toUpper();
                QString todoText = match.captured(4).trimmed();
                if (todoText.isEmpty()) todoText = QString("[%1]").arg(todoType);
                else todoText = QString("[%1] %2").arg(todoType, todoText);
                m_todoManager->addTodo(todoText, filePath, lineNum);
                foundCount++;
            }
        }
        file.close();
    }
    statusBar()->showMessage(QString("Scan complete: %1 TODO items found").arg(foundCount), 5000);
    QMessageBox::information(this, "Scan Complete",
        QString("Found %1 TODO/FIXME/XXX comments.\n\nItems added to TODO panel.").arg(foundCount));
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

// ============================================================================
// PHASE 2 POLISH FEATURES IMPLEMENTATION
// ============================================================================

void MainWindow::initializePhase2Polish()
{
    qDebug() << "[MainWindow] 🎨 Initializing Phase 2 Polish Features...";
    
    // ===== 1. DIFF PREVIEW DOCK (Lazy Loaded) =====
    try {
        // Just register the dock, don't create the widget yet
        m_diffPreviewDock = nullptr; // We'll use this as a flag or just check the dock widget
        // Wait, m_diffPreviewDock is a DiffDock* which IS a QDockWidget.
        // I'll change it to be created on demand.
        qDebug() << "  ✓ Diff Preview Dock registered (Lazy Loading enabled)";
        
    } catch (const std::exception& e) {
        qWarning() << "[MainWindow] Failed to init diff preview:" << e.what();
    }
    
    // ===== 2. STREAMING TOKEN PROGRESS (Already in AIChatPanel) =====
    // Connect AgenticEngine token signal to AIChatPanel progress
    if (m_agenticEngine) {
        // Token progress would be handled within each AIChatPanel
        qDebug() << "  ✓ Token progress available in chat panels";
    }
    
    // ===== 3. GPU BACKEND SELECTOR =====
    try {
        QToolBar* aiToolbar = nullptr;
        
        // Find existing AI toolbar or create new one
        for (QToolBar* toolbar : findChildren<QToolBar*>()) {
            if (toolbar->windowTitle() == "AI") {
                aiToolbar = toolbar;
                break;
            }
        }
        
        if (!aiToolbar) {
            aiToolbar = addToolBar("AI Settings");
        }
        
        m_backendSelector = new RawrXD::GPUBackendSelector(this);
        aiToolbar->addSeparator();
        aiToolbar->addWidget(new QLabel(" Backend: ", this));
        aiToolbar->addWidget(m_backendSelector);
        
        // Connect backend changes to inference engine
        connect(m_backendSelector, &RawrXD::GPUBackendSelector::backendChanged,
                this, [this](RawrXD::ComputeBackend backend) {
            QString backendName;
            switch (backend) {
                case RawrXD::ComputeBackend::CUDA: backendName = "CUDA"; break;
                case RawrXD::ComputeBackend::Vulkan: backendName = "Vulkan"; break;
                case RawrXD::ComputeBackend::CPU: backendName = "CPU"; break;
                case RawrXD::ComputeBackend::DirectML: backendName = "DirectML"; break;
                default: backendName = "Auto"; break;
            }
            
            qDebug() << "[MainWindow] Backend switched to:" << backendName;
            statusBar()->showMessage("✓ Backend: " + backendName, 3000);

            // Update latency monitor
            if (m_latencyMonitor) m_latencyMonitor->setBackend(backendName);
        });
        
        qDebug() << "  ✓ GPU Backend Selector initialized";
        
    } catch (const std::exception& e) {
        qWarning() << "[MainWindow] Failed to init backend selector:" << e.what();
    }
    
    // ===== 4. AUTO MODEL DOWNLOAD =====
    try {
        QTimer::singleShot(1500, this, [this]() {
            RawrXD::AutoModelDownloader downloader;
            
            if (!downloader.hasLocalModels()) {
                qDebug() << "[MainWindow] No models detected - offering download";
                showModelDownloadDialog();
            }
        });
        
        qDebug() << "  ✓ Auto Model Download scheduled";
        
    } catch (const std::exception& e) {
        qWarning() << "[MainWindow] Failed to init model downloader:" << e.what();
    }    // ===== 5. TELEMETRY OPT-IN =====
    try {
        QTimer::singleShot(2500, this, [this]() {
            if (!RawrXD::hasTelemetryPreference()) {
                qDebug() << "[MainWindow] No telemetry preference - showing opt-in dialog";
                
                RawrXD::TelemetryOptInDialog* dialog = new RawrXD::TelemetryOptInDialog(this);
                
                connect(dialog, &RawrXD::TelemetryOptInDialog::telemetryDecisionMade,
                        this, [this](bool enabled) {
                    qDebug() << "[MainWindow] Telemetry decision:" << (enabled ? "ENABLED" : "DISABLED");
                    statusBar()->showMessage(enabled ? 
                        "✓ Thank you for helping improve RawrXD IDE!" : 
                        "Telemetry disabled", 
                        5000);
                    // Apply telemetry preference immediately
                    GetTelemetry().enableTelemetry(enabled);
                    if (enabled) {
                        telemetry::InitializeHardware();
                        QJsonObject meta; meta["opt_in"] = true;
                        GetTelemetry().recordEvent("telemetry_opt_in", meta);
                    } else {
                        QJsonObject meta; meta["opt_in"] = false;
                        GetTelemetry().recordEvent("telemetry_opt_out", meta);
                    }
                });
                
                dialog->exec();
                dialog->deleteLater();
            }
        });
        
        qDebug() << "  ✓ Telemetry Opt-In scheduled";
        
    } catch (const std::exception& e) {
        qWarning() << "[MainWindow] Failed to init telemetry:" << e.what();
    }
    
    qDebug() << "[MainWindow] ✅ Phase 2 Polish Features initialized";
}

void MainWindow::onRefactorSuggested(const QString &original, const QString &suggested)
{
    if (!m_diffPreviewDock) {
        qDebug() << "[MainWindow] Lazy loading Diff Preview Dock...";
        m_diffPreviewDock = new DiffDock(this);
        addDockWidget(Qt::RightDockWidgetArea, m_diffPreviewDock);
        
        // Connect accept button - apply changes to editor
        connect(m_diffPreviewDock, &DiffDock::accepted, this,
                [this](const QString &text) {
            if (m_multiTabEditor && m_multiTabEditor->getCurrentEditor()) {
                auto cursor = m_multiTabEditor->getCurrentEditor()->textCursor();
                cursor.beginEditBlock();
                cursor.select(QTextCursor::BlockUnderCursor);
                cursor.insertText(text);
                cursor.endEditBlock();
                m_diffPreviewDock->hide();
                statusBar()->showMessage("✓ Refactor applied", 3000);
                qDebug() << "[MainWindow] Refactor accepted and applied";
            }
        });
        
        // Connect reject button
        connect(m_diffPreviewDock, &DiffDock::rejected, this,
                [this]() {
            m_diffPreviewDock->hide();
            statusBar()->showMessage("✗ Refactor rejected", 2000);
            qDebug() << "[MainWindow] Refactor rejected";
        });
    }

    if (m_diffPreviewDock) {
        m_diffPreviewDock->setDiff(original, suggested);
        m_diffPreviewDock->show();
        m_diffPreviewDock->raise();
        qDebug() << "[MainWindow] Refactor suggestion shown in diff dock";
    }
}

void MainWindow::showModelDownloadDialog()
{
    RawrXD::ModelDownloadDialog* dialog = new RawrXD::ModelDownloadDialog(this);
    
    if (dialog->exec() == QDialog::Accepted) {
        statusBar()->showMessage("✓ Model downloaded! Refreshing model list...", 5000);
        qDebug() << "[MainWindow] Model downloaded successfully";
        
        // Refresh models in all chat panels
        if (m_chatTabs) {
            for (int i = 0; i < m_chatTabs->count(); ++i) {
                if (auto* panel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(i))) {
                    // Trigger model list refresh
                    QMetaObject::invokeMethod(panel, "fetchAvailableModels", Qt::QueuedConnection);
                }
            }
            qDebug() << "  ✓ Refreshing models in chat panels";
        }
    } else {
        statusBar()->showMessage(
            "ℹ No models installed. Use AI → Download Model to get started", 
            10000);
        qDebug() << "[MainWindow] User skipped model download";
    }
    
    dialog->deleteLater();
}

void MainWindow::showThemeConfiguration() {
    if (m_themeDock) {
        if (!m_themeDock->widget()) {
            qDebug() << "[MainWindow] Lazy loading Theme Configuration...";
            auto* themePanel = new RawrXD::ThemeConfigurationPanel(this);
            m_themeDock->setWidget(themePanel);
            connect(themePanel, &RawrXD::ThemeConfigurationPanel::themeChanged, this, &MainWindow::onThemeChanged);
        }
        m_themeDock->setVisible(!m_themeDock->isVisible());
        if (m_themeDock->isVisible()) {
            m_themeDock->raise();
            statusBar()->showMessage("🎨 Theme Configuration opened", 2000);
        }
    }
}

void MainWindow::showTransparencyControls() {
    if (m_transparencyDock) {
        if (!m_transparencyDock->widget()) {
            qDebug() << "[MainWindow] Lazy loading Transparency Controls...";
            auto* transparencyPanel = new RawrXD::TransparencyControlPanel(this);
            m_transparencyDock->setWidget(transparencyPanel);
            connect(transparencyPanel, &RawrXD::TransparencyControlPanel::opacityChanged,
                    [](const QString& element, double opacity) {
                        qDebug() << "[MainWindow] Opacity changed:" << element << opacity;
                    });
        }
        m_transparencyDock->setVisible(!m_transparencyDock->isVisible());
        if (m_transparencyDock->isVisible()) {
            m_transparencyDock->raise();
            statusBar()->showMessage("🔮 Transparency Controls opened", 2000);
        }
    }
}

void MainWindow::onThemeChanged() {
    // Update all themed widgets
    if (m_multiTabEditor) {
        // m_multiTabEditor is a QWidget wrapper, we need to get tabs another way
        // For now, just apply theme to current editor
        if (auto* editor = m_multiTabEditor->getCurrentEditor()) {
            if (auto* themedEditor = qobject_cast<RawrXD::ThemedCodeEditor*>(editor)) {
                themedEditor->applyTheme();
            }
        }
    }
    
    // Update chat panels
    if (m_chatTabs) {
        for (int i = 0; i < m_chatTabs->count(); ++i) {
            if (auto* chatPanel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(i))) {
                chatPanel->applyTheme();
                qDebug() << "[MainWindow] Applying theme to chat panel" << i;
            }
        }
    }
    
    statusBar()->showMessage("🎨 Theme applied successfully", 2000);
}

AIChatPanel* MainWindow::createNewChatPanel()
{
    AIChatPanel* panel = new AIChatPanel(this);
    if (!panel) {
        qWarning() << "[MainWindow] Failed to create new AIChatPanel";
        return nullptr;
    }
    
    // Check if a model is already loaded BEFORE initialize() schedules fetchAvailableModels
    QString preloadedModel;
    if (m_inferenceEngine && m_inferenceEngine->isModelLoaded()) {
        QString modelPath = m_pendingModelPath.isEmpty() ? m_inferenceEngine->modelPath() : m_pendingModelPath;
        if (!modelPath.isEmpty()) {
            preloadedModel = QFileInfo(modelPath).fileName();
        }
    }
    
    panel->initialize(preloadedModel);  // Pass preloaded model to skip input disable
    
    int idx = m_chatTabs->addTab(panel, tr("Chat %1").arg(m_chatTabs->count() + 1));
    m_chatTabs->setCurrentIndex(idx);
    m_currentChatPanel = panel;
    
    // ===== FULLY RE-ENABLED: ChatHistoryManager integration =====
    if (m_historyManager) {
        // Set history manager on the panel
        panel->setHistoryManager(m_historyManager);
        qDebug() << "[MainWindow] ChatHistoryManager attached to panel";
        
        // Create new session for this chat panel
        QString sessionTitle = tr("Chat %1").arg(m_chatTabs->count());
        m_currentSessionId = m_historyManager->createSession(sessionTitle);
        qInfo() << "[MainWindow] Created new chat session:" << m_currentSessionId;
        
        // Pass session ID to panel for checkpoint management
        panel->setCurrentSessionId(m_currentSessionId);
        panel->setProperty("sessionId", m_currentSessionId);
        
        // Enable auto-checkpoint with 5 minute interval
        panel->enableAutoCheckpoint(true, 5);
        qDebug() << "[MainWindow] Auto-checkpoint enabled for session:" << m_currentSessionId;
    }
    
    // ✅ CRITICAL: Wire inference engine to panel for GGUF inference pipeline
    if (m_inferenceEngine) {
        panel->setInferenceEngine(m_inferenceEngine);
        qInfo() << "[MainWindow] ✓ Inference engine wired to chat panel - ALL AI requests will use GGUF pipeline";
    } else {
        qWarning() << "[MainWindow] ⚠ Inference engine not available - chat will not have local model support";
    }
    
    qDebug() << "[MainWindow] Created new chat panel at index" << idx << "with session" << m_currentSessionId;
    
    // Connect signals for this panel
    connect(panel, &AIChatPanel::messageSubmitted,
        this, &MainWindow::onChatMessageSent);
    
    connect(panel, &AIChatPanel::newChatRequested,
        this, [this]() { createNewChatPanel(); });
    
    connect(panel, &AIChatPanel::loadModelRequested,
        this, &MainWindow::loadModel);
    
    connect(panel, &AIChatPanel::settingsRequested,
        this, &MainWindow::showPreferences);
    
    connect(panel, &AIChatPanel::codeInsertRequested,
        this, [this](const QString& code) {
            if (m_multiTabEditor && m_multiTabEditor->getCurrentEditor()) {
                m_multiTabEditor->getCurrentEditor()->insertCode(code);
                statusBar()->showMessage("✓ Code inserted from AI", 3000);
            }
        });
    
    // If a model is already loaded, enable the chat input immediately
    if (m_inferenceEngine && m_inferenceEngine->isModelLoaded()) {
        // Get model path from either m_pendingModelPath or inference engine's loaded path
        QString modelPath = m_pendingModelPath;
        if (modelPath.isEmpty() && m_inferenceEngine) {
            // Try to get path from inference engine if pending path not available
            modelPath = m_inferenceEngine->modelPath();
        }
        
        if (!modelPath.isEmpty()) {
            QString modelDisplay = QFileInfo(modelPath).fileName();
            panel->setLocalModel(modelDisplay);
            panel->setSelectedModel(modelDisplay);
            panel->setInputEnabled(true);
            qInfo() << "[MainWindow] Chat panel initialized with loaded model:" << modelDisplay;
        } else {
            // Model is loaded but path unknown - still enable input
            panel->setLocalModel("custom-local-model");
            panel->setInputEnabled(true);
            qInfo() << "[MainWindow] Chat panel enabled with unknown model path";
        }
    }
    
    // Wire sessionSelected signal (for history loading when available)
    connect(panel, &AIChatPanel::sessionSelected, this, [this, panel](const QString& sessionId) {
        if (m_historyManager) {
            // Load messages from history
            auto messages = m_historyManager->getMessages(sessionId);
            qDebug() << "[MainWindow] Loading" << messages.size() << "messages from history";
            
            // Clear current panel and restore history
            panel->clear();
            for (const auto& msgVal : messages) {
                QJsonObject msgObj = msgVal.toObject();
                QString role = msgObj[QStringLiteral("role")].toString();
                QString content = msgObj[QStringLiteral("content")].toString();
                
                if (role == "user") {
                    panel->addUserMessage(content);
                } else if (role == "assistant") {
                    panel->addAssistantMessage(content, false);
                }
            }
            
            m_currentSessionId = sessionId;
            qInfo() << "[MainWindow] Session restored with" << messages.size() << "messages";
        }
    });
    
    // Wire messageSubmitted signal to route through MainWindow for persistence
    connect(panel, &AIChatPanel::messageSubmitted,
        this, [this, panel](const QString& message) {
            // Save user message to history immediately
            if (m_historyManager && !m_currentSessionId.isEmpty()) {
                if (!m_historyManager->addMessage(m_currentSessionId, "user", message)) {
                    qWarning() << "[MainWindow] Failed to save user message to history";
                }
            }
            // DO NOT call onChatMessageSent here - it's already connected above
            // This prevents duplicate message processing
        });
    
    // Wire modelSelected signal so MainWindow loads model into AgenticEngine
    connect(panel, &AIChatPanel::modelSelected,
        this, [this](const QString& modelName) {
            // Validate model name is not empty or placeholder
            if (modelName.isEmpty() || 
                modelName == "Loading models..." || 
                modelName == "Select a model..." ||
                modelName == "No models available" ||
                modelName == "Error loading models") {
                qDebug() << "[MainWindow] Ignoring invalid model selection:" << modelName;
                return;
            }
            
            qDebug() << "[MainWindow] Chat panel selected model:" << modelName;
            // model name might be path or name - try to resolve
            QString ggufPath = modelName;
            if (!QFile::exists(ggufPath)) {
                // Try resolving as Ollama model name
                if (m_inferenceEngine) {
                    QStringList ollamaModels = m_inferenceEngine->detectedOllamaModels();
                    if (ollamaModels.contains(modelName)) {
                        qInfo() << "[MainWindow] Detected Ollama model selection:" << modelName << "- routing to Ollama fallback";
                        m_inferenceEngine->setOllamaModel(modelName);
                        if (m_agenticEngine) {
                            m_agenticEngine->markModelAsLoaded(modelName);
                        }
                        applyChatModelSelection(modelName);
                        return;
                    }
                }

                // Try to resolve as local blob path fallback
                ggufPath = QString("D:/OllamaModels/%1").arg(modelName);
                if (!QFile::exists(ggufPath)) {
                    qWarning() << "[MainWindow] Could not resolve model path for:" << modelName;
                    return;
                }
            }
            onModelSelected(ggufPath);
        });
    
    return panel;
}

void MainWindow::toggleDiagnosticPanel()
{
    if (!m_diagnosticDock) {
        m_diagnosticDock = new QDockWidget("🔍 Diagnostics", this);
        m_diagnosticDock->setObjectName("diagnosticDock");
        m_diagnosticPanel = new DiagnosticPanel(this);
        m_diagnosticDock->setWidget(m_diagnosticPanel);
        addDockWidget(Qt::BottomDockWidgetArea, m_diagnosticDock);
    }
    
    if (m_diagnosticDock->isVisible()) {
        m_diagnosticDock->hide();
        statusBar()->showMessage("Diagnostic panel hidden", 2000);
    } else {
        m_diagnosticDock->show();
        m_diagnosticDock->raise();
        statusBar()->showMessage("🔍 Diagnostic panel opened", 2000);
    }
}

} // namespace RawrXD




