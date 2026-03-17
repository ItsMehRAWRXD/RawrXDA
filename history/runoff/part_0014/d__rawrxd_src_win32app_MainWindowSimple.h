#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <fstream>
#include <map>
#include <any>

#include "../../include/editor_buffer.h"
#include "../../include/syntax_engine.h"
#include "gguf_loader.hpp" 
#include "../cpu_inference_engine.h"

// Forward decls
namespace RawrXD::UI { 
    class SplitLayout; 
    class ChatPanel; 
    struct Pane { HWND hwnd; float ratio; };
}

namespace RawrXD::Backend { 
    struct OllamaChatMessage { std::string role; std::string content; }; 
    struct OllamaChatRequest { std::string model; bool stream; std::vector<OllamaChatMessage> messages; }; 
    struct OllamaResponse { struct { std::string content; } message; };
    class OllamaBackend { public: OllamaResponse chatSync(const OllamaChatRequest& r) { return {{""}}; } };
    class OllamaSession { 
    public: 
        void setSessionName(const std::string&) {}
        void recordUserPrompt(const std::string&) {}
        void recordAIResponse(const std::string&, const std::string&, uint64_t, uint64_t) {}
    };
}

struct AppState {
    int cpu_freq_mhz = 0;
    int gpu_freq_mhz = 0;
    bool governor_enabled = false;
};

struct Theme {
    std::string name;
    unsigned bg, fg, keyword, number, stringColor, commentColor, ident, type; 
};

struct Problem {
    std::string file;
    int line;
    std::string message;
    std::string severity;
};

struct Tab {
    std::string filename;
    EditorBuffer buffer;
    bool dirty = false;
};

struct EditCommand {
    size_t pos;
    std::string removed;
    std::string inserted;
};

class UndoStack {
public:
    bool canUndo() const { return !m_history.empty(); }
    bool canRedo() const { return !m_future.empty(); }
    void push(const EditCommand& cmd) { m_history.push_back(cmd); m_future.clear(); }
    EditCommand undo() { if(m_history.empty()) return {}; auto c=m_history.back(); m_history.pop_back(); m_future.push_back(c); return c; }
    EditCommand redo() { if(m_future.empty()) return {}; auto c=m_future.back(); m_future.pop_back(); m_history.push_back(c); return c; }
private:
    std::vector<EditCommand> m_history;
    std::vector<EditCommand> m_future;
};

// Menu IDs
#define IDM_FILE_NEW 100
#define IDM_FILE_NEW_WINDOW 101
#define IDM_FILE_OPEN 102
#define IDM_FILE_OPEN_FOLDER 103
#define IDM_FILE_SAVE 104
#define IDM_FILE_SAVEAS 105
#define IDM_FILE_AUTOSAVE 106
#define IDM_FILE_CLOSE_TAB 107
#define IDM_FILE_CLOSE_FOLDER 108
#define IDM_FILE_EXIT 109

#define IDM_EDIT_UNDO 200
#define IDM_EDIT_REDO 201
#define IDM_EDIT_CUT 202
#define IDM_EDIT_COPY 203
#define IDM_EDIT_PASTE 204
#define IDM_EDIT_FIND 205
#define IDM_EDIT_REPLACE 206
#define IDM_EDIT_GOTO_LINE 207
#define IDM_EDIT_SELECTALL 208
#define IDM_EDIT_TOGGLE_COMMENT 209
#define IDM_EDIT_MULTICURSOR_ADD 210
#define IDM_EDIT_MULTICURSOR_REMOVE 211

#define IDM_SEL_ALL 300
#define IDM_SEL_EXPAND 301
#define IDM_SEL_SHRINK 302
#define IDM_SEL_COLUMN_MODE 303
#define IDM_SEL_ADD_CURSOR_ABOVE 304
#define IDM_SEL_ADD_CURSOR_BELOW 305
#define IDM_SEL_ADD_NEXT_OCCURRENCE 306
#define IDM_SEL_SELECT_ALL_OCCURRENCES 307

#define IDM_VIEW_COMMAND_PALETTE 400
#define IDM_VIEW_ACTIVITY_BAR 401
#define IDM_VIEW_PRIMARY_SIDEBAR 402
#define IDM_VIEW_SECONDARY_SIDEBAR 403
#define IDM_VIEW_PANEL 404
#define IDM_VIEW_STATUS_BAR 405
#define IDM_VIEW_ZEN_MODE 406
#define IDM_VIEW_EXPLORER 407
#define IDM_VIEW_SEARCH 408
#define IDM_VIEW_SOURCE_CONTROL 409
#define IDM_VIEW_EXTENSIONS 410
#define IDM_VIEW_PROBLEMS 411
#define IDM_VIEW_OUTPUT 412
#define IDM_VIEW_TERMINAL 413
#define IDM_VIEW_MINIMAP 414
#define IDM_VIEW_WORD_WRAP 415
#define IDM_VIEW_LINE_NUMBERS 416

#define IDM_RUN_START_DEBUG 500
#define IDM_RUN_WITHOUT_DEBUG 501
#define IDM_RUN_STOP 502
#define IDM_RUN_RESTART 503
#define IDM_RUN_STEP_OVER 504
#define IDM_RUN_STEP_INTO 505
#define IDM_RUN_STEP_OUT 506
#define IDM_RUN_CONTINUE 507
#define IDM_RUN_TOGGLE_BREAKPOINT 508
#define IDM_RUN_CLEAR_BREAKPOINTS 509

#define IDM_TERM_NEW 600
#define IDM_TERM_SPLIT 601
#define IDM_TERM_PWSH 602
#define IDM_TERM_CMD 603
#define IDM_TERM_GITBASH 604
#define IDM_TERM_RUN_TASK 605
#define IDM_TERM_RUN_FILE 606
#define IDM_TERM_CLEAR 607
#define IDM_TERM_KILL 608

#define IDM_MODEL_LOAD 700
#define IDM_MODEL_UNLOAD 701
#define IDM_MODEL_INFO 702
#define IDM_MODEL_RELOAD 703
#define IDM_MODEL_SETTINGS 704

#define IDM_HELP_WELCOME 800
#define IDM_HELP_DOCS 801
#define IDM_HELP_TIPS_TRICKS 802
#define IDM_HELP_SHORTCUTS 803
#define IDM_HELP_RELEASE_NOTES 804
#define IDM_HELP_REPORT_ISSUE 805
#define IDM_HELP_CHECK_UPDATES 806
#define IDM_HELP_ABOUT 807

#define WM_CHAT_COMPLETE (WM_USER + 100)
#define WM_CUT 0x0300
#define WM_COPY 0x0301
#define WM_PASTE 0x0302

class MainWindow {
public:
    MainWindow();
    void show();
    int exec();
    void createWindow();
    
    void handleMenuCommand(WORD cmdId);
    void handleCommand(const std::string& cmd);

private:
    void createMenus();
    void createMenuBar();
    void createEditor();
    void createTabBar();
    void createTerminal();
    void createTerminalPane();
    void createLayoutPanes();
    void createCommandPalette();
    void createFloatingPanel();
    void createProblemsPanel();
    
    void addTab(const std::string& filename);
    void switchTab(size_t index);
    void closeTab(size_t index);
    void saveTab(size_t index);
    void saveAllDirtyTabs();
    void refreshTabBar();
    void selectLanguageForFile(const std::string& filename);
    void updateStatusBar();
    void initializeFileBrowser();
    void onFileBrowserDblClick();
    void initChat();
    void startChatRequest(const std::string& prompt);
    void handleChatResponse(const std::string& response);
    void appendTopChat(const std::string& who, const std::string& text);
    void loadFileWithLazyLoading(const std::string& filename);
    void syncEditorFromBuffer();
    void applyEdit(size_t pos, size_t eraseLen, std::string_view insertText);
    void performUndo();
    void performRedo();
    void retokenizeAndApplyColors();
    void sendToTerminal(const std::string& line);
    void appendTerminalOutput(const std::string& chunk);
    void startTerminalReader();
    void stopTerminalReader();
    void findNextInEditor(const std::string& searchText);
    void replaceNextInEditor(const std::string& findText, const std::string& replaceText);
    void replaceAllInEditor(const std::string& findText, const std::string& replaceText);
    void updateTelemetry();
    void updateAIMetricsDisplay();
    void loadGGUFModel();
    void unloadGGUFModel();
    void showModelInfo();
    void reloadCurrentModel();
    bool isModelLoaded() const;
    void loadSettings();
    void saveSettings();
    void toggleCommandPalette();
    void populateCommandPalette();
    void executePaletteSelection(int index);
    void toggleFloatingPanel();
    
    void runPesterTests();
    void buildWithMSBuild();
    void publishToGallery();
    void startRemoteSession(const std::string& remoteHost);
    void initExtensionSystem();
    void initRemoteDebugging();
    void initUnitTesting();
    void initBuildSystem();
    void initScriptPublishing();
    void wireOverclockPanel();
    void initPerformanceOpts();
    
    void addProblem(const std::string& file, int line, const std::string& message, const std::string& severity);
    void clearProblems();
    void autoRepairProblem(int problemIndex);
    void toggleProblemsPanel();
    void sortProblemsBySeverity();
    void filterProblemsByType(const std::string& type);
    void exportProblems(const std::string& filename);
    void jumpToProblem(int problemIndex);
    void showProblemDetails(int problemIndex);

    void startChatRequest(const std::string& prompt);
    void exportMetrics(const std::string& format);
    void clearMetrics();
    void showMetricsReport();

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

    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK FloatingPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    HWND m_hwnd;
    HWND m_editorHwnd;
    HWND m_terminalHwnd;
    HWND m_overclockHwnd;
    HWND m_floatingPanel;
    HWND m_problemsPanelHwnd;
    HWND m_findPanelHwnd;
    HWND m_findEditHwnd;
    HWND m_replaceEditHwnd;
    HWND m_findNextBtnHwnd;
    HWND m_replaceBtnHwnd;
    HWND m_replaceAllBtnHwnd;
    HWND m_statusBarHwnd;
    HWND m_tabBarHwnd;
    HWND m_fileBrowserHwnd;
    HWND m_commandPaletteHwnd;
    HWND m_topChatHwnd;
    HWND m_userChatInputHwnd;
    HWND m_userChatSendBtn;
    
    HMENU m_menuBar;

    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    
    bool m_terminalRunning;
    bool m_terminalReaderActive;
    bool m_floatingPanelVisible;
    bool m_problemsPanelVisible;
    bool m_autoSaveEnabled;
    bool m_columnSelectionMode;
    bool m_activityBarVisible;
    bool m_primarySidebarVisible;
    bool m_secondarySidebarVisible;
    bool m_panelVisible;
    bool m_statusBarVisible;
    bool m_zenModeEnabled;
    bool m_minimapEnabled;
    bool m_wordWrapEnabled;
    bool m_lineNumbersEnabled;
    bool m_modelLoaded;
    bool m_pesterAvailable = false;
    bool m_galleryReady = false;
    bool m_remoteDebugEnabled = false;
    bool m_autocompleteEnabled = true;
    bool m_useTabsForIndent = false;
    bool m_bracketMatchingEnabled = true;
    bool m_lazyLoadingEnabled = false;
    bool m_panelDragging = false;
    bool m_chatBusy = false;
    bool m_lastWasInsert = false;

    HANDLE m_psInWrite;
    HANDLE m_psOutRead;
    PROCESS_INFORMATION m_terminalProcess;
    
    std::shared_ptr<AppState> m_appState;
    std::vector<Theme> m_themes;
    std::vector<Tab> m_tabs;
    std::vector<HMODULE> m_loadedPlugins;
    std::vector<Problem> m_problems;
    std::vector<HWND> m_tabButtons;
    
    std::string m_windowTitle = "RawrXD IDE";
    std::string m_msbuildPath;
    std::string m_editorTheme = "dark";
    std::string m_fontName = "Consolas";
    std::string m_colorScheme = "default";
    std::string m_problemsFilter = "all";
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 0;
    long m_lastFindPos = -1;
    size_t m_lastEditPos = 0;
    uint64_t m_lastEditTick = 0;
    
    std::vector<std::string> m_editorBuffer; 
    
    POINT m_panelDragStart = {0,0};
    std::thread m_terminalReaderThread;
    
    struct ChatPanelShim { void* impl = nullptr; } m_chatPanelShim;
    RawrXD::UI::SplitLayout* m_splitLayout = nullptr;
    
    std::mutex m_chatMutex;
    RawrXD::Backend::OllamaSession m_chatSession;
    RawrXD::Backend::OllamaBackend m_ollama;
    std::vector<RawrXD::Backend::OllamaChatMessage> m_chatHistory;
    
    UndoStack m_undo;
    SyntaxEngine m_engine;
    LanguageDefinition m_cppLang;
    LanguageDefinition m_psLang;
    
    std::unique_ptr<GGUFLoaderQt> m_ggufLoader;
    std::unique_ptr<RawrXD::CPUInferenceEngine> m_localInferenceEngine;
    bool m_modelLoaded;
    std::string m_currentModelPath;
    
    size_t m_currentTheme = 0;
    size_t m_currentTab = 0;
    int m_fontSize = 11;
    int m_tabSize = 4;
    size_t m_maxFileSizeForLazyLoad = 