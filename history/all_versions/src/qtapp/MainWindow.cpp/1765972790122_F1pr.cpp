// RawrXD IDE MainWindow Implementation
// "One IDE to rule them all" - comprehensive development environment
#include "orchestration/TaskOrchestrator.h"
#include "orchestration/OrchestrationUI.h"
#include "ActivityBar.h"
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
#include "../agent/auto_bootstrap.hpp"
#include "../agent/hot_reload.hpp"
#include "../agent/self_test_gate.hpp"
#include "../agent/meta_planner.hpp"
#include "../agent/action_executor.hpp"
#include "../agent/model_invoker.hpp"
#include "widgets/layer_quant_widget.hpp"
#include "settings_dialog.h"
#include "settings_manager.h"

// Forward declaration resolved by include above

// ----------------  brutal-gzip glue  ----------------
#include "deflate_brutal_qt.hpp"     // compress / decompress

#include <QApplication>
#include <QAction>
#include <QActionGroup>
#include <QFileSystemModel>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QShortcut>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTextEdit>
#include <QToolBar>
#include <QTreeView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QTextStream>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QCloseEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QComboBox>
#include <QDockWidget>
#include <QColor>
#include <QUrl>
#include <QStackedWidget>
#include <QFrame>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPalette>
#include <QFont>
#include <QThread>
#include <QDateTime>
#include <QInputDialog>
#include <QMetaObject>
#include <QVariant>
#include <QSettings>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("RawrXD IDE - Quantization Ready");
    resize(1600, 1000);

    // Create the complete VS Code-like layout
    createVSCodeLayout();
    
    setupMenuBar();
    setupToolBars();
    setupStatusBar();
    
    initSubsystems();
    
    // Initialize inference engine in worker thread
    m_engineThread = new QThread(this);
    m_inferenceEngine = new InferenceEngine();
    m_inferenceEngine->moveToThread(m_engineThread);
    
    // Connect signals
    connect(m_engineThread, &QThread::finished, m_inferenceEngine, &QObject::deleteLater);
    connect(m_inferenceEngine, &InferenceEngine::resultReady, this, &MainWindow::showInferenceResult);
    connect(m_inferenceEngine, &InferenceEngine::error, this, &MainWindow::showInferenceError);
    connect(m_inferenceEngine, &InferenceEngine::modelLoadedChanged, this, &MainWindow::onModelLoadedChanged);
    
    m_engineThread->start();
    
    // Initialize GGUF server (auto-starts if port 11434 is available)
    m_ggufServer = new GGUFServer(m_inferenceEngine, this);
    connect(m_ggufServer, &GGUFServer::serverStarted, this, [this](quint16 port) {
        statusBar()->showMessage(tr("GGUF Server running on port %1").arg(port), 5000);
        qDebug() << "GGUF Server started on port" << port;
    });
    connect(m_ggufServer, &GGUFServer::error, this, [](const QString& err) {
        qWarning() << "GGUF Server error:" << err;
    });
    
    // Start server after a short delay to ensure engine thread is fully initialized
    QTimer::singleShot(500, this, [this]() {
        m_ggufServer->start(11434);
    });
    
    // Initialize streaming inference
    m_streamer = new StreamingInference(m_hexMagConsole, this);
    m_streamingMode = false;
    m_currentStreamId = 0;
    
        // Connect streaming signals (adapt signature qint64,QString -> QString)
        connect(m_inferenceEngine, &InferenceEngine::streamToken,
            this, [this](qint64 /*reqId*/, const QString& token) { m_streamer->pushToken(token); });
        connect(m_inferenceEngine, &InferenceEngine::streamFinished,
            this, [this](qint64 /*reqId*/) { m_streamer->finishStream(); });
    
    // Set dark theme
    applyDarkTheme();
    
    // Setup AI/agent components
    setupAIBackendSwitcher();
    setupLayerQuantWidget();
    setupSwarmEditing();
    setupAgentSystem();
    setupCommandPalette();
    setupAIChatPanel();
    setupMASMEditor();
    setupInterpretabilityPanel();  // Model analysis & diagnostics
    setupOrchestrationSystem();     // AI task orchestration
    
    // Setup Ctrl+Shift+P for command palette
    QShortcut* commandPaletteShortcut = new QShortcut(QKeySequence("Ctrl+Shift+P"), this);
    connect(commandPaletteShortcut, &QShortcut::activated, this, [this]() {
        if (m_commandPalette) m_commandPalette->show();
    });

    // Enable zero-touch triggers so the agent auto-starts without manual input
    // AutoBootstrap::installZeroTouch();

    // Optional: initialize per-layer quantization UI
    setupLayerQuantWidget();

    // Auto-load GGUF from env var if provided (e.g., RAWRXD_GGUF=D:\\OllamaModels\\BigDaddyG-Q2_K-ULTRA.gguf)
    QString ggufEnv = qEnvironmentVariable("RAWRXD_GGUF");
    if (!ggufEnv.isEmpty()) {
        statusBar()->showMessage(tr("Auto-loading GGUF: %1").arg(ggufEnv), 3000);
        QMetaObject::invokeMethod(m_inferenceEngine, "loadModel", Qt::QueuedConnection,
                                  Q_ARG(QString, ggufEnv));
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
    QWidget* mainContainer = new QWidget(this);
    QHBoxLayout* mainLayout = new QHBoxLayout(mainContainer);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    
    // ============= LEFT: Activity Bar (50px) =============
    m_activityBar = new ActivityBar(mainContainer);
    mainLayout->addWidget(m_activityBar, 0);
    
    // ============= CENTER: Vertical Splitter (Sidebar + Editor) =============
    QSplitter* centerSplitter = new QSplitter(Qt::Horizontal, mainContainer);
    centerSplitter->setOpaqueResize(true);
    centerSplitter->setStyleSheet("QSplitter::handle { background-color: #2d2d2d; }");
    
    // --------- Primary Sidebar (260px) ---------
    m_primarySidebar = new QFrame(mainContainer);
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
    m_sidebarStack = new QStackedWidget(m_primarySidebar);
    m_sidebarStack->setStyleSheet("QStackedWidget { background-color: #252526; }");
    
    // Create Explorer view (placeholder - tree widget)
    QTreeWidget* explorerView = new QTreeWidget(m_primarySidebar);
    explorerView->setStyleSheet("QTreeWidget { background-color: #252526; color: #e0e0e0; }");
    QTreeWidgetItem* rootItem = new QTreeWidgetItem();
    rootItem->setText(0, "Project Folder");
    explorerView->addTopLevelItem(rootItem);
    m_sidebarStack->addWidget(explorerView);
    
    // Create Search view (placeholder)
    QWidget* searchView = new QWidget(m_primarySidebar);
    QVBoxLayout* searchLayout = new QVBoxLayout(searchView);
    QLineEdit* searchInput = new QLineEdit(m_primarySidebar);
    searchInput->setPlaceholderText("Search files...");
    searchInput->setStyleSheet("QLineEdit { background-color: #3c3c3c; color: #e0e0e0; border: 1px solid #555; padding: 5px; }");
    searchLayout->addWidget(searchInput);
    m_sidebarStack->addWidget(searchView);
    
    // Create Source Control view (placeholder)
    QWidget* scmView = new QWidget(m_primarySidebar);
    QVBoxLayout* scmLayout = new QVBoxLayout(scmView);
    QLabel* scmLabel = new QLabel("Source Control\n\nNo folder open", m_primarySidebar);
    scmLabel->setStyleSheet("QLabel { color: #e0e0e0; }");
    scmLabel->setAlignment(Qt::AlignCenter);
    scmLayout->addWidget(scmLabel);
    m_sidebarStack->addWidget(scmView);
    
    // Create Debug view (placeholder)
    QWidget* debugView = new QWidget(m_primarySidebar);
    QVBoxLayout* debugLayout = new QVBoxLayout(debugView);
    QLabel* debugLabel = new QLabel("Run and Debug\n\nNo launch configuration", m_primarySidebar);
    debugLabel->setStyleSheet("QLabel { color: #e0e0e0; }");
    debugLabel->setAlignment(Qt::AlignCenter);
    debugLayout->addWidget(debugLabel);
    m_sidebarStack->addWidget(debugView);
    
    // Create Extensions view (placeholder)
    QWidget* extView = new QWidget(m_primarySidebar);
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
    
    editorTabs_ = new QTabWidget(editorFrame);
    editorTabs_->setStyleSheet(
        "QTabBar { background-color: #252526; }"
        "QTabBar::tab { background-color: #1e1e1e; color: #e0e0e0; padding: 8px; margin: 0px; border: 1px solid #3e3e42; }"
        "QTabBar::tab:selected { background-color: #252526; border-bottom: 2px solid #007acc; }"
        "QTabWidget::pane { border: none; }"
    );
    
    codeView_ = new QTextEdit(editorFrame);
    codeView_->setStyleSheet("QTextEdit { background-color: #1e1e1e; color: #e0e0e0; font-family: 'Consolas', monospace; font-size: 11pt; }");
    codeView_->setLineWrapMode(QTextEdit::NoWrap);
    editorTabs_->addTab(codeView_, "Untitled");
    
    editorLayout->addWidget(editorTabs_, 1);
    
    centerSplitter->addWidget(editorFrame);
    centerSplitter->setStretchFactor(0, 0);  // Sidebar doesn't stretch
    centerSplitter->setStretchFactor(1, 1);  // Editor stretches
    
    mainLayout->addWidget(centerSplitter, 1);
    
    // ============= BOTTOM: Panel Dock (Terminal/Output/Problems/Debug) =============
    m_bottomPanel = new QFrame(mainContainer);
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
    m_panelStack = new QStackedWidget(m_bottomPanel);
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
    QWidget* problemsView = new QWidget(m_bottomPanel);
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
    m_hexMagConsole = new QPlainTextEdit(m_bottomPanel);
    m_hexMagConsole->setStyleSheet(
        "QPlainTextEdit { background-color: #1e1e1e; color: #0dff00; "
        "font-family: 'Consolas', monospace; font-size: 10pt; }");
    m_hexMagConsole->appendPlainText("HexMag inference console ready...");
    m_panelStack->addWidget(m_hexMagConsole);        // index 4
    
    panelLayout->addWidget(m_panelStack, 1);
    
    // ============= Connect Activity Bar to Sidebar Views =============
    if (m_activityBar) {
        connect(m_activityBar, &ActivityBar::viewChanged, this, [this](ActivityBar::ViewType view) {
            m_sidebarStack->setCurrentIndex(static_cast<int>(view));
            // Update sidebar header label
            const char* titles[] = {"Explorer", "Search", "Source Control", "Run and Debug", "Extensions"};
            // Update the header label (would need to store it as member)
        });
    }
    
    // ============= Create Vertical Splitter (Editor + Panel) =============
    QSplitter* verticalSplitter = new QSplitter(Qt::Vertical, mainContainer);
    verticalSplitter->setOpaqueResize(true);
    verticalSplitter->addWidget(mainLayout->takeAt(0)->widget());  // Adjust layout if needed
    
    // Better approach: Create a proper vertical splitter at the root
    QWidget* centerWidget = new QWidget(this);
    QVBoxLayout* centerLayout = new QVBoxLayout(centerWidget);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);
    
    QSplitter* vertSplitter = new QSplitter(Qt::Vertical, centerWidget);
    vertSplitter->setOpaqueResize(true);
    vertSplitter->setStyleSheet("QSplitter::handle { background-color: #2d2d2d; height: 4px; }");
    
    // Create horizontal splitter for activity bar + sidebar + editor
    QWidget* topWidget = new QWidget(centerWidget);
    topWidget->setLayout(mainLayout);
    
    vertSplitter->addWidget(topWidget);
    vertSplitter->addWidget(m_bottomPanel);
    vertSplitter->setStretchFactor(0, 1);  // Top stretches
    vertSplitter->setStretchFactor(1, 0);  // Bottom doesn't stretch initially
    
    centerLayout->addWidget(vertSplitter);
    setCentralWidget(centerWidget);
    
    // Connect panel buttons
    connect(panelCloseBtn, &QPushButton::clicked, this, [this]() {
        m_bottomPanel->hide();
    });
    
    connect(panelMinBtn, &QPushButton::clicked, this, [this]() {
        m_bottomPanel->setFixedHeight(m_bottomPanel->height() > 50 ? 35 : 200);
    });
    
    // Connect terminal tab buttons
    connect(terminalTabBtn, &QPushButton::clicked, this, [this]() { m_panelStack->setCurrentIndex(0); });
    connect(outputTabBtn, &QPushButton::clicked, this, [this]() { m_panelStack->setCurrentIndex(1); });
    connect(problemsTabBtn, &QPushButton::clicked, this, [this]() { m_panelStack->setCurrentIndex(2); });
    connect(debugTabBtn, &QPushButton::clicked, this, [this]() { 
        if (m_hexMagConsole) m_panelStack->setCurrentWidget(m_hexMagConsole); 
        else m_panelStack->setCurrentIndex(3); 
    });
}

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
    // Stub for state management
    (void)state;
}

void MainWindow::setupMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("&New"), this, &MainWindow::handleNewEditor, QKeySequence::New);
    fileMenu->addAction(tr("&Open..."), this, &MainWindow::handleNewWindow, QKeySequence::Open);
    fileMenu->addAction(tr("&Save"), this, &MainWindow::handleSaveState, QKeySequence::Save);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("E&xit"), qApp, &QApplication::quit, QKeySequence::Quit);

    QMenu* editMenu = menuBar()->addMenu(tr("&Edit"));
    editMenu->addAction(tr("Cu&t"), QKeySequence::Cut);
    editMenu->addAction(tr("&Copy"), QKeySequence::Copy);
    editMenu->addAction(tr("&Paste"), QKeySequence::Paste);

    QMenu* viewMenu = menuBar()->addMenu(tr("&View"));
    
    // AI Orchestration
    QAction* orchestrationAct = viewMenu->addAction(tr("Task Orchestration"), this, [this](bool checked) {
        if (checked && !m_orchestrationDock) {
            setupOrchestrationSystem();
        } else if (m_orchestrationDock) {
            m_orchestrationDock->setVisible(checked);
        }
    });
    orchestrationAct->setCheckable(true);
    if (m_orchestrationDock) {
        orchestrationAct->setChecked(m_orchestrationDock->isVisible());
        connect(m_orchestrationDock, &QDockWidget::visibilityChanged, orchestrationAct, &QAction::setChecked);
    }
    
    // Main panels with checkbox sync
    QAction* projExplAct = viewMenu->addAction(tr("Project Explorer"), this, &MainWindow::toggleProjectExplorer);
    projExplAct->setCheckable(true);
    projExplAct->setChecked(false);  // Initially unchecked
    
    viewMenu->addAction(tr("Build System"), this, &MainWindow::toggleBuildSystem)->setCheckable(true);
    viewMenu->addAction(tr("Version Control"), this, &MainWindow::toggleVersionControl)->setCheckable(true);
    viewMenu->addAction(tr("Run & Debug"), this, &MainWindow::toggleRunDebug)->setCheckable(true);
    
    QAction* aiChatAct = viewMenu->addAction(tr("AI Chat Panel"), this, [this](bool checked) {
        if (m_aiChatPanelDock) {
            m_aiChatPanelDock->setVisible(checked);
        }
    });
    aiChatAct->setCheckable(true);
    if (m_aiChatPanelDock) {
        aiChatAct->setChecked(m_aiChatPanelDock->isVisible());
        connect(m_aiChatPanelDock, &QDockWidget::visibilityChanged, aiChatAct, &QAction::setChecked);
    }
    
    QAction* masmAct = viewMenu->addAction(tr("MASM Editor"), this, [this](bool checked) {
        if (m_masmEditorDock) {
            m_masmEditorDock->setVisible(checked);
        }
    });
    masmAct->setCheckable(true);
    if (m_masmEditorDock) {
        masmAct->setChecked(m_masmEditorDock->isVisible());
        connect(m_masmEditorDock, &QDockWidget::visibilityChanged, masmAct, &QAction::setChecked);
    }
    
    QAction* hotpatchAct = viewMenu->addAction(tr("Hotpatch Panel"), this, [this](bool checked) {
        if (m_hotpatchPanelDock) {
            m_hotpatchPanelDock->setVisible(checked);
        }
    });
    hotpatchAct->setCheckable(true);
    if (m_hotpatchPanelDock) {
        hotpatchAct->setChecked(m_hotpatchPanelDock->isVisible());
        connect(m_hotpatchPanelDock, &QDockWidget::visibilityChanged, hotpatchAct, &QAction::setChecked);
    }
    
    QAction* layerQuantAct = viewMenu->addAction(tr("Layer Quantization"), this, [this](bool checked) {
        if (m_layerQuantDock) {
            m_layerQuantDock->setVisible(checked);
        }
    });
    layerQuantAct->setCheckable(true);
    if (m_layerQuantDock) {
        layerQuantAct->setChecked(m_layerQuantDock->isVisible());
        connect(m_layerQuantDock, &QDockWidget::visibilityChanged, layerQuantAct, &QAction::setChecked);
    }
    
    QAction* interpretabilityAct = viewMenu->addAction(tr("Model Interpretability"), this, [this](bool checked) {
        if (m_interpretabilityPanelDock) {
            m_interpretabilityPanelDock->setVisible(checked);
        } else if (checked) {
            setupInterpretabilityPanel();
        }
    });
    interpretabilityAct->setCheckable(true);
    if (m_interpretabilityPanelDock) {
        interpretabilityAct->setChecked(m_interpretabilityPanelDock->isVisible());
        connect(m_interpretabilityPanelDock, &QDockWidget::visibilityChanged, interpretabilityAct, &QAction::setChecked);
    }
    
    viewMenu->addAction(tr("Terminal Cluster"), this, &MainWindow::toggleTerminalCluster)->setCheckable(true);
    viewMenu->addSeparator();
    
    // Model Monitor
    QAction* monAct = viewMenu->addAction(tr("Model Monitor"));
    monAct->setCheckable(true);
    if (m_modelMonitorDock) {
        monAct->setChecked(m_modelMonitorDock->isVisible());
        connect(m_modelMonitorDock, &QDockWidget::visibilityChanged, monAct, &QAction::setChecked);
    }
    connect(monAct, &QAction::toggled, this, [this](bool on){
        if (on && !m_modelMonitorDock) {
            m_modelMonitorDock = new QDockWidget(tr("Model Monitor"), this);
            ModelMonitor* monitor = new ModelMonitor(m_inferenceEngine, m_modelMonitorDock);
            monitor->initialize();  // Two-phase init - create Qt widgets after QApplication
            m_modelMonitorDock->setWidget(monitor);
            addDockWidget(Qt::RightDockWidgetArea, m_modelMonitorDock);
        } else if (m_modelMonitorDock) {
            m_modelMonitorDock->setVisible(on);
        }
    });

    // AI/GGUF menu with brutal_gzip integration
    QMenu* aiMenu = menuBar()->addMenu(tr("&AI"));
    aiMenu->addAction(tr("Load GGUF Model..."), this, &MainWindow::loadGGUFModel);
    aiMenu->addAction(tr("Run Inference..."), this, &MainWindow::runInference);
    aiMenu->addAction(tr("Unload Model"), this, &MainWindow::unloadGGUFModel);
    aiMenu->addSeparator();
    
    // Streaming mode toggle
    QAction* streamAct = aiMenu->addAction(tr("Streaming Mode"));
    streamAct->setCheckable(true);
    connect(streamAct, &QAction::toggled, this, [this](bool on){
        m_streamingMode = on;
        statusBar()->showMessage(on ? tr("Streaming inference ON")
                                    : tr("Streaming inference OFF"), 2000);
    });
    
    // Batch compress folder
    aiMenu->addSeparator();
    QAction* batchAct = aiMenu->addAction(tr("Batch Compress Folder..."));
    connect(batchAct, &QAction::triggered, this, &MainWindow::batchCompressFolder);
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
        QAction* action = agentMenu->addAction(QString::fromUtf8(mode.label));
        action->setCheckable(true);
        action->setData(QString::fromUtf8(mode.id));
        agentModeGroup->addAction(action);
        if (QString::fromUtf8(mode.id) == m_agentMode) {
            action->setChecked(true);
        }
    }
    connect(agentModeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        changeAgentMode(action->data().toString());
    });

    QMenu* modelMenu = menuBar()->addMenu(tr("&Model"));
    modelMenu->addAction(tr("Load Local GGUF..."), this, &MainWindow::loadGGUFModel);
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
        QString backendId = QString::fromUtf8(backend.id);
        QAction* backendAction = modelMenu->addAction(QString::fromUtf8(backend.label));
        backendAction->setCheckable(true);
        backendAction->setData(backendId);
        m_backendGroup->addAction(backendAction);
        if (backendId == m_currentBackend) {
            backendAction->setChecked(true);
        }
    }
    connect(m_backendGroup, &QActionGroup::triggered, this, &MainWindow::handleBackendSelection);

    modelMenu->addSeparator();
    modelMenu->addAction(tr("Manage Backends..."), this, &MainWindow::setupAIBackendSwitcher);

    QMenu* helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("&About"), this, &MainWindow::onAbout);
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
    
    connect(m_modelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx <= 0) return;  // Skip "No model loaded" and separators
        QString modelPath = m_modelSelector->itemData(idx).toString();
        if (!modelPath.isEmpty() && modelPath != "LOAD") {
            // Direct model selection - would need to implement overload or set path first
            loadGGUFModel();
        } else if (modelPath == "LOAD") {
            loadGGUFModel();  // File dialog
        }
    });
    
    toolbar->addSeparator();
    
    // Agent mode switcher
    m_agentModeSwitcher = new QComboBox(toolbar);
    m_agentModeSwitcher->setToolTip(tr("Switch agentic mode"));
    m_agentModeSwitcher->addItem(tr("Plan Mode"), QStringLiteral("Plan"));
    m_agentModeSwitcher->addItem(tr("Agent Mode"), QStringLiteral("Agent"));
    m_agentModeSwitcher->addItem(tr("Ask Mode"), QStringLiteral("Ask"));
    toolbar->addWidget(m_agentModeSwitcher);
    connect(m_agentModeSwitcher, &QComboBox::currentTextChanged, this, [this](const QString&) {
        if (!m_agentModeSwitcher) return;
        QVariant data = m_agentModeSwitcher->currentData();
        if (data.isValid()) changeAgentMode(data.toString());
    });
    changeAgentMode(m_agentMode); // sync UI state
}

void MainWindow::changeAgentMode(const QString& mode)
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
    statusBar()->showMessage(tr("Agent mode set to %1").arg(mode), 2000);
}

void MainWindow::handleBackendSelection(QAction* action)
{
    if (!action) return;
    QString backendId = action->data().toString();
    if (backendId.isEmpty() || backendId == m_currentBackend) return;
    m_currentBackend = backendId;
    statusBar()->showMessage(tr("Backend switched to %1").arg(action->text()), 2000);
    onAIBackendChanged(backendId, {});
}

void MainWindow::createCentralEditor()
{
    QWidget* central = new QWidget(this);
    QVBoxLayout* layout = new QVBoxLayout(central);
    
    editorTabs_ = new QTabWidget(central);
    codeView_ = new QTextEdit();
    editorTabs_->addTab(codeView_, "Untitled");
    
    layout->addWidget(editorTabs_);
    setCentralWidget(central);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Ready | ggml Q4_0/Q8_0 quantization available"));
}

void MainWindow::initSubsystems()
{
    // Initialize all subsystems - stubs for now
}

// ============================================================
// Real Agent System Implementations (replacing stubs)
// ============================================================

void MainWindow::handleGoalSubmit() {
    if (!goalInput_) return;
    
    QString wish = goalInput_->text().trimmed();
    if (wish.isEmpty()) {
        statusBar()->showMessage(tr("Please enter a goal/wish"), 2000);
        return;
    }
    
    // Use MetaPlanner to convert wish to action plan
    MetaPlanner planner;
    QJsonArray plan = planner.plan(wish);
    
    if (plan.isEmpty()) {
        statusBar()->showMessage(tr("Failed to generate plan"), 3000);
        return;
    }
    
    // Display plan summary
    if (chatHistory_) {
        chatHistory_->addItem(tr("Goal: %1").arg(wish));
        chatHistory_->addItem(tr("Plan: %1 actions generated").arg(plan.size()));
    }
    
    statusBar()->showMessage(tr("Executing plan with %1 actions...").arg(plan.size()), 3000);
    
    // Execute plan via ActionExecutor
    if (!m_actionExecutor) {
        m_actionExecutor = new ActionExecutor(this);
        connect(m_actionExecutor, &ActionExecutor::actionStarted,
                this, &MainWindow::onActionStarted);
        connect(m_actionExecutor, &ActionExecutor::actionCompleted,
                this, &MainWindow::onActionCompleted);
        connect(m_actionExecutor, &ActionExecutor::planCompleted,
                this, &MainWindow::onPlanCompleted);
    }
    
    ExecutionContext ctx;
    ctx.projectRoot = QDir::currentPath();
    m_actionExecutor->setContext(ctx);
    m_actionExecutor->executePlan(plan);
    
    emit onGoalSubmitted(wish);
}

void MainWindow::handleAgentMockProgress() {
    // Progress tracking - update UI with agent execution status
    if (mockStatusBadge_) {
        mockStatusBadge_->setText(tr("Agent Running..."));
    }
    statusBar()->showMessage(tr("Agent making progress..."), 1000);
}
void MainWindow::updateSuggestion(const QString& chunk) {
    suggestionBuffer_ += chunk;
    
    // Update AI suggestion overlay if it exists
    if (overlay_) {
        // overlay_->updateText(suggestionBuffer_);
    }
    
    // Also stream to AI chat panel
    if (m_aiChatPanel) {
        m_aiChatPanel->updateStreamingMessage(chunk);
    }
}

void MainWindow::appendModelChunk(const QString& chunk) {
    architectBuffer_ += chunk;
    
    // Append to hex mag console for model output
    if (m_hexMagConsole) {
        m_hexMagConsole->insertPlainText(chunk);
        m_hexMagConsole->ensureCursorVisible();
    }
}

void MainWindow::handleGenerationFinished() {
    suggestionEnabled_ = true;
    
    if (m_aiChatPanel) {
        m_aiChatPanel->finishStreaming();
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("\n--- Generation Complete ---\n");
    }
    
    statusBar()->showMessage(tr("AI generation complete"), 3000);
}
void MainWindow::handleQShellReturn() {
    if (!qshellInput_ || !qshellOutput_) return;
    
    QString command = qshellInput_->text().trimmed();
    if (command.isEmpty()) return;
    
    qshellOutput_->append(">> " + command);
    qshellInput_->clear();
    
    // Execute as agent wish via MetaPlanner
    MetaPlanner planner;
    QJsonArray plan = planner.plan(command);
    
    if (!plan.isEmpty() && m_actionExecutor) {
        ExecutionContext ctx;
        ctx.projectRoot = QDir::currentPath();
        m_actionExecutor->setContext(ctx);
        m_actionExecutor->executePlan(plan);
    } else {
        qshellOutput_->append("Error: Failed to parse command as agent wish");
    }
}
void MainWindow::handleArchitectChunk(const QString& chunk) {
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
            item->setText(tr("Architect: %1").arg(architectBuffer_));
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
    QJsonDocument doc = QJsonDocument::fromJson(architectBuffer_.toUtf8());
    if (doc.isArray()) {
        QJsonArray plan = doc.array();
        if (chatHistory_) {
            chatHistory_->addItem(tr("✓ Architect plan ready: %1 actions").arg(plan.size()));
        }
        
        // Auto-execute the plan
        if (m_actionExecutor) {
            ExecutionContext ctx;
            ctx.projectRoot = QDir::currentPath();
            m_actionExecutor->setContext(ctx);
            m_actionExecutor->executePlan(plan);
        }
    }
    
    architectBuffer_.clear();
    statusBar()->showMessage(tr("Architect planning complete"), 3000);
}
void MainWindow::onActionStarted(int index, const QString& description) {
    handleTaskStatusUpdate(QString::number(index), description, QStringLiteral("Agent"));
}

void MainWindow::onActionCompleted(int index, bool success, const QJsonObject& result) {
    QString summary = QJsonDocument(result).toJson(QJsonDocument::Compact);
    QString status = success ? QStringLiteral("Completed") : QStringLiteral("Failed");
    handleTaskStatusUpdate(QString::number(index), status, QStringLiteral("Agent"));
    handleTaskCompleted(QStringLiteral("Agent"), summary);
}

void MainWindow::onPlanCompleted(bool success, const QJsonObject& result) {
    Q_UNUSED(result);
    handleWorkflowFinished(success);
}

void MainWindow::handleTaskStatusUpdate(const QString& taskId, const QString& status, const QString& agentType) {
    QString msg = tr("[%1] %2: %3").arg(agentType, taskId, status);
    
    if (chatHistory_) {
        chatHistory_->addItem(msg);
        chatHistory_->scrollToBottom();
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(msg);
    }
    
    statusBar()->showMessage(msg, 2000);
}

void MainWindow::handleTaskCompleted(const QString& agentType, const QString& summary) {
    QString msg = tr("✓ %1 completed: %2").arg(agentType, summary);
    
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
    QString msg = success ? tr("✓✓✓ Workflow completed successfully!")
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

void MainWindow::handleTaskStreaming(const QString& taskId, const QString& chunk, const QString& agentType) {
    // Real-time streaming of task execution output
    if (m_hexMagConsole) {
        m_hexMagConsole->insertPlainText(chunk);
        m_hexMagConsole->ensureCursorVisible();
    }
    
    // Update task-specific widget if exists
    QString key = agentType + ":" + taskId;
    if (proposalItemMap_.contains(key)) {
        QListWidgetItem* item = proposalItemMap_[key];
        if (item) {
            QString currentText = item->text();
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
    
    qDebug() << "UI state saved successfully";
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
        QString savedModel = settings.value("ModelSelector/currentText").toString();
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
        QString savedMode = settings.value("AgentMode/current").toString();
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
    
    qDebug() << "UI state restored successfully";
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
        int index = editorTabs_->addTab(newEditor, tr("Untitled %1").arg(editorTabs_->count()));
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
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Add File to Project"),
        QString(),
        tr("All Files (*.*)"));
    
    if (!filePath.isEmpty()) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            QString content = in.readAll();
            file.close();
            
            if (editorTabs_) {
                QTextEdit* editor = new QTextEdit(this);
                editor->setStyleSheet(codeView_->styleSheet());
                editor->setText(content);
                int index = editorTabs_->addTab(editor, QFileInfo(filePath).fileName());
                editorTabs_->setCurrentIndex(index);
            }
            
            statusBar()->showMessage(tr("File added: %1").arg(QFileInfo(filePath).fileName()), 3000);
        }
    }
}

void MainWindow::handleAddFolder() {
    QString folderPath = QFileDialog::getExistingDirectory(
        this,
        tr("Add Folder to Project"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    
    if (!folderPath.isEmpty()) {
        if (projectExplorer_) {
            projectExplorer_->openProject(folderPath);
        }
        statusBar()->showMessage(tr("Folder added: %1").arg(folderPath), 3000);
    }
}

void MainWindow::handleAddSymbol() {
    bool ok;
    QString symbol = QInputDialog::getText(this, tr("Add Symbol"),
                                          tr("Symbol name:"), QLineEdit::Normal,
                                          QString(), &ok);
    if (ok && !symbol.isEmpty()) {
        if (contextList_) {
            contextList_->addItem(symbol);
        }
        statusBar()->showMessage(tr("Symbol added: %1").arg(symbol), 2000);
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
    
    QString itemText = item->text();
    
    // Check if it's a file path
    if (QFile::exists(itemText)) {
        QFile file(itemText);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QTextStream in(&file);
            if (codeView_) {
                codeView_->setText(in.readAll());
                statusBar()->showMessage(tr("Loaded: %1").arg(itemText), 3000);
            }
            file.close();
        }
    } else {
        // Use as search/symbol query
        statusBar()->showMessage(tr("Context item: %1").arg(itemText), 2000);
    }
}

void MainWindow::handleTabClose(int index) {
    if (!editorTabs_ || index < 0 || index >= editorTabs_->count()) return;
    
    QWidget* widget = editorTabs_->widget(index);
    
    // Ask for confirmation if content exists
    QTextEdit* editor = qobject_cast<QTextEdit*>(widget);
    if (editor && !editor->toPlainText().isEmpty()) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this,
            tr("Close Tab"),
            tr("Close '%1'? Unsaved changes will be lost.").arg(editorTabs_->tabText(index)),
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (reply == QMessageBox::No) {
            return;
        }
    }
    
    editorTabs_->removeTab(index);
    delete widget;
}
void MainWindow::handlePwshCommand() { statusBar()->showMessage(tr("PowerShell executing...")); }
void MainWindow::handleCmdCommand() { statusBar()->showMessage(tr("CMD executing...")); }
void MainWindow::readPwshOutput() { qDebug() << "Reading PowerShell output"; }
void MainWindow::readCmdOutput() { qDebug() << "Reading CMD output"; }
void MainWindow::clearDebugLog() { if (m_hexMagConsole) m_hexMagConsole->clear(); statusBar()->showMessage(tr("Debug log cleared"), 2000); }
void MainWindow::saveDebugLog() { statusBar()->showMessage(tr("Saving debug log...")); }
void MainWindow::filterLogLevel(const QString& level) { statusBar()->showMessage(tr("Filtering by: %1").arg(level), 2000); }
void MainWindow::showEditorContextMenu(const QPoint& pos) { qDebug() << "Context menu at" << pos; }
void MainWindow::explainCode() 
{ 
    QString sel = codeView_->textCursor().selectedText(); 
    if (sel.isEmpty()) {
        statusBar()->showMessage(tr("Select code first"), 2000);
        return;
    }
    
    if (m_aiChatPanel) {
        QString prompt = tr("Explain this code in detail:\n\n```\n%1\n```").arg(sel);
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
    QString sel = codeView_->textCursor().selectedText(); 
    if (sel.isEmpty()) {
        statusBar()->showMessage(tr("Select code first"), 2000);
        return;
    }
    
    if (m_aiChatPanel) {
        QString prompt = tr("Find and fix any bugs or issues in this code:\n\n```\n%1\n```\n\nProvide the corrected code.").arg(sel);
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
    QString sel = codeView_->textCursor().selectedText(); 
    if (sel.isEmpty()) {
        statusBar()->showMessage(tr("Select code first"), 2000);
        return;
    }
    
    if (m_aiChatPanel) {
        QString prompt = tr("Refactor this code to be more efficient, readable, and follow best practices:\n\n```\n%1\n```").arg(sel);
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
    QString sel = codeView_->textCursor().selectedText(); 
    if (sel.isEmpty()) {
        statusBar()->showMessage(tr("Select code first"), 2000);
        return;
    }
    
    if (m_aiChatPanel) {
        QString prompt = tr("Generate comprehensive unit tests for this code:\n\n```\n%1\n```\n\nInclude edge cases and error handling tests.").arg(sel);
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
    QString sel = codeView_->textCursor().selectedText();
    if (sel.isEmpty()) {
        sel = codeView_->toPlainText(); // Use entire file if nothing selected
    }
    
    if (m_aiChatPanel) {
        QString prompt = tr("Generate comprehensive documentation for this code:\n\n```\n%1\n```\n\nInclude function descriptions, parameter docs, and usage examples.").arg(sel);
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
void MainWindow::onProjectOpened(const QString& path) {
    qDebug() << "[PROJECT] Opened:" << path;
    
    statusBar()->showMessage(tr("Project: %1").arg(path), 5000);
    
    // Update project explorer if available
    if (projectExplorer_) {
        projectExplorer_->openProject(path);
    }
    
    // Log to hex console
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[PROJECT] Opened: %1").arg(path));
    }
    
    // Update chat history
    if (chatHistory_) {
        chatHistory_->addItem(tr("📁 Project opened: %1").arg(QFileInfo(path).fileName()));
    }
}

void MainWindow::onBuildStarted() {
    qDebug() << "[BUILD] Build started at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Build started..."));
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("\n=== BUILD STARTED ===");
        m_hexMagConsole->appendPlainText(QDateTime::currentDateTime().toString(Qt::ISODate));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔨 Build started..."));
    }
}

void MainWindow::onBuildFinished(bool success) {
    qDebug() << "[BUILD] Finished:" << (success ? "SUCCESS" : "FAILED");
    
    statusBar()->showMessage(success ? tr("Build OK") : tr("Build FAILED"), 3000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(success ? "=== BUILD SUCCESS ===" : "=== BUILD FAILED ===");
        m_hexMagConsole->appendPlainText(QDateTime::currentDateTime().toString(Qt::ISODate));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(success ? tr("✅ Build successful") : tr("❌ Build failed"));
    }
    
    // Show notification for failures
    if (!success) {
        QMessageBox::warning(this, tr("Build Failed"), 
                           tr("Build process failed. Check console for details."));
    }
}

void MainWindow::onVcsStatusChanged() {
    qDebug() << "[VCS] Status changed at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("VCS updated"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[VCS] Status changed");
    }
}

void MainWindow::onDebuggerStateChanged(bool running) {
    qDebug() << "[DEBUGGER] State:" << (running ? "RUNNING" : "STOPPED");
    
    statusBar()->showMessage(running ? tr("Debugger ON") : tr("Debugger OFF"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(running ? "[DEBUGGER] Started" : "[DEBUGGER] Stopped");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(running ? tr("🐛 Debugger started") : tr("🐛 Debugger stopped"));
    }
}

void MainWindow::onTestRunStarted() {
    qDebug() << "[TEST] Test run started at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Running tests..."));
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("\n=== TEST RUN STARTED ===");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🧪 Running tests..."));
    }
}

void MainWindow::onTestRunFinished() {
    qDebug() << "[TEST] Test run finished at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Tests done"), 3000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("=== TEST RUN COMPLETE ===");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("✅ Tests complete"));
    }
}
void MainWindow::onDatabaseConnected() {
    qDebug() << "[DATABASE] Connected at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("DB connected"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[DATABASE] Connection established");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("💾 Database connected"));
    }
}

void MainWindow::onDockerContainerListed() {
    qDebug() << "[DOCKER] Containers listed at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Docker ready"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[DOCKER] Container list refreshed");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🐳 Docker containers listed"));
    }
}

void MainWindow::onCloudResourceListed() {
    qDebug() << "[CLOUD] Resources listed at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Cloud resources loaded"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[CLOUD] Resource list updated");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("☁️ Cloud resources loaded"));
    }
}

void MainWindow::onPackageInstalled(const QString& pkg) {
    qDebug() << "[PACKAGE] Installed:" << pkg;
    
    statusBar()->showMessage(tr("Package: %1").arg(pkg), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[PACKAGE] Installed: %1").arg(pkg));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("📦 Package installed: %1").arg(pkg));
    }
}
void MainWindow::onDocumentationQueried(const QString& keyword) {
    qDebug() << "[DOCS] Query:" << keyword;
    
    statusBar()->showMessage(tr("Searching: %1").arg(keyword), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[DOCS] Searching for: %1").arg(keyword));
    }
    
    // Could integrate with AI chat to search documentation
    if (m_aiChatPanel && !keyword.isEmpty()) {
        QString prompt = tr("Show documentation for: %1").arg(keyword);
        m_aiChatPanel->addUserMessage(prompt);
    }
}

void MainWindow::onUMLGenerated(const QString& plantUml) {
    qDebug() << "[UML] Generated, length:" << plantUml.length() << "chars";
    
    statusBar()->showMessage(tr("UML generated"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[UML] Diagram generated");
        m_hexMagConsole->appendPlainText(plantUml.left(200) + "..."); // First 200 chars
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("📊 UML diagram generated"));
    }
}

void MainWindow::onImageEdited(const QString& path) {
    qDebug() << "[IMAGE] Edited:" << path;
    
    statusBar()->showMessage(tr("Image: %1").arg(QFileInfo(path).fileName()), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[IMAGE] Edited: %1").arg(path));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🖼️ Image edited: %1").arg(QFileInfo(path).fileName()));
    }
}

void MainWindow::onTranslationChanged(const QString& lang) {
    qDebug() << "[TRANSLATION] Language changed to:" << lang;
    
    statusBar()->showMessage(tr("Language: %1").arg(lang), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[TRANSLATION] Language: %1").arg(lang));
    }
    
    // Could trigger QApplication locale change here
    if (chatHistory_) {
        chatHistory_->addItem(tr("🌐 Language changed: %1").arg(lang));
    }
}

void MainWindow::onDesignImported(const QString& file) {
    qDebug() << "[DESIGN] Imported:" << file;
    
    statusBar()->showMessage(tr("Design from %1").arg(QFileInfo(file).fileName()), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[DESIGN] Imported: %1").arg(file));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🎨 Design imported: %1").arg(QFileInfo(file).fileName()));
    }
}
void MainWindow::onAIChatMessage(const QString& msg) {
    qDebug() << "[AI_CHAT] Message received, length:" << msg.length();
    
    if (m_aiChatPanel) {
        statusBar()->showMessage(tr("AI Chat: message received"), 2000);
        
        // Show AI chat panel if hidden
        if (m_aiChatPanelDock && !m_aiChatPanelDock->isVisible()) {
            m_aiChatPanelDock->show();
            m_aiChatPanelDock->raise();
        }
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[AI_CHAT] %1").arg(msg.left(100)));
    }
}

void MainWindow::onNotebookExecuted() {
    qDebug() << "[NOTEBOOK] Executed at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Notebook executed"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[NOTEBOOK] Cells executed");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("📓 Notebook executed"));
    }
}

void MainWindow::onMarkdownRendered() {
    qDebug() << "[MARKDOWN] Rendered at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Markdown rendered"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[MARKDOWN] Preview updated");
    }
}

void MainWindow::onSheetCalculated() {
    qDebug() << "[SPREADSHEET] Calculated at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Spreadsheet calculated"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[SPREADSHEET] Formulas recalculated");
    }
}

void MainWindow::onTerminalCommand(const QString& cmd) {
    qDebug() << "[TERMINAL] Command:" << cmd;
    
    statusBar()->showMessage(tr("Terminal: %1").arg(cmd.left(50)), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[TERMINAL] $ %1").arg(cmd));
    }
    
    // Could execute command via QProcess here
    if (chatHistory_) {
        chatHistory_->addItem(tr("💻 Terminal: %1").arg(cmd.left(50)));
    }
}
void MainWindow::onSnippetInserted(const QString& id) {
    qDebug() << "[SNIPPET] Inserted:" << id;
    
    statusBar()->showMessage(tr("Snippet: %1").arg(id), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[SNIPPET] Inserted: %1").arg(id));
    }
}

void MainWindow::onRegexTested(const QString& pattern) {
    qDebug() << "[REGEX] Testing pattern:" << pattern;
    
    statusBar()->showMessage(tr("Regex: %1").arg(pattern.left(30)), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[REGEX] Pattern: %1").arg(pattern));
    }
}

void MainWindow::onDiffMerged() {
    qDebug() << "[DIFF] Merge completed at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Diff merged"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[DIFF] Merge operation completed");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔀 Diff merged successfully"));
    }
}

void MainWindow::onColorPicked(const QColor& c) {
    qDebug() << "[COLOR] Picked:" << c.name() << "RGB:" << c.red() << c.green() << c.blue();
    
    statusBar()->showMessage(tr("Color: %1").arg(c.name()), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[COLOR] %1 (R:%2 G:%3 B:%4)")
            .arg(c.name()).arg(c.red()).arg(c.green()).arg(c.blue()));
    }
    
    // Could insert color into current editor
    if (codeView_ && codeView_->hasFocus()) {
        codeView_->insertPlainText(c.name());
    }
}

void MainWindow::onIconSelected(const QString& name) {
    qDebug() << "[ICON] Selected:" << name;
    
    statusBar()->showMessage(tr("Icon: %1").arg(name), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[ICON] Selected: %1").arg(name));
    }
}

void MainWindow::onPluginLoaded(const QString& name) {
    qDebug() << "[PLUGIN] Loaded:" << name;
    
    statusBar()->showMessage(tr("Plugin loaded: %1").arg(name), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[PLUGIN] Loaded: %1").arg(name));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔌 Plugin loaded: %1").arg(name));
    }
}

void MainWindow::onSettingsSaved() {
    qDebug() << "[SETTINGS] Saved at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Settings saved"), 2000);
    
    // Trigger our own save state
    handleSaveState();
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[SETTINGS] Configuration saved");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("⚙️ Settings saved"));
    }
}
void MainWindow::onNotificationClicked(const QString& id) {
    qDebug() << "[NOTIFICATION] Clicked:" << id;
    
    statusBar()->showMessage(tr("Notification: %1").arg(id), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[NOTIFICATION] User clicked: %1").arg(id));
    }
}

void MainWindow::onShortcutChanged(const QString& id, const QKeySequence& key) {
    qDebug() << "[SHORTCUT] Changed:" << id << "->" << key.toString();
    
    statusBar()->showMessage(tr("Shortcut %1: %2").arg(id, key.toString()), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[SHORTCUT] %1 = %2").arg(id, key.toString()));
    }
    
    // Save shortcuts to settings
    QSettings settings("RawrXD", "QtShell");
    settings.setValue(QString("Shortcuts/%1").arg(id), key.toString());
}

void MainWindow::onTelemetryReady() {
    qDebug() << "[TELEMETRY] System initialized at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[TELEMETRY] Observability system ready");
    }
}

void MainWindow::onUpdateAvailable(const QString& version) {
    qDebug() << "[UPDATE] New version available:" << version;
    
    statusBar()->showMessage(tr("Update available: %1").arg(version), 5000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[UPDATE] Version %1 available").arg(version));
    }
    
    // Show update dialog
    QMessageBox msgBox(this);
    msgBox.setWindowTitle(tr("Update Available"));
    msgBox.setText(tr("Version %1 is available for download.").arg(version));
    msgBox.setInformativeText(tr("Would you like to download it now?"));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);
    int ret = msgBox.exec();
    
    if (ret == QMessageBox::Yes) {
        // Could open browser to download page
        if (chatHistory_) {
            chatHistory_->addItem(tr("📥 Downloading update %1...").arg(version));
        }
    }
}

void MainWindow::onWelcomeProjectChosen(const QString& path) {
    qDebug() << "[WELCOME] Project chosen from welcome screen:" << path;
    onProjectOpened(path);
}

void MainWindow::onCommandPaletteTriggered(const QString& cmd) {
    qDebug() << "[COMMAND_PALETTE] Triggered:" << cmd;
    
    statusBar()->showMessage(tr("Command: %1").arg(cmd), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[CMD] %1").arg(cmd));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("⌨️ Command: %1").arg(cmd));
    }
}

void MainWindow::onProgressCancelled(const QString& taskId) {
    qDebug() << "[PROGRESS] Cancelled:" << taskId;
    
    statusBar()->showMessage(tr("Cancelled: %1").arg(taskId), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[PROGRESS] Cancelled: %1").arg(taskId));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("⏹️ Task cancelled: %1").arg(taskId));
    }
}
void MainWindow::onQuickFixApplied(const QString& fix) {
    qDebug() << "[QUICK_FIX] Applied:" << fix;
    
    statusBar()->showMessage(tr("Quick fix applied: %1").arg(fix.left(30)), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[QUICK_FIX] %1").arg(fix));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔧 Quick fix: %1").arg(fix.left(50)));
    }
}

void MainWindow::onMinimapClicked(qreal ratio) {
    qDebug() << "[MINIMAP] Clicked at ratio:" << ratio;
    
    statusBar()->showMessage(tr("Minimap: %1%").arg(int(ratio*100)), 1000);
    
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

void MainWindow::onBreadcrumbClicked(const QString& symbol) {
    qDebug() << "[BREADCRUMB] Navigate to:" << symbol;
    
    statusBar()->showMessage(tr("Navigate: %1").arg(symbol), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[NAV] %1").arg(symbol));
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

void MainWindow::onStatusFieldClicked(const QString& field) {
    qDebug() << "[STATUS_BAR] Field clicked:" << field;
    
    statusBar()->showMessage(tr("Status: %1").arg(field), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[STATUS] Clicked: %1").arg(field));
    }
}

void MainWindow::onTerminalEmulatorCommand(const QString& cmd) {
    qDebug() << "[TERMINAL_EMU] Command:" << cmd;
    
    statusBar()->showMessage(tr("Emulator: %1").arg(cmd.left(50)), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[EMU] $ %1").arg(cmd));
    }
}

void MainWindow::onSearchResultActivated(const QString& file, int line) {
    qDebug() << "[SEARCH] Opening:" << file << "at line" << line;
    
    statusBar()->showMessage(tr("Goto %1:%2").arg(QFileInfo(file).fileName()).arg(line), 2000);
    
    // Open file in editor
    QFile f(file);
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
        chatHistory_->addItem(tr("🔍 Opened: %1:%2").arg(QFileInfo(file).fileName()).arg(line));
    }
}

void MainWindow::onBookmarkToggled(const QString& file, int line) {
    qDebug() << "[BOOKMARK] Toggled:" << file << ":" << line;
    
    statusBar()->showMessage(tr("Bookmark: %1:%2").arg(QFileInfo(file).fileName()).arg(line), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[BOOKMARK] %1:%2").arg(file).arg(line));
    }
    
    // Save bookmark to settings
    QSettings settings("RawrXD", "QtShell");
    QString bookmarkKey = QString("Bookmarks/%1_%2").arg(file).arg(line);
    bool exists = settings.value(bookmarkKey, false).toBool();
    settings.setValue(bookmarkKey, !exists); // Toggle
}

void MainWindow::onTodoClicked(const QString& file, int line) {
    qDebug() << "[TODO] Clicked:" << file << ":" << line;
    
    statusBar()->showMessage(tr("TODO: %1:%2").arg(QFileInfo(file).fileName()).arg(line), 2000);
    
    // Open file at line (same as search result)
    onSearchResultActivated(file, line);
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("📝 TODO: %1:%2").arg(QFileInfo(file).fileName()).arg(line));
    }
}

void MainWindow::onMacroReplayed() {
    qDebug() << "[MACRO] Replayed at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Macro executed"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[MACRO] Playback complete");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🎬 Macro replayed"));
    }
}
void MainWindow::onCompletionCacheHit(const QString& key) {
    qDebug() << "[COMPLETION_CACHE] Hit:" << key << "at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    // Performance metric - cache is working
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[CACHE] Hit: %1").arg(key));
    }
}

void MainWindow::onLSPDiagnostic(const QString& file, const QJsonArray& diags) {
    int diagCount = diags.size();
    qDebug() << "[LSP] Diagnostics for" << file << ":" << diagCount << "issues";
    
    statusBar()->showMessage(tr("Diagnostics: %1 (%2 issues)").arg(QFileInfo(file).fileName()).arg(diagCount), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[LSP] %1: %2 diagnostics").arg(file).arg(diagCount));
        
        // Log first 3 diagnostics
        for (int i = 0; i < qMin(3, diagCount); ++i) {
            QJsonObject diag = diags[i].toObject();
            QString message = diag["message"].toString();
            int line = diag["line"].toInt();
            m_hexMagConsole->appendPlainText(QString("  Line %1: %2").arg(line).arg(message));
        }
    }
    
    if (chatHistory_ && diagCount > 0) {
        chatHistory_->addItem(tr("⚠️ %1 diagnostic issues in %2").arg(diagCount).arg(QFileInfo(file).fileName()));
    }
}

void MainWindow::onCodeLensClicked(const QString& command) {
    qDebug() << "[CODE_LENS] Clicked:" << command;
    
    statusBar()->showMessage(tr("CodeLens: %1").arg(command.left(50)), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[CODE_LENS] %1").arg(command));
    }
    
    // Could trigger command palette execution here
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔬 CodeLens: %1").arg(command.left(50)));
    }
}

void MainWindow::onInlayHintShown(const QString& file) {
    qDebug() << "[INLAY_HINT] Shown for:" << file;
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[INLAY] Hints active: %1").arg(file));
    }
}

void MainWindow::onInlineChatRequested(const QString& text) {
    qDebug() << "[INLINE_CHAT] Requested with text:" << text.left(100);
    
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
        chatHistory_->addItem(tr("💬 Inline chat: %1").arg(text.left(50)));
    }
}

void MainWindow::onAIReviewComment(const QString& comment) {
    qDebug() << "[AI_REVIEW] Comment:" << comment.left(100);
    
    statusBar()->showMessage(tr("AI review comment added"), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[AI_REVIEW] %1").arg(comment));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🤖 AI Review: %1").arg(comment.left(70)));
    }
}

void MainWindow::onCodeStreamEdit(const QString& patch) {
    qDebug() << "[CODE_STREAM] Edit received, patch size:" << patch.length();
    
    statusBar()->showMessage(tr("CodeStream sync: %1 bytes").arg(patch.length()), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[CODE_STREAM] Patch: %1 bytes").arg(patch.length()));
    }
    
    // Could apply patch to current editor here
    if (chatHistory_) {
        chatHistory_->addItem(tr("🔄 CodeStream sync"));
    }
}
void MainWindow::onAudioCallStarted() {
    qDebug() << "[AUDIO] Call started at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Audio call active"), 5000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[AUDIO] Call started");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("🎙️ Audio call active"));
    }
}

void MainWindow::onScreenShareStarted() {
    qDebug() << "[SCREEN_SHARE] Started at" << QDateTime::currentDateTime().toString(Qt::ISODate);
    
    statusBar()->showMessage(tr("Screen sharing active"), 5000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText("[SCREEN_SHARE] Broadcasting");
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("📺 Screen sharing started"));
    }
}

void MainWindow::onWhiteboardDraw(const QByteArray& svg) {
    qDebug() << "[WHITEBOARD] Drawing received, size:" << svg.size() << "bytes";
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[WHITEBOARD] SVG: %1 bytes").arg(svg.size()));
    }
    
    // Could render SVG in a dedicated widget
}

void MainWindow::onTimeEntryAdded(const QString& task) {
    qDebug() << "[TIME_TRACKING] Entry:" << task;
    
    statusBar()->showMessage(tr("Time logged: %1").arg(task), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[TIME] %1 @ %2")
            .arg(task)
            .arg(QDateTime::currentDateTime().toString("hh:mm:ss")));
    }
    
    // Save to settings for time tracking history
    QSettings settings("RawrXD", "QtShell");
    settings.setValue(QString("TimeTracking/%1").arg(QDateTime::currentMSecsSinceEpoch()), task);
}

void MainWindow::onKanbanMoved(const QString& taskId) {
    qDebug() << "[KANBAN] Task moved:" << taskId;
    
    statusBar()->showMessage(tr("Task: %1").arg(taskId), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[KANBAN] Moved: %1").arg(taskId));
    }
    
    if (chatHistory_) {
        chatHistory_->addItem(tr("📋 Task moved: %1").arg(taskId));
    }
}

void MainWindow::onPomodoroTick(int remaining) {
    // Only log every 5 seconds to avoid spam
    if (remaining % 5 == 0) {
        qDebug() << "[POMODORO] Remaining:" << remaining << "seconds";
    }
    
    statusBar()->showMessage(tr("Pomodoro: %1m %2s")
        .arg(remaining / 60)
        .arg(remaining % 60, 2, 10, QLatin1Char('0')), 1000);
    
    // Visual indicator when time is running out
    if (remaining <= 60 && remaining % 10 == 0) {
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(QString("[POMODORO] ⏰ %1 seconds remaining").arg(remaining));
        }
    }
}

void MainWindow::onWallpaperChanged(const QString& path) {
    qDebug() << "[THEME] Wallpaper changed:" << path;
    
    statusBar()->showMessage(tr("Theme updated: %1").arg(QFileInfo(path).fileName()), 2000);
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[THEME] Wallpaper: %1").arg(path));
    }
    
    // Could apply wallpaper to central widget background
    if (chatHistory_) {
        chatHistory_->addItem(tr("🎨 Theme changed"));
    }
}

void MainWindow::onAccessibilityToggled(bool on) {
    qDebug() << "[ACCESSIBILITY]" << (on ? "ENABLED" : "DISABLED");
    
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
            addDockWidget(Qt::RightDockWidgetArea, dock); \
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
            connect(projectExplorer_, &RawrXD::ProjectExplorerWidget::fileDoubleClicked,
                    this, [this](const QString& path) {
                QFile file(path);
                if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                    QTextStream in(&file);
                    if (codeView_) codeView_->setText(in.readAll());
                    file.close();
                    statusBar()->showMessage(tr("Opened: %1").arg(path), 3000);
                }
            });
            // Auto-open current directory or last project
            QString defaultPath = QDir::currentPath();
            if (QFile::exists("E:\\")) defaultPath = "E:\\";
            projectExplorer_->openProject(defaultPath);
        }
        QDockWidget* dock = new QDockWidget(tr("Project Explorer"), this);
        dock->setWidget(projectExplorer_);
        addDockWidget(Qt::LeftDockWidgetArea, dock);
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
bool MainWindow::eventFilter(QObject* watched, QEvent* event) 
{
    // Custom event filtering logic can be added here
    return QMainWindow::eventFilter(watched, event);
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

    for (const QUrl& u : mime->urls()) {
        QString path = u.toLocalFile();
        if (!path.endsWith(".gguf", Qt::CaseInsensitive)) {
            // Non-GGUF file - open in editor
            QFile file(path);
            if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                QTextStream in(&file);
                codeView_->setText(in.readAll());
                file.close();
            }
            continue;
        }

        // GGUF file - compress with brutal_gzip
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(this, tr("GGUF open"), tr("Cannot read %1").arg(path));
            continue;
        }
        QByteArray raw = f.readAll();          // whole file for demo
        f.close();
        
        QByteArray gz  = brutal::compress(raw);
        if (gz.isEmpty()) {
            QMessageBox::critical(this, tr("GGUF compress"), tr("Brutal deflate failed"));
            continue;
        }
        
        QString outName = path + ".gz";
        QFile og(outName);
        if (og.open(QIODevice::WriteOnly)) {
            og.write(gz);
            og.close();
            statusBar()->showMessage(
                tr("Compressed %1 → %2  (ratio %3%)")
                    .arg(QLocale().formattedDataSize(raw.size()))
                    .arg(QLocale().formattedDataSize(gz.size()))
                    .arg(QString::number(100.0 * gz.size() / raw.size(), 'f', 1)),
                5000);
        }
    }
    event->acceptProposedAction();
}

// ============================================================
// UI Creator Implementations
// ============================================================

QWidget* MainWindow::createGoalBar() {
    qDebug() << "[createGoalBar] Creating goal bar widget";
    
    try {
        QWidget* goalBar = new QWidget(this);
        goalBar->setObjectName("GoalBarWidget");
        goalBar->setStyleSheet("QWidget#GoalBarWidget { background-color: #252526; border-bottom: 1px solid #3e3e42; }");
        
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
        connect(submitBtn, &QPushButton::clicked, this, &MainWindow::handleGoalSubmit);
        connect(goalInput_, &QLineEdit::returnPressed, this, &MainWindow::handleGoalSubmit);
        
        qDebug() << "[createGoalBar] Goal bar created successfully";
        return goalBar;
        
    } catch (const std::exception& e) {
        qCritical() << "[createGoalBar] ERROR:" << e.what();
        return new QWidget(this);
    }
}

QWidget* MainWindow::createAgentPanel() {
    qDebug() << "[createAgentPanel] Creating agent control panel";
    
    try {
        QWidget* panel = new QWidget(this);
        panel->setObjectName("AgentPanel");
        panel->setStyleSheet("QWidget#AgentPanel { background-color: #252526; }");
        
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
        
        connect(agentSelector_, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int index) {
            QString mode = agentSelector_->itemText(index);
            qDebug() << "[AgentPanel] Mode changed to:" << mode;
            changeAgentMode(mode);
        });
        
        // Status badge
        QLabel* statusLabel = new QLabel("Status:", panel);
        statusLabel->setStyleSheet("QLabel { color: #e0e0e0; font-weight: bold; margin-top: 10px; }");
        layout->addWidget(statusLabel);
        
        mockStatusBadge_ = new QLabel("Idle", panel);
        mockStatusBadge_->setObjectName("StatusBadge");
        mockStatusBadge_->setAlignment(Qt::AlignCenter);
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
        
        qDebug() << "[createAgentPanel] Agent panel created successfully";
        return panel;
        
    } catch (const std::exception& e) {
        qCritical() << "[createAgentPanel] ERROR:" << e.what();
        return new QWidget(this);
    }
}

QWidget* MainWindow::createProposalReview() {
    qDebug() << "[createProposalReview] Creating proposal review panel";
    
    try {
        QWidget* panel = new QWidget(this);
        panel->setObjectName("ProposalReviewPanel");
        panel->setStyleSheet("QWidget#ProposalReviewPanel { background-color: #1e1e1e; }");
        
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
        
        qDebug() << "[createProposalReview] Proposal review panel created successfully";
        return panel;
        
    } catch (const std::exception& e) {
        qCritical() << "[createProposalReview] ERROR:" << e.what();
        return new QWidget(this);
    }
}

QWidget* MainWindow::createEditorArea() {
    qDebug() << "[createEditorArea] Creating central editor area";
    
    try {
        QWidget* editorWidget = new QWidget(this);
        editorWidget->setObjectName("EditorArea");
        editorWidget->setStyleSheet("QWidget#EditorArea { background-color: #1e1e1e; }");
        
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
        connect(editorTabs_, &QTabWidget::tabCloseRequested, this, &MainWindow::handleTabClose);
        
        layout->addWidget(editorTabs_);
        
        qDebug() << "[createEditorArea] Editor area created successfully";
        return editorWidget;
        
    } catch (const std::exception& e) {
        qCritical() << "[createEditorArea] ERROR:" << e.what();
        return new QWidget(this);
    }
}

QWidget* MainWindow::createQShellTab() {
    qDebug() << "[createQShellTab] Creating QShell interactive tab";
    
    try {
        QWidget* shellWidget = new QWidget(this);
        shellWidget->setObjectName("QShellTab");
        shellWidget->setStyleSheet("QWidget#QShellTab { background-color: #1e1e1e; }");
        
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
        QWidget* inputWidget = new QWidget(shellWidget);
        inputWidget->setStyleSheet("QWidget { background-color: #252526; border-top: 1px solid #3e3e42; }");
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
        connect(qshellInput_, &QLineEdit::returnPressed, this, &MainWindow::handleQShellReturn);
        connect(executeBtn, &QPushButton::clicked, this, &MainWindow::handleQShellReturn);
        
        qDebug() << "[createQShellTab] QShell tab created successfully";
        return shellWidget;
        
    } catch (const std::exception& e) {
        qCritical() << "[createQShellTab] ERROR:" << e.what();
        return new QWidget(this);
    }
}

QJsonDocument MainWindow::getMockArchitectJson() const {
    qDebug() << "[getMockArchitectJson] Generating mock architect plan";
    
    try {
        QJsonArray plan;
        
        // Task 1: Analyze requirements
        QJsonObject task1;
        task1["id"] = "task_001";
        task1["type"] = "analyze";
        task1["description"] = "Analyze project requirements and dependencies";
        task1["agent"] = "Architect";
        task1["status"] = "pending";
        task1["priority"] = "high";
        plan.append(task1);
        
        // Task 2: Design architecture
        QJsonObject task2;
        task2["id"] = "task_002";
        task2["type"] = "design";
        task2["description"] = "Design system architecture and component interfaces";
        task2["agent"] = "Architect";
        task2["status"] = "pending";
        task2["priority"] = "high";
        task2["depends_on"] = QJsonArray{"task_001"};
        plan.append(task2);
        
        // Task 3: Implement core features
        QJsonObject task3;
        task3["id"] = "task_003";
        task3["type"] = "implement";
        task3["description"] = "Implement core functionality according to design";
        task3["agent"] = "Coder";
        task3["status"] = "pending";
        task3["priority"] = "medium";
        task3["depends_on"] = QJsonArray{"task_002"};
        plan.append(task3);
        
        // Task 4: Write tests
        QJsonObject task4;
        task4["id"] = "task_004";
        task4["type"] = "test";
        task4["description"] = "Write comprehensive unit and integration tests";
        task4["agent"] = "Tester";
        task4["status"] = "pending";
        task4["priority"] = "medium";
        task4["depends_on"] = QJsonArray{"task_003"};
        plan.append(task4);
        
        // Task 5: Review and optimize
        QJsonObject task5;
        task5["id"] = "task_005";
        task5["type"] = "review";
        task5["description"] = "Code review, optimization, and documentation";
        task5["agent"] = "Reviewer";
        task5["status"] = "pending";
        task5["priority"] = "low";
        task5["depends_on"] = QJsonArray{"task_004"};
        plan.append(task5);
        
        QJsonDocument doc(plan);
        qDebug() << "[getMockArchitectJson] Generated plan with" << plan.size() << "tasks";
        
        return doc;
        
    } catch (const std::exception& e) {
        qCritical() << "[getMockArchitectJson] ERROR:" << e.what();
        return QJsonDocument();
    }
}

void MainWindow::populateFolderTree(QTreeWidgetItem* parent, const QString& path) {
    qDebug() << "[populateFolderTree] Populating tree for path:" << path;
    
    if (!parent || path.isEmpty()) {
        qWarning() << "[populateFolderTree] Invalid parent or path";
        return;
    }
    
    try {
        QDir dir(path);
        if (!dir.exists()) {
            qWarning() << "[populateFolderTree] Directory does not exist:" << path;
            return;
        }
        
        // Get directories first (sorted)
        QFileInfoList entries = dir.entryInfoList(
            QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot,
            QDir::DirsFirst | QDir::Name | QDir::IgnoreCase
        );
        
        int itemCount = 0;
        for (const QFileInfo& entry : entries) {
            // Skip hidden files unless explicitly needed
            if (entry.fileName().startsWith(".")) {
                continue;
            }
            
            QTreeWidgetItem* item = new QTreeWidgetItem(parent);
            item->setText(0, entry.fileName());
            item->setData(0, Qt::UserRole, entry.absoluteFilePath());
            
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
                QString sizeStr = QLocale().formattedDataSize(entry.size());
                item->setToolTip(0, QString("%1 (%2)").arg(entry.absoluteFilePath()).arg(sizeStr));
            }
            
            itemCount++;
        }
        
        qDebug() << "[populateFolderTree] Added" << itemCount << "items to" << path;
        
    } catch (const std::exception& e) {
        qCritical() << "[populateFolderTree] ERROR:" << e.what();
    }
}

QWidget* MainWindow::createTerminalPanel() {
    qDebug() << "[createTerminalPanel] Creating terminal panel with shell integration";
    
    try {
        QWidget* terminalWidget = new QWidget(this);
        terminalWidget->setObjectName("TerminalPanel");
        terminalWidget->setStyleSheet("QWidget#TerminalPanel { background-color: #1e1e1e; }");
        
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
        QWidget* pwshTab = new QWidget(terminalWidget);
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
        QWidget* cmdTab = new QWidget(terminalWidget);
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
        
        connect(pwshInput_, &QLineEdit::returnPressed, this, &MainWindow::handlePwshCommand);
        connect(cmdInput_, &QLineEdit::returnPressed, this, &MainWindow::handleCmdCommand);
        connect(pwshProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::readPwshOutput);
        connect(cmdProcess_, &QProcess::readyReadStandardOutput, this, &MainWindow::readCmdOutput);
        
        pwshOutput_->appendPlainText("PowerShell 7.x\nCopyright (c) Microsoft Corporation. All rights reserved.\n");
        cmdOutput_->appendPlainText("Microsoft Windows [Version 10.0.xxxxx]\n(c) Microsoft Corporation. All rights reserved.\n");
        
        qDebug() << "[createTerminalPanel] Terminal panel created successfully";
        return terminalWidget;
        
    } catch (const std::exception& e) {
        qCritical() << "[createTerminalPanel] ERROR:" << e.what();
        return new QWidget(this);
    }
}

QWidget* MainWindow::createDebugPanel() {
    qDebug() << "[createDebugPanel] Creating debug panel with log management";
    
    try {
        QWidget* debugWidget = new QWidget(this);
        debugWidget->setObjectName("DebugPanel");
        debugWidget->setStyleSheet("QWidget#DebugPanel { background-color: #1e1e1e; }");
        
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
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        debugOutput->appendPlainText(QString("[%1] [INFO] Debug panel initialized").arg(timestamp));
        debugOutput->appendPlainText(QString("[%1] [INFO] Logging system ready").arg(timestamp));
        debugOutput->appendPlainText(QString("[%1] [DEBUG] Production-ready observability enabled").arg(timestamp));
        
        layout->addWidget(debugOutput, 1);
        
        // Connect signals
        connect(logLevelFilter, &QComboBox::currentTextChanged, this, &MainWindow::filterLogLevel);
        connect(clearBtn, &QPushButton::clicked, this, &MainWindow::clearDebugLog);
        connect(saveBtn, &QPushButton::clicked, this, &MainWindow::saveDebugLog);
        
        qDebug() << "[createDebugPanel] Debug panel created successfully with" << debugOutput->blockCount() << "initial log entries";
        return debugWidget;
        
    } catch (const std::exception& e) {
        qCritical() << "[createDebugPanel] ERROR:" << e.what();
        return new QWidget(this);
    }
}

void MainWindow::setupDockWidgets() {
    qDebug() << "[setupDockWidgets] Initializing all dock widgets for IDE subsystems";
    
    try {
        // Create and configure dock widgets for each major subsystem
        
        // 1. Project Explorer Dock
        if (!projectExplorer_) {
            projectExplorer_ = new RawrXD::ProjectExplorerWidget(this);
            QDockWidget* projDock = new QDockWidget("Project Explorer", this);
            projDock->setObjectName("ProjectExplorerDock");
            projDock->setWidget(projectExplorer_);
            projDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
            projDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(Qt::LeftDockWidgetArea, projDock);
            projDock->hide();  // Hidden by default
            qDebug() << "[setupDockWidgets] Created Project Explorer dock";
        }
        
        // 2. Build System Dock
        if (!buildWidget_) {
            buildWidget_ = new BuildSystemWidget(this);
            QDockWidget* buildDock = new QDockWidget("Build System", this);
            buildDock->setObjectName("BuildSystemDock");
            buildDock->setWidget(buildWidget_);
            buildDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
            buildDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(Qt::BottomDockWidgetArea, buildDock);
            buildDock->hide();
            qDebug() << "[setupDockWidgets] Created Build System dock";
        }
        
        // 3. Version Control Dock
        if (!vcsWidget_) {
            vcsWidget_ = new VersionControlWidget(this);
            QDockWidget* vcsDock = new QDockWidget("Version Control", this);
            vcsDock->setObjectName("VersionControlDock");
            vcsDock->setWidget(vcsWidget_);
            vcsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
            vcsDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(Qt::LeftDockWidgetArea, vcsDock);
            vcsDock->hide();
            qDebug() << "[setupDockWidgets] Created Version Control dock";
        }
        
        // 4. Debug/Run Dock
        if (!debugWidget_) {
            debugWidget_ = new RunDebugWidget(this);
            QDockWidget* debugDock = new QDockWidget("Run & Debug", this);
            debugDock->setObjectName("RunDebugDock");
            debugDock->setWidget(debugWidget_);
            debugDock->setAllowedAreas(Qt::AllDockWidgetAreas);
            debugDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(Qt::BottomDockWidgetArea, debugDock);
            debugDock->hide();
            qDebug() << "[setupDockWidgets] Created Run & Debug dock";
        }
        
        // 5. Test Explorer Dock
        if (!testWidget_) {
            testWidget_ = new TestExplorerWidget(this);
            QDockWidget* testDock = new QDockWidget("Test Explorer", this);
            testDock->setObjectName("TestExplorerDock");
            testDock->setWidget(testWidget_);
            testDock->setAllowedAreas(Qt::RightDockWidgetArea | Qt::BottomDockWidgetArea);
            testDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(Qt::RightDockWidgetArea, testDock);
            testDock->hide();
            qDebug() << "[setupDockWidgets] Created Test Explorer dock";
        }
        
        // 6. Profiler Dock
        if (!profilerWidget_) {
            profilerWidget_ = new ProfilerWidget(this);
            QDockWidget* profilerDock = new QDockWidget("Profiler", this);
            profilerDock->setObjectName("ProfilerDock");
            profilerDock->setWidget(profilerWidget_);
            profilerDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea);
            profilerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(Qt::BottomDockWidgetArea, profilerDock);
            profilerDock->hide();
            qDebug() << "[setupDockWidgets] Created Profiler dock";
        }
        
        // 7. Database Tool Dock
        if (!database_) {
            database_ = new DatabaseToolWidget(this);
            QDockWidget* dbDock = new QDockWidget("Database Tools", this);
            dbDock->setObjectName("DatabaseToolDock");
            dbDock->setWidget(database_);
            dbDock->setAllowedAreas(Qt::AllDockWidgetAreas);
            dbDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(Qt::RightDockWidgetArea, dbDock);
            dbDock->hide();
            qDebug() << "[setupDockWidgets] Created Database Tools dock";
        }
        
        // 8. Docker Tools Dock
        if (!docker_) {
            docker_ = new DockerToolWidget(this);
            QDockWidget* dockerDock = new QDockWidget("Docker", this);
            dockerDock->setObjectName("DockerToolDock");
            dockerDock->setWidget(docker_);
            dockerDock->setAllowedAreas(Qt::AllDockWidgetAreas);
            dockerDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetClosable);
            addDockWidget(Qt::RightDockWidgetArea, dockerDock);
            dockerDock->hide();
            qDebug() << "[setupDockWidgets] Created Docker dock";
        }
        
        // Apply consistent styling to all docks
        QList<QDockWidget*> allDocks = findChildren<QDockWidget*>();
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
        
        qDebug() << "[setupDockWidgets] Successfully initialized" << allDocks.size() << "dock widgets";
        
    } catch (const std::exception& e) {
        qCritical() << "[setupDockWidgets] ERROR:" << e.what();
    }
}

void MainWindow::setupSystemTray() {
    qDebug() << "[setupSystemTray] Setting up system tray icon and menu";
    
    try {
        if (!QSystemTrayIcon::isSystemTrayAvailable()) {
            qWarning() << "[setupSystemTray] System tray not available on this platform";
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
        connect(restoreAction, &QAction::triggered, this, [this]() {
            qDebug() << "[SystemTray] Restore window triggered";
            showNormal();
            activateWindow();
            raise();
        });
        
        trayMenu->addSeparator();
        
        // Quick actions
        QAction* newFileAction = trayMenu->addAction("New File");
        newFileAction->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
        connect(newFileAction, &QAction::triggered, this, &MainWindow::handleNewEditor);
        
        QAction* newChatAction = trayMenu->addAction("New AI Chat");
        connect(newChatAction, &QAction::triggered, this, &MainWindow::handleNewChat);
        
        trayMenu->addSeparator();
        
        // Settings action
        QAction* settingsAction = trayMenu->addAction("Settings");
        settingsAction->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
        connect(settingsAction, &QAction::triggered, this, [this]() {
            qDebug() << "[SystemTray] Settings triggered";
            statusBar()->showMessage("Settings panel coming soon", 2000);
        });
        
        trayMenu->addSeparator();
        
        // Quit action
        QAction* quitAction = trayMenu->addAction("Quit RawrXD IDE");
        quitAction->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
        connect(quitAction, &QAction::triggered, this, [this]() {
            qDebug() << "[SystemTray] Quit triggered";
            QApplication::quit();
        });
        
        trayIcon_->setContextMenu(trayMenu);
        
        // Connect double-click to restore
        connect(trayIcon_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick) {
                qDebug() << "[SystemTray] Double-click detected, restoring window";
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
        
        qDebug() << "[setupSystemTray] System tray initialized successfully";
        
    } catch (const std::exception& e) {
        qCritical() << "[setupSystemTray] ERROR:" << e.what();
    }
}

 

void MainWindow::restoreSession() {
    // Use existing handleLoadState() which already implements full state restoration
    handleLoadState();
    
    QSettings settings("RawrXD", "QtShell");
    
    // Restore open file tabs
    int tabCount = settings.value("Session/tabCount", 0).toInt();
    for (int i = 0; i < tabCount; ++i) {
        QString tabKey = QString("Session/tab%1").arg(i);
        QString filePath = settings.value(tabKey + "/path").toString();
        QString content = settings.value(tabKey + "/content").toString();
        QString tabName = settings.value(tabKey + "/name").toString();
        
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
            QString tabKey = QString("Session/tab%1").arg(i);
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
    statusBar()->showMessage(tr("Run script invoked"));
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
    QString prompt = QInputDialog::getMultiLineText(
        this,
        tr("Run Inference"),
        tr("Enter your prompt:"),
        QString(),
        &ok
    );
    
    if (!ok || prompt.isEmpty()) {
        return;
    }
    
    statusBar()->showMessage(tr("Running inference..."));
    
    qint64 reqId = QDateTime::currentMSecsSinceEpoch();
    m_currentStreamId = reqId;
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(tr("\n[User] %1\n").arg(prompt));
    }
    
    // Call inference engine
    QMetaObject::invokeMethod(m_inferenceEngine, "request", Qt::QueuedConnection,
                              Q_ARG(QString, prompt),
                              Q_ARG(qint64, reqId));
}

void MainWindow::loadGGUFModel()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Select GGUF Model"),
        QString(),
        tr("GGUF Files (*.gguf);;All Files (*.*)")
    );
    
    if (filePath.isEmpty()) {
        return;
    }
    
    statusBar()->showMessage(tr("Loading GGUF model..."));
    
    // Call loadModel in the worker thread
    QMetaObject::invokeMethod(m_inferenceEngine, "loadModel", Qt::QueuedConnection,
                              Q_ARG(QString, filePath));
}

 

void MainWindow::unloadGGUFModel()
{
    QMetaObject::invokeMethod(m_inferenceEngine, "unloadModel", Qt::QueuedConnection);
    statusBar()->showMessage(tr("Unloading model..."));
}

void MainWindow::showInferenceResult(qint64 reqId, const QString& result)
{
    // If streaming mode is active, skip full result (tokens already streamed)
    if (m_streamingMode && reqId == m_currentStreamId) {
        return;
    }
    
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[%1] %2").arg(reqId).arg(result));
    }
    statusBar()->showMessage(tr("Inference complete"), 3000);
}

void MainWindow::showInferenceError(qint64 reqId, const QString& errorMsg)
{
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(QString("[%1] ERROR: %2").arg(reqId).arg(errorMsg));
    }
    statusBar()->showMessage(tr("Inference failed"), 3000);
}

void MainWindow::onModelLoadedChanged(bool loaded, const QString& modelName)
{
    QString msg = loaded ? tr("GGUF loaded: %1").arg(modelName) : tr("GGUF unloaded");
    statusBar()->showMessage(msg, 3000);
    if (m_hexMagConsole) {
        m_hexMagConsole->appendPlainText(msg);
    }
    
    if (loaded) {
        // Log how many tensors we saw in the loader
        QStringList names = m_inferenceEngine ? m_inferenceEngine->tensorNames() : QStringList();
        qInfo() << "Model loaded with" << names.size() << "tensors";
        if (m_hexMagConsole) {
            m_hexMagConsole->appendPlainText(QString("Detected %1 tensors").arg(names.size()));
        }

        // If developer wants auto per-layer set, use environment variable RAWRXD_AUTO_SET_LAYER
        QString devCmd = qEnvironmentVariable("RAWRXD_AUTO_SET_LAYER");
        if (!devCmd.isEmpty() && !names.isEmpty()) {
            QString target = names.first();
            QString quant = devCmd.isEmpty() ? "Q6_K" : devCmd; // default to Q6_K
            qInfo() << "Auto-setting layer quant for" << target << "->" << quant;
            if (m_hexMagConsole) m_hexMagConsole->appendPlainText(QString("Auto-set %1 -> %2").arg(target, quant));
            QMetaObject::invokeMethod(m_inferenceEngine, "setLayerQuant", Qt::QueuedConnection,
                                      Q_ARG(QString, target), Q_ARG(QString, quant));
        }
    }
}

void MainWindow::batchCompressFolder()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select GGUF Folder"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );
    
    if (dir.isEmpty()) {
        return;
    }
    
    QDirIterator it(dir, QStringList() << "*.gguf", QDir::Files, QDirIterator::Subdirectories);
    int total = 0, ok = 0;
    
    while (it.hasNext()) {
        QString inPath = it.next();
        QString outPath = inPath + ".gz";
        
        QFile inFile(inPath);
        if (!inFile.open(QIODevice::ReadOnly)) {
            ++total;
            continue;
        }
        
        QByteArray raw = inFile.readAll();
        inFile.close();
        
        QByteArray gz = brutal::compress(raw);
        if (gz.isEmpty()) {
            ++total;
            continue;
        }
        
        QFile outFile(outPath);
        if (outFile.open(QIODevice::WriteOnly)) {
            outFile.write(gz);
            outFile.close();
            ++ok;
        }
        
        ++total;
        statusBar()->showMessage(tr("Batch: %1/%2 compressed").arg(ok).arg(total), 500);
        QCoreApplication::processEvents();  // Keep UI responsive
    }
    
    QString finalMsg = tr("Batch compression complete: %1/%2 files").arg(ok).arg(total);
    statusBar()->showMessage(finalMsg, 5000);
    QMessageBox::information(this, tr("Batch Compress"), finalMsg);
}

// ---------- Ctrl+Shift+A inside the editor ----------
void MainWindow::onCtrlShiftA() {
    QString wish = codeView_->textCursor().selectedText().trimmed();
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
    connect(m_hotReload, &HotReload::quantReloaded, this, [this](const QString& quantType) {
        statusBar()->showMessage(tr("✓ Quantization reloaded: %1").arg(quantType), 3000);
    });
    
    connect(m_hotReload, &HotReload::moduleReloaded, this, [this](const QString& moduleName) {
        statusBar()->showMessage(tr("✓ Module reloaded: %1").arg(moduleName), 3000);
    });
    
    connect(m_hotReload, &HotReload::reloadFailed, this, [this](const QString& error) {
        statusBar()->showMessage(tr("✗ Reload failed: %1").arg(error), 5000);
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
    connect(hotReloadAction, &QAction::triggered, this, &MainWindow::onHotReload);
    
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
    connect(m_agentModeGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        QString mode = action->data().toString();
        changeAgentMode(mode);
    });
    
    // Add separator
    toolsMenu->addSeparator();
    
    // Add Self-Test Gate action
    QAction* selfTestAction = toolsMenu->addAction("Run Self-Test Gate");
    selfTestAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
    connect(selfTestAction, &QAction::triggered, this, [this]() {
        if (canRelease()) {
            statusBar()->showMessage("✓ Self-test gate passed - ready to release", 3000);
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
    m_hotpatchPanelDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_hotpatchPanelDock->setFeatures(QDockWidget::DockWidgetMovable |
                                      QDockWidget::DockWidgetFloatable |
                                      QDockWidget::DockWidgetClosable);
    
    // Add to bottom dock area by default
    addDockWidget(Qt::BottomDockWidgetArea, m_hotpatchPanelDock);
    
    // Wire HotReload signals to hotpatch panel for event logging
    connect(m_hotReload, &HotReload::quantReloaded, this, [this](const QString& quantType) {
        m_hotpatchPanel->logEvent("Quantization Reloaded", quantType, true);
    });
    
    connect(m_hotReload, &HotReload::moduleReloaded, this, [this](const QString& moduleName) {
        m_hotpatchPanel->logEvent("Module Reloaded", moduleName, true);
    });
    
    connect(m_hotReload, &HotReload::reloadFailed, this, [this](const QString& error) {
        m_hotpatchPanel->logEvent("Reload Failed", error, false);
    });
    
    // Connect manual reload button in hotpatch panel to onHotReload
    connect(m_hotpatchPanel, &HotpatchPanel::manualReloadRequested, this, [this](const QString& quantType) {
        m_currentQuantMode = quantType;
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
    connect(toggleHotpatchAction, &QAction::triggered, this, [this](bool visible) {
        toggleHotpatchPanel(visible);
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
    m_masmEditorDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_masmEditorDock->setFeatures(QDockWidget::DockWidgetMovable |
                                   QDockWidget::DockWidgetFloatable |
                                   QDockWidget::DockWidgetClosable);
    
    // Add to right dock area by default
    addDockWidget(Qt::RightDockWidgetArea, m_masmEditorDock);
    
    // Connect editor signals to main window
    connect(m_masmEditor, &MASMEditorWidget::tabChanged, this, [this](int index) {
        statusBar()->showMessage(tr("Switched to: %1").arg(m_masmEditor->getTabName(index)), 2000);
    });
    
    connect(m_masmEditor, &MASMEditorWidget::contentModified, this, [this](int index) {
        QString modified = m_masmEditor->isModified(index) ? " *" : "";
        statusBar()->showMessage(tr("Modified: %1%2").arg(m_masmEditor->getTabName(index)).arg(modified), 1000);
    });
    
    connect(m_masmEditor, &MASMEditorWidget::cursorPositionChanged, this, [this](int line, int col) {
        statusBar()->showMessage(tr("Line %1, Column %2").arg(line).arg(col), 1000);
    });
    
    // Add View menu toggle for MASM Editor
    QMenu* viewMenu = menuBar()->findChild<QMenu*>();
    if (!viewMenu) {
        viewMenu = menuBar()->addMenu("View");
    }
    
    QAction* toggleMASMAction = viewMenu->addAction("MASM Assembly Editor");
    toggleMASMAction->setCheckable(true);
    toggleMASMAction->setChecked(true);
    connect(toggleMASMAction, &QAction::triggered, this, [this](bool visible) {
        toggleMASMEditor(visible);
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
    m_aiChatPanelDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_aiChatPanelDock->setFeatures(QDockWidget::DockWidgetMovable |
                                    QDockWidget::DockWidgetFloatable |
                                    QDockWidget::DockWidgetClosable);
    
    // Add to right dock area by default
    addDockWidget(Qt::RightDockWidgetArea, m_aiChatPanelDock);
    
    // Tabify with MASM editor if present
    if (m_masmEditorDock) {
        tabifyDockWidget(m_masmEditorDock, m_aiChatPanelDock);
        m_aiChatPanelDock->raise();
    }
    
    // Connect chat panel signals to inference engine
    connect(m_aiChatPanel, &AIChatPanel::messageSubmitted,
            this, &MainWindow::onAIChatMessageSubmitted);
    connect(m_aiChatPanel, &AIChatPanel::quickActionTriggered,
            this, &MainWindow::onAIChatQuickActionTriggered);
    
    // Connect inference engine responses to chat panel
    connect(m_inferenceEngine, &InferenceEngine::streamToken,
            this, [this](qint64, const QString& token) {
                if (m_aiChatPanel) m_aiChatPanel->updateStreamingMessage(token);
            });
    connect(m_inferenceEngine, &InferenceEngine::streamFinished,
            this, [this](qint64) {
                if (m_aiChatPanel) m_aiChatPanel->finishStreaming();
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
    connect(toggleChatAction, &QAction::triggered, this, [this](bool visible) {
        if (m_aiChatPanelDock) {
            if (visible) {
                m_aiChatPanelDock->show();
                m_aiChatPanelDock->raise();
            } else {
                m_aiChatPanelDock->hide();
            }
        }
    });
    
    qDebug() << "AI Chat Panel dockable widget created on right side";
}

void MainWindow::onAIChatMessageSubmitted(const QString& message) {
    if (!m_aiChatPanel) return;
    
    try {
        // Add user message to chat
        m_aiChatPanel->addUserMessage(message);
        
        // Send to inference engine
        if (m_inferenceEngine && m_inferenceEngine->isModelLoaded()) {
            qint64 reqId = QDateTime::currentMSecsSinceEpoch();
            m_currentStreamId = reqId;
            m_streamingMode = true;
            
            m_aiChatPanel->addAssistantMessage("", true);  // Start streaming
            
            // Call the streaming 'request' slot
            QMetaObject::invokeMethod(m_inferenceEngine, "request", Qt::QueuedConnection,
                                      Q_ARG(QString, message),
                                      Q_ARG(qint64, reqId),
                                      Q_ARG(bool, true));
        } else {
            m_aiChatPanel->addAssistantMessage("No model loaded. Please load a GGUF model first.", false);
        }
    } catch (const std::exception& e) {
        qCritical() << "Chat message submission error:" << e.what();
        if (m_aiChatPanel) {
            m_aiChatPanel->addAssistantMessage(QString("Error: %1").arg(e.what()), false);
        }
    }
}

void MainWindow::onAIChatQuickActionTriggered(const QString& action, const QString& context) {
    if (!m_aiChatPanel) return;
    
    try {
        QString prompt;
        
        if (action == "explain") {
            prompt = QString("Explain this code:\n%1").arg(context);
        } else if (action == "fix") {
            prompt = QString("Fix any issues in this code:\n%1").arg(context);
        } else if (action == "refactor") {
            prompt = QString("Refactor this code to be more efficient:\n%1").arg(context);
        } else {
            prompt = action;
        }
        
        onAIChatMessageSubmitted(prompt);
    } catch (const std::exception& e) {
        qCritical() << "Quick action error:" << e.what();
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
    m_layerQuantDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_layerQuantDock->setFeatures(QDockWidget::DockWidgetMovable |
                                   QDockWidget::DockWidgetFloatable |
                                   QDockWidget::DockWidgetClosable);
    
    // Add to right dock area by default
    addDockWidget(Qt::RightDockWidgetArea, m_layerQuantDock);
    
    // Connect layer quant widget to inference engine
    connect(m_layerQuantWidget, &LayerQuantWidget::quantModeChanged,
            this, &MainWindow::onQuantModeChanged);
    
    qDebug() << "Layer Quantization widget created";
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
    
    
    connect(m_backendGroup, &QActionGroup::triggered,
            this, &MainWindow::handleBackendSelection);
    
    qDebug() << "AI Backend switcher configured";
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
        if (QString(mode) == "Q4_0") {
            act->setChecked(true);  // Default
        }
    }
    
    connect(quantGroup, &QActionGroup::triggered, this, [this](QAction* action) {
        m_currentQuantMode = action->data().toString();
        statusBar()->showMessage(tr("Quantization Mode: %1").arg(m_currentQuantMode), 3000);
        if (m_layerQuantWidget) {
            // m_layerQuantWidget->setQuantMode(m_currentQuantMode);
        }
    });
}

void MainWindow::onQuantModeChanged(const QString& mode) {
    m_currentQuantMode = mode;
    statusBar()->showMessage(tr("Quantization changed to: %1").arg(mode), 3000);
}

// ============================================================
// Swarm Editing Setup (Collaborative Editing)
// ============================================================

void MainWindow::setupSwarmEditing() {
    // Swarm editing is for collaborative real-time editing
    // Stub implementation - can be expanded with WebSocket support
    m_swarmSocket = nullptr;  // Would initialize QWebSocket here
    m_swarmSessionId.clear();
    
    qDebug() << "Swarm editing initialized (stub)";
}

void MainWindow::joinSwarmSession() {
    // Implement WebSocket connection for collaborative editing
    statusBar()->showMessage("Swarm session feature coming soon", 3000);
}

void MainWindow::onSwarmMessage(const QString& message) {
    (void)message;
    // Handle incoming collaborative edits
}

void MainWindow::broadcastEdit() {
    // Broadcast local edits to swarm session
}

void MainWindow::onAIBackendChanged(const QString& id, const QString& apiKey) {
    m_currentBackend = id;
    m_currentAPIKey = apiKey;
    statusBar()->showMessage(tr("Switched to AI backend: %1").arg(id), 3000);
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
    m_interpretabilityPanelDock->setAllowedAreas(Qt::AllDockWidgetAreas);
    m_interpretabilityPanelDock->setFeatures(QDockWidget::DockWidgetMovable |
                                             QDockWidget::DockWidgetFloatable |
                                             QDockWidget::DockWidgetClosable);
    
    // Add to right dock area by default
    addDockWidget(Qt::RightDockWidgetArea, m_interpretabilityPanelDock);
    m_interpretabilityPanelDock->hide();  // Hidden by default
    
    // Configure anomaly detection thresholds
    m_interpretabilityPanel->setAnomalyThresholds(1e-7f, 10.0f, 0.5f);
    m_interpretabilityPanel->setGradientTrackingEnabled(true);
    
    // Connect signals for real-time diagnostics
    connect(m_interpretabilityPanel, &InterpretabilityPanelEnhanced::anomalyDetected,
            this, [this](const QString& description) {
                // Show warning in status bar
                statusBar()->showMessage(
                    QString("⚠️ Model Anomaly: %1").arg(description), 10000
                );
                
                // Log to console
                if (m_hexMagConsole) {
                    m_hexMagConsole->appendPlainText(
                        QString("[INTERPRETABILITY] %1: %2")
                            .arg(QDateTime::currentDateTime().toString("HH:mm:ss"))
                            .arg(description)
                    );
                }
                
                qWarning() << "Model Anomaly Detected:" << description;
            });
    
    connect(m_interpretabilityPanel, &InterpretabilityPanelEnhanced::diagnosticsCompleted,
            this, [this](const QJsonObject& diagnostics_json) {
                // Update status with diagnostics summary from JSON
                QStringList issues;
                if (diagnostics_json["has_vanishing_gradients"].toBool()) issues << "Vanishing Gradients";
                if (diagnostics_json["has_exploding_gradients"].toBool()) issues << "Exploding Gradients";
                if (diagnostics_json["has_dead_neurons"].toBool()) issues << "Dead Neurons";
                if (diagnostics_json["average_sparsity"].toDouble() > 0.5) issues << "High Sparsity";
                if (diagnostics_json["attention_entropy_mean"].toDouble() < 1.0) issues << "Low Attention Entropy";
                
                if (!issues.isEmpty()) {
                    statusBar()->showMessage(
                        QString("🔍 Diagnostics: %1 issue(s) - %2")
                            .arg(issues.size())
                            .arg(issues.join(", ")),
                        8000
                    );
                } else {
                    statusBar()->showMessage("✅ Model Diagnostics: All checks passed", 5000);
                }
                
                qInfo() << "Diagnostics completed with issues:" << issues.count() << "total";
            });
    
    connect(m_interpretabilityPanel, &InterpretabilityPanelEnhanced::exportRequested,
            this, [this](const QString& format) {
                QString filter;
                QString defaultSuffix;
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
                
                QString filePath = QFileDialog::getSaveFileName(
                    this,
                    tr("Export Interpretability Data"),
                    QDir::homePath() + "/interpretability_export" + defaultSuffix,
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
                            tr("Interpretability data exported to:\n%1").arg(filePath));
                        qInfo() << "Exported interpretability data to:" << filePath;
                    } else {
                        QMessageBox::warning(this, tr("Export Failed"),
                            tr("Failed to export data to:\n%1").arg(filePath));
                        qWarning() << "Failed to export to:" << filePath;
                    }
                }
            });
    
    // Connect to inference engine for automatic data feed (if inference engine exists)
    if (m_inferenceEngine) {
        // When model is loaded, enable the panel
        connect(m_inferenceEngine, &InferenceEngine::modelLoadedChanged,
                this, [this](bool success) {
                    if (success) {
                        m_interpretabilityPanelDock->show();
                        statusBar()->showMessage(
                            QString("📊 Interpretability Panel enabled"), 3000
                        );
                    }
                });
        
        // TODO: Connect to actual inference data streams when available
        // connect(m_inferenceEngine, &InferenceEngine::attentionDataAvailable,
        //         m_interpretabilityPanel, &InterpretabilityPanelEnhanced::updateAttentionHeads);
        // connect(m_inferenceEngine, &InferenceEngine::gradientDataAvailable,
        //         m_interpretabilityPanel, &InterpretabilityPanelEnhanced::updateGradientFlow);
        // connect(m_inferenceEngine, &InferenceEngine::activationDataAvailable,
        //         m_interpretabilityPanel, &InterpretabilityPanelEnhanced::updateActivationStats);
    }
    
    qDebug() << "Interpretability Panel initialized successfully";
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
            qInfo() << "Interpretability panel shown, diagnostics updated";
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

void MainWindow::setupCommandPalette()
{
    // Stub implementation - command palette initialization
    // In production, this would create CommandPalette widget and wire signals
    qDebug() << "[MainWindow] Command palette setup requested - stub implementation";
    
    if (!m_commandPalette) {
        // Future: instantiate CommandPalette widget here
        qInfo() << "[MainWindow] Command palette widget not yet initialized";
    }
}


