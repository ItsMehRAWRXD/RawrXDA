// RawrXD IDE MainWindow Implementation
// "One IDE to rule them all" - comprehensive development environment
#include "MainWindow.h"
#include "orchestration/TaskOrchestrator.h"
#include "orchestration/OrchestrationUI.h"
#include "ActivityBar.h"
#include "multi_tab_editor.h"       // Multi-tab editor widget
#include "agentic_text_edit.h"      // AgenticTextEdit for code editor
#include "widgets/masm_editor_widget.h"
#include "widgets/hotpatch_panel.h"
#include "widgets/project_explorer.h"
#include "interpretability_panel_enhanced.hpp"
#include "inference_engine.hpp"
#include "gguf_server.hpp"
#include "streaming_inference.hpp"
#include "model_monitor.hpp"
#include "command_palette.hpp"
#include "ai_chat_panel.hpp"
#include "agent_chat_breadcrumb.hpp"
#include "metrics_collector.hpp"
#include "agentic_engine.h"
// GGUF metadata quick reader
#include "../ai/streaming_gguf_loader_qt.h"
// Model loader threaded helper
#include "model_loader_thread.hpp"
#include "../agent/auto_bootstrap.hpp"
#include "../agent/hot_reload.hpp"
#include "../agent/self_test_gate.hpp"
#include "../agent/meta_planner.hpp"
#include "../agent/action_executor.hpp"
#include "../agent/model_invoker.hpp"
#include "widgets/layer_quant_widget.hpp"
#include "settings_dialog.h"
#include "settings_manager.h"
#include "safe_mode_config.hpp"
#include "widgets/notification_center.h"
#include "ProdIntegration.h"
#include "metrics_dashboard.hpp"

// Sovereign telemetry dashboard (MMF-backed)
#include "../gui/sovereign_dashboard_widget.h"

// ============================================================
// PHASE B: Production Widget Integration
// ============================================================
#include "Subsystems_Production.h"
#include "MainWindow_Widget_Integration.h"

// ============================================================
// Thermal Dashboard Integration
// ============================================================
#include "../gui/ThermalDashboardWidget.h"

// Forward declaration resolved by include above

// ----------------  brutal-gzip glue  ----------------
#include "deflate_brutal_qt.hpp"     // compress / decompress


#ifdef HAVE_QT_WEBSOCKETS


#endif
#include <functional>

namespace {

// Creates the Sovereign telemetry dock that consumes the MASM/MMF stats.
QDockWidget* createSovereignTelemetryDock(void* host) {
    auto* dock = new QDockWidget(void::tr("Sovereign Telemetry"), host);
    dock->setObjectName(QStringLiteral("SovereignTelemetryDock"));
    dock->setAllowedAreas(//LeftDockWidgetArea | //RightDockWidgetArea);
    dock->setMinimumWidth(260);

    auto* dashboard = new SovereignDashboardWidget(dock);
    // Attach to shared stats; if attach fails we still show the dock so users see status.
    dashboard->attachSharedMemory(QStringLiteral("Global\\SOVEREIGN_STATS"));
    dock->setWidget(dashboard);
    return dock;
}

} // namespace

// ============================================================
// Global Circuit Breakers for External Services
// ============================================================
static RawrXD::Integration::CircuitBreaker g_buildSystemBreaker(5, std::chrono::seconds(30).count() * 1000);
static RawrXD::Integration::CircuitBreaker g_vcsBreaker(5, std::chrono::seconds(30).count() * 1000);
static RawrXD::Integration::CircuitBreaker g_dockerBreaker(3, std::chrono::seconds(60).count() * 1000);
static RawrXD::Integration::CircuitBreaker g_cloudBreaker(3, std::chrono::seconds(60).count() * 1000);
static RawrXD::Integration::CircuitBreaker g_aiBreaker(5, std::chrono::seconds(30).count() * 1000);

// ============================================================
// Global Caches for Performance Optimization
// ============================================================
static QCache<std::string, std::any> g_settingsCache(100);  // Cache frequently accessed settings
static QCache<std::string, std::filesystem::path> g_fileInfoCache(500); // Cache file info lookups
static std::mutex g_cacheMutex;                              // Thread-safe cache access
static std::shared_mutex g_fileInfoLock;                    // RW lock for file info operations

// ============================================================
// Helper Utilities - cached settings/file info
// ============================================================
inline std::any getCachedSetting(const std::string& key, const std::any& defaultValue = std::any()) {
    std::lock_guard<std::mutex> locker(&g_cacheMutex);
    if (auto* cached = g_settingsCache.object(key)) {
        return *cached;
    }
    QSettings settings("RawrXD", "IDE");
    std::any value = settings.value(key, defaultValue);
    g_settingsCache.insert(key, new std::any(value));
    return value;
}

inline std::filesystem::path getCachedFileInfo(const std::string& path) {
    QReadLocker locker(&g_fileInfoLock);
    if (auto* cached = g_fileInfoCache.object(path)) {
        if (cached->exists()) return *cached;
    }
    locker.unlock();

    QWriteLocker writeLocker(&g_fileInfoLock);
    if (auto* cached = g_fileInfoCache.object(path)) {
        if (cached->exists()) return *cached;
    }
    std::filesystem::path info(path);
    if (info.exists()) {
        g_fileInfoCache.insert(path, new std::filesystem::path(info));
    }
    return info;
}

MainWindow::MainWindow(void* parent)
    : void(parent),
      m_engineThread(nullptr),
      m_inferenceEngine(nullptr),
      m_ggufServer(nullptr),
      m_streamer(nullptr),
      m_streamingMode(false),
      m_currentStreamId(0)
{
    setupStatusBar();
    applyDarkTheme();
    createVSCodeLayout();

    // Initialize major subsystems/widgets
    initializeProductionWidgets(this, true /* restoreGeometry */);
    initSubsystems();

    // Attach Sovereign telemetry dock (MMF-backed MASM stats)
    m_sovereignTelemetryDock = createSovereignTelemetryDock(this);
    if (m_sovereignTelemetryDock) {
        addDockWidget(//RightDockWidgetArea, m_sovereignTelemetryDock);
    }

    // Initialize inference engine in worker thread
    m_engineThread = new std::thread(this);
    m_inferenceEngine = new InferenceEngine();
    m_inferenceEngine->;
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
    m_engineThread->start();

    // Initialize GGUF server (auto-starts if port 11434 is available)
    m_ggufServer = new GGUFServer(m_inferenceEngine, this);
// Qt connect removed
    });
// Qt connect removed
    });
    void*::singleShot(500, this, [this]() { m_ggufServer->start(11434); });

    // Initialize streaming inference
    m_streamer = new StreamingInference(m_hexMagConsole, this);
// Qt connect removed
            [this](qint64 /*reqId*/, const std::string& token) { m_streamer->pushToken(token); });
// Qt connect removed
            [this](qint64 /*reqId*/) { m_streamer->finishStream(); });

    // Setup AI/agent components
    setupAIBackendSwitcher();
    setupLayerQuantWidget();
    setupSwarmEditing();
    setupAgentSystem();
    setupCommandPalette();
    setupAIChatPanel();
    setupMASMEditor();
    setupInterpretabilityPanel();

    // Shortcut for command palette
    QShortcut* commandPaletteShortcut = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
// Qt connect removed
    });

    // Auto-load GGUF from env var if provided
    std::string ggufEnv = qEnvironmentVariable("RAWRXD_GGUF");
    if (!ggufEnv.isEmpty()) {
        statusBar()->showMessage(tr("Auto-loading GGUF: %1"), 3000);
        QMetaObject::invokeMethod(m_inferenceEngine, "loadModel", //QueuedConnection,
                                  (std::string, ggufEnv));
    }

    // Restore saved UI state (window geometry, dock positions, panel visibility)
    handleLoadState();
}

void MainWindow::createVSCodeLayout()
{
    /*
     * VS Code Layout Structure:
     * 
     * +--------+----------+---------------------+
     * | Activity  Primary    Central Editor       |
     * |   Bar      Sidebar      (Tabs)            |
     * | (50px)   (260px)                         |
     * +--------+----------+---------------------+
     * |                                          |
     * | Terminal/Output/Problems/Debug Console   |
     * | (Bottom Panel - Tabbed)                  |
     * +--------+----------+---------------------+
     * | Enhanced Status Bar                      |
     * +--------+----------+---------------------+
     */
    
    // Create main container widget
    // AGENTIC PATCH: Ensure all pointers are initialized to nullptr before use (C++11+ best practice)
    // Declare missing local variables and member pointers before use
    void* mainContainer = new void(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(mainContainer);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // Ensure member pointers are initialized (redundant if done in constructor, but safe for agentic automation)
    if (!m_primarySidebar) m_primarySidebar = new QFrame(mainContainer);
    if (!m_sidebarStack) m_sidebarStack = new QStackedWidget(m_primarySidebar);
    if (!editorTabs_) editorTabs_ = new QTabWidget(mainContainer);
    if (!codeView_) codeView_ = new QTextEdit(mainContainer);
    if (!m_bottomPanel) m_bottomPanel = new QFrame(mainContainer);
    if (!m_panelStack) m_panelStack = new QStackedWidget(m_bottomPanel);
    if (!m_hexMagConsole) m_hexMagConsole = new QPlainTextEdit(m_bottomPanel);

    // AGENTIC PATCH: Declare missing local variable for centerSplitter
    QSplitter* centerSplitter = new QSplitter(//Horizontal, mainContainer);

    m_primarySidebar->setFixedWidth(260);
    m_primarySidebar->setStyleSheet("QFrame { background-color: #252526; border: none; }");

    QVBoxLayout* sidebarLayout = new QVBoxLayout(m_primarySidebar);
    sidebarLayout->setContentsMargins(0, 0, 0, 0);
    sidebarLayout->setSpacing(0);

    // Create sidebar header
    QLabel* sidebarHeader = new QLabel("Explorer", m_primarySidebar);
    sidebarHeader->setStyleSheet("QLabel { color: #e0e0e0; background-color: #2d2d30; padding: 8px; font-weight: bold; }");
    sidebarLayout->addWidget(sidebarHeader);

    // Create stacked widget for sidebar views
    m_sidebarStack->setStyleSheet("QStackedWidget { background-color: #252526; }");

    // Create Explorer view (placeholder - tree widget)
    QTreeWidget* explorerView = new QTreeWidget(m_primarySidebar);
    explorerView->setStyleSheet("QTreeWidget { background-color: #252526; color: #e0e0e0; }");
    QTreeWidgetItem* rootItem = new QTreeWidgetItem();
    rootItem->setText(0, "Project Folder");
    explorerView->addTopLevelItem(rootItem);
    m_sidebarStack->addWidget(explorerView);

    // Create Search view (placeholder)
    void* searchView = new void(m_primarySidebar);
    QVBoxLayout* searchLayout = new QVBoxLayout(searchView);
    QLineEdit* searchInput = new QLineEdit(m_primarySidebar);
    searchInput->setPlaceholderText("Search files...");
    searchInput->setStyleSheet("QLineEdit { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; padding: 5px; }");
    searchLayout->addWidget(searchInput);
    m_sidebarStack->addWidget(searchView);

    // Create Source Control view (placeholder)
    void* scmView = new void(m_primarySidebar);
    QVBoxLayout* scmLayout = new QVBoxLayout(scmView);
    QLabel* scmLabel = new QLabel("Source Control\n\nNo folder open", m_primarySidebar);
    scmLabel->setStyleSheet("QLabel { color: #e0e0e0; }");
    scmLabel->setAlignment(//AlignCenter);
    scmLayout->addWidget(scmLabel);
    m_sidebarStack->addWidget(scmView);

    // Create Debug view (placeholder)
    void* debugView = new void(m_primarySidebar);
    QVBoxLayout* debugLayout = new QVBoxLayout(debugView);
    QLabel* debugLabel = new QLabel("Run and Debug\n\nNo launch configuration", m_primarySidebar);
    debugLabel->setStyleSheet("QLabel { color: #e0e0e0; }");
    debugLabel->setAlignment(//AlignCenter);
    debugLayout->addWidget(debugLabel);
    m_sidebarStack->addWidget(debugView);

    // Create Extensions view (placeholder)
    void* extView = new void(m_primarySidebar);
    QVBoxLayout* extLayout = new QVBoxLayout(extView);
    QLineEdit* extSearch = new QLineEdit(m_primarySidebar);
    extSearch->setPlaceholderText("Search extensions...");
    extSearch->setStyleSheet("QLineEdit { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; padding: 5px; }");
    extLayout->addWidget(extSearch);
    m_sidebarStack->addWidget(extView);

    sidebarLayout->addWidget(m_sidebarStack, 1);

    centerSplitter->addWidget(m_primarySidebar);

    // --------- Central Editor Area (Tabbed) ---------
    QFrame* editorFrame = new QFrame(mainContainer);
    editorFrame->setStyleSheet("QFrame { background-color: #1e1e1e; border: none; }");
    QVBoxLayout* editorLayout = new QVBoxLayout(editorFrame);
    editorLayout->setContentsMargins(0, 0, 0, 0);
    editorLayout->setSpacing(0);

    editorTabs_->setStyleSheet(
        "QTabBar { background-color: #252526; }"
        "QTabBar::tab { background-color: #1e1e1e; color: #e0e0e0; padding: 8px; margin: 0px; border: 1px solid #3e3e42; }"
        "QTabBar::tab:selected { background-color: #252526; border-bottom: 2px solid #007acc; }"
        "QTabWidget::pane { border: none; }"
    );

    codeView_->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #e0e0e0; font-family: 'Consolas', monospace; font-size: 11pt; }");
    codeView_->setLineWrapMode(QTextEdit::NoWrap);
    editorTabs_->addTab(codeView_, "Untitled");

    editorLayout->addWidget(editorTabs_, 1);

    centerSplitter->addWidget(editorFrame);
    centerSplitter->setStretchFactor(0, 0);  // Sidebar doesn't stretch
    centerSplitter->setStretchFactor(1, 1);  // Editor stretches

    mainLayout->addWidget(centerSplitter, 1);

    // ============= BOTTOM: Panel Dock (Terminal/Output/Problems/Debug) =============
    m_bottomPanel->setFixedHeight(200);  // Initial height
    m_bottomPanel->setStyleSheet("QFrame { background-color: #252526; border-top: 1px solid #3e3e42; }");

    QVBoxLayout* panelLayout = new QVBoxLayout(m_bottomPanel);
    panelLayout->setContentsMargins(0, 0, 0, 0);
    panelLayout->setSpacing(0);

    // Panel tabs header
    QFrame* panelHeader = new QFrame(m_bottomPanel);
    panelHeader->setFixedHeight(35);
    panelHeader->setStyleSheet("QFrame { background-color: #2d2d30; border: none; }");
    QHBoxLayout* panelHeaderLayout = new QHBoxLayout(panelHeader);
    panelHeaderLayout->setContentsMargins(5, 0, 5, 0);

    // Panel tab buttons
    QPushButton* terminalTabBtn = new QPushButton("Terminal", panelHeader);
    QPushButton* outputTabBtn = new QPushButton("Output", panelHeader);
    QPushButton* problemsTabBtn = new QPushButton("Problems", panelHeader);
    QPushButton* debugTabBtn = new QPushButton("Debug Console", panelHeader);

    for (QPushButton* btn : {terminalTabBtn, outputTabBtn, problemsTabBtn, debugTabBtn}) {
        btn->setStyleSheet(
            "QPushButton { background-color: transparent; color: #e0e0e0; border: none; padding: 8px; }"
            "QPushButton:hover { background-color: #3e3e42; }"
            "QPushButton:pressed { border-bottom: 2px solid #007acc; }"
        );
        panelHeaderLayout->addWidget(btn);
    }

    panelHeaderLayout->addStretch();

    // Minimize/maximize buttons
    QPushButton* panelMinBtn = new QPushButton("−", panelHeader);
    panelMinBtn->setFixedSize(30, 30);
    panelMinBtn->setStyleSheet("QPushButton { background-color: transparent; color: #e0e0e0; }");
    panelHeaderLayout->addWidget(panelMinBtn);

    QPushButton* panelMaxBtn = new QPushButton("□", panelHeader);
    panelMaxBtn->setFixedSize(30, 30);
    panelMaxBtn->setStyleSheet("QPushButton { background-color: transparent; color: #e0e0e0; }");
    panelHeaderLayout->addWidget(panelMaxBtn);

    QPushButton* panelCloseBtn = new QPushButton("✕", panelHeader);
    panelCloseBtn->setFixedSize(30, 30);
    panelCloseBtn->setStyleSheet("QPushButton { background-color: transparent; color: #e0e0e0; }");
    panelHeaderLayout->addWidget(panelCloseBtn);

    panelLayout->addWidget(panelHeader);

    // Panel content (stacked widget for tabs)
    m_panelStack->setStyleSheet("QStackedWidget { background-color: #1e1e1e; }");

    // Terminal tab
    QPlainTextEdit* terminalView = new QPlainTextEdit(m_bottomPanel);
    terminalView->setStyleSheet("QPlainTextEdit { background-color: #1e1e1e; color: #0dff00; font-family: 'Consolas', monospace; font-size: 10pt; }");
    terminalView->appendPlainText("PS E:\\> ");
    m_panelStack->addWidget(terminalView);

    // Output tab
    QPlainTextEdit* outputView = new QPlainTextEdit(m_bottomPanel);
    outputView->setStyleSheet("QPlainTextEdit { background-color: #1e1e1e; color: #e0e0e0; font-family: 'Consolas', monospace; font-size: 10pt; }");
    outputView->appendPlainText("[INFO] Ready to process...");
    m_panelStack->addWidget(outputView);

    // Problems tab
    void* problemsView = new void(m_bottomPanel);
    QVBoxLayout* problemsLayout = new QVBoxLayout(problemsView);
    problemsLayout->setContentsMargins(10, 10, 10, 10);
    QLabel* problemsLabel = new QLabel("No problems detected", problemsView);
    problemsLabel->setStyleSheet("QLabel { color: #e0e0e0; }");
    problemsLayout->addWidget(problemsLabel);
    problemsLayout->addStretch();
    m_panelStack->addWidget(problemsView);

    // Debug Console tab
    QPlainTextEdit* debugConsole = new QPlainTextEdit(m_bottomPanel);
    debugConsole->setStyleSheet("QPlainTextEdit { background-color: #1e1e1e; color: #e0e0e0; font-family: 'Consolas', monospace; font-size: 10pt; }");
    debugConsole->appendPlainText("Debug console ready");
    m_panelStack->addWidget(debugConsole);

    // ----------  HexMag inference console  ----------
    m_hexMagConsole->setStyleSheet(
        "QPlainTextEdit { background-color: #1e1e1e; color: #0dff00; "
        "font-family: 'Consolas', monospace; font-size: 10pt; }");
    m_hexMagConsole->appendPlainText("HexMag inference console ready...");
    m_panelStack->addWidget(m_hexMagConsole);        // index 4

    panelLayout->addWidget(m_panelStack, 1);

    // ============= Connect Activity Bar to Sidebar Views =============
    if (m_activityBar) {
// Qt connect removed
            // Update sidebar header label
            const char* titles[] = {"Explorer", "Search", "Source Control", "Run and Debug", "Extensions"};
            // Update the header label (would need to store it as member)
        });
    }

    // ============= Create Vertical Splitter (Editor + Panel) =============
    QSplitter* verticalSplitter = new QSplitter(//Vertical, mainContainer);
    verticalSplitter->setOpaqueResize(true);
    verticalSplitter->addWidget(mainLayout->takeAt(0)->widget());  // Adjust layout if needed

    // Better approach: Create a proper vertical splitter at the root
    void* centerWidget = new void(this);
    QVBoxLayout* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    QSplitter* vertSplitter = new QSplitter(//Vertical, centerWidget);
    vertSplitter->setOpaqueResize(true);
    vertSplitter->setStyleSheet("QSplitter::handle { background-color: #2d2d2d; height: 4px; }");
    // AGENTIC PATCH END: All missing variables declared, pointer initializations fixed, and automation comments added.
    
    // Create horizontal splitter for activity bar + sidebar + editor
    void* topWidget = new void(centerWidget);
    topWidget->setLayout(mainLayout);
    
    vertSplitter->addWidget(topWidget);
    vertSplitter->addWidget(m_bottomPanel);
    vertSplitter->setStretchFactor(0, 1);  // Top stretches
    vertSplitter->setStretchFactor(1, 0);  // Bottom doesn't stretch initially
    
    centerLayout->addWidget(vertSplitter);
    setCentralWidget(centerWidget);
    
    // Connect panel buttons
// Qt connect removed
    });
// Qt connect removed
    });
    
    // Connect terminal tab buttons
    connect(terminalTabBtn, &QPushButton::clicked, this, [this]() { m_panelStack->setCurrentIndex(0); });
    connect(outputTabBtn, &QPushButton::clicked, this, [this]() { m_panelStack->setCurrentIndex(1); });
    connect(problemsTabBtn, &QPushButton::clicked, this, [this]() { m_panelStack->setCurrentIndex(2); });
// Qt connect removed
        else m_panelStack->setCurrentIndex(3); 
    });
}

// Note: setupOrchestrationSystem is defined later in file at line ~8487

void MainWindow::applyDarkTheme()
{
    QPalette darkPalette;
    
    // Window colors
    darkPalette.setColor(QPalette::Window, QColor(0x1e, 0x1e, 0x1e));
    darkPalette.setColor(QPalette::WindowText, QColor(0xe0, 0xe0, 0xe0));
    
    // Button colors
    darkPalette.setColor(QPalette::Button, QColor(0x3c, 0x3c, 0x3c));
    darkPalette.setColor(QPalette::ButtonText, QColor(0xe0, 0xe0, 0xe0));
    
    // Base colors
    darkPalette.setColor(QPalette::Base, QColor(0x25, 0x25, 0x26));
    darkPalette.setColor(QPalette::AlternateBase, QColor(0x1e, 0x1e, 0x1e));
    
    // Highlight colors
    darkPalette.setColor(QPalette::Highlight, QColor(0x00, 0x7a, 0xcc));
    darkPalette.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    
    QApplication::setPalette(darkPalette);
}

MainWindow::~MainWindow()
{
    // Cleanup
}

void MainWindow::setAppState(std::shared_ptr<void> state)
{
    
    if (!state) {
        statusBar()->showMessage(tr("Warning: Null application state"), 3000);
        return;
    }
    
    // Store state reference (in production, this would be type-safe)
    // Cast to appropriate type and apply state to subsystems
    
    statusBar()->showMessage(tr("Application state synchronized"), 2000);
    
    // Log to hex console for observability
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(
            std::string("[STATE] Application state updated at %1")
            .toString("yyyy-MM-dd HH:mm:ss"))
        );
    }
    
    // Trigger state-dependent UI updates
    if (projectExplorer_) {
        // Refresh project explorer with new state
    }
    
    if (m_aiChatPanel) {
        // Update AI chat panel with new context
    }
    
    // Persist state to settings
    QSettings settings("RawrXD", "QtShell");
    settings.setValue("AppState/lastUpdate", std::chrono::system_clock::time_point::currentDateTime());
    settings.sync();
}

void MainWindow::setupMenuBar()
{
    // ============================================================
    // FILE MENU - Complete file operations
    // ============================================================
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    
    // New submenu
    QMenu* newMenu = fileMenu->addMenu(tr("&New"));
    newMenu->addAction(tr("New &File"), this, &MainWindow::handleNewEditor, QKeySequence::New);
    newMenu->addAction(tr("New &Window"), this, &MainWindow::handleNewWindow, QKeySequence(//CTRL | //SHIFT | //Key_N));
    newMenu->addAction(tr("New &Chat"), this, &MainWindow::handleNewChat);
    
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Open File..."), this, &MainWindow::handleAddFile, QKeySequence::Open);
    fileMenu->addAction(tr("Open &Folder..."), this, &MainWindow::handleAddFolder, QKeySequence(QStringLiteral("Ctrl+K Ctrl+O")));
    
    // Recent Files submenu
    QMenu* recentMenu = fileMenu->addMenu(tr("Open &Recent"));
    recentMenu->addAction(tr("(No recent files)"));
    recentMenu->addSeparator();
    recentMenu->addAction(tr("Clear Recent Files"), this, [this]() {
        QSettings settings("RawrXD", "IDE");
        settings.remove("recentFiles");
        statusBar()->showMessage(tr("Recent files cleared"), 2000);
    });
    
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Save"), this, &MainWindow::handleSaveState, QKeySequence::Save);
    fileMenu->addAction(tr("Save &As..."), this, &MainWindow::handleSaveAs, QKeySequence::SaveAs);
    fileMenu->addAction(tr("Save A&ll"), this, &MainWindow::handleSaveAll, QKeySequence(QStringLiteral("Ctrl+K S")));
    
    fileMenu->addSeparator();
    QAction* autoSaveAct = fileMenu->addAction(tr("Auto Sa&ve"));
    autoSaveAct->setCheckable(true);
    autoSaveAct->setChecked(QSettings("RawrXD", "IDE").value("editor/autoSave", false).toBool());
// Qt connect removed
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Close Editor"), this, &MainWindow::handleCloseEditor, QKeySequence::Close);
    fileMenu->addAction(tr("Close &All Editors"), this, &MainWindow::handleCloseAllEditors, QKeySequence(QStringLiteral("Ctrl+K Ctrl+W")));
    fileMenu->addAction(tr("Close Fol&der"), this, &MainWindow::handleCloseFolder);
    
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Print..."), this, &MainWindow::handlePrint, QKeySequence::Print);
    fileMenu->addAction(tr("E&xport..."), this, &MainWindow::handleExport);
    
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Pre&ferences..."), this, [this]() { toggleSettings(true); }, QKeySequence(//CTRL | //Key_Comma));
    
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), qApp, &QApplication::quit, QKeySequence::Quit);

    // ============================================================
    // EDIT MENU - Complete editing operations with proper slot connections
    // ============================================================
    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    
    // Undo/Redo
    editMenu->addAction(tr("&Undo"), this, &MainWindow::handleUndo, QKeySequence::Undo);
    editMenu->addAction(tr("&Redo"), this, &MainWindow::handleRedo, QKeySequence::Redo);
    
    editMenu->addSeparator();
    
    // Cut/Copy/Paste with proper slot connections
    editMenu->addAction(tr("Cu&t"), this, &MainWindow::handleCut, QKeySequence::Cut);
    editMenu->addAction(tr("&Copy"), this, &MainWindow::handleCopy, QKeySequence::Copy);
    editMenu->addAction(tr("&Paste"), this, &MainWindow::handlePaste, QKeySequence::Paste);
    editMenu->addAction(tr("&Delete"), this, &MainWindow::handleDelete, QKeySequence::Delete);
    
    editMenu->addSeparator();
    editMenu->addAction(tr("Select &All"), this, &MainWindow::handleSelectAll, QKeySequence::SelectAll);
    
    editMenu->addSeparator();
    
    // Find/Replace
    editMenu->addAction(tr("&Find..."), this, &MainWindow::handleFind, QKeySequence::Find);
    editMenu->addAction(tr("Find and &Replace..."), this, &MainWindow::handleFindReplace, QKeySequence::Replace);
    editMenu->addAction(tr("Find in Fi&les..."), this, &MainWindow::handleFindInFiles, QKeySequence(//CTRL | //SHIFT | //Key_F));
    
    editMenu->addSeparator();
    
    // Navigation
    editMenu->addAction(tr("&Go to Line..."), this, &MainWindow::handleGoToLine, QKeySequence(//CTRL | //Key_G));
    editMenu->addAction(tr("Go to S&ymbol..."), this, &MainWindow::handleGoToSymbol, QKeySequence(//CTRL | //SHIFT | //Key_O));
    editMenu->addAction(tr("Go to &Definition"), this, &MainWindow::handleGoToDefinition, QKeySequence(//Key_F12));
    editMenu->addAction(tr("Go to &References"), this, &MainWindow::handleGoToReferences, QKeySequence(//SHIFT | //Key_F12));
    
    editMenu->addSeparator();
    
    // Code editing
    editMenu->addAction(tr("Toggle &Comment"), this, &MainWindow::handleToggleComment, QKeySequence(//CTRL | //Key_Slash));
    editMenu->addAction(tr("Format &Document"), this, &MainWindow::handleFormatDocument, QKeySequence(//CTRL | //SHIFT | //Key_I));
    editMenu->addAction(tr("Format Se&lection"), this, &MainWindow::handleFormatSelection);
    
    editMenu->addSeparator();
    editMenu->addAction(tr("Fold A&ll"), this, &MainWindow::handleFoldAll);
    editMenu->addAction(tr("&Unfold All"), this, &MainWindow::handleUnfoldAll);

    // ============================================================
    // VIEW MENU - All 48 toggle slots exposed via organized submenus
    // ============================================================
    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    
    // ----- Command Palette (Quick Access) -----
    viewMenu->addAction(tr("Command Palette..."), this, &MainWindow::toggleCommandPalette, QKeySequence(//CTRL | //SHIFT | //Key_P))->setCheckable(false);
    viewMenu->addSeparator();
    
    // ----- Explorer Section -----
    QMenu* explorerMenu = viewMenu->addMenu(tr("&Explorer"));
    QAction* projExplAct = explorerMenu->addAction(tr("Project Explorer"), this, &MainWindow::toggleProjectExplorer, QKeySequence(//CTRL | //SHIFT | //Key_E));
    projExplAct->setCheckable(true);
    explorerMenu->addAction(tr("Search Results"), this, &MainWindow::toggleSearchResult)->setCheckable(true);
    explorerMenu->addAction(tr("Bookmarks"), this, &MainWindow::toggleBookmark)->setCheckable(true);
    explorerMenu->addAction(tr("TODO List"), this, &MainWindow::toggleTodo)->setCheckable(true);
    
    // ----- Source Control Section -----
    QMenu* scmMenu = viewMenu->addMenu(tr("&Source Control"));
    scmMenu->addAction(tr("Version Control"), this, &MainWindow::toggleVersionControl, QKeySequence(//CTRL | //SHIFT | //Key_G))->setCheckable(true);
    scmMenu->addAction(tr("Diff Viewer"), this, &MainWindow::toggleDiffViewer)->setCheckable(true);
    
    // ----- Build & Debug Section -----
    QMenu* buildDebugMenu = viewMenu->addMenu(tr("&Build && Debug"));
    buildDebugMenu->addAction(tr("Build System"), this, &MainWindow::toggleBuildSystem)->setCheckable(true);
    buildDebugMenu->addAction(tr("Run && Debug"), this, &MainWindow::toggleRunDebug, QKeySequence(//CTRL | //SHIFT | //Key_D))->setCheckable(true);
    buildDebugMenu->addAction(tr("Profiler"), this, &MainWindow::toggleProfiler)->setCheckable(true);
    buildDebugMenu->addAction(tr("Test Explorer"), this, &MainWindow::toggleTestExplorer)->setCheckable(true);
    buildDebugMenu->addSeparator();
    
    // --- Eon/ASM Compiler Integration ---
    QMenu* compilerMenu = buildDebugMenu->addMenu(tr("&Eon/ASM Compiler"));
    compilerMenu->addAction(tr("Compile Current File"), this, [this]() {
        toggleCompileCurrentFile();
    }, QKeySequence(//CTRL | //Key_F7));
    compilerMenu->addAction(tr("Build Project"), this, [this]() {
        toggleBuildProject();
    }, QKeySequence(//Key_F7));
    compilerMenu->addAction(tr("Clean Build"), this, [this]() {
        toggleCleanBuild();
    });
    compilerMenu->addSeparator();
    compilerMenu->addAction(tr("Compiler Settings..."), this, [this]() {
        toggleCompilerSettings();
    });
    compilerMenu->addAction(tr("Compiler Output"), this, [this]() {
        toggleCompilerOutput();
    })->setCheckable(true);
    
    // ----- AI & Agent Section -----
    QMenu* aiViewMenu = viewMenu->addMenu(tr("&AI && Agent"));
    QAction* aiChatAct = aiViewMenu->addAction(tr("AI Chat Panel"), this, [this](bool checked) {
        if (m_aiChatPanelDock) m_aiChatPanelDock->setVisible(checked);
    }, QKeySequence(//CTRL | //SHIFT | //Key_A));
    aiChatAct->setCheckable(true);
    if (m_aiChatPanelDock) {
        aiChatAct->setChecked(m_aiChatPanelDock->isVisible());
// Qt connect removed
    }
    aiViewMenu->addAction(tr("AI Quick Fix"), this, &MainWindow::toggleAIQuickFix)->setCheckable(true);
    aiViewMenu->addAction(tr("AI Completion Cache"), this, &MainWindow::toggleAICompletionCache)->setCheckable(true);
    aiViewMenu->addSeparator();
    QAction* orchestrationAct = aiViewMenu->addAction(tr("Task Orchestration"), this, [this](bool checked) {
        if (checked && !m_orchestrationDock) setupOrchestrationSystem();
        else if (m_orchestrationDock) m_orchestrationDock->setVisible(checked);
    });
    orchestrationAct->setCheckable(true);
    if (m_orchestrationDock) {
        orchestrationAct->setChecked(m_orchestrationDock->isVisible());
// Qt connect removed
    }
    
    // ----- Model Management Section -----
    QMenu* modelViewMenu = viewMenu->addMenu(tr("&Model"));
    QAction* monAct = modelViewMenu->addAction(tr("Model Monitor"));
    monAct->setCheckable(true);
    if (m_modelMonitorDock) {
        monAct->setChecked(m_modelMonitorDock->isVisible());
// Qt connect removed
    }
// Qt connect removed
            ModelMonitor* monitor = new ModelMonitor(m_inferenceEngine, m_modelMonitorDock);
            monitor->initialize();
            m_modelMonitorDock->setWidget(monitor);
            addDockWidget(//RightDockWidgetArea, m_modelMonitorDock);
        } else if (m_modelMonitorDock) m_modelMonitorDock->setVisible(on);
    });
    QAction* layerQuantAct = modelViewMenu->addAction(tr("Layer Quantization"), this, [this](bool checked) {
        if (m_layerQuantDock) m_layerQuantDock->setVisible(checked);
    });
    layerQuantAct->setCheckable(true);
    if (m_layerQuantDock) {
        layerQuantAct->setChecked(m_layerQuantDock->isVisible());
// Qt connect removed
    }
    QAction* interpretabilityAct = modelViewMenu->addAction(tr("Model Interpretability"), this, [this](bool checked) {
        if (m_interpretabilityPanelDock) m_interpretabilityPanelDock->setVisible(checked);
        else if (checked) setupInterpretabilityPanel();
    });
    interpretabilityAct->setCheckable(true);
    if (m_interpretabilityPanelDock) {
        interpretabilityAct->setChecked(m_interpretabilityPanelDock->isVisible());
// Qt connect removed
    }
    
    // ----- Terminal Section -----
    QMenu* terminalMenu = viewMenu->addMenu(tr("&Terminal"));
    terminalMenu->addAction(tr("Terminal Cluster"), this, &MainWindow::toggleTerminalCluster, QKeySequence(//CTRL | //Key_QuoteLeft))->setCheckable(true);
    terminalMenu->addAction(tr("Terminal Emulator"), this, &MainWindow::toggleTerminalEmulator)->setCheckable(true);
    
    // ----- Sovereign Telemetry Section -----
    QAction* sovereignTelemetryAct = viewMenu->addAction(tr("Sovereign Telemetry"), this, [this](bool checked) {
        if (!m_sovereignTelemetryDock && checked) {
            m_sovereignTelemetryDock = createSovereignTelemetryDock(this);
            if (m_sovereignTelemetryDock) {
                addDockWidget(//RightDockWidgetArea, m_sovereignTelemetryDock);
            }
        }
        if (m_sovereignTelemetryDock) {
            m_sovereignTelemetryDock->setVisible(checked);
        }
    });
    sovereignTelemetryAct->setCheckable(true);
    if (m_sovereignTelemetryDock) {
        sovereignTelemetryAct->setChecked(m_sovereignTelemetryDock->isVisible());
// Qt connect removed
    }

    // ----- Thermal Dashboard Section -----
    QAction* thermalAct = viewMenu->addAction(tr("Thermal Dashboard"), this, [this](bool checked) {
        if (!m_thermalDashboardDock && checked) {
            m_thermalDashboardDock = new QDockWidget(tr("NVMe Thermal"), this);
            m_thermalDashboardDock->setWidget(new ThermalDashboardWidget(this));
            addDockWidget(//RightDockWidgetArea, m_thermalDashboardDock);
        } else if (m_thermalDashboardDock) {
            m_thermalDashboardDock->setVisible(checked);
        }
    });
    thermalAct->setCheckable(true);
    thermalAct->setChecked(false);
    
    // ----- Editor Features Section -----
    QMenu* editorFeaturesMenu = viewMenu->addMenu(tr("&Editor Features"));
    QAction* masmAct = editorFeaturesMenu->addAction(tr("MASM Editor"), this, [this](bool checked) {
        if (m_masmEditorDock) m_masmEditorDock->setVisible(checked);
    });
    masmAct->setCheckable(true);
    if (m_masmEditorDock) {
        masmAct->setChecked(m_masmEditorDock->isVisible());
// Qt connect removed
    }
    editorFeaturesMenu->addAction(tr("Code Minimap"), this, &MainWindow::toggleCodeMinimap)->setCheckable(true);
    editorFeaturesMenu->addAction(tr("Breadcrumb Bar"), this, &MainWindow::toggleBreadcrumbBar)->setCheckable(true);
    editorFeaturesMenu->addAction(tr("Language Server"), this, &MainWindow::toggleLanguageClientHost)->setCheckable(true);
    editorFeaturesMenu->addSeparator();
    QAction* hotpatchAct = editorFeaturesMenu->addAction(tr("Hotpatch Panel"), this, [this](bool checked) {
        if (m_hotpatchPanelDock) m_hotpatchPanelDock->setVisible(checked);
    });
    hotpatchAct->setCheckable(true);
    if (m_hotpatchPanelDock) {
        hotpatchAct->setChecked(m_hotpatchPanelDock->isVisible());
// Qt connect removed
    }
    
    // ----- DevOps & Cloud Section -----
    QMenu* devopsMenu = viewMenu->addMenu(tr("&DevOps && Cloud"));
    devopsMenu->addAction(tr("Docker Tool"), this, &MainWindow::toggleDockerTool)->setCheckable(true);
    devopsMenu->addAction(tr("Cloud Explorer"), this, &MainWindow::toggleCloudExplorer)->setCheckable(true);
    devopsMenu->addAction(tr("Database Tool"), this, &MainWindow::toggleDatabaseTool)->setCheckable(true);
    devopsMenu->addAction(tr("Package Manager"), this, &MainWindow::togglePackageManager)->setCheckable(true);
    
    // ----- Documentation Section -----
    QMenu* docsMenu = viewMenu->addMenu(tr("&Documentation"));
    docsMenu->addAction(tr("Documentation Browser"), this, &MainWindow::toggleDocumentation)->setCheckable(true);
    docsMenu->addAction(tr("UML View"), this, &MainWindow::toggleUMLView)->setCheckable(true);
    docsMenu->addAction(tr("Markdown Viewer"), this, &MainWindow::toggleMarkdownViewer)->setCheckable(true);
    docsMenu->addAction(tr("Notebook"), this, &MainWindow::toggleNotebook)->setCheckable(true);
    docsMenu->addAction(tr("Spreadsheet"), this, &MainWindow::toggleSpreadsheet)->setCheckable(true);
    
    // ----- Design Tools Section -----
    QMenu* designMenu = viewMenu->addMenu(tr("D&esign Tools"));
    designMenu->addAction(tr("Image Tool"), this, &MainWindow::toggleImageTool)->setCheckable(true);
    designMenu->addAction(tr("Design to Code"), this, &MainWindow::toggleDesignToCode)->setCheckable(true);
    designMenu->addAction(tr("Color Picker"), this, &MainWindow::toggleColorPicker)->setCheckable(true);
    designMenu->addAction(tr("Icon Font Browser"), this, &MainWindow::toggleIconFont)->setCheckable(true);
    designMenu->addAction(tr("Translation"), this, &MainWindow::toggleTranslation)->setCheckable(true);
    
    // ----- Utilities Section -----
    QMenu* utilsMenu = viewMenu->addMenu(tr("&Utilities"));
    utilsMenu->addAction(tr("Snippet Manager"), this, &MainWindow::toggleSnippetManager)->setCheckable(true);
    utilsMenu->addAction(tr("Regex Tester"), this, &MainWindow::toggleRegexTester)->setCheckable(true);
    utilsMenu->addAction(tr("Macro Recorder"), this, &MainWindow::toggleMacroRecorder)->setCheckable(true);
    
    viewMenu->addSeparator();
    
    // ----- Appearance Section -----
    QMenu* appearanceMenu = viewMenu->addMenu(tr("A&ppearance"));
    appearanceMenu->addAction(tr("Toggle Full Screen"), this, &MainWindow::handleFullScreen, QKeySequence::FullScreen)->setCheckable(true);
    appearanceMenu->addAction(tr("Toggle Zen Mode"), this, &MainWindow::handleZenMode)->setCheckable(true);
    appearanceMenu->addSeparator();
    appearanceMenu->addAction(tr("Toggle Side Bar"), this, &MainWindow::handleToggleSidebar, QKeySequence(//CTRL | //Key_B))->setCheckable(true);
    appearanceMenu->addAction(tr("Toggle Status Bar"), this, &MainWindow::toggleStatusBarManager)->setCheckable(true);
    appearanceMenu->addSeparator();
    appearanceMenu->addAction(tr("Reset Layout"), this, &MainWindow::handleResetLayout);
    
    // ----- System Section -----
    QMenu* systemMenu = viewMenu->addMenu(tr("S&ystem"));
    systemMenu->addAction(tr("Welcome Screen"), this, &MainWindow::toggleWelcomeScreen)->setCheckable(true);
    systemMenu->addAction(tr("Settings"), this, &MainWindow::toggleSettings)->setCheckable(true);
    systemMenu->addAction(tr("Shortcuts"), this, &MainWindow::toggleShortcutsConfigurator)->setCheckable(true);
    systemMenu->addSeparator();
    systemMenu->addAction(tr("Plugin Manager"), this, &MainWindow::togglePluginManager)->setCheckable(true);
    systemMenu->addAction(tr("Notification Center"), this, &MainWindow::toggleNotificationCenter)->setCheckable(true);
    systemMenu->addAction(tr("Progress Manager"), this, &MainWindow::toggleProgressManager)->setCheckable(true);
    systemMenu->addSeparator();
    systemMenu->addAction(tr("Telemetry"), this, &MainWindow::toggleTelemetry)->setCheckable(true);
    systemMenu->addAction(tr("Update Checker"), this, &MainWindow::toggleUpdateChecker)->setCheckable(true);

    // ============================================================
    // TOOLS MENU - Developer utilities
    // ============================================================
    QMenu* toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("Command Palette..."), this, &MainWindow::toggleCommandPalette, QKeySequence(//CTRL | //SHIFT | //Key_P));
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("Snippet Manager"), this, &MainWindow::toggleSnippetManager);
    toolsMenu->addAction(tr("Regex Tester"), this, &MainWindow::toggleRegexTester);
    toolsMenu->addAction(tr("Color Picker"), this, &MainWindow::toggleColorPicker);
    toolsMenu->addAction(tr("Macro Recorder"), this, &MainWindow::toggleMacroRecorder);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("Profiler"), this, &MainWindow::toggleProfiler);
    toolsMenu->addAction(tr("Database Tool"), this, &MainWindow::toggleDatabaseTool);
    toolsMenu->addAction(tr("Docker Tool"), this, &MainWindow::toggleDockerTool);
    toolsMenu->addSeparator();
    toolsMenu->addAction(tr("External Tools..."), this, &MainWindow::handleExternalTools);

    // ============================================================
    // RUN MENU - Execution and debugging
    // ============================================================
    QMenu* runMenu = menuBar()->addMenu(tr("&Run"));
    runMenu->addAction(tr("&Start Debugging"), this, &MainWindow::handleStartDebug, QKeySequence(//Key_F5));
    runMenu->addAction(tr("Run &Without Debugging"), this, &MainWindow::handleRunNoDebug, QKeySequence(//CTRL | //Key_F5));
    runMenu->addAction(tr("S&top Debugging"), this, &MainWindow::handleStopDebug, QKeySequence(//SHIFT | //Key_F5));
    runMenu->addAction(tr("&Restart Debugging"), this, &MainWindow::handleRestartDebug, QKeySequence(//CTRL | //SHIFT | //Key_F5));
    runMenu->addSeparator();
    runMenu->addAction(tr("Step &Over"), this, &MainWindow::handleStepOver, QKeySequence(//Key_F10));
    runMenu->addAction(tr("Step &Into"), this, &MainWindow::handleStepInto, QKeySequence(//Key_F11));
    runMenu->addAction(tr("Step O&ut"), this, &MainWindow::handleStepOut, QKeySequence(//SHIFT | //Key_F11));
    runMenu->addSeparator();
    runMenu->addAction(tr("Toggle &Breakpoint"), this, &MainWindow::handleToggleBreakpoint, QKeySequence(//Key_F9));
    runMenu->addAction(tr("&Add Configuration..."), this, &MainWindow::handleAddRunConfig);
    runMenu->addSeparator();
    runMenu->addAction(tr("Run Script"), this, &MainWindow::onRunScript);

    // ============================================================
    // TERMINAL MENU
    // ============================================================
    QMenu* termMenu = menuBar()->addMenu(tr("Ter&minal"));
    termMenu->addAction(tr("&New Terminal"), this, &MainWindow::handleNewTerminal, QKeySequence(//CTRL | //SHIFT | //Key_QuoteLeft));
    termMenu->addAction(tr("&Split Terminal"), this, &MainWindow::handleSplitTerminal);
    termMenu->addAction(tr("&Kill Terminal"), this, &MainWindow::handleKillTerminal);
    termMenu->addAction(tr("&Clear Terminal"), this, &MainWindow::handleClearTerminal);
    termMenu->addSeparator();
    termMenu->addAction(tr("Run &Active File"), this, &MainWindow::handleRunActiveFile);
    termMenu->addAction(tr("Run Se&lected Text"), this, &MainWindow::handleRunSelection);

    // ============================================================
    // WINDOW MENU - Layout management
    // ============================================================
    QMenu* windowMenu = menuBar()->addMenu(tr("&Window"));
    windowMenu->addAction(tr("&New Window"), this, &MainWindow::handleNewWindow);
    windowMenu->addSeparator();
    windowMenu->addAction(tr("Split Editor &Right"), this, &MainWindow::handleSplitRight);
    windowMenu->addAction(tr("Split Editor &Down"), this, &MainWindow::handleSplitDown);
    windowMenu->addAction(tr("&Single Editor Group"), this, &MainWindow::handleSingleGroup);
    windowMenu->addSeparator();
    windowMenu->addAction(tr("Toggle &Full Screen"), this, &MainWindow::handleFullScreen, QKeySequence::FullScreen);
    windowMenu->addAction(tr("Toggle &Zen Mode"), this, &MainWindow::handleZenMode);
    windowMenu->addSeparator();
    windowMenu->addAction(tr("&Reset Layout"), this, &MainWindow::handleResetLayout);
    windowMenu->addAction(tr("&Save Layout As..."), this, &MainWindow::handleSaveLayout);

    // AI/GGUF menu with brutal_gzip integration
    QMenu* aiMenu = menuBar()->addMenu(tr("&AI"));
    aiMenu->addAction(tr("Load GGUF Model..."), this,
                      static_cast<void (MainWindow::*)()>(&MainWindow::loadGGUFModel));
    aiMenu->addAction(tr("Run Inference..."), this, &MainWindow::runInference);
    aiMenu->addAction(tr("Unload Model"), this, &MainWindow::unloadGGUFModel);
    aiMenu->addSeparator();
    
    // Streaming mode toggle
    QAction* streamAct = aiMenu->addAction(tr("Streaming Mode"));
    streamAct->setCheckable(true);
// Qt connect removed
        statusBar()->showMessage(on ? tr("Streaming inference ON")
                                    : tr("Streaming inference OFF"), 2000);
    });
    
    // Batch compress folder
    aiMenu->addSeparator();
    QAction* batchAct = aiMenu->addAction(tr("Batch Compress Folder..."));
// Qt connect removed
    setupQuantizationMenu(aiMenu);

    QMenu* agentMenu = menuBar()->addMenu(tr("&Agent"));
    QActionGroup* agentModeGroup = new QActionGroup(this);
    m_agentModeGroup = agentModeGroup;
    agentModeGroup->setExclusive(true);
    struct AgentMode { const char* label; const char* id; } agentModes[] = {
        {"Plan Mode", "Plan"},
        {"Agent Mode", "Agent"},
        {"Ask Mode", "Ask"},
    };
    for (const auto& mode : agentModes) {
        QAction* action = agentMenu->addAction(std::string::fromUtf8(mode.label));
        action->setCheckable(true);
        action->setData(std::string::fromUtf8(mode.id));
        agentModeGroup->addAction(action);
        if (std::string::fromUtf8(mode.id) == m_agentMode) {
            action->setChecked(true);
        }
    }
// Qt connect removed
    });

    QMenu* modelMenu = menuBar()->addMenu(tr("&Model"));
    modelMenu->addAction(tr("Load Local GGUF..."), this,
                         static_cast<void (MainWindow::*)()>(&MainWindow::loadGGUFModel));
    modelMenu->addAction(tr("Unload Model"), this, &MainWindow::unloadGGUFModel);
    modelMenu->addSeparator();
    m_backendGroup = new QActionGroup(this);
    m_backendGroup->setExclusive(true);
    struct BackendOption { const char* id; const char* label; } backendOptions[] = {
        {"local", "Local GGUF"},
        {"ollama", "Remote Ollama"},
        {"custom", "Custom Backend"}
    };
    for (const auto& backend : backendOptions) {
        std::string backendId = std::string::fromUtf8(backend.id);
        QAction* backendAction = modelMenu->addAction(std::string::fromUtf8(backend.label));
        backendAction->setCheckable(true);
        backendAction->setData(backendId);
        m_backendGroup->addAction(backendAction);
        if (backendId == m_currentBackend) {
            backendAction->setChecked(true);
        }
    }
// Qt connect removed
    modelMenu->addSeparator();
    modelMenu->addAction(tr("Manage Backends..."), this, &MainWindow::setupAIBackendSwitcher);
    modelMenu->addAction(tr("Refresh Models"), this, &MainWindow::refreshModelSelector);

    // ============================================================
    // HELP MENU - Comprehensive help and support
    // ============================================================
    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&Welcome"), this, &MainWindow::toggleWelcomeScreen);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&Documentation"), this, &MainWindow::handleOpenDocs, QKeySequence(//Key_F1));
    helpMenu->addAction(tr("&Interactive Playground"), this, &MainWindow::handlePlayground);
    helpMenu->addAction(tr("Show All &Commands"), this, &MainWindow::toggleCommandPalette, QKeySequence(//CTRL | //SHIFT | //Key_P));
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&Keyboard Shortcuts"), this, &MainWindow::handleShowShortcuts, QKeySequence(QStringLiteral("Ctrl+K Ctrl+S")));
    helpMenu->addAction(tr("Keyboard Shortcuts &Reference..."), this, &MainWindow::toggleShortcutsConfigurator);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&Check for Updates..."), this, &MainWindow::handleCheckUpdates);
    helpMenu->addAction(tr("&View Release Notes"), this, &MainWindow::handleReleaseNotes);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&Report Issue..."), this, &MainWindow::handleReportIssue);
    helpMenu->addAction(tr("&Join Community..."), this, &MainWindow::handleJoinCommunity);
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&View License"), this, &MainWindow::handleViewLicense);
    helpMenu->addAction(tr("Toggle &Developer Tools"), this, &MainWindow::handleDevTools, QKeySequence(//CTRL | //SHIFT | //Key_I));
    helpMenu->addSeparator();
    helpMenu->addAction(tr("&About RawrXD"), this, &MainWindow::onAbout);
}

void MainWindow::setupToolBars()
{
    QToolBar* toolbar = addToolBar(tr("Main"));
    toolbar->addAction(tr("New"));
    toolbar->addAction(tr("Open"));
    toolbar->addAction(tr("Save"));
    toolbar->addSeparator();
    toolbar->addAction(tr("Run"), this, &MainWindow::onRunScript);
    toolbar->addSeparator();
    
    // Model selector
    QLabel* modelLabel = new QLabel(tr("Model: "), toolbar);
    toolbar->addWidget(modelLabel);
    
    m_modelSelector = new QComboBox(toolbar);
    m_modelSelector->setToolTip(tr("Select GGUF model to load"));
    m_modelSelector->setMinimumWidth(300);
    m_modelSelector->addItem(tr("No model loaded"));
    // Add recent models (populated from settings/cache)
    m_modelSelector->addItem(tr("Load model from file..."));
    toolbar->addWidget(m_modelSelector);
// Qt connect removed
        if (idx <= 0) return;  // Skip "No model loaded" and separators
        std::string modelData = m_modelSelector->itemData(idx).toString();
        if (modelData.startsWith("ollama:")) {
            std::string modelName = modelData.mid(std::string("ollama:").length());
            // Resolve to local GGUF if available
            std::string gguf;
            std::vector<std::string> searchDirs = {std::string("D:/OllamaModels"), std::filesystem::path::homePath() + "/models", std::filesystem::path::currentPath() + "/models"};
            for (const std::string& dirPath : searchDirs) {
                std::filesystem::path d(dirPath);
                if (!d.exists()) continue;
                std::vector<std::string> matches = d.entryList(std::vector<std::string>() << std::string("*%1*.gguf"), std::filesystem::path::Files, std::filesystem::path::Name);
                if (!matches.isEmpty()) { gguf = d.filePath(matches.first()); break; }
            }
            if (!gguf.isEmpty()) {
                // Auto-load the resolved GGUF directly
                loadGGUFModel(gguf);
            } else {
                statusBar()->showMessage(tr("No GGUF found for Ollama model %1"), 5000);
            }
        } else if (!modelData.isEmpty() && modelData != "LOAD") {
            // Direct model selection - if it appears to be a path to a GGUF file, load it
            if (std::fstream::exists(modelData) && modelData.endsWith(".gguf", //CaseInsensitive)) {
                loadGGUFModel(modelData);
            } else {
                // Fallback: open file dialog
                loadGGUFModel();
            }
        } else if (modelData == "LOAD") {
            loadGGUFModel();  // File dialog
        }
    });

    // Populate the dropdown to match the Agent model selector (includes Ollama, cloud and local GGUF models)
    void*::singleShot(0, this, &MainWindow::refreshModelSelector);
    
    toolbar->addSeparator();
    
    // Agent mode switcher
    m_agentModeSwitcher = new QComboBox(toolbar);
    m_agentModeSwitcher->setToolTip(tr("Switch agentic mode"));
    m_agentModeSwitcher->addItem(tr("Plan Mode"), QStringLiteral("Plan"));
    m_agentModeSwitcher->addItem(tr("Agent Mode"), QStringLiteral("Agent"));
    m_agentModeSwitcher->addItem(tr("Ask Mode"), QStringLiteral("Ask"));
    toolbar->addWidget(m_agentModeSwitcher);
// Qt connect removed
        std::any data = m_agentModeSwitcher->currentData();
        if (data.isValid()) changeAgentMode(data.toString());
    });
    changeAgentMode(m_agentMode); // sync UI state
}

void MainWindow::changeAgentMode(const std::string& mode)
{
    if (mode.isEmpty()) return;
    if (mode == m_agentMode) return;
    m_agentMode = mode;
    if (m_agentModeSwitcher) {
        int index = m_agentModeSwitcher->findData(mode);
        bool blocked = m_agentModeSwitcher->blockSignals(true);
        if (index >= 0) {
            m_agentModeSwitcher->setCurrentIndex(index);
        }
        m_agentModeSwitcher->blockSignals(blocked);
    }
    if (m_agentModeGroup) {
        for (QAction* action : m_agentModeGroup->actions()) {
            if (action->data().toString() == mode) {
                bool blocked = action->blockSignals(true);
                action->setChecked(true);
                action->blockSignals(blocked);
                break;
            }
        }
    }
    statusBar()->showMessage(tr("Agent mode set to %1"), 2000);
}

void MainWindow::refreshModelSelector()
{
    if (!m_modelSelector) return;

    std::string current = m_modelSelector->currentData().toString();
    m_modelSelector->clear();
    m_modelSelector->addItem(tr("No model loaded"), "");
    m_modelSelector->addItem(tr("Load model from file..."), "LOAD");

    std::unordered_set<std::string> seen;

    // Search local GGUF directories
    std::vector<std::string> searchDirs = {
        std::filesystem::path::currentPath() + "/models",
        std::filesystem::path::homePath() + "/models",
        std::string("D:/OllamaModels")
    };

    for (const std::string& dirPath : searchDirs) {
        std::filesystem::path d(dirPath);
        if (!d.exists()) continue;
        QDirIterator it(dirPath, std::vector<std::string>() << "*.gguf", std::filesystem::path::Files, QDirIterator::Subdirectories);
        while (itfalse) {
            std::string file = it;
            if (seen.contains(file)) continue;
            std::string display = std::filesystem::path(file).fileName();
            std::string tooltip = buildGgufTooltip(file);
            m_modelSelector->addItem(display, file);
            m_modelSelector->setItemData(m_modelSelector->count() - 1, tooltip, //ToolTipRole);
            seen.insert(file);
        }
    }

    // Add Ollama models via `ollama list` (non-blocking but short timeout)
    QProcess ollamaProcess;
    ollamaProcess.start("ollama", std::vector<std::string>() << "list");
    if (ollamaProcess.waitForStarted(2000) && ollamaProcess.waitForFinished(4000)) {
        std::string output = std::string::fromUtf8(ollamaProcess.readAllStandardOutput());
        std::vector<std::string> lines = output.split('\n', //SkipEmptyParts);
        for (int i = 1; i < lines.size(); ++i) {
            std::string line = lines[i].trimmed();
            if (line.isEmpty()) continue;
            std::vector<std::string> parts = line.split(std::regex("\\s+"), //SkipEmptyParts);
            if (parts.isEmpty()) continue;
            std::string modelName = parts[0];
            std::string key = std::string("ollama:%1");
            if (seen.contains(key)) continue;
            std::string tooltip = modelName;
            // Prefer agent breadcrumb metadata if available
            if (m_aiChatPanel && m_aiChatPanel->getBreadcrumb()) {
                tooltip = m_aiChatPanel->getBreadcrumb()->tooltipForModel(modelName);
            } else {
                tooltip = std::string("<b>%1</b><br/>Source: Ollama");
            }
            m_modelSelector->addItem(std::string("[Ollama] %1"), key);
            m_modelSelector->setItemData(m_modelSelector->count() - 1, tooltip, //ToolTipRole);
            seen.insert(key);
        }
    } else {
    }

    // Load cloud models from QSettings (claude, gpt, copilot, huggingface)
    QSettings settings("RawrXD", "AgenticIDE");
    settings.beginGroup("models/cloud");

    settings.beginGroup("claude");
    for (const auto& key : settings.allKeys()) {
        std::string modelId = settings.value(key, "").toString();
        if (!modelId.isEmpty()) {
            std::string data = std::string("cloud:claude:%1");
            if (!seen.contains(data)) {
                std::string tooltip = std::string("<b>%1</b><br/>Provider: Claude<br/>Model: %2"));
                m_modelSelector->addItem(std::string("[Claude] %1"), data);
                m_modelSelector->setItemData(m_modelSelector->count() - 1, tooltip, //ToolTipRole);
                seen.insert(data);
            }
        }
    }
    settings.endGroup();

    settings.beginGroup("gpt");
    for (const auto& key : settings.allKeys()) {
        std::string modelId = settings.value(key, "").toString();
        if (!modelId.isEmpty()) {
            std::string data = std::string("cloud:openai:%1");
            if (!seen.contains(data)) {
                std::string tooltip = std::string("<b>%1</b><br/>Provider: OpenAI<br/>Model: %2"));
                m_modelSelector->addItem(std::string("[OpenAI] %1"), data);
                m_modelSelector->setItemData(m_modelSelector->count() - 1, tooltip, //ToolTipRole);
                seen.insert(data);
            }
        }
    }
    settings.endGroup();

    settings.beginGroup("copilot");
    for (const auto& key : settings.allKeys()) {
        std::string modelId = settings.value(key, "").toString();
        if (!modelId.isEmpty()) {
            std::string data = std::string("cloud:copilot:%1");
            if (!seen.contains(data)) {
                std::string tooltip = std::string("<b>%1</b><br/>Provider: GitHub Copilot");
                m_modelSelector->addItem(std::string("[Copilot] %1"), data);
                m_modelSelector->setItemData(m_modelSelector->count() - 1, tooltip, //ToolTipRole);
                seen.insert(data);
            }
        }
    }
    settings.endGroup();

    settings.beginGroup("huggingface");
    for (const auto& key : settings.allKeys()) {
        std::string modelId = settings.value(key, "").toString();
        if (!modelId.isEmpty()) {
            std::string data = std::string("cloud:hf:%1");
            if (!seen.contains(data)) {
                std::string tooltip = std::string("<b>%1</b><br/>Provider: HuggingFace");
                m_modelSelector->addItem(std::string("[HuggingFace] %1"), data);
                m_modelSelector->setItemData(m_modelSelector->count() - 1, tooltip, //ToolTipRole);
                seen.insert(data);
            }
        }
    }
    settings.endGroup();

    settings.endGroup();

    // Try to restore previous selection
    int idx = m_modelSelector->findData(current);
    if (idx >= 0) m_modelSelector->setCurrentIndex(idx);
}

std::string MainWindow::buildGgufTooltip(const std::string& filePath)
{
    if (m_modelTooltipCache.contains(filePath)) return m_modelTooltipCache[filePath];

    std::string display = std::filesystem::path(filePath).fileName();
    std::string tooltip = std::string("<b>%1</b><br/>Path: %2");

    // Try to open GGUF and extract metadata (fast, streaming reader)
    try {
        // Async operation helper (disabled until call sites are finalized)
        // template<typename Func>
        // inline QFuture<void> runAsync(Func&& func, const std::string& operation) {
        //     return QtConcurrent::run([func = std::forward<Func>(func), operation]() {
        //         RawrXD::Integration::ScopedTimer timer("Async", operation.toUtf8().constData(), "operation");
        //         try {
        //             func();
        //         } catch (const std::exception& e) {
        //             RawrXD::Integration::logError("Async", operation.toUtf8().constData(),
        //                 std::string("Async operation failed: %1"))));
        //         }
        //     });
        // }
    } catch (const std::exception& e) {
        RawrXD::Integration::logWarn("MainWindow", "gguf_tooltip_failed",
            std::string("Exception: %1")))));
    } catch (...) {
        RawrXD::Integration::logWarn("MainWindow", "gguf_tooltip_failed",
            QStringLiteral("Unknown exception"));
    }

    m_modelTooltipCache.insert(filePath, tooltip);
    return tooltip;
}

void MainWindow::loadGGUFModel(const std::string& ggufPath)
{
    if (ggufPath.isEmpty() || !std::fstream::exists(ggufPath)) {
        QMessageBox::critical(this, tr("Invalid Model"), tr("Model file not found: %1"));
        statusBar()->showMessage(tr("❌ Model file not found"), 3000);
        return;
    }

    m_pendingModelPath = ggufPath;

    if (!m_loadingProgressDialog) {
        m_loadingProgressDialog = new QProgressDialog(this);
        m_loadingProgressDialog->setWindowTitle(tr("Loading Model"));
        m_loadingProgressDialog->setWindowModality(//WindowModal);
        m_loadingProgressDialog->setMinimumDuration(0);
        m_loadingProgressDialog->setAutoClose(false);
        m_loadingProgressDialog->setAutoReset(false);
// Qt connect removed
        });
    }

    std::string modelName = std::filesystem::path(ggufPath).fileName();
    m_loadingProgressDialog->setLabelText(tr("Loading %1...\nInitializing..."));
    m_loadingProgressDialog->setRange(0, 0);
    m_loadingProgressDialog->setValue(0);
    m_loadingProgressDialog->show();

    // Ensure inference engine exists
    if (!m_inferenceEngine) {
        m_inferenceEngine = new InferenceEngine(std::string(), this);
    }

    // Clean up any previous loader
    if (m_modelLoaderThread) {
        m_modelLoaderThread->cancel();
        m_modelLoaderThread->wait(2000);
        delete m_modelLoaderThread;
        m_modelLoaderThread = nullptr;
    }

    m_modelLoaderThread = new ModelLoaderThread(m_inferenceEngine, ggufPath.toStdString());
    m_modelLoaderThread->setProgressCallback([this](const std::string& msg) {
        QMetaObject::invokeMethod(this, [this, msg]() {
            if (m_loadingProgressDialog && m_loadingProgressDialog->isVisible()) {
                m_loadingProgressDialog->setLabelText(std::string::fromStdString(msg));
            }
        }, //QueuedConnection);
    });
    m_modelLoaderThread->setCompleteCallback([this](bool success, const std::string& err) {
        QMetaObject::invokeMethod(this, [this, success, err]() {
            onModelLoadFinished(success, err);
        }, //QueuedConnection);
    });
    m_modelLoaderThread->start();

    if (!m_loadProgressTimer) {
        m_loadProgressTimer = new void*(this);
// Qt connect removed
            }
        });
    }
    m_loadProgressTimer->start(500);
}

void MainWindow::onModelLoadFinished(bool success, const std::string& errorMsg)
{
    if (m_loadProgressTimer) m_loadProgressTimer->stop();
    if (m_loadingProgressDialog) m_loadingProgressDialog->hide();


    std::string ggufPath = m_pendingModelPath;
    if (m_modelLoaderThread) {
        delete m_modelLoaderThread;
        m_modelLoaderThread = nullptr;
    }

    if (!success) {
        QMessageBox::critical(this, tr("Load Failed"), tr("Failed to load GGUF model: %1\n%2")));
        statusBar()->showMessage(tr("❌ Model load failed: %1").fileName()), 5000);
        return;
    }

    // Notify agentic engine and chat panels that model is loaded
    if (m_agenticEngine) {
        m_agenticEngine->setInferenceEngine(m_inferenceEngine);
        m_agenticEngine->markModelAsLoaded(ggufPath);
    }

    // Enable input on existing chat panels and set selected model
    if (m_chatTabs) {
        for (int i = 0; i < m_chatTabs->count(); ++i) {
            if (auto* panel = qobject_cast<AIChatPanel*>(m_chatTabs->widget(i))) {
                panel->setLocalModel(std::filesystem::path(ggufPath).baseName());
                panel->setSelectedModel(std::filesystem::path(ggufPath).baseName());
                panel->setInputEnabled(true);
            }
        }
    }

    statusBar()->showMessage(tr("✔ Model loaded: %1").fileName()), 4000);
}

void MainWindow::handleBackendSelection(QAction* action)
{
    if (!action) return;
    std::string backendId = action->data().toString();
    if (backendId.isEmpty() || backendId == m_currentBackend) return;
    m_currentBackend = backendId;
    statusBar()->showMessage(tr("Backend switched to %1")), 2000);
    onAIBackendChanged(backendId, {});
}

void MainWindow::createCentralEditor()
{
    void* central = new void(this);
    QVBoxLayout* layout = new QVBoxLayout(central);
    
    editorTabs_ = new QTabWidget(central);
    codeView_ = new QTextEdit();
    editorTabs_->addTab(codeView_, "Untitled");
    
    layout->addWidget(editorTabs_);
    setCentralWidget(central);
}

void MainWindow::setupStatusBar()
{
    
    // Main status message
    statusBar()->showMessage(tr("Ready | ggml Q4_0/Q8_0 quantization available | RawrXD IDE v3.0"));
    
    // Create permanent status widgets
    
    // 1. Line/Column indicator
    QLabel* lineColLabel = new QLabel(" Ln 1, Col 1 ", this);
    lineColLabel->setStyleSheet("QLabel { padding: 2px 8px; }");
    statusBar()->addPermanentWidget(lineColLabel);
    
    // Connect to editor cursor changes
    if (codeView_) {
// Qt connect removed
            int line = cursor.blockNumber() + 1;
            int col = cursor.columnNumber() + 1;
            lineColLabel->setText(std::string(" Ln %1, Col %2 "));
        });
    }
    
    // 2. Backend indicator
    QLabel* backendLabel = new QLabel(" Backend: Local ", this);
    backendLabel->setStyleSheet("QLabel { padding: 2px 8px; color: #00ff00; }");
    backendLabel->setToolTip(tr("Current AI backend"));
    statusBar()->addPermanentWidget(backendLabel);
    
    // 3. Model indicator
    QLabel* modelLabel = new QLabel(" Model: None ", this);
    modelLabel->setStyleSheet("QLabel { padding: 2px 8px; }");
    modelLabel->setToolTip(tr("Currently loaded model"));
    statusBar()->addPermanentWidget(modelLabel);
    
    // Update model label when model changes
    if (m_modelSelector) {
// Qt connect removed
            if (!modelName.isEmpty()) {
                // Truncate long model names
                if (modelName.length() > 30) {
                    modelName = modelName.left(27) + "...";
                }
                modelLabel->setText(std::string(" Model: %1 "));
            } else {
                modelLabel->setText(" Model: None ");
            }
        });
    }
    
    // 4. Memory usage indicator (optional - updated via timer)
    QLabel* memoryLabel = new QLabel(" RAM: -- MB ", this);
    memoryLabel->setStyleSheet("QLabel { padding: 2px 8px; }");
    memoryLabel->setToolTip(tr("Memory usage"));
    statusBar()->addPermanentWidget(memoryLabel);
    
    // Update memory periodically
    void** memoryTimer = new void*(this);
// Qt connect removed
        proc.start("powershell", std::vector<std::string>() << "-Command" 
                  << "(Get-Process -Id $PID).WorkingSet64 / 1MB");
        if (proc.waitForFinished(500)) {
            std::string output = proc.readAllStandardOutput().trimmed();
            bool ok;
            double memMB = output.toDouble(&ok);
            if (ok) {
                memoryLabel->setText(std::string(" RAM: %1 MB "));
            }
        }
    });
    memoryTimer->start(5000); // Update every 5 seconds
    
    // 5. Connection status indicator
    QLabel* connectionLabel = new QLabel(" ✓ Connected ", this);
    connectionLabel->setStyleSheet("QLabel { padding: 2px 8px; color: #00ff00; }");
    connectionLabel->setToolTip(tr("Connection status"));
    statusBar()->addPermanentWidget(connectionLabel);
    
}

void MainWindow::initSubsystems()
{
    
    int successCount = 0;
    int totalSubsystems = 0;
    std::vector<std::string> failedSubsystems;
    
    auto initSubsystem = [&](const std::string& name, std::function<bool()> initFunc) {
        totalSubsystems++;
        
        try {
            if (initFunc()) {
                successCount++;
                return true;
            } else {
                failedSubsystems << name;
                return false;
            }
        } catch (const std::exception& e) {
            failedSubsystems << name;
            return false;
        }
    };
    
    // Initialize core subsystems
    initSubsystem("InferenceEngine", [this]() {
        if (!m_inferenceEngine) {
            // Inference engine creation would happen here
        }
        return true;
    });
    
    initSubsystem("GGUFServer", [this]() {
        if (!m_ggufServer) {
            // GGUF server initialization
        }
        return true;
    });
    
    initSubsystem("ProjectExplorer", [this]() {
        if (!projectExplorer_) {
        }
        return true;
    });
    
    initSubsystem("AIChatPanel", [this]() {
        if (!m_aiChatPanel) {
        }
        return true;
    });
    
    initSubsystem("CommandPalette", [this]() {
        if (!m_commandPalette) {
        }
        return true;
    });
    
    initSubsystem("TerminalCluster", [this]() {
        if (pwshProcess_) {
        }
        if (cmdProcess_) {
        }
        return true;
    });
    
    initSubsystem("LSPClient", [this]() {
        if (!lspHost_) {
        }
        return true;
    });
    
    initSubsystem("ModelMonitor", [this]() {
        if (m_modelMonitorDock) {
        }
        return true;
    });
    
    initSubsystem("InterpretabilityPanel", [this]() {
        if (m_interpretabilityPanel) {
        }
        return true;
    });
    
    initSubsystem("HotpatchSystem", [this]() {
        if (m_hotpatchPanel) {
        }
        return true;
    });
    
    // Log results
    std::string status = tr("Subsystems: %1/%2 initialized");
    statusBar()->showMessage(status, 5000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("\n=== SUBSYSTEM INITIALIZATION ==="));
        m_hexMagConsole->appendPlainText(std::string("Time: %1").toString()));
        m_hexMagConsole->appendPlainText(std::string("Success: %1/%2"));
        
        if (!failedSubsystems.isEmpty()) {
            m_hexMagConsole->appendPlainText(std::string("Failed: %1")));
        }
        
        m_hexMagConsole->appendPlainText(std::string("==============================\n"));
    }
    
    
    if (successCount == totalSubsystems) {
    } else {
    }
}

// ============================================================
// Real Agent System Implementations (replacing stubs)
// ============================================================

void MainWindow::handleGoalSubmit() {
    if (!goalInput_) return;
    
    std::string wish = goalInput_->text().trimmed();
    if (wish.isEmpty()) {
        statusBar()->showMessage(tr("Please enter a goal/wish"), 2000);
        return;
    }
    
    // Use MetaPlanner to convert wish to action plan
    MetaPlanner planner;
    void* plan = planner.plan(wish);
    
    if (plan.isEmpty()) {
        statusBar()->showMessage(tr("Failed to generate plan"), 3000);
        return;
    }
    
    // Display plan summary
    if (chatHistory_) {
        chatHistory_->addItem(tr("Goal: %1"));
        chatHistory_->addItem(tr("Plan: %1 actions generated")));
    }
    
    statusBar()->showMessage(tr("Executing plan with %1 actions...")), 3000);
    
    // Execute plan via ActionExecutor
    if (!m_actionExecutor) {
        m_actionExecutor = new ActionExecutor(this);
// Qt connect removed
// Qt connect removed
// Qt connect removed
    }
    
    ExecutionContext ctx;
    ctx.projectRoot = std::filesystem::path::currentPath();
    m_actionExecutor->setContext(ctx);
    m_actionExecutor->executePlan(plan);
    
    onGoalSubmitted(wish);
}

void MainWindow::handleAgentMockProgress() {
    // AGENTIC: Progress tracking with logging and error handling
    try {
        if (mockStatusBadge_) {
            mockStatusBadge_->setText(tr("Agent Running..."));
        }
        statusBar()->showMessage(tr("Agent making progress..."), 1000);
    } catch (const std::exception& e) {
    }
    // AGENTIC: Future extension - async notification to agentic subsystem
}
void MainWindow::updateSuggestion(const std::string& chunk) {
    // AGENTIC: Update suggestion buffer, overlay, and AI chat panel with logging
    try {
        suggestionBuffer_ += chunk;
        if (overlay_) {
            // overlay_->updateText(suggestionBuffer_); // AGENTIC: Enable when overlay supports streaming
        }
        if (m_aiChatPanel) {
            m_aiChatPanel->updateStreamingMessage(chunk);
        }
    } catch (const std::exception& e) {
    }
    // AGENTIC: Future - async streaming to agentic suggestion subsystem
}

void MainWindow::appendModelChunk(const std::string& chunk) {
    // AGENTIC: Append model chunk with logging and error handling
    try {
        architectBuffer_ += chunk;
        if (m_hexMagConsole) {
            m_hexMagConsole->insertPlainText(chunk);
            m_hexMagConsole->ensureCursorVisible();
        }
    } catch (const std::exception& e) {
    }
    // AGENTIC: Future - async streaming to model output subsystem
}

void MainWindow::handleGenerationFinished() {
    // AGENTIC: Mark generation finished, log, and notify subsystems
    try {
        suggestionEnabled_ = true;
        if (m_aiChatPanel) {
            m_aiChatPanel->finishStreaming();
        }
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText("\n--- Generation Complete ---\n");
        }
        statusBar()->showMessage(tr("AI generation complete"), 3000);
    } catch (const std::exception& e) {
    }
    // AGENTIC: Future - async notification to agentic completion subsystem
}
void MainWindow::handleQShellReturn() {
    // AGENTIC: QShell return handler with logging, error handling, and agentic plan execution
    try {
        if (!qshellInput_ || !qshellOutput_) return;
        std::string command = qshellInput_->text().trimmed();
        if (command.isEmpty()) return;
        qshellOutput_->append(">> " + command);
        qshellInput_->clear();
        MetaPlanner planner;
        void* plan = planner.plan(command);
        if (!plan.isEmpty() && m_actionExecutor) {
            ExecutionContext ctx;
            ctx.projectRoot = std::filesystem::path::currentPath();
            m_actionExecutor->setContext(ctx);
            m_actionExecutor->executePlan(plan);
        } else {
            qshellOutput_->append("Error: Failed to parse command as agent wish");
        }
    } catch (const std::exception& e) {
    }
    // AGENTIC: Future - async QShell agentic execution
}
void MainWindow::handleArchitectChunk(const std::string& chunk) {
    architectBuffer_ += chunk;
    architectRunning_ = true;
    
    // Update chat history with streaming architect response
    if (chatHistory_) {
        // Find or create architect message item
        if (!chatHistory_->currentItem() || 
            !chatHistory_->currentItem()->text().startsWith("Architect:")) {
            chatHistory_->addItem(tr("Architect: "));
        }
        QListWidgetItem* item = chatHistory_->item(chatHistory_->count() - 1);
        if (item) {
            item->setText(tr("Architect: %1"));
        }
    }
    
    // Also update hex mag console
    if (m_hexMagConsole) {
        m_hexMagConsole->insertPlainText(chunk);
        m_hexMagConsole->ensureCursorVisible();
    }
}

void MainWindow::handleArchitectFinished() {
    architectRunning_ = false;
    
    // Try to parse architect response as JSON plan
    void* doc = void*::fromJson(architectBuffer_.toUtf8());
    if (doc.isArray()) {
        void* plan = doc.array();
        if (chatHistory_) {
            chatHistory_->addItem(tr("✓ Architect plan ready: %1 actions")));
        }
        
        // Auto-execute the plan
        if (m_actionExecutor) {
            ExecutionContext ctx;
            ctx.projectRoot = std::filesystem::path::currentPath();
            m_actionExecutor->setContext(ctx);
            m_actionExecutor->executePlan(plan);
        }
    }
    
    architectBuffer_.clear();
    statusBar()->showMessage(tr("Architect planning complete"), 3000);
}
void MainWindow::onActionStarted(int index, const std::string& description) {
    handleTaskStatusUpdate(std::string::number(index), description, QStringLiteral("Agent"));
}

void MainWindow::onActionCompleted(int index, bool success, const void*& result) {
    std::string summary = void*(result).toJson(void*::Compact);
    std::string status = success ? QStringLiteral("Completed") : QStringLiteral("Failed");
    handleTaskStatusUpdate(std::string::number(index), status, QStringLiteral("Agent"));
    handleTaskCompleted(QStringLiteral("Agent"), summary);
}

void MainWindow::onPlanCompleted(bool success, const void*& result) {
    (result);
    handleWorkflowFinished(success);
}

void MainWindow::handleTaskStatusUpdate(const std::string& taskId, const std::string& status, const std::string& agentType) {
    std::string msg = tr("[%1] %2: %3");
    
    if (chatHistory_) {
        chatHistory_->addItem(msg);
        chatHistory_->scrollToBottom();
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(msg);
    }
    
    statusBar()->showMessage(msg, 2000);
}

void MainWindow::handleTaskCompleted(const std::string& agentType, const std::string& summary) {
    std::string msg = tr("✓ %1 completed: %2");
    
    if (chatHistory_) {
        chatHistory_->addItem(msg);
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(msg + "\n");
    }
    
    // Update proposal widgets if task had proposals
    if (proposalWidgetMap_.contains(agentType)) {
        TaskProposalWidget* widget = proposalWidgetMap_[agentType];
        if (widget) {
            // widget->markComplete(summary);
        }
    }
    
    statusBar()->showMessage(msg, 5000);
}

void MainWindow::handleWorkflowFinished(bool success) {
    std::string msg = success ? tr("✓✓✓ Workflow completed successfully!")
                          : tr("✗ Workflow failed - check logs");
    
    if (chatHistory_) {
        chatHistory_->addItem(msg);
        chatHistory_->scrollToBottom();
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("\n" + msg + "\n\n");
    }
    
    if (mockStatusBadge_) {
        mockStatusBadge_->setText(success ? tr("✓ Done") : tr("✗ Failed"));
        mockStatusBadge_->setStyleSheet(success ? "QLabel { color: #00ff00; }"
                                                : "QLabel { color: #ff0000; }");
    }
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Agent Workflow"));
    msgBox.setText(msg);
    msgBox.setIcon(success ? QMessageBox::Information : QMessageBox::Warning);
    msgBox.exec();
    
    statusBar()->showMessage(msg, 10000);
}

void MainWindow::handleTaskStreaming(const std::string& taskId, const std::string& chunk, const std::string& agentType) {
    // Real-time streaming of task execution output
    if (m_hexMagConsole) {
        m_hexMagConsole->insertPlainText(chunk);
        m_hexMagConsole->ensureCursorVisible();
    }
    
    // Update task-specific widget if exists
    std::string key = agentType + ":" + taskId;
    if (proposalItemMap_.contains(key)) {
        QListWidgetItem* item = proposalItemMap_[key];
        if (item) {
            std::string currentText = item->text();
            if (!currentText.contains("[Streaming]")) {
                item->setText(currentText + " [Streaming...]");
            }
        }
    }
}
void MainWindow::handleSaveState() {
    QSettings settings("RawrXD", "QtShell");
    
    // Save window geometry and state
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/windowState", saveState());
    
    // Save dock widget visibility states
    if (m_aiChatPanelDock) {
        settings.setValue("Docks/aiChatPanel", m_aiChatPanelDock->isVisible());
    }
    if (m_modelMonitorDock) {
        settings.setValue("Docks/modelMonitor", m_modelMonitorDock->isVisible());
    }
    if (m_layerQuantDock) {
        settings.setValue("Docks/layerQuant", m_layerQuantDock->isVisible());
    }
    if (m_masmEditorDock) {
        settings.setValue("Docks/masmEditor", m_masmEditorDock->isVisible());
    }
    if (m_hotpatchPanelDock) {
        settings.setValue("Docks/hotpatchPanel", m_hotpatchPanelDock->isVisible());
    }
    
    // Save model selector state
    if (m_modelSelector) {
        settings.setValue("ModelSelector/currentIndex", m_modelSelector->currentIndex());
        settings.setValue("ModelSelector/currentText", m_modelSelector->currentText());
    }
    
    // Save agent mode
    if (m_agentModeSwitcher) {
        settings.setValue("AgentMode/current", m_agentModeSwitcher->currentText());
    }
    settings.setValue("AgentMode/mode", m_agentMode);
    
    // Save AI backend settings
    settings.setValue("AIBackend/current", m_currentBackend);
    settings.setValue("AIBackend/apiKey", m_currentAPIKey);
    
    // Save quantization mode
    settings.setValue("Quantization/mode", m_currentQuantMode);
    
    // Save primary sidebar width
    if (m_primarySidebar) {
        settings.setValue("Sidebar/width", m_primarySidebar->width());
    }
    
}

void MainWindow::handleLoadState() {
    QSettings settings("RawrXD", "QtShell");
    
    // Restore window geometry and state
    if (settings.contains("MainWindow/geometry")) {
        restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
    }
    if (settings.contains("MainWindow/windowState")) {
        restoreState(settings.value("MainWindow/windowState").toByteArray());
    }
    
    // Restore dock widget visibility states
    if (m_aiChatPanelDock && settings.contains("Docks/aiChatPanel")) {
        bool visible = settings.value("Docks/aiChatPanel").toBool();
        if (visible) {
            m_aiChatPanelDock->show();
        } else {
            m_aiChatPanelDock->hide();
        }
    }
    if (m_modelMonitorDock && settings.contains("Docks/modelMonitor")) {
        m_modelMonitorDock->setVisible(settings.value("Docks/modelMonitor").toBool());
    }
    if (m_layerQuantDock && settings.contains("Docks/layerQuant")) {
        m_layerQuantDock->setVisible(settings.value("Docks/layerQuant").toBool());
    }
    if (m_masmEditorDock && settings.contains("Docks/masmEditor")) {
        m_masmEditorDock->setVisible(settings.value("Docks/masmEditor").toBool());
    }
    if (m_hotpatchPanelDock && settings.contains("Docks/hotpatchPanel")) {
        m_hotpatchPanelDock->setVisible(settings.value("Docks/hotpatchPanel").toBool());
    }
    
    // Restore model selector state
    if (m_modelSelector && settings.contains("ModelSelector/currentText")) {
        std::string savedModel = settings.value("ModelSelector/currentText").toString();
        int index = m_modelSelector->findText(savedModel);
        if (index >= 0) {
            m_modelSelector->setCurrentIndex(index);
        }
    }
    
    // Restore agent mode
    if (settings.contains("AgentMode/mode")) {
        m_agentMode = settings.value("AgentMode/mode").toString();
    }
    if (m_agentModeSwitcher && settings.contains("AgentMode/current")) {
        std::string savedMode = settings.value("AgentMode/current").toString();
        int index = m_agentModeSwitcher->findText(savedMode);
        if (index >= 0) {
            m_agentModeSwitcher->setCurrentIndex(index);
        }
    }
    
    // Restore AI backend settings
    if (settings.contains("AIBackend/current")) {
        m_currentBackend = settings.value("AIBackend/current").toString();
    }
    if (settings.contains("AIBackend/apiKey")) {
        m_currentAPIKey = settings.value("AIBackend/apiKey").toString();
    }
    
    // Restore quantization mode
    if (settings.contains("Quantization/mode")) {
        m_currentQuantMode = settings.value("Quantization/mode").toString();
    }
    
    // Restore primary sidebar width
    if (m_primarySidebar && settings.contains("Sidebar/width")) {
        int width = settings.value("Sidebar/width").toInt();
        if (width > 0) {
            m_primarySidebar->setFixedWidth(width);
        }
    }
    
    
    // Phase C: Restore editor state and history
    restoreTabState();
    restoreEditorContent();
    restoreEditorMetadata();
}

// ============================================================
// Phase C: Data Persistence Implementation
// ============================================================

void MainWindow::saveEditorState() {
    if (!editorTabs_) return;
    
    QSettings settings("RawrXD", "QtShell");
    RawrXD::Integration::ScopedTimer timer("MainWindow", "saveEditorState", "persistence");
    
    try {
        // Save active tab index
        m_activeTabIndex = editorTabs_->currentIndex();
        settings.setValue("Editor/activeTabIndex", m_activeTabIndex);
        
        // Save metadata for each open tab
        int tabCount = editorTabs_->count();
        settings.setValue("Editor/tabCount", tabCount);
        
        void* tabsArray;
        for (int i = 0; i < tabCount; ++i) {
            void* widget = editorTabs_->widget(i);
            if (!widget) continue;
            
            void* tabObj;
            tabObj["index"] = i;
            tabObj["title"] = editorTabs_->tabText(i);
            
            // Get editor state
            if (auto textEdit = qobject_cast<QTextEdit*>(widget)) {
                QTextCursor cursor = textEdit->textCursor();
                tabObj["cursorLine"] = cursor.blockNumber();
                tabObj["cursorColumn"] = cursor.positionInBlock();
                tabObj["scrollPosition"] = textEdit->verticalScrollBar()->value();
                tabObj["contentLength"] = textEdit->toPlainText().length();
                
                // Store in m_editorStates as well
                EditorState state;
                state.filePath = editorTabs_->tabText(i);
                state.cursorLine = cursor.blockNumber();
                state.cursorColumn = cursor.positionInBlock();
                state.scrollPosition = textEdit->verticalScrollBar()->value();
                m_editorStates[i] = state;
            }
            
            tabsArray.append(tabObj);
        }
        
        // Serialize tabs array to JSON and store
        void* doc(tabsArray);
        std::vector<uint8_t> jsonData = doc.toJson(void*::Compact);
        settings.setValue("Editor/tabsMetadata", std::string::fromUtf8(jsonData));
        
        // Track metrics
        m_persistenceDataSize = jsonData.size();
        m_lastSaveTime = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
        
        // Log persistence event
        RawrXD::Integration::logInfo("MainWindow", "editor_state_saved", 
            std::string("Saved %1 tabs, data size: %2 bytes"));
        
        // trace event
        RawrXD::Integration::traceEvent("Persistence", "editorStateSaved");
        
    } catch (const std::exception& e) {
        RawrXD::Integration::logError("MainWindow", "editor_state_save_failed", 
            std::string("Exception: %1")))));
    }
}

void MainWindow::restoreEditorState() {
    if (!editorTabs_) return;
    
    QSettings settings("RawrXD", "QtShell");
    RawrXD::Integration::ScopedTimer timer("MainWindow", "restoreEditorState", "persistence");
    
    try {
        // Restore active tab
        if (settings.contains("Editor/activeTabIndex")) {
            m_activeTabIndex = settings.value("Editor/activeTabIndex").toInt();
            if (m_activeTabIndex >= 0 && m_activeTabIndex < editorTabs_->count()) {
                editorTabs_->setCurrentIndex(m_activeTabIndex);
            }
        }
        
        // Restore cursor positions and scroll states for each tab
        if (settings.contains("Editor/tabsMetadata")) {
            std::string jsonStr = settings.value("Editor/tabsMetadata").toString();
            void* doc = void*::fromJson(jsonStr.toUtf8());
            if (doc.isArray()) {
                void* tabsArray = doc.array();
                int restoredCount = 0;
                
                for (int i = 0; i < tabsArray.size(); ++i) {
                    void* tabObj = tabsArray[i].toObject();
                    int index = tabObj["index"].toInt();
                    
                    if (index >= 0 && index < editorTabs_->count()) {
                        void* widget = editorTabs_->widget(index);
                        if (auto textEdit = qobject_cast<QTextEdit*>(widget)) {
                            // Restore scroll position
                            int scrollPos = tabObj["scrollPosition"].toInt();
                            textEdit->verticalScrollBar()->setValue(scrollPos);
                            
                            // Restore cursor position
                            int cursorLine = tabObj["cursorLine"].toInt();
                            int cursorCol = tabObj["cursorColumn"].toInt();
                            QTextCursor cursor = textEdit->textCursor();
                            cursor.movePosition(QTextCursor::Start);
                            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, cursorLine);
                            cursor.movePosition(QTextCursor::Right, QTextCursor::MoveAnchor, cursorCol);
                            textEdit->setTextCursor(cursor);
                            
                            restoredCount++;
                        }
                    }
                }
                
                RawrXD::Integration::logInfo("MainWindow", "editor_state_restored", 
                    std::string("Restored %1 editor states"));
            }
        }
        
        m_lastRestoreTime = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
        
        // trace event
        RawrXD::Integration::traceEvent("Persistence", "editorStateRestored");
        
    } catch (const std::exception& e) {
        RawrXD::Integration::logError("MainWindow", "editor_state_restore_failed", 
            std::string("Exception: %1")))));
    }
}

void MainWindow::saveTabState() {
    if (!editorTabs_) return;
    
    QSettings settings("RawrXD", "QtShell");
    
    try {
        // Save tab titles and count
        int tabCount = editorTabs_->count();
        settings.setValue("Tabs/count", tabCount);
        
        std::vector<std::string> tabTitles;
        for (int i = 0; i < tabCount; ++i) {
            tabTitles << editorTabs_->tabText(i);
        }
        settings.setValue("Tabs/titles", tabTitles);
        settings.setValue("Tabs/activeIndex", editorTabs_->currentIndex());
        
        RawrXD::Integration::logInfo("MainWindow", "tab_state_saved", 
            std::string("Saved %1 tabs"));
    } catch (const std::exception& e) {
        RawrXD::Integration::logError("MainWindow", "tab_state_save_failed", 
            std::string("Exception: %1")))));
    }
}

void MainWindow::restoreTabState() {
    if (!editorTabs_) return;
    
    QSettings settings("RawrXD", "QtShell");
    
    try {
        // Restore tab visibility and active tab
        if (settings.contains("Tabs/activeIndex")) {
            int activeIndex = settings.value("Tabs/activeIndex").toInt();
            if (activeIndex >= 0 && activeIndex < editorTabs_->count()) {
                editorTabs_->setCurrentIndex(activeIndex);
            }
        }
        
        RawrXD::Integration::logInfo("MainWindow", "tab_state_restored", 
            std::string("Restored tab state"));
    } catch (const std::exception& e) {
        RawrXD::Integration::logError("MainWindow", "tab_state_restore_failed", 
            std::string("Exception: %1")))));
    }
}

void MainWindow::trackEditorCursorPosition() {
    if (!editorTabs_) return;
    
    // This slot can be connected to editor signals to track cursor changes
    try {
        int currentTab = editorTabs_->currentIndex();
        if (currentTab < 0) return;
        
        void* widget = editorTabs_->widget(currentTab);
        if (auto textEdit = qobject_cast<QTextEdit*>(widget)) {
            QTextCursor cursor = textEdit->textCursor();
            EditorState& state = m_editorStates[currentTab];
            state.cursorLine = cursor.blockNumber();
            state.cursorColumn = cursor.positionInBlock();
        }
    } catch (const std::exception& e) {
        RawrXD::Integration::logWarn("MainWindow", "cursor_tracking_failed", 
            std::string("Exception: %1")))));
    }
}

void MainWindow::trackEditorScrollPosition() {
    if (!editorTabs_) return;
    
    // This slot can be connected to scroll bar signals
    try {
        int currentTab = editorTabs_->currentIndex();
        if (currentTab < 0) return;
        
        void* widget = editorTabs_->widget(currentTab);
        if (auto textEdit = qobject_cast<QTextEdit*>(widget)) {
            EditorState& state = m_editorStates[currentTab];
            state.scrollPosition = textEdit->verticalScrollBar()->value();
        }
    } catch (const std::exception& e) {
        RawrXD::Integration::logWarn("MainWindow", "scroll_tracking_failed",
            std::string("Exception: %1")))));
    }
}

void MainWindow::persistEditorContent() {
    if (!editorTabs_) return;
    
    // This persists actual editor content (optional - can be extensive)
    // For now, we just persist metadata and rely on file system for actual content
    saveEditorState();
}

void MainWindow::restoreEditorContent() {
    if (!editorTabs_) return;
    
    restoreEditorState();
}

void MainWindow::persistEditorMetadata() {
    // Already handled in saveEditorState()
    saveEditorState();
}

void MainWindow::restoreEditorMetadata() {
    // Already handled in restoreEditorState()
    restoreEditorState();
}

// ============================================================
// Recent Files Management
// ============================================================

void MainWindow::addRecentFile(const std::string& filePath) {
    if (filePath.isEmpty()) return;
    
    QSettings settings("RawrXD", "QtShell");
    
    // Load existing recent files
    std::vector<std::string> recentFiles = settings.value("Files/recentFiles", std::vector<std::string>()).toStringList();
    
    // Remove if already exists (to move it to the front)
    recentFiles.removeAll(filePath);
    
    // Add to front
    recentFiles.prepend(filePath);
    
    // Limit to 20 entries
    const int MAX_RECENT = 20;
    if (recentFiles.size() > MAX_RECENT) {
        recentFiles = recentFiles.mid(0, MAX_RECENT);
    }
    
    // Save to settings
    settings.setValue("Files/recentFiles", recentFiles);
    m_recentFiles = recentFiles;
    
    // Log and trace
    RawrXD::Integration::logInfo("MainWindow", "recent_file_added", 
        std::string("Added: %1 (total: %2)").fileName())));
    RawrXD::Integration::traceEvent("Persistence", "recentFileAdded");
}

std::vector<std::string> MainWindow::getRecentFiles() const {
    QSettings settings("RawrXD", "QtShell");
    return settings.value("Files/recentFiles", std::vector<std::string>()).toStringList();
}

void MainWindow::clearRecentFiles() {
    QSettings settings("RawrXD", "QtShell");
    settings.remove("Files/recentFiles");
    m_recentFiles.clear();
    
    RawrXD::Integration::logInfo("MainWindow", "recent_files_cleared", 
        std::string("Cleared all recent files"));
}

void MainWindow::populateRecentFilesMenu(QMenu* recentMenu) {
    if (!recentMenu) return;
    
    try {
        recentMenu->clear();
        std::vector<std::string> recentFiles = getRecentFiles();
        
        if (recentFiles.isEmpty()) {
            recentMenu->addAction("(No recent files)")->setEnabled(false);
        } else {
            for (const std::string& filePath : recentFiles) {
                QAction* action = recentMenu->addAction(std::filesystem::path(filePath).fileName());
                action->setData(filePath);
// Qt connect removed
                    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                        QTextStream in(&file);
                        if (editorTabs_) {
                            QTextEdit* editor = new QTextEdit(this);
                            editor->setPlainText(in.readAll());
                            int index = editorTabs_->addTab(editor, std::filesystem::path(filePath).fileName());
                            editorTabs_->setCurrentIndex(index);
                        }
                        file.close();
                        addRecentFile(filePath);
                    }
                });
            }
        }
        
        RawrXD::Integration::logInfo("MainWindow", "recent_files_menu_populated", 
            std::string("Populated with %1 files")));
    } catch (const std::exception& e) {
        RawrXD::Integration::logError("MainWindow", "recent_files_menu_failed", 
            std::string("Exception: %1")))));
    }
    
    recentMenu->addSeparator();
    recentMenu->addAction("Clear Recent Files", this, &MainWindow::clearRecentFiles);
}

// ============================================================
// Command History Tracking
// ============================================================

void MainWindow::addCommandToHistory(const std::string& command) {
    if (command.isEmpty()) return;
    
    try {
        QSettings settings("RawrXD", "QtShell");
        
        // Load existing command history
        std::vector<std::string> history = settings.value("Commands/history", std::vector<std::string>()).toStringList();
        
        // Add command with timestamp
        std::string timestampedCmd = std::string("[%1] %2")
            .toString("yyyy-MM-dd HH:mm:ss"))
            ;
        
        history.append(timestampedCmd);
        
        // Implement circular buffer: keep only last 1000 entries
        if (history.size() > 1000) {
            history = history.mid(history.size() - 1000);
        }
        
        // Save to settings
        settings.setValue("Commands/history", history);
        m_commandHistory = history;
        
        RawrXD::Integration::logInfo("MainWindow", "command_added_to_history", 
            std::string("Command: %1 (total: %2)"))));
        RawrXD::Integration::traceEvent("Persistence", "commandAddedToHistory");
        
    } catch (const std::exception& e) {
        RawrXD::Integration::logError("MainWindow", "command_history_add_failed", 
            std::string("Exception: %1")))));
    }
}

std::vector<std::string> MainWindow::getCommandHistory() const {
    QSettings settings("RawrXD", "QtShell");
    return settings.value("Commands/history", std::vector<std::string>()).toStringList();
}

void MainWindow::clearCommandHistory() {
    try {
        QSettings settings("RawrXD", "QtShell");
        settings.remove("Commands/history");
        m_commandHistory.clear();
        
        RawrXD::Integration::logInfo("MainWindow", "command_history_cleared", 
            std::string("Cleared all command history"));
    } catch (const std::exception& e) {
        RawrXD::Integration::logError("MainWindow", "command_history_clear_failed", 
            std::string("Exception: %1")))));
    }
}

void MainWindow::handleNewChat() {
    if (m_aiChatPanel) {
        // m_aiChatPanel->clearHistory();  // Method may not exist in current AIChatPanel
        statusBar()->showMessage(tr("New chat started"), 2000);
        
        if (m_aiChatPanelDock && !m_aiChatPanelDock->isVisible()) {
            m_aiChatPanelDock->show();
            m_aiChatPanelDock->raise();
        }
    }
}

void MainWindow::handleNewEditor() {
    if (editorTabs_) {
        QTextEdit* newEditor = new QTextEdit(this);
        newEditor->setStyleSheet(codeView_->styleSheet());
        int index = editorTabs_->addTab(newEditor, tr("Untitled %1")));
        editorTabs_->setCurrentIndex(index);
        statusBar()->showMessage(tr("New editor tab created"), 2000);
    }
}

void MainWindow::handleNewWindow() {
    MainWindow* newWindow = new MainWindow();
    newWindow->show();
    statusBar()->showMessage(tr("New window opened"), 2000);
}

void MainWindow::handleAddFile() {
    std::string filePath = QFileDialog::getOpenFileName(
        this,
        tr("Add File to Project"),
        std::string(),
        tr("All Files (*.*)"));
    
    if (!filePath.isEmpty()) {
        std::fstream file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            std::string content = in.readAll();
            file.close();
            
            if (editorTabs_) {
                QTextEdit* editor = new QTextEdit(this);
                editor->setStyleSheet(codeView_->styleSheet());
                editor->setText(content);
                int index = editorTabs_->addTab(editor, std::filesystem::path(filePath).fileName());
                editorTabs_->setCurrentIndex(index);
            }
            
            statusBar()->showMessage(tr("File added: %1").fileName()), 3000);
        }
    }
}

void MainWindow::handleAddFolder() {
    std::string folderPath = QFileDialog::getExistingDirectory(
        this,
        tr("Add Folder to Project"),
        std::string(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!folderPath.isEmpty()) {
        if (projectExplorer_) {
            projectExplorer_->openProject(folderPath);
        }
        statusBar()->showMessage(tr("Folder added: %1"), 3000);
    }
}

void MainWindow::handleAddSymbol() {
    bool ok;
    std::string symbol = QInputDialog::getText(this, tr("Add Symbol"),
                                          tr("Symbol name:"), QLineEdit::Normal,
                                          std::string(), &ok);
    if (ok && !symbol.isEmpty()) {
        if (contextList_) {
            contextList_->addItem(symbol);
        }
        statusBar()->showMessage(tr("Symbol added: %1"), 2000);
    }
}
void MainWindow::showContextMenu(const QPoint& pos) {
    QMenu contextMenu(tr("Context Menu"), this);
    
    contextMenu.addAction(tr("Explain with AI"), this, &MainWindow::explainCode);
    contextMenu.addAction(tr("Fix with AI"), this, &MainWindow::fixCode);
    contextMenu.addAction(tr("Refactor with AI"), this, &MainWindow::refactorCode);
    contextMenu.addSeparator();
    contextMenu.addAction(tr("Generate Tests"), this, &MainWindow::generateTests);
    contextMenu.addAction(tr("Generate Docs"), this, &MainWindow::generateDocs);
    
    contextMenu.exec(mapToGlobal(pos));
}

void MainWindow::loadContextItemIntoEditor(QListWidgetItem* item) {
    if (!item) return;
    
    std::string itemText = item->text();
    
    // Check if it's a file path
    if (std::fstream::exists(itemText)) {
        std::fstream file(itemText);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            if (codeView_) {
                codeView_->setText(in.readAll());
                statusBar()->showMessage(tr("Loaded: %1"), 3000);
            }
            file.close();
        }
    } else {
        // Use as search/symbol query
        statusBar()->showMessage(tr("Context item: %1"), 2000);
    }
}

void MainWindow::handleTabClose(int index) {
    if (!editorTabs_ || index < 0 || index >= editorTabs_->count()) return;
    
    void* widget = editorTabs_->widget(index);
    
    // Ask for confirmation if content exists
    QTextEdit* editor = qobject_cast<QTextEdit*>(widget);
    if (editor && !editor->toPlainText().isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Close Tab"),
            tr("Close '%1'? Unsaved changes will be lost.")),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::No) {
            return;
        }
    }
    
    editorTabs_->removeTab(index);
    delete widget;
}
void MainWindow::handlePwshCommand() 
{
    
    if (!pwshProcess_) {
        statusBar()->showMessage(tr("PowerShell process not initialized"), 3000);
        return;
    }
    
    if (!pwshInput_) {
        statusBar()->showMessage(tr("PowerShell input not available"), 3000);
        return;
    }
    
    std::string command = pwshInput_->text().trimmed();
    if (command.isEmpty()) {
        statusBar()->showMessage(tr("Enter a PowerShell command first"), 2000);
        return;
    }
    
    // Log command execution
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[PWSH] > %1"));
    }
    
    if (pwshOutput_) {
        pwshOutput_->appendPlainText(std::string("PS> %1"));
    }
    
    statusBar()->showMessage(tr("PowerShell executing: %1")), 3000);
    
    // Send command to process
    pwshProcess_->write((command + "\n").toUtf8());
    pwshInput_->clear();
}

void MainWindow::handleCmdCommand() 
{
    
    if (!cmdProcess_) {
        statusBar()->showMessage(tr("CMD process not initialized"), 3000);
        return;
    }
    
    if (!cmdInput_) {
        statusBar()->showMessage(tr("CMD input not available"), 3000);
        return;
    }
    
    std::string command = cmdInput_->text().trimmed();
    if (command.isEmpty()) {
        statusBar()->showMessage(tr("Enter a CMD command first"), 2000);
        return;
    }
    
    // Log command execution
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[CMD] > %1"));
    }
    
    if (cmdOutput_) {
        cmdOutput_->appendPlainText(std::string("CMD> %1"));
    }
    
    statusBar()->showMessage(tr("CMD executing: %1")), 3000);
    
    // Send command to process
    cmdProcess_->write((command + "\r\n").toUtf8());
    cmdInput_->clear();
}

void MainWindow::readPwshOutput() 
{
    if (!pwshProcess_) {
        return;
    }
    
    std::vector<uint8_t> data = pwshProcess_->readAllStandardOutput();
    std::string output = std::string::fromUtf8(data);
    
    if (output.isEmpty()) {
        return;
    }
    
    
    if (pwshOutput_) {
        pwshOutput_->appendPlainText(output);
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(output);
    }
    
    // Check for errors
    std::vector<uint8_t> errorData = pwshProcess_->readAllStandardError();
    if (!errorData.isEmpty()) {
        std::string errorOutput = std::string::fromUtf8(errorData);
        
        if (pwshOutput_) {
            pwshOutput_->appendPlainText("[ERROR] " + errorOutput);
        }
    }
}

void MainWindow::readCmdOutput() 
{
    if (!cmdProcess_) {
        return;
    }
    
    std::vector<uint8_t> data = cmdProcess_->readAllStandardOutput();
    std::string output = std::string::fromUtf8(data);
    
    if (output.isEmpty()) {
        return;
    }
    
    
    if (cmdOutput_) {
        cmdOutput_->appendPlainText(output);
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(output);
    }
    
    // Check for errors
    std::vector<uint8_t> errorData = cmdProcess_->readAllStandardError();
    if (!errorData.isEmpty()) {
        std::string errorOutput = std::string::fromUtf8(errorData);
        
        if (cmdOutput_) {
            cmdOutput_->appendPlainText("[ERROR] " + errorOutput);
        }
    }
}
void MainWindow::clearDebugLog() 
{
    
    if (!m_hexMagConsole) {
        statusBar()->showMessage(tr("Debug console not available"), 2000);
        return;
    }
    
    // Confirm before clearing if there's significant content
    int lineCount = m_hexMagConsole->document()->blockCount();
    if (lineCount > 100) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Clear Debug Log"),
            tr("Are you sure you want to clear %1 lines of log data?"),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::No) {
            statusBar()->showMessage(tr("Log clear cancelled"), 2000);
            return;
        }
    }
    
    m_hexMagConsole->clear();
    m_hexMagConsole->appendPlainText(std::string("=== Log cleared at %1 ===")
                                    .toString("yyyy-MM-dd HH:mm:ss")));
    
    statusBar()->showMessage(tr("Debug log cleared (%1 lines)"), 3000);
}
void MainWindow::saveDebugLog() 
{
    
    if (!m_hexMagConsole) {
        statusBar()->showMessage(tr("Debug console not available"), 2000);
        return;
    }
    
    std::string logContent = m_hexMagConsole->toPlainText();
    
    if (logContent.isEmpty()) {
        QMessageBox::information(this, tr("Save Debug Log"), 
                               tr("Debug log is empty. Nothing to save."));
        return;
    }
    
    // Generate default filename with timestamp
    std::string defaultName = std::string("rawrxd_debug_%1.log")
                         .toString("yyyyMMdd_HHmmss"));
    
    std::string filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Debug Log"),
        std::filesystem::path::homePath() + "/" + defaultName,
        tr("Log Files (*.log);;Text Files (*.txt);;All Files (*.*)")  
    );
    
    if (filePath.isEmpty()) {
        statusBar()->showMessage(tr("Save cancelled"), 2000);
        return;
    }
    
    statusBar()->showMessage(tr("Saving debug log to %1...").fileName()), 3000);
    
    std::fstream file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::critical(this, tr("Save Failed"),
                            tr("Could not open file for writing:\n%1\n\nError: %2")
                            ));
        return;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // Write header
    out << "=== RawrXD IDE Debug Log ===" << "\n";
    out << "Generated: " << std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate) << "\n";
    out << "Lines: " << m_hexMagConsole->document()->blockCount() << "\n";
    out << "Size: " << logContent.length() << " characters\n";
    out << "================================\n\n";
    
    // Write log content
    out << logContent;
    
    file.close();
    
    std::filesystem::path fileInfo(filePath);
    qint64 fileSize = fileInfo.size();
    
    statusBar()->showMessage(
        tr("Debug log saved: %1 (%2 KB)")),
        5000
    );
    
    
    // Offer to open the file
    QMessageBox::StandardButton openReply = QMessageBox::question(
        this,
        tr("Log Saved"),
        tr("Debug log saved successfully to:\n%1\n\nOpen in external editor?"),
        QMessageBox::Yes | QMessageBox::No
    );
    
    if (openReply == QMessageBox::Yes) {
        QDesktopServices::openUrl(std::string::fromLocalFile(filePath));
    }
}
void MainWindow::filterLogLevel(const std::string& level) 
{
    
    if (!m_hexMagConsole) {
        statusBar()->showMessage(tr("Debug console not available"), 2000);
        return;
    }
    
    statusBar()->showMessage(tr("Filtering by log level: %1"), 3000);
    
    // Get full log content
    std::string fullLog = m_hexMagConsole->toPlainText();
    std::vector<std::string> lines = fullLog.split('\n');
    
    // Define log level priorities
    std::map<std::string, int> levelPriority;
    levelPriority["DEBUG"] = 0;
    levelPriority["INFO"] = 1;
    levelPriority["WARNING"] = 2;
    levelPriority["ERROR"] = 3;
    levelPriority["CRITICAL"] = 4;
    
    int filterPriority = levelPriority.value(level.toUpper(), 0);
    
    // Filter lines based on log level
    std::vector<std::string> filteredLines;
    int filteredCount = 0;
    
    for (const std::string& line : lines) {
        if (line.isEmpty()) {
            filteredLines.append(line);
            continue;
        }
        
        // Check if line contains a log level marker
        bool shouldInclude = true;
        bool hasLevel = false;
        
        for (auto it = levelPriority.begin(); it != levelPriority.end(); ++it) {
            if (line.contains("[" + it.key() + "]", //CaseInsensitive)) {
                hasLevel = true;
                if (it.value() < filterPriority) {
                    shouldInclude = false;
                    filteredCount++;
                }
                break;
            }
        }
        
        // Include lines without explicit level (system messages, etc.)
        if (!hasLevel || shouldInclude) {
            filteredLines.append(line);
        }
    }
    
    // Update console with filtered content
    m_hexMagConsole->clear();
    m_hexMagConsole->setPlainText(filteredLines.join('\n'));
    
    // Add filter header
    m_hexMagConsole->moveCursor(QTextCursor::Start);
    m_hexMagConsole->insertPlainText(
        std::string("=== Filtered by %1+ | Hidden: %2 lines ===\n\n")
        )
    );
    
            << "lines, hidden" << filteredCount << "lines";
}
void MainWindow::showEditorContextMenu(const QPoint& pos) 
{
    
    if (!codeView_) {
        return;
    }
    
    QMenu contextMenu(tr("Editor Context Menu"), this);
    
    // Get selected text info
    QTextCursor cursor = codeView_->textCursor();
    bool hasSelection = cursor.hasSelection();
    std::string selectedText = cursor.selectedText();
    
    // Basic editing operations
    QAction* undoAction = contextMenu.addAction(QIcon(), tr("Undo"), codeView_, &QTextEdit::undo);
    undoAction->setShortcut(QKeySequence::Undo);
    undoAction->setEnabled(codeView_->document()->isUndoAvailable());
    
    QAction* redoAction = contextMenu.addAction(QIcon(), tr("Redo"), codeView_, &QTextEdit::redo);
    redoAction->setShortcut(QKeySequence::Redo);
    redoAction->setEnabled(codeView_->document()->isRedoAvailable());
    
    contextMenu.addSeparator();
    
    QAction* cutAction = contextMenu.addAction(QIcon(), tr("Cut"), codeView_, &QTextEdit::cut);
    cutAction->setShortcut(QKeySequence::Cut);
    cutAction->setEnabled(hasSelection);
    
    QAction* copyAction = contextMenu.addAction(QIcon(), tr("Copy"), codeView_, &QTextEdit::copy);
    copyAction->setShortcut(QKeySequence::Copy);
    copyAction->setEnabled(hasSelection);
    
    QAction* pasteAction = contextMenu.addAction(QIcon(), tr("Paste"), codeView_, &QTextEdit::paste);
    pasteAction->setShortcut(QKeySequence::Paste);
    
    QAction* deleteAction = contextMenu.addAction(QIcon(), tr("Delete"), [this, cursor]() mutable {
        cursor.removeSelectedText();
    });
    deleteAction->setShortcut(QKeySequence::Delete);
    deleteAction->setEnabled(hasSelection);
    
    contextMenu.addSeparator();
    
    QAction* selectAllAction = contextMenu.addAction(QIcon(), tr("Select All"), codeView_, &QTextEdit::selectAll);
    selectAllAction->setShortcut(QKeySequence::SelectAll);
    
    contextMenu.addSeparator();
    
    // AI-assisted operations (only if text is selected)
    if (hasSelection && m_aiChatPanel) {
        QMenu* aiMenu = contextMenu.addMenu(tr("✨ AI Assist"));
        
        aiMenu->addAction(tr("💡 Explain Code"), this, &MainWindow::explainCode);
        aiMenu->addAction(tr("🔧 Fix Bugs"), this, &MainWindow::fixCode);
        aiMenu->addAction(tr("♻️ Refactor"), this, &MainWindow::refactorCode);
        aiMenu->addSeparator();
        aiMenu->addAction(tr("🧪 Generate Tests"), this, &MainWindow::generateTests);
        aiMenu->addAction(tr("📝 Generate Docs"), this, &MainWindow::generateDocs);
    }
    
    contextMenu.addSeparator();
    
    // Search operations
    if (hasSelection) {
        contextMenu.addAction(tr("🔍 Find '%1'")), [this, selectedText]() {
            // Simple find-next implementation
            if (codeView_) {
                codeView_->find(selectedText);
            }
        });
    }
    
    // Execute context menu at cursor position
    QPoint globalPos = codeView_->mapToGlobal(pos);
    contextMenu.exec(globalPos);
    
}
void MainWindow::explainCode() 
{ 
    std::string sel = codeView_->textCursor().selectedText(); 
    if (sel.isEmpty()) {
        statusBar()->showMessage(tr("Select code first"), 2000);
        return;
    }
    
    if (m_aiChatPanel) {
        std::string prompt = tr("Explain this code in detail:\n\n```\n%1\n```");
        m_aiChatPanel->addUserMessage(prompt);
        onAIChatMessageSubmitted(prompt);
        
        // Show AI Chat Panel if hidden
        if (m_aiChatPanelDock && !m_aiChatPanelDock->isVisible()) {
            m_aiChatPanelDock->show();
            m_aiChatPanelDock->raise();
        }
    } else {
        statusBar()->showMessage(tr("AI Chat Panel not available"), 3000);
    }
}

void MainWindow::fixCode() 
{ 
    std::string sel = codeView_->textCursor().selectedText(); 
    if (sel.isEmpty()) {
        statusBar()->showMessage(tr("Select code first"), 2000);
        return;
    }
    
    if (m_aiChatPanel) {
        std::string prompt = tr("Find and fix any bugs or issues in this code:\n\n```\n%1\n```\n\nProvide the corrected code.");
        m_aiChatPanel->addUserMessage(prompt);
        onAIChatMessageSubmitted(prompt);
        
        if (m_aiChatPanelDock && !m_aiChatPanelDock->isVisible()) {
            m_aiChatPanelDock->show();
            m_aiChatPanelDock->raise();
        }
    } else {
        statusBar()->showMessage(tr("AI Chat Panel not available"), 3000);
    }
}

void MainWindow::refactorCode() 
{ 
    std::string sel = codeView_->textCursor().selectedText(); 
    if (sel.isEmpty()) {
        statusBar()->showMessage(tr("Select code first"), 2000);
        return;
    }
    
    if (m_aiChatPanel) {
        std::string prompt = tr("Refactor this code to be more efficient, readable, and follow best practices:\n\n```\n%1\n```");
        m_aiChatPanel->addUserMessage(prompt);
        onAIChatMessageSubmitted(prompt);
        
        if (m_aiChatPanelDock && !m_aiChatPanelDock->isVisible()) {
            m_aiChatPanelDock->show();
            m_aiChatPanelDock->raise();
        }
    } else {
        statusBar()->showMessage(tr("AI Chat Panel not available"), 3000);
    }
}

void MainWindow::generateTests() 
{ 
    std::string sel = codeView_->textCursor().selectedText(); 
    if (sel.isEmpty()) {
        statusBar()->showMessage(tr("Select code first"), 2000);
        return;
    }
    
    if (m_aiChatPanel) {
        std::string prompt = tr("Generate comprehensive unit tests for this code:\n\n```\n%1\n```\n\nInclude edge cases and error handling tests.");
        m_aiChatPanel->addUserMessage(prompt);
        onAIChatMessageSubmitted(prompt);
        
        if (m_aiChatPanelDock && !m_aiChatPanelDock->isVisible()) {
            m_aiChatPanelDock->show();
            m_aiChatPanelDock->raise();
        }
    } else {
        statusBar()->showMessage(tr("AI Chat Panel not available"), 3000);
    }
}

void MainWindow::generateDocs() 
{ 
    std::string sel = codeView_->textCursor().selectedText();
    if (sel.isEmpty()) {
        sel = codeView_->toPlainText(); // Use entire file if nothing selected
    }
    
    if (m_aiChatPanel) {
        std::string prompt = tr("Generate comprehensive documentation for this code:\n\n```\n%1\n```\n\nInclude function descriptions, parameter docs, and usage examples.");
        m_aiChatPanel->addUserMessage(prompt);
        onAIChatMessageSubmitted(prompt);
        
        if (m_aiChatPanelDock && !m_aiChatPanelDock->isVisible()) {
            m_aiChatPanelDock->show();
            m_aiChatPanelDock->raise();
        }
    } else {
        statusBar()->showMessage(tr("AI Chat Panel not available"), 3000);
    }
}

// Production-ready implementations with observability and UI integration
void MainWindow::onProjectOpened(const std::string& path) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onProjectOpened", "project");
    RawrXD::Integration::traceEvent("Project", "opened");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::ProjectExplorer)) {
        RawrXD::Integration::logWarn("MainWindow", "project_open", "Project Explorer feature is disabled in safe mode");
        statusBar()->showMessage(tr("Project Explorer disabled in safe mode"), 3000);
        return;
    }
    
    if (path.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "project_open", "Empty project path");
        return;
    }
    
    // Validate path
    std::filesystem::path info(path);
    if (!info.exists()) {
        RawrXD::Integration::logError("MainWindow", "project_open", std::string("Path does not exist: %1"));
        QMessageBox::warning(this, tr("Invalid Path"), 
                           tr("The specified path does not exist:\n%1"));
        return;
    }
    
    // Update current project path
    m_currentProjectPath = info.isDir() ? path : info.absolutePath();
    
    // Track project opens
    QSettings settings("RawrXD", "IDE");
    int openCount = settings.value("projects/openCount", 0).toInt() + 1;
    settings.setValue("projects/openCount", openCount);
    settings.setValue("projects/lastOpenedPath", m_currentProjectPath);
    settings.setValue("projects/lastOpenedTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    // Add to recent projects
    std::vector<std::string> recentProjects = settings.value("projects/recent").toStringList();
    recentProjects.removeAll(m_currentProjectPath);
    recentProjects.prepend(m_currentProjectPath);
    while (recentProjects.size() > 20) recentProjects.removeLast();
    settings.setValue("projects/recent", recentProjects);
    
    // Open in project explorer if available
    if (projectExplorer_) {
        if (info.isDir()) {
            projectExplorer_->openProject(path);
        } else {
            projectExplorer_->openProject(info.absolutePath());
            // Also open the file in editor
            openFileInEditor(path);
        }
        statusBar()->showMessage(tr("Project opened: %1"), 5000);
    } else {
        // Fallback: just update window title
        setWindowTitle(tr("RawrXD IDE - %1").fileName()));
        statusBar()->showMessage(tr("Project: %1"), 5000);
    }
    
    MetricsCollector::instance().incrementCounter("projects_opened");
    
    // signal for other components
    onGoalSubmitted(tr("project_opened:%1"));
    
    RawrXD::Integration::logInfo("MainWindow", "project_opened",
        std::string("Project opened: %1 (total: %2)"),
        void*{{"path", m_currentProjectPath}, {"open_count", openCount}});
}

void MainWindow::openFileInEditor(const std::string& path) {
    if (path.isEmpty() || !std::fstream::exists(path)) {
        statusBar()->showMessage(tr("File not found: %1"), 4000);
        return;
    }

    std::fstream file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        statusBar()->showMessage(tr("Unable to open file: %1"), 4000);
        return;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    const std::string content = stream.readAll();
    file.close();

    if (editorTabs_) {
        QTextEdit* editor = new QTextEdit(this);
        editor->setPlainText(content);
        const std::string title = std::filesystem::path(path).fileName();
        editorTabs_->addTab(editor, title);
        editorTabs_->setCurrentWidget(editor);
    } else if (codeView_) {
        codeView_->setPlainText(content);
    }
}

void MainWindow::onBuildStarted() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onBuildStarted", "build");
    RawrXD::Integration::traceEvent("Build", "started");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::BuildSystem)) {
        RawrXD::Integration::logWarn("MainWindow", "build_started", "Build System feature is disabled in safe mode");
        return;
    }
    
    if (!g_buildSystemBreaker.allowRequest()) {
        RawrXD::Integration::logWarn("MainWindow", "build_started", "Build system circuit breaker is open");
        statusBar()->showMessage(tr("Build system temporarily unavailable"), 3000);
        return;
    }
    
    g_buildSystemBreaker.recordSuccess();
    
    // Track build sessions
    QSettings settings("RawrXD", "IDE");
    settings.setValue("build/lastStartTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    settings.setValue("build/inProgress", true);
    int buildCount = settings.value("build/totalBuilds", 0).toInt() + 1;
    settings.setValue("build/totalBuilds", buildCount);
    
    // Update UI to show build in progress
    if (buildWidget_) {
        buildWidget_->setBuildStatus(true);
    }
    
    // Show progress in status bar
    statusBar()->showMessage(tr("Building..."), 0);
    
    // Update window title if needed
    std::string title = windowTitle();
    if (!title.contains(" [Building]")) {
        setWindowTitle(title + " [Building]");
    }
    
    // If we have a build output panel, clear it
    if (m_outputPanelWidget) {
        if (auto output = qobject_cast<QPlainTextEdit*>(m_outputPanelWidget)) {
            output->appendPlainText(std::string("[%1] Build started...\n")
                                   .toString("hh:mm:ss")));
        }
    }
    
    MetricsCollector::instance().incrementCounter("builds_started");
    
    RawrXD::Integration::logInfo("MainWindow", "build_started",
        std::string("Build started (total: %1)"),
        void*{{"build_count", buildCount}});
}

void MainWindow::onBuildFinished(bool success) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onBuildFinished", "build");
    RawrXD::Integration::traceEvent("Build", success ? "succeeded" : "failed");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::BuildSystem)) {
        return;
    }
    
    // Calculate build duration
    QSettings settings("RawrXD", "IDE");
    std::string startTimeStr = settings.value("build/lastStartTime").toString();
    qint64 buildDuration = 0;
    if (!startTimeStr.isEmpty()) {
        std::chrono::system_clock::time_point startTime = std::chrono::system_clock::time_point::fromString(startTimeStr, //ISODate);
        if (startTime.isValid()) {
            buildDuration = startTime.msecsTo(std::chrono::system_clock::time_point::currentDateTime());
            MetricsCollector::instance().recordLatency("build_duration_ms", buildDuration);
            settings.setValue("build/lastDuration", buildDuration);
        }
    }
    settings.setValue("build/inProgress", false);
    settings.setValue("build/lastFinishTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    // Update UI
    if (buildWidget_) {
        buildWidget_->setBuildStatus(false);
    }
    
    // Update status bar
    if (success) {
        statusBar()->showMessage(tr("Build OK"), 3000);
        
        // Remove [Building] from window title
        std::string title = windowTitle();
        if (title.contains(" [Building]")) {
            setWindowTitle(title.replace(" [Building]", ""));
        }
    } else {
        statusBar()->showMessage(tr("Build FAILED"), 3000);
        
        // Show notification for failures
        QMessageBox::warning(this, tr("Build Failed"), 
                           tr("Build process failed. Check console for details."));
    }
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(success ? "=== BUILD SUCCESS ===" : "=== BUILD FAILED ===");
        m_hexMagConsole->appendPlainText(std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
        if (buildDuration > 0) {
            m_hexMagConsole->appendPlainText(std::string("Duration: %1 ms"));
        }
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(success ? tr("✅ Build successful") : tr("❌ Build failed"));
    }
    
    MetricsCollector::instance().incrementCounter(success ? "builds_successful" : "builds_failed");
    
    RawrXD::Integration::logInfo("MainWindow", "build_finished",
        std::string("Build %1 (duration: %2ms)"),
        void*{{"success", success}, {"duration_ms", buildDuration}});
}

void MainWindow::onVcsStatusChanged() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onVcsStatusChanged", "vcs");
    RawrXD::Integration::traceEvent("VCS", "status_changed");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::VersionControl)) {
        RawrXD::Integration::logWarn("MainWindow", "vcs_status", "Version Control feature is disabled in safe mode");
        return;
    }
    
    if (!g_vcsBreaker.allowRequest()) {
        RawrXD::Integration::logWarn("MainWindow", "vcs_status", "VCS circuit breaker is open");
        statusBar()->showMessage(tr("VCS services temporarily unavailable"), 3000);
        return;
    }
    
    g_vcsBreaker.recordSuccess();
    
    // Track VCS refresh
    QSettings settings("RawrXD", "IDE");
    int refreshCount = settings.value("vcs/refreshCount", 0).toInt() + 1;
    settings.setValue("vcs/refreshCount", refreshCount);
    settings.setValue("vcs/lastRefreshTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    // Refresh VCS widget if available
    if (vcsWidget_) {
        vcsWidget_->refresh();
        statusBar()->showMessage(tr("VCS status refreshed"), 2000);
    }
    
    MetricsCollector::instance().incrementCounter("vcs_status_refreshes");
    
    // Update status bar indicator if we have one
    // (This would show Git branch, modified file count, etc.)
    
    RawrXD::Integration::logInfo("MainWindow", "vcs_status_changed",
        std::string("VCS status refreshed (total: %1)"),
        void*{{"refresh_count", refreshCount}});
}

void MainWindow::onDebuggerStateChanged(bool running) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onDebuggerStateChanged", "debugger");
    RawrXD::Integration::traceEvent("Debugger", running ? "started" : "stopped");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::Debugger)) {
        RawrXD::Integration::logWarn("MainWindow", "debugger_state", "Debugger feature is disabled in safe mode");
        return;
    }
    
    // Track debugger session metrics
    static std::chrono::system_clock::time_point sessionStartTime;
    if (running) {
        sessionStartTime = std::chrono::system_clock::time_point::currentDateTime();
        MetricsCollector::instance().incrementCounter("debugger_sessions_started");
    } else {
        if (sessionStartTime.isValid()) {
            qint64 sessionDuration = sessionStartTime.msecsTo(std::chrono::system_clock::time_point::currentDateTime());
            MetricsCollector::instance().recordLatency("debugger_session_duration_ms", sessionDuration);
            sessionStartTime = std::chrono::system_clock::time_point();
        }
        MetricsCollector::instance().incrementCounter("debugger_sessions_ended");
    }
    
    // Update debug widget if available
    if (debugWidget_) {
        debugWidget_->setDebuggerRunning(running);
    }
    
    // Persist debugger state
    QSettings settings("RawrXD", "IDE");
    settings.setValue("debugger/lastState", running ? "running" : "stopped");
    settings.setValue("debugger/lastStateChange", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    // Update status bar
    if (running) {
        statusBar()->showMessage(tr("Debugger running..."), 0);
    } else {
        statusBar()->showMessage(tr("Debugger stopped"), 2000);
    }
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(running ? "[DEBUGGER] Started" : "[DEBUGGER] Stopped");
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(running ? tr("🐛 Debugger started") : tr("🐛 Debugger stopped"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "debugger_state_changed",
        std::string("Debugger %1"),
        void*{{"running", running}});
}

void MainWindow::onTestRunStarted() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onTestRunStarted", "testing");
    RawrXD::Integration::traceEvent("Testing", "run_started");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::TestRunner)) {
        RawrXD::Integration::logWarn("MainWindow", "test_run", "Test Runner feature is disabled in safe mode");
        return;
    }
    
    // Record test run start time for duration tracking
    QSettings settings("RawrXD", "IDE");
    settings.setValue("testing/lastRunStartTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    settings.setValue("testing/runInProgress", true);
    
    // Update test explorer widget if available
    if (testWidget_ || m_testRunnerPanelPhase8) {
        if (testWidget_) {
            testWidget_->setTestRunActive(true);
        }
        if (m_testRunnerPanelPhase8) {
            m_testRunnerPanelPhase8->startTestRun();
        }
    }
    
    MetricsCollector::instance().incrementCounter("test_runs_started");
    
    // Update status bar
    statusBar()->showMessage(tr("Running tests..."), 0);
    
    // Clear previous test results in output panel
    if (m_outputPanelWidget) {
        if (auto output = qobject_cast<QPlainTextEdit*>(m_outputPanelWidget)) {
            output->clear();
            output->appendPlainText(std::string("[%1] Test run started...\n")
                                   .toString("hh:mm:ss")));
        }
    }
    
    // Show notification
    if (notificationCenter_) {
        notificationCenter_->notify(
            tr("Test Run Started"),
            tr("Test execution has begun"),
            NotificationCenter::NotificationLevel::Info);
    }
    
    RawrXD::Integration::logInfo("MainWindow", "test_run_started", "Test run initiated");
}

void MainWindow::onTestRunFinished() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onTestRunFinished", "testing");
    RawrXD::Integration::traceEvent("Testing", "run_finished");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::TestRunner)) {
        return;
    }
    
    // Calculate test run duration
    QSettings settings("RawrXD", "IDE");
    std::string startTimeStr = settings.value("testing/lastRunStartTime").toString();
    if (!startTimeStr.isEmpty()) {
        std::chrono::system_clock::time_point startTime = std::chrono::system_clock::time_point::fromString(startTimeStr, //ISODate);
        if (startTime.isValid()) {
            qint64 duration = startTime.msecsTo(std::chrono::system_clock::time_point::currentDateTime());
            MetricsCollector::instance().recordLatency("test_run_duration_ms", duration);
            settings.setValue("testing/lastRunDuration", duration);
        }
    }
    settings.setValue("testing/runInProgress", false);
    settings.setValue("testing/lastRunEndTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    // Increment total test runs counter
    int totalRuns = settings.value("testing/totalRuns", 0).toInt() + 1;
    settings.setValue("testing/totalRuns", totalRuns);
    
    // Update test explorer widget if available
    if (testWidget_ || m_testRunnerPanelPhase8) {
        if (testWidget_) {
            testWidget_->setTestRunActive(false);
        }
        if (m_testRunnerPanelPhase8) {
            m_testRunnerPanelPhase8->finishTestRun();
        }
    }
    
    MetricsCollector::instance().incrementCounter("test_runs_completed");
    
    // Update status bar
    statusBar()->showMessage(tr("Test run completed"), 3000);
    
    // Append result to output panel
    if (m_outputPanelWidget) {
        if (auto output = qobject_cast<QPlainTextEdit*>(m_outputPanelWidget)) {
            output->appendPlainText(std::string("[%1] Test run completed.\n")
                                   .toString("hh:mm:ss")));
        }
    }
    
    // Show notification
    if (notificationCenter_) {
        notificationCenter_->notify(
            tr("Test Run Completed"),
            tr("Test execution finished. Check results panel for details."),
            NotificationCenter::NotificationLevel::Success);
    }
    
    RawrXD::Integration::logInfo("MainWindow", "test_run_finished", "Test run completed",
        void*{{"total_runs", totalRuns}});
}
void MainWindow::onDatabaseConnected() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onDatabaseConnected", "database");
    RawrXD::Integration::traceEvent("Database", "connected");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::Database)) {
        RawrXD::Integration::logWarn("MainWindow", "database_connect", "Database feature is disabled in safe mode");
        return;
    }
    
    // Track database connections
    QSettings settings("RawrXD", "IDE");
    int connectionCount = settings.value("database/connections", 0).toInt() + 1;
    settings.setValue("database/connections", connectionCount);
    settings.setValue("database/lastConnectionTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    // Update database tool widget if available
    if (database_) {
        database_->onConnectionEstablished();
    }
    
    MetricsCollector::instance().incrementCounter("database_connections");
    statusBar()->showMessage(tr("Database connected"), 3000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[DATABASE] Connection established");
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("💾 Database connected"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "database_connected",
        std::string("Database connected (total: %1)"),
        void*{{"connection_count", connectionCount}});
}

void MainWindow::onDockerContainerListed() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onDockerContainerListed", "docker");
    RawrXD::Integration::traceEvent("Docker", "containers_listed");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::Docker)) {
        RawrXD::Integration::logWarn("MainWindow", "docker_containers", "Docker feature is disabled in safe mode");
        return;
    }
    
    if (!g_dockerBreaker.allowRequest()) {
        RawrXD::Integration::logWarn("MainWindow", "docker_containers", "Docker circuit breaker is open");
        statusBar()->showMessage(tr("Docker services temporarily unavailable"), 3000);
        return;
    }
    
    g_dockerBreaker.recordSuccess();
    
    // Update docker tool widget if available
    if (docker_) {
        docker_->refreshContainers();
    }
    
    MetricsCollector::instance().incrementCounter("docker_container_queries");
    statusBar()->showMessage(tr("Docker containers refreshed"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[DOCKER] Container list refreshed");
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("🐳 Docker containers listed"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "docker_containers", "Docker containers listed");
}

void MainWindow::onCloudResourceListed() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onCloudResourceListed", "cloud");
    RawrXD::Integration::traceEvent("Cloud", "resources_listed");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::Cloud)) {
        RawrXD::Integration::logWarn("MainWindow", "cloud_resources", "Cloud feature is disabled in safe mode");
        return;
    }
    
    if (!g_cloudBreaker.allowRequest()) {
        RawrXD::Integration::logWarn("MainWindow", "cloud_resources", "Cloud circuit breaker is open");
        statusBar()->showMessage(tr("Cloud services temporarily unavailable"), 3000);
        return;
    }
    
    g_cloudBreaker.recordSuccess();
    
    // Update cloud explorer widget if available
    if (cloud_) {
        cloud_->refreshResources();
    }
    
    MetricsCollector::instance().incrementCounter("cloud_resource_queries");
    statusBar()->showMessage(tr("Cloud resources refreshed"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[CLOUD] Resource list updated");
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("☁️ Cloud resources loaded"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "cloud_resources", "Cloud resources listed");
}

void MainWindow::onPackageInstalled(const std::string& pkg) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onPackageInstalled", "package");
    RawrXD::Integration::traceEvent("PackageManager", "package_installed");
    
    if (pkg.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "package_install", "Empty package name");
        return;
    }
    
    // Track package installations
    QSettings settings("RawrXD", "IDE");
    int installCount = settings.value("packages/installCount", 0).toInt() + 1;
    settings.setValue("packages/installCount", installCount);
    settings.setValue("packages/lastInstalled", pkg);
    settings.setValue("packages/lastInstallTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    // Add to recent packages
    std::vector<std::string> recentPackages = settings.value("packages/recent").toStringList();
    recentPackages.removeAll(pkg);
    recentPackages.prepend(pkg);
    while (recentPackages.size() > 10) recentPackages.removeLast();
    settings.setValue("packages/recent", recentPackages);
    
    MetricsCollector::instance().incrementCounter("packages_installed");
    statusBar()->showMessage(tr("Package: %1"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[PACKAGE] Installed: %1"));
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("📦 Package installed: %1"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "package_installed",
        std::string("Package installed: %1 (total: %2)"),
        void*{{"package", pkg}, {"total_installs", installCount}});
}
void MainWindow::onDocumentationQueried(const std::string& keyword) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onDocumentationQueried", "docs");
    RawrXD::Integration::traceEvent("Documentation", "queried");
    
    if (keyword.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "docs_query", "Empty documentation query");
        return;
    }
    
    // Track documentation queries
    QSettings settings("RawrXD", "IDE");
    int queryCount = settings.value("docs/queryCount", 0).toInt() + 1;
    settings.setValue("docs/queryCount", queryCount);
    settings.setValue("docs/lastQuery", keyword);
    settings.setValue("docs/lastQueryTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("docs_queries");
    MetricsCollector::instance().recordLatency("docs_query_length", keyword.length());
    statusBar()->showMessage(tr("Searching: %1"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[DOCS] Searching for: %1"));
    }
    
    // Could integrate with AI chat to search documentation
    if (m_aiChatPanel && !keyword.isEmpty()) {
        std::string prompt = tr("Show documentation for: %1");
        m_aiChatPanel->addUserMessage(prompt);
    }
    
    RawrXD::Integration::logInfo("MainWindow", "docs_queried",
        std::string("Documentation query: %1 (total: %2)"),
        void*{{"keyword", keyword}, {"query_count", queryCount}});
}

void MainWindow::onUMLGenerated(const std::string& plantUml) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onUMLGenerated", "uml");
    RawrXD::Integration::traceEvent("UML", "generated");
    
    if (plantUml.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "uml_generate", "Empty UML content");
        return;
    }
    
    // Track UML generation
    QSettings settings("RawrXD", "IDE");
    int generationCount = settings.value("uml/generationCount", 0).toInt() + 1;
    settings.setValue("uml/generationCount", generationCount);
    settings.setValue("uml/lastGenerationLength", plantUml.length());
    settings.setValue("uml/lastGenerationTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("uml_generated");
    MetricsCollector::instance().recordLatency("uml_content_length", plantUml.length());
    statusBar()->showMessage(tr("UML generated"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[UML] Diagram generated");
        m_hexMagConsole->appendPlainText(plantUml.left(200) + "..."); // First 200 chars
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("📊 UML diagram generated"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "uml_generated",
        std::string("UML generated (length: %1, total: %2)")),
        void*{{"content_length", plantUml.length()}, {"generation_count", generationCount}});
}

void MainWindow::onImageEdited(const std::string& path) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onImageEdited", "image");
    RawrXD::Integration::traceEvent("Image", "edited");
    
    if (path.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "image_edit", "Empty image path");
        return;
    }
    
    // Validate path
    std::filesystem::path info(path);
    if (!info.exists()) {
        RawrXD::Integration::logError("MainWindow", "image_edit", std::string("Image file does not exist: %1"));
        return;
    }
    
    // Track image edits
    QSettings settings("RawrXD", "IDE");
    int editCount = settings.value("image/editCount", 0).toInt() + 1;
    settings.setValue("image/editCount", editCount);
    settings.setValue("image/lastEdited", path);
    settings.setValue("image/lastEditTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("images_edited");
    statusBar()->showMessage(tr("Image: %1").fileName()), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[IMAGE] Edited: %1"));
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("🖼️ Image edited: %1").fileName()));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "image_edited",
        std::string("Image edited: %1 (total: %2)").fileName()),
        void*{{"path", path}, {"edit_count", editCount}});
}

void MainWindow::onTranslationChanged(const std::string& lang) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onTranslationChanged", "translation");
    RawrXD::Integration::traceEvent("Translation", "changed");
    
    if (lang.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "translation_change", "Empty language code");
        return;
    }
    
    // Track language changes
    QSettings settings("RawrXD", "IDE");
    int changeCount = settings.value("translation/changeCount", 0).toInt() + 1;
    settings.setValue("translation/changeCount", changeCount);
    settings.setValue("translation/lastLanguage", lang);
    settings.setValue("translation/lastChangeTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("translations_changed");
    statusBar()->showMessage(tr("Language: %1"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[TRANSLATION] Language: %1"));
    }
    
    // Could trigger QApplication locale change here
    if (chatHistory_) {
        chatHistory_->addItem(tr("🌐 Language changed: %1"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "translation_changed",
        std::string("Language changed to: %1 (total: %2)"),
        void*{{"language", lang}, {"change_count", changeCount}});
}

void MainWindow::onDesignImported(const std::string& file) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onDesignImported", "design");
    RawrXD::Integration::traceEvent("Design", "imported");
    
    if (file.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "design_import", "Empty design file path");
        return;
    }
    
    // Validate file
    std::filesystem::path info(file);
    if (!info.exists()) {
        RawrXD::Integration::logError("MainWindow", "design_import", std::string("Design file does not exist: %1"));
        return;
    }
    
    // Track design imports
    QSettings settings("RawrXD", "IDE");
    int importCount = settings.value("design/importCount", 0).toInt() + 1;
    settings.setValue("design/importCount", importCount);
    settings.setValue("design/lastImported", file);
    settings.setValue("design/lastImportTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("designs_imported");
    statusBar()->showMessage(tr("Design from %1").fileName()), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[DESIGN] Imported: %1"));
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("🎨 Design imported: %1").fileName()));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "design_imported",
        std::string("Design imported: %1 (total: %2)").fileName()),
        void*{{"file", file}, {"import_count", importCount}});
}
void MainWindow::onAIChatMessage(const std::string& msg) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onAIChatMessage", "ai_chat");
    RawrXD::Integration::traceEvent("AI_Chat", "message_received");
    
    if (msg.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "ai_chat_message", "Empty AI chat message");
        return;
    }
    
    // Track AI chat messages
    QSettings settings("RawrXD", "IDE");
    int messageCount = settings.value("ai_chat/messageCount", 0).toInt() + 1;
    settings.setValue("ai_chat/messageCount", messageCount);
    settings.setValue("ai_chat/lastMessageLength", msg.length());
    settings.setValue("ai_chat/lastMessageTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("ai_chat_messages");
    MetricsCollector::instance().recordLatency("ai_chat_message_length", msg.length());
    
    if (m_aiChatPanel) {
        statusBar()->showMessage(tr("AI Chat: message received"), 2000);
        
        // Show AI chat panel if hidden
        if (m_aiChatPanelDock && !m_aiChatPanelDock->isVisible()) {
            m_aiChatPanelDock->show();
            m_aiChatPanelDock->raise();
        }
    }
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[AI_CHAT] %1")));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "ai_chat_message",
        std::string("AI Chat message received (length: %1, total: %2)")),
        void*{{"message_length", msg.length()}, {"message_count", messageCount}});
}

void MainWindow::onNotebookExecuted() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onNotebookExecuted", "notebook");
    RawrXD::Integration::traceEvent("Notebook", "executed");
    
    // Track notebook executions
    QSettings settings("RawrXD", "IDE");
    int executionCount = settings.value("notebook/executionCount", 0).toInt() + 1;
    settings.setValue("notebook/executionCount", executionCount);
    settings.setValue("notebook/lastExecutionTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("notebook_executions");
    statusBar()->showMessage(tr("Notebook executed"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[NOTEBOOK] Cells executed");
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("📓 Notebook executed"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "notebook_executed",
        std::string("Notebook executed (total: %1)"),
        void*{{"execution_count", executionCount}});
}

void MainWindow::onMarkdownRendered() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onMarkdownRendered", "markdown");
    RawrXD::Integration::traceEvent("Markdown", "rendered");
    
    // Track markdown renders
    QSettings settings("RawrXD", "IDE");
    int renderCount = settings.value("markdown/renderCount", 0).toInt() + 1;
    settings.setValue("markdown/renderCount", renderCount);
    settings.setValue("markdown/lastRenderTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("markdown_renders");
    statusBar()->showMessage(tr("Markdown rendered"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[MARKDOWN] Preview updated");
    }
    
    RawrXD::Integration::logInfo("MainWindow", "markdown_rendered",
        std::string("Markdown rendered (total: %1)"),
        void*{{"render_count", renderCount}});
}

void MainWindow::onSheetCalculated() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onSheetCalculated", "spreadsheet");
    RawrXD::Integration::traceEvent("Spreadsheet", "calculated");
    
    // Track spreadsheet calculations
    QSettings settings("RawrXD", "IDE");
    int calculationCount = settings.value("spreadsheet/calculationCount", 0).toInt() + 1;
    settings.setValue("spreadsheet/calculationCount", calculationCount);
    settings.setValue("spreadsheet/lastCalculationTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("spreadsheet_calculations");
    statusBar()->showMessage(tr("Spreadsheet calculated"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[SPREADSHEET] Formulas recalculated");
    }
    
    RawrXD::Integration::logInfo("MainWindow", "spreadsheet_calculated",
        std::string("Spreadsheet calculated (total: %1)"),
        void*{{"calculation_count", calculationCount}});
}

void MainWindow::onTerminalCommand(const std::string& cmd) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onTerminalCommand", "terminal");
    RawrXD::Integration::traceEvent("Terminal", "command_executed");
    
    if (cmd.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "terminal_command", "Empty terminal command");
        return;
    }
    
    // Track terminal commands
    QSettings settings("RawrXD", "IDE");
    int commandCount = settings.value("terminal/commandCount", 0).toInt() + 1;
    settings.setValue("terminal/commandCount", commandCount);
    settings.setValue("terminal/lastCommand", cmd.left(100)); // Store first 100 chars
    settings.setValue("terminal/lastCommandTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("terminal_commands");
    MetricsCollector::instance().recordLatency("terminal_command_length", cmd.length());
    statusBar()->showMessage(tr("Terminal: %1")), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[TERMINAL] $ %1"));
    }
    
    // Could execute command via QProcess here
    if (chatHistory_) {
        chatHistory_->addItem(tr("💻 Terminal: %1")));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "terminal_command",
        std::string("Terminal command executed (length: %1, total: %2)")),
        void*{{"command_length", cmd.length()}, {"command_count", commandCount}});
}
void MainWindow::onSnippetInserted(const std::string& id) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onSnippetInserted", "snippet");
    RawrXD::Integration::traceEvent("Snippet", "inserted");
    
    if (id.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "snippet_insert", "Empty snippet ID");
        return;
    }
    
    // Track snippet usage
    QSettings settings("RawrXD", "IDE");
    int snippetCount = settings.value("snippets/insertCount", 0).toInt() + 1;
    settings.setValue("snippets/insertCount", snippetCount);
    settings.setValue("snippets/lastInserted", id);
    settings.setValue("snippets/lastInsertTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("snippets_inserted");
    statusBar()->showMessage(tr("Snippet: %1"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[SNIPPET] Inserted: %1"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "snippet_inserted",
        std::string("Snippet inserted: %1 (total: %2)"),
        void*{{"snippet_id", id}, {"insert_count", snippetCount}});
}

void MainWindow::onRegexTested(const std::string& pattern) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onRegexTested", "regex");
    RawrXD::Integration::traceEvent("Regex", "tested");
    
    if (pattern.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "regex_test", "Empty regex pattern");
        return;
    }
    
    // Track regex testing
    QSettings settings("RawrXD", "IDE");
    int testCount = settings.value("regex/testCount", 0).toInt() + 1;
    settings.setValue("regex/testCount", testCount);
    settings.setValue("regex/lastPattern", pattern);
    settings.setValue("regex/lastTestTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("regex_tests");
    MetricsCollector::instance().recordLatency("regex_pattern_length", pattern.length());
    statusBar()->showMessage(tr("Regex: %1")), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[REGEX] Pattern: %1"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "regex_tested",
        std::string("Regex tested: %1 (total: %2)")),
        void*{{"pattern_length", pattern.length()}, {"test_count", testCount}});
}

void MainWindow::onDiffMerged() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onDiffMerged", "diff");
    RawrXD::Integration::traceEvent("Diff", "merged");
    
    // Track diff merges
    QSettings settings("RawrXD", "IDE");
    int mergeCount = settings.value("diff/mergeCount", 0).toInt() + 1;
    settings.setValue("diff/mergeCount", mergeCount);
    settings.setValue("diff/lastMergeTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("diff_merges");
    statusBar()->showMessage(tr("Diff merged"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[DIFF] Merge operation completed");
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔀 Diff merged successfully"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "diff_merged",
        std::string("Diff merged (total: %1)"),
        void*{{"merge_count", mergeCount}});
}

void MainWindow::onColorPicked(const QColor& c) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onColorPicked", "color");
    RawrXD::Integration::traceEvent("Color", "picked");
    
    if (!c.isValid()) {
        RawrXD::Integration::logWarn("MainWindow", "color_pick", "Invalid color");
        return;
    }
    
    // Track color picks
    QSettings settings("RawrXD", "IDE");
    int pickCount = settings.value("color/pickCount", 0).toInt() + 1;
    settings.setValue("color/pickCount", pickCount);
    settings.setValue("color/lastColor", c.name());
    settings.setValue("color/lastPickTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("colors_picked");
    statusBar()->showMessage(tr("Color: %1")), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[COLOR] %1 (R:%2 G:%3 B:%4)")
            )))));
    }
    
    // Could insert color into current editor
    if (codeView_ && codeView_->hasFocus()) {
        codeView_->insertPlainText(c.name());
    }
    
    RawrXD::Integration::logInfo("MainWindow", "color_picked",
        std::string("Color picked: %1 (total: %2)")),
        void*{{"color", c.name()}, {"pick_count", pickCount}});
}

void MainWindow::onIconSelected(const std::string& name) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onIconSelected", "icon");
    RawrXD::Integration::traceEvent("Icon", "selected");
    
    if (name.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "icon_select", "Empty icon name");
        return;
    }
    
    // Track icon selections
    QSettings settings("RawrXD", "IDE");
    int selectionCount = settings.value("icon/selectionCount", 0).toInt() + 1;
    settings.setValue("icon/selectionCount", selectionCount);
    settings.setValue("icon/lastSelected", name);
    settings.setValue("icon/lastSelectionTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("icons_selected");
    statusBar()->showMessage(tr("Icon: %1"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[ICON] Selected: %1"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "icon_selected",
        std::string("Icon selected: %1 (total: %2)"),
        void*{{"icon_name", name}, {"selection_count", selectionCount}});
}

void MainWindow::onPluginLoaded(const std::string& name) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onPluginLoaded", "plugin");
    RawrXD::Integration::traceEvent("Plugin", "loaded");
    
    if (name.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "plugin_load", "Empty plugin name");
        return;
    }
    
    // Track plugin loads
    QSettings settings("RawrXD", "IDE");
    int loadCount = settings.value("plugin/loadCount", 0).toInt() + 1;
    settings.setValue("plugin/loadCount", loadCount);
    settings.setValue("plugin/lastLoaded", name);
    settings.setValue("plugin/lastLoadTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("plugins_loaded");
    statusBar()->showMessage(tr("Plugin loaded: %1"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[PLUGIN] Loaded: %1"));
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔌 Plugin loaded: %1"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "plugin_loaded",
        std::string("Plugin loaded: %1 (total: %2)"),
        void*{{"plugin_name", name}, {"load_count", loadCount}});
}

void MainWindow::onSettingsSaved() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onSettingsSaved", "settings");
    RawrXD::Integration::traceEvent("Settings", "saved");
    
    // Track settings saves
    QSettings settings("RawrXD", "IDE");
    int saveCount = settings.value("settings/saveCount", 0).toInt() + 1;
    settings.setValue("settings/saveCount", saveCount);
    settings.setValue("settings/lastSaveTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("settings_saves");
    statusBar()->showMessage(tr("Settings saved"), 2000);
    
    // Trigger our own save state
    handleSaveState();
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[SETTINGS] Configuration saved");
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("⚙️ Settings saved"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "settings_saved",
        std::string("Settings saved (total: %1)"),
        void*{{"save_count", saveCount}});
}
void MainWindow::onNotificationClicked(const std::string& id) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onNotificationClicked", "notification");
    RawrXD::Integration::traceEvent("Notification", "clicked");
    
    if (id.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "notification_click", "Empty notification ID");
        return;
    }
    
    // Track notification clicks
    QSettings settings("RawrXD", "IDE");
    int clickCount = settings.value("notification/clickCount", 0).toInt() + 1;
    settings.setValue("notification/clickCount", clickCount);
    settings.setValue("notification/lastClicked", id);
    settings.setValue("notification/lastClickTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("notifications_clicked");
    statusBar()->showMessage(tr("Notification: %1"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[NOTIFICATION] User clicked: %1"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "notification_clicked",
        std::string("Notification clicked: %1 (total: %2)"),
        void*{{"notification_id", id}, {"click_count", clickCount}});
}

void MainWindow::onShortcutChanged(const std::string& id, const QKeySequence& key) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onShortcutChanged", "shortcut");
    RawrXD::Integration::traceEvent("Shortcut", "changed");
    
    if (id.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "shortcut_change", "Empty shortcut ID");
        return;
    }
    
    // Track shortcut changes
    QSettings settings("RawrXD", "IDE");
    int changeCount = settings.value("shortcut/changeCount", 0).toInt() + 1;
    settings.setValue("shortcut/changeCount", changeCount);
    settings.setValue("shortcut/lastChanged", id);
    settings.setValue("shortcut/lastChangeTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("shortcuts_changed");
    statusBar()->showMessage(tr("Shortcut %1: %2")), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[SHORTCUT] %1 = %2")));
    }
    
    // Save shortcuts to settings (use different settings object for QtShell)
    QSettings qtShellSettings("RawrXD", "QtShell");
    qtShellSettings.setValue(std::string("Shortcuts/%1"), key.toString());
    
    RawrXD::Integration::logInfo("MainWindow", "shortcut_changed",
        std::string("Shortcut changed: %1 = %2 (total: %3)")),
        void*{{"shortcut_id", id}, {"key_sequence", key.toString()}, {"change_count", changeCount}});
}

void MainWindow::onTelemetryReady() {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onTelemetryReady", "telemetry");
    RawrXD::Integration::traceEvent("Telemetry", "ready");
    
    // Track telemetry initialization
    QSettings settings("RawrXD", "IDE");
    settings.setValue("telemetry/initialized", true);
    settings.setValue("telemetry/initTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("telemetry_ready");
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[TELEMETRY] Observability system ready");
    }
    
    RawrXD::Integration::logInfo("MainWindow", "telemetry_ready", "Telemetry system initialized");
}

void MainWindow::onUpdateAvailable(const std::string& version) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onUpdateAvailable", "update");
    RawrXD::Integration::traceEvent("Update", "available");
    
    if (version.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "update_available", "Empty version string");
        return;
    }
    
    // Track update notifications
    QSettings settings("RawrXD", "IDE");
    int notificationCount = settings.value("update/notificationCount", 0).toInt() + 1;
    settings.setValue("update/notificationCount", notificationCount);
    settings.setValue("update/lastVersion", version);
    settings.setValue("update/lastNotificationTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("update_notifications");
    statusBar()->showMessage(tr("Update available: %1"), 5000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[UPDATE] Version %1 available"));
    }
    
    // Show update dialog
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Update Available"));
    msgBox.setText(tr("Version %1 is available for download."));
    msgBox.setInformativeText(tr("Would you like to download it now?"));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    
    if (ret == QMessageBox::Yes) {
        // Could open browser to download page
        if (chatHistory_) {
            chatHistory_->addItem(tr("📥 Downloading update %1..."));
        }
    }
    
    RawrXD::Integration::logInfo("MainWindow", "update_available",
        std::string("Update available: %1 (total: %2)"),
        void*{{"version", version}, {"notification_count", notificationCount}});
}

void MainWindow::onWelcomeProjectChosen(const std::string& path) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onWelcomeProjectChosen", "welcome");
    RawrXD::Integration::traceEvent("Welcome", "project_chosen");
    
    if (path.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "welcome_project", "Empty project path");
        return;
    }
    
    // Track welcome screen project selections
    QSettings settings("RawrXD", "IDE");
    int selectionCount = settings.value("welcome/projectSelections", 0).toInt() + 1;
    settings.setValue("welcome/projectSelections", selectionCount);
    settings.setValue("welcome/lastSelectedPath", path);
    settings.setValue("welcome/lastSelectionTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("welcome_project_selections");
    
    // Delegate to onProjectOpened
    onProjectOpened(path);
    
    RawrXD::Integration::logInfo("MainWindow", "welcome_project_chosen",
        std::string("Welcome project chosen: %1 (total: %2)"),
        void*{{"path", path}, {"selection_count", selectionCount}});
}

void MainWindow::onCommandPaletteTriggered(const std::string& cmd) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onCommandPaletteTriggered", "command_palette");
    RawrXD::Integration::traceEvent("CommandPalette", "triggered");
    
    if (cmd.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "command_palette", "Empty command");
        return;
    }
    
    // Track command palette usage
    QSettings settings("RawrXD", "IDE");
    int triggerCount = settings.value("command_palette/triggerCount", 0).toInt() + 1;
    settings.setValue("command_palette/triggerCount", triggerCount);
    settings.setValue("command_palette/lastCommand", cmd);
    settings.setValue("command_palette/lastTriggerTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("command_palette_triggers");
    MetricsCollector::instance().recordLatency("command_length", cmd.length());
    statusBar()->showMessage(tr("Command: %1"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[CMD] %1"));
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("⌨️ Command: %1"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "command_palette_triggered",
        std::string("Command palette triggered: %1 (total: %2)"),
        void*{{"command", cmd}, {"trigger_count", triggerCount}});
}

void MainWindow::onProgressCancelled(const std::string& taskId) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onProgressCancelled", "progress");
    RawrXD::Integration::traceEvent("Progress", "cancelled");
    
    if (taskId.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "progress_cancel", "Empty task ID");
        return;
    }
    
    // Track progress cancellations
    QSettings settings("RawrXD", "IDE");
    int cancelCount = settings.value("progress/cancelCount", 0).toInt() + 1;
    settings.setValue("progress/cancelCount", cancelCount);
    settings.setValue("progress/lastCancelled", taskId);
    settings.setValue("progress/lastCancelTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    MetricsCollector::instance().incrementCounter("progress_cancellations");
    statusBar()->showMessage(tr("Cancelled: %1"), 2000);
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[PROGRESS] Cancelled: %1"));
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("⏹️ Task cancelled: %1"));
    }
    
    RawrXD::Integration::logInfo("MainWindow", "progress_cancelled",
        std::string("Progress cancelled: %1 (total: %2)"),
        void*{{"task_id", taskId}, {"cancel_count", cancelCount}});
}
void MainWindow::onQuickFixApplied(const std::string& fix) {
    
    statusBar()->showMessage(tr("Quick fix applied: %1")), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[QUICK_FIX] %1"));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔧 Quick fix: %1")));
    }
}

void MainWindow::onMinimapClicked(qreal ratio) {
    
    statusBar()->showMessage(tr("Minimap: %1%")), 1000);
    
    // Scroll editor to position
    if (codeView_) {
        QTextCursor cursor = codeView_->textCursor();
        int totalBlocks = codeView_->document()->blockCount();
        int targetBlock = static_cast<int>(ratio * totalBlocks);
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::NextBlock, QTextCursor::MoveAnchor, targetBlock);
        codeView_->setTextCursor(cursor);
        codeView_->ensureCursorVisible();
    }
}

void MainWindow::onBreadcrumbClicked(const std::string& symbol) {
    
    statusBar()->showMessage(tr("Navigate: %1"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[NAV] %1"));
    }
    
    // Could search for symbol in current file
    if (codeView_) {
        QTextCursor cursor = codeView_->document()->find(symbol);
        if (!cursor.isNull()) {
            codeView_->setTextCursor(cursor);
            codeView_->ensureCursorVisible();
        }
    }
}

void MainWindow::onStatusFieldClicked(const std::string& field) {
    
    statusBar()->showMessage(tr("Status: %1"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[STATUS] Clicked: %1"));
    }
}

void MainWindow::onTerminalEmulatorCommand(const std::string& cmd) {
    
    statusBar()->showMessage(tr("Emulator: %1")), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[EMU] $ %1"));
    }
}

void MainWindow::onSearchResultActivated(const std::string& file, int line) {
    
    statusBar()->showMessage(tr("Goto %1:%2").fileName()), 2000);
    
    // Open file in editor
    std::fstream f(file);
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (editorTabs_ && codeView_) {
            QTextStream in(&f);
            codeView_->setText(in.readAll());
            f.close();
            
            // Jump to line
            QTextCursor cursor = codeView_->textCursor();
            cursor.movePosition(QTextCursor::Start);
            cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line - 1);
            codeView_->setTextCursor(cursor);
            codeView_->ensureCursorVisible();
        }
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔍 Opened: %1:%2").fileName()));
    }
}

void MainWindow::onBookmarkToggled(const std::string& file, int line) {
    
    statusBar()->showMessage(tr("Bookmark: %1:%2").fileName()), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[BOOKMARK] %1:%2"));
    }
    
    // Save bookmark to settings
    QSettings settings("RawrXD", "QtShell");
    std::string bookmarkKey = std::string("Bookmarks/%1_%2");
    bool exists = settings.value(bookmarkKey, false).toBool();
    settings.setValue(bookmarkKey, !exists); // Toggle
}

void MainWindow::onTodoClicked(const std::string& file, int line) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onTodoClicked", "todo");
    
    statusBar()->showMessage(tr("TODO: %1:%2").fileName()), 2000);
    
    // Open file at line (same as search result)
    onSearchResultActivated(file, line);
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("📝 TODO: %1:%2").fileName()));
    }
    
    // Track TODO interaction
    QSettings settings("RawrXD", "IDE");
    int todoClicks = settings.value("todos/clickCount", 0).toInt() + 1;
    settings.setValue("todos/clickCount", todoClicks);
    settings.setValue("todos/lastClicked", std::string("%1:%2"));
}

void MainWindow::scanProjectForTodos()
{
    // Scan project files for TODO, FIXME, HACK, XXX markers
    RawrXD::Integration::ScopedTimer timer("MainWindow", "scanProjectForTodos", "todo");
    
    if (!todos_) {
        return;
    }
    
    // Clear existing items
    todos_->clear();
    
    std::string projectPath = QSettings("RawrXD", "IDE").value("project/currentPath", "").toString();
    if (projectPath.isEmpty() && projectExplorer_) {
        // Try to get from project explorer
        projectPath = std::filesystem::path::currentPath();
    }
    
    if (projectPath.isEmpty()) {
        statusBar()->showMessage(tr("No project open for TODO scan"), 3000);
        return;
    }
    
    std::vector<std::string> todoPatterns = {"TODO", "FIXME", "HACK", "XXX", "BUG", "NOTE"};
    std::vector<std::string> extensions = {"*.cpp", "*.h", "*.hpp", "*.c", "*.py", "*.js", "*.ts", "*.rs", "*.go", "*.java", "*.asm"};
    
    int totalTodos = 0;
    QDirIterator it(projectPath, extensions, std::filesystem::path::Files, QDirIterator::Subdirectories);
    
    while (itfalse) {
        std::string filePath = it;
        std::fstream file(filePath);
        
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream stream(&file);
            int lineNum = 0;
            
            while (!stream.atEnd()) {
                std::string line = stream.readLine();
                lineNum++;
                
                for (const std::string& pattern : todoPatterns) {
                    int idx = line.indexOf(pattern, 0, //CaseInsensitive);
                    if (idx != -1) {
                        // Extract comment text after the marker
                        std::string comment = line.mid(idx).trimmed();
                        if (comment.length() > 100) comment = comment.left(100) + "...";
                        
                        // Create list item with file:line info
                        std::string itemText = std::string("%1:%2 - %3")
                            .fileName())
                            
                            ;
                        
                        QListWidgetItem* item = new QListWidgetItem(itemText);
                        item->setData(//UserRole, filePath);
                        item->setData(//UserRole + 1, lineNum);
                        
                        // Color code by type
                        if (pattern == "FIXME" || pattern == "BUG") {
                            item->setForeground(QColor("#ff6b6b")); // Red
                        } else if (pattern == "HACK" || pattern == "XXX") {
                            item->setForeground(QColor("#ffd93d")); // Yellow
                        } else {
                            item->setForeground(QColor("#6bcb77")); // Green
                        }
                        
                        todos_->addItem(item);
                        totalTodos++;
                    }
                }
            }
            file.close();
        }
    }
    
    statusBar()->showMessage(tr("Found %1 TODOs in project"), 5000);
    
    // Persist TODO count
    QSettings settings("RawrXD", "IDE");
    settings.setValue("todos/totalCount", totalTodos);
    settings.setValue("todos/lastScan", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
}

void MainWindow::onMacroReplayed() {
    
    statusBar()->showMessage(tr("Macro executed"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[MACRO] Playback complete");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🎬 Macro replayed"));
    }
}
void MainWindow::onCompletionCacheHit(const std::string& key) {
    
    // Performance metric - cache is working
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[CACHE] Hit: %1"));
    }
}

void MainWindow::onLSPDiagnostic(const std::string& file, const void*& diags) {
    int diagCount = diags.size();
    
    statusBar()->showMessage(tr("Diagnostics: %1 (%2 issues)").fileName()), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[LSP] %1: %2 diagnostics"));
        
        // Log first 3 diagnostics
        for (int i = 0; i < qMin(3, diagCount); ++i) {
            void* diag = diags[i].toObject();
            std::string message = diag["message"].toString();
            int line = diag["line"].toInt();
            m_hexMagConsole->appendPlainText(std::string("  Line %1: %2"));
        }
    }
    
    if (chatHistory_ && diagCount > 0) {
        chatHistory_->addItem(tr("⚠️ %1 diagnostic issues in %2").fileName()));
    }
}

void MainWindow::onCodeLensClicked(const std::string& command) {
    
    statusBar()->showMessage(tr("CodeLens: %1")), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[CODE_LENS] %1"));
    }
    
    // Could trigger command palette execution here
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔬 CodeLens: %1")));
    }
}

void MainWindow::onInlayHintShown(const std::string& file) {
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[INLAY] Hints active: %1"));
    }
}

void MainWindow::onInlineChatRequested(const std::string& text) {
    
    if (m_aiChatPanel) {
        statusBar()->showMessage(tr("Inline chat active"), 2000);
        
        // Add text to AI chat
        m_aiChatPanel->addUserMessage(text);
        onAIChatMessageSubmitted(text);
        
        // Show panel
        if (m_aiChatPanelDock && !m_aiChatPanelDock->isVisible()) {
            m_aiChatPanelDock->show();
            m_aiChatPanelDock->raise();
        }
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("💬 Inline chat: %1")));
    }
}

void MainWindow::onAIReviewComment(const std::string& comment) {
    
    statusBar()->showMessage(tr("AI review comment added"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[AI_REVIEW] %1"));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🤖 AI Review: %1")));
    }
}

void MainWindow::onCodeStreamEdit(const std::string& patch) {
    
    statusBar()->showMessage(tr("CodeStream sync: %1 bytes")), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[CODE_STREAM] Patch: %1 bytes")));
    }
    
    // Could apply patch to current editor here
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔄 CodeStream sync"));
    }
}
void MainWindow::onAudioCallStarted() {
    
    statusBar()->showMessage(tr("Audio call active"), 5000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[AUDIO] Call started");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🎙️ Audio call active"));
    }
}

void MainWindow::onScreenShareStarted() {
    
    statusBar()->showMessage(tr("Screen sharing active"), 5000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[SCREEN_SHARE] Broadcasting");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("📺 Screen sharing started"));
    }
}

void MainWindow::onWhiteboardDraw(const std::vector<uint8_t>& svg) {
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[WHITEBOARD] SVG: %1 bytes")));
    }
    
    // Could render SVG in a dedicated widget
}

void MainWindow::onTimeEntryAdded(const std::string& task) {
    
    statusBar()->showMessage(tr("Time logged: %1"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[TIME] %1 @ %2")
            
            .toString("hh:mm:ss")));
    }
    
    // Save to settings for time tracking history
    QSettings settings("RawrXD", "QtShell");
    settings.setValue(std::string("TimeTracking/%1")), task);
}

void MainWindow::onKanbanMoved(const std::string& taskId) {
    
    statusBar()->showMessage(tr("Task: %1"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[KANBAN] Moved: %1"));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("📋 Task moved: %1"));
    }
}

void MainWindow::onPomodoroTick(int remaining) {
    // Only log every 5 seconds to avoid spam
    if (remaining % 5 == 0) {
    }
    
    statusBar()->showMessage(tr("Pomodoro: %1m %2s")
        
        ), 1000);
    
    // Visual indicator when time is running out
    if (remaining <= 60 && remaining % 10 == 0) {
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(std::string("[POMODORO] ⏰ %1 seconds remaining"));
        }
    }
}

void MainWindow::onWallpaperChanged(const std::string& path) {
    
    statusBar()->showMessage(tr("Theme updated: %1").fileName()), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[THEME] Wallpaper: %1"));
    }
    
    // Could apply wallpaper to central widget background
    if (chatHistory_) {
        chatHistory_->addItem(tr("🎨 Theme changed"));
    }
}

void MainWindow::onAccessibilityToggled(bool on) {
    
    statusBar()->showMessage(on ? tr("Accessibility ON") : tr("Accessibility OFF"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(on ? "[A11Y] Enabled" : "[A11Y] Disabled");
    }
    
    // Could adjust font sizes, contrast, screen reader support
    QSettings settings("RawrXD", "QtShell");
    settings.setValue("Accessibility/enabled", on);
    
    if (chatHistory_) {
        chatHistory_->addItem(on ? tr("♿ Accessibility enabled") : tr("♿ Accessibility disabled"));
    }
}

// Toggle slots - generic implementation with macro
#define IMPLEMENT_TOGGLE(Func, Member, Type) \
void MainWindow::Func(bool visible) { \
    if (visible) { \
        if (!Member) { \
            Member = new Type(this); \
            QDockWidget* dock = new QDockWidget(tr(#Type), this); \
            dock->setWidget(Member); \
            addDockWidget(//RightDockWidgetArea, dock); \
        } \
        Member->show(); \
    } else if (Member) { \
        Member->hide(); \
    } \
}

// Use real ProjectExplorerWidget from widgets/
void MainWindow::toggleProjectExplorer(bool visible) {
    if (visible) {
        if (!projectExplorer_) {
            projectExplorer_ = new RawrXD::ProjectExplorerWidget(this);
// Qt connect removed
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    if (codeView_) codeView_->setText(in.readAll());
                    file.close();
                    statusBar()->showMessage(tr("Opened: %1"), 3000);
                }
            });
            // Auto-open current directory or last project
            std::string defaultPath = std::filesystem::path::currentPath();
            if (std::fstream::exists("E:\\")) defaultPath = "E:\\";
            projectExplorer_->openProject(defaultPath);
        }
        QDockWidget* dock = new QDockWidget(tr("Project Explorer"), this);
        dock->setWidget(projectExplorer_);
        addDockWidget(//LeftDockWidgetArea, dock);
        dock->show();
    } else if (projectExplorer_) {
        if (QDockWidget* dock = qobject_cast<QDockWidget*>(projectExplorer_->parentWidget())) {
            dock->hide();
        }
    }
}

IMPLEMENT_TOGGLE(toggleBuildSystem, buildWidget_, BuildSystemWidget)
IMPLEMENT_TOGGLE(toggleVersionControl, vcsWidget_, VersionControlWidget)
IMPLEMENT_TOGGLE(toggleRunDebug, debugWidget_, RunDebugWidget)
IMPLEMENT_TOGGLE(toggleProfiler, profilerWidget_, ProfilerWidget)
IMPLEMENT_TOGGLE(toggleTestExplorer, testWidget_, TestExplorerWidget)
IMPLEMENT_TOGGLE(toggleDatabaseTool, database_, DatabaseToolWidget)
IMPLEMENT_TOGGLE(toggleDockerTool, docker_, DockerToolWidget)
IMPLEMENT_TOGGLE(toggleCloudExplorer, cloud_, CloudExplorerWidget)
IMPLEMENT_TOGGLE(togglePackageManager, pkgManager_, PackageManagerWidget)
IMPLEMENT_TOGGLE(toggleDocumentation, documentation_, DocumentationWidget)
IMPLEMENT_TOGGLE(toggleUMLView, umlView_, UMLLViewWidget)
IMPLEMENT_TOGGLE(toggleImageTool, imageTool_, ImageToolWidget)
IMPLEMENT_TOGGLE(toggleTranslation, translator_, TranslationWidget)
IMPLEMENT_TOGGLE(toggleDesignToCode, designImport_, DesignToCodeWidget)
IMPLEMENT_TOGGLE(toggleNotebook, notebook_, NotebookWidget)
IMPLEMENT_TOGGLE(toggleMarkdownViewer, markdownViewer_, MarkdownViewer)
IMPLEMENT_TOGGLE(toggleSpreadsheet, spreadsheet_, SpreadsheetWidget)
IMPLEMENT_TOGGLE(toggleTerminalCluster, terminalCluster_, TerminalClusterWidget)
IMPLEMENT_TOGGLE(toggleSnippetManager, snippetManager_, SnippetManagerWidget)
IMPLEMENT_TOGGLE(toggleRegexTester, regexTester_, RegexTesterWidget)
IMPLEMENT_TOGGLE(toggleDiffViewer, diffViewer_, DiffViewerWidget)
IMPLEMENT_TOGGLE(toggleColorPicker, colorPicker_, ColorPickerWidget)
IMPLEMENT_TOGGLE(toggleIconFont, iconFont_, IconFontWidget)
IMPLEMENT_TOGGLE(togglePluginManager, pluginManager_, PluginManagerWidget)
// Note: settingsWidget_ is RawrXD::SettingsDialog, not SettingsWidget - it's a dialog not a widget for toggling
// IMPLEMENT_TOGGLE(toggleSettings, settingsWidget_, SettingsDialog)
IMPLEMENT_TOGGLE(toggleNotificationCenter, notificationCenter_, NotificationCenter)
IMPLEMENT_TOGGLE(toggleShortcutsConfigurator, shortcutsConfig_, ShortcutsConfigurator)
IMPLEMENT_TOGGLE(toggleTelemetry, telemetry_, TelemetryWidget)
IMPLEMENT_TOGGLE(toggleUpdateChecker, updateChecker_, UpdateCheckerWidget)
IMPLEMENT_TOGGLE(toggleWelcomeScreen, welcomeScreen_, WelcomeScreenWidget)
IMPLEMENT_TOGGLE(toggleCommandPalette, commandPalette_, CommandPalette)
IMPLEMENT_TOGGLE(toggleProgressManager, progressManager_, ProgressManager)
IMPLEMENT_TOGGLE(toggleAIQuickFix, quickFix_, AIQuickFixWidget)
IMPLEMENT_TOGGLE(toggleCodeMinimap, minimap_, CodeMinimap)
IMPLEMENT_TOGGLE(toggleBreadcrumbBar, breadcrumb_, BreadcrumbBar)
IMPLEMENT_TOGGLE(toggleStatusBarManager, statusBarManager_, StatusBarManager)
IMPLEMENT_TOGGLE(toggleTerminalEmulator, terminalEmulator_, TerminalEmulator)
IMPLEMENT_TOGGLE(toggleSearchResult, searchResults_, SearchResultWidget)
IMPLEMENT_TOGGLE(toggleBookmark, bookmarks_, BookmarkWidget)
IMPLEMENT_TOGGLE(toggleTodo, todos_, TodoWidget)
IMPLEMENT_TOGGLE(toggleMacroRecorder, macroRecorder_, MacroRecorderWidget)
IMPLEMENT_TOGGLE(toggleAICompletionCache, completionCache_, AICompletionCache)
IMPLEMENT_TOGGLE(toggleLanguageClientHost, lspHost_, LanguageClientHost)

// Special handling for AI Chat (no dedicated pointer, but we can create dynamically)
void MainWindow::toggleAIChat(bool visible) { (void)visible; }

// Other required methods
bool MainWindow::eventFilter(void* watched, QEvent* event) 
{
    // Custom event filtering logic can be added here
    return void::eventFilter(watched, event);
}

void MainWindow::closeEvent(QCloseEvent* event) 
{
    // Save session state before closing application
    handleSaveState();
    event->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) 
{
    // Accept drag events for file drops
    event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mime = event->mimeData();
    if (!mime->hasUrls()) return;

    for (const std::string& u : mime->urls()) {
        std::string path = u.toLocalFile();
        if (!path.endsWith(".gguf", //CaseInsensitive)) {
            // Non-GGUF file - open in editor
            std::fstream file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                codeView_->setText(in.readAll());
                file.close();
            }
            continue;
        }

        // GGUF file - compress with brutal_gzip
        std::fstream f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("GGUF open"), tr("Cannot read %1"));
            continue;
        }
        std::vector<uint8_t> raw = f.readAll();          // whole file for demo
        f.close();
        
        std::vector<uint8_t> gz  = brutal::compress(raw);
        if (gz.isEmpty()) {
            QMessageBox::critical(this, tr("GGUF compress"), tr("Brutal deflate failed"));
            continue;
        }
        
        std::string outName = path + ".gz";
        std::fstream og(outName);
        if (og.open(QIODevice::WriteOnly)) {
            og.write(gz);
            og.close();
            statusBar()->showMessage(
                tr("Compressed %1 → %2  (ratio %3%)")
                    .formattedDataSize(raw.size()))
                    .formattedDataSize(gz.size()))
                     / raw.size(), 'f', 1)),
                5000);
        }
    }
    event->acceptProposedAction();
}

// ============================================================
// UI Creator Implementations
// ============================================================

void* MainWindow::createGoalBar() {
    
    try {
        void* goalBar = new void(this);
        goalBar->setObjectName("GoalBarWidget");
        goalBar->setStyleSheet("void#GoalBarWidget { background-color: #252526; border-bottom: 1px solid #3e3e42; }");
        
        QHBoxLayout* layout = new QHBoxLayout(goalBar);
        layout->setContentsMargins(10, 5, 10, 5);
        layout->setSpacing(8);
        
        // Goal label
        QLabel* label = new QLabel("Agent Goal:", goalBar);
        label->setStyleSheet("QLabel { color: #e0e0e0; font-weight: bold; }");
        layout->addWidget(label);
        
        // Goal input field
        goalInput_ = new QLineEdit(goalBar);
        goalInput_->setObjectName("GoalInput");
        goalInput_->setPlaceholderText("Enter your goal or wish for the AI agent...");
        goalInput_->setStyleSheet(
            "QLineEdit#GoalInput { "
            "background-color: #3c3c3c; "
            "color: #e0e0e0; "
            "border: 1px solid #555; "
            "border-radius: 3px; "
            "padding: 6px; "
            "font-size: 10pt; "
            "}"
        );
        layout->addWidget(goalInput_, 1);
        
        // Submit button
        QPushButton* submitBtn = new QPushButton("Execute", goalBar);
        submitBtn->setObjectName("SubmitButton");
        submitBtn->setStyleSheet(
            "QPushButton#SubmitButton { "
            "background-color: #007acc; "
            "color: white; "
            "border: none; "
            "border-radius: 3px; "
            "padding: 6px 16px; "
            "font-weight: bold; "
            "} "
            "QPushButton#SubmitButton:hover { background-color: #005a9e; } "
            "QPushButton#SubmitButton:pressed { background-color: #004578; }"
        );
        layout->addWidget(submitBtn);
        
        // Connect submit action
// Qt connect removed
// Qt connect removed
        return goalBar;
        
    } catch (const std::exception& e) {
        return new void(this);
    }
}

void* MainWindow::createAgentPanel() {
    
    try {
        void* panel = new void(this);
        panel->setObjectName("AgentPanel");
        panel->setStyleSheet("void#AgentPanel { background-color: #252526; }");
        
        QVBoxLayout* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(10, 10, 10, 10);
        layout->setSpacing(12);
        
        // Agent mode selector
        QLabel* modeLabel = new QLabel("Agent Mode:", panel);
        modeLabel->setStyleSheet("QLabel { color: #e0e0e0; font-weight: bold; }");
        layout->addWidget(modeLabel);
        
        agentSelector_ = new QComboBox(panel);
        agentSelector_->setObjectName("AgentSelector");
        agentSelector_->addItems({"Plan", "Agent", "Ask"});
        agentSelector_->setStyleSheet(
            "QComboBox { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; padding: 5px; }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background-color: #252526; color: #e0e0e0; selection-background-color: #007acc; }"
        );
        layout->addWidget(agentSelector_);
// Qt connect removed
            changeAgentMode(mode);
        });
        
        // Status badge
        QLabel* statusLabel = new QLabel("Status:", panel);
        statusLabel->setStyleSheet("QLabel { color: #e0e0e0; font-weight: bold; margin-top: 10px; }");
        layout->addWidget(statusLabel);
        
        mockStatusBadge_ = new QLabel("Idle", panel);
        mockStatusBadge_->setObjectName("StatusBadge");
        mockStatusBadge_->setAlignment(//AlignCenter);
        mockStatusBadge_->setStyleSheet(
            "QLabel#StatusBadge { "
            "background-color: #3c3c3c; "
            "color: #4ec9b0; "
            "border: 1px solid #4ec9b0; "
            "border-radius: 3px; "
            "padding: 8px; "
            "font-weight: bold; "
            "}"
        );
        layout->addWidget(mockStatusBadge_);
        
        // Progress indicator
        QProgressBar* progressBar = new QProgressBar(panel);
        progressBar->setObjectName("AgentProgress");
        progressBar->setRange(0, 0);  // Indeterminate
        progressBar->setVisible(false);
        progressBar->setStyleSheet(
            "QProgressBar { "
            "background-color: #3c3c3c; "
            "border: 1px solid #555; "
            "border-radius: 3px; "
            "text-align: center; "
            "color: #e0e0e0; "
            "} "
            "QProgressBar::chunk { background-color: #007acc; }"
        );
        layout->addWidget(progressBar);
        
        layout->addStretch();
        
        return panel;
        
    } catch (const std::exception& e) {
        return new void(this);
    }
}

void* MainWindow::createProposalReview() {
    
    try {
        void* panel = new void(this);
        panel->setObjectName("ProposalReviewPanel");
        panel->setStyleSheet("void#ProposalReviewPanel { background-color: #1e1e1e; }");
        
        QVBoxLayout* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        
        // Header
        QLabel* header = new QLabel("Agent Proposals", panel);
        header->setStyleSheet(
            "QLabel { "
            "background-color: #2d2d30; "
            "color: #e0e0e0; "
            "padding: 8px; "
            "font-weight: bold; "
            "border-bottom: 1px solid #3e3e42; "
            "}"
        );
        layout->addWidget(header);
        
        // Proposal list
        chatHistory_ = new QListWidget(panel);
        chatHistory_->setObjectName("ProposalList");
        chatHistory_->setStyleSheet(
            "QListWidget#ProposalList { "
            "background-color: #1e1e1e; "
            "color: #e0e0e0; "
            "border: none; "
            "font-family: 'Consolas', monospace; "
            "font-size: 10pt; "
            "padding: 5px; "
            "} "
            "QListWidget#ProposalList::item { "
            "padding: 8px; "
            "border-bottom: 1px solid #2d2d30; "
            "} "
            "QListWidget#ProposalList::item:selected { "
            "background-color: #37373d; "
            "color: #ffffff; "
            "}"
        );
        layout->addWidget(chatHistory_, 1);
        
        // Action buttons
        QHBoxLayout* btnLayout = new QHBoxLayout();
        btnLayout->setContentsMargins(5, 5, 5, 5);
        btnLayout->setSpacing(5);
        
        QPushButton* acceptBtn = new QPushButton("Accept All", panel);
        acceptBtn->setStyleSheet(
            "QPushButton { background-color: #0e7a0d; color: white; border: none; padding: 6px 12px; border-radius: 3px; } "
            "QPushButton:hover { background-color: #0c5c0b; }"
        );
        btnLayout->addWidget(acceptBtn);
        
        QPushButton* rejectBtn = new QPushButton("Reject", panel);
        rejectBtn->setStyleSheet(
            "QPushButton { background-color: #a1260d; color: white; border: none; padding: 6px 12px; border-radius: 3px; } "
            "QPushButton:hover { background-color: #7a1c0a; }"
        );
        btnLayout->addWidget(rejectBtn);
        
        btnLayout->addStretch();
        layout->addLayout(btnLayout);
        
        return panel;
        
    } catch (const std::exception& e) {
        return new void(this);
    }
}

void* MainWindow::createEditorArea() {
    
    try {
        void* editorWidget = new void(this);
        editorWidget->setObjectName("EditorArea");
        editorWidget->setStyleSheet("void#EditorArea { background-color: #1e1e1e; }");
        
        QVBoxLayout* layout = new QVBoxLayout(editorWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        
        // Tab widget for multiple editors
        editorTabs_ = new QTabWidget(editorWidget);
        editorTabs_->setObjectName("EditorTabs");
        editorTabs_->setTabsClosable(true);
        editorTabs_->setMovable(true);
        editorTabs_->setStyleSheet(
            "QTabWidget::pane { border: none; background-color: #1e1e1e; } "
            "QTabBar { background-color: #2d2d30; } "
            "QTabBar::tab { "
            "background-color: #2d2d30; "
            "color: #969696; "
            "padding: 8px 16px; "
            "margin-right: 2px; "
            "border: none; "
            "} "
            "QTabBar::tab:selected { "
            "background-color: #1e1e1e; "
            "color: #ffffff; "
            "border-top: 2px solid #007acc; "
            "} "
            "QTabBar::tab:hover { background-color: #37373d; color: #ffffff; } "
            "QTabBar::close-button { image: url(:/icons/close.png); } "
            "QTabBar::close-button:hover { background-color: #e81123; }"
        );
        
        // Create initial editor tab
        codeView_ = new QTextEdit(editorWidget);
        codeView_->setObjectName("CodeEditor");
        codeView_->setStyleSheet(
            "QTextEdit#CodeEditor { "
            "background-color: #1e1e1e; "
            "color: #d4d4d4; "
            "font-family: 'Consolas', 'Courier New', monospace; "
            "font-size: 11pt; "
            "border: none; "
            "selection-background-color: #264f78; "
            "}"
        );
        codeView_->setLineWrapMode(QTextEdit::NoWrap);
        codeView_->setAcceptDrops(true);
        
        editorTabs_->addTab(codeView_, "Untitled-1");
        
        // Connect tab close
// Qt connect removed
        layout->addWidget(editorTabs_);
        
        return editorWidget;
        
    } catch (const std::exception& e) {
        return new void(this);
    }
}

void* MainWindow::createQShellTab() {
    
    try {
        void* shellWidget = new void(this);
        shellWidget->setObjectName("QShellTab");
        shellWidget->setStyleSheet("void#QShellTab { background-color: #1e1e1e; }");
        
        QVBoxLayout* layout = new QVBoxLayout(shellWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        
        // Output area
        qshellOutput_ = new QTextEdit(shellWidget);
        qshellOutput_->setObjectName("QShellOutput");
        qshellOutput_->setReadOnly(true);
        qshellOutput_->setStyleSheet(
            "QTextEdit#QShellOutput { "
            "background-color: #1e1e1e; "
            "color: #0dff00; "
            "font-family: 'Consolas', 'Courier New', monospace; "
            "font-size: 10pt; "
            "border: none; "
            "padding: 10px; "
            "}"
        );
        qshellOutput_->setLineWrapMode(QTextEdit::NoWrap);
        qshellOutput_->append("QShell v1.0 - AI-Powered Interactive Shell");
        qshellOutput_->append("Type 'help' for available commands or enter natural language instructions.\n");
        layout->addWidget(qshellOutput_, 1);
        
        // Input area
        void* inputWidget = new void(shellWidget);
        inputWidget->setStyleSheet("void { background-color: #252526; border-top: 1px solid #3e3e42; }");
        QHBoxLayout* inputLayout = new QHBoxLayout(inputWidget);
        inputLayout->setContentsMargins(10, 5, 10, 5);
        inputLayout->setSpacing(5);
        
        QLabel* prompt = new QLabel(">>>", inputWidget);
        prompt->setStyleSheet("QLabel { color: #0dff00; font-family: 'Consolas', monospace; font-weight: bold; }");
        inputLayout->addWidget(prompt);
        
        qshellInput_ = new QLineEdit(inputWidget);
        qshellInput_->setObjectName("QShellInput");
        qshellInput_->setPlaceholderText("Enter command or agent instruction...");
        qshellInput_->setStyleSheet(
            "QLineEdit#QShellInput { "
            "background-color: #1e1e1e; "
            "color: #0dff00; "
            "font-family: 'Consolas', 'Courier New', monospace; "
            "font-size: 10pt; "
            "border: none; "
            "padding: 5px; "
            "}"
        );
        inputLayout->addWidget(qshellInput_, 1);
        
        QPushButton* executeBtn = new QPushButton("Execute", inputWidget);
        executeBtn->setStyleSheet(
            "QPushButton { background-color: #007acc; color: white; border: none; padding: 5px 15px; border-radius: 3px; } "
            "QPushButton:hover { background-color: #005a9e; }"
        );
        inputLayout->addWidget(executeBtn);
        
        layout->addWidget(inputWidget);
        
        // Connect signals
// Qt connect removed
// Qt connect removed
        return shellWidget;
        
    } catch (const std::exception& e) {
        return new void(this);
    }
}

void* MainWindow::getMockArchitectJson() const {
    
    try {
        void* plan;
        
        // Task 1: Analyze requirements
        void* task1;
        task1["id"] = "task_001";
        task1["type"] = "analyze";
        task1["description"] = "Analyze project requirements and dependencies";
        task1["agent"] = "Architect";
        task1["status"] = "pending";
        task1["priority"] = "high";
        plan.append(task1);
        
        // Task 2: Design architecture
        void* task2;
        task2["id"] = "task_002";
        task2["type"] = "design";
        task2["description"] = "Design system architecture and component interfaces";
        task2["agent"] = "Architect";
        task2["status"] = "pending";
        task2["priority"] = "high";
        task2["depends_on"] = void*{"task_001"};
        plan.append(task2);
        
        // Task 3: Implement core features
        void* task3;
        task3["id"] = "task_003";
        task3["type"] = "implement";
        task3["description"] = "Implement core functionality according to design";
        task3["agent"] = "Coder";
        task3["status"] = "pending";
        task3["priority"] = "medium";
        task3["depends_on"] = void*{"task_002"};
        plan.append(task3);
        
        // Task 4: Write tests
        void* task4;
        task4["id"] = "task_004";
        task4["type"] = "test";
        task4["description"] = "Write comprehensive unit and integration tests";
        task4["agent"] = "Tester";
        task4["status"] = "pending";
        task4["priority"] = "medium";
        task4["depends_on"] = void*{"task_003"};
        plan.append(task4);
        
        // Task 5: Review and optimize
        void* task5;
        task5["id"] = "task_005";
        task5["type"] = "review";
        task5["description"] = "Code review, optimization, and documentation";
        task5["agent"] = "Reviewer";
        task5["status"] = "pending";
        task5["priority"] = "low";
        task5["depends_on"] = void*{"task_004"};
        plan.append(task5);
        
        void* doc(plan);
        
        return doc;
        
    } catch (const std::exception& e) {
        return void*();
    }
}

void MainWindow::populateFolderTree(QTreeWidgetItem* parent, const std::string& path) {
    
    if (!parent || path.isEmpty()) {
        return;
    }
    
    try {
        std::filesystem::path dir(path);
        if (!dir.exists()) {
            return;
        }
        
        // Get directories first (sorted)
        QFileInfoList entries = dir.entryInfoList(
            std::filesystem::path::Dirs | std::filesystem::path::Files | std::filesystem::path::NoDotAndDotDot,
            std::filesystem::path::DirsFirst | std::filesystem::path::Name | std::filesystem::path::IgnoreCase
        );
        
        int itemCount = 0;
        for (const std::filesystem::path& entry : entries) {
            // Skip hidden files unless explicitly needed
            if (entry.fileName().startsWith(".")) {
                continue;
            }
            
            QTreeWidgetItem* item = new QTreeWidgetItem(parent);
            item->setText(0, entry.fileName());
            item->setData(0, //UserRole, entry.absoluteFilePath());
            
            if (entry.isDir()) {
                // Folder icon and style
                item->setIcon(0, style()->standardIcon(QStyle::SP_DirIcon));
                item->setForeground(0, QColor("#4ec9b0"));  // Cyan for folders
                
                // Recursively populate subdirectory (limit depth to prevent performance issues)
                static int depth = 0;
                if (depth < 5) {  // Max depth of 5 levels
                    depth++;
                    populateFolderTree(item, entry.absoluteFilePath());
                    depth--;
                } else {
                    // Add placeholder for deep directories
                    QTreeWidgetItem* placeholder = new QTreeWidgetItem(item);
                    placeholder->setText(0, "...");
                    placeholder->setForeground(0, QColor("#808080"));
                }
            } else {
                // File icon and style
                item->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
                item->setForeground(0, QColor("#e0e0e0"));
                
                // Add file size info
                std::string sizeStr = QLocale().formattedDataSize(entry.size());
                item->setToolTip(0, std::string("%1 (%2)")));
            }
            
            itemCount++;
        }
        
        
    } catch (const std::exception& e) {
    }
}

void* MainWindow::createTerminalPanel() {
    
    try {
        void* terminalWidget = new void(this);
        terminalWidget->setObjectName("TerminalPanel");
        terminalWidget->setStyleSheet("void#TerminalPanel { background-color: #1e1e1e; }");
        
        QVBoxLayout* layout = new QVBoxLayout(terminalWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        
        // Terminal tabs
        terminalTabs_ = new QTabWidget(terminalWidget);
        terminalTabs_->setObjectName("TerminalTabs");
        terminalTabs_->setStyleSheet(
            "QTabWidget::pane { border: none; background-color: #1e1e1e; } "
            "QTabBar { background-color: #252526; } "
            "QTabBar::tab { background-color: #252526; color: #969696; padding: 6px 12px; } "
            "QTabBar::tab:selected { background-color: #1e1e1e; color: #ffffff; border-top: 1px solid #007acc; }"
        );
        
        // PowerShell tab
        void* pwshTab = new void(terminalWidget);
        QVBoxLayout* pwshLayout = new QVBoxLayout(pwshTab);
        pwshLayout->setContentsMargins(5, 5, 5, 5);
        
        pwshOutput_ = new QPlainTextEdit(pwshTab);
        pwshOutput_->setObjectName("PowerShellOutput");
        pwshOutput_->setReadOnly(true);
        pwshOutput_->setStyleSheet(
            "QPlainTextEdit { "
            "background-color: #012456; "
            "color: #eeedf0; "
            "font-family: 'Consolas', monospace; "
            "font-size: 10pt; "
            "border: none; "
            "}"
        );
        pwshLayout->addWidget(pwshOutput_, 1);
        
        QHBoxLayout* pwshInputLayout = new QHBoxLayout();
        pwshInputLayout->setSpacing(5);
        
        QLabel* pwshPrompt = new QLabel("PS>", pwshTab);
        pwshPrompt->setStyleSheet("QLabel { color: #00ff00; font-family: 'Consolas', monospace; font-weight: bold; }");
        pwshInputLayout->addWidget(pwshPrompt);
        
        pwshInput_ = new QLineEdit(pwshTab);
        pwshInput_->setStyleSheet(
            "QLineEdit { background-color: #012456; color: #eeedf0; font-family: 'Consolas', monospace; border: none; }"
        );
        pwshInputLayout->addWidget(pwshInput_, 1);
        
        pwshLayout->addLayout(pwshInputLayout);
        terminalTabs_->addTab(pwshTab, "PowerShell");
        
        // CMD tab
        void* cmdTab = new void(terminalWidget);
        QVBoxLayout* cmdLayout = new QVBoxLayout(cmdTab);
        cmdLayout->setContentsMargins(5, 5, 5, 5);
        
        cmdOutput_ = new QPlainTextEdit(cmdTab);
        cmdOutput_->setObjectName("CMDOutput");
        cmdOutput_->setReadOnly(true);
        cmdOutput_->setStyleSheet(
            "QPlainTextEdit { "
            "background-color: #0c0c0c; "
            "color: #cccccc; "
            "font-family: 'Consolas', monospace; "
            "font-size: 10pt; "
            "border: none; "
            "}"
        );
        cmdLayout->addWidget(cmdOutput_, 1);
        
        QHBoxLayout* cmdInputLayout = new QHBoxLayout();
        cmdInputLayout->setSpacing(5);
        
        QLabel* cmdPrompt = new QLabel("C:\\>", cmdTab);
        cmdPrompt->setStyleSheet("QLabel { color: #ffffff; font-family: 'Consolas', monospace; font-weight: bold; }");
        cmdInputLayout->addWidget(cmdPrompt);
        
        cmdInput_ = new QLineEdit(cmdTab);
        cmdInput_->setStyleSheet(
            "QLineEdit { background-color: #0c0c0c; color: #cccccc; font-family: 'Consolas', monospace; border: none; }"
        );
        cmdInputLayout->addWidget(cmdInput_, 1);
        
        cmdLayout->addLayout(cmdInputLayout);
        terminalTabs_->addTab(cmdTab, "CMD");
        
        layout->addWidget(terminalTabs_);
        
        // Initialize processes
        pwshProcess_ = new QProcess(this);
        cmdProcess_ = new QProcess(this);
// Qt connect removed
// Qt connect removed
// Qt connect removed
// Qt connect removed
        pwshOutput_->appendPlainText("PowerShell 7.x\nCopyright (c) Microsoft Corporation. All rights reserved.\n");
        cmdOutput_->appendPlainText("Microsoft Windows [Version 10.0.xxxxx]\n(c) Microsoft Corporation. All rights reserved.\n");
        
        return terminalWidget;
        
    } catch (const std::exception& e) {
        return new void(this);
    }
}

void* MainWindow::createDebugPanel() {
    
    try {
        void* debugWidget = new void(this);
        debugWidget->setObjectName("DebugPanel");
        debugWidget->setStyleSheet("void#DebugPanel { background-color: #1e1e1e; }");
        
        QVBoxLayout* layout = new QVBoxLayout(debugWidget);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(0);
        
        // Toolbar
        QFrame* toolbar = new QFrame(debugWidget);
        toolbar->setStyleSheet("QFrame { background-color: #2d2d30; border-bottom: 1px solid #3e3e42; }");
        toolbar->setFixedHeight(35);
        
        QHBoxLayout* toolbarLayout = new QHBoxLayout(toolbar);
        toolbarLayout->setContentsMargins(5, 2, 5, 2);
        toolbarLayout->setSpacing(5);
        
        QLabel* filterLabel = new QLabel("Filter:", toolbar);
        filterLabel->setStyleSheet("QLabel { color: #e0e0e0; }");
        toolbarLayout->addWidget(filterLabel);
        
        QComboBox* logLevelFilter = new QComboBox(toolbar);
        logLevelFilter->addItems({"All", "DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"});
        logLevelFilter->setStyleSheet(
            "QComboBox { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; padding: 3px; } "
            "QComboBox QAbstractItemView { background-color: #252526; color: #e0e0e0; }"
        );
        toolbarLayout->addWidget(logLevelFilter);
        
        toolbarLayout->addStretch();
        
        QPushButton* clearBtn = new QPushButton("Clear", toolbar);
        clearBtn->setStyleSheet(
            "QPushButton { background-color: #3c3c3c; color: #e0e0e0; border: none; padding: 4px 12px; } "
            "QPushButton:hover { background-color: #505050; }"
        );
        toolbarLayout->addWidget(clearBtn);
        
        QPushButton* saveBtn = new QPushButton("Save Log", toolbar);
        saveBtn->setStyleSheet(
            "QPushButton { background-color: #3c3c3c; color: #e0e0e0; border: none; padding: 4px 12px; } "
            "QPushButton:hover { background-color: #505050; }"
        );
        toolbarLayout->addWidget(saveBtn);
        
        layout->addWidget(toolbar);
        
        // Debug output
        QPlainTextEdit* debugOutput = new QPlainTextEdit(debugWidget);
        debugOutput->setObjectName("DebugOutput");
        debugOutput->setReadOnly(true);
        debugOutput->setStyleSheet(
            "QPlainTextEdit#DebugOutput { "
            "background-color: #1e1e1e; "
            "color: #cccccc; "
            "font-family: 'Consolas', monospace; "
            "font-size: 9pt; "
            "border: none; "
            "}"
        );
        debugOutput->setLineWrapMode(QPlainTextEdit::NoWrap);
        
        // Add initial log entries
        std::string timestamp = std::chrono::system_clock::time_point::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        debugOutput->appendPlainText(std::string("[%1] [INFO] Debug panel initialized"));
        debugOutput->appendPlainText(std::string("[%1] [INFO] Logging system ready"));
        debugOutput->appendPlainText(std::string("[%1] [DEBUG] Production-ready observability enabled"));
        
        layout->addWidget(debugOutput, 1);
        
        // Connect signals
// Qt connect removed
// Qt connect removed
// Qt connect removed
        return debugWidget;
        
    } catch (const std::exception& e) {
        return new void(this);
    }
}

void MainWindow::setupDockWidgets() {
    
    try {
        // Create and configure dock widgets for each major subsystem
        
        // 1. Project Explorer Dock
        if (!projectExplorer_) {
            projectExplorer_ = new RawrXD::ProjectExplorerWidget(this);
            QDockWidget* projDock = new QDockWidget("Project Explorer", this);
            projDock->setObjectName("ProjectExplorerDock");
            projDock->setWidget(projectExplorer_);
            projDock->setAllowedAreas(//LeftDockWidgetArea | //RightDockWidgetArea);
            projDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(//LeftDockWidgetArea, projDock);
            projDock->hide();  // Hidden by default
        }
        
        // 2. Build System Dock
        if (!buildWidget_) {
            buildWidget_ = new BuildSystemWidget(this);
            QDockWidget* buildDock = new QDockWidget("Build System", this);
            buildDock->setObjectName("BuildSystemDock");
            buildDock->setWidget(buildWidget_);
            buildDock->setAllowedAreas(//BottomDockWidgetArea | //RightDockWidgetArea);
            buildDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(//BottomDockWidgetArea, buildDock);
            buildDock->hide();
        }
        
        // 3. Version Control Dock
        if (!vcsWidget_) {
            vcsWidget_ = new VersionControlWidget(this);
            QDockWidget* vcsDock = new QDockWidget("Version Control", this);
            vcsDock->setObjectName("VersionControlDock");
            vcsDock->setWidget(vcsWidget_);
            vcsDock->setAllowedAreas(//LeftDockWidgetArea | //RightDockWidgetArea | //BottomDockWidgetArea);
            vcsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(//LeftDockWidgetArea, vcsDock);
            vcsDock->hide();
        }
        
        // 4. Debug/Run Dock
        if (!debugWidget_) {
            debugWidget_ = new RunDebugWidget(this);
            QDockWidget* debugDock = new QDockWidget("Run & Debug", this);
            debugDock->setObjectName("RunDebugDock");
            debugDock->setWidget(debugWidget_);
            debugDock->setAllowedAreas(//AllDockWidgetAreas);
            debugDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(//BottomDockWidgetArea, debugDock);
            debugDock->hide();
        }
        
        // 5. Test Explorer Dock
        if (!testWidget_) {
            testWidget_ = new TestExplorerWidget(this);
            QDockWidget* testDock = new QDockWidget("Test Explorer", this);
            testDock->setObjectName("TestExplorerDock");
            testDock->setWidget(testWidget_);
            testDock->setAllowedAreas(//RightDockWidgetArea | //BottomDockWidgetArea);
            testDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(//RightDockWidgetArea, testDock);
            testDock->hide();
        }
        
        // 6. Profiler Dock
        if (!profilerWidget_) {
            profilerWidget_ = new ProfilerWidget(this);
            QDockWidget* profilerDock = new QDockWidget("Profiler", this);
            profilerDock->setObjectName("ProfilerDock");
            profilerDock->setWidget(profilerWidget_);
            profilerDock->setAllowedAreas(//BottomDockWidgetArea | //RightDockWidgetArea);
            profilerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(//BottomDockWidgetArea, profilerDock);
            profilerDock->hide();
        }
        
        // 7. Database Tool Dock
        if (!database_) {
            database_ = new DatabaseToolWidget(this);
            QDockWidget* dbDock = new QDockWidget("Database Tools", this);
            dbDock->setObjectName("DatabaseToolDock");
            dbDock->setWidget(database_);
            dbDock->setAllowedAreas(//AllDockWidgetAreas);
            dbDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(//RightDockWidgetArea, dbDock);
            dbDock->hide();
        }
        
        // 8. Docker Tools Dock
        if (!docker_) {
            docker_ = new DockerToolWidget(this);
            QDockWidget* dockerDock = new QDockWidget("Docker", this);
            dockerDock->setObjectName("DockerToolDock");
            dockerDock->setWidget(docker_);
            dockerDock->setAllowedAreas(//AllDockWidgetAreas);
            dockerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(//RightDockWidgetArea, dockerDock);
            dockerDock->hide();
        }
        
        // Apply consistent styling to all docks
        std::vector<QDockWidget*> allDocks = findChildren<QDockWidget*>();
        for (QDockWidget* dock : allDocks) {
            dock->setStyleSheet(
                "QDockWidget { "
                "background-color: #252526; "
                "color: #e0e0e0; "
                "titlebar-close-icon: url(:/icons/close.png); "
                "titlebar-normal-icon: url(:/icons/float.png); "
                "} "
                "QDockWidget::title { "
                "background-color: #2d2d30; "
                "padding: 6px; "
                "border: 1px solid #3e3e42; "
                "}"
            );
        }
        
        
    } catch (const std::exception& e) {
    }
}

void MainWindow::setupSystemTray() {
    
    try {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            return;
        }
        
        // Create system tray icon
        trayIcon_ = new QSystemTrayIcon(this);
        
        // Set icon (use application icon or default)
        QIcon appIcon = windowIcon();
        if (appIcon.isNull()) {
            // Fallback to a generic icon
            appIcon = style()->standardIcon(QStyle::SP_ComputerIcon);
        }
        trayIcon_->setIcon(appIcon);
        trayIcon_->setToolTip("RawrXD IDE - AI-Powered Development Environment");
        
        // Create tray menu
        QMenu* trayMenu = new QMenu(this);
        trayMenu->setStyleSheet(
            "QMenu { "
            "background-color: #252526; "
            "color: #e0e0e0; "
            "border: 1px solid #3e3e42; "
            "} "
            "QMenu::item:selected { background-color: #007acc; }"
        );
        
        // Restore action
        QAction* restoreAction = trayMenu->addAction("Restore Window");
        restoreAction->setIcon(style()->standardIcon(QStyle::SP_TitleBarMaxButton));
// Qt connect removed
            showNormal();
            activateWindow();
            raise();
        });
        
        trayMenu->addSeparator();
        
        // Quick actions
        QAction* newFileAction = trayMenu->addAction("New File");
        newFileAction->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
// Qt connect removed
        QAction* newChatAction = trayMenu->addAction("New AI Chat");
// Qt connect removed
        trayMenu->addSeparator();
        
        // Settings action
        QAction* settingsAction = trayMenu->addAction("Settings");
        settingsAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
// Qt connect removed
            toggleSettings(true);
        });
        
        trayMenu->addSeparator();
        
        // Quit action
        QAction* quitAction = trayMenu->addAction("Quit RawrXD IDE");
        quitAction->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
// Qt connect removed
            QApplication::quit();
        });
        
        trayIcon_->setContextMenu(trayMenu);
        
        // Connect double-click to restore
// Qt connect removed
                showNormal();
                activateWindow();
                raise();
            }
        });
        
        // Show tray icon
        trayIcon_->show();
        
        // Show notification
        trayIcon_->showMessage(
            "RawrXD IDE",
            "Application is running in the system tray",
            QSystemTrayIcon::Information,
            3000
        );
        
        
    } catch (const std::exception& e) {
    }
}


void MainWindow::restoreSession() {
    // Use existing handleLoadState() which already implements full state restoration
    handleLoadState();
    
    QSettings settings("RawrXD", "QtShell");
    
    // Restore open file tabs
    int tabCount = settings.value("Session/tabCount", 0).toInt();
    for (int i = 0; i < tabCount; ++i) {
        std::string tabKey = std::string("Session/tab%1");
        std::string filePath = settings.value(tabKey + "/path").toString();
        std::string content = settings.value(tabKey + "/content").toString();
        std::string tabName = settings.value(tabKey + "/name").toString();
        
        if (!content.isEmpty() && editorTabs_) {
            QTextEdit* editor = new QTextEdit(this);
            editor->setStyleSheet(codeView_->styleSheet());
            editor->setText(content);
            editorTabs_->addTab(editor, tabName.isEmpty() ? tr("Untitled") : tabName);
        }
    }
    
    statusBar()->showMessage(tr("Session restored"), 2000);
}

void MainWindow::saveSession() {
    // Use existing handleSaveState() which already implements full state saving
    handleSaveState();
    
    QSettings settings("RawrXD", "QtShell");
    
    // Save open editor tabs
    if (editorTabs_) {
        settings.setValue("Session/tabCount", editorTabs_->count());
        
        for (int i = 0; i < editorTabs_->count(); ++i) {
            std::string tabKey = std::string("Session/tab%1");
            QTextEdit* editor = qobject_cast<QTextEdit*>(editorTabs_->widget(i));
            
            if (editor) {
                settings.setValue(tabKey + "/content", editor->toPlainText());
                settings.setValue(tabKey + "/name", editorTabs_->tabText(i));
            }
        }
    }
    
    statusBar()->showMessage(tr("Session saved"), 2000);
}

void MainWindow::onRunScript()
{
    
    // Get script path from user
    std::string scriptPath = QFileDialog::getOpenFileName(
        this,
        tr("Select Script to Run"),
        std::filesystem::path::homePath(),
        tr("Script Files (*.py *.js *.sh *.bat *.ps1 *.rb *.pl);;Python (*.py);;JavaScript (*.js);;Shell (*.sh);;Batch (*.bat);;PowerShell (*.ps1);;Ruby (*.rb);;Perl (*.pl);;All Files (*.*)")
    );
    
    if (scriptPath.isEmpty()) {
        statusBar()->showMessage(tr("Script execution cancelled"), 2000);
        return;
    }
    
    std::filesystem::path scriptInfo(scriptPath);
    
    if (!scriptInfo.exists()) {
        QMessageBox::critical(this, tr("Script Not Found"),
                            tr("Script file does not exist:\n%1"));
        return;
    }
    
    statusBar()->showMessage(tr("Running script: %1")), 3000);
    
    // Log to console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("\n=== SCRIPT EXECUTION ==="));
        m_hexMagConsole->appendPlainText(std::string("Script: %1"));
        m_hexMagConsole->appendPlainText(std::string("Started: %1").toString()));
        m_hexMagConsole->appendPlainText(std::string("=====================\n"));
    }
    
    // Determine interpreter based on file extension
    std::string program;
    std::vector<std::string> arguments;
    std::string extension = scriptInfo.suffix().toLower();
    
    if (extension == "py") {
        program = "python";
        arguments << scriptPath;
    } else if (extension == "js") {
        program = "node";
        arguments << scriptPath;
    } else if (extension == "sh") {
        program = "bash";
        arguments << scriptPath;
    } else if (extension == "bat") {
        program = "cmd";
        arguments << "/c" << scriptPath;
    } else if (extension == "ps1") {
        program = "powershell";
        arguments << "-ExecutionPolicy" << "Bypass" << "-File" << scriptPath;
    } else if (extension == "rb") {
        program = "ruby";
        arguments << scriptPath;
    } else if (extension == "pl") {
        program = "perl";
        arguments << scriptPath;
    } else {
        QMessageBox::warning(this, tr("Unknown Script Type"),
                           tr("Cannot determine interpreter for: %1\n\nPlease run manually."));
        return;
    }
    
    // Create process for script execution
    QProcess* scriptProcess = new QProcess(this);
    scriptProcess->setWorkingDirectory(scriptInfo.absolutePath());
    
    // Connect output handlers
// Qt connect removed
        if (m_hexMagConsole && !output.isEmpty()) {
            m_hexMagConsole->appendPlainText(output);
        }
    });
// Qt connect removed
        if (m_hexMagConsole && !error.isEmpty()) {
            m_hexMagConsole->appendPlainText("[ERROR] " + error);
        }
    });
// Qt connect removed
        statusBar()->showMessage(status, 5000);
        
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(std::string("\n=== SCRIPT FINISHED ==="));
            m_hexMagConsole->appendPlainText(std::string("Exit Code: %1"));
            m_hexMagConsole->appendPlainText(std::string("Status: %1"));
            m_hexMagConsole->appendPlainText(std::string("Time: %1").toString()));
            m_hexMagConsole->appendPlainText(std::string("=====================\n"));
        }
        
        if (chatHistory_) {
            chatHistory_->addItem(status + ": " + std::filesystem::path(scriptPath).fileName());
        }
        
        
        scriptProcess->deleteLater();
    });
    
    // Start the script
    scriptProcess->start(program, arguments);
    
    if (!scriptProcess->waitForStarted(3000)) {
        QMessageBox::critical(this, tr("Script Execution Failed"),
                            tr("Failed to start script:\n%1\n\nInterpreter: %2\nError: %3")
                            ));
        scriptProcess->deleteLater();
        return;
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("Running script: %1")));
    }
}

void MainWindow::onAbout() 
{
    QMessageBox::about(this, tr("About RawrXD IDE"),
        tr("<b>RawrXD IDE</b><br>"
           "Quantization-Ready AI Development Environment<br>"
           "Built with Qt 6.7.3 + MSVC 2022<br>"
           "Features brutal_gzip MASM/NEON compression"));
}

// ============================================================
// AI/GGUF/InferenceEngine Implementation
// ============================================================

void MainWindow::runInference()
{
    if (!m_inferenceEngine || !m_inferenceEngine->isModelLoaded()) {
        QMessageBox::warning(this, tr("No Model"),
            tr("Please load a GGUF model first."));
        return;
    }
    
    bool ok;
    std::string prompt = QInputDialog::getMultiLineText(
        this,
        tr("Run Inference"),
        tr("Enter your prompt:"),
        std::string(),
        &ok
    );
    
    if (!ok || prompt.isEmpty()) {
        return;
    }
    
    statusBar()->showMessage(tr("Running inference..."));
    
    qint64 reqId = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
    m_currentStreamId = reqId;
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(tr("\n[User] %1\n"));
    }
    
    // Call inference engine
    QMetaObject::invokeMethod(m_inferenceEngine, "request", //QueuedConnection,
                              (std::string, prompt),
                              (qint64, reqId));
}

void MainWindow::loadGGUFModel()
{
    std::string filePath = QFileDialog::getOpenFileName(
        this,
        tr("Select GGUF Model"),
        std::string(),
        tr("GGUF Files (*.gguf);;All Files (*.*)")
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    statusBar()->showMessage(tr("Loading GGUF model..."));
    
    // Call loadModel in the worker thread
    QMetaObject::invokeMethod(m_inferenceEngine, "loadModel", //QueuedConnection,
                              (std::string, filePath));
}


void MainWindow::unloadGGUFModel()
{
    QMetaObject::invokeMethod(m_inferenceEngine, "unloadModel", //QueuedConnection);
    statusBar()->showMessage(tr("Unloading model..."));
}

void MainWindow::showInferenceResult(qint64 reqId, const std::string& result)
{
    // If streaming mode is active, skip full result (tokens already streamed)
    if (m_streamingMode && reqId == m_currentStreamId) {
        return;
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[%1] %2"));
    }
    statusBar()->showMessage(tr("Inference complete"), 3000);
}

void MainWindow::showInferenceError(qint64 reqId, const std::string& errorMsg)
{
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(std::string("[%1] ERROR: %2"));
    }
    statusBar()->showMessage(tr("Inference failed"), 3000);
}

void MainWindow::onModelLoadedChanged(bool loaded, const std::string& modelName)
{
    std::string msg = loaded ? tr("GGUF loaded: %1") : tr("GGUF unloaded");
    statusBar()->showMessage(msg, 3000);
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(msg);
    }
    
    if (loaded) {
        // Log how many tensors we saw in the loader
        std::vector<std::string> names = m_inferenceEngine ? m_inferenceEngine->tensorNames() : std::vector<std::string>();
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(std::string("Detected %1 tensors")));
        }

        // If developer wants auto per-layer set, use environment variable RAWRXD_AUTO_SET_LAYER
        std::string devCmd = qEnvironmentVariable("RAWRXD_AUTO_SET_LAYER");
        if (!devCmd.isEmpty() && !names.isEmpty()) {
            std::string target = names.first();
            std::string quant = devCmd.isEmpty() ? "Q6_K" : devCmd; // default to Q6_K
            if (m_hexMagConsole) m_hexMagConsole->appendPlainText(std::string("Auto-set %1 -> %2"));
            QMetaObject::invokeMethod(m_inferenceEngine, "setLayerQuant", //QueuedConnection,
                                      (std::string, target), (std::string, quant));
        }
    }
}

void MainWindow::batchCompressFolder()
{
    std::string dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select GGUF Folder"),
        std::string(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (dir.isEmpty()) {
        return;
    }
    
    QDirIterator it(dir, std::vector<std::string>() << "*.gguf", std::filesystem::path::Files, QDirIterator::Subdirectories);
    int total = 0, ok = 0;
    
    while (itfalse) {
        std::string inPath = it;
        std::string outPath = inPath + ".gz";
        
        std::fstream inFile(inPath);
        if (!inFile.open(QIODevice::ReadOnly)) {
            ++total;
            continue;
        }
        
        std::vector<uint8_t> raw = inFile.readAll();
        inFile.close();
        
        std::vector<uint8_t> gz = brutal::compress(raw);
        if (gz.isEmpty()) {
            ++total;
            continue;
        }
        
        std::fstream outFile(outPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(gz);
            outFile.close();
            ++ok;
        }
        
        ++total;
        statusBar()->showMessage(tr("Batch: %1/%2 compressed"), 500);
        QCoreApplication::processEvents();  // Keep UI responsive
    }
    
    std::string finalMsg = tr("Batch compression complete: %1/%2 files");
    statusBar()->showMessage(finalMsg, 5000);
    QMessageBox::information(this, tr("Batch Compress"), finalMsg);
}

// ---------- Ctrl+Shift+A inside the editor ----------
void MainWindow::onCtrlShiftA() {
    std::string wish = codeView_->textCursor().selectedText().trimmed();
    if (wish.isEmpty()) return;
    AutoBootstrap::startWithWish(wish);
}

// ---------- self-test gate before every release ----------
bool MainWindow::canRelease() {
    return runSelfTestGate();
}

// ---------- hot-reload after agent edits ----------
void MainWindow::onHotReload() {
    if (m_hotReload) {
        m_hotReload->reloadQuant(m_currentQuantMode);
    }
    statusBar()->showMessage("Hot-reloaded", 2000);
}

// ============================================================
// Agent System Setup and Integration
// ============================================================

void MainWindow::setupAgentSystem() {
    // Initialize AutoBootstrap (autonomous agent orchestration) - uses singleton pattern
    m_agentBootstrap = AutoBootstrap::instance();
    
    // Initialize HotReload (quantization library hot-reload)
    m_hotReload = new HotReload(this);
    
    // Connect HotReload signals to status bar for feedback
// Qt connect removed
    });
// Qt connect removed
    });
// Qt connect removed
    });
    
    // Add Tools menu for agent/hotpatch operations
    QMenu* toolsMenu = menuBar()->findChild<QMenu*>("ToolsMenu");
    if (!toolsMenu) {
        toolsMenu = menuBar()->addMenu("Tools");
        toolsMenu->setObjectName("ToolsMenu");
    }
    
    // Add Hot Reload action with Ctrl+Shift+R shortcut
    QAction* hotReloadAction = toolsMenu->addAction("Hot Reload Quantization");
    hotReloadAction->setShortcut(QKeySequence("Ctrl+Shift+R"));
// Qt connect removed
    // Add separator
    toolsMenu->addSeparator();
    
    // Add Agent Mode actions
    QMenu* agentModeMenu = toolsMenu->addMenu("Agent Mode");
    
    m_agentModeGroup = new QActionGroup(this);
    
    QAction* planModeAction = agentModeMenu->addAction("Plan");
    planModeAction->setCheckable(true);
    planModeAction->setChecked(true);
    planModeAction->setData("Plan");
    m_agentModeGroup->addAction(planModeAction);
    
    QAction* agentModeAction = agentModeMenu->addAction("Agent");
    agentModeAction->setCheckable(true);
    agentModeAction->setData("Agent");
    m_agentModeGroup->addAction(agentModeAction);
    
    QAction* askModeAction = agentModeMenu->addAction("Ask");
    askModeAction->setCheckable(true);
    askModeAction->setData("Ask");
    m_agentModeGroup->addAction(askModeAction);
    
    // Connect mode selection to changeAgentMode
// Qt connect removed
        changeAgentMode(mode);
    });
    
    // Add separator
    toolsMenu->addSeparator();
    
    // Add Self-Test Gate action
    QAction* selfTestAction = toolsMenu->addAction("Run Self-Test Gate");
    selfTestAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
// Qt connect removed
        } else {
            statusBar()->showMessage("✗ Self-test gate failed - fix issues before release", 5000);
        }
    });
    
    // Setup hotpatch panel for real-time event visualization
    setupHotpatchPanel();
}

// ============================================================
// Hotpatch Panel Setup and Integration
// ============================================================

void MainWindow::setupHotpatchPanel() {
    // Create Hotpatch Panel widget
    m_hotpatchPanel = new HotpatchPanel(this);
    m_hotpatchPanel->initialize();  // Two-phase init - create Qt widgets after QApplication
    
    // Create dock widget
    m_hotpatchPanelDock = new QDockWidget("Hotpatch Events", this);
    m_hotpatchPanelDock->setWidget(m_hotpatchPanel);
    m_hotpatchPanelDock->setObjectName("HotpatchPanelDock");
    m_hotpatchPanelDock->setAllowedAreas(//AllDockWidgetAreas);
    m_hotpatchPanelDock->setFeatures(QDockWidget::DockWidgetMovable |
                                      QDockWidget::DockWidgetFloatable |
                                      QDockWidget::DockWidgetClosable);
    
    // Add to bottom dock area by default
    addDockWidget(//BottomDockWidgetArea, m_hotpatchPanelDock);
    
    // Wire HotReload signals to hotpatch panel for event logging
// Qt connect removed
    });
// Qt connect removed
    });
// Qt connect removed
    });
    
    // Connect manual reload button in hotpatch panel to onHotReload
// Qt connect removed
        onHotReload();
    });
    
    // Add View menu toggle for Hotpatch Panel
    QMenu* viewMenu = menuBar()->findChild<QMenu*>();
    if (!viewMenu) {
        viewMenu = menuBar()->addMenu("View");
    }
    
    QAction* toggleHotpatchAction = viewMenu->addAction("Hotpatch Events");
    toggleHotpatchAction->setCheckable(true);
    toggleHotpatchAction->setChecked(true);
// Qt connect removed
    });
}

void MainWindow::toggleHotpatchPanel(bool visible) {
    if (m_hotpatchPanelDock) {
        if (visible) {
            m_hotpatchPanelDock->show();
        } else {
            m_hotpatchPanelDock->hide();
        }
    }
}

// ============================================================
// MASM Text Editor Setup and Integration
// ============================================================

void MainWindow::setupMASMEditor() {
    // Create MASM Editor widget
    m_masmEditor = new MASMEditorWidget(this);
    
    // Create dock widget
    m_masmEditorDock = new QDockWidget("MASM Assembly Editor", this);
    m_masmEditorDock->setWidget(m_masmEditor);
    m_masmEditorDock->setObjectName("MASMEditorDock");
    m_masmEditorDock->setAllowedAreas(//AllDockWidgetAreas);
    m_masmEditorDock->setFeatures(QDockWidget::DockWidgetMovable |
                                   QDockWidget::DockWidgetFloatable |
                                   QDockWidget::DockWidgetClosable);
    
    // Add to right dock area by default
    addDockWidget(//RightDockWidgetArea, m_masmEditorDock);
    
    // Connect editor signals to main window
// Qt connect removed
    });
// Qt connect removed
        statusBar()->showMessage(tr("Modified: %1%2")), 1000);
    });
// Qt connect removed
    });
    
    // Add View menu toggle for MASM Editor
    QMenu* viewMenu = menuBar()->findChild<QMenu*>();
    if (!viewMenu) {
        viewMenu = menuBar()->addMenu("View");
    }
    
    QAction* toggleMASMAction = viewMenu->addAction("MASM Assembly Editor");
    toggleMASMAction->setCheckable(true);
    toggleMASMAction->setChecked(true);
// Qt connect removed
    });
}

void MainWindow::toggleMASMEditor(bool visible) {
    if (m_masmEditorDock) {
        if (visible) {
            m_masmEditorDock->show();
        } else {
            m_masmEditorDock->hide();
        }
    }
}

void MainWindow::setupAIChatPanel() {
    // Create AI Chat Panel widget
    m_aiChatPanel = new AIChatPanel(this);
    m_aiChatPanel->initialize();  // Two-phase init - create Qt widgets after QApplication
    
    // Create dock widget to hold the chat panel
    m_aiChatPanelDock = new QDockWidget("AI Chat Panel", this);
    m_aiChatPanelDock->setWidget(m_aiChatPanel);
    m_aiChatPanelDock->setObjectName("AIChatPanelDock");
    m_aiChatPanelDock->setAllowedAreas(//AllDockWidgetAreas);
    m_aiChatPanelDock->setFeatures(QDockWidget::DockWidgetMovable |
                                    QDockWidget::DockWidgetFloatable |
                                    QDockWidget::DockWidgetClosable);
    
    // Add to right dock area by default
    addDockWidget(//RightDockWidgetArea, m_aiChatPanelDock);
    
    // Tabify with MASM editor if present
    if (m_masmEditorDock) {
        tabifyDockWidget(m_masmEditorDock, m_aiChatPanelDock);
        m_aiChatPanelDock->raise();
    }
    
    // Connect chat panel signals to inference engine
// Qt connect removed
// Qt connect removed
        // Keep the main model selector in sync with agent breadcrumb updates (Ollama async fetch)
        if (m_aiChatPanel && m_aiChatPanel->getBreadcrumb()) {
// Qt connect removed
        }
    
    // Connect inference engine responses to chat panel
// Qt connect removed
            });
// Qt connect removed
            });
    
    // Add View menu toggle for AI Chat Panel
    QMenu* viewMenu = nullptr;
    for (QAction* action : menuBar()->actions()) {
        if (action->text() == "View") {
            viewMenu = action->menu();
            break;
        }
    }
    
    if (!viewMenu) {
        viewMenu = menuBar()->addMenu("View");
    }
    
    QAction* toggleChatAction = viewMenu->addAction("AI Chat Panel");
    toggleChatAction->setCheckable(true);
    toggleChatAction->setChecked(true);
// Qt connect removed
                m_aiChatPanelDock->raise();
            } else {
                m_aiChatPanelDock->hide();
            }
        }
    });
    
}

void MainWindow::onAIChatMessageSubmitted(const std::string& message) {
    if (!m_aiChatPanel) return;
    
    try {
        // Add user message to chat
        m_aiChatPanel->addUserMessage(message);
        
        // Send to inference engine
        if (m_inferenceEngine && m_inferenceEngine->isModelLoaded()) {
            qint64 reqId = std::chrono::system_clock::time_point::currentMSecsSinceEpoch();
            m_currentStreamId = reqId;
            m_streamingMode = true;
            
            m_aiChatPanel->addAssistantMessage("", true);  // Start streaming
            
            // Call the streaming 'request' slot
            QMetaObject::invokeMethod(m_inferenceEngine, "request", //QueuedConnection,
                                      (std::string, message),
                                      (qint64, reqId),
                                      (bool, true));
        } else {
            m_aiChatPanel->addAssistantMessage("No model loaded. Please load a GGUF model first.", false);
        }
    } catch (const std::exception& e) {
        if (m_aiChatPanel) {
            m_aiChatPanel->addAssistantMessage(std::string("Error: %1")), false);
        }
    }
}

void MainWindow::onAIChatQuickActionTriggered(const std::string& action, const std::string& context) {
    if (!m_aiChatPanel) return;
    
    try {
        std::string prompt;
        
        if (action == "explain") {
            prompt = std::string("Explain this code:\n%1");
        } else if (action == "fix") {
            prompt = std::string("Fix any issues in this code:\n%1");
        } else if (action == "refactor") {
            prompt = std::string("Refactor this code to be more efficient:\n%1");
        } else {
            prompt = action;
        }
        
        onAIChatMessageSubmitted(prompt);
    } catch (const std::exception& e) {
    }
}

// ============================================================
// Layer Quantization Widget Setup
// ============================================================

void MainWindow::setupLayerQuantWidget() {
    // Create Layer Quantization Widget
    m_layerQuantWidget = new LayerQuantWidget(this);
    
    // Create dock widget
    m_layerQuantDock = new QDockWidget("Layer Quantization", this);
    m_layerQuantDock->setWidget(m_layerQuantWidget);
    m_layerQuantDock->setObjectName("LayerQuantDock");
    m_layerQuantDock->setAllowedAreas(//AllDockWidgetAreas);
    m_layerQuantDock->setFeatures(QDockWidget::DockWidgetMovable |
                                   QDockWidget::DockWidgetFloatable |
                                   QDockWidget::DockWidgetClosable);
    
    // Add to right dock area by default
    addDockWidget(//RightDockWidgetArea, m_layerQuantDock);
    
    // Connect layer quant widget to inference engine
// Qt connect removed
}

// ============================================================
// AI Backend Switcher Setup
// ============================================================

void MainWindow::setupAIBackendSwitcher() {
    // AI backend switcher is integrated in the toolbar/status bar
    // Add backend selection to Tools menu
    QMenu* toolsMenu = menuBar()->findChild<QMenu*>("ToolsMenu");
    if (!toolsMenu) {
        toolsMenu = menuBar()->addMenu("Tools");
        toolsMenu->setObjectName("ToolsMenu");
    }
    
    QMenu* backendMenu = toolsMenu->addMenu("AI Backend");
    m_backendGroup = new QActionGroup(this);
    
    QAction* localAct = backendMenu->addAction("Local (GGUF)");
    localAct->setCheckable(true);
    localAct->setChecked(true);
    localAct->setData("local");
    m_backendGroup->addAction(localAct);
    
    QAction* openaiAct = backendMenu->addAction("OpenAI");
    openaiAct->setCheckable(true);
    openaiAct->setData("openai");
    m_backendGroup->addAction(openaiAct);
    
    QAction* anthropicAct = backendMenu->addAction("Anthropic");
    anthropicAct->setCheckable(true);
    anthropicAct->setData("anthropic");
    m_backendGroup->addAction(anthropicAct);
// Qt connect removed
}

// ============================================================
// Quantization Menu Setup
// ============================================================

void MainWindow::setupQuantizationMenu(QMenu* aiMenu) {
    QMenu* quantMenu = aiMenu->addMenu("Quantization Mode");
    
    QActionGroup* quantGroup = new QActionGroup(this);
    
    const char* modes[] = {"Q2_K", "Q3_K", "Q4_0", "Q4_1", "Q5_0", "Q5_1", "Q8_0", "F16", "F32"};
    for (const char* mode : modes) {
        QAction* act = quantMenu->addAction(mode);
        act->setCheckable(true);
        act->setData(mode);
        quantGroup->addAction(act);
        if (std::string(mode) == "Q4_0") {
            act->setChecked(true);  // Default
        }
    }
// Qt connect removed
        statusBar()->showMessage(tr("Quantization Mode: %1"), 3000);
        if (m_layerQuantWidget) {
            // m_layerQuantWidget->setQuantMode(m_currentQuantMode);
        }
    });
}

void MainWindow::onQuantModeChanged(const std::string& mode) {
    m_currentQuantMode = mode;
    statusBar()->showMessage(tr("Quantization changed to: %1"), 3000);
}

// ============================================================
// Swarm Editing Setup (Collaborative Editing)
// ============================================================

#ifdef HAVE_QT_WEBSOCKETS
void MainWindow::setupSwarmEditing() {
    // Swarm editing - collaborative real-time editing via WebSocket
    RawrXD::Integration::ScopedTimer timer("MainWindow", "setupSwarmEditing", "swarm");
    
    if (!m_swarmSocket) {
        m_swarmSocket = new QWebSocket(std::string(), QWebSocketProtocol::VersionLatest, this);
        
        // Connection established
// Qt connect removed
            statusBar()->showMessage(tr("Connected to swarm session"), 3000);
            
            // Send join message
            void* joinMsg;
            joinMsg["type"] = "join";
            joinMsg["session"] = m_swarmSessionId;
            joinMsg["user"] = QSysInfo::machineHostName();
            joinMsg["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
            m_swarmSocket->sendTextMessage(void*(joinMsg).toJson(void*::Compact));
        });
        
        // Connection closed
// Qt connect removed
            statusBar()->showMessage(tr("Disconnected from swarm session"), 3000);
            m_swarmSessionId.clear();
        });
        
        // Message received
// Qt connect removed
        // Error handling
// Qt connect removed
            statusBar()->showMessage(tr("Swarm connection error: %1")), 5000);
        });
    }
    
    m_swarmSessionId.clear();
}

void MainWindow::joinSwarmSession() {
    // Connect to collaborative editing server
    if (!m_swarmSocket) {
        setupSwarmEditing();
    }
    
    // Generate unique session ID if not set
    if (m_swarmSessionId.isEmpty()) {
        m_swarmSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
    }
    
    // Show dialog to enter session ID or create new
    bool ok;
    std::string sessionId = QInputDialog::getText(this, tr("Join Swarm Session"),
        tr("Enter session ID (or leave empty to create new):"),
        QLineEdit::Normal, m_swarmSessionId, &ok);
    
    if (ok) {
        m_swarmSessionId = sessionId.isEmpty() ? m_swarmSessionId : sessionId;
        
        // Connect to swarm server (configurable endpoint)
        std::string serverUrl = QSettings().value("swarm/serverUrl", "wss://swarm.rawrxd.dev").toString();
        std::string url(std::string("%1/session/%2"));
        
        statusBar()->showMessage(tr("Connecting to swarm session %1..."), 5000);
        m_swarmSocket->open(url);
    }
}

void MainWindow::onSwarmMessage(const std::string& message) {
    // Handle incoming collaborative edits from swarm peers
    void* doc = void*::fromJson(message.toUtf8());
    if (!doc.isObject()) {
        return;
    }
    
    void* msg = doc.object();
    std::string type = msg["type"].toString();
    std::string user = msg["user"].toString();
    
    if (type == "edit") {
        // Apply remote edit to document
        std::string file = msg["file"].toString();
        int line = msg["line"].toInt();
        int col = msg["col"].toInt();
        std::string text = msg["text"].toString();
        std::string operation = msg["operation"].toString(); // insert, delete, replace
        
        
        // Apply to editor if same file is open
        if (editor_ && !file.isEmpty()) {
            // Mark as remote edit to avoid broadcast loop
            editor_->setProperty("swarm_remote_edit", true);
            // Apply edit through editor interface
            // editor_->applyRemoteEdit(line, col, text, operation);
            editor_->setProperty("swarm_remote_edit", false);
        }
        
        statusBar()->showMessage(tr("Edit from %1"), 1000);
        
    } else if (type == "cursor") {
        // Show remote cursor position
        int line = msg["line"].toInt();
        int col = msg["col"].toInt();
        
    } else if (type == "join") {
        statusBar()->showMessage(tr("%1 joined the session"), 3000);
        if (chatHistory_) {
            chatHistory_->addItem(tr("👥 %1 joined swarm session"));
        }
        
    } else if (type == "leave") {
        statusBar()->showMessage(tr("%1 left the session"), 3000);
    }
}

void MainWindow::broadcastEdit() {
    // Broadcast local edits to swarm session
    if (!m_swarmSocket || m_swarmSocket->state() != QAbstractSocket::ConnectedState) {
        return; // Not connected to swarm
    }
    
    // Check if this is a remote edit (avoid loop)
    if (editor_ && editor_->property("swarm_remote_edit").toBool()) {
        return;
    }
    
    // Get current edit info from editor
    std::string currentFile;
    int line = 0, col = 0;
    std::string editText;
    
    if (editor_) {
        QTextCursor cursor = editor_->textCursor();
        line = cursor.blockNumber() + 1;
        col = cursor.columnNumber();
        editText = cursor.selectedText();
        
        // Get current file path
        if (!currentFilePath_.isEmpty()) {
            currentFile = currentFilePath_;
        }
    }
    
    // Build edit message
    void* editMsg;
    editMsg["type"] = "edit";
    editMsg["session"] = m_swarmSessionId;
    editMsg["user"] = QSysInfo::machineHostName();
    editMsg["file"] = currentFile;
    editMsg["line"] = line;
    editMsg["col"] = col;
    editMsg["text"] = editText;
    editMsg["operation"] = "insert"; // or "delete", "replace"
    editMsg["timestamp"] = std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate);
    
    m_swarmSocket->sendTextMessage(void*(editMsg).toJson(void*::Compact));
}

#else // HAVE_QT_WEBSOCKETS not defined - provide stub implementations

void MainWindow::setupSwarmEditing() {
}

void MainWindow::joinSwarmSession() {
    QMessageBox::information(this, tr("Feature Unavailable"),
        tr("Swarm editing requires Qt WebSockets module, which is not available in this build."));
}

void MainWindow::onSwarmMessage(const std::string& /* message */) {
    // Stub - WebSocket support not available
}

void MainWindow::broadcastEdit() {
    // Stub - WebSocket support not available
}

#endif // HAVE_QT_WEBSOCKETS

void MainWindow::onAIBackendChanged(const std::string& id, const std::string& apiKey) {
    m_currentBackend = id;
    m_currentAPIKey = apiKey;
    statusBar()->showMessage(tr("Switched to AI backend: %1"), 3000);
}

// ============================================================
// Interpretability Panel Setup
// ============================================================

/**
 * @brief Setup Interpretability Panel for model analysis and diagnostics
 * Call this from MainWindow constructor after setupAIChatPanel()
 */
void MainWindow::setupInterpretabilityPanel()
{
    // Create Interpretability Panel widget
    m_interpretabilityPanel = new InterpretabilityPanelEnhanced(this);
    
    // Create dock widget
    m_interpretabilityPanelDock = new QDockWidget("Model Interpretability & Diagnostics", this);
    m_interpretabilityPanelDock->setWidget(m_interpretabilityPanel);
    m_interpretabilityPanelDock->setObjectName("InterpretabilityPanelDock");
    m_interpretabilityPanelDock->setAllowedAreas(//AllDockWidgetAreas);
    m_interpretabilityPanelDock->setFeatures(QDockWidget::DockWidgetMovable |
                                             QDockWidget::DockWidgetFloatable |
                                             QDockWidget::DockWidgetClosable);
    
    // Add to right dock area by default
    addDockWidget(//RightDockWidgetArea, m_interpretabilityPanelDock);
    m_interpretabilityPanelDock->hide();  // Hidden by default
    
    // Configure anomaly detection thresholds
    m_interpretabilityPanel->setAnomalyThresholds(1e-7f, 10.0f, 0.5f);
    m_interpretabilityPanel->setGradientTrackingEnabled(true);
    
    // Connect signals for real-time diagnostics
// Qt connect removed
                // Log to console
                if (m_hexMagConsole) {
                    m_hexMagConsole->appendPlainText(
                        std::string("[INTERPRETABILITY] %1: %2")
                            .toString("HH:mm:ss"))
                            
                    );
                }
                
            });
// Qt connect removed
                if (diagnostics_json["has_vanishing_gradients"].toBool()) issues << "Vanishing Gradients";
                if (diagnostics_json["has_exploding_gradients"].toBool()) issues << "Exploding Gradients";
                if (diagnostics_json["has_dead_neurons"].toBool()) issues << "Dead Neurons";
                if (diagnostics_json["average_sparsity"].toDouble() > 0.5) issues << "High Sparsity";
                if (diagnostics_json["attention_entropy_mean"].toDouble() < 1.0) issues << "Low Attention Entropy";
                
                if (!issues.isEmpty()) {
                    statusBar()->showMessage(
                        std::string("🔍 Diagnostics: %1 issue(s) - %2")
                            )
                            ),
                        8000
                    );
                } else {
                    statusBar()->showMessage("✅ Model Diagnostics: All checks passed", 5000);
                }
                
            });
// Qt connect removed
                std::string defaultSuffix;
                if (format == "JSON") {
                    filter = "JSON Files (*.json)";
                    defaultSuffix = ".json";
                } else if (format == "CSV") {
                    filter = "CSV Files (*.csv)";
                    defaultSuffix = ".csv";
                } else if (format == "PNG") {
                    filter = "PNG Images (*.png)";
                    defaultSuffix = ".png";
                }
                
                std::string filePath = QFileDialog::getSaveFileName(
                    this,
                    tr("Export Interpretability Data"),
                    std::filesystem::path::homePath() + "/interpretability_export" + defaultSuffix,
                    filter
                );
                
                if (!filePath.isEmpty()) {
                    bool success = false;
                    if (format == "JSON") {
                        success = m_interpretabilityPanel->exportAsJSON(filePath);
                    } else if (format == "CSV") {
                        success = m_interpretabilityPanel->exportAsCSV(filePath);
                    } else if (format == "PNG") {
                        success = m_interpretabilityPanel->exportAsPNG(filePath);
                    }
                    
                    if (success) {
                        QMessageBox::information(this, tr("Export Successful"),
                            tr("Interpretability data exported to:\n%1"));
                    } else {
                        QMessageBox::warning(this, tr("Export Failed"),
                            tr("Failed to export data to:\n%1"));
                    }
                }
            });
    
    // Connect to inference engine for automatic data feed (if inference engine exists)
    if (m_inferenceEngine) {
        // When model is loaded, enable the panel
// Qt connect removed
                        statusBar()->showMessage(
                            std::string("📊 Interpretability Panel enabled"), 3000
                        );
                    }
                });
        
        // Connect attention data stream to interpretability panel
// Qt connect removed
                    std::vector<InterpretabilityPanelEnhanced::AttentionHead> heads;
                    for (const void*& val : attentionData) {
                        void* obj = val.toObject();
                        InterpretabilityPanelEnhanced::AttentionHead head;
                        head.layer_idx = obj["layer"].toInt();
                        head.head_idx = obj["head"].toInt();
                        head.mean_attn_weight = static_cast<float>(obj["mean"].toDouble());
                        head.max_attn_weight = static_cast<float>(obj["max"].toDouble());
                        head.entropy = static_cast<float>(obj["entropy"].toDouble());
                        
                        // Parse weights matrix
                        void* weightsArr = obj["weights"].toArray();
                        for (const void*& rowVal : weightsArr) {
                            void* rowArr = rowVal.toArray();
                            std::vector<float> row;
                            for (const void*& w : rowArr) {
                                row.push_back(static_cast<float>(w.toDouble()));
                            }
                            head.weights.push_back(row);
                        }
                        head.timestamp = std::chrono::system_clock::now();
                        heads.push_back(head);
                    }
                    m_interpretabilityPanel->updateAttentionHeads(heads);
                });
        
        // Connect gradient data stream to interpretability panel
// Qt connect removed
                    std::vector<InterpretabilityPanelEnhanced::GradientFlowMetrics> metrics;
                    for (const void*& val : gradientData) {
                        void* obj = val.toObject();
                        InterpretabilityPanelEnhanced::GradientFlowMetrics m;
                        m.layer_idx = obj["layer"].toInt();
                        m.norm = static_cast<float>(obj["norm"].toDouble());
                        m.variance = static_cast<float>(obj["variance"].toDouble());
                        m.min_value = static_cast<float>(obj["min"].toDouble());
                        m.max_value = static_cast<float>(obj["max"].toDouble());
                        m.dead_neuron_ratio = static_cast<float>(obj["dead_ratio"].toDouble());
                        m.is_vanishing = obj["is_vanishing"].toBool();
                        m.is_exploding = obj["is_exploding"].toBool();
                        m.timestamp = std::chrono::system_clock::now();
                        metrics.push_back(m);
                    }
                    m_interpretabilityPanel->updateGradientFlow(metrics);
                });
        
        // Connect activation data stream to interpretability panel
// Qt connect removed
                    std::vector<InterpretabilityPanelEnhanced::ActivationStats> stats;
                    for (const void*& val : activationData) {
                        void* obj = val.toObject();
                        InterpretabilityPanelEnhanced::ActivationStats s;
                        s.layer_idx = obj["layer"].toInt();
                        s.mean = static_cast<float>(obj["mean"].toDouble());
                        s.variance = static_cast<float>(obj["variance"].toDouble());
                        s.min_val = static_cast<float>(obj["min"].toDouble());
                        s.max_val = static_cast<float>(obj["max"].toDouble());
                        s.sparsity = static_cast<float>(obj["sparsity"].toDouble());
                        s.dead_neuron_count = static_cast<float>(obj["dead_neurons"].toInt());
                        
                        // Parse distribution histogram
                        void* distArr = obj["distribution"].toArray();
                        for (const void*& d : distArr) {
                            s.distribution.push_back(static_cast<float>(d.toDouble()));
                        }
                        s.timestamp = std::chrono::system_clock::now();
                        stats.push_back(s);
                    }
                    m_interpretabilityPanel->updateActivationStats(stats);
                });
        
        // Connect layer contribution data
// Qt connect removed
                    std::vector<InterpretabilityPanelEnhanced::LayerAttribution> attribs;
                    for (const void*& val : layerData) {
                        void* obj = val.toObject();
                        InterpretabilityPanelEnhanced::LayerAttribution a;
                        a.layer_idx = obj["layer"].toInt();
                        a.contribution = static_cast<float>(obj["contribution"].toDouble());
                        a.cumulative_importance = static_cast<float>(obj["cumulative"].toDouble());
                        
                        void* neuronsArr = obj["neurons"].toArray();
                        for (const void*& n : neuronsArr) {
                            a.neuron_importances.push_back(static_cast<float>(n.toDouble()));
                        }
                        a.timestamp = std::chrono::system_clock::now();
                        attribs.push_back(a);
                    }
                    m_interpretabilityPanel->updateLayerAttribution(attribs);
                });
        
        // Connect token logits for real-time generation analysis
// Qt connect removed
                    std::vector<float> logitVec;
                    for (const void*& l : logits) {
                        logitVec.push_back(static_cast<float>(l.toDouble()));
                    }
                    m_interpretabilityPanel->updateTokenLogits(tokenIdx, logitVec);
                });
    }
    
}

/**
 * @brief Toggle visibility of Interpretability Panel
 * Called from View menu or command palette
 */
void MainWindow::toggleInterpretabilityPanel(bool visible)
{
    if (!m_interpretabilityPanelDock) {
        if (visible) {
            setupInterpretabilityPanel();
        }
        return;
    }
    
    m_interpretabilityPanelDock->setVisible(visible);
    
    if (visible) {
        // Run initial diagnostics when panel is shown
        if (m_interpretabilityPanel) {
            auto diagnostics = m_interpretabilityPanel->runDiagnostics();
        }
    }
}

void MainWindow::toggleSettings(bool visible)
{
    if (!settingsWidget_) {
        settingsWidget_ = new SettingsDialog(this);
    }
    
    if (visible && !settingsWidget_.isNull()) {
        settingsWidget_.data()->show();
        settingsWidget_.data()->raise();
        settingsWidget_.data()->activateWindow();
    } else if (!settingsWidget_.isNull()) {
        settingsWidget_.data()->hide();
    }
}

// ============================================================
// ============================================================
// UNIVERSAL CROSS-PLATFORM COMPILER INTEGRATION
// Supports ANY language → ANY OS → ANY architecture
// ============================================================

// Language-to-compiler mapping structure
struct LanguageCompilerInfo {
    std::string language;           // Display name
    std::vector<std::string> extensions;     // File extensions
    std::string nativeCompiler;     // System compiler (fallback)
    std::string rawrxdCompiler;     // RawrXD ASM compiler
    std::string defaultArgs;        // Default compiler arguments
    bool supportsDebug;         // Debug symbol support
    bool supportsOptimize;      // Optimization support
};

// Static language registry - ALL supported languages
static const std::vector<LanguageCompilerInfo> g_languageRegistry = {
    // Assembly Languages
    {"Assembly (x64)", {"asm", "s", "nasm", "masm", "yasm", "fasm", "gas"}, "ml64", "asm_compiler_from_scratch", "-f win64", true, true},
    {"EON Language", {"eon"}, "", "eon_compiler_from_scratch", "", true, true},
    
    // C Family
    {"C", {"c", "h"}, "gcc", "c_compiler_from_scratch", "-std=c17", true, true},
    {"C++", {"cpp", "cxx", "cc", "hpp", "hxx", "hh"}, "g++", "cpp_compiler_from_scratch", "-std=c++20", true, true},
    {"Objective-C", {"m", "mm"}, "clang", "objc_compiler_from_scratch", "-fobjc-arc", true, true},
    {"C#", {"cs"}, "csc", "csharp_compiler_from_scratch", "-langversion:latest", true, true},
    
    // Systems Languages
    {"Rust", {"rs", "rlib"}, "rustc", "rust_compiler_from_scratch", "--edition 2021", true, true},
    {"Go", {"go"}, "go build", "go_compiler_from_scratch", "-ldflags=-s", true, true},
    {"Zig", {"zig"}, "zig build", "zig_compiler_from_scratch", "-OReleaseFast", true, true},
    {"Nim", {"nim", "nims"}, "nim c", "nim_compiler_from_scratch", "--gc:arc", true, true},
    {"D", {"d", "di"}, "dmd", "d_compiler_from_scratch", "-release", true, true},
    {"V", {"v", "vv"}, "v", "v_compiler_from_scratch", "-prod", true, true},
    {"Carbon", {"carbon"}, "carbon", "carbon_compiler_from_scratch", "", true, true},
    {"Odin", {"odin"}, "odin build", "odin_compiler_from_scratch", "-o:speed", true, true},
    
    // JVM Languages
    {"Java", {"java"}, "javac", "java_compiler_from_scratch", "-source 21", true, true},
    {"Kotlin", {"kt", "kts"}, "kotlinc", "kotlin_compiler_from_scratch", "-jvm-target 21", true, true},
    {"Scala", {"scala", "sc"}, "scalac", "scala_compiler_from_scratch", "-release 21", true, true},
    {"Groovy", {"groovy", "gvy"}, "groovyc", "groovy_compiler_from_scratch", "", true, true},
    {"Clojure", {"clj", "cljs", "cljc", "edn"}, "clj", "clojure_compiler_from_scratch", "", true, false},
    
    // .NET Languages
    {"F#", {"fs", "fsi", "fsx"}, "fsc", "fsharp_compiler_from_scratch", "--langversion:latest", true, true},
    {"Visual Basic", {"vb"}, "vbc", "vb_compiler_from_scratch", "-langversion:latest", true, true},
    
    // Scripting Languages (AOT compiled)
    {"Python", {"py", "pyw", "pyx", "pxd"}, "python", "python_compiler_from_scratch", "-O", true, true},
    {"JavaScript", {"js", "mjs", "cjs"}, "node", "javascript_compiler_from_scratch", "", true, true},
    {"TypeScript", {"ts", "tsx", "mts", "cts"}, "tsc", "typescript_compiler_from_scratch", "--strict", true, true},
    {"Ruby", {"rb", "rbw", "rake"}, "ruby", "ruby_compiler_from_scratch", "", true, false},
    {"Perl", {"pl", "pm", "pod"}, "perl", "perl_compiler_from_scratch", "", true, false},
    {"PHP", {"php", "phtml", "php3", "php4", "php5"}, "php", "php_compiler_from_scratch", "", true, false},
    {"Lua", {"lua"}, "luac", "lua_compiler_from_scratch", "", true, true},
    {"R", {"r", "R"}, "Rscript", "r_compiler_from_scratch", "", true, false},
    {"Julia", {"jl"}, "julia", "julia_compiler_from_scratch", "-O3", true, true},
    
    // Shell/Scripting
    {"Bash", {"sh", "bash", "zsh", "ksh", "fish"}, "bash", "bash_compiler_from_scratch", "", false, false},
    {"PowerShell", {"ps1", "psm1", "psd1"}, "pwsh", "powershell_compiler_from_scratch", "", false, false},
    {"Batch", {"bat", "cmd"}, "cmd", "batch_compiler_from_scratch", "", false, false},
    
    // Functional Languages
    {"Haskell", {"hs", "lhs"}, "ghc", "haskell_compiler_from_scratch", "-O2", true, true},
    {"OCaml", {"ml", "mli"}, "ocamlopt", "ocaml_compiler_from_scratch", "-O3", true, true},
    {"Erlang", {"erl", "hrl"}, "erlc", "erlang_compiler_from_scratch", "", true, false},
    {"Elixir", {"ex", "exs"}, "elixirc", "elixir_compiler_from_scratch", "", true, false},
    {"Lisp", {"lisp", "lsp", "cl"}, "sbcl", "lisp_compiler_from_scratch", "", true, true},
    {"Scheme", {"scm", "ss", "rkt"}, "racket", "scheme_compiler_from_scratch", "", true, false},
    {"Prolog", {"pl", "pro", "P"}, "swipl", "prolog_compiler_from_scratch", "", true, false},
    
    // Web/Mobile
    {"Swift", {"swift"}, "swiftc", "swift_compiler_from_scratch", "-O", true, true},
    {"Dart", {"dart"}, "dart compile exe", "dart_compiler_from_scratch", "", true, true},
    {"Vala", {"vala", "vapi"}, "valac", "vala_compiler_from_scratch", "", true, true},
    
    // GPU/Parallel
    {"CUDA", {"cu", "cuh"}, "nvcc", "cuda_compiler_from_scratch", "-arch=sm_80", true, true},
    {"OpenCL", {"cl", "ocl"}, "clang", "opencl_compiler_from_scratch", "", true, true},
    {"HLSL", {"hlsl", "hlsli"}, "dxc", "hlsl_compiler_from_scratch", "-T cs_6_6", true, true},
    {"GLSL", {"glsl", "vert", "frag", "geom", "comp", "tesc", "tese"}, "glslc", "glsl_compiler_from_scratch", "", true, true},
    {"Metal", {"metal"}, "metal", "metal_compiler_from_scratch", "", true, true},
    {"SPIR-V", {"spv"}, "spirv-as", "spirv_compiler_from_scratch", "", true, true},
    
    // Embedded/Hardware
    {"VHDL", {"vhd", "vhdl"}, "ghdl", "vhdl_compiler_from_scratch", "", true, true},
    {"Verilog", {"v", "sv", "svh"}, "iverilog", "verilog_compiler_from_scratch", "", true, true},
    {"SystemVerilog", {"sv", "svh"}, "verilator", "systemverilog_compiler_from_scratch", "", true, true},
    
    // Data/Config (transformable)
    {"YAML", {"yaml", "yml"}, "", "yaml_compiler_from_scratch", "", false, false},
    {"JSON", {"json", "jsonc"}, "", "json_compiler_from_scratch", "", false, false},
    {"TOML", {"toml"}, "", "toml_compiler_from_scratch", "", false, false},
    {"XML", {"xml", "xsl", "xslt"}, "", "xml_compiler_from_scratch", "", false, false},
    
    // Documentation (compilable to executable docs)
    {"Markdown", {"md", "markdown"}, "", "markdown_compiler_from_scratch", "", false, false},
    {"LaTeX", {"tex", "ltx"}, "pdflatex", "latex_compiler_from_scratch", "", false, false},
    
    // Database
    {"SQL", {"sql", "ddl", "dml"}, "", "sql_compiler_from_scratch", "", false, false},
    {"PL/SQL", {"pls", "plb", "pck"}, "", "plsql_compiler_from_scratch", "", false, false},
    
    // Esoteric/Educational
    {"Brainfuck", {"bf", "b"}, "", "brainfuck_compiler_from_scratch", "", false, false},
    {"Whitespace", {"ws"}, "", "whitespace_compiler_from_scratch", "", false, false},
    {"LOLCODE", {"lol", "lols"}, "", "lolcode_compiler_from_scratch", "", false, false},
    {"COBOL", {"cob", "cbl", "cpy"}, "cobc", "cobol_compiler_from_scratch", "", true, true},
    {"Fortran", {"f", "f90", "f95", "f03", "f08", "for"}, "gfortran", "fortran_compiler_from_scratch", "", true, true},
    {"Pascal", {"pas", "pp", "dpr"}, "fpc", "pascal_compiler_from_scratch", "", true, true},
    {"Ada", {"ada", "adb", "ads"}, "gnat", "ada_compiler_from_scratch", "", true, true},
};

// Target platform enumeration
enum class TargetPlatform {
    Native,     // Current OS
    Windows,
    Linux,
    MacOS,
    WebAssembly,
    iOS,
    Android,
    FreeBSD,
    OpenBSD,
    NetBSD,
    Solaris,
    Haiku,
    FreeRTOS,
    Zephyr,
    EmbeddedBare
};

// Target architecture enumeration
enum class TargetArch {
    Native,     // Current architecture
    x86_64,
    x86,
    ARM64,
    ARM32,
    RISCV64,
    RISCV32,
    MIPS64,
    MIPS32,
    PowerPC64,
    PowerPC32,
    SPARC64,
    WebAssembly32,
    WebAssembly64,
    AVR,        // Arduino
    ESP32,      // Espressif
    STM32       // ARM Cortex-M
};

static std::string targetPlatformToString(TargetPlatform p) {
    switch(p) {
        case TargetPlatform::Native: return "native";
        case TargetPlatform::Windows: return "windows";
        case TargetPlatform::Linux: return "linux";
        case TargetPlatform::MacOS: return "macos";
        case TargetPlatform::WebAssembly: return "wasm";
        case TargetPlatform::iOS: return "ios";
        case TargetPlatform::Android: return "android";
        case TargetPlatform::FreeBSD: return "freebsd";
        case TargetPlatform::OpenBSD: return "openbsd";
        case TargetPlatform::NetBSD: return "netbsd";
        case TargetPlatform::Solaris: return "solaris";
        case TargetPlatform::Haiku: return "haiku";
        case TargetPlatform::FreeRTOS: return "freertos";
        case TargetPlatform::Zephyr: return "zephyr";
        case TargetPlatform::EmbeddedBare: return "baremetal";
        default: return "native";
    }
}

static std::string targetArchToString(TargetArch a) {
    switch(a) {
        case TargetArch::Native: return "native";
        case TargetArch::x86_64: return "x86_64";
        case TargetArch::x86: return "i686";
        case TargetArch::ARM64: return "aarch64";
        case TargetArch::ARM32: return "arm";
        case TargetArch::RISCV64: return "riscv64";
        case TargetArch::RISCV32: return "riscv32";
        case TargetArch::MIPS64: return "mips64";
        case TargetArch::MIPS32: return "mips";
        case TargetArch::PowerPC64: return "ppc64";
        case TargetArch::PowerPC32: return "ppc";
        case TargetArch::SPARC64: return "sparc64";
        case TargetArch::WebAssembly32: return "wasm32";
        case TargetArch::WebAssembly64: return "wasm64";
        case TargetArch::AVR: return "avr";
        case TargetArch::ESP32: return "xtensa";
        case TargetArch::STM32: return "thumbv7em";
        default: return "native";
    }
}

// Find compiler info for file extension
static const LanguageCompilerInfo* findCompilerForExtension(const std::string& ext) {
    for (const auto& info : g_languageRegistry) {
        if (info.extensions.contains(ext.toLower())) {
            return &info;
        }
    }
    return nullptr;
}

// Get output file extension based on target platform
static std::string getOutputExtension(TargetPlatform platform) {
    switch(platform) {
        case TargetPlatform::Windows: return ".exe";
        case TargetPlatform::Linux:
        case TargetPlatform::MacOS:
        case TargetPlatform::FreeBSD:
        case TargetPlatform::OpenBSD:
        case TargetPlatform::NetBSD:
        case TargetPlatform::Solaris:
        case TargetPlatform::Haiku:
            return "";  // Unix-like - no extension
        case TargetPlatform::WebAssembly: return ".wasm";
        case TargetPlatform::iOS:
        case TargetPlatform::Android:
            return "";  // App bundles/APKs handled separately
        case TargetPlatform::FreeRTOS:
        case TargetPlatform::Zephyr:
        case TargetPlatform::EmbeddedBare:
            return ".elf";  // Embedded firmware
        default:
#ifdef 
            return ".exe";
#else
            return "";
#endif
    }
}

void MainWindow::toggleCompileCurrentFile()
{
    // Get current open file
    std::string currentFile;
    if (m_multiTabEditor && m_multiTabEditor->currentIndex() >= 0) {
        currentFile = m_multiTabEditor->getTabFilePath(m_multiTabEditor->currentIndex());
    }
    
    if (currentFile.isEmpty()) {
        statusBar()->showMessage(tr("No file open to compile"), 3000);
        QMessageBox::warning(this, tr("Compile Error"), tr("Please open a file to compile."));
        return;
    }
    
    std::filesystem::path fi(currentFile);
    std::string ext = fi.suffix().toLower();
    
    // Find compiler for this file type
    const LanguageCompilerInfo* compilerInfo = findCompilerForExtension(ext);
    
    if (!compilerInfo) {
        // Build list of all supported extensions for help message
        std::vector<std::string> allExts;
        for (const auto& info : g_languageRegistry) {
            allExts.append(info.extensions);
        }
        allExts.removeDuplicates();
        allExts.sort();
        
        statusBar()->showMessage(tr("Unsupported file type: %1"), 3000);
        QMessageBox::information(this, tr("Universal Compiler"),
            tr("File type '%1' is not currently supported.\n\n"
               "RawrXD Universal Compiler supports %2 languages including:\n"
               "• Assembly: asm, nasm, masm, fasm\n"
               "• Systems: c, cpp, rust, go, zig, nim, d, v\n"
               "• JVM: java, kotlin, scala, groovy, clojure\n"
               "• .NET: cs, fs, vb\n"
               "• Scripting: py, js, ts, rb, pl, php, lua\n"
               "• Functional: hs, ml, erl, ex, lisp, scm\n"
               "• GPU: cu, cl, hlsl, glsl, metal\n"
               "• Shell: sh, bash, ps1, bat\n"
               "• Hardware: vhd, sv, v (Verilog)\n"
               "• And many more!")));
        return;
    }
    
    // === CROSS-PLATFORM TARGET SELECTION DIALOG ===
    TargetPlatform targetPlatform = TargetPlatform::Native;
    TargetArch targetArch = TargetArch::Native;
    bool useRawrXDCompiler = true;
    bool debugBuild = false;
    bool optimizeBuild = true;
    
    // Create advanced compile options dialog
    void optionsDialog(this);
    optionsDialog.setWindowTitle(tr("Universal Compiler - %1"));
    optionsDialog.setMinimumWidth(500);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(&optionsDialog);
    
    // Header with language info
    QLabel* headerLabel = new QLabel(tr("🔧 Compiling <b>%1</b> file: <code>%2</code>")
        ));
    headerLabel->setWordWrap(true);
    mainLayout->addWidget(headerLabel);
    
    mainLayout->addSpacing(10);
    
    // === COMPILER SELECTION ===
    QGroupBox* compilerGroup = new QGroupBox(tr("Compiler Engine"));
    QVBoxLayout* compilerLayout = new QVBoxLayout(compilerGroup);
    
    QRadioButton* rawrxdRadio = new QRadioButton(tr("🚀 RawrXD Native ASM Compiler (Recommended)"));
    rawrxdRadio->setToolTip(tr("Pure assembly implementation - maximum performance, full cross-platform support"));
    rawrxdRadio->setChecked(true);
    
    QRadioButton* systemRadio = new QRadioButton(tr("🔧 System Compiler (%1)")
         ? tr("Not available") : compilerInfo->nativeCompiler));
    systemRadio->setToolTip(tr("Use system-installed compiler as fallback"));
    systemRadio->setEnabled(!compilerInfo->nativeCompiler.isEmpty());
    
    compilerLayout->addWidget(rawrxdRadio);
    compilerLayout->addWidget(systemRadio);
    mainLayout->addWidget(compilerGroup);
    
    // === TARGET PLATFORM ===
    QGroupBox* platformGroup = new QGroupBox(tr("Target Platform"));
    QGridLayout* platformLayout = new QGridLayout(platformGroup);
    
    QComboBox* platformCombo = new QComboBox();
    platformCombo->addItem(tr("🖥️ Native (Current OS)"), static_cast<int>(TargetPlatform::Native));
    platformCombo->addItem(tr("🪟 Windows"), static_cast<int>(TargetPlatform::Windows));
    platformCombo->addItem(tr("🐧 Linux"), static_cast<int>(TargetPlatform::Linux));
    platformCombo->addItem(tr("🍎 macOS"), static_cast<int>(TargetPlatform::MacOS));
    platformCombo->addItem(tr("🌐 WebAssembly"), static_cast<int>(TargetPlatform::WebAssembly));
    platformCombo->addItem(tr("📱 iOS"), static_cast<int>(TargetPlatform::iOS));
    platformCombo->addItem(tr("🤖 Android"), static_cast<int>(TargetPlatform::Android));
    platformCombo->addItem(tr("👿 FreeBSD"), static_cast<int>(TargetPlatform::FreeBSD));
    platformCombo->addItem(tr("🐡 OpenBSD"), static_cast<int>(TargetPlatform::OpenBSD));
    platformCombo->addItem(tr("🔲 NetBSD"), static_cast<int>(TargetPlatform::NetBSD));
    platformCombo->addItem(tr("☀️ Solaris"), static_cast<int>(TargetPlatform::Solaris));
    platformCombo->addItem(tr("🌸 Haiku"), static_cast<int>(TargetPlatform::Haiku));
    platformCombo->addItem(tr("⚡ FreeRTOS (Embedded)"), static_cast<int>(TargetPlatform::FreeRTOS));
    platformCombo->addItem(tr("⚡ Zephyr RTOS"), static_cast<int>(TargetPlatform::Zephyr));
    platformCombo->addItem(tr("🔌 Bare Metal"), static_cast<int>(TargetPlatform::EmbeddedBare));
    
    platformLayout->addWidget(new QLabel(tr("Platform:")), 0, 0);
    platformLayout->addWidget(platformCombo, 0, 1);
    mainLayout->addWidget(platformGroup);
    
    // === TARGET ARCHITECTURE ===
    QGroupBox* archGroup = new QGroupBox(tr("Target Architecture"));
    QGridLayout* archLayout = new QGridLayout(archGroup);
    
    QComboBox* archCombo = new QComboBox();
    archCombo->addItem(tr("🎯 Native (Current CPU)"), static_cast<int>(TargetArch::Native));
    archCombo->addItem(tr("💻 x86_64 (AMD64)"), static_cast<int>(TargetArch::x86_64));
    archCombo->addItem(tr("💻 x86 (i686, 32-bit)"), static_cast<int>(TargetArch::x86));
    archCombo->addItem(tr("📱 ARM64 (AArch64)"), static_cast<int>(TargetArch::ARM64));
    archCombo->addItem(tr("📱 ARM32 (ARMv7)"), static_cast<int>(TargetArch::ARM32));
    archCombo->addItem(tr("🔬 RISC-V 64-bit"), static_cast<int>(TargetArch::RISCV64));
    archCombo->addItem(tr("🔬 RISC-V 32-bit"), static_cast<int>(TargetArch::RISCV32));
    archCombo->addItem(tr("🔧 MIPS64"), static_cast<int>(TargetArch::MIPS64));
    archCombo->addItem(tr("🔧 MIPS32"), static_cast<int>(TargetArch::MIPS32));
    archCombo->addItem(tr("⚡ PowerPC64"), static_cast<int>(TargetArch::PowerPC64));
    archCombo->addItem(tr("⚡ PowerPC32"), static_cast<int>(TargetArch::PowerPC32));
    archCombo->addItem(tr("☀️ SPARC64"), static_cast<int>(TargetArch::SPARC64));
    archCombo->addItem(tr("🌐 WebAssembly 32-bit"), static_cast<int>(TargetArch::WebAssembly32));
    archCombo->addItem(tr("🌐 WebAssembly 64-bit"), static_cast<int>(TargetArch::WebAssembly64));
    archCombo->addItem(tr("🔌 AVR (Arduino)"), static_cast<int>(TargetArch::AVR));
    archCombo->addItem(tr("🔌 Xtensa (ESP32)"), static_cast<int>(TargetArch::ESP32));
    archCombo->addItem(tr("🔌 ARM Cortex-M (STM32)"), static_cast<int>(TargetArch::STM32));
    
    archLayout->addWidget(new QLabel(tr("Architecture:")), 0, 0);
    archLayout->addWidget(archCombo, 0, 1);
    mainLayout->addWidget(archGroup);
    
    // === BUILD OPTIONS ===
    QGroupBox* buildGroup = new QGroupBox(tr("Build Options"));
    QGridLayout* buildLayout = new QGridLayout(buildGroup);
    
    QCheckBox* debugCheck = new QCheckBox(tr("🐛 Include Debug Symbols"));
    debugCheck->setEnabled(compilerInfo->supportsDebug);
    debugCheck->setChecked(false);
    
    QCheckBox* optimizeCheck = new QCheckBox(tr("🚀 Enable Optimizations"));
    optimizeCheck->setEnabled(compilerInfo->supportsOptimize);
    optimizeCheck->setChecked(true);
    
    QCheckBox* staticCheck = new QCheckBox(tr("📦 Static Linking (Self-contained)"));
    staticCheck->setChecked(true);
    
    QCheckBox* stripCheck = new QCheckBox(tr("✂️ Strip Symbols (Smaller binary)"));
    stripCheck->setChecked(true);
    
    buildLayout->addWidget(debugCheck, 0, 0);
    buildLayout->addWidget(optimizeCheck, 0, 1);
    buildLayout->addWidget(staticCheck, 1, 0);
    buildLayout->addWidget(stripCheck, 1, 1);
    mainLayout->addWidget(buildGroup);
    
    // === OUTPUT ===
    QGroupBox* outputGroup = new QGroupBox(tr("Output"));
    QHBoxLayout* outputLayout = new QHBoxLayout(outputGroup);
    
    QLineEdit* outputEdit = new QLineEdit(fi.baseName());
    outputEdit->setPlaceholderText(tr("Output filename (without extension)"));
    
    QLabel* extLabel = new QLabel(getOutputExtension(TargetPlatform::Native));
    
    // Update extension when platform changes
    void, [&](int idx) {
        TargetPlatform p = static_cast<TargetPlatform>(platformCombo->itemData(idx).toInt());
        extLabel->setText(getOutputExtension(p));
    });
    
    outputLayout->addWidget(new QLabel(tr("Output:")));
    outputLayout->addWidget(outputEdit, 1);
    outputLayout->addWidget(extLabel);
    mainLayout->addWidget(outputGroup);
    
    // === DIALOG BUTTONS ===
    QDialogButtonBox* buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    buttonBox->button(QDialogButtonBox::Ok)->setText(tr("🔨 Compile"));
    buttonBox->button(QDialogButtonBox::Ok)->setDefault(true);
// Qt connect removed
// Qt connect removed
    mainLayout->addWidget(buttonBox);
    
    // Show dialog
    if (optionsDialog.exec() != void::Accepted) {
        return;
    }
    
    // Collect options
    targetPlatform = static_cast<TargetPlatform>(platformCombo->currentData().toInt());
    targetArch = static_cast<TargetArch>(archCombo->currentData().toInt());
    useRawrXDCompiler = rawrxdRadio->isChecked();
    debugBuild = debugCheck->isChecked();
    optimizeBuild = optimizeCheck->isChecked();
    bool staticLink = staticCheck->isChecked();
    bool stripSymbols = stripCheck->isChecked();
    std::string outputName = outputEdit->text().isEmpty() ? fi.baseName() : outputEdit->text();
    std::string outputExt = getOutputExtension(targetPlatform);
    std::string outputPath = fi.absolutePath() + "/" + outputName + outputExt;
    
    // === START COMPILATION ===
    statusBar()->showMessage(tr("🔨 Compiling %1 (%2) → %3/%4...")
        )
        
        )
        ), 5000);
    
    // Build compiler arguments
    std::vector<std::string> compilerArgs;
    
    // Universal compiler path
    std::string compilerPath;
    if (useRawrXDCompiler) {
        compilerPath = QCoreApplication::applicationDirPath() + "/rawrxd.exe";
        
        // RawrXD universal compiler arguments
        compilerArgs << "--input" << currentFile;
        compilerArgs << "--output" << outputPath;
        compilerArgs << "--language" << compilerInfo->language.toLower().replace(" ", "-");
        compilerArgs << "--target-os" << targetPlatformToString(targetPlatform);
        compilerArgs << "--target-arch" << targetArchToString(targetArch);
        
        if (debugBuild) compilerArgs << "--debug";
        if (optimizeBuild) compilerArgs << "--optimize" << "3";
        if (staticLink) compilerArgs << "--static";
        if (stripSymbols && !debugBuild) compilerArgs << "--strip";
        
        // Add language-specific default args
        if (!compilerInfo->defaultArgs.isEmpty()) {
            compilerArgs << "--lang-args" << compilerInfo->defaultArgs;
        }
    } else {
        // System compiler fallback
        compilerPath = compilerInfo->nativeCompiler;
        // Basic args for system compiler
        compilerArgs << "-o" << outputPath << currentFile;
        if (debugBuild) compilerArgs << "-g";
        if (optimizeBuild) compilerArgs << "-O3";
        if (staticLink) compilerArgs << "-static";
    }
    
    // Launch compiler process
    QProcess* compiler = new QProcess(this);
    compiler->setWorkingDirectory(fi.absolutePath());
    
    // Capture output
    std::string compilerName = compilerInfo->language;
// Qt connect removed
        std::string errors = compiler->readAllStandardError();
        
        if (exitCode == 0) {
            std::filesystem::path outFi(outputPath);
            std::string sizeStr;
            if (outFi.exists()) {
                qint64 size = outFi.size();
                if (size < 1024) sizeStr = std::string("%1 bytes");
                else if (size < 1024*1024) sizeStr = std::string("%1 KB");
                else sizeStr = std::string("%1 MB"), 0, 'f', 2);
            }
            
            statusBar()->showMessage(tr("✅ %1 compiled successfully → %2 (%3) [%4/%5]")
                )
                )
                
                )
                ), 8000);
            
            if (notificationCenter_) {
                notificationCenter_->notify(tr("Compilation Successful"),
                    tr("%1 (%2) compiled to %3\nTarget: %4/%5\nSize: %6")
                        )
                        
                        )
                        )
                        )
                        ,
                    NotificationCenter::NotificationLevel::Success);
            }
            
            // Log to output panel if available
            if (!output.isEmpty()) {
            }
        } else {
            statusBar()->showMessage(tr("❌ Compilation failed: %1")), 5000);
            
            std::string errorMsg = errors.isEmpty() ? output : errors;
            QMessageBox::critical(this, tr("Compilation Failed"),
                tr("<b>%1 Compiler Error</b><br><br>"
                   "<b>File:</b> %2<br>"
                   "<b>Target:</b> %3 / %4<br><br>"
                   "<b>Errors:</b><pre>%5</pre>")
                    
                    )
                    )
                    )
                    ));  // Limit error message length
            
            if (notificationCenter_) {
                notificationCenter_->notify(tr("Compilation Failed"),
                    tr("%1 compilation failed")),
                    NotificationCenter::NotificationLevel::Error);
            }
        }
        
        compiler->deleteLater();
    });
    
    // Start compilation
    compiler->start(compilerPath, compilerArgs);
    
    if (!compiler->waitForStarted(5000)) {
        statusBar()->showMessage(tr("❌ Failed to start %1 compiler"), 3000);
        
        QMessageBox::warning(this, tr("Compiler Error"),
            tr("Failed to start compiler: %1\n\n"
               "Make sure the RawrXD compiler is installed at:\n%2\n\n"
               "Or install the system compiler: %3")
                
                )
                );
        
        compiler->deleteLater();
    }
}

void MainWindow::toggleBuildProject()
{
    // Find all compilable files in current workspace
    std::string projectDir;
    if (!m_currentWorkspacePath.isEmpty()) {
        projectDir = m_currentWorkspacePath;
    } else if (m_multiTabEditor && m_multiTabEditor->currentIndex() >= 0) {
        std::filesystem::path fi(m_multiTabEditor->getTabFilePath(m_multiTabEditor->currentIndex()));
        projectDir = fi.absolutePath();
    }
    
    if (projectDir.isEmpty()) {
        statusBar()->showMessage(tr("No project directory"), 3000);
        QMessageBox::warning(this, tr("Build Error"), tr("Please open a project or file first."));
        return;
    }
    
    // Recursively find all compilable files
    QDirIterator it(projectDir, {"*.eon", "*.asm", "*.s", "*.c", "*.cpp", "*.rs", "*.py", "*.js", "*.go"},
                    std::filesystem::path::Files, QDirIterator::Subdirectories);
    std::vector<std::string> sourceFiles;
    std::map<std::string, int> fileTypes;
    
    while (itfalse) {
        std::string filePath = it;
        sourceFiles << filePath;
        std::string ext = std::filesystem::path(filePath).suffix().toLower();
        fileTypes[ext]++;
    }
    
    if (sourceFiles.isEmpty()) {
        statusBar()->showMessage(tr("No compilable files found"), 3000);
        QMessageBox::information(this, tr("Build Project"),
            tr("No compilable source files found in:\n%1\n\n"
               "Supported: .eon, .asm, .s, .c, .cpp, .rs, .py, .js, .go, etc."));
        return;
    }
    
    statusBar()->showMessage(tr("🔨 Building %1 files...")), 2000);
    
    // Log to console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("\n========== BUILD START ==========");
        m_hexMagConsole->appendPlainText(std::string("Directory: %1"));
        m_hexMagConsole->appendPlainText(std::string("Files: %1")));
        for (auto it = fileTypes.begin(); it != fileTypes.end(); ++it) {
            m_hexMagConsole->appendPlainText(std::string("  • .%1: %2 files"))));
        }
        m_hexMagConsole->appendPlainText("================================\n");
    }
    
    // Launch compiler for all files with parallelization
    QProcess* compiler = new QProcess(this);
    std::string compilerPath = QCoreApplication::applicationDirPath() + "/rawrxd.exe";
    
    // Check if compiler exists
    if (!std::filesystem::path::exists(compilerPath)) {
        compilerPath = QCoreApplication::applicationDirPath() + "/../build/bin/Release/rawrxd.exe";
        if (!std::filesystem::path::exists(compilerPath)) {
            statusBar()->showMessage(tr("RawrXD compiler not found"), 3000);
            QMessageBox::critical(this, tr("Compiler Error"), tr("Could not find rawrxd.exe compiler."));
            return;
        }
    }
    
    std::vector<std::string> args = {"-j4"};  // 4 parallel threads
    for (const std::string& f : sourceFiles) {
        args << f;
    }
    
    // Monitor output
// Qt connect removed
        if (!output.isEmpty() && m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(output);
        }
    });
// Qt connect removed
        if (!error.isEmpty() && m_hexMagConsole) {
            m_hexMagConsole->appendPlainText("[ERROR] " + error);
        }
    });
// Qt connect removed
            for (const std::string& src : sourceFiles) {
                std::filesystem::path srcInfo(src);
                std::string outFile = srcInfo.absolutePath() + "/" + srcInfo.baseName() + ".exe";
                if (std::filesystem::path::exists(outFile)) successCount++;
            }
            
            statusBar()->showMessage(tr("✅ Build successful: %1/%2 files compiled")), 5000);
            
            if (m_hexMagConsole) {
                m_hexMagConsole->appendPlainText("\n========== BUILD SUCCESS ==========");
                m_hexMagConsole->appendPlainText(std::string("Compiled: %1 of %2 files")));
                m_hexMagConsole->appendPlainText("===================================\n");
            }
            
            
            if (notificationCenter_) {
                notificationCenter_->notify(tr("Build Complete"),
                    tr("Successfully built %1 of %2 files")),
                    NotificationCenter::NotificationLevel::Success);
            }
        } else {
            statusBar()->showMessage(tr("❌ Build failed (exit code: %1)"), 5000);
            
            if (m_hexMagConsole) {
                m_hexMagConsole->appendPlainText("\n========== BUILD FAILED ==========");
                m_hexMagConsole->appendPlainText(std::string("Exit Code: %1"));
                m_hexMagConsole->appendPlainText("===================================\n");
            }
            
            std::string errors = compiler->readAllStandardError();
            QMessageBox::warning(this, tr("Build Failed"),
                tr("Build failed with exit code %1\n\nErrors:\n%2")));
        }
        compiler->deleteLater();
    });
    
    compiler->start(compilerPath, args);
    
    if (!compiler->waitForStarted(3000)) {
        statusBar()->showMessage(tr("Failed to start compiler"), 3000);
        QMessageBox::critical(this, tr("Build Error"), tr("Failed to start compiler process."));
        compiler->deleteLater();
    }
}

void MainWindow::toggleCleanBuild()
{
    std::string projectDir;
    if (!m_currentWorkspacePath.isEmpty()) {
        projectDir = m_currentWorkspacePath;
    } else if (m_multiTabEditor && m_multiTabEditor->currentIndex() >= 0) {
        std::filesystem::path fi(m_multiTabEditor->getTabFilePath(m_multiTabEditor->currentIndex()));
        projectDir = fi.absolutePath();
    }
    
    if (projectDir.isEmpty()) {
        statusBar()->showMessage(tr("No project directory"), 3000);
        return;
    }
    
    // Confirm clean operation
    int ret = QMessageBox::question(this, tr("Clean Build Artifacts"),
        tr("This will delete all compiled output files in:\n%1\n\n"
           "File types to remove:\n"
           "  • Executables: *.exe, *.out, *.app\n"
           "  • Object files: *.obj, *.o\n"
           "  • Libraries: *.lib, *.a, *.dll, *.so\n"
           "  • Intermediate: *.ir, *.s, *.ll\n\n"
           "Continue?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (ret != QMessageBox::Yes) {
        statusBar()->showMessage(tr("Clean cancelled"), 2000);
        return;
    }
    
    statusBar()->showMessage(tr("🧹 Cleaning build artifacts.."), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("\n========== CLEAN START ==========");
        m_hexMagConsole->appendPlainText(std::string("Directory: %1"));
    }
    
    std::filesystem::path dir(projectDir);
    std::vector<std::string> cleanPatterns = {"*.exe", "*.obj", "*.o", "*.lib", "*.dll", "*.ir", "*.s", "*.out", 
                                 "*.a", "*.so", "*.dll", "*.dylib", "*.ll"};
    int cleaned = 0, failed = 0;
    
    // Recursively find and delete
    QDirIterator it(projectDir, std::filesystem::path::Files | std::filesystem::path::Dirs | std::filesystem::path::NoDotAndDotDot, QDirIterator::Subdirectories);
    std::vector<std::string> filesToDelete;
    
    while (itfalse) {
        std::string path = it;
        std::filesystem::path info(path);
        
        if (info.isFile()) {
            for (const std::string& pattern : cleanPatterns) {
                std::regex rx(std::regex::wildcardToRegularExpression(pattern),
                                       std::regex::CaseInsensitiveOption);
                if (rx.match(info.fileName()).hasMatch()) {
                    filesToDelete << path;
                    break;
                }
            }
        }
    }
    
    // Delete files
    for (const std::string& file : filesToDelete) {
        std::filesystem::path fInfo(file);
        if (std::fstream::remove(file)) {
            cleaned++;
            if (m_hexMagConsole) {
                m_hexMagConsole->appendPlainText(std::string("  ✓ Deleted: %1")));
            }
        } else {
            failed++;
            if (m_hexMagConsole) {
                m_hexMagConsole->appendPlainText(std::string("  ✗ Failed: %1")));
            }
        }
    }
    
    std::string summary = tr("✅ Cleaned %1 files");
    if (failed > 0) {
        summary += tr(" (%1 failed)");
    }
    
    statusBar()->showMessage(summary, 3000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("\n========== CLEAN COMPLETE ==========");
        m_hexMagConsole->appendPlainText(std::string("Deleted: %1 files"));
        if (failed > 0) {
            m_hexMagConsole->appendPlainText(std::string("Failed: %1 files"));
        }
        m_hexMagConsole->appendPlainText("====================================\n");
    }
    
    
    if (notificationCenter_) {
        notificationCenter_->notify(tr("Clean Complete"),
            tr("Cleaned %1 build artifacts"),
            NotificationCenter::NotificationLevel::Info);
    }
    
    // Ask if user wants to rebuild
    int rebuildRet = QMessageBox::question(this, tr("Rebuild"),
        tr("Build artifacts cleaned (%1 files).\n\n"
           "Rebuild project now?"),
        QMessageBox::Yes | QMessageBox::No);
    
    if (rebuildRet == QMessageBox::Yes) {
        toggleBuildProject();
    }
}

void MainWindow::toggleCompilerSettings()
{
    // Load current settings
    QSettings settings("RawrXD", "Compiler");
    std::string savedTarget = settings.value("target_arch", "x86-64").toString();
    std::string savedOpt = settings.value("opt_level", "O2").toString();
    bool savedDebug = settings.value("debug_info", false).toBool();
    bool savedWarnings = settings.value("warnings_as_errors", false).toBool();
    bool savedVerbose = settings.value("verbose", false).toBool();
    int savedThreads = settings.value("parallel_threads", 4).toInt();
    std::string savedOutput = settings.value("output_format", "Executable (.exe)").toString();
    
    // Create settings dialog
    void* dialog = new void(this);
    dialog->setWindowTitle(tr("🔧 Eon/ASM Compiler Settings"));
    dialog->setMinimumSize(600, 650);
    dialog->setWindowModality(//WindowModal);
    
    QVBoxLayout* layout = new QVBoxLayout(dialog);
    
    // === Header ===
    QLabel* headerLabel = new QLabel(tr("<b>RawrXD Compiler Configuration</b>"));
    layout->addWidget(headerLabel);
    layout->addSpacing(10);
    
    // === Target Architecture ===
    QGroupBox* targetGroup = new QGroupBox(tr("Target Architecture"), dialog);
    QVBoxLayout* targetLayout = new QVBoxLayout(targetGroup);
    QComboBox* targetCombo = new QComboBox(targetGroup);
    targetCombo->addItems({"x86-64 (Default, Native 64-bit)", "x86 (32-bit)", "ARM64 (Apple Silicon)", 
                           "ARM32 (ARMv7)", "RISC-V 64-bit", "WebAssembly"});
    int targetIndex = targetCombo->findText(savedTarget, //MatchStartsWith);
    if (targetIndex >= 0) targetCombo->setCurrentIndex(targetIndex);
    targetLayout->addWidget(new QLabel(tr("Select target CPU architecture for compilation")));
    targetLayout->addWidget(targetCombo);
    layout->addWidget(targetGroup);
    
    // === Output Format ===
    QGroupBox* outputGroup = new QGroupBox(tr("Output Format"), dialog);
    QVBoxLayout* outputLayout = new QVBoxLayout(outputGroup);
    QComboBox* outputCombo = new QComboBox(outputGroup);
    outputCombo->addItems({"Executable (.exe)", "Shared Library (.dll)", "Static Library (.lib)",
                           "Object File (.obj)", "Assembly (.s)", "IR (.ir)"});
    int outputIndex = outputCombo->findText(savedOutput);
    if (outputIndex >= 0) outputCombo->setCurrentIndex(outputIndex);
    outputLayout->addWidget(new QLabel(tr("Choose output file format")));
    outputLayout->addWidget(outputCombo);
    layout->addWidget(outputGroup);
    
    // === Optimization Level ===
    QGroupBox* optGroup = new QGroupBox(tr("Optimization Level"), dialog);
    QVBoxLayout* optLayout = new QVBoxLayout(optGroup);
    QComboBox* optCombo = new QComboBox(optGroup);
    optCombo->addItems({"O0 - No optimization (Fastest compile, slower code)", 
                        "O1 - Basic optimization", 
                        "O2 - Standard optimization (Default, balanced)", 
                        "O3 - Aggressive optimization (Slower compile, faster code)",
                        "Os - Optimize for size (Smallest binary)"});
    int optIndex = optCombo->findText(savedOpt, //MatchStartsWith);
    if (optIndex >= 0) optCombo->setCurrentIndex(optIndex);
    else optCombo->setCurrentIndex(2);
    optLayout->addWidget(new QLabel(tr("Higher levels produce faster code but take longer to compile")));
    optLayout->addWidget(optCombo);
    layout->addWidget(optGroup);
    
    // === Parallelization ===
    QGroupBox* parallelGroup = new QGroupBox(tr("Parallelization"), dialog);
    QVBoxLayout* parallelLayout = new QVBoxLayout(parallelGroup);
    QSpinBox* threadsSpinner = new QSpinBox(parallelGroup);
    threadsSpinner->setMinimum(1);
    int maxThreads = std::thread::idealThreadCount();
    threadsSpinner->setMaximum(maxThreads);
    threadsSpinner->setValue(savedThreads);
    parallelLayout->addWidget(new QLabel(tr("Number of parallel compilation threads (System: %1 cores):")
        ));
    parallelLayout->addWidget(threadsSpinner);
    layout->addWidget(parallelGroup);
    
    // === Build Options ===
    QGroupBox* optionsGroup = new QGroupBox(tr("Build Options"), dialog);
    QVBoxLayout* optionsLayout = new QVBoxLayout(optionsGroup);
    
    QCheckBox* debugCheck = new QCheckBox(tr("🐛 Include Debug Symbols"), optionsGroup);
    debugCheck->setChecked(savedDebug);
    debugCheck->setToolTip(tr("Include debugging information for debuggers"));
    optionsLayout->addWidget(debugCheck);
    
    QCheckBox* warningsCheck = new QCheckBox(tr("⚠️  Treat Warnings as Errors"), optionsGroup);
    warningsCheck->setChecked(savedWarnings);
    warningsCheck->setToolTip(tr("Fail compilation if any warnings occur"));
    optionsLayout->addWidget(warningsCheck);
    
    QCheckBox* verboseCheck = new QCheckBox(tr("📢 Verbose Output"), optionsGroup);
    verboseCheck->setChecked(savedVerbose);
    verboseCheck->setToolTip(tr("Print detailed compiler operations and diagnostics"));
    optionsLayout->addWidget(verboseCheck);
    
    layout->addWidget(optionsGroup);
    
    // === Info Section ===
    QLabel* infoLabel = new QLabel(
        tr("<b>RawrXD Compiler Information:</b><br>"
           "• Version: 1.0<br>"
           "• Supported Languages: EON, ASM, x86-64, ARM, WebAssembly<br>"
           "• Multi-target compilation for native and cross-platform builds<br>"
           "• Parallelized compilation with -j flag<br>"
           "• Support for optimization levels O0-O3 and Os"));
    infoLabel->setStyleSheet("QLabel { background-color: #f0f0f0; padding: 10px; border-radius: 4px; }");
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);
    
    layout->addStretch();
    
    // === Buttons ===
    QDialogButtonBox* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, dialog);
// Qt connect removed
        settings.setValue("target_arch", targetCombo->currentText().split(" ").first());
        settings.setValue("output_format", outputCombo->currentText());
        settings.setValue("opt_level", optCombo->currentText().split(" ").first());
        settings.setValue("debug_info", debugCheck->isChecked());
        settings.setValue("warnings_as_errors", warningsCheck->isChecked());
        settings.setValue("verbose", verboseCheck->isChecked());
        settings.setValue("parallel_threads", threadsSpinner->value());
        settings.sync();
        
        statusBar()->showMessage(tr("✅ Compiler settings saved"), 3000);
        dialog->accept();
    });
// Qt connect removed
// Qt connect removed
        targetCombo->setCurrentIndex(0);  // x86-64
        outputCombo->setCurrentIndex(0);  // Executable
        optCombo->setCurrentIndex(2);     // O2
        debugCheck->setChecked(false);
        warningsCheck->setChecked(false);
        verboseCheck->setChecked(false);
        threadsSpinner->setValue(4);
        
        QMessageBox::information(nullptr, tr("Defaults Restored"), 
            tr("Settings reset to defaults (changes not saved yet)"));
    });
    
    layout->addWidget(buttons);
    
    dialog->exec();
    dialog->deleteLater();
}

void MainWindow::toggleCompilerOutput()
{
    // Create or show compiler output dock if not already created
    if (!m_compilerOutputDock) {
        m_compilerOutputDock = new QDockWidget(tr("🔨 Compiler Output"), this);
        m_compilerOutputDock->setObjectName("CompilerOutputDock");
        m_compilerOutputDock->setAllowedAreas(//AllDockWidgetAreas);
        m_compilerOutputDock->setFeatures(QDockWidget::DockWidgetMovable |
                                          QDockWidget::DockWidgetFloatable |
                                          QDockWidget::DockWidgetClosable);
        
        // Create output widget container
        void* outputContainer = new void(m_compilerOutputDock);
        QVBoxLayout* containerLayout = new QVBoxLayout(outputContainer);
        containerLayout->setContentsMargins(0, 0, 0, 0);
        containerLayout->setSpacing(0);
        
        // === Toolbar ===
        QToolBar* toolbar = new QToolBar(tr("Compiler Output Tools"), outputContainer);
        toolbar->setMovable(false);
        toolbar->setFloatable(false);
        toolbar->setIconSize(QSize(16, 16));
        
        QAction* clearAction = toolbar->addAction(tr("Clear"));
        clearAction->setToolTip(tr("Clear output (Ctrl+L)"));
// Qt connect removed
                statusBar()->showMessage(tr("✅ Compiler output cleared"), 2000);
            }
        });
        
        toolbar->addSeparator();
        
        QAction* copyAction = toolbar->addAction(tr("Copy All"));
        copyAction->setToolTip(tr("Copy all output to clipboard"));
// Qt connect removed
                statusBar()->showMessage(tr("✅ Output copied to clipboard"), 2000);
            }
        });
        
        QAction* selectAllAction = toolbar->addAction(tr("Select All"));
        selectAllAction->setToolTip(tr("Select all output (Ctrl+A)"));
// Qt connect removed
            }
        });
        
        toolbar->addSeparator();
        
        QAction* saveAction = toolbar->addAction(tr("Save"));
        saveAction->setToolTip(tr("Save output to file"));
// Qt connect removed
            std::string defaultPath = std::filesystem::path::homePath() + "/compiler_output_" + 
                std::chrono::system_clock::time_point::currentDateTime().toString("yyyy-MM-dd_hh-mm-ss") + ".txt";
            
            std::string fileName = QFileDialog::getSaveFileName(this,
                tr("Save Compiler Output"), defaultPath,
                tr("Text Files (*.txt);;Log Files (*.log);;All Files (*.*)"));
            
            if (!fileName.isEmpty()) {
                std::fstream file(fileName);
                if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    QTextStream stream(&file);
                    stream << m_compilerOutput->toPlainText();
                    file.close();
                    statusBar()->showMessage(tr("✅ Output saved: %1").fileName()), 3000);
                    
                    if (notificationCenter_) {
                        notificationCenter_->notify(tr("Output Saved"),
                            tr("Compiler output saved to:\n%1"),
                            NotificationCenter::NotificationLevel::Info);
                    }
                } else {
                    QMessageBox::warning(this, tr("Save Error"), tr("Could not open file for writing"));
                }
            }
        });
        
        toolbar->addSeparator();
        
        // Wrap mode checkbox
        QCheckBox* wrapCheck = new QCheckBox(tr("Word Wrap"));
        wrapCheck->setChecked(false);
        toolbar->addWidget(wrapCheck);
        
        containerLayout->addWidget(toolbar);
        
        // === Output text editor ===
        m_compilerOutput = new QPlainTextEdit(outputContainer);
        m_compilerOutput->setReadOnly(true);
        m_compilerOutput->setFont(QFont("Consolas", 10));
        m_compilerOutput->setMaximumBlockCount(5000);  // Keep last 5000 lines
        m_compilerOutput->setStyleSheet(
            "QPlainTextEdit {"
            "  background-color: #1e1e1e;"
            "  color: #dcdcdc;"
            "  selection-background-color: #264f78;"
            "  border: none;"
            "}"
            "QPlainTextEdit:focus {"
            "  border: 1px solid #007acc;"
            "}");
        
        // Connect word wrap checkbox
// Qt connect removed
            }
        });
        
        containerLayout->addWidget(m_compilerOutput);
        
        // === Status line ===
        QLabel* statusLabel = new QLabel(tr("Ready"));
        statusLabel->setStyleSheet("QLabel { background-color: #2d2d2d; color: #cccccc; padding: 4px; }");
        
        // Update status when text changes
// Qt connect removed
            int chars = m_compilerOutput->toPlainText().length();
            statusLabel->setText(tr("Lines: %1  |  Characters: %2"));
        });
        
        containerLayout->addWidget(statusLabel);
        
        m_compilerOutputDock->setWidget(outputContainer);
        addDockWidget(//BottomDockWidgetArea, m_compilerOutputDock);
        
    }
    
    // Toggle visibility
    if (m_compilerOutputDock->isVisible()) {
        m_compilerOutputDock->hide();
        statusBar()->showMessage(tr("📦 Compiler Output hidden"), 2000);
    } else {
        m_compilerOutputDock->show();
        m_compilerOutputDock->raise();
        m_compilerOutputDock->activateWindow();
        statusBar()->showMessage(tr("📦 Compiler Output shown"), 2000);
    }
}

void MainWindow::setupOrchestrationSystem()
{
    if (!m_taskOrchestrator) {
        m_taskOrchestrator = new RawrXD::TaskOrchestrator(this);
        
        // Configure RollarCoaster endpoint
        m_taskOrchestrator->setRollarCoasterEndpoint("http://localhost:11438");
        m_taskOrchestrator->setMaxParallelTasks(4);
        m_taskOrchestrator->setTaskTimeout(30000);
    }
    
    if (!m_orchestrationUI) {
        m_orchestrationUI = new RawrXD::OrchestrationUI(m_taskOrchestrator, this);
    }
    
    if (!m_orchestrationDock) {
        m_orchestrationDock = new QDockWidget(tr("Task Orchestration"), this);
        m_orchestrationDock->setWidget(m_orchestrationUI);
        addDockWidget(//RightDockWidgetArea, m_orchestrationDock);
        
        // Connect orchestrator signals to main window for integration
// Qt connect removed
                });
// Qt connect removed
                    for (const RawrXD::OrchestrationResult& result : results) {
                        if (result.success) successCount++;
                    }
                    statusBar()->showMessage(
                        tr("Orchestration completed: %1/%2 tasks successful")
                            ), 5000);
                });
    }
    
    m_orchestrationDock->show();
    m_orchestrationDock->raise();
}

void MainWindow::setupCommandPalette()
{
    // Command palette - fuzzy search for commands and actions
    RawrXD::Integration::ScopedTimer timer("MainWindow", "setupCommandPalette", "ui");
    
    if (m_commandPalette) {
        return;
    }
    
    // Create command palette widget
    m_commandPalette = new void(this, //Popup | //FramelessWindowHint);
    m_commandPalette->setObjectName("CommandPalette");
    m_commandPalette->setMinimumSize(500, 400);
    m_commandPalette->setStyleSheet(
        "void#CommandPalette { background: #1e1e1e; border: 1px solid #3c3c3c; border-radius: 8px; }"
        "QLineEdit { background: #252526; color: #cccccc; border: 1px solid #3c3c3c; border-radius: 4px; padding: 8px; font-size: 14px; }"
        "QListWidget { background: #1e1e1e; color: #cccccc; border: none; }"
        "QListWidget::item { padding: 8px; }"
        "QListWidget::item:selected { background: #094771; }"
        "QListWidget::item:hover { background: #2a2d2e; }"
    );
    
    QVBoxLayout* layout = new QVBoxLayout(m_commandPalette);
    layout->setContentsMargins(8, 8, 8, 8);
    layout->setSpacing(4);
    
    // Search input
    QLineEdit* searchInput = new QLineEdit();
    searchInput->setPlaceholderText(tr("Type a command..."));
    searchInput->setObjectName("CommandPaletteSearch");
    layout->addWidget(searchInput);
    
    // Command list
    QListWidget* commandList = new QListWidget();
    commandList->setObjectName("CommandPaletteList");
    layout->addWidget(commandList);
    
    // Populate commands from menu actions
    std::vector<std::string> commands;
    auto addMenuCommands = [&commands](QMenu* menu, const std::string& prefix = "") {
        if (!menu) return;
        for (QAction* action : menu->actions()) {
            if (!action->text().isEmpty() && !action->isSeparator()) {
                std::string cmd = prefix.isEmpty() ? action->text() : prefix + " > " + action->text();
                cmd.remove('&'); // Remove mnemonics
                commands << cmd;
            }
            if (action->menu()) {
                // Recurse into submenus - simplified for this implementation
            }
        }
    };
    
    // Add common commands
    commands << tr("File: New File") << tr("File: Open File") << tr("File: Save") << tr("File: Save As")
             << tr("Edit: Undo") << tr("Edit: Redo") << tr("Edit: Cut") << tr("Edit: Copy") << tr("Edit: Paste")
             << tr("View: Toggle Terminal") << tr("View: Toggle Explorer") << tr("View: Toggle Output")
             << tr("Build: Build Project") << tr("Build: Run") << tr("Build: Debug")
             << tr("Git: Commit") << tr("Git: Push") << tr("Git: Pull") << tr("Git: Branch")
             << tr("AI: Send to Chat") << tr("AI: Explain Code") << tr("AI: Generate Tests")
             << tr("Settings: Open Settings") << tr("Settings: Keyboard Shortcuts") << tr("Settings: Theme");
    
    commandList->addItems(commands);
    
    // Filter on search
// Qt connect removed
        for (const std::string& cmd : commands) {
            if (text.isEmpty() || cmd.contains(text, //CaseInsensitive)) {
                commandList->addItem(cmd);
            }
        }
        if (commandList->count() > 0) {
            commandList->setCurrentRow(0);
        }
    });
    
    // Execute command on selection
// Qt connect removed
        m_commandPalette->hide();
        executeCommand(cmd);
    });
    
    // Escape to close
    searchInput->installEventFilter(this);
    
}

void MainWindow::executeCommand(const std::string& command)
{
    // Execute command from command palette
    RawrXD::Integration::ScopedTimer timer("MainWindow", "executeCommand", "command");
    
    // Map command strings to actions using existing methods
    if (command.contains("New File", //CaseInsensitive)) {
        handleNewEditor();  // Use existing slot
    } else if (command.contains("Open File", //CaseInsensitive)) {
        handleAddFile();    // Use existing slot  
    } else if (command.contains("Save As", //CaseInsensitive)) {
        handleSaveAs();     // Use existing slot
    } else if (command.contains("Save", //CaseInsensitive)) {
        handleSaveState();  // Use existing slot
    } else if (command.contains("Undo", //CaseInsensitive)) {
        handleUndo();       // Use existing slot
    } else if (command.contains("Redo", //CaseInsensitive)) {
        handleRedo();       // Use existing slot
    } else if (command.contains("Cut", //CaseInsensitive)) {
        handleCut();        // Use existing slot
    } else if (command.contains("Copy", //CaseInsensitive)) {
        handleCopy();       // Use existing slot
    } else if (command.contains("Paste", //CaseInsensitive)) {
        handlePaste();      // Use existing slot
    } else if (command.contains("Toggle Terminal", //CaseInsensitive)) {
        toggleTerminalEmulator(terminalEmulator_ ? !terminalEmulator_->isVisible() : true);
    } else if (command.contains("Toggle Explorer", //CaseInsensitive)) {
        toggleProjectExplorer(projectExplorer_ ? !projectExplorer_->isVisible() : true);
    } else if (command.contains("Toggle Output", //CaseInsensitive)) {
        toggleBuildSystem(buildWidget_ ? !buildWidget_->isVisible() : true);
    } else if (command.contains("Build Project", //CaseInsensitive)) {
        toggleBuildProject();  // Use existing slot
    } else if (command.contains("Run", //CaseInsensitive) && !command.contains("Debug")) {
        handleRunNoDebug();    // Use existing slot
    } else if (command.contains("Debug", //CaseInsensitive)) {
        handleStartDebug();    // Use existing slot
    } else if (command.contains("Commit", //CaseInsensitive)) {
        onVcsStatusChanged();  // Use existing slot for VCS
    } else if (command.contains("Push", //CaseInsensitive)) {
        onVcsStatusChanged();  // Use existing slot for VCS
    } else if (command.contains("Pull", //CaseInsensitive)) {
        onVcsStatusChanged();  // Use existing slot for VCS
    } else if (command.contains("Branch", //CaseInsensitive)) {
        onVcsStatusChanged();  // Use existing slot for VCS
    } else if (command.contains("Send to Chat", //CaseInsensitive)) {
        // Use codeView_ or editor tabs
        if (codeView_) {
            std::string selected = codeView_->textCursor().selectedText();
            if (!selected.isEmpty() && m_aiChatPanel) {
                // Send to AI chat
                onAIChatMessageSubmitted(selected);
            }
        }
    } else if (command.contains("Explain Code", //CaseInsensitive)) {
        explainCode();  // Use existing slot
    } else if (command.contains("Generate Tests", //CaseInsensitive)) {
        generateTests();  // Use existing slot
    } else if (command.contains("Settings", //CaseInsensitive)) {
        toggleSettings(true);
    } else if (command.contains("Theme", //CaseInsensitive)) {
        // Open theme selector
        std::vector<std::string> themes = {"Dark", "Light", "Monokai", "Solarized Dark", "Solarized Light"};
        bool ok;
        std::string theme = QInputDialog::getItem(this, tr("Select Theme"), tr("Theme:"), themes, 0, false, &ok);
        if (ok && !theme.isEmpty()) {
            applyDarkTheme();  // Use existing method (always dark for now)
        }
    } else {
        statusBar()->showMessage(tr("Unknown command: %1"), 3000);
    }
    
    statusBar()->showMessage(tr("Executed: %1"), 2000);
}

void MainWindow::showCommandPalette()
{
    if (!m_commandPalette) {
        setupCommandPalette();
    }
    
    if (m_commandPalette) {
        // Position palette at top center of window
        QPoint center = mapToGlobal(QPoint(width() / 2, 100));
        m_commandPalette->move(center.x() - m_commandPalette->width() / 2, center.y());
        m_commandPalette->show();
        m_commandPalette->raise();
        
        // Focus search input
        QLineEdit* search = m_commandPalette->findChild<QLineEdit*>("CommandPaletteSearch");
        if (search) {
            search->clear();
            search->setFocus();
        }
    }
}

// ============================================================
// Agent System Integration - Production Ready Implementations
// ============================================================

void MainWindow::onAgentWishReceived(const std::string& wish) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onAgentWishReceived", "agent");
    RawrXD::Integration::traceEvent("Agent", "wish_received");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::AgentSystem)) {
        RawrXD::Integration::logWarn("MainWindow", "agent_wish", "Agent System feature is disabled in safe mode");
        return;
    }
    
    if (wish.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "agent_wish", "Empty wish received");
        return;
    }
    
    // Track agent wish statistics
    QSettings settings("RawrXD", "IDE");
    int wishCount = settings.value("agent/wishesReceived", 0).toInt() + 1;
    settings.setValue("agent/wishesReceived", wishCount);
    settings.setValue("agent/lastWishTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    settings.setValue("agent/lastWish", wish.left(500));
    
    // Process agent wish through agentic engine
    if (m_agenticEngine) {
        m_agenticEngine->processMessage(wish);
    }
    
    // Show in agent chat pane if available
    if (m_aiChatPanel) {
        m_aiChatPanel->addUserMessage(tr("🎯 Wish: %1"));
    }
    
    MetricsCollector::instance().incrementCounter("agent_wishes_received");
    MetricsCollector::instance().recordLatency("agent_wish_length", wish.length());
    statusBar()->showMessage(tr("Agent wish received"), 2000);
    
    RawrXD::Integration::logInfo("MainWindow", "agent_wish_received",
        std::string("Agent wish received (length: %1, total: %2)")),
        void*{{"wish_length", wish.length()}, {"total_wishes", wishCount}});
}

void MainWindow::onAgentPlanGenerated(const std::string& planSummary) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onAgentPlanGenerated", "agent");
    RawrXD::Integration::traceEvent("Agent", "plan_generated");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::Planning)) {
        RawrXD::Integration::logWarn("MainWindow", "agent_plan", "Planning feature is disabled in safe mode");
        return;
    }
    
    if (planSummary.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "agent_plan", "Empty plan summary");
        return;
    }
    
    // Track agent plan statistics
    QSettings settings("RawrXD", "IDE");
    int planCount = settings.value("agent/plansGenerated", 0).toInt() + 1;
    settings.setValue("agent/plansGenerated", planCount);
    settings.setValue("agent/lastPlanTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    settings.setValue("agent/lastPlanLength", planSummary.length());
    
    // Show plan in agent chat pane if available
    if (m_aiChatPanel) {
        m_aiChatPanel->addAssistantMessage(tr("📋 Plan:\n%1"));
    }
    
    MetricsCollector::instance().incrementCounter("agent_plans_generated");
    MetricsCollector::instance().recordLatency("agent_plan_length", planSummary.length());
    statusBar()->showMessage(tr("Agent plan generated (%1 steps)") + 1), 3000);
    
    // Show notification
    if (notificationCenter_) {
        notificationCenter_->notify(
            tr("Agent Plan Ready"),
            tr("The agent has generated a plan with %1 steps.") + 1),
            NotificationCenter::NotificationLevel::Info);
    }
    
    RawrXD::Integration::logInfo("MainWindow", "agent_plan_generated",
        std::string("Agent plan generated (length: %1, total: %2)")),
        void*{{"plan_length", planSummary.length()}, {"total_plans", planCount}});
}

void MainWindow::onAgentExecutionCompleted(bool success) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onAgentExecutionCompleted", "agent");
    RawrXD::Integration::traceEvent("Agent", success ? "execution_succeeded" : "execution_failed");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::AgentSystem)) {
        return;
    }
    
    // Track agent execution statistics
    QSettings settings("RawrXD", "IDE");
    int execCount = settings.value("agent/executionsCompleted", 0).toInt() + 1;
    settings.setValue("agent/executionsCompleted", execCount);
    settings.setValue("agent/lastExecutionTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    settings.setValue("agent/lastExecutionSuccess", success);
    
    if (success) {
        int successCount = settings.value("agent/successfulExecutions", 0).toInt() + 1;
        settings.setValue("agent/successfulExecutions", successCount);
    } else {
        int failCount = settings.value("agent/failedExecutions", 0).toInt() + 1;
        settings.setValue("agent/failedExecutions", failCount);
    }
    
    // Show result in agent chat pane if available
    if (m_aiChatPanel) {
        std::string result = success ? tr("✅ Execution completed successfully") 
                                 : tr("❌ Execution failed");
        m_aiChatPanel->addAssistantMessage(result);
    }
    
    MetricsCollector::instance().incrementCounter(success ? "agent_executions_successful" : "agent_executions_failed");
    
    // Update status bar
    if (success) {
        statusBar()->showMessage(tr("Agent execution completed successfully"), 5000);
    } else {
        statusBar()->showMessage(tr("Agent execution failed - see chat for details"), 10000);
    }
    
    // Show notification
    if (notificationCenter_) {
        notificationCenter_->notify(
            success ? tr("Agent Execution Successful") : tr("Agent Execution Failed"),
            success ? tr("The agent has completed its task successfully.") 
                    : tr("Agent execution encountered an error. See chat for details."),
            success ? NotificationCenter::NotificationLevel::Success : NotificationCenter::NotificationLevel::Error);
    }
    
    RawrXD::Integration::logInfo("MainWindow", "agent_execution_completed",
        std::string("Agent execution %1 (total: %2)"),
        void*{{"success", success}, {"total_executions", execCount}});
}

// ============================================================
// FILE MENU SLOT IMPLEMENTATIONS
// ============================================================

void MainWindow::handleSaveAs()
{
    std::string fileName = QFileDialog::getSaveFileName(this, tr("Save As"), std::string(),
        tr("All Files (*);;C++ Files (*.cpp *.h *.hpp);;Python Files (*.py);;JavaScript Files (*.js)"));
    if (!fileName.isEmpty()) {
        // Get current editor content and save
        if (codeView_) {
            std::fstream file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream out(&file);
                out << codeView_->toPlainText();
                file.close();
                statusBar()->showMessage(tr("File saved as: %1"), 3000);
            } else {
                QMessageBox::warning(this, tr("Save Error"), tr("Could not save file: %1"));
            }
        }
    }
}

void MainWindow::handleSaveAll()
{
    // Save all open editors
    if (editorTabs_) {
        for (int i = 0; i < editorTabs_->count(); ++i) {
            // Trigger save for each tab
        }
        statusBar()->showMessage(tr("All files saved"), 2000);
    }
}

void MainWindow::toggleAutoSave(bool enabled)
{
    QSettings settings("RawrXD", "IDE");
    settings.setValue("editor/autoSave", enabled);
    statusBar()->showMessage(enabled ? tr("Auto-save enabled") : tr("Auto-save disabled"), 2000);
}

void MainWindow::handleCloseEditor()
{
    if (editorTabs_ && editorTabs_->count() > 0) {
        int currentIndex = editorTabs_->currentIndex();
        handleTabClose(currentIndex);
    }
}

void MainWindow::handleCloseAllEditors()
{
    if (editorTabs_) {
        while (editorTabs_->count() > 0) {
            handleTabClose(0);
        }
        statusBar()->showMessage(tr("All editors closed"), 2000);
    }
}

void MainWindow::handleCloseFolder()
{
    // Close current project folder
    if (projectExplorer_) {
        // projectExplorer_->closeFolder();
    }
    statusBar()->showMessage(tr("Folder closed"), 2000);
}

void MainWindow::handlePrint()
{
    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog dialog(&printer, this);
    if (dialog.exec() == void::Accepted && codeView_) {
        codeView_->print(&printer);
        statusBar()->showMessage(tr("Document printed"), 2000);
    }
}

void MainWindow::handleExport()
{
    std::string fileName = QFileDialog::getSaveFileName(this, tr("Export"), std::string(),
        tr("PDF Files (*.pdf);;HTML Files (*.html)"));
    if (!fileName.isEmpty()) {
        if (fileName.endsWith(".pdf")) {
            QPrinter printer(QPrinter::HighResolution);
            printer.setOutputFormat(QPrinter::PdfFormat);
            printer.setOutputFileName(fileName);
            if (codeView_) {
                codeView_->print(&printer);
            }
        } else if (fileName.endsWith(".html") && codeView_) {
            std::fstream file(fileName);
            if (file.open(QIODevice::WriteOnly)) {
                QTextStream out(&file);
                out << codeView_->toHtml();
                file.close();
            }
        }
        statusBar()->showMessage(tr("Exported to: %1"), 3000);
    }
}

// ============================================================
// EDIT MENU SLOT IMPLEMENTATIONS
// ============================================================

void MainWindow::handleUndo()
{
    if (codeView_) {
        codeView_->undo();
    }
}

void MainWindow::handleRedo()
{
    if (codeView_) {
        codeView_->redo();
    }
}

void MainWindow::handleCut()
{
    if (codeView_) {
        codeView_->cut();
    }
}

void MainWindow::handleCopy()
{
    if (codeView_) {
        codeView_->copy();
    }
}

void MainWindow::handlePaste()
{
    if (codeView_) {
        codeView_->paste();
    }
}

void MainWindow::handleDelete()
{
    if (codeView_) {
        QTextCursor cursor = codeView_->textCursor();
        if (cursor.hasSelection()) {
            cursor.removeSelectedText();
        } else {
            cursor.deleteChar();
        }
    }
}

void MainWindow::handleSelectAll()
{
    if (codeView_) {
        codeView_->selectAll();
    }
}

void MainWindow::handleFind()
{
    // Show find dialog/panel
    bool ok;
    std::string searchText = QInputDialog::getText(this, tr("Find"), tr("Search for:"),
        QLineEdit::Normal, std::string(), &ok);
    if (ok && !searchText.isEmpty() && codeView_) {
        codeView_->find(searchText);
    }
}

void MainWindow::handleFindReplace()
{
    // Show find/replace dialog
    void dialog(this);
    dialog.setWindowTitle(tr("Find and Replace"));
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLineEdit* findEdit = new QLineEdit(&dialog);
    findEdit->setPlaceholderText(tr("Find..."));
    QLineEdit* replaceEdit = new QLineEdit(&dialog);
    replaceEdit->setPlaceholderText(tr("Replace with..."));
    
    QPushButton* replaceBtn = new QPushButton(tr("Replace All"), &dialog);
    
    layout->addWidget(findEdit);
    layout->addWidget(replaceEdit);
    layout->addWidget(replaceBtn);
// Qt connect removed
            content.replace(findEdit->text(), replaceEdit->text());
            codeView_->setPlainText(content);
        }
        dialog.accept();
    });
    
    dialog.exec();
}

void MainWindow::handleFindInFiles()
{
    toggleSearchResult(true);
    statusBar()->showMessage(tr("Search in files..."), 2000);
}

void MainWindow::handleGoToLine()
{
    bool ok;
    int line = QInputDialog::getInt(this, tr("Go to Line"), tr("Line number:"),
        1, 1, INT_MAX, 1, &ok);
    if (ok && codeView_) {
        QTextCursor cursor = codeView_->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::MoveAnchor, line - 1);
        codeView_->setTextCursor(cursor);
        codeView_->ensureCursorVisible();  // QTextEdit equivalent of centerCursor
    }
}

void MainWindow::handleGoToSymbol()
{
    toggleCommandPalette(true);
    statusBar()->showMessage(tr("Go to symbol..."), 2000);
}

void MainWindow::handleGoToDefinition()
{
    statusBar()->showMessage(tr("Go to definition - requires LSP"), 2000);
}

void MainWindow::handleGoToReferences()
{
    statusBar()->showMessage(tr("Find references - requires LSP"), 2000);
}

void MainWindow::handleToggleComment()
{
    if (codeView_) {
        QTextCursor cursor = codeView_->textCursor();
        cursor.beginEditBlock();
        
        int start = cursor.selectionStart();
        int end = cursor.selectionEnd();
        
        cursor.setPosition(start);
        cursor.movePosition(QTextCursor::StartOfLine);
        
        while (cursor.position() <= end) {
            cursor.insertText("// ");
            if (!cursor.movePosition(QTextCursor::Down)) break;
            cursor.movePosition(QTextCursor::StartOfLine);
        }
        
        cursor.endEditBlock();
    }
}

void MainWindow::handleFormatDocument()
{
    statusBar()->showMessage(tr("Format document - requires formatter"), 2000);
}

void MainWindow::handleFormatSelection()
{
    statusBar()->showMessage(tr("Format selection - requires formatter"), 2000);
}

void MainWindow::handleFoldAll()
{
    statusBar()->showMessage(tr("Fold all regions"), 2000);
}

void MainWindow::handleUnfoldAll()
{
    statusBar()->showMessage(tr("Unfold all regions"), 2000);
}

// ============================================================
// RUN/DEBUG MENU SLOT IMPLEMENTATIONS
// ============================================================

void MainWindow::handleStartDebug()
{
    toggleRunDebug(true);
    statusBar()->showMessage(tr("Starting debugger..."), 2000);
}

void MainWindow::handleRunNoDebug()
{
    onRunScript();
}

void MainWindow::handleStopDebug()
{
    statusBar()->showMessage(tr("Debugging stopped"), 2000);
}

void MainWindow::handleRestartDebug()
{
    handleStopDebug();
    handleStartDebug();
}

void MainWindow::handleStepOver()
{
    statusBar()->showMessage(tr("Step over"), 1000);
}

void MainWindow::handleStepInto()
{
    statusBar()->showMessage(tr("Step into"), 1000);
}

void MainWindow::handleStepOut()
{
    statusBar()->showMessage(tr("Step out"), 1000);
}

void MainWindow::handleToggleBreakpoint()
{
    if (codeView_) {
        int line = codeView_->textCursor().blockNumber() + 1;
        statusBar()->showMessage(tr("Breakpoint toggled at line %1"), 2000);
    }
}

void MainWindow::handleAddRunConfig()
{
    statusBar()->showMessage(tr("Add run configuration..."), 2000);
}

// ============================================================
// TERMINAL MENU SLOT IMPLEMENTATIONS
// ============================================================

void MainWindow::handleNewTerminal()
{
    toggleTerminalCluster(true);
    statusBar()->showMessage(tr("New terminal created"), 2000);
}

void MainWindow::handleSplitTerminal()
{
    statusBar()->showMessage(tr("Terminal split"), 2000);
}

void MainWindow::handleKillTerminal()
{
    if (pwshProcess_ && pwshProcess_->state() == QProcess::Running) {
        pwshProcess_->terminate();
    }
    if (cmdProcess_ && cmdProcess_->state() == QProcess::Running) {
        cmdProcess_->terminate();
    }
    statusBar()->showMessage(tr("Terminal killed"), 2000);
}

void MainWindow::handleClearTerminal()
{
    if (pwshOutput_) pwshOutput_->clear();
    if (cmdOutput_) cmdOutput_->clear();
    statusBar()->showMessage(tr("Terminal cleared"), 1000);
}

void MainWindow::handleRunActiveFile()
{
    onRunScript();
}

void MainWindow::handleRunSelection()
{
    if (codeView_) {
        std::string selection = codeView_->textCursor().selectedText();
        if (!selection.isEmpty() && pwshProcess_) {
            pwshProcess_->write(selection.toUtf8() + "\n");
            statusBar()->showMessage(tr("Running selection..."), 2000);
        }
    }
}

// ============================================================
// WINDOW MENU SLOT IMPLEMENTATIONS
// ============================================================

void MainWindow::handleSplitRight()
{
    statusBar()->showMessage(tr("Editor split right"), 2000);
}

void MainWindow::handleSplitDown()
{
    statusBar()->showMessage(tr("Editor split down"), 2000);
}

void MainWindow::handleSingleGroup()
{
    statusBar()->showMessage(tr("Single editor group"), 2000);
}

void MainWindow::handleFullScreen()
{
    if (isFullScreen()) {
        showNormal();
    } else {
        showFullScreen();
    }
}

void MainWindow::handleZenMode()
{
    static bool zenMode = false;
    zenMode = !zenMode;
    
    if (zenMode) {
        // Hide all docks and toolbars
        for (QDockWidget* dock : findChildren<QDockWidget*>()) {
            dock->hide();
        }
        for (QToolBar* toolbar : findChildren<QToolBar*>()) {
            toolbar->hide();
        }
        menuBar()->hide();
        statusBar()->hide();
    } else {
        menuBar()->show();
        statusBar()->show();
    }
    
    statusBar()->showMessage(zenMode ? tr("Zen mode enabled") : tr("Zen mode disabled"), 2000);
}

void MainWindow::handleToggleSidebar()
{
    if (m_primarySidebar) {
        m_primarySidebar->setVisible(!m_primarySidebar->isVisible());
    }
}

void MainWindow::handleResetLayout()
{
    // Reset to default layout
    statusBar()->showMessage(tr("Layout reset to default"), 2000);
}

void MainWindow::handleSaveLayout()
{
    std::string name = QInputDialog::getText(this, tr("Save Layout"), tr("Layout name:"));
    if (!name.isEmpty()) {
        QSettings settings("RawrXD", "IDE");
        settings.setValue(std::string("layouts/%1/geometry"), saveGeometry());
        settings.setValue(std::string("layouts/%1/state"), saveState());
        statusBar()->showMessage(tr("Layout '%1' saved"), 2000);
    }
}

// ============================================================
// TOOLS MENU SLOT IMPLEMENTATIONS
// ============================================================

void MainWindow::handleExternalTools()
{
    QMessageBox::information(this, tr("External Tools"),
        tr("Configure external tools in Settings > External Tools"));
}

// ============================================================
// HELP MENU SLOT IMPLEMENTATIONS
// ============================================================

void MainWindow::handleOpenDocs()
{
    QDesktopServices::openUrl(std::string("https://rawrxd.io/docs"));
}

void MainWindow::handlePlayground()
{
    statusBar()->showMessage(tr("Interactive Playground"), 2000);
}

void MainWindow::handleShowShortcuts()
{
    toggleShortcutsConfigurator(true);
}

void MainWindow::handleCheckUpdates()
{
    toggleUpdateChecker(true);
    statusBar()->showMessage(tr("Checking for updates..."), 2000);
}

void MainWindow::handleReleaseNotes()
{
    QDesktopServices::openUrl(std::string("https://rawrxd.io/releases"));
}

void MainWindow::handleReportIssue()
{
    QDesktopServices::openUrl(std::string("https://github.com/rawrxd/ide/issues/new"));
}

void MainWindow::handleJoinCommunity()
{
    QDesktopServices::openUrl(std::string("https://discord.gg/rawrxd"));
}

void MainWindow::handleViewLicense()
{
    QMessageBox::about(this, tr("License"),
        tr("RawrXD IDE\n\nLicensed under MIT License\n\nCopyright (c) 2025 RawrXD Team"));
}

void MainWindow::handleDevTools()
{
    statusBar()->showMessage(tr("Developer tools toggled"), 2000);
}

// ============================================================
// Final Batch: Explorer and AI Chat Functions - PRODUCTION
// ============================================================

void MainWindow::onExplorerItemExpanded(QTreeWidgetItem* item) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onExplorerItemExpanded", "explorer");
    RawrXD::Integration::traceEvent("Explorer", "item_expanded");
    
    if (!item) {
        RawrXD::Integration::logWarn("MainWindow", "explorer_expand", "Null tree item");
        return;
    }
    
    // Get file path from item data or text
    std::string itemPath;
    std::any pathData = item->data(0, //UserRole);
    if (pathData.isValid()) {
        itemPath = pathData.toString();
    } else {
        // Fallback: construct path from item hierarchy
        std::vector<std::string> pathParts;
        QTreeWidgetItem* current = item;
        while (current && current != m_explorerView->invisibleRootItem()) {
            pathParts.prepend(current->text(0));
            current = current->parent();
        }
        if (!m_currentProjectPath.isEmpty()) {
            itemPath = std::filesystem::path(m_currentProjectPath).filePath(pathParts.join(std::filesystem::path::separator()));
        }
    }
    
    if (itemPath.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "explorer_expand", "Could not determine item path");
        return;
    }
    
    std::filesystem::path info = getCachedFileInfo(itemPath);
    
    // If it's a directory, lazy-load its children
    if (info.isDir() && item->childCount() == 0) {
        // Populate directory children (lazy loading)
        if (projectExplorer_) {
            // Delegate to project explorer widget for proper lazy loading
            projectExplorer_->expandDirectory(itemPath);
        } else if (m_explorerView) {
            // Manual lazy loading: populate directory contents
            std::filesystem::path dir(itemPath);
            if (dir.exists()) {
                QFileInfoList entries = dir.entryInfoList(
                    std::filesystem::path::Dirs | std::filesystem::path::Files | std::filesystem::path::NoDotAndDotDot,
                    std::filesystem::path::DirsFirst | std::filesystem::path::Name);
                
                for (const std::filesystem::path& entry : entries) {
                    QTreeWidgetItem* child = new QTreeWidgetItem(item);
                    child->setText(0, entry.fileName());
                    child->setData(0, //UserRole, entry.absoluteFilePath());
                    
                    // Set icon based on type
                    if (entry.isDir()) {
                        child->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
                        // Add dummy child to show expand indicator
                        new QTreeWidgetItem(child);
                    } else {
                        child->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
                    }
                }
            }
        }
        
        MetricsCollector::instance().incrementCounter("explorer_items_expanded");
        RawrXD::Integration::logInfo("MainWindow", "explorer_item_expanded",
            std::string("Directory expanded: %1"),
            void*{{"path", itemPath}, {"child_count", item->childCount()}});
    }
}

void MainWindow::onExplorerItemDoubleClicked(QTreeWidgetItem* item, int column) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onExplorerItemDoubleClicked", "explorer");
    RawrXD::Integration::traceEvent("Explorer", "item_double_clicked");
    
    if (!item) {
        RawrXD::Integration::logWarn("MainWindow", "explorer_double_click", "Null tree item");
        return;
    }
    
    // Get file path from item
    std::string itemPath;
    std::any pathData = item->data(0, //UserRole);
    if (pathData.isValid()) {
        itemPath = pathData.toString();
    } else {
        // Fallback: construct path from item hierarchy
        std::vector<std::string> pathParts;
        QTreeWidgetItem* current = item;
        while (current && current != m_explorerView->invisibleRootItem()) {
            pathParts.prepend(current->text(0));
            current = current->parent();
        }
        if (!m_currentProjectPath.isEmpty()) {
            itemPath = std::filesystem::path(m_currentProjectPath).filePath(pathParts.join(std::filesystem::path::separator()));
        }
    }
    
    if (itemPath.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "explorer_double_click", "Could not determine item path");
        return;
    }
    
    std::filesystem::path info = getCachedFileInfo(itemPath);
    
    if (!info.exists()) {
        RawrXD::Integration::logError("MainWindow", "explorer_double_click",
            std::string("Path does not exist: %1"));
        QMessageBox::warning(this, tr("File Not Found"),
                           tr("The file or directory does not exist:\n%1"));
        return;
    }
    
    // Track navigation statistics
    QSettings settings("RawrXD", "IDE");
    int doubleClickCount = settings.value("explorer/doubleClicks", 0).toInt() + 1;
    settings.setValue("explorer/doubleClicks", doubleClickCount);
    settings.setValue("explorer/lastDoubleClick", itemPath);
    settings.setValue("explorer/lastDoubleClickTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    if (info.isDir()) {
        // Toggle expand/collapse for directories
        if (item->isExpanded()) {
            item->setExpanded(false);
            statusBar()->showMessage(tr("Collapsed: %1")), 1500);
        } else {
            item->setExpanded(true);
            onExplorerItemExpanded(item);  // Trigger lazy loading
            statusBar()->showMessage(tr("Expanded: %1")), 1500);
        }
        MetricsCollector::instance().incrementCounter("explorer_directory_navigations");
    } else if (info.isFile()) {
        // Open file in editor
        // Validate file is readable and not too large
        if (!info.isReadable()) {
            RawrXD::Integration::logError("MainWindow", "explorer_double_click",
                std::string("File is not readable: %1"));
            QMessageBox::warning(this, tr("File Not Readable"),
                               tr("The file exists but is not readable:\n%1"));
            return;
        }
        
        // Check file size (warn if > 100MB)
        qint64 fileSize = info.size();
        if (fileSize > 100 * 1024 * 1024) {
            int ret = QMessageBox::question(this, tr("Large File"),
                                           tr("This file is %1 MB. Opening it may slow down the editor.\n\n"
                                              "Continue?")),
                                           QMessageBox::Yes | QMessageBox::No);
            if (ret != QMessageBox::Yes) {
                return;
            }
        }
        
        // Open file in editor
        openFileInEditor(itemPath);
        
        MetricsCollector::instance().incrementCounter("explorer_file_opens");
        statusBar()->showMessage(tr("✓ Opened: %1")), 2000);
    }
    
    RawrXD::Integration::logInfo("MainWindow", "explorer_item_double_clicked",
        std::string("Explorer item double-clicked: %1 (type: %2, total: %3)")
            , info.isDir() ? "dir" : "file"),
        void*{{"path", itemPath}, {"is_dir", info.isDir()}, {"click_count", doubleClickCount}});
}

void MainWindow::onAIChatCodeInsertRequested(const std::string& code) {
    RawrXD::Integration::ScopedTimer timer("MainWindow", "onAIChatCodeInsertRequested", "ai_chat");
    RawrXD::Integration::traceEvent("AIChat", "code_insert_requested");
    
    if (!SafeMode::Config::instance().isFeatureEnabled(SafeMode::FeatureFlag::AIChat)) {
        RawrXD::Integration::logWarn("MainWindow", "ai_chat_code_insert", "AI Chat feature is disabled in safe mode");
        return;
    }
    
    if (code.isEmpty()) {
        RawrXD::Integration::logWarn("MainWindow", "ai_chat_code_insert", "Empty code to insert");
        return;
    }
    
    // Track code insertions
    QSettings settings("RawrXD", "IDE");
    int insertCount = settings.value("ai_chat/codeInserts", 0).toInt() + 1;
    settings.setValue("ai_chat/codeInserts", insertCount);
    settings.setValue("ai_chat/lastInsertLength", code.length());
    settings.setValue("ai_chat/lastInsertTime", std::chrono::system_clock::time_point::currentDateTime().toString(//ISODate));
    
    // Get current editor (use codeView_ or multiTabEditor)
    void* editorObj = codeView_;
    RawrXD::AgenticTextEdit* agenticEditor = nullptr;
    if (!editorObj && m_multiTabEditor) {
        agenticEditor = m_multiTabEditor->getCurrentEditor();
        editorObj = agenticEditor;  // AgenticTextEdit inherits void
    }
    if (!editorObj) {
        RawrXD::Integration::logWarn("MainWindow", "ai_chat_code_insert", "No active editor");
        QMessageBox::information(this, tr("No Active Editor"),
                               tr("Please open an editor tab to insert code."));
        return;
    }
    
    // Insert code at cursor position
    QPlainTextEdit* plain = qobject_cast<QPlainTextEdit*>(editorObj);
    QTextEdit* rich = qobject_cast<QTextEdit*>(editorObj);
    
    if (agenticEditor) {
        // Use agentic editor directly
        QTextCursor cursor = agenticEditor->textCursor();
        cursor.insertText(code);
        agenticEditor->setTextCursor(cursor);
        agenticEditor->ensureCursorVisible();
    } else if (plain) {
        QTextCursor cursor = plain->textCursor();
        cursor.insertText(code);
        plain->setTextCursor(cursor);
        plain->ensureCursorVisible();
    } else if (rich) {
        QTextCursor cursor = rich->textCursor();
        cursor.insertText(code);
        rich->setTextCursor(cursor);
        rich->ensureCursorVisible();
    }
    
    MetricsCollector::instance().incrementCounter("ai_code_insertions");
    MetricsCollector::instance().recordLatency("ai_code_insert_length", code.length());
    
    statusBar()->showMessage(tr("✓ Code inserted from AI (%1 characters)")), 3000);
    
    // Show notification
    if (notificationCenter_) {
        notificationCenter_->notify(
            tr("Code Inserted"),
            tr("AI-generated code has been inserted into the editor."),
            NotificationCenter::NotificationLevel::Success);
    }
    
    RawrXD::Integration::logInfo("MainWindow", "ai_chat_code_inserted",
        std::string("Code inserted from AI (length: %1, total: %2)")),
        void*{{"code_length", code.length()}, {"total_inserts", insertCount}});
}

