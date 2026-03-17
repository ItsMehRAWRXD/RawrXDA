#pragma once
/*  MainWindow.h  –  "One IDE to rule them all"
    This header keeps every original symbol but adds every major IDE subsystem
    as a first-class citizen.  All new widgets are owned by MainWindow and
    can be toggled on/off from the View menu.  Every subsystem is already
    connected to the existing StreamerClient / AgentOrchestrator so that
    AI assistance, auto-fixes, explanations, refactorings, etc. work out of
    the box for every panel.
    
    \file MainWindow.h
    \brief Main window class for RawrXD IDE - "One IDE to rule them all"
    \author RawrXD Team
    \date 2025
    \version 1.0
    
    This comprehensive IDE integrates:
    - Project exploration and file management
    - Build system integration (CMake, QMake, Meson, etc.)
    - Version control systems (Git, SVN, Mercurial, Perforce)
    - Debugging and profiling tools
    - AI-assisted code completion and refactoring
    - Docker and cloud resource management
    - Multiple scripting language support
    - Collaborative editing features
    
    All subsystems can be dynamically toggled via dock widgets for maximum flexibility.
*/

#include <QMainWindow>
#include <QUrl>
#include <QHash>
#include <QString>
#include <QPointer>
#include <QListWidgetItem>
#include "Subsystems.h" // Include the stubs/definitions

QT_BEGIN_NAMESPACE
/* ---------------  Qt primitives  --------------- */
class QLineEdit;
class QPushButton;
class QProgressBar;
class QListWidget;
class QListWidgetItem;
class QTabWidget;
class QTextEdit;
class QLabel;
class QComboBox;
class QTreeWidgetItem;
class QDockWidget;
class QPlainTextEdit;
class QProcess;
class QSplitter;
class QToolBar;
class QMenu;
class QToolButton;
class QProgressDialog;
class QSystemTrayIcon;
class QThread;
class QFileSystemModel;
class QTreeView;
class QStackedWidget;
class QFrame;
class QActionGroup;
/* ---------------  Qt advanced  --------------- */
class QJsonDocument;
class QWebEngineView;
class QWebChannel;
// class QtCharts::QChartView; // Forward declaration for charts if needed
class QGraphicsView;
class QUndoGroup;
class QUndoView;
class QSyntaxHighlighter;
class QProgressBar;
class QSystemTrayIcon;
QT_END_NAMESPACE

namespace RawrXD {
class ProjectExplorerWidget;
class TaskOrchestrator;
class OrchestrationUI;
}

/* ---------------  Our own forward decls  --------------- */
class SettingsDialog;
class StreamerClient;
class AgentOrchestrator;
class AISuggestionOverlay;
class TaskProposalWidget;
class PowerShellHighlighter;
class TerminalWidget;
class ActivityBar;
class CommandPalette;
class AIChatPanel;
class MASMEditorWidget;
class HotpatchPanel;
class LayerQuantWidget;
class InterpretabilityPanelEnhanced;

/* ============================================================ */
/**
 * \class MainWindow
 * \brief Main window for the RawrXD comprehensive IDE
 * 
 * Manages all UI components, dock widgets, and subsystems for the IDE.
 * Supports dynamic loading/unloading of subsystems via toggle slots.
 * 
 * Key Features:
 * - Central editor with syntax highlighting
 * - Multiple dockable subsystems (project explorer, debugger, AI chat, etc.)
 * - Project and session management
 * - Integration with LSP (Language Server Protocol) for intelligent code features
 * - Drag-and-drop file support
 * - Customizable keybindings and settings
 * 
 * \note This is the central hub for all IDE functionality. All subsystems
 *       are owned by MainWindow and destroyed when the window closes.
 * \see Subsystems.h for stub implementations of all subsystem widgets
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    /**
     * \brief Constructs the main window
     * \param parent The parent widget (typically nullptr for the top-level window)
     */
    explicit MainWindow(QWidget* parent = nullptr);
    
    /**
     * \brief Destructor - cleans up all subsystems and resources
     */
    ~MainWindow();

    /**
     * \brief Sets the application state to be managed by this window
     * \param state A shared_ptr to the application state object
     * \note This allows external state management to be integrated with the IDE
     */
    void setAppState(std::shared_ptr<void> state);

signals:
    void onGoalSubmitted(const QString& goal);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots: /* ----------  original slots  ---------- */
    void handleGoalSubmit();
    void handleAgentMockProgress();
    void updateSuggestion(const QString& chunk);
    void appendModelChunk(const QString& chunk);
    void handleGenerationFinished();
    void handleQShellReturn();
    void handleArchitectChunk(const QString& chunk);
    void handleArchitectFinished();
    void handleTaskStatusUpdate(const QString& taskId, const QString& status, const QString& agentType);
    void handleTaskCompleted(const QString& agentType, const QString& summary);
    void handleWorkflowFinished(bool success);
    void handleTaskStreaming(const QString& taskId, const QString& chunk, const QString& agentType);
    void handleSaveState();
    void handleLoadState();
    void handleNewChat();
    void handleNewEditor();
    void handleNewWindow();
    void handleAddFile();
    void handleAddFolder();
    void handleAddSymbol();
    void showContextMenu(const QPoint& pos);
    void loadContextItemIntoEditor(QListWidgetItem* item);
    void handleTabClose(int index);
    void handlePwshCommand();
    void handleCmdCommand();
    void readPwshOutput();
    void readCmdOutput();
    void clearDebugLog();
    void saveDebugLog();
    void filterLogLevel(const QString& level);
    void showEditorContextMenu(const QPoint& pos);
    void explainCode();
    void fixCode();
    void refactorCode();
    void generateTests();
    void generateDocs();
    // Adapter slots for ActionExecutor signals
    void onActionStarted(int index, const QString& description);
    void onActionCompleted(int index, bool success, const QJsonObject& result);
    void onPlanCompleted(bool success, const QJsonObject& result);

private slots: /* ----------  new IDE-wide slots  ---------- */
    void onProjectOpened(const QString& path);
    void onBuildStarted();
    void onBuildFinished(bool success);
    void onVcsStatusChanged();
    void onDebuggerStateChanged(bool running);
    void onTestRunStarted();
    void onTestRunFinished();
    void onDatabaseConnected();
    void onDockerContainerListed();
    void onCloudResourceListed();
    void onPackageInstalled(const QString& pkg);
    void onDocumentationQueried(const QString& keyword);
    void onUMLGenerated(const QString& plantUml);
    void onImageEdited(const QString& path);
    void onTranslationChanged(const QString& lang);
    void onDesignImported(const QString& file);
    void onAIChatMessage(const QString& msg);
    void onNotebookExecuted();
    void onMarkdownRendered();
    void onSheetCalculated();
    void onTerminalCommand(const QString& cmd);
    void onSnippetInserted(const QString& id);
    void onRegexTested(const QString& pattern);
    void onDiffMerged();
    void onColorPicked(const QColor& c);
    void onIconSelected(const QString& name);
    void onPluginLoaded(const QString& name);
    void onSettingsSaved();
    void onNotificationClicked(const QString& id);
    void onShortcutChanged(const QString& id, const QKeySequence& key);
    void onTelemetryReady();
    void onUpdateAvailable(const QString& version);
    void onWelcomeProjectChosen(const QString& path);
    void onCommandPaletteTriggered(const QString& cmd);
    void onProgressCancelled(const QString& taskId);
    void onQuickFixApplied(const QString& fix);
    void onMinimapClicked(qreal ratio);
    void onBreadcrumbClicked(const QString& symbol);
    void onStatusFieldClicked(const QString& field);
    void onTerminalEmulatorCommand(const QString& cmd);
    void onSearchResultActivated(const QString& file, int line);
    void onBookmarkToggled(const QString& file, int line);
    void onTodoClicked(const QString& file, int line);
    void onMacroReplayed();
    void onCompletionCacheHit(const QString& key);
    void onLSPDiagnostic(const QString& file, const QJsonArray& diags);
    void onCodeLensClicked(const QString& command);
    void onInlayHintShown(const QString& file);
    void onInlineChatRequested(const QString& text);
    void onAIReviewComment(const QString& comment);
    void onCodeStreamEdit(const QString& patch);
    void onAudioCallStarted();
    void onScreenShareStarted();
    void onWhiteboardDraw(const QByteArray& svg);
    void onTimeEntryAdded(const QString& task);
    void onKanbanMoved(const QString& taskId);
    void onPomodoroTick(int remaining);
    void onWallpaperChanged(const QString& path);
    void onAccessibilityToggled(bool on);

    // AI/GGUF/InferenceEngine slots
    void loadGGUFModel();
    void runInference();
    void unloadGGUFModel();
    void showInferenceResult(qint64 reqId, const QString& result);
    void showInferenceError(qint64 reqId, const QString& errorMsg);
    void onModelLoadedChanged(bool loaded, const QString& modelName);
    void batchCompressFolder();
    void onAIChatMessageSubmitted(const QString& message);
    void onAIChatQuickActionTriggered(const QString& action, const QString& context);

    // Agent integration
    void onCtrlShiftA();
    bool canRelease();
    void onHotReload();
    void changeAgentMode(const QString& mode);
    void handleBackendSelection(QAction* action);

    // Toggle slots
    void toggleProjectExplorer(bool visible);
    void toggleBuildSystem(bool visible);
    void toggleVersionControl(bool visible);
    void toggleRunDebug(bool visible);
    void toggleProfiler(bool visible);
    void toggleTestExplorer(bool visible);
    void toggleDatabaseTool(bool visible);
    void toggleDockerTool(bool visible);
    void toggleCloudExplorer(bool visible);
    void togglePackageManager(bool visible);
    void toggleDocumentation(bool visible);
    void toggleUMLView(bool visible);
    void toggleImageTool(bool visible);
    void toggleTranslation(bool visible);
    void toggleDesignToCode(bool visible);
    void toggleAIChat(bool visible);
    void toggleNotebook(bool visible);
    void toggleMarkdownViewer(bool visible);
    void toggleSpreadsheet(bool visible);
    void toggleTerminalCluster(bool visible);
    void toggleSnippetManager(bool visible);
    void toggleRegexTester(bool visible);
    void toggleDiffViewer(bool visible);
    void toggleColorPicker(bool visible);
    void toggleIconFont(bool visible);
    void togglePluginManager(bool visible);
    void toggleSettings(bool visible);
    void toggleNotificationCenter(bool visible);
    void toggleShortcutsConfigurator(bool visible);
    void toggleTelemetry(bool visible);
    void toggleUpdateChecker(bool visible);
    void toggleWelcomeScreen(bool visible);
    void toggleCommandPalette(bool visible);
    void toggleProgressManager(bool visible);
    void toggleAIQuickFix(bool visible);
    void toggleCodeMinimap(bool visible);
    void toggleBreadcrumbBar(bool visible);
    void toggleStatusBarManager(bool visible);
    void toggleTerminalEmulator(bool visible);
    void toggleSearchResult(bool visible);
    void toggleBookmark(bool visible);
    void toggleTodo(bool visible);
    void toggleMacroRecorder(bool visible);
    void toggleAICompletionCache(bool visible);
    void toggleLanguageClientHost(bool visible);
    void toggleMASMEditor(bool visible);
    void toggleHotpatchPanel(bool visible);
    void toggleInterpretabilityPanel(bool visible);

private: /* ---------------  UI creators  --------------- */
    QWidget* createGoalBar();
    QWidget* createAgentPanel();
    QWidget* createProposalReview();
    QWidget* createEditorArea();
    QWidget* createQShellTab();
    QJsonDocument getMockArchitectJson() const;
    void populateFolderTree(QTreeWidgetItem* parent, const QString& path);
    QWidget* createTerminalPanel();
    QWidget* createDebugPanel();

    void setupMenuBar();
    void setupToolBars();
    void setupDockWidgets();
    void setupStatusBar();
    void setupSystemTray();
    void setupShortcuts();
    void setupAIBackendSwitcher();
    void setupQuantizationMenu(QMenu* aiMenu);
    void setupLayerQuantWidget();
    void setupSwarmEditing();
    void setupCollaborationMenu();
    void setupAgentSystem();
    void setupCommandPalette();
    void setupAIChatPanel();
    void setupMASMEditor();
    void setupHotpatchPanel();
    void setupInterpretabilityPanel();
    void setupOrchestrationSystem();
    void restoreSession();
    void saveSession();
    
    void createCentralEditor();
    void onRunScript();
    void onAbout();

    void initSubsystems();

    

private: /* ---------------  original members  --------------- */
    QLineEdit* goalInput_{};
    QLabel* mockStatusBadge_{};
    QComboBox* agentSelector_{};
    QListWidget* chatHistory_{};
    QListWidget* contextList_{};
    QTabWidget* editorTabs_{};
    QTextEdit* codeView_{};
    AISuggestionOverlay* overlay_{};
    QString suggestionBuffer_{};
    QString architectBuffer_{};
    bool suggestionEnabled_{true};
    bool forceMockArchitect_{false};
    bool architectRunning_{false};
    QHash<QString, QListWidgetItem*> proposalItemMap_{};
    QHash<QString, TaskProposalWidget*> proposalWidgetMap_{};
    QTextEdit* qshellOutput_{};
    QLineEdit* qshellInput_{};
    StreamerClient* streamer_{};
    QUrl streamerUrl_{QStringLiteral("http://localhost:11434")};
    AgentOrchestrator* orchestrator_{};
    QDockWidget* terminalDock_{};
    QTabWidget* terminalTabs_{};
    QPlainTextEdit* pwshOutput_{};
    QPlainTextEdit* cmdOutput_{};
    QLineEdit* pwshInput_{};
    QLineEdit* cmdInput_{};
    QProcess* pwshProcess_{};
    QProcess* cmdProcess_{};

private: /* ---------------  new IDE members  --------------- */
    /* Core */
    QPointer<WelcomeScreenWidget> welcomeScreen_{};
    QPointer<CommandPalette> commandPalette_{};
    QPointer<ProgressManager> progressManager_{};
    QPointer<NotificationCenter> notificationCenter_{};
    QPointer<ShortcutsConfigurator> shortcutsConfig_{};
    QPointer<SettingsDialog> settingsWidget_{};
    QPointer<UpdateCheckerWidget> updateChecker_{};
    QPointer<TelemetryWidget> telemetry_{};
    QPointer<PluginManagerWidget> pluginManager_{};
    QPointer<QSystemTrayIcon> trayIcon_{};

    /* Project & Build */
    QPointer<RawrXD::ProjectExplorerWidget> projectExplorer_{};  // Real file browser!
    QPointer<BuildSystemWidget> buildWidget_{};
    QPointer<VersionControlWidget> vcsWidget_{};
    QPointer<RunDebugWidget> debugWidget_{};
    QPointer<ProfilerWidget> profilerWidget_{};
    QPointer<TestExplorerWidget> testWidget_{};

    /* Editors & Language */
    QPointer<LanguageClientHost> lspHost_{};
    QPointer<CodeLensProvider> codeLens_{};
    QPointer<InlayHintProvider> inlay_{};
    QPointer<SemanticHighlighter> semantic_{};
    QPointer<CodeMinimap> minimap_{};
    QPointer<BreadcrumbBar> breadcrumb_{};
    QPointer<SearchResultWidget> searchResults_{};
    QPointer<BookmarkWidget> bookmarks_{};
    QPointer<TodoWidget> todos_{};
    QPointer<MacroRecorderWidget> macroRecorder_{};
    QPointer<AICompletionCache> completionCache_{};
    QPointer<InlineChatWidget> inlineChat_{};
    QPointer<AIQuickFixWidget> quickFix_{};
    QPointer<DiffViewerWidget> diffViewer_{};
    QPointer<UMLLViewWidget> umlView_{};

    /* Docs & Notes */
    QPointer<DocumentationWidget> documentation_{};
    QPointer<NotebookWidget> notebook_{};
    QPointer<MarkdownViewer> markdownViewer_{};
    QPointer<SpreadsheetWidget> spreadsheet_{};

    /* Assets & Design */
    QPointer<ImageToolWidget> imageTool_{};
    QPointer<DesignToCodeWidget> designImport_{};
    QPointer<ColorPickerWidget> colorPicker_{};
    QPointer<IconFontWidget> iconFont_{};
    QPointer<TranslationWidget> translator_{};

    /* DevOps & Cloud */
    QPointer<DockerToolWidget> docker_{};
    QPointer<CloudExplorerWidget> cloud_{};
    QPointer<PackageManagerWidget> pkgManager_{};
    QPointer<DatabaseToolWidget> database_{};

    /* Snippets & Utilities */
    QPointer<SnippetManagerWidget> snippetManager_{};
    QPointer<RegexTesterWidget> regexTester_{};
    QPointer<TerminalClusterWidget> terminalCluster_{};
    QPointer<TerminalEmulator> terminalEmulator_{};
    QPointer<StatusBarManager> statusBarManager_{};
    QPointer<WallpaperWidget> wallpaper_{};
    QPointer<AccessibilityWidget> accessibility_{};
    QPointer<TimeTrackerWidget> timeTracker_{};
    QPointer<TaskManagerWidget> taskManager_{};
    QPointer<PomodoroWidget> pomodoro_{};
    QPointer<AudioCallWidget> audioCall_{};
    QPointer<ScreenShareWidget> screenShare_{};
    QPointer<WhiteboardWidget> whiteboard_{};
    QPointer<CodeStreamWidget> codeStream_{};
    QPointer<AIReviewWidget> aiReview_{};

    /* AI/GGUF/Inference Components */
    class InferenceEngine* m_inferenceEngine{};
    class GGUFServer* m_ggufServer{};
    class QThread* m_engineThread{};
    class StreamingInference* m_streamer{};
    bool m_streamingMode{false};
    qint64 m_currentStreamId{0};
    QDockWidget* m_modelMonitorDock{};
    QDockWidget* m_aiChatPanelDock{};
    RawrXD::TaskOrchestrator* m_taskOrchestrator{};
    RawrXD::OrchestrationUI* m_orchestrationUI{};
    QDockWidget* m_orchestrationDock{};
    
    /* Unified AI Backend (Cursor-style switcher) */
    class AISwitcher* m_aiSwitcher{};
    class UnifiedBackend* m_unifiedBackend{};
    QString m_currentBackend{"local"};
    QString m_currentAPIKey{};
    
    /* Quantization & Layer Management */
    class LayerQuantWidget* m_layerQuantWidget{};
    QDockWidget* m_layerQuantDock{};
    QString m_currentQuantMode{"Q4_0"};
    
    /* Collaborative Editing */
    class QWebSocket* m_swarmSocket{};
    QString m_swarmSessionId{};
    
    /* Autonomous Agent System */
    class AutoBootstrap* m_agentBootstrap{};
    class HotReload* m_hotReload{};
    class ActionExecutor* m_actionExecutor{};  // Real agent plan executor
    class ModelInvoker* m_modelInvoker{};      // LLM invocation for wish→plan
    class MetaPlanner* m_metaPlanner{};        // Plan generator

    /* MASM Text Editor */
    class MASMEditorWidget* m_masmEditor{};
    QDockWidget* m_masmEditorDock{};

    /* Hotpatch Panel */
    class HotpatchPanel* m_hotpatchPanel{};
    QDockWidget* m_hotpatchPanelDock{};
    
    /* Interpretability Panel - Model Analysis & Diagnostics */
    class InterpretabilityPanelEnhanced* m_interpretabilityPanel{};
    QDockWidget* m_interpretabilityPanelDock{};

    /* VS Code-like Layout Components */
    class ActivityBar* m_activityBar{};
    class CommandPalette* m_commandPalette{};
    class AIChatPanel* m_aiChatPanel{};
    QDockWidget* m_aiChatDock{};
    QFrame* m_primarySidebar{};
    QStackedWidget* m_sidebarStack{};
    QFrame* m_bottomPanel{};
    QStackedWidget* m_panelStack{};
    QPlainTextEdit* m_hexMagConsole{};
    QComboBox* m_modelSelector{};      // Model selection dropdown
    QComboBox* m_agentModeSwitcher{};
    QString m_agentMode{"Plan"};
    QActionGroup* m_agentModeGroup{};
    QActionGroup* m_backendGroup{};
    
    void createVSCodeLayout();
    void applyDarkTheme();
    void onAIBackendChanged(const QString& id, const QString& apiKey);
    void onQuantModeChanged(const QString& mode);
    void joinSwarmSession();
    void onSwarmMessage(const QString& message);
    void broadcastEdit();
    void triggerAgentMode();
    void onAgentWishReceived(const QString& wish);
    void onAgentPlanGenerated(const QString& planSummary);
    void onAgentExecutionCompleted(bool success);
};





