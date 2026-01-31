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

// Phase 2 Polish Features
#include "../ui/diff_dock.h"
#include "../ui/gpu_backend_selector.h"
#include "../ui/auto_model_downloader.h"
#include "../ui/model_download_dialog_new.h"
#include "../ui/telemetry_optin_dialog.h"

// ALL  Headers - MUST be included to force AUTOMOC processing
// MOC doesn't discover these through other .cpp file includes
#include "agentic_ide.h"
#include "agentic_executor.h"
#include "agentic_copilot_bridge.h"
#include "chat_workspace.h"
#include "planning_agent.h"
#include "ghost_text_renderer.h"
#include "scalar_server.h"
#include "telemetry.h"
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


namespace RawrXD {

MainWindow::MainWindow(void *parent)
    : void(parent)
    , m_multiTabEditor(nullptr)
    , m_chatInterface(nullptr)
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
    
    // Basic window setup only
    setWindowTitle("RawrXD Agentic IDE v5.0 - Production Ready");
    resize(1400, 900);
    
    // Create splash widget for initialization progress
    m_splashWidget = new void(this);
    m_splashWidget->setStyleSheet("background-color: #1e1e1e; color: #d4d4d4;");
    void *splashLayout = new void(m_splashWidget);
    splashLayout->setAlignment(//AlignCenter);
    
    void *titleLabel = new void("<h1>RawrXD Agentic IDE</h1><p>v5.0 Production Ready</p>");
    titleLabel->setAlignment(//AlignCenter);
    titleLabel->setStyleSheet("color: #4ec9b0; margin: 20px;");
    splashLayout->addWidget(titleLabel);
    
    m_splashLabel = new void("Initializing...");
    m_splashLabel->setAlignment(//AlignCenter);
    splashLayout->addWidget(m_splashLabel);
    
    m_splashProgress = new void();
    m_splashProgress->setRange(0, 100);
    m_splashProgress->setValue(0);
    m_splashProgress->setTextVisible(true);
    m_splashProgress->setStyleSheet(
        "void { border: 2px solid #3c3c3c; border-radius: 5px; text-align: center; }"
        "void::chunk { background-color: #4ec9b0; }"
    );
    splashLayout->addWidget(m_splashProgress);
    
    setCentralWidget(m_splashWidget);
    
    // Defer all heavy initialization to after event loop starts
    void*::singleShot(0, this, &MainWindow::initialize);
}

MainWindow::~MainWindow()
{
    saveSettings();
}

void MainWindow::initialize()
{
    updateSplashProgress("⏳ Phase 1/4: Initializing core editor...", 10);
    
    try {
        // Create central editor (lightweight - just void wrapper)
        m_multiTabEditor = new MultiTabEditor(this);
        m_multiTabEditor->initialize();  // Deferred widget creation
        m_multiTabEditor->hide();  // Keep hidden until splash is done
        
        updateSplashProgress("✓ Editor initialized", 25);
        
        // Schedule next initialization phase
        void*::singleShot(100, this, &MainWindow::initializePhase2);
        
    } catch (const std::exception& e) {
        updateSplashProgress("✗ Editor initialization failed", 25);
        QMessageBox::critical(this, "Initialization Error", 
            std::string("Failed to initialize editor: %1")));
    }
}

void MainWindow::initializePhase2()
{
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
        config.arguments = std::vector<std::string>{"--background-index", "--clang-tidy"};
        config.workspaceRoot = std::filesystem::path::currentPath();
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
        void*::singleShot(100, this, &MainWindow::initializePhase3);
        
    } catch (const std::exception& e) {
        updateSplashProgress("⚠ AI initialization warning", 55);
        statusBar()->showMessage(std::string("AI initialization warning: %1")));
        // Continue anyway - IDE can work without AI
        void*::singleShot(100, this, &MainWindow::initializePhase3);
    }
}

void MainWindow::initializePhase3()
{
    updateSplashProgress("⏳ Phase 3/4: Creating UI panels...", 60);
    
    try {
        // Create chat interface dock
        m_chatInterface = new ChatInterface(this);
        m_chatInterface->initialize();
        m_chatInterface->setAgenticEngine(m_agenticEngine);
        m_chatInterface->setPlanOrchestrator(m_planOrchestrator);
        
        m_chatDock = new void("AI Chat & Commands", this);
        m_chatDock->setWidget(m_chatInterface);
        addDockWidget(//RightDockWidgetArea, m_chatDock);
        
        updateSplashProgress("✓ Chat interface ready", 65);
        
        // Connect chat messages to agentic engine with editor context
// Qt connect removed
// Qt connect removed
        // Connect model selection to load GGUF files
// Qt connect removed
        // Connect model ready signal to enable/disable chat input
// Qt connect removed
        // Wire progress signals
// Qt connect removed
                });
// Qt connect removed
                });
// Qt connect removed
                    std::string color = success ? "#4ec9b0" : "#f48771";
                    m_chatInterface->addMessage("Task", 
                        std::string("<span style='color:%1;'>%2 [%3] %4</span>")
                            );
                });
        
        // Create file browser dock
        m_fileBrowser = new FileBrowser(this);
        m_fileBrowser->initialize();
        
        m_fileDock = new void("Files", this);
        m_fileDock->setWidget(m_fileBrowser);
        addDockWidget(//LeftDockWidgetArea, m_fileDock);
        
        // Connect file browser to editor
// Qt connect removed
        updateSplashProgress("✓ File browser ready", 75);
        
        // Create terminal pool dock
        m_terminalPool = new TerminalPool(3, this);
        m_terminalPool->initialize();
        
        m_terminalDock = new void("Terminals", this);
        m_terminalDock->setWidget(m_terminalPool);
        addDockWidget(//BottomDockWidgetArea, m_terminalDock);
        
        updateSplashProgress("✓ Terminals ready", 85);
        
        // Create TODO dock
        m_todoManager = new TodoManager(this);
        m_todoDock = new TodoDock(m_todoManager, this);
        
        m_todoDockWidget = new void("TODO List", this);
        m_todoDockWidget->setWidget(m_todoDock);
        addDockWidget(//RightDockWidgetArea, m_todoDockWidget);
        
        updateSplashProgress("✓ All panels created", 90);
        
        // Schedule next phase
        void*::singleShot(100, this, &MainWindow::initializePhase4);
        
    } catch (const std::exception& e) {
        updateSplashProgress("⚠ Dock creation warning", 90);
        statusBar()->showMessage(std::string("Dock creation error: %1")));
        void*::singleShot(100, this, &MainWindow::initializePhase4);
    }
}

void MainWindow::initializePhase4()
{
    updateSplashProgress("⏳ Phase 4/4: Finalizing UI...", 92);
    
    try {
        setupMenuBar();
        updateSplashProgress("✓ Menus created", 95);
        
        setupToolBars();
        setupStatusBar();
        updateSplashProgress("✓ Toolbars created", 98);
        
        loadSettings();
        
        updateSplashProgress("✅ Initialization complete!", 100);
        
        // Replace splash with actual editor
        void*::singleShot(500, [this]() {
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
        updateSplashProgress("⚠ Finalization warning", 100);
        statusBar()->showMessage("Ready (with warnings)");
        
        // Still cleanup splash on error
        void*::singleShot(500, [this]() {
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
    void *menuBar = this->menuBar();
    
    // File menu
    void *fileMenu = menuBar->addMenu("&File");
    fileMenu->addAction("&New File", this, &MainWindow::newFile, QKeySequence::New);
    fileMenu->addAction("&Open File", this, &MainWindow::openFile, QKeySequence::Open);
    fileMenu->addAction("&Save", this, &MainWindow::saveFile, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction("E&xit", this, &MainWindow::close, QKeySequence::Quit);
    
    // Edit menu
    void *editMenu = menuBar->addMenu("&Edit");
    editMenu->addAction("&Undo", this, &MainWindow::undo, QKeySequence::Undo);
    editMenu->addAction("&Redo", this, &MainWindow::redo, QKeySequence::Redo);
    editMenu->addSeparator();
    editMenu->addAction("&Find", this, &MainWindow::find, QKeySequence::Find);
    editMenu->addAction("&Replace", this, &MainWindow::replace, QKeySequence::Replace);
    editMenu->addSeparator();
    editMenu->addAction("&Preferences...", this, &MainWindow::showPreferences, QKeySequence("Ctrl+,"));
    
    // View menu
    void *viewMenu = menuBar->addMenu("&View");
    viewMenu->addAction("Toggle &File Browser", this, &MainWindow::toggleFileBrowser);
    viewMenu->addAction("Toggle &Chat", this, &MainWindow::toggleChat);
    viewMenu->addAction("Toggle &Terminals", this, &MainWindow::toggleTerminals);
    viewMenu->addAction("Toggle &TODOs", this, &MainWindow::toggleTodos);
    m_telemetryAction = viewMenu->addAction("Telemetry &Monitor", this, &MainWindow::toggleTelemetryWindow);
    if (m_telemetryAction) {
        m_telemetryAction->setCheckable(true);
    }
    viewMenu->addSeparator();
    
    // TODO Panel submenu
    void *todoMenu = viewMenu->addMenu("TODO Panel");
    todoMenu->addAction("Add TODO", this, &MainWindow::addTodo, QKeySequence("Ctrl+T"));
    todoMenu->addAction("Scan Code for TODOs", this, &MainWindow::scanCodeForTodos);
    
    // Terminals submenu
    void *termMenu = viewMenu->addMenu("Terminals");
    termMenu->addAction("New Terminal", this, &MainWindow::newTerminal, QKeySequence("Ctrl+Shift+T"));
    termMenu->addAction("Close Terminal", this, &MainWindow::closeTerminal);
    termMenu->addAction("Next Terminal", this, &MainWindow::nextTerminal, QKeySequence("Ctrl+PgDown"));
    termMenu->addAction("Previous Terminal", this, &MainWindow::previousTerminal, QKeySequence("Ctrl+PgUp"));
    
    // AI menu
    void *aiMenu = menuBar->addMenu("&AI");
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
    void *lspMenu = aiMenu->addMenu("&LSP Server");
    lspMenu->addAction("&Start Server", this, &MainWindow::startLSPServer);
    lspMenu->addAction("Sto&p Server", this, &MainWindow::stopLSPServer);
    lspMenu->addAction("&Restart Server", this, &MainWindow::restartLSPServer);
    lspMenu->addAction("Server &Status", this, &MainWindow::showLSPStatus);
    
    // Help menu
    void *helpMenu = menuBar->addMenu("&Help");
    helpMenu->addAction("&About", this, &MainWindow::showAbout);
    helpMenu->addAction("AI &Commands", this, &MainWindow::showAIHelp);
}

void MainWindow::setupToolBars()
{
    void *fileToolBar = addToolBar("File");
    fileToolBar->addAction("New", this, &MainWindow::newFile);
    fileToolBar->addAction("Open", this, &MainWindow::openFile);
    fileToolBar->addAction("Save", this, &MainWindow::saveFile);
    
    void *aiToolBar = addToolBar("AI");
    aiToolBar->addAction("Chat", this, &MainWindow::startChat);
    aiToolBar->addAction("Load Model", this, &MainWindow::loadModel);
    aiToolBar->addAction("Analyze", this, &MainWindow::analyzeCode);
    aiToolBar->addAction("Refactor", this, &MainWindow::refactorCode);
    
    void *todoToolBar = addToolBar("TODO");
    todoToolBar->addAction("Add TODO", this, &MainWindow::addTodo);
    todoToolBar->addAction("Scan TODOs", this, &MainWindow::scanCodeForTodos);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage("Initializing...");
}

void MainWindow::loadSettings()
{
    void* settings("RawrXD", "AgenticIDE");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

void MainWindow::saveSettings()
{
    void* settings("RawrXD", "AgenticIDE");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::updateSplashProgress(const std::string& message, int percent)
{
    if (m_splashLabel) {
        m_splashLabel->setText(message);
    }
    if (m_splashProgress) {
        m_splashProgress->setValue(percent);
    }
    // processEvents();  // Force UI update
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
    std::string fileName = QFileDialog::getOpenFileName(this, "Open File", "", 
        "All Files (*);;C++ Files (*.cpp *.h);;Python Files (*.py)");
    if (!fileName.empty() && m_multiTabEditor) {
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

void MainWindow::toggleTelemetryWindow()
{
    if (!m_telemetryWindow) {
        m_telemetryWindow = new RawrXD::TelemetryWindow(this);
        m_telemetryWindow->setLogDirectory(std::filesystem::path::currentPath());
// Qt connect removed
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
    std::string modelPath = QFileDialog::getOpenFileName(this, 
        "Load AI Model", 
        std::filesystem::path::homePath(), 
        "GGUF Models (*.gguf);;All Files (*)");
    
    if (!modelPath.empty()) {
        // Actually load the model through onModelSelected which does the real work
        statusBar()->showMessage(std::string("Loading model: %1...").fileName()), 5000);
        
        // Use void* to prevent blocking the UI during model loading
        void*::singleShot(100, this, [this, modelPath]() {
            onModelSelected(modelPath);
        });
    }
}

void MainWindow::onModelSelected(const std::string &ggufPath)
{
    // Validate model path
    if (ggufPath.empty() || !std::fstream::exists(ggufPath)) {
        QMessageBox::critical(this, "Invalid Model", 
            std::string("Model file not found: %1"));
        statusBar()->showMessage("❌ Model file not found", 3000);
        return;
    }
    
    statusBar()->showMessage("🔄 Loading model...", 0);
    // processEvents(); // Update UI
    
    // Create inference engine if it doesn't exist
    if (!m_inferenceEngine) {
        m_inferenceEngine = new ::InferenceEngine(std::string(), this);  // Empty path - no immediate load
        
        // Connect signal for unsupported quantization type detection
// Qt connect removed
            if (conversionDialog->exec() == void::Accepted) {
                auto result = conversionDialog->conversionResult();
                if (result == ModelConversionDialog::ConversionSucceeded) {
                    std::string convertedPath = conversionDialog->convertedModelPath();
                    
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
        m_loadingProgressDialog = nullptr;
        m_loadingProgressDialog->setWindowTitle("Loading Model");
        m_loadingProgressDialog->setWindowModality(//WindowModal);
        m_loadingProgressDialog->setMinimumDuration(0);
        m_loadingProgressDialog->setAutoClose(false);
        m_loadingProgressDialog->setAutoReset(false);
// Qt connect removed
    }
    
    std::string modelName = std::filesystem::path(ggufPath).fileName();
    m_loadingProgressDialog->setLabelText(std::string("Loading %1...\nInitializing..."));
    m_loadingProgressDialog->setRange(0, 0);  // Indeterminate
    m_loadingProgressDialog->setValue(0);
    m_loadingProgressDialog->show();


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
        // Post to main thread via void*
        QMetaObject::invokeMethod(this, [this, msg]() {
            if (m_loadingProgressDialog && m_loadingProgressDialog->isVisible()) {
                std::string qmsg = std::string::fromStdString(msg);
                m_loadingProgressDialog->setLabelText(qmsg);
            }
        }, //QueuedConnection);
    });
    
    // Set completion callback (called from background thread)
    m_modelLoaderThread->setCompleteCallback([this](bool success, const std::string& errorMsg) {
        // Post to main thread
        QMetaObject::invokeMethod(this, [this, success, errorMsg]() {
            onModelLoadFinished(success, errorMsg);
        }, //QueuedConnection);
    });
    
    // Start the thread
    m_modelLoaderThread->start();
    
    // Setup timer to check if thread is still alive
    if (!m_loadProgressTimer) {
        m_loadProgressTimer = new void*(this);
// Qt connect removed
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
    std::string ggufPath = m_pendingModelPath;
    
    // Stop progress timer
    if (m_loadProgressTimer) {
        m_loadProgressTimer->stop();
    }
    
    // Hide progress dialog
    if (m_loadingProgressDialog) {
        m_loadingProgressDialog->hide();
    }


    if (loadSuccess) {
            // Link to agentic engine AND sync the modelLoaded flag
            if (m_agenticEngine) {
                m_agenticEngine->setInferenceEngine(m_inferenceEngine);
                // CRITICAL: Sync AgenticEngine's m_modelLoaded flag so processMessage() uses real inference
                m_agenticEngine->markModelAsLoaded(ggufPath);
            }
            
            // Update status bar with comprehensive info
            std::string modelName = std::filesystem::path(ggufPath).baseName();
            std::string backend = "CPU";  // Default, could be read from settings
            void* settings("RawrXD", "AgenticIDE");
            std::string savedBackend = settings.value("AI/backend", "Auto").toString();
            if (savedBackend.contains("Vulkan")) backend = "Vulkan";
            else if (savedBackend.contains("CUDA")) backend = "CUDA";
            
            std::string lspStatus = (m_lspClient && m_lspClient->isRunning()) ? "✔" : "✘";
            
            statusBar()->showMessage(
                std::string("Model: %1 | GPU: %2 | LSP: %3")
                );
            
            // Enable chat after model loads
            if (m_chatInterface) {
                m_chatInterface->setCanSendMessage(true);
            }
            
    } else {
        QMessageBox::critical(this, "Load Failed", 
            std::string("Failed to load GGUF model: %1\n\nCheck the console for detailed error messages."));
        statusBar()->showMessage(std::string("❌ Model load failed: %1").fileName()), 5000);
    }
    
    m_pendingModelPath.clear();
}

void MainWindow::onModelLoadCanceled()
{
    
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
    void* settings("RawrXD", "AgenticIDE");
    
    // Read settings or use defaults
    float temperature = settings.value("AI/temperature", 0.8f).toFloat();
    float topP = settings.value("AI/topP", 0.9f).toFloat();
    int maxTokens = settings.value("AI/maxTokens", 512).toInt();
    
             << "temp=" << temperature << "topP=" << topP << "maxTokens=" << maxTokens;
    
    // Forward to AgenticEngine
    if (m_agenticEngine) {
        AgenticEngine::GenerationConfig cfg;
        cfg.temperature = temperature;
        cfg.topP = topP;
        cfg.maxTokens = maxTokens;
        
        m_agenticEngine->setGenerationConfig(cfg);
        
        statusBar()->showMessage(
            std::string("⚙️ Inference settings updated: Temp=%.1f, TopP=%.2f, Tokens=%1")
            , 3000);
    }
}

void MainWindow::onChatMessageSent(const std::string& message)
{
    // This slot is called when ChatInterface emits messageSent
    // We enhance the message with editor context before sending to AgenticEngine
    
    std::string editorContext;
    if (m_multiTabEditor) {
        editorContext = m_multiTabEditor->getSelectedText();
    }
    
    // Forward to AgenticEngine with context
    if (m_agenticEngine) {
        m_agenticEngine->processMessage(message, editorContext);
                 << editorContext.length() << "chars of editor context";
    } else {
    }
}

void MainWindow::showInferenceSettings()
{
    void *dialog = new void(this);
    dialog->setWindowTitle("Inference Settings");
    dialog->setModal(true);
    dialog->setMinimumWidth(400);
    
    void *layout = new void(dialog);
    
    // Temperature setting
    void *tempLayout = new void();
    tempLayout->addWidget(new void("Temperature:"));
    QDoubleSpinBox *tempSpin = nullptr;
    tempSpin->setRange(0.0, 2.0);
    tempSpin->setSingleStep(0.1);
    void* settings("RawrXD", "AgenticIDE");
    tempSpin->setValue(settings.value("AI/temperature", 0.8).toDouble());
    tempLayout->addWidget(tempSpin);
    layout->addLayout(tempLayout);
    
    // Top-P setting
    void *topPLayout = new void();
    topPLayout->addWidget(new void("Top-P:"));
    QDoubleSpinBox *topPSpin = nullptr;
    topPSpin->setRange(0.0, 1.0);
    topPSpin->setSingleStep(0.05);
    topPSpin->setValue(settings.value("AI/topP", 0.9).toDouble());
    topPLayout->addWidget(topPSpin);
    layout->addLayout(topPLayout);
    
    // Max Tokens setting
    void *tokensLayout = new void();
    tokensLayout->addWidget(new void("Max Tokens:"));
    void *tokensSpin = nullptr;
    tokensSpin->setRange(1, 4096);
    tokensSpin->setValue(settings.value("AI/maxTokens", 512).toInt());
    tokensLayout->addWidget(tokensSpin);
    layout->addLayout(tokensLayout);
    
    // Backend selection
    void *backendLayout = new void();
    backendLayout->addWidget(new void("Backend:"));
    void *backendCombo = new void();
    backendCombo->addItems({"Auto", "CPU", "GPU (Vulkan)", "GPU (CUDA)"});
    std::string savedBackend = settings.value("AI/backend", "Auto").toString();
    int backendIdx = backendCombo->findText(savedBackend);
    if (backendIdx >= 0) backendCombo->setCurrentIndex(backendIdx);
    backendLayout->addWidget(backendCombo);
    layout->addLayout(backendLayout);
    
    // Buttons
    QDialogButtonBox *buttons = nullptr;
// Qt connect removed
        s.setValue("AI/temperature", tempSpin->value());
        s.setValue("AI/topP", topPSpin->value());
        s.setValue("AI/maxTokens", tokensSpin->value());
        s.setValue("AI/backend", backendCombo->currentText());
        
        applyInferenceSettings();
        dialog->accept();
    });
// Qt connect removed
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
        void*::singleShot(500, [this]() {
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
    std::string status = isRunning ? "Running ✓" : "Stopped ✗";
    
    QMessageBox::information(this, "LSP Server Status",
        std::string("Status: %1\n\nLanguage: cpp\nServer: clangd\nCapabilities: Completions, Diagnostics, Hover, Definitions\n\nWorkspace: %2")
            
            ));
}

void MainWindow::showPreferences()
{
    void *dialog = new void(this);
    dialog->setWindowTitle("Preferences");
    dialog->setModal(true);
    dialog->resize(600, 400);
    
    void *mainLayout = new void(dialog);
    void *tabs = new void();
    
    // LSP Settings Tab
    void *lspTab = new void();
    void *lspLayout = new void(lspTab);
    
    void *lspCmdLayout = new void();
    lspCmdLayout->addWidget(new void("LSP Command:"));
    void *lspCmdEdit = new void("clangd");
    lspCmdLayout->addWidget(lspCmdEdit);
    lspLayout->addLayout(lspCmdLayout);
    
    void *lspAutoStart = nullptr;
    lspAutoStart->setChecked(true);
    lspLayout->addWidget(lspAutoStart);
    
    lspLayout->addStretch();
    tabs->addTab(lspTab, "LSP");
    
    // AI Settings Tab
    void *aiTab = new void();
    void *aiLayout = new void(aiTab);
    
    void *modelLayout = new void();
    modelLayout->addWidget(new void("Default Model:"));
    void *modelEdit = new void();
    modelLayout->addWidget(modelEdit);
    void *browseBtn = new void("Browse...");
// Qt connect removed
        if (!path.empty()) modelEdit->setText(path);
    });
    modelLayout->addWidget(browseBtn);
    aiLayout->addLayout(modelLayout);
    
    aiLayout->addStretch();
    tabs->addTab(aiTab, "AI Model");
    
    // Terminal Settings Tab
    void *termTab = new void();
    void *termLayout = new void(termTab);
    
    void *shellLayout = new void();
    shellLayout->addWidget(new void("Shell:"));
    void *shellCombo = new void();
    shellCombo->addItems({"PowerShell", "Cmd", "Bash", "Custom"});
    shellLayout->addWidget(shellCombo);
    termLayout->addLayout(shellLayout);
    
    termLayout->addStretch();
    tabs->addTab(termTab, "Terminal");
    
    // Editor Settings Tab
    void *editorTab = new void();
    void *editorLayout = new void(editorTab);
    
    void *fontLayout = new void();
    fontLayout->addWidget(new void("Font Size:"));
    void *fontSpin = nullptr;
    fontSpin->setRange(8, 24);
    fontSpin->setValue(12);
    fontLayout->addWidget(fontSpin);
    editorLayout->addLayout(fontLayout);
    
    void *lineNumbers = nullptr;
    lineNumbers->setChecked(true);
    editorLayout->addWidget(lineNumbers);
    
    void *wordWrap = nullptr;
    editorLayout->addWidget(wordWrap);
    
    editorLayout->addStretch();
    tabs->addTab(editorTab, "Editor");
    
    mainLayout->addWidget(tabs);
    
    // Buttons
    QDialogButtonBox *buttons = nullptr;
// Qt connect removed
// Qt connect removed
    mainLayout->addWidget(buttons);
    
    if (dialog->exec() == void::Accepted) {
        // TODO: Save preferences to void*
        statusBar()->showMessage("Preferences saved", 3000);
    }
    
    delete dialog;
}

void MainWindow::addTodo()
{
    if (!m_todoManager) return;
    
    bool ok;
    std::string text = QInputDialog::getText(this, "Add TODO", 
        "TODO Description:", void::Normal, "", &ok);
    
    if (ok && !text.empty()) {
        m_todoManager->addTodo(text, std::string(), 0);  // No file/line association
        statusBar()->showMessage("TODO added", 2000);
    }
}

void MainWindow::scanCodeForTodos()
{
    if (!m_todoManager) return;
    
    // Get current project directory (use current working directory)
    std::string projectDir = std::filesystem::path::currentPath();
    
    // Allow user to select directory
    std::string selectedDir = QFileDialog::getExistingDirectory(
        this,
        "Select Project Directory to Scan",
        projectDir,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (selectedDir.empty()) return;
    projectDir = selectedDir;
    
    // Confirm scan
    auto reply = QMessageBox::question(this, "Scan for TODOs",
        std::string("Scan all source files in:\n%1\n\nfor TODO/FIXME/XXX comments?"),
        QMessageBox::Yes | QMessageBox::Cancel);
    
    if (reply != QMessageBox::Yes) return;
    
    // Scan recursively
    int foundCount = 0;
    std::vector<std::string> filters;
    filters << "*.cpp" << "*.h" << "*.hpp" << "*.c" << "*.cc" << "*.cxx"
            << "*.py" << "*.js" << "*.ts" << "*.java" << "*.cs" << "*.rs"
            << "*.go" << "*.rb" << "*.php" << "*.swift" << "*.kt" << "*.scala"
            << "*.md" << "*.txt" << "*.cmake" << "CMakeLists.txt";
    
    QDirIterator it(projectDir, filters, std::filesystem::path::Files | std::filesystem::path::NoSymLinks,
                    QDirIterator::Subdirectories);
    
    std::regex todoRegex(
        R"((//|#|;|<!--|/\*)\s*(TODO|FIXME|XXX|HACK|NOTE|BUG)(:|\s+)(.*))",
        std::regex::CaseInsensitiveOption
    );
    
    while (itfalse) {
        std::string filePath = it;
        if (filePath.contains("/build/") || filePath.contains("\\\\build\\\\") ||
            filePath.contains("/build_") || filePath.contains("\\\\build_\\") ||
            filePath.contains("/.git/") || filePath.contains("\\\\.git\\\\")) continue;
        std::fstream file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
        QTextStream in(&file);
        int lineNum = 0;
        while (!in.atEnd()) {
            lineNum++;
            std::string line = in.readLine();
            std::smatch match = todoRegex.match(line);
            if (match.hasMatch()) {
                std::string todoType = match"".toUpper();
                std::string todoText = match"".trimmed();
                if (todoText.empty()) todoText = std::string("[%1]");
                else todoText = std::string("[%1] %2");
                m_todoManager->addTodo(todoText, filePath, lineNum);
                foundCount++;
            }
        }
        file.close();
    }
    statusBar()->showMessage(std::string("Scan complete: %1 TODO items found"), 5000);
    QMessageBox::information(this, "Scan Complete",
        std::string("Found %1 TODO/FIXME/XXX comments.\n\nItems added to TODO panel."));
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
    
    // ===== 1. DIFF PREVIEW DOCK =====
    try {
        m_diffPreviewDock = new DiffDock(this);
        addDockWidget(//RightDockWidgetArea, m_diffPreviewDock);
        m_diffPreviewDock->hide();  // Hidden until refactor is suggested
        
        // Connect accept button - apply changes to editor
// Qt connect removed
                cursor.beginEditBlock();
                cursor.select(QTextCursor::BlockUnderCursor);
                cursor.insertText(text);
                cursor.endEditBlock();
                m_diffPreviewDock->hide();
                statusBar()->showMessage("✓ Refactor applied", 3000);
            }
        });
        
        // Connect reject button
// Qt connect removed
            statusBar()->showMessage("✗ Refactor rejected", 2000);
        });


    } catch (const std::exception& e) {
    }
    
    // ===== 2. STREAMING TOKEN PROGRESS (Already in ChatInterface) =====
    // Connect AgenticEngine token signal to ChatInterface progress bar
    if (m_agenticEngine && m_chatInterface) {
// Qt connect removed
    }
    
    // ===== 3. GPU BACKEND SELECTOR =====
    try {
        void* aiToolbar = nullptr;
        
        // Find existing AI toolbar or create new one
        for (void* toolbar : findChildren<void*>()) {
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
        aiToolbar->addWidget(new void(" Backend: ", this));
        aiToolbar->addWidget(m_backendSelector);
        
        // Connect backend changes to inference engine
// Qt connect removed
            switch (backend) {
                case RawrXD::ComputeBackend::CUDA: backendName = "CUDA"; break;
                case RawrXD::ComputeBackend::Vulkan: backendName = "Vulkan"; break;
                case RawrXD::ComputeBackend::CPU: backendName = "CPU"; break;
                case RawrXD::ComputeBackend::DirectML: backendName = "DirectML"; break;
                default: backendName = "Auto"; break;
            }
            
            statusBar()->showMessage("✓ Backend: " + backendName, 3000);
        });


    } catch (const std::exception& e) {
    }
    
    // ===== 4. AUTO MODEL DOWNLOAD =====
    try {
        void*::singleShot(1500, this, [this]() {
            RawrXD::AutoModelDownloader downloader;
            
            if (!downloader.hasLocalModels()) {
                showModelDownloadDialog();
            }
        });


    } catch (const std::exception& e) {
    }    // ===== 5. TELEMETRY OPT-IN =====
    try {
        void*::singleShot(2500, this, [this]() {
            if (!RawrXD::hasTelemetryPreference()) {
                
                RawrXD::TelemetryOptInDialog* dialog = new RawrXD::TelemetryOptInDialog(this);
// Qt connect removed
                    statusBar()->showMessage(enabled ? 
                        "✓ Thank you for helping improve RawrXD IDE!" : 
                        "Telemetry disabled", 
                        5000);
                });
                
                dialog->exec();
                dialog->deleteLater();
            }
        });


    } catch (const std::exception& e) {
    }
    
}

void MainWindow::onRefactorSuggested(const std::string &original, const std::string &suggested)
{
    if (m_diffPreviewDock) {
        m_diffPreviewDock->setDiff(original, suggested);
    }
}

void MainWindow::showModelDownloadDialog()
{
    RawrXD::ModelDownloadDialog* dialog = new RawrXD::ModelDownloadDialog(this);
    
    if (dialog->exec() == void::Accepted) {
        statusBar()->showMessage("✓ Model downloaded! Refreshing model list...", 5000);
        
        if (m_chatInterface) {
            m_chatInterface->refreshModels();
        }
    } else {
        statusBar()->showMessage(
            "ℹ No models installed. Use AI → Download Model to get started", 
            10000);
    }
    
    dialog->deleteLater();
}

} // namespace RawrXD


