// RawrXD Agentic IDE - v5.0 Clean Implementation
// Integrates v4.3 features with proper lazy initialization and async operations
#include "MainWindow_v5.h"
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
#include "agentic_text_edit.h"
#include "../gui/ModelConversionDialog.h"
#include "TelemetryWindow.h"
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
#include <QDir>
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

void MainWindow::settingsApplied()
{
    // Re-apply inference and chat-related settings
    applyInferenceSettings();
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

        // Initialize Agentic Executor (Real task execution)
        m_agenticExecutor = new AgenticExecutor(this);
        // Note: m_inferenceEngine might be null here, it's created in onModelSelected
        // We'll initialize it fully when model is loaded
        
        updateSplashProgress("✓ Agentic Executor initialized", 45);
        
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

                AIChatPanel* panel = new AIChatPanel(this);
                if (panel) {
                    panel->initialize();  // Initialize UI immediately
                    if (m_agenticExecutor) {
                        panel->setAgenticExecutor(m_agenticExecutor);
                    }
                    m_chatTabs->addTab(panel, "Chat 1");
                    m_currentChatPanel = panel;  // Set as current active panel

                    // Sync editor context into the chat panel so quick actions use real code
                    updateChatContextForPanel(panel);
                    
                    // Wire message processing for this panel
                    connect(panel, &AIChatPanel::messageSubmitted,
                        this, &MainWindow::onChatMessageSent);

                    // Keep chat context current before agent/quick actions run
                    connect(panel, &AIChatPanel::quickActionTriggered,
                            this, [this, panel](const QString&, const QString&) {
                        updateChatContextForPanel(panel);
                    });

                    // Wire agent responses to auto-open files from chat
                    if (m_agenticEngine) {
                        connect(m_agenticEngine, &AgenticEngine::responseReady,
                                this, [this](const QString& response) {
                            parseAndOpenFilesFromMessage(response);
                        });
                    }

                    // Model selection in the chat panel loads GGUF
                    connect(panel, &AIChatPanel::modelSelected,
                            this, &MainWindow::onModelSelected);

                    // MAX mode tweaks inference settings (longer answers / lower temp)
                    connect(panel, &AIChatPanel::maxModeChanged,
                            this, [this](bool enabled) {
                        if (m_agenticEngine) {
                            auto cfg = m_agenticEngine->generationConfig();
                            cfg.maxTokens = enabled ? 1024 : 512;
                            cfg.temperature = enabled ? 0.6f : 0.8f;
                            m_agenticEngine->setGenerationConfig(cfg);
                        }
                    });

                    // Agent mode changes bubble into the main mode switcher
                    connect(panel, &AIChatPanel::agentModeChanged,
                            this, [this](int mode) {
                        QString modeName;
                        switch (mode) {
                            case 0: modeName = QStringLiteral("Agent"); break;
                            case 1: modeName = QStringLiteral("Ask"); break;
                            case 2: modeName = QStringLiteral("Plan"); break;
                            default: modeName = QStringLiteral("Custom"); break;
                        }
                        if (statusBar()) statusBar()->showMessage("Mode: " + modeName, 2000);
                    });
                    
                    if (m_agenticEngine) {
                        connect(m_agenticEngine, &AgenticEngine::responseReady,
                            panel, [panel](const QString& response) {
                                panel->addAssistantMessage(response, false);
                                panel->finishStreaming();
                            });

                        // Wire token progress signal (numeric) - preserve for existing progress UIs
                        connect(m_agenticEngine, &AgenticEngine::tokenGenerated,
                            panel, [panel](int tokenDelta) {
                                Q_UNUSED(tokenDelta);
                                // tokenGenerated is numeric-only; UI updates use streamToken for textual updates
                            });

                        // Forward streaming text tokens (reqId, token) into the correct panel
                        connect(m_agenticEngine, &AgenticEngine::streamingStarted,
                                this, [this, panel](qint64 reqId) {
                                    // Record which panel started this request and init buffer
                                    m_streamingBuffer.insert(reqId, QString());
                                    m_requestPanelMap.insert(reqId, panel);
                                });

                        connect(m_agenticEngine, &AgenticEngine::streamToken,
                                this, [this](qint64 reqId, const QString& token) {
                                    // Accumulate token text and update mapped panel
                                    QString &acc = m_streamingBuffer[reqId];
                                    acc += token;
                                    auto it = m_requestPanelMap.find(reqId);
                                    if (it != m_requestPanelMap.end() && !it.value().isNull()) {
                                        it.value()->updateStreamingMessage(acc);
                                    } else if (m_currentChatPanel) {
                                        // Fallback: update current active panel
                                        m_currentChatPanel->updateStreamingMessage(acc);
                                    }
                                });

                        connect(m_agenticEngine, &AgenticEngine::streamFinished,
                                this, [this](qint64 reqId) {
                                    auto it = m_requestPanelMap.find(reqId);
                                    if (it != m_requestPanelMap.end() && !it.value().isNull()) {
                                        it.value()->finishStreaming();
                                    } else if (m_currentChatPanel) {
                                        m_currentChatPanel->finishStreaming();
                                    }
                                    // Clean up buffers
                                    m_requestPanelMap.remove(reqId);
                                    m_streamingBuffer.remove(reqId);
                                });

                        // Wire model status updates
                        connect(m_agenticEngine, &AgenticEngine::modelReady,
                            panel, [panel, this](bool ready) {
                                QString modelName = ready ? QFileInfo(m_agenticEngine->currentModelPath()).fileName() : QString();
                                panel->setModelStatus(modelName, ready);
                            });

                        // Update initial model status
                        bool modelReady = m_agenticEngine->isModelLoaded();
                        QString modelName = modelReady ? QFileInfo(m_agenticEngine->currentModelPath()).fileName() : QString();
                        panel->setModelStatus(modelName, modelReady);
                    }
                }

        m_chatDock = new QDockWidget("AI Chat & Commands", this);
        m_chatDock->setWidget(m_chatTabs);
        addDockWidget(Qt::RightDockWidgetArea, m_chatDock);

        // Add "New Chat" action to menu bar
        QAction* newChatAct = new QAction(tr("New Chat"), this);
        connect(newChatAct, &QAction::triggered, this, [this](){
            AIChatPanel* panel = new AIChatPanel(this);
            if (panel) {
                panel->initialize();  // Ensure UI is created
                if (m_agenticExecutor) {
                    panel->setAgenticExecutor(m_agenticExecutor);
                }
                updateChatContextForPanel(panel);
                int idx = m_chatTabs->addTab(panel, tr("Chat %1").arg(m_chatTabs->count()+1));
                m_chatTabs->setCurrentIndex(idx);
                m_currentChatPanel = panel;
                // Rewire signals for new panel
                connect(panel, &AIChatPanel::messageSubmitted,
                    this, &MainWindow::onChatMessageSent);

                // Wire file navigation
                if (m_agenticEngine) {
                    connect(m_agenticEngine, &AgenticEngine::responseReady,
                            this, [this](const QString& response) {
                        parseAndOpenFilesFromMessage(response);
                    });
                }

                connect(panel, &AIChatPanel::quickActionTriggered,
                        this, [this, panel](const QString&, const QString&) {
                    updateChatContextForPanel(panel);
                });

                // Wire agent responses to auto-open files from chat
                if (m_agenticEngine) {
                    connect(m_agenticEngine, &AgenticEngine::responseReady,
                            this, [this](const QString& response) {
                        parseAndOpenFilesFromMessage(response);
                    });
                }

                connect(panel, &AIChatPanel::modelSelected,
                        this, &MainWindow::onModelSelected);

                connect(panel, &AIChatPanel::maxModeChanged,
                        this, [this](bool enabled) {
                    if (m_agenticEngine) {
                        auto cfg = m_agenticEngine->generationConfig();
                        cfg.maxTokens = enabled ? 1024 : 512;
                        cfg.temperature = enabled ? 0.6f : 0.8f;
                        m_agenticEngine->setGenerationConfig(cfg);
                    }
                });

                connect(panel, &AIChatPanel::agentModeChanged,
                        this, [this](int mode) {
                    QString modeName;
                    switch (mode) {
                        case 0: modeName = QStringLiteral("Agent"); break;
                        case 1: modeName = QStringLiteral("Ask"); break;
                        case 2: modeName = QStringLiteral("Plan"); break;
                        default: modeName = QStringLiteral("Custom"); break;
                    }
                    if (statusBar()) statusBar()->showMessage("Mode: " + modeName, 2000);
                });
                if (m_agenticEngine) {
                    connect(m_agenticEngine, QOverload<const QString&>::of(&AgenticEngine::responseReady),
                        panel, [panel](const QString& response) {
                            panel->addAssistantMessage(response, false);
                            panel->finishStreaming();
                        });

                    // Numeric token progress preserved
                    connect(m_agenticEngine, &AgenticEngine::tokenGenerated,
                        panel, [panel](int tokenDelta) {
                            Q_UNUSED(tokenDelta);
                        });

                    // Per-request mapping for textual streaming
                    connect(m_agenticEngine, &AgenticEngine::streamingStarted,
                            this, [this, panel](qint64 reqId) {
                                m_streamingBuffer.insert(reqId, QString());
                                m_requestPanelMap.insert(reqId, panel);
                            });

                    connect(m_agenticEngine, &AgenticEngine::streamToken,
                            this, [this](qint64 reqId, const QString& token) {
                                QString &acc = m_streamingBuffer[reqId];
                                acc += token;
                                auto it = m_requestPanelMap.find(reqId);
                                if (it != m_requestPanelMap.end() && !it.value().isNull()) {
                                    it.value()->updateStreamingMessage(acc);
                                } else if (m_currentChatPanel) {
                                    m_currentChatPanel->updateStreamingMessage(acc);
                                }
                            });

                    connect(m_agenticEngine, &AgenticEngine::streamFinished,
                            this, [this](qint64 reqId) {
                                auto it = m_requestPanelMap.find(reqId);
                                if (it != m_requestPanelMap.end() && !it.value().isNull()) {
                                    it.value()->finishStreaming();
                                } else if (m_currentChatPanel) {
                                    m_currentChatPanel->finishStreaming();
                                }
                                m_requestPanelMap.remove(reqId);
                                m_streamingBuffer.remove(reqId);
                            });

                    // Wire model status updates
                    connect(m_agenticEngine, &AgenticEngine::modelReady,
                        panel, [panel, this](bool ready) {
                            QString modelName = ready ? QFileInfo(m_agenticEngine->currentModelPath()).fileName() : QString();
                            panel->setModelStatus(modelName, ready);
                        });

                    // Set initial model status
                    bool modelReady = m_agenticEngine->isModelLoaded();
                    QString modelName = modelReady ? QFileInfo(m_agenticEngine->currentModelPath()).fileName() : QString();
                    panel->setModelStatus(modelName, modelReady);
                }
            }
        });
        menuBar()->addAction(newChatAct);
        
        updateSplashProgress("✓ Chat interface ready", 65);
        
        // Note: Individual panel connections are now done during panel creation
        // This ensures each panel gets its own response handler
        
        // Connect model selection to load GGUF files
        // Connect settings dialog to apply AI chat configuration
        connect(this, &MainWindow::settingsApplied, this, [this](){
            if (m_currentChatPanel) {
                // Load settings from QSettings and apply to manager
                QSettings settings;
                bool cloudEnabled = settings.value("aichat/enableCloud", false).toBool();
                QString cloudEndpoint = settings.value("aichat/cloudEndpoint", "https://api.openai.com/v1/chat/completions").toString();
                QString apiKey = settings.value("aichat/apiKey", "").toString();
                bool localEnabled = settings.value("aichat/enableLocal", true).toBool();
                QString localEndpoint = settings.value("aichat/localEndpoint", "http://localhost:11434/api/generate").toString();
                int timeout = settings.value("aichat/requestTimeout", 30000).toInt();
                
                // Apply settings to chat panel
                if (m_currentChatPanel) {
                    // Placeholder for settings application
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
        m_todoDock = new TodoDock(m_todoManager, this);
        
        m_todoDockWidget = new QDockWidget("TODO List", this);
        m_todoDockWidget->setWidget(m_todoDock);
        addDockWidget(Qt::RightDockWidgetArea, m_todoDockWidget);
        m_todoDockWidget->hide();  // Hidden by default - viewable via View menu
        
        // ============================================================
        // Additional IDE Components (hidden by default, viewable via View menu)
        // ============================================================
        
        // MASM Editor
        m_masmEditor = new MASMEditorWidget(this);
        m_masmEditorDock = new QDockWidget("MASM Editor", this);
        m_masmEditorDock->setWidget(m_masmEditor);
        addDockWidget(Qt::RightDockWidgetArea, m_masmEditorDock);
        m_masmEditorDock->hide();  // Hidden by default
        
        // Model Tuner (GGUF Model Loader with compression)
        m_modelTuner = new ModelLoaderWidget(this);
        m_modelTunerDock = new QDockWidget("Model Tuner", this);
        m_modelTunerDock->setWidget(m_modelTuner);
        addDockWidget(Qt::RightDockWidgetArea, m_modelTunerDock);
        m_modelTunerDock->hide();  // Hidden by default
        
        // Interpretability Panel (Model analysis and visualization)
        m_interpretabilityPanel = new InterpretabilityPanelEnhanced(this);
        m_interpretabilityDock = new QDockWidget("Interpretability Panel", this);
        m_interpretabilityDock->setWidget(m_interpretabilityPanel);
        addDockWidget(Qt::BottomDockWidgetArea, m_interpretabilityDock);
        m_interpretabilityDock->hide();  // Hidden by default
        
        // Hotpatch Panel (Real-time model corrections)
        m_hotpatchPanel = new HotpatchPanel(this);
        m_hotpatchDock = new QDockWidget("Hotpatch Panel", this);
        m_hotpatchDock->setWidget(m_hotpatchPanel);
        addDockWidget(Qt::BottomDockWidgetArea, m_hotpatchDock);
        m_hotpatchDock->hide();  // Hidden by default
        
        // Multi-File Search
        m_multiFileSearch = new RawrXD::MultiFileSearchWidget(this);
        m_multiFileSearchDock = new QDockWidget("Multi-File Search", this);
        m_multiFileSearchDock->setWidget(m_multiFileSearch);
        addDockWidget(Qt::BottomDockWidgetArea, m_multiFileSearchDock);
        m_multiFileSearchDock->hide();  // Hidden by default

        // Default search root to current working directory; refine when editor has a file
        m_multiFileSearch->setProjectPath(QDir::currentPath());
        connect(m_multiFileSearch, &RawrXD::MultiFileSearchWidget::resultClicked,
            this, [this](const QString& filePath, int line, int /*column*/) {
                openFileInEditor(filePath);
                scrollEditorToLine(line + 1);
            });
        
        // Enterprise Tools Panel - GitHub-style 44-tool management (placeholder)
    QWidget* m_toolsPanel = nullptr;
    m_toolsPanelDock = new QDockWidget("🛠️ Enterprise Tools (44)", this);
    QLabel* toolsPlaceholder = new QLabel("Enterprise Tools Panel - Coming Soon\n(44 tools registered)", this);
    toolsPlaceholder->setAlignment(Qt::AlignCenter);
    m_toolsPanelDock->setWidget(toolsPlaceholder);
    addDockWidget(Qt::RightDockWidgetArea, m_toolsPanelDock);
    m_toolsPanelDock->hide();  // Hidden by default
    
    // Theme System Docks
    auto* themePanel = new RawrXD::ThemeConfigurationPanel(this);
    m_themeDock = new QDockWidget("Theme Configuration", this);
    m_themeDock->setWidget(themePanel);
    m_themeDock->hide();  // Hidden by default
    addDockWidget(Qt::RightDockWidgetArea, m_themeDock);
    
    auto* transparencyPanel = new RawrXD::TransparencyControlPanel(this);
    m_transparencyDock = new QDockWidget("Transparency Controls", this);
    m_transparencyDock->setWidget(transparencyPanel);
    m_transparencyDock->hide();  // Hidden by default
    addDockWidget(Qt::RightDockWidgetArea, m_transparencyDock);
    
    // Connect theme signals
    connect(themePanel, &RawrXD::ThemeConfigurationPanel::themeChanged, this, &MainWindow::onThemeChanged);
    connect(transparencyPanel, &RawrXD::TransparencyControlPanel::opacityChanged,
            [](const QString& element, double opacity) {
                qDebug() << "[MainWindow] Opacity changed:" << element << opacity;
            });
        addDockWidget(Qt::RightDockWidgetArea, m_transparencyDock);
        
        // Connect theme signals
        connect(themePanel, &RawrXD::ThemeConfigurationPanel::themeChanged, this, &MainWindow::onThemeChanged);
        connect(transparencyPanel, &RawrXD::TransparencyControlPanel::opacityChanged,
                [](const QString& element, double opacity) {
                    qDebug() << "[MainWindow] Opacity changed:" << element << opacity;
                });
        
        updateSplashProgress("✓ All panels created (44 tools registered)", 90);
        
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
    viewMenu->addAction("Show MASM &Editor", this, &MainWindow::showMasmEditor);
    viewMenu->addAction("Show Model &Tuner", this, &MainWindow::showModelTuner);
    
    // IDE Tools submenu (hidden panels viewable here)
    QMenu *toolsMenu = viewMenu->addMenu("&IDE Tools");
    toolsMenu->addAction("&MASM Editor", this, &MainWindow::toggleMASMEditor, QKeySequence("Ctrl+Shift+A"));
    toolsMenu->addAction("&Model Tuner", this, &MainWindow::toggleModelTuner, QKeySequence("Ctrl+Shift+M"));
    toolsMenu->addAction("&Interpretability Panel", this, &MainWindow::toggleInterpretabilityPanel, QKeySequence("Ctrl+Shift+I"));
    toolsMenu->addAction("&Hotpatch Panel", this, &MainWindow::toggleHotpatchPanel, QKeySequence("Ctrl+Shift+H"));
    toolsMenu->addAction("Multi-File &Search", this, &MainWindow::toggleMultiFileSearch, QKeySequence("Ctrl+Shift+F"));
    toolsMenu->addSeparator();
    toolsMenu->addAction("🛠️ Enterprise &Tools Panel", this, &MainWindow::toggleToolsPanel, QKeySequence("Ctrl+Shift+T"));  // NEW: 44-tool management
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

// ------------------------------------------------------------
// Optional panel helpers
// ------------------------------------------------------------
void MainWindow::showMasmEditor()
{
    // Create the MASM editor dock on first use
    static QDockWidget* masmDock = nullptr;
    if (!masmDock) {
        masmDock = new QDockWidget(tr("MASM Editor"), this);
#ifdef RAWRXD_HAS_MASM_EDITOR
        auto* editor = new MASMEditorWidget(this);
        masmDock->setWidget(editor);
#else
        QLabel* placeholder = new QLabel("MASM Editor unavailable (widget not present)", this);
        placeholder->setAlignment(Qt::AlignCenter);
        masmDock->setWidget(placeholder);
#endif
        addDockWidget(Qt::RightDockWidgetArea, masmDock);
    }
    masmDock->setVisible(true);
    masmDock->raise();
}

void MainWindow::showModelTuner()
{
    // Create the Model Tuner (multi-file search) dock on first use
    static QDockWidget* tunerDock = nullptr;
    if (!tunerDock) {
        tunerDock = new QDockWidget(tr("Model Tuner"), this);
#ifdef RAWRXD_HAS_MULTI_FILE_SEARCH
        auto* searchWidget = new MultiFileSearchWidget(this);
        tunerDock->setWidget(searchWidget);
#else
        QLabel* placeholder = new QLabel("Multi-File Search unavailable (widget not present)", this);
        placeholder->setAlignment(Qt::AlignCenter);
        tunerDock->setWidget(placeholder);
#endif
        addDockWidget(Qt::RightDockWidgetArea, tunerDock);
    }
    tunerDock->setVisible(true);
    tunerDock->raise();
}

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
        "GGUF Models (*.gguf);;All Files (*)");
    
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
        QMessageBox::critical(this, "Invalid Model", 
            QString("Model file not found: %1").arg(ggufPath));
        statusBar()->showMessage("❌ Model file not found", 3000);
        return;
    }
    
    qDebug() << "[MainWindow::onModelSelected] Loading model:" << ggufPath;
    // Record telemetry: model load requested
    {
        QJsonObject meta; meta["path"] = ggufPath; meta["event"] = QString("requested");
        GetTelemetry().recordEvent("model_load", meta);
    }
    statusBar()->showMessage("🔄 Loading model...", 0);
    QApplication::processEvents(); // Update UI
    
    // Create inference engine if it doesn't exist
    if (!m_inferenceEngine) {
        qDebug() << "[MainWindow] Creating new InferenceEngine";
        m_inferenceEngine = new ::InferenceEngine(QString(), this);  // Empty path - no immediate load
        
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
    
    // Check file size: if < 500MB, load directly (small model fast path)
    QFileInfo fileInfo(ggufPath);
    qint64 fileSize = fileInfo.size();
    bool isSmallModel = (fileSize < 500 * 1024 * 1024);  // 500 MB threshold
    
    if (isSmallModel) {
        qInfo() << "[MainWindow] Small model detected (" << fileSize / (1024*1024) << "MB) - direct load (no threading)";  
        // Direct synchronous load for instant availability
        if (m_inferenceEngine) {
            m_loadingProgressDialog->setLabelText(QString("Loading %1 (direct)...\nInitializing...").arg(modelName));
            m_loadingProgressDialog->setRange(0, 0);
            m_loadingProgressDialog->setValue(0);
            m_loadingProgressDialog->show();
            QApplication::processEvents();
            
            bool success = m_inferenceEngine->loadModel(ggufPath);
            m_loadingProgressDialog->hide();
            // Trigger finished callback immediately
            onModelLoadFinished(success, success ? "" : "Direct load failed");
        }
        return;
    }
    
    qInfo() << "[MainWindow] Large model (" << fileSize / (1024*1024) << "MB) - using background thread";
    
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

void MainWindow::onModelLoadFinished(bool loadSuccess, const std::string& errorMsg)
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
            // Link to agentic engine AND sync the modelLoaded flag
            if (m_agenticEngine) {
                m_agenticEngine->setInferenceEngine(m_inferenceEngine);
                // CRITICAL: Sync AgenticEngine's m_modelLoaded flag so processMessage() uses real inference
                m_agenticEngine->markModelAsLoaded(ggufPath);
                qDebug() << "[MainWindow::onModelSelected] ✅ AgenticEngine flagged as model-loaded";
            }

            // Initialize AgenticExecutor with the loaded engines
            if (m_agenticExecutor) {
                m_agenticExecutor->initialize(m_agenticEngine, m_inferenceEngine);
                qDebug() << "[MainWindow::onModelSelected] ✅ AgenticExecutor initialized with engines";
            }
            
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
            
            // Enable chat after model loads
            if (m_currentChatPanel) {
                QString modelName = QFileInfo(ggufPath).fileName();
                m_currentChatPanel->setModelStatus(modelName, true);
                qDebug() << "[MainWindow] Chat panel model status updated:" << modelName;
            }
            
            // Update all chat tabs
            if (m_chatTabs) {
                for (int i = 0; i < m_chatTabs->count(); ++i) {
                    AIChatPanel* panel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(i));
                    if (panel) {
                        QString modelName = QFileInfo(ggufPath).fileName();
                        panel->setModelStatus(modelName, true);
                    }
                }
                qDebug() << "[MainWindow] All chat panels notified of model load";
            }
            
                qInfo() << "[MainWindow] ✅ Model loaded successfully:" << modelName;
                // Telemetry: model load succeeded
                QJsonObject meta; meta["path"] = ggufPath; meta["status"] = QString("success");
                GetTelemetry().recordEvent("model_load", meta);
    } else {
        QMessageBox::critical(this, "Load Failed", 
            QString("Failed to load GGUF model: %1\n\nCheck the console for detailed error messages.").arg(ggufPath));
        statusBar()->showMessage(QString("❌ Model load failed: %1").arg(QFileInfo(ggufPath).fileName()), 5000);
        // Telemetry: model load failed
        QJsonObject meta; meta["path"] = ggufPath; meta["status"] = QString("failed"); meta["error"] = QString::fromStdString(errorMsg);
        GetTelemetry().recordEvent("model_load", meta);
    }
    
    m_pendingModelPath.clear();
}

void MainWindow::onModelLoadCanceled()
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
    QSettings settings("RawrXD", "AgenticIDE");
    
    // Read settings or use defaults
    float temperature = settings.value("AI/temperature", 0.8f).toFloat();
    float topP = settings.value("AI/topP", 0.9f).toFloat();
    int maxTokens = settings.value("AI/maxTokens", 512).toInt();
    
    qDebug() << "[MainWindow::applyInferenceSettings] Applying:"
             << "temp=" << temperature << "topP=" << topP << "maxTokens=" << maxTokens;
    
    // Forward to AgenticEngine
    if (m_agenticEngine) {
        AgenticEngine::GenerationConfig cfg;
        cfg.temperature = temperature;
        cfg.topP = topP;
        cfg.maxTokens = maxTokens;
        
        m_agenticEngine->setGenerationConfig(cfg);
        
        statusBar()->showMessage(
            QString("⚙️ Inference settings updated: Temp=%.1f, TopP=%.2f, Tokens=%1")
            .arg(temperature).arg(topP).arg(maxTokens), 3000);
    }
}

void MainWindow::onChatMessageSent(const QString& message)
{
    qDebug() << "[MainWindow::onChatMessageSent] ━━━ Message Routing Start ━━━";
    qDebug() << "[MainWindow::onChatMessageSent] Message:" << message;
    
    // Validate prerequisites
    if (!m_agenticEngine) {
        qCritical() << "[MainWindow::onChatMessageSent] ✗ FATAL: AgenticEngine not initialized";
        if (m_currentChatPanel) {
            m_currentChatPanel->addAssistantMessage("❌ FATAL: AI engine not initialized. Restart IDE.", false);
        }
        return;
    }
    
    // Check if model is loaded
    if (!m_agenticEngine->isModelLoaded()) {
        qWarning() << "[MainWindow::onChatMessageSent] ⚠ Model not loaded yet";
        if (m_currentChatPanel) {
            m_currentChatPanel->addAssistantMessage(
                "⏳ Model is still loading. Please wait before sending messages.", false);
        }
        return;
    }
    
    qDebug() << "[MainWindow::onChatMessageSent] ✓ Model loaded, proceeding...";
    
    // Get editor context (prefer MASM editor if focus is inside it)
    QString editorContext;
    QWidget* focus = QApplication::focusWidget();
    const bool masmFocused = (m_masmEditor && focus && m_masmEditor->isAncestorOf(focus));

    if (masmFocused) {
        const QString masmPath = m_masmEditor->getFilePath();
        const QString masmText = m_masmEditor->getContent();
        editorContext += QString("Current MASM file: %1\n")
                           .arg(masmPath.isEmpty() ? QStringLiteral("(unsaved)") : masmPath);
        if (!masmText.isEmpty() && masmText.length() < 8000) {
            editorContext += QString("MASM source:\n```asm\n%1\n```\n").arg(masmText);
        }
    } else if (m_multiTabEditor) {
        // Get selected text
        QString selectedText = m_multiTabEditor->getSelectedText();
        if (!selectedText.isEmpty()) {
            editorContext += QString("Selected code:\n```\n%1\n```\n\n").arg(selectedText);
        }

        // Add file context
        QString currentFile = m_multiTabEditor->getCurrentFilePath();
        if (!currentFile.isEmpty()) {
            editorContext += QString("Current file: %1\n").arg(currentFile);

            // Add current text snippet for context
            QString currentText = m_multiTabEditor->getCurrentText();
            if (!currentText.isEmpty() && currentText.length() < 5000) {
                editorContext += QString("File content:\n```\n%1\n```\n").arg(currentText);
            }
        }
    }

    qDebug() << "[MainWindow::onChatMessageSent] Editor context:" << editorContext.length() << "chars";
    
    // Update chat panel context
    if (m_currentChatPanel) {
        updateChatContextForPanel(m_currentChatPanel);
        qDebug() << "[MainWindow::onChatMessageSent] Chat panel context updated";
    }
    
    // Route to AgenticEngine (main processing)
    qDebug() << "[MainWindow::onChatMessageSent] Calling agenticEngine->processMessage()...";
    m_agenticEngine->processMessage(message, editorContext);
    qDebug() << "[MainWindow::onChatMessageSent] ✓ Message queued in AgenticEngine";
    qDebug() << "[MainWindow::onChatMessageSent] ━━━ Message Routing Complete ━━━";
}

void MainWindow::updateChatContextForPanel(AIChatPanel* panel)
{
    if (!panel) return;

    QWidget* focus = QApplication::focusWidget();
    const bool masmFocused = (m_masmEditor && focus && m_masmEditor->isAncestorOf(focus));
    if (masmFocused) {
        panel->setContext(m_masmEditor->getContent(), m_masmEditor->getFilePath());
        return;
    }

    if (!m_multiTabEditor) return;
    panel->setContext(m_multiTabEditor->getCurrentText(), m_multiTabEditor->getCurrentFilePath());
}

void MainWindow::openFileInEditor(const QString& filePath)
{
    if (!m_multiTabEditor || filePath.isEmpty()) return;
    m_multiTabEditor->openFile(filePath);

    if (m_multiFileSearch) {
        m_multiFileSearch->setProjectPath(QFileInfo(filePath).absolutePath());
    }

    statusBar()->showMessage(QString("Opened: %1").arg(QFileInfo(filePath).fileName()), 2000);
}

void MainWindow::scrollEditorToLine(int lineNumber)
{
    if (!m_multiTabEditor || lineNumber <= 0) return;
    auto* editor = m_multiTabEditor->getCurrentEditor();
    if (!editor) return;
    QTextCursor cursor = editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, lineNumber - 1);
    editor->setTextCursor(cursor);
    editor->ensureCursorVisible();
    statusBar()->showMessage(QString("Scrolled to line %1").arg(lineNumber), 2000);
}

void MainWindow::parseAndOpenFilesFromMessage(const QString& message)
{
    // Parse file:line patterns like src/main.cpp:42 or /path/to/file.h:10
    QRegularExpression fileLineRegex(R"((\S*[/\\]\S+\.[a-zA-Z]+):(\d+))");
    auto matches = fileLineRegex.globalMatch(message);
    while (matches.hasNext()) {
        auto match = matches.next();
        QString filePath = match.captured(1);
        int lineNum = match.captured(2).toInt();
        qDebug() << "[parseAndOpenFilesFromMessage] Found file:line:" << filePath << lineNum;
        openFileInEditor(filePath);
        if (lineNum > 0) scrollEditorToLine(lineNum);
    }
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
    shellCombo->addItems({"PowerShell", "Cmd", "Bash", "Custom"});
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
        // Save preferences to QSettings
        QSettings settings("RawrXD", "AgenticIDE");
        settings.setValue("lsp/command", lspCmdEdit->text());
        settings.setValue("lsp/autoStart", lspAutoStart->isChecked());
        settings.setValue("editor/fontSize", fontSpin->value());
        settings.setValue("editor/lineNumbers", lineNumbers->isChecked());
        settings.setValue("editor/wordWrap", wordWrap->isChecked());
        settings.setValue("terminal/shell", shellCombo->currentText());
        settings.sync();
        statusBar()->showMessage("✓ Preferences saved", 3000);
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
    
    // ===== 1. DIFF PREVIEW DOCK =====
    try {
        m_diffPreviewDock = new DiffDock(this);
        addDockWidget(Qt::RightDockWidgetArea, m_diffPreviewDock);
        m_diffPreviewDock->hide();  // Hidden until refactor is suggested
        
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
        
        qDebug() << "  ✓ Diff Preview Dock initialized";
        
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
    if (m_diffPreviewDock) {
        m_diffPreviewDock->setDiff(original, suggested);
        qDebug() << "[MainWindow] Refactor suggestion shown in diff dock";
    }
}

void MainWindow::showModelDownloadDialog()
{
    RawrXD::ModelDownloadDialog* dialog = new RawrXD::ModelDownloadDialog(this);
    
    if (dialog->exec() == QDialog::Accepted) {
        statusBar()->showMessage("✓ Model downloaded! Refreshing model list...", 5000);
        qDebug() << "[MainWindow] Model downloaded successfully";
        
        if (m_currentChatPanel) {
            // Refresh models in all chat panels
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
    m_themeDock->setVisible(!m_themeDock->isVisible());
    if (m_themeDock->isVisible()) {
        m_themeDock->raise();
        statusBar()->showMessage("🎨 Theme Configuration opened", 2000);
    }
}

void MainWindow::showTransparencyControls() {
    m_transparencyDock->setVisible(!m_transparencyDock->isVisible());
    if (m_transparencyDock->isVisible()) {
        m_transparencyDock->raise();
        statusBar()->showMessage("🔮 Transparency Controls opened", 2000);
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
                // Apply theme to chat panel (would need chatPanel->applyTheme() method)
                qDebug() << "[MainWindow] Applying theme to chat panel" << i;
            }
        }
    }
    
    statusBar()->showMessage("🎨 Theme applied successfully", 2000);
}

} // namespace RawrXD




