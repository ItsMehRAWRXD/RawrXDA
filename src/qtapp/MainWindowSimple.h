#pragma once

#define NOMINMAX  // Prevent Windows.h min/max macro conflicts with std::min/max

#include <string>
#include <memory>
#include <vector>
#include <stack>

#ifdef _WIN32
#include <windows.h>

// ============================================================================
// Menu Bar Command IDs
// ============================================================================

// File Menu (100-199)
#define IDM_FILE_NEW            101
#define IDM_FILE_OPEN           102
#define IDM_FILE_SAVE           103
#define IDM_FILE_SAVEAS         104
#define IDM_FILE_AUTOSAVE       105
#define IDM_FILE_CLOSE_FOLDER   106
#define IDM_FILE_EXIT           107
#define IDM_FILE_OPEN_FOLDER    108
#define IDM_FILE_NEW_WINDOW     109
#define IDM_FILE_CLOSE_TAB      110

// Edit Menu (200-299)
#define IDM_EDIT_UNDO           201
#define IDM_EDIT_REDO           202
#define IDM_EDIT_CUT            203
#define IDM_EDIT_COPY           204
#define IDM_EDIT_PASTE          205
#define IDM_EDIT_FIND           206
#define IDM_EDIT_REPLACE        207
#define IDM_EDIT_SELECTALL      208
#define IDM_EDIT_MULTICURSOR_ADD    209
#define IDM_EDIT_MULTICURSOR_REMOVE 210
#define IDM_EDIT_GOTO_LINE      211
#define IDM_EDIT_TOGGLE_COMMENT 212

// Selection Menu (300-399)
#define IDM_SEL_ALL             301
#define IDM_SEL_EXPAND          302
#define IDM_SEL_SHRINK          303
#define IDM_SEL_COLUMN_MODE     304
#define IDM_SEL_ADD_CURSOR_ABOVE 305
#define IDM_SEL_ADD_CURSOR_BELOW 306
#define IDM_SEL_ADD_NEXT_OCCURRENCE 307
#define IDM_SEL_SELECT_ALL_OCCURRENCES 308

// View Menu (400-499)
#define IDM_VIEW_ACTIVITY_BAR   401
#define IDM_VIEW_PRIMARY_SIDEBAR 402
#define IDM_VIEW_SECONDARY_SIDEBAR 403
#define IDM_VIEW_PANEL          404
#define IDM_VIEW_STATUS_BAR     405
#define IDM_VIEW_ZEN_MODE       406
#define IDM_VIEW_COMMAND_PALETTE 407
#define IDM_VIEW_EXPLORER       408
#define IDM_VIEW_SEARCH         409
#define IDM_VIEW_SOURCE_CONTROL 410
#define IDM_VIEW_EXTENSIONS     411
#define IDM_VIEW_PROBLEMS       412
#define IDM_VIEW_OUTPUT         413
#define IDM_VIEW_TERMINAL       414
#define IDM_VIEW_MINIMAP        415
#define IDM_VIEW_WORD_WRAP      416
#define IDM_VIEW_LINE_NUMBERS   417

// Run Menu (500-599)
#define IDM_RUN_START_DEBUG     501
#define IDM_RUN_WITHOUT_DEBUG   502
#define IDM_RUN_STOP            503
#define IDM_RUN_RESTART         504
#define IDM_RUN_STEP_OVER       505
#define IDM_RUN_STEP_INTO       506
#define IDM_RUN_STEP_OUT        507
#define IDM_RUN_CONTINUE        508
#define IDM_RUN_TOGGLE_BREAKPOINT 509
#define IDM_RUN_CLEAR_BREAKPOINTS 510

// Terminal Menu (600-699)
#define IDM_TERM_NEW            601
#define IDM_TERM_SPLIT          602
#define IDM_TERM_RUN_TASK       603
#define IDM_TERM_CLEAR          604
#define IDM_TERM_KILL           605
#define IDM_TERM_PWSH           606
#define IDM_TERM_CMD            607
#define IDM_TERM_GITBASH        608
#define IDM_TERM_RUN_FILE       609

// Help Menu (700-799)
#define IDM_HELP_WELCOME        701
#define IDM_HELP_DOCS           702
#define IDM_HELP_SHORTCUTS      703
#define IDM_HELP_RELEASE_NOTES  704
#define IDM_HELP_CHECK_UPDATES  705
#define IDM_HELP_ABOUT          706
#define IDM_HELP_REPORT_ISSUE   707
#define IDM_HELP_TIPS_TRICKS    708

#endif

#include "../../include/gui.h"
#include "../../include/editor_buffer.h"
#include "../../include/syntax_engine.h"

struct EditCommand {
    size_t pos{};
    std::string removed;
    std::string inserted;
};

class UndoStack {
public:
    void push(const EditCommand& cmd) {
        // truncate redo tail
        while(m_index < m_commands.size()) m_commands.pop_back();
        m_commands.push_back(cmd);
        m_index = m_commands.size();
    }
    bool canUndo() const { return m_index > 0; }
    bool canRedo() const { return m_index < m_commands.size(); }
    EditCommand undo() { if(!canUndo()) return {}; const auto& cmd = m_commands[m_index-1]; --m_index; return cmd; }
    EditCommand redo() { if(!canRedo()) return {}; const auto& cmd = m_commands[m_index]; ++m_index; return cmd; }
private:
    std::vector<EditCommand> m_commands; size_t m_index = 0;
};
#include "../../include/telemetry/ai_metrics.h"
#include "../../include/session/ai_session.h"
#include "../../include/backend/ollama_client.h"

class MainWindow
{
public:
    explicit MainWindow();
    ~MainWindow() = default;

    void show();
    int exec();

private:
    void createWindow();
    void createEditor();
    void createTabBar();
    void createTerminal();
    void createOverclockPanel();
    void createMenus();
    void createMenuBar();          // Creates the top menu bar
    void handleMenuCommand(WORD cmdId);  // Handles menu selections
    void createFloatingPanel();
    void toggleFloatingPanel();
    void handleCommand(const std::string& cmd);
    void updateTelemetry();
    void updateAIMetricsDisplay();
    void sendToTerminal(const std::string& line);
    void addTab(const std::string& filename = "Untitled");
    void switchTab(size_t index);
    void closeTab(size_t index);
    void refreshTabBar();
    void selectLanguageForFile(const std::string& filename);
    void loadSettings();
    void saveSettings();
    
    // Advanced features
    void loadFileWithLazyLoading(const std::string& filename);
    void runPesterTests();
    void buildWithMSBuild();
    void publishToGallery();
    void startRemoteSession(const std::string& remoteHost);
    
    // Editor Settings (10 features)
    void setEditorTheme(const std::string& theme);
    void setEditorFont(const std::string& fontName, int fontSize);
    void setTabSize(int spaces);
    void toggleMinimap();
    void toggleLineNumbers();
    void toggleWordWrap();
    void setColorScheme(const std::string& scheme);
    void toggleAutocomplete();
    void setIndentStyle(bool useTabs);
    void toggleBracketMatching();
    
    // Problems Panel (10 features)
    void createProblemsPanel();
    void addProblem(const std::string& file, int line, const std::string& message, const std::string& severity);
    void clearProblems();
    void autoRepairProblem(int problemIndex);
    void toggleProblemsPanel();
    void sortProblemsBySeverity();
    void filterProblemsByType(const std::string& type);
    void exportProblems(const std::string& filename);
    void jumpToProblem(int problemIndex);
    void showProblemDetails(int problemIndex);
    
    // AI Metrics & Telemetry
    void simulateAIRequest(const std::string& model, bool success = true);
    void exportMetrics(const std::string& format);
    void clearMetrics();
    void showMetricsReport();
    
    // Advanced features placeholders
    void initExtensionSystem(); // Stub: No plugin architecture
    void initRemoteDebugging(); // Stub: No PSRemoting support
    void initUnitTesting(); // Stub: No Pester integration
    void initBuildSystem(); // Stub: No MSBuild support
    void initScriptPublishing(); // Stub: No PowerShell Gallery integration
    void wireOverclockPanel(); // Wire to backend
    void initPerformanceOpts(); // Stub: No lazy loading for large files

#ifdef _WIN32
    HWND m_hwnd = nullptr;
    HWND m_editorHwnd = nullptr;
    HWND m_terminalHwnd = nullptr;
    HWND m_overclockHwnd = nullptr;
    HWND m_floatingPanel = nullptr;
    HWND m_problemsPanelHwnd = nullptr;
    HMENU m_menuBar = nullptr;           // Main menu bar handle
    // Menu visibility/toggle states
    bool m_autoSaveEnabled = false;
    bool m_activityBarVisible = true;
    bool m_primarySidebarVisible = true;
    bool m_secondarySidebarVisible = false;
    bool m_panelVisible = true;
    bool m_statusBarVisible = true;
    bool m_zenModeEnabled = false;
    bool m_columnSelectionMode = false;
    // New layout panes
    HWND m_fileBrowserHwnd = nullptr;    // left: file browser
    HWND m_topChatHwnd = nullptr;        // right: AI chat transcript (read-only)
    HWND m_userChatInputHwnd = nullptr;  // bottom-right: user chat input box
    HWND m_userChatSendBtn = nullptr;    // bottom-right: send button
    // Chat panel composite bottom (legacy - kept for compatibility)
    struct ChatPanelShim {
        void* impl = nullptr;
    } m_chatPanelShim;
    void createLayoutPanes();
    void appendTopChat(const std::string& who, const std::string& text);
    void initializeFileBrowser();
    void initChat();
    void startChatRequest(const std::string& prompt);
    void handleChatResponse(const std::string& response);
    void createTerminalPane();
    void onFileBrowserDblClick();
    // Find / Replace floating panel elements
    HWND m_findPanelHwnd = nullptr;
    HWND m_findEditHwnd = nullptr;
    HWND m_replaceEditHwnd = nullptr;
    HWND m_findNextBtnHwnd = nullptr;
    HWND m_replaceBtnHwnd = nullptr;
    HWND m_replaceAllBtnHwnd = nullptr;
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK FloatingPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    HWND m_tabBarHwnd = nullptr;
#endif

    std::string m_windowTitle = "RawrXD IDE";
    std::shared_ptr<AppState> m_appState;
    std::string m_currentFile;
    std::vector<std::string> m_editorBuffer;
    PROCESS_INFORMATION m_terminalProcess = {0};
    bool m_terminalRunning = false;
    HANDLE m_psInWrite = nullptr;   // Parent write to PowerShell stdin
    HANDLE m_psOutRead = nullptr;   // Parent read from PowerShell stdout/err
    std::thread m_terminalReaderThread; 
    std::atomic<bool> m_terminalReaderActive{false};
    
    // Advanced features state
    std::vector<HMODULE> m_loadedPlugins;
    bool m_remoteDebugEnabled = false;
    bool m_pesterAvailable = false;
    std::string m_msbuildPath;
    bool m_galleryReady = false;
    bool m_lazyLoadingEnabled = false;
    size_t m_maxFileSizeForLazyLoad = 0;
    
    // Floating panel state
    bool m_floatingPanelVisible = false;
    POINT m_panelDragStart = {0, 0};
    bool m_panelDragging = false;
    // Search state
    long m_lastFindPos = -1;
    bool m_searchCaseSensitive = false;
    bool m_searchRegex = false;
    
    // Editor Settings state
    std::string m_editorTheme = "dark";
    std::string m_fontName = "Consolas";
    int m_fontSize = 11;
    int m_tabSize = 4;
    bool m_minimapEnabled = true;
    bool m_lineNumbersEnabled = true;
    bool m_wordWrapEnabled = false;
    bool m_autocompleteEnabled = true;
    bool m_useTabsForIndent = false;
    bool m_bracketMatchingEnabled = true;
    std::string m_colorScheme = "vscode-dark";
    
    // Problems Panel state
    struct Problem {
        std::string file;
        int line;
        std::string message;
        std::string severity; // error, warning, info
    };
    std::vector<Problem> m_problems;
    bool m_problemsPanelVisible = true;
    std::string m_problemsFilter = "all";

    // Search helpers
    void findNextInEditor(const std::string& searchText);
    void replaceNextInEditor(const std::string& findText, const std::string& replaceText);
    void replaceAllInEditor(const std::string& findText, const std::string& replaceText);
    void appendTerminalOutput(const std::string& chunk);
    void startTerminalReader();
    void stopTerminalReader();
    void syncEditorFromBuffer();
    void applyEdit(size_t pos, size_t eraseLen, std::string_view insertText);
    void performUndo();
    void performRedo();
    void retokenizeAndApplyColors();

    struct Tab {
        std::string filename;
        BufferModel buffer;
        bool dirty = false;
    };
    std::vector<Tab> m_tabs;
    size_t m_currentTab = 0;
    std::vector<HWND> m_tabButtons;
    BufferModel& currentBuffer() { return m_tabs[m_currentTab].buffer; }
    const BufferModel& currentBuffer() const { return m_tabs[m_currentTab].buffer; }

    SyntaxEngine m_engine;
    CppLanguagePlugin m_cppLang;
    PowerShellLanguagePlugin m_psLang;
    // Split layout pointer (opaque to header)
    void* m_splitLayout = nullptr; // cast to UI::SplitLayout* in implementation
    // Chat / AI state
    RawrXD::Backend::OllamaClient m_ollama{ "http://localhost:11434" };
    RawrXD::Session::AISession m_chatSession;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    bool m_chatBusy = false;
    std::mutex m_chatMutex;
    // Custom message for async chat completion
#ifdef _WIN32
    static constexpr UINT WM_CHAT_COMPLETE = WM_APP + 101;
#endif

    struct ThemeProfile {
        std::string name;
        unsigned bg{}; unsigned fg{}; unsigned keyword{}; unsigned number{}; unsigned ident{}; unsigned stringColor{}; unsigned commentColor{};
    };
        // Status bar & command palette
        HWND m_statusBarHwnd = nullptr;
        HWND m_commandPaletteHwnd = nullptr;
        void updateStatusBar();
        void createCommandPalette();
        void toggleCommandPalette();
        void populateCommandPalette();
        void executePaletteSelection(int index);

        // Persistence helpers
        void saveTab(size_t index);
        void saveAllDirtyTabs();
    std::vector<ThemeProfile> m_themes;
    size_t m_currentTheme = 0;
    UndoStack  m_undo;
    uint64_t   m_lastEditTick = 0; // for coalescing
    size_t     m_lastEditPos = 0;
    bool       m_lastWasInsert = false;
};