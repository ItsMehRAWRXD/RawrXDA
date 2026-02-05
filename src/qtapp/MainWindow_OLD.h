#pragma once
/*  MainWindow.h  –  “One IDE to rule them all”
    This header keeps every original symbol but adds every major IDE subsystem
    as a first-class citizen.  All new widgets are owned by MainWindow and
    can be toggled on/off from the View menu.  Every subsystem is already
    connected to the existing StreamerClient / AgentOrchestrator so that
    AI assistance, auto-fixes, explanations, refactorings, etc. work out of
    the box for every panel.                                                     */

#include <QMainWindow>
#include <QUrl>
#include <QHash>
#include <QString>
#include <QPointer>

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
/* ---------------  Qt advanced  --------------- */
class QJsonDocument;
class QWebEngineView;
class QWebChannel;
// class QtCharts::QChartView; // Forward declaration for charts if needed
class QGraphicsView;
class QUndoGroup;
class QUndoView;
class QSyntaxHighlighter;
QT_END_NAMESPACE

/* ---------------  Our own forward decls  --------------- */
class StreamerClient;
class AgentOrchestrator;
class AISuggestionOverlay;
class TaskProposalWidget;
class PowerShellHighlighter;
class TerminalWidget;

/* ---------------  New high-level subsystems  --------------- */
class ProjectExplorerWidget;          // full project tree + file-system watcher
class BuildSystemWidget;              // CMake / QMake / Meson / Bazel / Ninja / MSBuild
class VersionControlWidget;           // Git / SVN / Perforce / Mercurial
class RunDebugWidget;                 // debugger engine (LLDB / GDB / CDB / MSVC)
class ProfilerWidget;                 // CPU, memory, GPU, I/O profilers
class TestExplorerWidget;             // GoogleTest / Catch2 / QtTest / pytest / JUnit
class DatabaseToolWidget;             // universal DB client (MySQL, PG, SQLite, Oracle, SQL-Server, Mongo, Redis…)
class DockerToolWidget;               // container / Kubernetes explorer
class CloudExplorerWidget;            // AWS, Azure, GCP, DO, Vercel, Netlify
class PackageManagerWidget;           // Conan / vcpkg / npm / pip / cargo / nuget / maven
class DocumentationWidget;            // integrated help + Zeal-like offline docsets
class UMLViewWidget;                  // live UML / PlantUML renderer
class ImageToolWidget;                // built-in image viewer / editor / converter
class TranslationWidget;              // Qt-Linguist-like UI for i18n
class DesignToCodeWidget;             // import Figma, Sketch, Adobe-XD, PSD
class AIChatWidget;                   // persistent AI chat that survives sessions
class NotebookWidget;                 // Jupyter-like notebook for rapid prototyping
class MarkdownViewer;                 // live markdown + math (KaTeX) renderer
class SpreadsheetWidget;              // Excel-like grid for quick data tasks
class TerminalClusterWidget;          // grid of terminals (tmux-like)
class SnippetManagerWidget;           // global code-snippet library
class RegexTesterWidget;              // live regex with Qt / PCRE / Boost engines
class DiffViewerWidget;               // side-by-side diff & 3-way merge
class ColorPickerWidget;              // palette, contrast checker, accessibility
class IconFontWidget;                 // FontAwesome, Material, Feather picker
class PluginManagerWidget;            // hot-load C++/Python/JS plugins
class SettingsWidget;                 // unified settings (JSON + UI)
class NotificationCenter;             // in-app notification hub
class ShortcutsConfigurator;          // editable global shortcuts
class TelemetryWidget;                // usage analytics / crash reporter
class UpdateCheckerWidget;            // auto-updater (GitHub, custom CDN)
class WelcomeScreenWidget;            // project picker / recent / news
class CommandPalette;                 // VS-Code-like fuzzy command launcher
class ProgressManager;                // central async task progress UI
class AIQuickFixWidget;               // lightbulb popup for instant AI fixes
class CodeMinimap;                    // Sublime-like minimap for every editor
class BreadcrumbBar;                  // file / symbol / namespace breadcrumb
class StatusBarManager;               // extensible status-bar fields
class TerminalEmulator;               // full VT-100/UTF-8 terminal per tab
class SearchResultWidget;             // global search / replace results
class BookmarkWidget;                 // persistent bookmarks across files
class TodoWidget;                     // TODO/FIXME/XXX aggregator
class MacroRecorderWidget;            // record/replay UI macros
class AICompletionCache;              // local LLM cache for fast completion
class LanguageClientHost;             // LSP client host (clangd, pylsp, gopls, rust-analyzer…)
class CodeLensProvider;               // inline actionable hints
class InlayHintProvider;              // type hints, parameter names
class SemanticHighlighter;            // LSP-based token colouring
class InlineChatWidget;               // inline chat (JetBrains Fleet style)
class AIReviewWidget;                 // AI PR reviewer
class CodeStreamWidget;               // collaborative editing (CRDT)
class AudioCallWidget;                // voice chat inside the IDE
class ScreenShareWidget;              // share IDE or desktop
class WhiteboardWidget;               // draw / diagram during calls
class TimeTrackerWidget;              // automatic time & billing
class TaskManagerWidget;              // generic todo / Kanban board
class PomodoroWidget;                 // productivity timer
class WallpaperWidget;                // optional animated wallpaper
class AccessibilityWidget;            // screen-reader & high-contrast helpers

/* ============================================================ */
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void setAppState(std::shared_ptr<void> state);

signals:
    void onGoalSubmitted(const QString& goal);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void closeEvent(QCloseEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots: /* ----------  original slots  ---------- */
    // Basic Editor Slots
    void onEditorTextChanged();
    void updateLineColumnInfo();
    void onFileTreeDoubleClicked(const QModelIndex& index);
    void onTerminalCommandExecute();
    void onPowerShellOutput();
    void onPowerShellError();
    void onApplyClicked();
    void onResetClicked();
    void onRunScript();
    void onNewFile();
    void onOpenFile();
    void onSaveFile();
    void onAbout();
    
    // Advanced Slots
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

    // Subsystem Toggle Slots
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

private: /* ---------------  UI creators  --------------- */
    void createCentralEditor();
    void createFileExplorer();
    void createTerminalPanel();
    void createOutputPanel();
    void createOverclockPanel();
    void createSyntaxHighlighting();
    void createToolBars();
    void createMenus();
    void createStatusBar();
    void executeCommand(const QString& command);
    
    QWidget* createGoalBar();
    QWidget* createAgentPanel();
    QWidget* createProposalReview();
    QWidget* createEditorArea();
    QWidget* createQShellTab();
    QJsonDocument getMockArchitectJson() const;
    void populateFolderTree(QTreeWidgetItem* parent, const QString& path);
    QWidget* createTerminalPanelWidget();
    QWidget* createDebugPanelWidget();

    void setupMenuBar();
    void setupToolBars();
    void setupDockWidgets();
    void setupStatusBar();
    void setupSystemTray();
    void setupShortcuts();
    void restoreSession();
    void saveSession();
    void initSubsystems();
    void createDockWidgets();

private: /* ---------------  original members  --------------- */
    // Basic UI
    QSplitter* m_mainSplitter = nullptr;
    QSplitter* m_editorSplitter = nullptr;
    QTabWidget* m_editorTabs = nullptr;
    QTextEdit* m_editor = nullptr;
    QPlainTextEdit* m_terminalOutput = nullptr;
    QLineEdit* m_commandInput = nullptr;
    QTextEdit* m_outputPanel = nullptr;
    QWidget* m_overclockWidget = nullptr;
    QLabel* m_cpuTelemetryLabel = nullptr;
    QLabel* m_gpuTelemetryLabel = nullptr;
    QLabel* m_offsetLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_applyButton = nullptr;
    QPushButton* m_resetButton = nullptr;
    PowerShellHighlighter* m_highlighter = nullptr;
    QFileSystemModel* m_fileSystemModel = nullptr;
    QTreeView* m_fileExplorer = nullptr;
    QProcess* m_powerShellProcess = nullptr;
    TerminalWidget* m_terminalWidget = nullptr;
    
    // Advanced UI (original)
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

private: /* ---------------  new IDE subsystem members  --------------- */
    /* Core */
    QPointer<WelcomeScreenWidget> welcomeScreen_;
    QPointer<CommandPalette> commandPalette_;
    QPointer<ProgressManager> progressManager_;
    QPointer<NotificationCenter> notificationCenter_;
    QPointer<ShortcutsConfigurator> shortcutsConfig_;
    QPointer<SettingsWidget> settingsWidget_;
    QPointer<UpdateCheckerWidget> updateChecker_;
    QPointer<TelemetryWidget> telemetry_;
    QPointer<PluginManagerWidget> pluginManager_;
    QPointer<QSystemTrayIcon> trayIcon_;

    /* Project & Build */
    QPointer<ProjectExplorerWidget> projectExplorer_;
    QPointer<BuildSystemWidget> buildWidget_;
    QPointer<VersionControlWidget> vcsWidget_;
    QPointer<RunDebugWidget> debugWidget_;
    QPointer<ProfilerWidget> profilerWidget_;
    QPointer<TestExplorerWidget> testWidget_;

    /* Editors & Language */
    QPointer<LanguageClientHost> lspHost_;
    QPointer<CodeLensProvider> codeLens_;
    QPointer<InlayHintProvider> inlay_;
    QPointer<SemanticHighlighter> semantic_;
    QPointer<CodeMinimap> minimap_;
    QPointer<BreadcrumbBar> breadcrumb_;
    QPointer<SearchResultWidget> searchResults_;
    QPointer<BookmarkWidget> bookmarks_;
    QPointer<TodoWidget> todos_;
    QPointer<MacroRecorderWidget> macroRecorder_;
    QPointer<AICompletionCache> completionCache_;
    QPointer<InlineChatWidget> inlineChat_;
    QPointer<AIQuickFixWidget> quickFix_;
    QPointer<DiffViewerWidget> diffViewer_;
    QPointer<UMLViewWidget> umlView_;

    /* Docs & Notes */
    QPointer<DocumentationWidget> documentation_;
    QPointer<NotebookWidget> notebook_;
    QPointer<MarkdownViewer> markdownViewer_;
    QPointer<SpreadsheetWidget> spreadsheet_;

    /* Assets & Design */
    QPointer<ImageToolWidget> imageTool_;
    QPointer<DesignToCodeWidget> designImport_;
    QPointer<ColorPickerWidget> colorPicker_;
    QPointer<IconFontWidget> iconFont_;
    QPointer<TranslationWidget> translator_;

    /* DevOps & Cloud */
    QPointer<DockerToolWidget> docker_;
    QPointer<CloudExplorerWidget> cloud_;
    QPointer<PackageManagerWidget> pkgManager_;
    QPointer<DatabaseToolWidget> database_;

    /* Snippets & Utilities */
    QPointer<SnippetManagerWidget> snippets_;
    QPointer<RegexTesterWidget> regexTester_;

    /* Terminals */
    QPointer<TerminalClusterWidget> terminalCluster_;
    QPointer<TerminalEmulator> embeddedTerminal_;

    /* AI & Chat */
    QPointer<AIChatWidget> aiChat_;
    QPointer<AIReviewWidget> aiReview_;
    QPointer<CodeStreamWidget> codeStream_;

    /* Collaboration */
    QPointer<AudioCallWidget> audioCall_;
    QPointer<ScreenShareWidget> screenShare_;
    QPointer<WhiteboardWidget> whiteboard_;

    /* Productivity */
    QPointer<TimeTrackerWidget> timeTracker_;
    QPointer<TaskManagerWidget> taskManager_;
    QPointer<PomodoroWidget> pomodoro_;
    QPointer<WallpaperWidget> wallpaper_;
    QPointer<AccessibilityWidget> accessibility_;

    /* Status & UI */
    QPointer<StatusBarManager> statusBarManager_;
    QPointer<QUndoGroup> undoGroup_;               // global undo for all editors
    QPointer<QThread> backgroundThread_;           // for heavy LSP / AI tasks
};
