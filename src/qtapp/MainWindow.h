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


#include "Subsystems.h" // Include the stubs/definitions

#include "masm/MASMCompilerWidget.h"
#include "masm/pe_writer.h"
#include "masm/elf_writer.h"
#include "masm/mach_o_writer.h"

QT_BEGIN_NAMESPACE
/* ---------------  Qt primitives  --------------- */


/* ---------------  Qt advanced  --------------- */


// class QtCharts::QChartView; // Forward declaration for charts if needed


QT_END_NAMESPACE

namespace RawrXD {
class ProjectExplorerWidget;
class TaskOrchestrator;     // Forward declaration for orchestration system
class OrchestrationUI;      // Forward declaration for orchestration UI
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
class ModelLoaderThread;
class AgenticEngine;
class MultiTabEditor;  // Forward declaration for multi-tab editor

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
class MainWindow : public void
{

public:
    /**
     * \brief Constructs the main window
     * \param parent The parent widget (typically nullptr for the top-level window)
     */
    explicit MainWindow(void* parent = nullptr);
    
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


    void onGoalSubmitted(const std::string& goal);

protected:
    bool eventFilter(void* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private: /* ----------  original slots  ---------- */
    void handleGoalSubmit();
    void handleAgentMockProgress();
    void updateSuggestion(const std::string& chunk);
    void appendModelChunk(const std::string& chunk);
    void handleGenerationFinished();
    void handleQShellReturn();
    void handleArchitectChunk(const std::string& chunk);
    void handleArchitectFinished();
    void handleTaskStatusUpdate(const std::string& taskId, const std::string& status, const std::string& agentType);
    void handleTaskCompleted(const std::string& agentType, const std::string& summary);
    void handleWorkflowFinished(bool success);
    void handleTaskStreaming(const std::string& taskId, const std::string& chunk, const std::string& agentType);
    void handleSaveState();
    void handleLoadState();
    
    // ============================================================
    // Phase C: Data Persistence Methods
    // ============================================================
    
    // Editor State Persistence
    void saveEditorState();
    void restoreEditorState();
    void saveTabState();
    void restoreTabState();
    void trackEditorCursorPosition();
    void trackEditorScrollPosition();
    
    // Recent Files Management
    void addRecentFile(const std::string& filePath);
    std::vector<std::string> getRecentFiles() const;
    void clearRecentFiles();
    void populateRecentFilesMenu(QMenu* recentMenu);
    
    // Command History Tracking
    void addCommandToHistory(const std::string& command);
    std::vector<std::string> getCommandHistory() const;
    void clearCommandHistory();
    int getCommandHistoryLimit() const { return 1000; }
    
    // Helper Methods for Persistence
    void persistEditorContent();
    void restoreEditorContent();
    void persistEditorMetadata();
    void restoreEditorMetadata();
    
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
    void filterLogLevel(const std::string& level);
    void showEditorContextMenu(const QPoint& pos);
    void explainCode();
    void fixCode();
    void refactorCode();
    void generateTests();
    void generateDocs();
    // Adapter slots for ActionExecutor signals
    void onActionStarted(int index, const std::string& description);
    void onActionCompleted(int index, bool success, const void*& result);
    void onPlanCompleted(bool success, const void*& result);
    
    // Explorer event handlers
    void onExplorerItemExpanded(QTreeWidgetItem* item);
    void onExplorerItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onAIChatCodeInsertRequested(const std::string& code);

private: /* ----------  new IDE-wide slots  ---------- */
    void onProjectOpened(const std::string& path);
    void onBuildStarted();
    void onBuildFinished(bool success);
    void onVcsStatusChanged();
    void onDebuggerStateChanged(bool running);
    void onTestRunStarted();
    void onTestRunFinished();
    void onDatabaseConnected();
    void onDockerContainerListed();
    void onCloudResourceListed();
    void onPackageInstalled(const std::string& pkg);
    void onDocumentationQueried(const std::string& keyword);
    void onUMLGenerated(const std::string& plantUml);
    void onImageEdited(const std::string& path);
    void onTranslationChanged(const std::string& lang);
    void onDesignImported(const std::string& file);
    void onAIChatMessage(const std::string& msg);
    void onNotebookExecuted();
    void onMarkdownRendered();
    void onSheetCalculated();
    void onTerminalCommand(const std::string& cmd);
    void onSnippetInserted(const std::string& id);
    void onRegexTested(const std::string& pattern);
    void onDiffMerged();
    void onColorPicked(const QColor& c);
    void onIconSelected(const std::string& name);
    void onPluginLoaded(const std::string& name);
    void onSettingsSaved();
    void onNotificationClicked(const std::string& id);
    void onShortcutChanged(const std::string& id, const QKeySequence& key);
    void onTelemetryReady();
    void onUpdateAvailable(const std::string& version);
    void onWelcomeProjectChosen(const std::string& path);
    // Refresh the main model selector to include Ollama, cloud, and local GGUF models
    void refreshModelSelector();
    void onCommandPaletteTriggered(const std::string& cmd);
    void executeCommand(const std::string& command);  // Execute command from palette
    void showCommandPalette();  // Show command palette
    void onProgressCancelled(const std::string& taskId);
    void onQuickFixApplied(const std::string& fix);
    void onMinimapClicked(qreal ratio);
    void onBreadcrumbClicked(const std::string& symbol);
    void onStatusFieldClicked(const std::string& field);
    void onTerminalEmulatorCommand(const std::string& cmd);
    void onSearchResultActivated(const std::string& file, int line);
    void onBookmarkToggled(const std::string& file, int line);
    void onTodoClicked(const std::string& file, int line);
    void onMacroReplayed();
    void onCompletionCacheHit(const std::string& key);
    void onLSPDiagnostic(const std::string& file, const void*& diags);
    void onCodeLensClicked(const std::string& command);
    void onInlayHintShown(const std::string& file);
    void onInlineChatRequested(const std::string& text);
    void onAIReviewComment(const std::string& comment);
    void onCodeStreamEdit(const std::string& patch);
    void onAudioCallStarted();
    void onScreenShareStarted();
    void onWhiteboardDraw(const std::vector<uint8_t>& svg);
    void onTimeEntryAdded(const std::string& task);
    void onKanbanMoved(const std::string& taskId);
    void onPomodoroTick(int remaining);
    void onWallpaperChanged(const std::string& path);
    void onAccessibilityToggled(bool on);

    // AI/GGUF/InferenceEngine slots
    void loadGGUFModel();
    void loadGGUFModel(const std::string& ggufPath);
    void runInference();
    void unloadGGUFModel();
    void showInferenceResult(qint64 reqId, const std::string& result);
    void showInferenceError(qint64 reqId, const std::string& errorMsg);
    void onModelLoadedChanged(bool loaded, const std::string& modelName);
    void onModelLoadFinished(bool success, const std::string& errorMsg);
    void batchCompressFolder();
    void onAIChatMessageSubmitted(const std::string& message);
    void onAIChatQuickActionTriggered(const std::string& action, const std::string& context);

    // Agent integration
    void onCtrlShiftA();
    bool canRelease();
    void onHotReload();
    void changeAgentMode(const std::string& mode);
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
    
    // ============================================================
    // Eon/ASM Compiler Slots
    // ============================================================
    void toggleCompileCurrentFile();
    void toggleBuildProject();
    void toggleCleanBuild();
    void toggleCompilerSettings();
    void toggleCompilerOutput();

    // ============================================================
    // File Menu Slots
    // ============================================================
    void handleSaveAs();
    void handleSaveAll();
    void toggleAutoSave(bool enabled);
    void handleCloseEditor();
    void handleCloseAllEditors();
    void handleCloseFolder();
    void handlePrint();
    void handleExport();
    
    // ============================================================
    // Edit Menu Slots
    // ============================================================
    void handleUndo();
    void handleRedo();
    void handleCut();
    void handleCopy();
    void handlePaste();
    void handleDelete();
    void handleSelectAll();
    void handleFind();
    void handleFindReplace();
    void handleFindInFiles();
    void handleGoToLine();
    void handleGoToSymbol();
    void handleGoToDefinition();
    void handleGoToReferences();
    void handleToggleComment();
    void handleFormatDocument();
    void handleFormatSelection();
    void handleFoldAll();
    void handleUnfoldAll();
    
    // ============================================================
    // Run/Debug Menu Slots
    // ============================================================
    void handleStartDebug();
    void handleRunNoDebug();
    void handleStopDebug();
    void handleRestartDebug();
    void handleStepOver();
    void handleStepInto();
    void handleStepOut();
    void handleToggleBreakpoint();
    void handleAddRunConfig();
    
    // ============================================================
    // Terminal Menu Slots
    // ============================================================
    void handleNewTerminal();
    void handleSplitTerminal();
    void handleKillTerminal();
    void handleClearTerminal();
    void handleRunActiveFile();
    void handleRunSelection();
    
    // ============================================================
    // Window Menu Slots
    // ============================================================
    void handleSplitRight();
    void handleSplitDown();
    void handleSingleGroup();
    void handleFullScreen();
    void handleZenMode();
    void handleToggleSidebar();
    void handleResetLayout();
    void handleSaveLayout();
    
    // ============================================================
    // Tools Menu Slots
    // ============================================================
    void handleExternalTools();
    
    // ============================================================
    // Help Menu Slots
    // ============================================================
    void handleOpenDocs();
    void handlePlayground();
    void handleShowShortcuts();
    void handleCheckUpdates();
    void handleReleaseNotes();
    void handleReportIssue();
    void handleJoinCommunity();
    void handleViewLicense();
    void handleDevTools();

private: /* ---------------  UI creators  --------------- */
    void* createGoalBar();
    void* createAgentPanel();
    void* createProposalReview();
    void* createEditorArea();
    void* createQShellTab();
    void* getMockArchitectJson() const;
    void populateFolderTree(QTreeWidgetItem* parent, const std::string& path);
    void* createTerminalPanel();
    void* createDebugPanel();

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
    void setupOrchestrationSystem();
    void setupCommandPalette();
    void setupAIChatPanel();
    void setupMASMEditor();
    void setupHotpatchPanel();
    void setupInterpretabilityPanel();
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
    std::string suggestionBuffer_{};
    std::string architectBuffer_{};
    bool suggestionEnabled_{true};
    bool forceMockArchitect_{false};
    bool architectRunning_{false};
    std::unordered_map<std::string, QListWidgetItem*> proposalItemMap_{};
    std::unordered_map<std::string, TaskProposalWidget*> proposalWidgetMap_{};
    QTextEdit* qshellOutput_{};
    QLineEdit* qshellInput_{};
    StreamerClient* streamer_{};
    std::string streamerUrl_{QStringLiteral("http://localhost:11434")};
    AgentOrchestrator* orchestrator_{};
    QDockWidget* terminalDock_{};
    QTabWidget* terminalTabs_{};
    QPlainTextEdit* pwshOutput_{};
    QPlainTextEdit* cmdOutput_{};
    QLineEdit* pwshInput_{};
    QLineEdit* cmdInput_{};
    QProcess* pwshProcess_{};
    QProcess* cmdProcess_{};

    /* ============================================================
     * Missing members required by MainWindow.cpp
     * ============================================================ */
    QTextEdit* editor_{};                      // Primary editor for code
    std::string currentFilePath_{};                 // Path of currently open file
    class MultiTabEditor* m_multiTabEditor{};  // Multi-tab editor manager
    std::string m_currentWorkspacePath{};           // Current workspace/project path
    QTreeWidget* m_explorerView{};              // Project explorer tree view
    class AIChatPanel* m_agentChatPane{};      // Agent chat panel (alias)
    QDockWidget* m_compilerOutputDock{};        // Compiler output dock widget
    QPlainTextEdit* m_compilerOutput{};         // Compiler output text
    class RawrXD::TaskOrchestrator* m_taskOrchestrator{}; // Task orchestration system

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
    QPointer<TestExplorerWidget> m_testRunnerPanelPhase8{};
    void* m_outputPanelWidget{};
    std::string m_currentProjectPath{};

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
    class std::thread* m_engineThread{};
    class StreamingInference* m_streamer{};
    bool m_streamingMode{false};
    qint64 m_currentStreamId{0};
    QDockWidget* m_modelMonitorDock{};
    QDockWidget* m_aiChatPanelDock{};
    QDockWidget* m_orchestrationDock{};
    QDockWidget* m_thermalDashboardDock{};  // NVMe Thermal Dashboard
    QDockWidget* m_sovereignTelemetryDock{}; // Sovereign MMF telemetry
    class RawrXD::OrchestrationUI* m_orchestrationUI{}; // Orchestration UI panel
    
    /* Unified AI Backend (Cursor-style switcher) */
    class AISwitcher* m_aiSwitcher{};
    class UnifiedBackend* m_unifiedBackend{};
    std::string m_currentBackend{"local"};
    std::string m_currentAPIKey{};
    
    /* Quantization & Layer Management */
    class LayerQuantWidget* m_layerQuantWidget{};
    QDockWidget* m_layerQuantDock{};
    std::string m_currentQuantMode{"Q4_0"};
    
    /* Collaborative Editing (requires Qt WebSockets) */
#ifdef HAVE_QT_WEBSOCKETS
    class QWebSocket* m_swarmSocket{};
#else
    void* m_swarmSocket{};  // Placeholder when WebSockets not available
#endif
    std::string m_swarmSessionId{};
    
    /* Autonomous Agent System */
    class AutoBootstrap* m_agentBootstrap{};
    class HotReload* m_hotReload{};
    class ActionExecutor* m_actionExecutor{};  // Real agent plan executor
    class ModelInvoker* m_modelInvoker{};      // LLM invocation for wish→plan
    class MetaPlanner* m_metaPlanner{};        // Plan generator
    class AgenticEngine* m_agenticEngine{};

    /* MASM Text Editor */
    class MASMEditorWidget* m_masmEditor{};
    QDockWidget* m_masmEditorDock{};

    /* Hotpatch Panel */
    class HotpatchPanel* m_hotpatchPanel{};
    QDockWidget* m_hotpatchPanelDock{};
    
    /* Interpretability Panel - Model Analysis & Diagnostics */
    QPointer<InterpretabilityPanelEnhanced> m_interpretabilityPanel{};
    QDockWidget* m_interpretabilityPanelDock{};

    // MASM Compiler Integration
    std::unique_ptr<MASMCompilerWidget> m_masmCompiler;
    QDockWidget* m_masmDock;

    /* VS Code-like Layout Components */
    class ActivityBar* m_activityBar{};
    void* m_commandPalette{};  // Generic command palette widget (not the CommandPalette class)
    class AIChatPanel* m_aiChatPanel{};
    QDockWidget* m_aiChatDock{};
    QFrame* m_primarySidebar{};
    QStackedWidget* m_sidebarStack{};
    QFrame* m_bottomPanel{};
    QStackedWidget* m_panelStack{};
    QTabWidget* m_chatTabs{};
    QPlainTextEdit* m_hexMagConsole{};
    QComboBox* m_modelSelector{};      // Model selection dropdown
    std::unordered_map<std::string, std::string> m_modelTooltipCache{};
    std::string m_pendingModelPath{};
    QProgressDialog* m_loadingProgressDialog{};
    void** m_loadProgressTimer{};
    ModelLoaderThread* m_modelLoaderThread{};
    QComboBox* m_agentModeSwitcher{};
    std::string m_agentMode{"Plan"};
    QActionGroup* m_agentModeGroup{};
    QActionGroup* m_backendGroup{};
    
    /* ============================================================
     * Phase C: Data Persistence Members
     * ============================================================ */
    std::vector<std::string> m_recentFiles;          // List of recent file paths (20 max)
    std::vector<std::string> m_commandHistory;       // Circular buffer of executed commands (1000 max)
    
    // Editor state tracking
    struct EditorState {
        std::string filePath;               // Current file path
        int cursorLine = 0;             // Cursor line number
        int cursorColumn = 0;           // Cursor column number
        int scrollPosition = 0;         // Scroll offset
        std::vector<uint8_t> selectionStart;      // Selection start position
        std::vector<uint8_t> selectionEnd;        // Selection end position
    };
    std::map<int, EditorState> m_editorStates;  // State per tab (tab index -> state)
    int m_activeTabIndex = -1;          // Currently active tab index
    
    // Metrics for observability
    qint64 m_lastSaveTime = 0;          // Timestamp of last save
    qint64 m_lastRestoreTime = 0;       // Timestamp of last restore
    qint64 m_persistenceSaveMs = 0;     // Duration of save operation in ms
    qint64 m_persistenceRestoreMs = 0;  // Duration of restore operation in ms
    qint64 m_persistenceDataSize = 0;   // Total data persisted in bytes
    
    void createVSCodeLayout();
    void applyDarkTheme();
    void onAIBackendChanged(const std::string& id, const std::string& apiKey);
    void onQuantModeChanged(const std::string& mode);
    void joinSwarmSession();
    void onSwarmMessage(const std::string& message);
    void broadcastEdit();
    void triggerAgentMode();
    void onAgentWishReceived(const std::string& wish);
    void onAgentPlanGenerated(const std::string& planSummary);
    void onAgentExecutionCompleted(bool success);
    std::string buildGgufTooltip(const std::string& filePath);
    void scanProjectForTodos();
    void openFileInEditor(const std::string& path);
};

