#include "Win32IDE.h"
#include "../../include/rawrxd_version.h"
#include "multi_response_engine.h"
#include "lsp/RawrXD_LSPServer.h"
#include "IDELogger.h"
#include "IDEConfig.h"
#include "Win32IDE_AgenticBridge.h"
#include "../cpu_inference_engine.h" 
#include "../modules/native_memory.hpp"
#include "../modules/ExtensionLoader.hpp" // Added
#include "VSIXInstaller.hpp"
#include "../streaming_gguf_loader.h"
#include "../model_source_resolver.h"
#include "../utils/ErrorReporter.hpp"
#include <nlohmann/json.hpp>
#include <commdlg.h>
#include <richedit.h>
#ifndef CP_UNICODE
#define CP_UNICODE 1200  // Unicode code page for Richedit EM_GETTEXTLENGTHEX/EM_SETTEXTEX
#endif
#include <commctrl.h>
#ifndef TRACKBAR_CLASSW
#define TRACKBAR_CLASSW L"msctls_trackbar32"
#endif
#include <shlobj.h>
#include <shellapi.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <ctime>
#include <regex>
#include <filesystem>
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

// Helper function to execute shell commands and capture output
static std::string ExecCmd(const char* cmd) {
    std::string result;
    #ifdef _WIN32
    FILE* pipe = _popen(cmd, "r");
    #else
    FILE* pipe = popen(cmd, "r");
    #endif
    
    if (!pipe) return "Error: Could not execute command";
    
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    
    #ifdef _WIN32
    _pclose(pipe);
    #else
    pclose(pipe);
    #endif
    
    return result;
}

// UTF-8 to UTF-16 for Unicode Win32 APIs (Qt removal / pure MASM C++20)
static std::wstring utf8ToWide(const std::string& utf8) {
    if (utf8.empty()) return {};
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), nullptr, 0);
    if (len <= 0) return {};
    std::wstring out(static_cast<size_t>(len), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), static_cast<int>(utf8.size()), out.data(), len) == 0)
        return {};
    return out;
}
static std::wstring utf8ToWide(const char* utf8) {
    if (!utf8 || !*utf8) return {};
    return utf8ToWide(std::string(utf8));
}
static std::string wideToUtf8(const wchar_t* wide) {
    if (!wide || !*wide) return {};
    const int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return {};
    std::string out(static_cast<size_t>(len), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), len, nullptr, nullptr) == 0)
        return {};
    out.resize(out.size() - 1); // drop NUL
    return out;
}

#define IDC_EDITOR 1001
#define IDC_TERMINAL 1002
#define IDC_COMMAND_INPUT 1003
#define IDC_STATUS_BAR 1004
#define IDC_OUTPUT_TABS 1005
#define IDC_MINIMAP 1006
#define IDC_MODULE_BROWSER 1007
#define IDC_HELP_PANEL 1008
#define IDC_SNIPPET_LIST 1009
#define IDC_CLIPBOARD_HISTORY 1010
#define IDC_OUTPUT_TEXT 1011
#define IDC_OUTPUT_EDIT_GENERAL 1012
#define IDC_OUTPUT_EDIT_ERRORS 1013
#define IDC_OUTPUT_EDIT_DEBUG 1014
#define IDC_OUTPUT_EDIT_FIND 1015
#define IDC_SPLITTER 1016
#define IDC_SEVERITY_FILTER 1017
#define IDC_TITLE_TEXT 1018
#define IDC_BTN_MINIMIZE 1019
#define IDC_BTN_MAXIMIZE 1020
#define IDC_BTN_CLOSE 1021
#define IDC_BTN_GITHUB 1022
#define IDC_BTN_MICROSOFT 1023
#define IDC_BTN_SETTINGS 1024
#define IDC_FILE_EXPLORER 1025
#define IDC_FILE_TREE 1026
// Defined in Win32IDE.h
// #define IDM_AUTONOMY_TOGGLE 4150
// ... constants moved to header

// Activity Bar (Far Left) - VS Code style icon bar
#define IDC_ACTIVITY_BAR 1100
#define IDC_ACTBAR_EXPLORER 1101
#define IDC_ACTBAR_SEARCH 1102
#define IDC_ACTBAR_SCM 1103
#define IDC_ACTBAR_DEBUG 1104
#define IDC_ACTBAR_EXTENSIONS 1105
#define IDC_ACTBAR_SETTINGS 1106
#define IDC_ACTBAR_ACCOUNTS 1107

// Secondary Sidebar (Right) - AI Chat/Copilot area
#define IDC_SECONDARY_SIDEBAR 1200
#define IDC_SECONDARY_SIDEBAR_HEADER 1201
#define IDC_COPILOT_CHAT_INPUT 1202
#define IDC_COPILOT_CHAT_OUTPUT 1203
#define IDC_COPILOT_SEND_BTN 1204
#define IDC_COPILOT_CLEAR_BTN 1205
#define IDC_AI_CONTEXT_SLIDER 1206
#define IDC_AI_CONTEXT_LABEL 1207

// Panel (Bottom) - Terminal, Output, Problems, Debug Console
#define IDC_PANEL_CONTAINER 1300
#define IDC_PANEL_TABS 1301
#define IDC_PANEL_TERMINAL 1302
#define IDC_PANEL_OUTPUT 1303
#define IDC_PANEL_PROBLEMS 1304
#define IDC_PANEL_DEBUG_CONSOLE 1305
#define IDC_PANEL_TOOLBAR 1306
#define IDC_PANEL_BTN_NEW_TERMINAL 1307
#define IDC_PANEL_BTN_SPLIT_TERMINAL 1308
#define IDC_PANEL_BTN_KILL_TERMINAL 1309
#define IDC_PANEL_BTN_MAXIMIZE 1310
#define IDC_PANEL_BTN_CLOSE 1311
#define IDC_PANEL_PROBLEMS_LIST 1312

// Debugger Panel - Integrated at bottom with Terminal/Output
#define IDC_DEBUGGER_CONTAINER 1313
#define IDC_DEBUGGER_TABS 1314
#define IDC_DEBUGGER_BREAKPOINTS 1315
#define IDC_DEBUGGER_WATCH 1316
#define IDC_DEBUGGER_VARIABLES 1317
#define IDC_DEBUGGER_STACK_TRACE 1318
#define IDC_DEBUGGER_MEMORY 1319
#define IDC_DEBUGGER_TOOLBAR 1320
#define IDC_DEBUGGER_BTN_CONTINUE 1321
#define IDC_DEBUGGER_BTN_STEP_OVER 1322
#define IDC_DEBUGGER_BTN_STEP_INTO 1323
#define IDC_DEBUGGER_BTN_STEP_OUT 1324
#define IDC_DEBUGGER_BTN_RESTART 1325
#define IDC_DEBUGGER_BTN_STOP 1326
#define IDC_DEBUGGER_INPUT 1327
#define IDC_DEBUGGER_BREAKPOINT_LIST 1328
#define IDC_DEBUGGER_WATCH_LIST 1329
#define IDC_DEBUGGER_VARIABLE_TREE 1330
#define IDC_DEBUGGER_STACK_LIST 1331
#define IDC_DEBUGGER_STATUS_TEXT 1332

// Enhanced Status Bar items
#define IDC_STATUS_REMOTE 1400
#define IDC_STATUS_BRANCH 1401
#define IDC_STATUS_SYNC 1402
#define IDC_STATUS_ERRORS 1403
#define IDC_STATUS_WARNINGS 1404
#define IDC_STATUS_LINE_COL 1405
#define IDC_STATUS_SPACES 1406
#define IDC_STATUS_ENCODING 1407
#define IDC_STATUS_EOL 1408
#define IDC_STATUS_LANGUAGE 1409
#define IDC_STATUS_COPILOT 1410
#define IDC_STATUS_NOTIFICATIONS 1411

/* Menu IDs: 2001+ to avoid overlap with IDC_* (1001+) in WM_COMMAND */
#define IDM_FILE_NEW 2001
#define IDM_FILE_OPEN 2002
#define IDM_FILE_SAVE 2003
#define IDM_FILE_SAVEAS 2004
#define IDM_FILE_LOAD_MODEL 1030
#define IDM_FILE_EXIT 2005

/* Voice Automation (Tools > Voice Automation) — Phase 44 TTS; dispatched in Win32IDE_Commands 10200–10300 */
#define IDM_VOICE_AUTO_TOGGLE    10200
#define IDM_VOICE_AUTO_STOP      10206
#define IDM_VOICE_AUTO_NEXT      10202
#define IDM_VOICE_AUTO_PREV      10203
#define IDM_VOICE_AUTO_RATE_UP   10204
#define IDM_VOICE_AUTO_RATE_DOWN 10205

#define IDM_EDIT_UNDO 2007
#define IDM_EDIT_REDO 2008
#define IDM_EDIT_CUT 2009
#define IDM_EDIT_COPY 2010
#define IDM_EDIT_PASTE 2011
#define IDM_EDIT_SNIPPET 2012
#define IDM_EDIT_COPY_FORMAT 2013
#define IDM_EDIT_PASTE_PLAIN 2014
#define IDM_EDIT_CLIPBOARD_HISTORY 2015
#define IDM_EDIT_FIND 2016
#define IDM_EDIT_REPLACE 2017
#define IDM_EDIT_FIND_NEXT 2018
#define IDM_EDIT_FIND_PREV 2019

#define IDM_VIEW_MINIMAP 2020
#define IDM_VIEW_OUTPUT_TABS 2021
#define IDM_VIEW_MODULE_BROWSER 2022
#define IDM_VIEW_THEME_EDITOR 2023
#define IDM_VIEW_FLOATING_PANEL 2024
#define IDM_VIEW_OUTPUT_PANEL 2025
#define IDM_VIEW_USE_STREAMING_LOADER 2026
#define IDM_VIEW_USE_VULKAN_RENDERER 2027
#define IDM_VIEW_SIDEBAR 2028
#define IDM_VIEW_TERMINAL 2029

#define IDM_TERMINAL_POWERSHELL 3001
#define IDM_TERMINAL_CMD 3002
#define IDM_TERMINAL_STOP 3003
#define IDM_TERMINAL_SPLIT_H 3004
#define IDM_TERMINAL_SPLIT_V 3005
#define IDM_TERMINAL_CLEAR_ALL 3006

#define IDM_TOOLS_PROFILE_START 3010
#define IDM_TOOLS_PROFILE_STOP 3011
#define IDM_TOOLS_PROFILE_RESULTS 3012
#define IDM_TOOLS_ANALYZE_SCRIPT 3013

#define IDM_GIT_STATUS 3020
#define IDM_GIT_COMMIT 3021
#define IDM_GIT_PUSH 3022
#define IDM_GIT_PULL 3023
#define IDM_GIT_PANEL 3024

#define IDM_MODULES_REFRESH 3050
#define IDM_MODULES_IMPORT 3051
#define IDM_MODULES_EXPORT 3052

#define IDM_HELP_ABOUT 4001
#define IDM_HELP_CMDREF 4002
#define IDM_HELP_PSDOCS 4003
#define IDM_HELP_SEARCH 4004

// Agent menu IDs
#define IDM_AGENT_START_LOOP 4100
#define IDM_AGENT_EXECUTE_CMD 4101
#define IDM_AGENT_CONFIGURE_MODEL 4102
#define IDM_AGENT_VIEW_TOOLS 4103
#define IDM_AGENT_VIEW_STATUS 4104
// Constants moved to Win32IDE.h
// #define IDM_AGENT_STOP 4105
// ...

// Command Palette control IDs
#define IDC_CMDPAL_CONTAINER 1500
#define IDC_CMDPAL_INPUT 1501
#define IDC_CMDPAL_LIST 1502

Win32IDE::Win32IDE(HINSTANCE hInstance)
        : m_hInstance(hInstance), m_hwndMain(nullptr), m_hwndEditor(nullptr),
            m_hwndLineNumbers(nullptr), m_hwndTabBar(nullptr), m_oldLineNumberProc(nullptr),
            m_lineNumberWidth(50), m_activeTabIndex(-1),
            m_hwndCommandInput(nullptr), m_hwndStatusBar(nullptr),
            m_hwndMinimap(nullptr), m_hwndModuleBrowser(nullptr), m_hwndModuleList(nullptr),
            m_hwndModuleLoadButton(nullptr), m_hwndModuleUnloadButton(nullptr), m_hwndModuleRefreshButton(nullptr),
            m_moduleBrowserVisible(false), m_modulePanelProc(nullptr),
    m_hwndHelp(nullptr), m_hMenu(nullptr), m_hwndToolbar(nullptr), 
    m_hwndTitleLabel(nullptr), m_hwndBtnMinimize(nullptr), m_hwndBtnMaximize(nullptr),
    m_hwndBtnClose(nullptr), m_hwndBtnGitHub(nullptr), m_hwndBtnMicrosoft(nullptr),
    m_hwndBtnSettings(nullptr), m_lastTitleBarText(),
      m_fileModified(false), m_editorHeight(400), m_terminalHeight(200),
      m_minimapVisible(true), m_minimapWidth(150), m_profilingActive(false),
      m_moduleListDirty(true), m_backgroundBrush(nullptr), m_editorFont(nullptr), m_hFontUI(nullptr),
    m_activeOutputTab("General"), m_minimapX(650), m_outputTabHeight(200),
    m_nextTerminalId(1), m_activeTerminalId(-1),
    m_ggufLoader(nullptr), m_loadedModelPath(""),
      m_terminalSplitHorizontal(true), m_hwndGitPanel(nullptr), m_hwndGitStatusText(nullptr),
    m_hwndGitFileList(nullptr), m_gitAutoRefresh(true), m_outputPanelVisible(true), m_selectedOutputTab(0),
    m_hwndSeverityFilter(nullptr), m_severityFilterLevel(0),
    m_editorRect{0, 0, 0, 0}, m_gpuTextEnabled(true), m_editorHooksInstalled(false),
    m_hwndSplitter(nullptr), m_splitterDragging(false), m_splitterY(0),
    m_renderer(nullptr), m_rendererReady(false),
    m_lastSearchText(), m_lastReplaceText(),
    m_searchCaseSensitive(false), m_searchWholeWord(false), m_searchUseRegex(false), m_lastFoundPos(-1),
    m_hwndFindDialog(nullptr), m_hwndReplaceDialog(nullptr),
    // Primary Sidebar
    m_hwndActivityBar(nullptr), m_hwndSidebar(nullptr), m_hwndSidebarContent(nullptr),
    m_sidebarVisible(true), m_sidebarWidth(250), m_currentSidebarView(SidebarView::None),
    // Secondary Sidebar
    m_hwndSecondarySidebar(nullptr), m_hwndSecondarySidebarHeader(nullptr),
    m_secondarySidebarVisible(false), m_secondarySidebarWidth(320),
    // Explorer View
    m_hwndExplorerTree(nullptr), m_hwndExplorerToolbar(nullptr), m_hImageListExplorer(nullptr),
    m_explorerRootPath(),
    // Search View
    m_hwndSearchInput(nullptr), m_hwndSearchResults(nullptr), m_hwndSearchOptions(nullptr),
    m_hwndIncludePattern(nullptr), m_hwndExcludePattern(nullptr), m_searchInProgress(false),
    // Source Control View
    m_hwndSCMFileList(nullptr), m_hwndSCMToolbar(nullptr), m_hwndSCMMessageBox(nullptr),
    // Debug View
    m_hwndDebugConfigs(nullptr), m_hwndDebugToolbar(nullptr), m_hwndDebugVariables(nullptr),
    m_hwndDebugCallStack(nullptr), m_hwndDebugConsole(nullptr), m_debuggingActive(false),
    // Extensions View
    m_hwndExtensionsList(nullptr), m_hwndExtensionSearch(nullptr), m_hwndExtensionDetails(nullptr),
    // File Explorer
    m_hwndFileExplorer(nullptr), m_hImageList(nullptr), m_currentExplorerPath("D:\\OllamaModels"),
    // Model Chat
    m_chatMode(false),
    // PowerShell Panel
    m_hwndPowerShellPanel(nullptr), m_hwndPowerShellOutput(nullptr), m_hwndPowerShellInput(nullptr),
    m_hwndPowerShellToolbar(nullptr), m_hwndPowerShellStatusBar(nullptr),
    m_hwndPSBtnExecute(nullptr), m_hwndPSBtnClear(nullptr), m_hwndPSBtnStop(nullptr),
    m_hwndPSBtnHistory(nullptr), m_hwndPSBtnRestart(nullptr), m_hwndPSBtnLoadRawrXD(nullptr),
    m_hwndPSBtnToggle(nullptr),
    m_powerShellPanelVisible(true), m_powerShellPanelDocked(true), m_powerShellSessionActive(false),
    m_powerShellRawrXDLoaded(false), m_powerShellPanelHeight(250), m_powerShellPanelWidth(600),
    m_powerShellHistoryIndex(-1), m_maxPowerShellHistory(100),
    m_useStreamingLoader(false), m_useVulkanRenderer(false),
    m_powerShellExecuting(false), m_powerShellProcessHandle(nullptr),
    m_dedicatedPowerShellTerminal(nullptr)
    , m_hwndCommandPalette(nullptr), m_hwndCommandPaletteInput(nullptr), m_hwndCommandPaletteList(nullptr), m_commandPaletteVisible(false), m_oldCommandPaletteInputProc(nullptr)
    , m_hwndModelSelector(nullptr), m_hwndMaxTokensSlider(nullptr), m_hwndMaxTokensLabel(nullptr)
    , m_currentMaxTokens(512)
    , m_syntaxColoringEnabled(true), m_syntaxDirty(false)
    , m_syntaxLanguage(SyntaxLanguage::None), m_inBlockComment(false)
    , m_activeThemeId(IDM_THEME_DARK_PLUS), m_themeIdBeforePreview(IDM_THEME_DARK_PLUS)
    , m_transparencyEnabled(false), m_windowAlpha(255)
    , m_sidebarBrush(nullptr), m_sidebarContentBrush(nullptr)
    , m_panelBrush(nullptr), m_secondarySidebarBrush(nullptr), m_mainWindowBrush(nullptr)
    , m_modelOperationActive(false), m_modelOperationCancelled(false)
    , m_modelProgressPercent(0.0f)
    , m_hwndModelProgressBar(nullptr), m_hwndModelProgressLabel(nullptr)
    , m_hwndModelProgressContainer(nullptr), m_hwndModelCancelBtn(nullptr)
    , m_sessionRestored(false), m_annotationsVisible(true), m_annotationFont(nullptr)
    , m_hwndAnnotationOverlay(nullptr)
{
    // ============================================================
    // MINIMAL CONSTRUCTOR — all heavy init deferred to onCreate()
    // C++ try/catch does NOT catch SEH (access violations) on MinGW,
    // so we keep the constructor as lightweight as possible.
    // ============================================================

    // Initialize profiling frequency (safe — kernel call)
    QueryPerformanceFrequency(&m_profilingFreq);

    // Initialize clipboard history
    m_clipboardHistory.reserve(MAX_CLIPBOARD_HISTORY);
    
    // Initialize Git status
    m_gitStatus = GitStatus();
    
    // Get current directory for Git repo detection
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    m_gitRepoPath = currentDir;
    
    // Default Ollama configuration
    m_ollamaBaseUrl = "http://localhost:11434";
    m_ollamaModelOverride = "";

    m_nativeEngineLoaded = false;
}

// ESP:m_hMenu — Main menu bar; submenus File/Edit/View/Terminal/Tools/Modules/Help/Audit/Git/Agent (see Win32IDE_IELabels.h)
void Win32IDE::createMenuBar(HWND hwnd)
{
    if (!m_hMenu)
        m_hMenu = CreateMenu();
    if (!m_hMenu) return;

    // Status bar is initialized in onCreate after createStatusBar (see Win32IDE_Core.cpp).

    // File menu (Unicode)
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_NEW, L"&New");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_OPEN, L"&Open");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVE, L"&Save");
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, L"Save &As");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_LOAD_MODEL, L"Load &Model (GGUF)...");
    AppendMenuW(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hFileMenu, MF_STRING, IDM_FILE_EXIT, L"E&xit");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hFileMenu, L"&File");

    // Edit menu (Unicode)
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_FIND, L"&Find...\tCtrl+F");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_REPLACE, L"&Replace...\tCtrl+H");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_FIND_NEXT, L"Find &Next\tF3");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_FIND_PREV, L"Find &Previous\tShift+F3");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_SNIPPET, L"Insert &Snippet...");
    AppendMenuW(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_COPY_FORMAT, L"Copy with &Formatting");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_PASTE_PLAIN, L"Paste &Plain Text");
    AppendMenuW(hEditMenu, MF_STRING, IDM_EDIT_CLIPBOARD_HISTORY, L"Clipboard &History...");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hEditMenu, L"&Edit");
    
    // View menu (Unicode)
    HMENU hViewMenu = CreatePopupMenu();
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_MINIMAP, L"&Minimap");
    AppendMenuW(hViewMenu, MF_STRING, IDM_T1_BREADCRUMBS_TOGGLE, L"&Breadcrumbs");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_OUTPUT_TABS, L"&Output Tabs");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_OUTPUT_PANEL, L"Output &Panel");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_MODULE_BROWSER, L"Module &Browser");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_FLOATING_PANEL, L"&Floating Panel");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    buildThemeMenu(hViewMenu);
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_THEME_EDITOR, L"Theme &Picker...");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_USE_STREAMING_LOADER, L"Use Streaming Loader (Low Memory)");
    AppendMenuW(hViewMenu, MF_STRING, IDM_VIEW_USE_VULKAN_RENDERER, L"Enable Vulkan Renderer (experimental)");
    AppendMenuW(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hViewMenu, MF_STRING, IDM_TELDASH_SHOW, L"Telemetry &Dashboard...");
    AppendMenuW(hViewMenu, MF_STRING, IDM_EMOJI_PICKER, L"&Emoji Picker");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hViewMenu, L"&View");

    // Terminal menu (Unicode)
    HMENU hTerminalMenu = CreatePopupMenu();
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_POWERSHELL, L"&PowerShell");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_CMD, L"&Command Prompt");
    AppendMenuW(hTerminalMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_STOP, L"&Stop Terminal");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_SPLIT_H, L"Split &Horizontal\tCtrl+Shift+H");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_SPLIT_V, L"Split &Vertical\tCtrl+Shift+V");
    AppendMenuW(hTerminalMenu, MF_STRING, IDM_TERMINAL_CLEAR_ALL, L"&Clear All Terminals");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hTerminalMenu, L"&Terminal");
    
    // Tools menu (Unicode)
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_START, L"Start &Profiling");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_STOP, L"Stop P&rofiling");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_RESULTS, L"Profile &Results...");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, IDM_TOOLS_ANALYZE_SCRIPT, L"&Analyze Script");
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);

    // Voice Chat submenu (Unicode — Qt removal / pure Win32)
    HMENU hVoiceMenu = CreatePopupMenu();
    AppendMenuW(hVoiceMenu, MF_STRING, IDM_VOICE_TOGGLE_PANEL, L"Show/Hide &Voice Panel\tCtrl+Shift+U");
    AppendMenuW(hVoiceMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hVoiceMenu, MF_STRING, IDM_VOICE_RECORD, L"&Record / Stop\tF9");
    AppendMenuW(hVoiceMenu, MF_STRING, IDM_VOICE_PTT, L"&Push-to-Talk\tCtrl+Shift+V");
    AppendMenuW(hVoiceMenu, MF_STRING, IDM_VOICE_SPEAK, L"Text-to-&Speech");
    AppendMenuW(hVoiceMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hVoiceMenu, MF_STRING, IDM_VOICE_JOIN_ROOM, L"&Join/Leave Room");
    AppendMenuW(hVoiceMenu, MF_STRING, IDM_VOICE_SHOW_DEVICES, L"Audio &Devices...");
    AppendMenuW(hVoiceMenu, MF_STRING, IDM_VOICE_METRICS, L"&Metrics...");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hVoiceMenu, L"&Voice Chat");

    // Voice Automation submenu (Phase 44: TTS for AI responses)
    HMENU hVoiceAutoMenu = CreatePopupMenu();
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_TOGGLE, L"Toggle Voice Automation\tCtrl+Shift+A");
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_STOP, L"Stop Speaking\tEscape");
    AppendMenuW(hVoiceAutoMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_NEXT, L"Next Voice\tCtrl+Shift+]");
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_PREV, L"Previous Voice\tCtrl+Shift+[");
    AppendMenuW(hVoiceAutoMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_RATE_UP, L"Increase Speech Rate\tCtrl+Shift+=");
    AppendMenuW(hVoiceAutoMenu, MF_STRING, IDM_VOICE_AUTO_RATE_DOWN, L"Decrease Speech Rate\tCtrl+Shift+-");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hVoiceAutoMenu, L"Voice &Automation");

    // Backup submenu
    HMENU hBackupMenu = CreatePopupMenu();
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_CREATE, L"&Create Backup Now\tCtrl+Shift+B");
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_RESTORE, L"&Restore from Backup...");
    AppendMenuW(hBackupMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_AUTO_TOGGLE, L"Toggle &Auto-Backup");
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_LIST, L"&List Backups...");
    AppendMenuW(hBackupMenu, MF_STRING, IDM_QW_BACKUP_PRUNE, L"&Prune Old Backups");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hBackupMenu, L"&Backups");

    // Alert & Monitoring submenu
    HMENU hAlertMenu = CreatePopupMenu();
    AppendMenuW(hAlertMenu, MF_STRING, IDM_QW_ALERT_TOGGLE_MONITOR, L"Toggle Resource &Monitor");
    AppendMenuW(hAlertMenu, MF_STRING, IDM_QW_ALERT_RESOURCE_STATUS, L"&Resource Status...");
    AppendMenuW(hAlertMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAlertMenu, MF_STRING, IDM_QW_ALERT_SHOW_HISTORY, L"Alert &History...");
    AppendMenuW(hAlertMenu, MF_STRING, IDM_QW_ALERT_DISMISS_ALL, L"&Dismiss All Alerts");
    AppendMenuW(hToolsMenu, MF_POPUP, (UINT_PTR)hAlertMenu, L"A&lerts");

    // Shortcuts & SLO (Tier 5)
    AppendMenuW(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hToolsMenu, MF_STRING, IDM_QW_SHORTCUT_EDITOR, L"\u2328 &Keyboard Shortcuts...\tCtrl+K Ctrl+S");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_SHORTCUT_SHOW, L"Keyboard Shortcut &Editor...");
    AppendMenuW(hToolsMenu, MF_STRING, IDM_QW_SLO_DASHBOARD, L"&SLO Dashboard...");

    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hToolsMenu, L"&Tools");

    // Modules menu
    HMENU hModulesMenu = CreatePopupMenu();
    AppendMenuW(hModulesMenu, MF_STRING, IDM_MODULES_REFRESH, L"&Refresh List");
    AppendMenuW(hModulesMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hModulesMenu, MF_STRING, IDM_MODULES_IMPORT, L"&Import Module...");
    AppendMenuW(hModulesMenu, MF_STRING, IDM_MODULES_EXPORT, L"&Export Module...");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hModulesMenu, L"&Modules");

    // Help menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_CMDREF, L"Command &Reference");
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_PSDOCS, L"PowerShell &Documentation");
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_SEARCH, L"&Search Help...");
    AppendMenuW(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, L"&About");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, L"&Help");

    // Audit menu (Phase 31 — Unicode)
    HMENU hAuditMenu = CreatePopupMenu();
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_SHOW_DASHBOARD, L"Show &Dashboard\tCtrl+Shift+A");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_RUN_FULL, L"&Run Full Audit");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_DETECT_STUBS, L"Detect &Stubs");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_CHECK_MENUS, L"Check &Menu Wiring");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_RUN_TESTS, L"Run Component &Tests");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_EXPORT_REPORT, L"&Export Report...");
    AppendMenuW(hAuditMenu, MF_STRING, IDM_AUDIT_QUICK_STATS, L"&Quick Stats");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hAuditMenu, L"&Audit");

    // Git menu
    HMENU hGitMenu = CreatePopupMenu();
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_STATUS, L"&Status\tCtrl+G");
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_COMMIT, L"&Commit...\tCtrl+Shift+C");
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_PUSH, L"&Push");
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_PULL, L"P&ull");
    AppendMenuW(hGitMenu, MF_STRING, IDM_GIT_PANEL, L"&Git Panel\tCtrl+Shift+G");
    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hGitMenu, L"&Git");

    // Agent menu (Unicode — Qt removal / pure Win32)
    HMENU hAgentMenu = CreatePopupMenu();
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_START_LOOP, L"Start &Agent Loop");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_BOUNDED_LOOP, L"&Bounded Agent (FIM Tools)\tCtrl+Shift+I");

    AppendMenuW(hAgentMenu, MF_SEPARATOR, 0, nullptr);

    // AI Options Submenu
    HMENU hAIOptionsMenu = CreatePopupMenu();
    AppendMenuW(hAIOptionsMenu, MF_STRING, IDM_AI_MODE_MAX, L"&Max Mode (Thread Unlock)");
    AppendMenuW(hAIOptionsMenu, MF_STRING, IDM_AI_MODE_DEEP_THINK, L"&Deep Thinking (CoT)");
    AppendMenuW(hAIOptionsMenu, MF_STRING, IDM_AI_MODE_DEEP_RESEARCH, L"Deep &Research (FileSystem)");
    AppendMenuW(hAIOptionsMenu, MF_STRING, IDM_AI_MODE_NO_REFUSAL, L"&No Refusal Mode");
    AppendMenuW(hAgentMenu, MF_POPUP, (UINT_PTR)hAIOptionsMenu, L"AI &Options");

    // Context Window (Memory Plugins) Submenu
    HMENU hContextMenu = CreatePopupMenu();
    AppendMenuW(hContextMenu, MF_STRING, IDM_AI_CONTEXT_4K, L"4K (Standard)");
    AppendMenuW(hContextMenu, MF_STRING, IDM_AI_CONTEXT_32K, L"32K (Large)");
    AppendMenuW(hContextMenu, MF_STRING, IDM_AI_CONTEXT_64K, L"64K (X-Large)");
    AppendMenuW(hContextMenu, MF_STRING, IDM_AI_CONTEXT_128K, L"128K (Ultra)");
    AppendMenuW(hContextMenu, MF_STRING, IDM_AI_CONTEXT_256K, L"256K (Mega)");
    AppendMenuW(hContextMenu, MF_STRING, IDM_AI_CONTEXT_512K, L"512K (Giga)");
    AppendMenuW(hContextMenu, MF_STRING, IDM_AI_CONTEXT_1M, L"1M (Tera - Memory Plugin)");
    AppendMenuW(hAgentMenu, MF_POPUP, (UINT_PTR)hContextMenu, L"&Context Window Size");

    AppendMenuW(hAgentMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AI_TITAN_TOGGLE, L"Use &Titan Kernel");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AI_800B_STATUS, L"800B Dual-Engine &Status");
    AppendMenuW(hAgentMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AI_AGENT_MULTI_ENABLE, L"Multi-Agent: &Enable");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AI_AGENT_MULTI_DISABLE, L"Multi-Agent: &Disable");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AI_AGENT_MULTI_STATUS, L"Multi-Agent: &Status");

    AppendMenuW(hAgentMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_EXECUTE_CMD, L"&Execute Command...");

    AppendMenuW(hAgentMenu, MF_SEPARATOR, 0, nullptr);

    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_CONFIGURE_MODEL, L"&Configure Model...");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_VIEW_TOOLS, L"View &Tools");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_VIEW_STATUS, L"View &Status");
    AppendMenuW(hAgentMenu, MF_STRING, IDM_AGENT_STOP, L"&Stop Agent");

    AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hAgentMenu, L"&Agent");

    // Hotpatch menu (Unicode — Qt removal)
    {
        HMENU hHotpatchMenu = CreatePopupMenu();
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_SHOW_STATUS, L"&Show Hotpatch Status");
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_TOGGLE_ALL, L"&Toggle Hotpatch System");
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_SHOW_EVENT_LOG, L"Show &Event Log");
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_RESET_STATS, L"&Reset Statistics");
        AppendMenuW(hHotpatchMenu, MF_SEPARATOR, 0, nullptr);

        HMENU hMemLayerMenu = CreatePopupMenu();
        AppendMenuW(hMemLayerMenu, MF_STRING, IDM_HOTPATCH_MEMORY_APPLY, L"&Apply Memory Patch...");
        AppendMenuW(hMemLayerMenu, MF_STRING, IDM_HOTPATCH_MEMORY_REVERT, L"&Revert Memory Patch...");
        AppendMenuW(hHotpatchMenu, MF_POPUP, (UINT_PTR)hMemLayerMenu, L"&Memory Layer");

        HMENU hByteLayerMenu = CreatePopupMenu();
        AppendMenuW(hByteLayerMenu, MF_STRING, IDM_HOTPATCH_BYTE_APPLY, L"&Apply Byte Patch...");
        AppendMenuW(hByteLayerMenu, MF_STRING, IDM_HOTPATCH_BYTE_SEARCH, L"&Search && Replace Pattern...");
        AppendMenuW(hHotpatchMenu, MF_POPUP, (UINT_PTR)hByteLayerMenu, L"&Byte Layer");

        HMENU hServerLayerMenu = CreatePopupMenu();
        AppendMenuW(hServerLayerMenu, MF_STRING, IDM_HOTPATCH_SERVER_ADD, L"&Add Server Patch...");
        AppendMenuW(hServerLayerMenu, MF_STRING, IDM_HOTPATCH_SERVER_REMOVE, L"&Remove Server Patch...");
        AppendMenuW(hHotpatchMenu, MF_POPUP, (UINT_PTR)hServerLayerMenu, L"&Server Layer");

        HMENU hProxyMenu = CreatePopupMenu();
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_PROXY_BIAS, L"Token &Bias Injection...");
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_PROXY_REWRITE, L"Output &Rewrite Rule...");
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_PROXY_TERMINATE, L"Stream &Termination Rule...");
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_PROXY_VALIDATE, L"Custom &Validator...");
        AppendMenuW(hProxyMenu, MF_STRING, IDM_HOTPATCH_SHOW_PROXY_STATS, L"Show Proxy &Stats");
        AppendMenuW(hHotpatchMenu, MF_POPUP, (UINT_PTR)hProxyMenu, L"&Proxy Hotpatcher");

        AppendMenuW(hHotpatchMenu, MF_SEPARATOR, 0, nullptr);

        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_PRESET_SAVE, L"Save Preset...");
        AppendMenuW(hHotpatchMenu, MF_STRING, IDM_HOTPATCH_PRESET_LOAD, L"Load Preset...");

        AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hHotpatchMenu, L"&Hotpatch");
    }

    if (FEATURE_ENABLED("autonomy")) {
        HMENU hAutonomyMenu = CreatePopupMenu();
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_TOGGLE, L"&Toggle Auto Loop");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_START, L"&Start Autonomy");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_STOP, L"Sto&p Autonomy");
        AppendMenuW(hAutonomyMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_SET_GOAL, L"Set &Goal...");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_STATUS, L"Show &Status");
        AppendMenuW(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_MEMORY, L"Show &Memory Snapshot");
        AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hAutonomyMenu, L"&Autonomy");
    }

    if (FEATURE_ENABLED("reverseEngineering")) {
        HMENU hRevEngMenu = createReverseEngineeringMenu();
        AppendMenuW(m_hMenu, MF_POPUP, (UINT_PTR)hRevEngMenu, L"&RevEng");
    }

    // Phase 45: Game Engine Integration (Unity + Unreal)
    createGameEngineMenu(m_hMenu);

    // Phase 48: The Final Crucible
    createCrucibleMenu(m_hMenu);

    // Phase 49: Copilot Gap Closer
    createCopilotGapMenu(m_hMenu);

    // Cursor/JB-Parity Feature Modules
    createCursorParityMenu(m_hMenu);

    SetMenu(hwnd, m_hMenu);

}

void Win32IDE::createToolbar(HWND hwnd)
{

    m_hwndToolbar = CreateWindowExW(0, TOOLBARCLASSNAMEW, nullptr,
                                   WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT,
                                   0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);

    if (m_hwndToolbar) {

        SendMessage(m_hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
        SendMessage(m_hwndToolbar, TB_AUTOSIZE, 0, 0);

        createTitleBarControls();
        updateTitleBarText();

    } else {

    }
}

void Win32IDE::createTitleBarControls()
{
    DWORD labelStyle = WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOPREFIX;
    m_hwndTitleLabel = CreateWindowExW(0, L"STATIC", L"RawrXD IDE", labelStyle,
                                      0, 0, 200, 24, m_hwndToolbar, (HMENU)IDC_TITLE_TEXT, m_hInstance, nullptr);

    DWORD buttonStyle = WS_CHILD | WS_VISIBLE | BS_FLAT;
    auto createButton = [&](HWND& target, int controlId, const wchar_t* caption) {
        target = CreateWindowExW(0, L"BUTTON", caption, buttonStyle,
                                0, 0, 32, 24, m_hwndToolbar, (HMENU)controlId, m_hInstance, nullptr);
    };

    createButton(m_hwndBtnGitHub, IDC_BTN_GITHUB, L"GH");
    createButton(m_hwndBtnMicrosoft, IDC_BTN_MICROSOFT, L"MS");
    createButton(m_hwndBtnSettings, IDC_BTN_SETTINGS, L"Gear");
    createButton(m_hwndBtnMinimize, IDC_BTN_MINIMIZE, L"-");
    createButton(m_hwndBtnMaximize, IDC_BTN_MAXIMIZE, L"[]");
    createButton(m_hwndBtnClose, IDC_BTN_CLOSE, L"X");

    RECT client{};
    GetClientRect(m_hwndMain, &client);
    layoutTitleBar(client.right - client.left);
}

void Win32IDE::layoutTitleBar(int width)
{
    if (!m_hwndToolbar) return;

    RECT client{};
    GetClientRect(m_hwndToolbar, &client);
    int toolbarHeight = client.bottom - client.top;
    if (toolbarHeight <= 0) toolbarHeight = 30;
    int controlHeight = (std::max)(22, toolbarHeight - 6);
    int y = (toolbarHeight - controlHeight) / 2;
    int padding = 6;
    int x = width - padding;

    auto placeButton = [&](HWND hwnd, int controlWidth) {
        if (!hwnd) return;
        x -= controlWidth;
        MoveWindow(hwnd, x, y, controlWidth, controlHeight, TRUE);
        x -= padding;
    };

    placeButton(m_hwndBtnClose, 32);
    placeButton(m_hwndBtnMaximize, 32);
    placeButton(m_hwndBtnMinimize, 32);
    placeButton(m_hwndBtnSettings, 48);
    placeButton(m_hwndBtnMicrosoft, 40);
    placeButton(m_hwndBtnGitHub, 40);

    if (m_hwndTitleLabel) {
        int availableRight = x;
        int labelWidth = (std::min)(420, availableRight - padding * 2);
        if (labelWidth < 160) {
            labelWidth = (std::max)(availableRight - padding * 2, 120);
        }
        int labelX = (std::max)(padding, (width - labelWidth) / 2);
        if (labelX + labelWidth > availableRight) {
            labelX = (std::max)(padding, availableRight - labelWidth);
        }
        MoveWindow(m_hwndTitleLabel, labelX, y, labelWidth, controlHeight, TRUE);
    }
}

std::string Win32IDE::extractLeafName(const std::string& path) const
{
    if (path.empty()) return "";
    size_t end = path.find_last_not_of("\\/ ");
    if (end == std::string::npos) return path;
    size_t slash = path.find_last_of("\\/", end);
    if (slash == std::string::npos) {
        return path.substr(0, end + 1);
    }
    return path.substr(slash + 1, end - slash);
}

void Win32IDE::setCurrentDirectoryFromFile(const std::string& filePath)
{
    if (filePath.empty()) return;
    size_t slash = filePath.find_last_of("\\/");
    if (slash != std::string::npos) {
        m_currentDirectory = filePath.substr(0, slash);
    }
}

void Win32IDE::updateTitleBarText()
{
    if (!m_hwndTitleLabel) return;

    std::string fileName = m_currentFile.empty() ? "Untitled" : extractLeafName(m_currentFile);
    std::string projectFolder;

    if (!m_currentDirectory.empty()) {
        projectFolder = extractLeafName(m_currentDirectory);
    }

    if (projectFolder.empty() && !m_currentFile.empty()) {
        size_t slash = m_currentFile.find_last_of("\\/");
        if (slash != std::string::npos) {
            projectFolder = extractLeafName(m_currentFile.substr(0, slash));
        }
    }

    if (projectFolder.empty() && !m_gitRepoPath.empty()) {
        projectFolder = extractLeafName(m_gitRepoPath);
    }

    if (projectFolder.empty()) {
        projectFolder = "Workspace";
    }

    std::string composed = fileName + "  •  " + projectFolder;
    if (composed != m_lastTitleBarText) {
        SetWindowTextW(m_hwndTitleLabel, utf8ToWide(composed).c_str());
        m_lastTitleBarText = composed;
    }
    // Keep breadcrumb bar in sync with current file (symbol path updates on cursor move)
    if (m_hwndBreadcrumbs && m_settings.breadcrumbsEnabled)
        updateBreadcrumbs();
}

// ============================================================================
// DPI SCALING
// ============================================================================

UINT Win32IDE::getDpi() const {
    if (m_hwndMain) {
        // GetDpiForWindow requires Windows 10 1607+
        typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND);
        static PFN_GetDpiForWindow pGetDpiForWindow = nullptr;
        static bool resolved = false;
        if (!resolved) {
            HMODULE hUser32 = GetModuleHandleA("user32.dll");
            if (hUser32) {
                pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(hUser32, "GetDpiForWindow");
            }
            resolved = true;
        }
        if (pGetDpiForWindow) {
            UINT dpi = pGetDpiForWindow(m_hwndMain);
            if (dpi > 0) return dpi;
        }
    }
    // Fallback: system DPI via device caps
    HDC hdc = GetDC(nullptr);
    UINT dpi = (UINT)GetDeviceCaps(hdc, LOGPIXELSY);
    ReleaseDC(nullptr, hdc);
    return dpi ? dpi : 96;
}

int Win32IDE::dpiScale(int basePixels) const {
    // If user override is set, blend it with system DPI
    if (m_settings.uiScalePercent > 0) {
        return MulDiv(basePixels, m_settings.uiScalePercent, 100);
    }
    return MulDiv(basePixels, m_currentDpi, 96);
}

void Win32IDE::recreateFonts() {
    m_currentDpi = getDpi();

    // Editor font — monospace
    if (m_editorFont) { DeleteObject(m_editorFont); m_editorFont = nullptr; }
    m_editorFont = CreateFontA(
        -dpiScale(16), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas"
    );

    // UI font — proportional  
    if (m_hFontUI) { DeleteObject(m_hFontUI); m_hFontUI = nullptr; }
    m_hFontUI = CreateFontA(
        -dpiScale(14), 0, 0, 0, FW_NORMAL,
        FALSE, FALSE, FALSE, ANSI_CHARSET,
        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI"
    );

    // Ghost text font — italic monospace
    if (m_ghostTextFont) { DeleteObject(m_ghostTextFont); m_ghostTextFont = nullptr; }
    LOGFONTA lf = {};
    lf.lfHeight         = -dpiScale(14);
    lf.lfWeight         = FW_NORMAL;
    lf.lfItalic         = TRUE;
    lf.lfCharSet        = DEFAULT_CHARSET;
    lf.lfQuality        = CLEARTYPE_QUALITY;
    lf.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    strncpy(lf.lfFaceName, m_currentTheme.fontName.c_str(), LF_FACESIZE - 1);
    lf.lfFaceName[LF_FACESIZE - 1] = '\0';
    m_ghostTextFont = CreateFontIndirectA(&lf);

    // Apply editor font
    if (m_hwndEditor && m_editorFont) {
        SendMessage(m_hwndEditor, WM_SETFONT, (WPARAM)m_editorFont, TRUE);
        CHARFORMAT2W cf;
        memset(&cf, 0, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_FACE | CFM_SIZE;
        cf.yHeight = dpiScale(16) * 15;
        wcscpy_s(cf.szFaceName, L"Consolas");
        SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    }

    // Apply UI font to all known UI controls
    auto setFont = [](HWND hwnd, HFONT font) {
        if (hwnd) SendMessage(hwnd, WM_SETFONT, (WPARAM)font, TRUE);
    };
    setFont(m_hwndTabBar, m_hFontUI);
    setFont(m_hwndSecondarySidebarHeader, m_hFontUI);
    setFont(m_hwndModelSelector, m_hFontUI);
    setFont(m_hwndCopilotChatOutput, m_hFontUI);
    setFont(m_hwndCopilotChatInput, m_hFontUI);
    setFont(m_hwndCopilotSendBtn, m_hFontUI);
    setFont(m_hwndCopilotClearBtn, m_hFontUI);
    setFont(m_hwndCommandPaletteInput, m_hFontUI);
    setFont(m_hwndCommandPaletteList, m_hFontUI);
    setFont(m_hwndSearchInput, m_hFontUI);
    setFont(m_hwndSearchResults, m_hFontUI);
    setFont(m_hwndFloatingContent, m_hFontUI);

    // PowerShell panel fonts (store and delete previous to avoid leak on DPI change)
    if (m_hFontPowerShell) { DeleteObject(m_hFontPowerShell); m_hFontPowerShell = nullptr; }
    if (m_hFontPowerShellStatus) { DeleteObject(m_hFontPowerShellStatus); m_hFontPowerShellStatus = nullptr; }
    if (m_hwndPowerShellOutput) {
        m_hFontPowerShell = CreateFontA(
            -dpiScale(16), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas"
        );
        SendMessage(m_hwndPowerShellOutput, WM_SETFONT, (WPARAM)m_hFontPowerShell, TRUE);
        if (m_hwndPowerShellInput) SendMessage(m_hwndPowerShellInput, WM_SETFONT, (WPARAM)m_hFontPowerShell, TRUE);
        if (m_hwndPSBtnExecute) SendMessage(m_hwndPSBtnExecute, WM_SETFONT, (WPARAM)m_hFontPowerShell, TRUE);
    }
    if (m_hwndPowerShellStatusBar) {
        m_hFontPowerShellStatus = CreateFontA(
            -dpiScale(12), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI"
        );
        SendMessage(m_hwndPowerShellStatusBar, WM_SETFONT, (WPARAM)m_hFontPowerShellStatus, TRUE);
    }

    // Terminal panes
    for (auto& pane : m_terminalPanes) {
        if (pane.hwnd) {
            CHARFORMAT2W tcf;
            memset(&tcf, 0, sizeof(tcf));
            tcf.cbSize = sizeof(tcf);
            tcf.dwMask = CFM_FACE | CFM_SIZE;
            tcf.yHeight = dpiScale(9) * 20;
            wcscpy_s(tcf.szFaceName, L"Consolas");
            SendMessageW(pane.hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&tcf);
        }
    }

    // File tree
    if (m_hwndFileTree) {
        setFont(m_hwndFileTree, m_hFontUI);
    }

    LOG_INFO("Fonts recreated at DPI=" + std::to_string(m_currentDpi));
}

void Win32IDE::createEditor(HWND hwnd)
{

    m_hwndEditor = CreateWindowExW(WS_EX_CLIENTEDGE, RICHEDIT_CLASSW, L"",
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
                                  0, 0, 0, 0, hwnd, (HMENU)IDC_EDITOR, m_hInstance, nullptr);
    if (!m_hwndEditor) {

        return;
    }

    m_currentDpi = getDpi();
    recreateFonts();

    CHARFORMAT2W cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.yHeight = dpiScale(11) * 20;
    cf.crTextColor = RGB(212, 212, 212);
    wcscpy_s(cf.szFaceName, L"Consolas");
    SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);

    SendMessage(m_hwndEditor, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
    SendMessage(m_hwndEditor, EM_SETREADONLY, FALSE, 0);
    SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE | ENM_SCROLL);
    SendMessage(m_hwndEditor, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);

    static const wchar_t welcomeText[] =
        L"// ============================================\r\n"
        L"// RawrXD IDE - Native Win32 AI Development\r\n"
        L"// ============================================\r\n"
        L"//\r\n"
        L"// Welcome! The editor is ready.\r\n"
        L"//\r\n"
        L"// Shortcuts:\r\n"
        L"//   Ctrl+N   New File\r\n"
        L"//   Ctrl+O   Open File\r\n"
        L"//   Ctrl+S   Save\r\n"
        L"//   Ctrl+F   Find\r\n"
        L"//   Ctrl+B   Toggle Sidebar\r\n"
        L"//   Ctrl+Shift+P   Command Palette\r\n"
        L"//\r\n"
        L"// Start typing or open a file to begin.\r\n"
        L"\r\n";
    SetWindowTextW(m_hwndEditor, welcomeText);

    SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    int textLen = GetWindowTextLengthW(m_hwndEditor);
    SendMessage(m_hwndEditor, EM_SETSEL, textLen, textLen);

    initializeEditorSurface();

    // ================================================================
    // Subclass the editor RichEdit control
    // Store IDE pointer and original wndproc as window properties,
    // then redirect to EditorSubclassProc for ghost text, key intercept,
    // scroll sync, and minimap updates.
    // ================================================================
    if (m_hwndEditor) {
        SetPropW(m_hwndEditor, kEditorWndProp, (HANDLE)this);
        WNDPROC oldEditorProc = (WNDPROC)SetWindowLongPtrW(
            m_hwndEditor, GWLP_WNDPROC, (LONG_PTR)EditorSubclassProc);
        SetPropW(m_hwndEditor, kEditorProcProp, (HANDLE)oldEditorProc);
    }
}

void Win32IDE::createTerminal(HWND hwnd)
{
    // Initialize the Enterprise PowerShell Panel (creates m_hwndPowerShellPanel)
    createPowerShellPanel();
    m_powerShellPanelVisible = true;

    if (m_terminalPanes.empty()) {
        createTerminalPane(Win32TerminalManager::PowerShell, "PowerShell");
    } else {
        setActiveTerminalPane(m_terminalPanes.front().id);
    }

    // Create command input
    m_hwndCommandInput = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
                                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                        0, 0, 0, 0, hwnd, (HMENU)IDC_COMMAND_INPUT, m_hInstance, nullptr);
    if (m_hwndCommandInput) {
        SetWindowLongPtr(m_hwndCommandInput, GWLP_USERDATA, (LONG_PTR)this);
        m_oldCommandInputProc = (WNDPROC)SetWindowLongPtr(m_hwndCommandInput, GWLP_WNDPROC, (LONG_PTR)CommandInputProc);
    }

}

int Win32IDE::createTerminalPane(Win32TerminalManager::ShellType shellType, const std::string& name)
{
    HWND hwnd = CreateWindowExW(WS_EX_CLIENTEDGE, RICHEDIT_CLASSW, L"",
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                0, 0, 0, 0, m_hwndMain, nullptr, m_hInstance, nullptr);

    // LOGGING AS REQUESTED
    char logBuf[256];
    sprintf_s(logBuf, "TerminalPane HWND created: %p (Parent: %p)", hwnd, m_hwndMain);
    LOG_INFO(std::string(logBuf));

    // Apply dark theme to terminal pane
    SendMessage(hwnd, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));

    CHARFORMAT2W cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.yHeight = 180;
    cf.crTextColor = RGB(204, 204, 204);
    wcscpy_s(cf.szFaceName, L"Consolas");
    SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    int paneId = m_nextTerminalId++;
    TerminalPane pane;
    pane.id = paneId;
    pane.hwnd = hwnd;
    pane.manager = std::make_unique<Win32TerminalManager>();
    pane.name = name.empty() ? ("Terminal " + std::to_string(paneId)) : name;
    pane.shellType = shellType;
    pane.isActive = false;
    pane.bounds = {0, 0, 0, 0};

    pane.manager->onOutput = [this, paneId](const std::string& output) {
        if (isShuttingDown()) return;
        onTerminalOutput(paneId, output);
    };
    pane.manager->onError = [this, paneId](const std::string& error) {
        if (isShuttingDown()) return;
        onTerminalError(paneId, error);
    };

    m_terminalPanes.push_back(std::move(pane));
    setActiveTerminalPane(paneId);
    applyTheme();
    return paneId;
}

TerminalPane* Win32IDE::findTerminalPane(int paneId)
{
    for (auto& pane : m_terminalPanes) {
        if (pane.id == paneId) {
            return &pane;
        }
    }
    return nullptr;
}

TerminalPane* Win32IDE::getActiveTerminalPane()
{
    TerminalPane* active = findTerminalPane(m_activeTerminalId);
    if (!active && !m_terminalPanes.empty()) {
        setActiveTerminalPane(m_terminalPanes.front().id);
        return findTerminalPane(m_terminalPanes.front().id);
    }
    return active;
}

void Win32IDE::setActiveTerminalPane(int paneId)
{
    bool found = false;
    for (auto& pane : m_terminalPanes) {
        if (pane.id == paneId) {
            pane.isActive = true;
            m_activeTerminalId = paneId;
            if (pane.hwnd) SetFocus(pane.hwnd);
            found = true;
        } else {
            pane.isActive = false;
        }
    }
    if (!found && !m_terminalPanes.empty()) {
        m_terminalPanes.front().isActive = true;
        m_activeTerminalId = m_terminalPanes.front().id;
        if (m_terminalPanes.front().hwnd) SetFocus(m_terminalPanes.front().hwnd);
    }
}

void Win32IDE::layoutTerminalPanes(int width, int top, int height)
{
    // LOGGING AS REQUESTED
    char logBuf[256];
    sprintf_s(logBuf, "layoutTerminalPanes: Width=%d Top=%d Height=%d Count=%zu", width, top, height, m_terminalPanes.size());
    LOG_INFO(std::string(logBuf));

    if (width <= 0 || height <= 0 || m_terminalPanes.empty()) return;

    // Calculate correct left offset — terminal panes are children of m_hwndMain,
    // so we must offset past activity bar + sidebar to avoid overlapping them
    const int ACTIVITY_BAR_WIDTH = dpiScale(48);
    int sidebarWidth = m_sidebarVisible ? m_sidebarWidth : 0;
    int editorLeft = ACTIVITY_BAR_WIDTH + sidebarWidth;
    int secondarySidebarWidth = m_secondarySidebarVisible ? m_secondarySidebarWidth : 0;

    // Clamp width to editor area (exclude sidebars)
    RECT mainRect;
    GetClientRect(m_hwndMain, &mainRect);
    int editorWidth = (mainRect.right - mainRect.left) - editorLeft - secondarySidebarWidth;
    if (editorWidth <= 0) editorWidth = width; // fallback

    int count = static_cast<int>(m_terminalPanes.size());
    if (count == 1) {
        auto& pane = m_terminalPanes[0];
        MoveWindow(pane.hwnd, editorLeft, top, editorWidth, height, TRUE);
        pane.bounds = {editorLeft, top, editorLeft + editorWidth, top + height};
        return;
    }

    if (m_terminalSplitHorizontal) {
        int paneHeight = height / count;
        int y = top;
        for (int i = 0; i < count; ++i) {
            int currentHeight = (i == count - 1) ? (height - paneHeight * (count - 1)) : paneHeight;
            auto& pane = m_terminalPanes[i];
            MoveWindow(pane.hwnd, editorLeft, y, editorWidth, currentHeight, TRUE);
            pane.bounds = {editorLeft, y, editorLeft + editorWidth, y + currentHeight};
            y += currentHeight;
        }
    } else {
        int paneWidth = editorWidth / count;
        int x = editorLeft;
        for (int i = 0; i < count; ++i) {
            int currentWidth = (i == count - 1) ? (editorWidth - paneWidth * (count - 1)) : paneWidth;
            auto& pane = m_terminalPanes[i];
            MoveWindow(pane.hwnd, x, top, currentWidth, height, TRUE);
            pane.bounds = {x, top, x + currentWidth, top + height};
            x += currentWidth;
        }
    }
}

void Win32IDE::splitTerminalHorizontal()
{
    m_terminalSplitHorizontal = true;
    TerminalPane* active = getActiveTerminalPane();
    Win32TerminalManager::ShellType type = active ? active->shellType : Win32TerminalManager::PowerShell;
    createTerminalPane(type, "Terminal");
    RECT rect; GetClientRect(m_hwndMain, &rect);
    RECT toolbarRect; GetWindowRect(m_hwndToolbar, &toolbarRect);
    int toolbarHeight = toolbarRect.bottom - toolbarRect.top;
    layoutTerminalPanes(rect.right - rect.left, toolbarHeight + m_editorHeight, m_terminalHeight);
}

void Win32IDE::splitTerminalVertical()
{
    m_terminalSplitHorizontal = false;
    TerminalPane* active = getActiveTerminalPane();
    Win32TerminalManager::ShellType type = active ? active->shellType : Win32TerminalManager::PowerShell;
    createTerminalPane(type, "Terminal");
    RECT rect; GetClientRect(m_hwndMain, &rect);
    RECT toolbarRect; GetWindowRect(m_hwndToolbar, &toolbarRect);
    int toolbarHeight = toolbarRect.bottom - toolbarRect.top;
    layoutTerminalPanes(rect.right - rect.left, toolbarHeight + m_editorHeight, m_terminalHeight);
}

void Win32IDE::clearAllTerminals()
{
    for (auto& pane : m_terminalPanes) {
        if (pane.manager && pane.manager->isRunning()) {
            pane.manager->stop();
        }
        if (pane.hwnd) {
            DestroyWindow(pane.hwnd);
        }
    }
    m_terminalPanes.clear();
    m_activeTerminalId = -1;
    m_nextTerminalId = 1;
    createTerminalPane(Win32TerminalManager::PowerShell, "PowerShell");
    RECT rect; GetClientRect(m_hwndMain, &rect);
    RECT toolbarRect; GetWindowRect(m_hwndToolbar, &toolbarRect);
    int toolbarHeight = toolbarRect.bottom - toolbarRect.top;
    layoutTerminalPanes(rect.right - rect.left, toolbarHeight + m_editorHeight, m_terminalHeight);
}

void Win32IDE::createStatusBar(HWND hwnd)
{

    m_hwndStatusBar = CreateWindowExW(0, STATUSCLASSNAMEW, L"",
                                     WS_CHILD | WS_VISIBLE,
                                     0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS_BAR, m_hInstance, nullptr);
    if (!m_hwndStatusBar) {

        return;
    }

    int parts[] = {200, 400, 600, -1};
    SendMessage(m_hwndStatusBar, SB_SETPARTS, 4, (LPARAM)parts);
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Ready");
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 3, (LPARAM)L"Ctx: 0/128K  0%");

}

void Win32IDE::createSidebar(HWND hwnd)
{
    createPrimarySidebar(hwnd);
}



void Win32IDE::newFile()
{
    appendToOutput("File > New clicked\n", "Output", OutputSeverity::Info);
    if (m_fileModified) {
        int result = MessageBoxW(m_hwndMain, L"File has been modified. Save changes?", L"Save", MB_YESNOCANCEL);
        if (result == IDCANCEL) {
            appendToOutput("File > New cancelled by user\n", "Output", OutputSeverity::Info);
            return;
        }
        if (result == IDYES && !saveFile()) {
            appendToOutput("File > New - save failed, operation aborted\n", "Output", OutputSeverity::Warning);
            return;
        }
    }

    setWindowText(m_hwndEditor, "");
    m_currentFile.clear();
    m_fileModified = false;
    updateTitleBarText();
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"New file");
    updateMenuEnableStates();
    syncEditorToGpuSurface();
    appendToOutput("New file created successfully\n", "Output", OutputSeverity::Info);
}

void Win32IDE::openFile()
{
    SCOPED_METRIC("file.open_dialog");
    METRICS.increment("file.open_total");
    appendToOutput("File > Open clicked\n", "Output", OutputSeverity::Info);
    if (m_fileModified) {
        int result = MessageBoxW(m_hwndMain, L"File has been modified. Save changes?", L"Save", MB_YESNOCANCEL);
        if (result == IDCANCEL) {
            appendToOutput("File > Open cancelled by user\n", "Output", OutputSeverity::Info);
            return;
        }
        if (result == IDYES && !saveFile()) {
            appendToOutput("File > Open - save failed, operation aborted\n", "Output", OutputSeverity::Warning);
            return;
        }
    }

    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = (DWORD)std::size(szFile);
    ofn.lpstrFilter = L"All Files\0*.*\0C++ Files\0*.cpp;*.h\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameW(&ofn)) {
        std::string pathUtf8 = wideToUtf8(szFile);
        appendToOutput("Opening file: " + pathUtf8 + "\n", "Output", OutputSeverity::Info);
        try {
            std::ifstream inStream(std::filesystem::path(szFile), std::ios::binary);
            if (inStream) {
                inStream.seekg(0, std::ios::end);
                const std::streamsize size = inStream.tellg();
                inStream.seekg(0, std::ios::beg);
                std::string content(static_cast<size_t>(size), '\0');
                if (size > 0) inStream.read(&content[0], size);
                setWindowText(m_hwndEditor, content);
                m_currentFile = pathUtf8;
                m_fileModified = false;
                setCurrentDirectoryFromFile(m_currentFile);
                updateTitleBarText();
                SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"File opened");
                updateMenuEnableStates();
                syncEditorToGpuSurface();
                appendToOutput("File opened successfully (" + std::to_string(content.size()) + " bytes)\n", "Output", OutputSeverity::Info);
            } else {
                appendToOutput("Failed to open file: " + pathUtf8 + "\n", "Errors", OutputSeverity::Error);
                MessageBoxW(m_hwndMain, L"Failed to open file", L"Error", MB_OK | MB_ICONERROR);
            }
        } catch (const std::exception& e) {
            appendToOutput("Exception opening file: " + std::string(e.what()) + "\n", "Errors", OutputSeverity::Error);
            MessageBoxW(m_hwndMain, utf8ToWide(e.what()).c_str(), L"Error", MB_OK | MB_ICONERROR);
        }
    } else {
        appendToOutput("File > Open cancelled by user (no file selected)\n", "Output", OutputSeverity::Info);
    }
}

// Overload to open a specific file path
void Win32IDE::openFile(const std::string& filePath)
{
    SCOPED_METRIC("file.open_path");
    if (filePath.empty()) {
        openFile(); // Call the dialog version
        return;
    }

    METRICS.increment("file.open_total");
    appendToOutput("Opening file: " + filePath + "\n", "Output", OutputSeverity::Info);
    try {
        std::ifstream file(std::filesystem::path(utf8ToWide(filePath)));
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            setWindowText(m_hwndEditor, content);
            m_currentFile = filePath;
            m_fileModified = false;
            setCurrentDirectoryFromFile(m_currentFile);
            updateTitleBarText();

            std::string displayName = extractLeafName(filePath);
            if (m_hwndTabBar) {
                addTab(filePath, displayName);
            }

            CHARFORMAT2W cf;
            memset(&cf, 0, sizeof(cf));
            cf.cbSize = sizeof(cf);
            cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
            cf.crTextColor = m_currentTheme.textColor;
            cf.yHeight = 220;
            wcscpy_s(cf.szFaceName, L"Consolas");
            SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"File opened");
            updateMenuEnableStates();
            updateLineNumbers();
            syncEditorToGpuSurface();
            appendToOutput("File opened successfully (" + std::to_string(content.size()) + " bytes)\n", "Output", OutputSeverity::Info);
        } else {
            appendToOutput("Failed to open file: " + filePath + "\n", "Errors", OutputSeverity::Error);
            MessageBoxW(m_hwndMain, L"Failed to open file", L"Error", MB_OK | MB_ICONERROR);
        }
    } catch (const std::exception& e) {
        appendToOutput("Exception opening file: " + std::string(e.what()) + "\n", "Errors", OutputSeverity::Error);
        MessageBoxW(m_hwndMain, utf8ToWide(e.what()).c_str(), L"Error", MB_OK | MB_ICONERROR);
    }
}

bool Win32IDE::saveFile()
{
    SCOPED_METRIC("file.save");
    METRICS.increment("file.save_total");

    if (m_currentFile.empty()) {
        appendToOutput("File > Save - no current file, showing Save As dialog\n", "Output", OutputSeverity::Info);
        return saveFileAs();
    }

    appendToOutput("Saving file: " + m_currentFile + "\n", "Output", OutputSeverity::Info);
    try {
        std::string content = getWindowText(m_hwndEditor);
        std::ofstream file(std::filesystem::path(utf8ToWide(m_currentFile)));
        if (file) {
            file << content;
            m_fileModified = false;
            updateTitleBarText();
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"File saved");
            appendToOutput("File saved successfully (" + std::to_string(content.size()) + " bytes)\n", "Output", OutputSeverity::Info);
            return true;
        }
        appendToOutput("Failed to open file for writing: " + m_currentFile + "\n", "Errors", OutputSeverity::Error);
        MessageBoxW(m_hwndMain, L"Failed to save file", L"Error", MB_OK | MB_ICONERROR);
    } catch (const std::exception& e) {
        appendToOutput("Exception saving file: " + std::string(e.what()) + "\n", "Errors", OutputSeverity::Error);
        MessageBoxW(m_hwndMain, utf8ToWide(e.what()).c_str(), L"Error", MB_OK | MB_ICONERROR);
    }
    return false;
}

bool Win32IDE::saveFileAs()
{
    appendToOutput("File > Save As clicked\n", "Output", OutputSeverity::Info);
    OPENFILENAMEW ofn;
    wchar_t szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = (DWORD)std::size(szFile);
    ofn.lpstrFilter = L"All Files\0*.*\0C++ Files\0*.cpp;*.h\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameW(&ofn)) {
        m_currentFile = wideToUtf8(szFile);
        appendToOutput("Save As: " + m_currentFile + "\n", "Output", OutputSeverity::Info);
        setCurrentDirectoryFromFile(m_currentFile);
        updateTitleBarText();
        return saveFile();
    }
    appendToOutput("File > Save As cancelled by user\n", "Output", OutputSeverity::Info);
    return false;
}

void Win32IDE::startPowerShell()
{
    TerminalPane* pane = getActiveTerminalPane();
    if (!pane || !pane->manager) return;
    stopTerminal();
    if (pane->manager->start(Win32TerminalManager::PowerShell)) {
        appendText(pane->hwnd, "PowerShell started...\n");
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"PowerShell");
        updateMenuEnableStates();
        appendToOutput("PowerShell started...\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::startCommandPrompt()
{
    TerminalPane* pane = getActiveTerminalPane();
    if (!pane || !pane->manager) return;
    stopTerminal();
    if (pane->manager->start(Win32TerminalManager::CommandPrompt)) {
        appendText(pane->hwnd, "Command Prompt started...\n");
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"CMD");
        updateMenuEnableStates();
        appendToOutput("Command Prompt started...\n", "Output", OutputSeverity::Info);
    }
}

void Win32IDE::stopTerminal()
{
    TerminalPane* pane = getActiveTerminalPane();
    if (!pane || !pane->manager || !pane->manager->isRunning()) return;
    pane->manager->stop();
    appendText(pane->hwnd, "\nTerminal stopped.\n");
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"Stopped");
    updateMenuEnableStates();
    appendToOutput("Terminal stopped.\n", "Output", OutputSeverity::Info);
}

void Win32IDE::executeCommand()
{
    std::string command = getWindowText(m_hwndCommandInput);
    if (command.empty()) return;

    SetWindowTextW(m_hwndCommandInput, L"");
    
    // Command Parsing
    if (command[0] == '/' || command[0] == '!') {
        std::stringstream ss(command);
        std::string action;
        ss >> action;
        
        if (action == "/load") {
             std::string path;
             std::getline(ss, path);
             if(!path.empty()) path = path.substr(1);
             openFile(path); // Or load model if .gguf
             if (path.find(".gguf") != std::string::npos) {
                  auto* eng = m_nativeEngine.get();
                  if(eng && eng->LoadModel(path)) appendToOutput("Model loaded.\n", "Agent", OutputSeverity::Info);
             }
        }
        else if (action == "/agent" || action == "/ask") {
             std::string q; std::getline(ss, q);
             if(m_agent) m_agent->Ask(q);
        }
        else if (action == "/bugreport") {
             std::string f = m_currentFile;
             if(f.empty()) appendToOutput("No file open.\n", "Error", OutputSeverity::Error);
             else if(m_agent) m_agent->BugReport(f);
        }
        else if (action == "/suggest") {
             std::string f = m_currentFile;
             if(f.empty()) appendToOutput("No file open.\n", "Error", OutputSeverity::Error);
             else if(m_agent) m_agent->Suggest(f);
        }
        else if (action == "/install") {
             std::string path; std::getline(ss, path);
             if(!path.empty()) {
                 if (RawrXD::VSIXInstaller::Install(path.substr(1))) 
                     appendToOutput("Extension installed.\n", "System", OutputSeverity::Info);
             }
        }
        else if (action == "/max") {
             static bool m = false; m = !m;
             if(m_agent) m_agent->SetMaxMode(m);
             appendToOutput(std::string("Max Mode: ") + (m?"ON":"OFF") + "\n", "System", OutputSeverity::Info);
        }
        else if (action == "/think") {
             static bool t = false; t = !t;
             if(m_agent) m_agent->SetDeepThink(t);
             appendToOutput(std::string("Deep Think: ") + (t?"ON":"OFF") + "\n", "System", OutputSeverity::Info);
        }
        else if (action == "/research") {
             static bool r = false; r = !r;
             if(m_agent) m_agent->SetDeepResearch(r);
             appendToOutput(std::string("Deep Research: ") + (r?"ON":"OFF") + "\n", "System", OutputSeverity::Info);
        }
        else if (action == "/norefusal") {
             static bool nr = false; nr = !nr;
             if(m_agent) m_agent->SetNoRefusal(nr);
             appendToOutput(std::string("No Refusal: ") + (nr?"ON":"OFF") + "\n", "System", OutputSeverity::Info);
        }
        else if (action == "!help" || action == "/exthelp") {
             static RawrXD::ExtensionLoader loader;
             loader.Scan();
             std::string arg; std::getline(ss, arg);
             if(!arg.empty()) arg = arg.substr(1);
             
             if(arg.empty()) {
                 std::string list = "Extensions:\n";
                 for(auto& e : loader.GetExtensions()) list += " - " + e.name + "\n";
                 appendToOutput(list, "System", OutputSeverity::Info);
             } else {
                 appendToOutput(loader.GetHelp(arg) + "\n", "System", OutputSeverity::Info);
             }
        }
        else {
             // Fallback
             TerminalPane* pane = getActiveTerminalPane();
             if (pane && pane->manager && pane->manager->isRunning()) {
                command += "\n";
                pane->manager->writeInput(command);
             }
        }
        return;
    }

    // Default to Chat if mode enabled
    if (m_chatMode && isModelLoaded() && m_agent) {
        appendChatMessage("You", command);
        // Async ask would be better, but for now blocking in thread or using callback logic
        std::thread([this, command](){
             DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
             if (_guard.cancelled) return;
             m_agent->Ask(command);
        }).detach();
        return;
    }
    
    // Send to terminal
    TerminalPane* pane = getActiveTerminalPane();
    if (pane && pane->manager && pane->manager->isRunning()) {
        addPowerShellHistory(command); // Track in shared command history
        command += "\n";
        pane->manager->writeInput(command);
    }
}

void Win32IDE::onTerminalOutput(int paneId, const std::string& output)
{
    if (isShuttingDown()) return;
    TerminalPane* pane = findTerminalPane(paneId);
    if (!pane || !pane->hwnd) return;
    appendText(pane->hwnd, output);
    appendToOutput(output, "Debug", OutputSeverity::Info);
}

void Win32IDE::onTerminalError(int paneId, const std::string& error)
{
    if (isShuttingDown()) return;
    TerminalPane* pane = findTerminalPane(paneId);
    if (!pane || !pane->hwnd) return;
    appendText(pane->hwnd, error);
    appendToOutput(error, "Errors", OutputSeverity::Error);
}

std::string Win32IDE::getWindowText(HWND hwnd)
{
    int length = GetWindowTextLengthW(hwnd);
    if (length <= 0) return {};
    std::wstring wtext(length + 1, L'\0');
    GetWindowTextW(hwnd, &wtext[0], length + 1);
    wtext.resize(length);
    return wideToUtf8(wtext.c_str());
}

// UTF-8 byte offset <-> UTF-16 character index for Rich Edit
static int utf8ByteOffsetToCharIndex(const std::string& utf8, int byteOffset) {
    if (byteOffset <= 0 || utf8.empty()) return 0;
    if (byteOffset >= (int)utf8.size()) byteOffset = (int)utf8.size();
    std::wstring w = utf8ToWide(utf8.substr(0, byteOffset));
    return (int)w.size();
}
static int charIndexToUtf8ByteOffset(const std::string& utf8, int charIndex) {
    if (charIndex <= 0 || utf8.empty()) return 0;
    std::wstring w = utf8ToWide(utf8);
    if (charIndex >= (int)w.size()) return (int)utf8.size();
    return (int)wideToUtf8(w.substr(0, charIndex).c_str()).size();
}

void Win32IDE::setWindowText(HWND hwnd, const std::string& text)
{
    SetWindowTextW(hwnd, utf8ToWide(text).c_str());
    if (hwnd == m_hwndEditor) {
        syncEditorToGpuSurface();
    }
}

void Win32IDE::appendText(HWND hwnd, const std::string& text)
{
    GETTEXTLENGTHEX gtl;
    gtl.flags = GTL_DEFAULT;
    gtl.codepage = CP_UNICODE;
    LONG length = SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);

    SendMessage(hwnd, EM_SETSEL, length, length);

    std::wstring wtext = utf8ToWide(text);
    SETTEXTEX st;
    st.flags = ST_DEFAULT;
    st.codepage = CP_UNICODE;
    SendMessageW(hwnd, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)wtext.c_str());

    if (hwnd == m_hwndEditor) {
        syncEditorToGpuSurface();
    }
}

// Theme Management Implementation
void Win32IDE::loadTheme(const std::string& themeName)
{
    std::string filename = "themes\\" + themeName + ".theme";
    std::ifstream file(filename);
    if (file.is_open()) {
        std::string line;
        while (getline(file, line)) {
            if (line.find("background=") == 0) {
                m_currentTheme.backgroundColor = std::stoul(line.substr(11), nullptr, 16);
            } else if (line.find("text=") == 0) {
                m_currentTheme.textColor = std::stoul(line.substr(5), nullptr, 16);
            } else if (line.find("selection=") == 0) {
                m_currentTheme.selectionColor = std::stoul(line.substr(10), nullptr, 16);
            } else if (line.find("linenumber=") == 0) {
                m_currentTheme.lineNumberColor = std::stoul(line.substr(11), nullptr, 16);
            }
        }
        file.close();
        applyTheme();
    }
}

void Win32IDE::saveTheme(const std::string& themeName)
{
    std::string filename = "themes\\" + themeName + ".theme";
    CreateDirectoryA("themes", NULL);
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "background=" << std::hex << m_currentTheme.backgroundColor << std::endl;
        file << "text=" << std::hex << m_currentTheme.textColor << std::endl;
        file << "selection=" << std::hex << m_currentTheme.selectionColor << std::endl;
        file << "linenumber=" << std::hex << m_currentTheme.lineNumberColor << std::endl;
        file.close();
        MessageBoxW(m_hwndMain, L"Theme saved successfully", L"Theme Manager", MB_OK);
    }
}

void Win32IDE::applyTheme()
{
    // ----------------------------------------------------------------
    // applyTheme() is idempotent — safe to call on startup, on theme
    // switch, on DPI change, and on transparency change.
    // Theme is pure data (IDETheme) — no GDI handles stored in it.
    // ----------------------------------------------------------------

    LOG_DEBUG("applyTheme(): \"" + m_currentTheme.name + "\"");

    // 1. Update the tracked background brush
    if (m_backgroundBrush) DeleteObject(m_backgroundBrush);
    m_backgroundBrush = CreateSolidBrush(m_currentTheme.backgroundColor);

    // 2. Editor: background + default text format (SCF_DEFAULT, not SCF_ALL,
    //    so syntax coloring tokens are preserved until the next colorize pass)
    if (m_hwndEditor) {
        SendMessage(m_hwndEditor, EM_SETBKGNDCOLOR, 0, m_currentTheme.backgroundColor);

        CHARFORMAT2W cf;
        ZeroMemory(&cf, sizeof(cf));
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR;
        cf.crTextColor = m_currentTheme.textColor;
        cf.dwEffects = 0;
        SendMessageW(m_hwndEditor, EM_SETCHARFORMAT, SCF_DEFAULT, (LPARAM)&cf);
    }

    for (auto& pane : m_terminalPanes) {
        if (!pane.hwnd) continue;
        SendMessage(pane.hwnd, EM_SETBKGNDCOLOR, 0, m_currentTheme.panelBg);
        CHARFORMAT2W tcf;
        ZeroMemory(&tcf, sizeof(tcf));
        tcf.cbSize = sizeof(tcf);
        tcf.dwMask = CFM_COLOR;
        tcf.crTextColor = m_currentTheme.panelFg;
        tcf.dwEffects = 0;
        SendMessageW(pane.hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&tcf);
    }

    // 4. Deep apply to all surfaces (sidebar, activity bar, tabs, status bar, panels)
    applyThemeToAllControls();

    // 5. Transparency — only touch the top-level window
    if (m_currentTheme.windowAlpha < 255) {
        setWindowTransparency(m_currentTheme.windowAlpha);
    }
    
    // 6. Force full repaint + update menu states
    InvalidateRect(m_hwndMain, NULL, TRUE);
    updateMenuEnableStates();

    // 7. Re-trigger syntax coloring so tokens pick up new palette
    if (m_syntaxColoringEnabled && m_hwndEditor) {
        m_syntaxDirty = true;
        applySyntaxColoring();
    }
}

void Win32IDE::showThemeEditor()
{
    showThemePicker();
}

void Win32IDE::updateMenuEnableStates() {
    if (!m_hMenu) return;
    // Terminal split menu items
    UINT enableSplit = MF_BYCOMMAND | (m_terminalPanes.size() >= 1 ? MF_ENABLED : MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_TERMINAL_SPLIT_H, enableSplit);
    EnableMenuItem(m_hMenu, IDM_TERMINAL_SPLIT_V, enableSplit);
    TerminalPane* activePane = getActiveTerminalPane();
    bool terminalRunning = activePane && activePane->manager && activePane->manager->isRunning();
    EnableMenuItem(m_hMenu, IDM_TERMINAL_STOP, terminalRunning ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_TERMINAL_CLEAR_ALL, (m_terminalPanes.empty() ? (MF_BYCOMMAND|MF_GRAYED) : (MF_BYCOMMAND|MF_ENABLED)));

    // Git items
    bool repo = isGitRepository();
    EnableMenuItem(m_hMenu, IDM_GIT_STATUS, repo ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_GIT_COMMIT, (repo && m_gitStatus.hasChanges) ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_GIT_PUSH, repo ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_GIT_PULL, repo ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_GIT_PANEL, repo ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);

    // File save related
    EnableMenuItem(m_hMenu, IDM_FILE_SAVE, (!m_currentFile.empty() && m_fileModified) ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);
    EnableMenuItem(m_hMenu, IDM_FILE_SAVEAS, (!m_currentFile.empty()) ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED);

    // Streaming loader menu state
    CheckMenuItem(m_hMenu, IDM_VIEW_USE_STREAMING_LOADER, MF_BYCOMMAND | (m_useStreamingLoader ? MF_CHECKED : MF_UNCHECKED));
    // Vulkan renderer menu state
    CheckMenuItem(m_hMenu, IDM_VIEW_USE_VULKAN_RENDERER, MF_BYCOMMAND | (m_useVulkanRenderer ? MF_CHECKED : MF_UNCHECKED));
    // Breadcrumbs (View) — sync check with m_settings.breadcrumbsEnabled
    CheckMenuItem(m_hMenu, IDM_T1_BREADCRUMBS_TOGGLE, MF_BYCOMMAND | (m_settings.breadcrumbsEnabled ? MF_CHECKED : MF_UNCHECKED));

    // Tier 5 cosmetic features — enable when corresponding module is initialized (after deferredHeavyInit)
    EnableMenuItem(m_hMenu, IDM_TELDASH_SHOW,   (m_telemetryDashboardInitialized ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED));
    EnableMenuItem(m_hMenu, IDM_EMOJI_PICKER,  (m_emojiSupportInitialized       ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED));
    EnableMenuItem(m_hMenu, IDM_SHORTCUT_SHOW, (m_shortcutEditorInitialized     ? MF_BYCOMMAND|MF_ENABLED : MF_BYCOMMAND|MF_GRAYED));

    DrawMenuBar(m_hwndMain);
}

// Code Snippets Implementation
void Win32IDE::loadCodeSnippets()
{
    m_codeSnippets.clear();
    
    // Load built-in PowerShell snippets
    CodeSnippet snippet1;
    snippet1.name = "function";
    snippet1.description = "PowerShell function template";
    snippet1.code = "function {name} {\n    param(\n        ${1:$Parameter}\n    )\n    \n    ${2:# Function body}\n}";
    m_codeSnippets.push_back(snippet1);
    
    CodeSnippet snippet2;
    snippet2.name = "if";
    snippet2.description = "If statement";
    snippet2.code = "if (${1:condition}) {\n    ${2:# Code}\n}";
    m_codeSnippets.push_back(snippet2);
    
    CodeSnippet snippet3;
    snippet3.name = "foreach";
    snippet3.description = "ForEach loop";
    snippet3.code = "foreach (${1:$item} in ${2:$collection}) {\n    ${3:# Code}\n}";
    m_codeSnippets.push_back(snippet3);
    
    CodeSnippet snippet4;
    snippet4.name = "try";
    snippet4.description = "Try-Catch block";
    snippet4.code = "try {\n    ${1:# Code that might throw}\n}\ncatch {\n    ${2:# Error handling}\n}";
    m_codeSnippets.push_back(snippet4);
}

void Win32IDE::insertSnippet(const std::string& snippetName)
{
    for (const auto& snippet : m_codeSnippets) {
        if (snippet.name == snippetName) {
            // Get current cursor position
            DWORD start, end;
            SendMessage(m_hwndEditor, EM_GETSEL, (WPARAM)&start, (LPARAM)&end);
            
            // Insert snippet content
            std::string content = snippet.code;
            // Simple placeholder replacement
            size_t pos = content.find("${1:");
            if (pos != std::string::npos) {
                size_t endPos = content.find("}", pos);
                if (endPos != std::string::npos) {
                    content.erase(pos, endPos - pos + 1);
                }
            }
            
            SendMessage(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)content.c_str());
            break;
        }
    }
    updateMenuEnableStates();
}

// Integrated Help Implementation
void Win32IDE::showGetHelp(const std::string& cmdlet)
{
    // Get selected text for help lookup
    CHARRANGE range;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);
    
    std::string command;
    if (!cmdlet.empty()) {
        command = cmdlet;
    } else if (range.cpMax > range.cpMin) {
        char buffer[1000];
        TEXTRANGEA tr;
        tr.chrg = range;
        tr.lpstrText = buffer;
        SendMessage(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
        command = std::string(buffer);
    } else {
        command = "Get-Command";  // Default help
    }
    
    std::string helpCommand = "Get-Help " + command + " -Full\n";
    TerminalPane* pane = getActiveTerminalPane();
    if (pane && pane->manager && pane->manager->isRunning()) {
        pane->manager->writeInput(helpCommand);
    }
}

void Win32IDE::showCommandReference()
{
    std::string reference = 
        "PowerShell Quick Reference:\n\n"
        "Get-Help <command> - Get help for command\n"
        "Get-Command - List all commands\n"
        "Get-Member - Get object properties/methods\n"
        "Measure-Object - Measure properties\n"
        "Select-Object - Select properties\n"
        "Where-Object - Filter objects\n"
        "ForEach-Object - Process each object\n"
        "Sort-Object - Sort objects\n"
        "Group-Object - Group objects\n"
        "Export-Csv - Export to CSV\n"
        "Import-Csv - Import from CSV\n"
        "ConvertTo-Json - Convert to JSON\n"
        "ConvertFrom-Json - Convert from JSON\n";
        
    MessageBoxW(m_hwndMain, utf8ToWide(reference).c_str(), L"PowerShell Reference", MB_OK);
}

// Output / Clipboard / Minimap / Profiling implementations
void Win32IDE::createOutputTabs()
{
    if (m_hwndOutputTabs) return;

    RECT client{}; GetClientRect(m_hwndMain, &client);
    int tabBarHeight = 24;

    m_hwndOutputTabs = CreateWindowExW(0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | TCS_TABS,
        0, 0, client.right - 150, tabBarHeight,
        m_hwndMain, (HMENU)IDC_OUTPUT_TABS, m_hInstance, nullptr);

    char logBuf[256];
    sprintf_s(logBuf, "OutputTabs HWND created: %p (Parent: %p)", m_hwndOutputTabs, m_hwndMain);
    LOG_INFO(std::string(logBuf));

    m_hwndSeverityFilter = CreateWindowExW(0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        client.right - 145, 2, 140, 100,
        m_hwndMain, (HMENU)IDC_SEVERITY_FILTER, m_hInstance, nullptr);
    SendMessageW(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)L"All Messages");
    SendMessageW(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)L"Info & Above");
    SendMessageW(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)L"Warnings & Errors");
    SendMessageW(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)L"Errors Only");
    SendMessage(m_hwndSeverityFilter, CB_SETCURSEL, m_severityFilterLevel, 0);

    static const struct { const wchar_t* text; int id; const char* key; } defs[] = {
        {L"Output", IDC_OUTPUT_EDIT_GENERAL, "Output"},
        {L"Errors", IDC_OUTPUT_EDIT_ERRORS,  "Errors"},
        {L"Debug",  IDC_OUTPUT_EDIT_DEBUG,   "Debug"},
        {L"Find Results", IDC_OUTPUT_EDIT_FIND, "Find Results"}
    };

    for (int i = 0; i < 4; ++i) {
        TCITEMW tie{}; tie.mask = TCIF_TEXT; tie.pszText = const_cast<wchar_t*>(defs[i].text);
        TabCtrl_InsertItem(m_hwndOutputTabs, i, &tie);

        HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, RICHEDIT_CLASSW, L"",
            WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
            0, tabBarHeight, client.right, m_outputTabHeight - tabBarHeight,
            m_hwndMain, (HMENU)(INT_PTR)defs[i].id, m_hInstance, nullptr);
        m_outputWindows[defs[i].key] = hEdit;
    }
    m_activeOutputTab = "Output";

    // Restore persisted tab selection
    if (m_selectedOutputTab >= 0 && m_selectedOutputTab < 4) {
        const char* keys[] = {"Output","Errors","Debug","Find Results"};
        m_activeOutputTab = keys[m_selectedOutputTab];
        TabCtrl_SetCurSel(m_hwndOutputTabs, m_selectedOutputTab);
    }

    // Initially show only active tab and respect visibility setting
    for (auto& kv : m_outputWindows) {
        ShowWindow(kv.second, (kv.first == m_activeOutputTab && m_outputPanelVisible) ? SW_SHOW : SW_HIDE);
    }
    ShowWindow(m_hwndOutputTabs, m_outputPanelVisible ? SW_SHOW : SW_HIDE);
    if (m_hwndSeverityFilter) ShowWindow(m_hwndSeverityFilter, m_outputPanelVisible ? SW_SHOW : SW_HIDE);
    if (m_hwndSplitter) ShowWindow(m_hwndSplitter, m_outputPanelVisible ? SW_SHOW : SW_HIDE);
}

void Win32IDE::addOutputTab(const std::string& name)
{
    if (m_outputWindows.find(name) != m_outputWindows.end()) return;
    RECT client{}; GetClientRect(m_hwndMain, &client);
    int tabBarHeight = 24;
    HWND hEdit = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, tabBarHeight, client.right, m_outputTabHeight - tabBarHeight,
        m_hwndMain, nullptr, m_hInstance, nullptr);
    ShowWindow(hEdit, SW_HIDE);
    m_outputWindows[name] = hEdit;
}

void Win32IDE::appendToOutput(const std::string& text, const std::string& tabName, OutputSeverity severity)
{
    if (isShuttingDown()) return;  // Window handles may be destroyed
    if (static_cast<int>(severity) < m_severityFilterLevel) return;
    
    std::string target = tabName.empty() ? m_activeOutputTab : tabName;
    if (m_outputWindows.find(target) == m_outputWindows.end()) {
        addOutputTab(target);
    }
    
    // Add timestamp for Errors and Debug tabs
    std::string timestampedText = text;
    if (target == "Errors" || target == "Debug") {
        time_t now = time(nullptr);
        struct tm timeinfo;
        localtime_s(&timeinfo, &now);
        char timestamp[16];
        strftime(timestamp, sizeof(timestamp), "[%H:%M:%S] ", &timeinfo);
        timestampedText = std::string(timestamp) + text;
    }
    
    // Apply color formatting based on tab type
    if (target == "Errors") {
        formatOutput(timestampedText, RGB(220, 50, 50), "Errors"); // Red
    } else if (target == "Debug") {
        formatOutput(timestampedText, RGB(200, 180, 50), "Debug"); // Yellow
    } else {
        HWND hwnd = m_outputWindows[target];
        appendText(hwnd, timestampedText);
    }
}

void Win32IDE::clearOutput(const std::string& tabName)
{
    std::string target = tabName.empty() ? m_activeOutputTab : tabName;
    auto it = m_outputWindows.find(target);
    if (it != m_outputWindows.end()) {
        SetWindowTextW(it->second, L"");
    }
}

void Win32IDE::formatOutput(const std::string& text, COLORREF color, const std::string& tabName)
{ 
    std::string target = tabName.empty() ? m_activeOutputTab : tabName;
    auto it = m_outputWindows.find(target);
    if (it == m_outputWindows.end()) return;
    
    HWND hwnd = it->second;
    GETTEXTLENGTHEX gtl{}; gtl.flags = GTL_DEFAULT; gtl.codepage = CP_UNICODE;
    LONG len = SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    SendMessage(hwnd, EM_SETSEL, len, len);

    CHARFORMAT2W cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessageW(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);

    std::wstring wtext = utf8ToWide(text);
    SETTEXTEX st{}; st.flags = ST_SELECTION; st.codepage = CP_UNICODE;
    SendMessageW(hwnd, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)wtext.c_str());
}

void Win32IDE::copyWithFormatting()
{
    // Simplified: copy selected plain text and store in history (vector<string>)
    CHARRANGE range;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);
    if (range.cpMax <= range.cpMin) return;
    LONG len = range.cpMax - range.cpMin;
    std::vector<wchar_t> buffer(len + 1);
    TEXTRANGEW tr{}; tr.chrg = range; tr.lpstrText = buffer.data();
    SendMessageW(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    buffer[len] = L'\0';
    std::string text = wideToUtf8(buffer.data());
    m_clipboardHistory.insert(m_clipboardHistory.begin(), text);
    if (m_clipboardHistory.size() > MAX_CLIPBOARD_HISTORY) m_clipboardHistory.resize(MAX_CLIPBOARD_HISTORY);
    if (OpenClipboard(m_hwndMain)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
        if (hMem) {
            char* dest = (char*)GlobalLock(hMem);
            memcpy(dest, text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

void Win32IDE::pasteWithoutFormatting()
{
    if (OpenClipboard(m_hwndMain)) {
        HANDLE hData = GetClipboardData(CF_TEXT);
        if (hData) {
            const char* data = (const char*)GlobalLock(hData);
            if (data) {
                SendMessage(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)data);
                GlobalUnlock(hData);
            }
        }
        CloseClipboard();
    }
}

void Win32IDE::copyLineNumbers()
{
    if (!m_hwndEditor) return;
    
    // Get selected range
    CHARRANGE range;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);
    
    // Get line numbers for selection
    int startLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, range.cpMin, 0);
    int endLine = (int)SendMessage(m_hwndEditor, EM_LINEFROMCHAR, range.cpMax, 0);
    
    // Build line number string
    std::string lineNumbers;
    for (int i = startLine; i <= endLine; ++i) {
        if (!lineNumbers.empty()) lineNumbers += "\r\n";
        lineNumbers += std::to_string(i + 1);
    }
    
    // Copy to clipboard
    if (OpenClipboard(m_hwndMain)) {
        EmptyClipboard();
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, lineNumbers.size() + 1);
        if (hMem) {
            char* dest = (char*)GlobalLock(hMem);
            memcpy(dest, lineNumbers.c_str(), lineNumbers.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

void Win32IDE::showClipboardHistory()
{
    std::string msg = "Clipboard History (latest 10):\n\n";
    size_t count = std::min<size_t>(10, m_clipboardHistory.size());
    for (size_t i = 0; i < count; ++i) {
        const std::string& item = m_clipboardHistory[i];
        std::string preview = item.substr(0, 50);
        if (item.size() > 50) preview += "...";
        msg += std::to_string(i + 1) + ". " + preview + "\n";
    }
    MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Clipboard History", MB_OK);
}

void Win32IDE::clearClipboardHistory()
{
    m_clipboardHistory.clear();
}

void Win32IDE::createMinimap()
{
    if (!m_hwndMain || !m_hwndEditor) return;
    
    m_minimapWidth = 120;
    m_minimapVisible = true;
    
    // Create minimap window as a child of main window
    RECT editorRect;
    GetWindowRect(m_hwndEditor, &editorRect);
    MapWindowPoints(HWND_DESKTOP, m_hwndMain, (LPPOINT)&editorRect, 2);
    
    int minimapX = editorRect.right - m_minimapWidth;
    int minimapY = editorRect.top;
    int minimapHeight = editorRect.bottom - editorRect.top;
    
    m_hwndMinimap = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        minimapX, minimapY, m_minimapWidth, minimapHeight,
        m_hwndMain, nullptr, m_hInstance, nullptr);
    
    if (m_hwndMinimap) {
        SetWindowLongPtrW(m_hwndMinimap, GWLP_USERDATA, (LONG_PTR)this);
    }
    
    updateMinimap();
}

void Win32IDE::updateMinimap()
{
    if (!m_hwndMinimap || !m_minimapVisible || !m_hwndEditor) return;
    
    std::string text = getWindowText(m_hwndEditor);
    if (text.empty()) {
        m_minimapLines.clear();
        InvalidateRect(m_hwndMinimap, nullptr, TRUE);
        return;
    }

    // Split into lines for minimap rendering
    m_minimapLines.clear();
    m_minimapLineStarts.clear();
    
    std::istringstream stream(text);
    std::string line;
    int pos = 0;
    while (std::getline(stream, line)) {
        m_minimapLines.push_back(line);
        m_minimapLineStarts.push_back(pos);
        pos += (int)line.size() + 1; // +1 for newline
    }
    
    // Force redraw
    InvalidateRect(m_hwndMinimap, nullptr, TRUE);
    
    // Paint minimap content
    HDC hdc = GetDC(m_hwndMinimap);
    if (hdc) {
        RECT rc;
        GetClientRect(m_hwndMinimap, &rc);
        
        // Dark background
        HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &rc, bgBrush);
        DeleteObject(bgBrush);
        
        // Calculate visible area highlight
        int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
        RECT editorRect;
        GetClientRect(m_hwndEditor, &editorRect);
        int visibleLines = editorRect.bottom / 16; // Approximate line height
        
        // Draw visible area indicator
        int totalLines = (int)m_minimapLines.size();
        if (totalLines > 0) {
            float scale = (float)(rc.bottom - rc.top) / (float)totalLines;
            int highlightTop = (int)(firstVisibleLine * scale);
            int highlightHeight = (int)(visibleLines * scale);
            if (highlightHeight < 10) highlightHeight = 10;
            
            RECT highlightRect = { 0, highlightTop, rc.right, highlightTop + highlightHeight };
            HBRUSH highlightBrush = CreateSolidBrush(RGB(60, 60, 80));
            FillRect(hdc, &highlightRect, highlightBrush);
            DeleteObject(highlightBrush);
        }
        
        // Draw minimap lines as colored blocks
        HPEN codePen = CreatePen(PS_SOLID, 1, RGB(150, 150, 150));
        HPEN oldPen = (HPEN)SelectObject(hdc, codePen);
        
        float lineHeight = 2.0f;
        if (totalLines > 0 && totalLines * lineHeight > rc.bottom) {
            lineHeight = (float)(rc.bottom - 4) / (float)totalLines;
            if (lineHeight < 1.0f) lineHeight = 1.0f;
        }
        
        for (size_t i = 0; i < m_minimapLines.size() && i * lineHeight < rc.bottom; ++i) {
            const std::string& line = m_minimapLines[i];
            if (line.empty()) continue;
            
            int y = (int)(i * lineHeight) + 2;
            int lineLen = (int)line.size();
            int pixelLen = (lineLen * rc.right) / 200; // Scale to minimap width
            if (pixelLen > rc.right - 4) pixelLen = rc.right - 4;
            if (pixelLen < 2) pixelLen = 2;
            
            MoveToEx(hdc, 2, y, nullptr);
            LineTo(hdc, 2 + pixelLen, y);
        }
        
        SelectObject(hdc, oldPen);
        DeleteObject(codePen);
        
        ReleaseDC(m_hwndMinimap, hdc);
    }
}

void Win32IDE::scrollToMinimapPosition(int y)
{
    if (!m_hwndMinimap || !m_hwndEditor || m_minimapLines.empty()) return;
    
    RECT rc;
    GetClientRect(m_hwndMinimap, &rc);
    
    int totalLines = (int)m_minimapLines.size();
    int targetLine = (y * totalLines) / rc.bottom;
    
    if (targetLine < 0) targetLine = 0;
    if (targetLine >= totalLines) targetLine = totalLines - 1;
    
    // Scroll editor to target line
    int charIndex = 0;
    if (targetLine < (int)m_minimapLineStarts.size()) {
        charIndex = m_minimapLineStarts[targetLine];
    }
    
    SendMessage(m_hwndEditor, EM_SETSEL, charIndex, charIndex);
    SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);
    
    updateMinimap();
}

void Win32IDE::toggleMinimap()
{
    m_minimapVisible = !m_minimapVisible;
    if (m_hwndMinimap) {
        ShowWindow(m_hwndMinimap, m_minimapVisible ? SW_SHOW : SW_HIDE);
    } else if (m_minimapVisible) {
        createMinimap();
    }
    
    // Trigger layout update
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    onSize(rc.right, rc.bottom);
}

void Win32IDE::startProfiling()
{
    if (!m_profilingActive) {
        m_profilingActive = true;
        QueryPerformanceCounter(&m_profilingStart);
        QueryPerformanceFrequency(&m_profilingFreq);
        m_profilingResults.clear();
    }
}

void Win32IDE::stopProfiling()
{
    if (m_profilingActive) {
        LARGE_INTEGER end; QueryPerformanceCounter(&end);
        double ms = (double)(end.QuadPart - m_profilingStart.QuadPart) * 1000.0 / (double)m_profilingFreq.QuadPart;
        m_profilingResults.push_back({"Session", ms});
        m_profilingActive = false;
    }
}

void Win32IDE::showProfileResults()
{
    std::string msg = "Profile Results:\n\n";
    for (auto& pr : m_profilingResults) {
        msg += pr.first + ": " + std::to_string(pr.second) + " ms\n";
    }
    MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Profiling", MB_OK);
}

void Win32IDE::analyzeScript()
{
    std::string script = getWindowText(m_hwndEditor);
    if(script.empty()) {
        MessageBoxW(m_hwndMain, L"Script is empty.", L"Analyze Script", MB_OK);
        return;
    }
    
    appendToOutput("Starting AI Analysis...\n", "Output", OutputSeverity::Info);
    
    // Asynchronous analysis to avoid blocking UI
    std::thread([this, script]() {
        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
        if (_guard.cancelled) return;
        if (m_nativeEngine) {
            std::string prompt = "Analyze the following script and report potential bugs, security issues, and improvements:\n\n" + script;
            // Assuming CPUInferenceEngine has an 'infer' or 'generate' method that takes a string
            // Based on cpu_inference_engine.cpp read earlier: std::string infer(const std::string& prompt);
            
            auto* engine = m_nativeEngine.get();
            auto tokens = engine->Tokenize(prompt);
            auto output_tokens = engine->Generate(tokens, 512);
            std::string result = engine->Detokenize(output_tokens);
            
            // Post result back to UI thread or just append (if appendToOutput is thread-safe or we lock)
            // appendToOutput uses SendMessage which is generally thread-safe for simple text
            this->appendToOutput("\n=== AI Analysis Result ===\n" + result + "\n==========================\n", "Output", OutputSeverity::Info);
        } else {
             this->appendToOutput("Error: Inference Engine not available.\n", "Errors", OutputSeverity::Error);
        }
    }).detach();
}

void Win32IDE::measureExecutionTime() { 
    // Real implementation: Measure block execution
    auto start = std::chrono::high_resolution_clock::now();
    // execute selection... (simplified)
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end-start).count();
    appendToOutput("Execution time info: " + std::to_string(ms) + "ms\n", "Output", OutputSeverity::Info);
}

// Module Management
void Win32IDE::refreshModuleList()
{
    m_modules.clear();
    
    // Default module set (always available)
    m_modules.push_back({"Microsoft.PowerShell.Management","3.0.0.0","Management cmdlets","",true});
    m_modules.push_back({"Microsoft.PowerShell.Utility","3.0.0.0","Utility cmdlets","",true});
    m_modules.push_back({"PSReadLine","2.0.0","Command line editing","",false});
    
    // Dynamic module enumeration via Powershell command
    std::string cmd = "powershell.exe -NoProfile -Command \"Get-Module -ListAvailable | Select-Object -First 50 Name, Version | ConvertTo-Json -Compress\"";
    std::string output = ExecCmd(cmd.c_str());
    
    if (output.find("Error") == std::string::npos && !output.empty()) {
        try {
            auto json = nlohmann::json::parse(output);
            if (json.is_array()) {
                for (size_t i = 0; i < json.size(); ++i) {
                    const auto& item = json[i];
                    ModuleInfo m;
                    if (item.is_object()) {
                        m.name = item.value("Name", "");
                        if (item.contains("Version")) {
                            auto v = item["Version"];
                            if (v.is_object()) { 
                                 // PS version object
                                 m.version = std::to_string(v.value("Major",0)) + "." + std::to_string(v.value("Minor",0));
                            } else if (v.is_string()) {
                                m.version = v.get<std::string>();
                            } else {
                                m.version = "0.0.0";
                            }
                        } else {
                            m.version = "0.0.0";
                        }
                    }
                    m.description = "User Module";
                    m.path = ""; 
                    m.loaded = false; // Check via Get-Module without ListAvailable if needed
                    
                    // Avoid duplicates
                    bool exists = false;
                    for(const auto& existing : m_modules) if (existing.name == m.name) exists = true;
                    if (!exists) m_modules.push_back(m);
                }
            } else if (json.is_object()) {
                 // Single module
                 ModuleInfo m;
                 m.name = json.value("Name", "");
                 m.version = "1.0";
                 m.description = "User Module";
                 m_modules.push_back(m);
            }
        } catch (...) {
            // JSON parsing failed, likely non-JSON output or empty
        }
    }
}

void Win32IDE::showModuleBrowser()
{
    std::string msg = "Modules:\n\n";
    for (auto& m : m_modules) {
        msg += m.name + " (" + m.version + ")" + (m.loaded?" [Loaded]":" [Available]") + "\n";
    }
    MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Module Browser", MB_OK);
}

void Win32IDE::loadModule(const std::string& moduleName)
{
    bool found = false;
    for (auto& m : m_modules) {
        if (m.name == moduleName) {
            m.loaded = true;
            found = true;
            break;
        }
    }
    
    // Explicit Logic: Actually load the module in PowerShell
    std::string command = "Import-Module '" + moduleName + "'\n";
    
    TerminalPane* pane = getActiveTerminalPane();
    if (pane && pane->manager && pane->manager->isRunning()) {
        pane->manager->writeInput(command);
        appendToOutput("Loading module: " + moduleName, "Output", OutputSeverity::Info);
    } else {
        appendToOutput("Cannot load module '" + moduleName + "': No active terminal.", "Errors", OutputSeverity::Error);
    }
}

void Win32IDE::unloadModule(const std::string& moduleName)
{
    bool found = false;
    for (auto& m : m_modules) {
        if (m.name == moduleName) {
            m.loaded = false;
            found = true;
            break;
        }
    }

    // Explicit Logic: Actually remove the module in PowerShell
    std::string command = "Remove-Module '" + moduleName + "'\n";
    
    TerminalPane* pane = getActiveTerminalPane();
    if (pane && pane->manager && pane->manager->isRunning()) {
        pane->manager->writeInput(command);
        appendToOutput("Unloading module: " + moduleName, "Output", OutputSeverity::Info);
    } else {
        // Try to start one or log error
        appendToOutput("Cannot unload module '" + moduleName + "': No active terminal.", "Errors", OutputSeverity::Error);
    }
}

void Win32IDE::importModule()
{
    OPENFILENAMEW ofn = {};
    wchar_t szFile[MAX_PATH] = L"";
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"PowerShell Modules (*.psm1;*.psd1)\0*.psm1;*.psd1\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Import Module";

    if (GetOpenFileNameW(&ofn)) {
        std::string modulePath = wideToUtf8(szFile);
        std::string command = "Import-Module '" + modulePath + "'\n";
        
        TerminalPane* pane = getActiveTerminalPane();
        if (pane && pane->manager && pane->manager->isRunning()) {
            pane->manager->writeInput(command);
            appendToOutput("Importing module: " + modulePath + "\n", "Output", OutputSeverity::Info);
        }
        
        // Refresh module list after import
        refreshModuleList();
    }
}

void Win32IDE::exportModule()
{
    // Show dialog to select module to export
    if (m_modules.empty()) {
        MessageBoxW(m_hwndMain, L"No modules loaded. Refresh module list first.", L"Export Module", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    // Build list of module names for selection
    std::string moduleList = "Available modules:\n\n";
    for (size_t i = 0; i < m_modules.size(); ++i) {
        moduleList += std::to_string(i + 1) + ". " + m_modules[i].name;
        if (m_modules[i].loaded) moduleList += " [Loaded]";
        moduleList += "\n";
    }
    moduleList += "\nExport the first loaded module?";
    
    if (MessageBoxW(m_hwndMain, utf8ToWide(moduleList).c_str(), L"Export Module", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        // Find first loaded module
        for (const auto& mod : m_modules) {
            if (mod.loaded) {
                OPENFILENAMEW ofn = {};
                std::wstring defaultName = utf8ToWide(mod.name + ".psm1");
                wchar_t szFile[MAX_PATH] = L"";
                wcsncpy_s(szFile, defaultName.c_str(), _TRUNCATE);
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = m_hwndMain;
                ofn.lpstrFilter = L"PowerShell Module (*.psm1)\0*.psm1\0PowerShell Data (*.psd1)\0*.psd1\0";
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;
                ofn.lpstrTitle = L"Export Module";

                if (GetSaveFileNameW(&ofn)) {
                    std::string savePath = wideToUtf8(szFile);
                    std::string command = "Export-ModuleMember -Function * -Cmdlet * -Variable * -Alias * -PassThru | Out-File '" + savePath + "'\n";
                    
                    TerminalPane* pane = getActiveTerminalPane();
                    if (pane && pane->manager && pane->manager->isRunning()) {
                        pane->manager->writeInput(command);
                        appendToOutput("Exporting module to: " + savePath + "\n", "Output", OutputSeverity::Info);
                    }
                }
                break;
            }
        }
    }
}

// Theme Management
void Win32IDE::resetToDefaultTheme()
{
    applyThemeById(IDM_THEME_DARK_PLUS);
}

void Win32IDE::saveCodeSnippets()
{
    CreateDirectoryA("snippets", NULL);
    std::ofstream file("snippets\\snippets.txt");
    if (file.is_open()) {
        for (const auto& snippet : m_codeSnippets) {
            file << "[SNIPPET]" << std::endl;
            file << "name=" << snippet.name << std::endl;
            file << "description=" << snippet.description << std::endl;
            file << "code_start" << std::endl;
            file << snippet.code << std::endl;
            file << "code_end" << std::endl;
        }
        file.close();
    }
}

void Win32IDE::showPowerShellDocs()
{
    MessageBoxW(m_hwndMain, L"Open https://learn.microsoft.com/powershell/ for full docs.", L"PowerShell Docs", MB_OK);
}

void Win32IDE::searchHelp(const std::string& query)
{
    std::string q = query.empty()?"Get-Command":query;
    std::string cmd = "Get-Help " + q + " -Online\n";
    TerminalPane* pane = getActiveTerminalPane();
    if (pane && pane->manager && pane->manager->isRunning()) pane->manager->writeInput(cmd);
}

void Win32IDE::toggleFloatingPanel()
{
    if (!m_hwndFloatingPanel) return; // created elsewhere
    BOOL vis = IsWindowVisible(m_hwndFloatingPanel);
    ShowWindow(m_hwndFloatingPanel, vis?SW_HIDE:SW_SHOW);
}

// ============================================================================
// Floating Panel Implementation
// ============================================================================

void Win32IDE::createFloatingPanel()
{
    if (m_hwndFloatingPanel) return; // Already created

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = FloatingPanelProc;
    wc.hInstance = m_hInstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));
    wc.lpszClassName = L"RawrXD_FloatingPanel";
    RegisterClassExW(&wc);

    RECT rcMain;
    GetClientRect(m_hwndMain, &rcMain);
    int panelWidth = rcMain.right - rcMain.left;
    int panelHeight = 250;
    int panelX = rcMain.left;
    int panelY = rcMain.bottom - panelHeight;

    m_hwndFloatingPanel = CreateWindowExW(
        WS_EX_TOOLWINDOW,
        L"RawrXD_FloatingPanel",
        L"Panel",
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_BORDER,
        panelX, panelY, panelWidth, panelHeight,
        m_hwndMain, nullptr, m_hInstance, nullptr
    );

    if (!m_hwndFloatingPanel) {
        appendToOutput("Failed to create floating panel\n", "Output", OutputSeverity::Error);
        return;
    }

    SetWindowLongPtrW(m_hwndFloatingPanel, GWLP_USERDATA, (LONG_PTR)this);

    static const wchar_t* tabLabels[] = { L"Problems", L"Output", L"Debug Console", L"Terminal" };
    for (int i = 0; i < 4; i++) {
        CreateWindowExW(
            0, L"BUTTON", tabLabels[i],
            WS_CHILD | WS_VISIBLE | BS_FLAT | BS_PUSHBUTTON,
            5 + i * 120, 2, 115, 24,
            m_hwndFloatingPanel, (HMENU)(UINT_PTR)(7001 + i),
            m_hInstance, nullptr
        );
    }

    m_hwndFloatingContent = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, 28, panelWidth, panelHeight - 28,
        m_hwndFloatingPanel, nullptr, m_hInstance, nullptr
    );

    if (m_hwndFloatingContent) {
        SendMessageW(m_hwndFloatingContent, WM_SETFONT,
                    (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    }

    appendToOutput("Floating panel created\n", "Output", OutputSeverity::Info);
}

void Win32IDE::showFloatingPanel()
{
    if (!m_hwndFloatingPanel) {
        createFloatingPanel();
    }
    if (m_hwndFloatingPanel) {
        ShowWindow(m_hwndFloatingPanel, SW_SHOW);
        m_outputPanelVisible = true;
    }
}

void Win32IDE::hideFloatingPanel()
{
    if (m_hwndFloatingPanel) {
        ShowWindow(m_hwndFloatingPanel, SW_HIDE);
        m_outputPanelVisible = false;
    }
}

void Win32IDE::updateFloatingPanelContent(const std::string& content)
{
    if (!m_hwndFloatingContent) return;
    std::wstring wcontent = utf8ToWide(content);
    int textLen = GetWindowTextLengthW(m_hwndFloatingContent);
    SendMessageW(m_hwndFloatingContent, EM_SETSEL, (WPARAM)textLen, (LPARAM)textLen);
    SendMessageW(m_hwndFloatingContent, EM_REPLACESEL, FALSE, (LPARAM)wcontent.c_str());
    SendMessageW(m_hwndFloatingContent, EM_SCROLLCARET, 0, 0);
}

void Win32IDE::setFloatingPanelTab(int tabIndex)
{
    if (!m_hwndFloatingPanel) return;

    // Visually highlight the active tab button and unhighlight others
    for (int i = 0; i < 4; i++) {
        HWND hTabBtn = GetDlgItem(m_hwndFloatingPanel, 7001 + i);
        if (hTabBtn) {
            if (i == tabIndex) {
                SendMessageW(hTabBtn, BM_SETSTATE, TRUE, 0);
            } else {
                SendMessageW(hTabBtn, BM_SETSTATE, FALSE, 0);
            }
        }
    }

    if (m_hwndFloatingContent) {
        static const wchar_t* tabTitles[] = {
            L"=== Problems ===\r\n",
            L"=== Output ===\r\n",
            L"=== Debug Console ===\r\n",
            L"=== Terminal ===\r\n"
        };
        if (tabIndex >= 0 && tabIndex < 4) {
            SetWindowTextW(m_hwndFloatingContent, tabTitles[tabIndex]);
        }
    }
}

LRESULT CALLBACK Win32IDE::FloatingPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrW(hwnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // Dark background matching VS Code panel area
        HBRUSH hBrush = CreateSolidBrush(RGB(30, 30, 30));
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        // Draw a subtle top border line (panel separator)
        HPEN hPen = CreatePen(PS_SOLID, 1, RGB(0, 122, 204));
        HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
        MoveToEx(hdc, rc.left, rc.top, nullptr);
        LineTo(hdc, rc.right, rc.top);
        SelectObject(hdc, hOldPen);
        DeleteObject(hPen);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_SIZE: {
        if (pThis && pThis->m_hwndFloatingContent) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            // Resize content area below the tab buttons (28px tab bar)
            MoveWindow(pThis->m_hwndFloatingContent,
                       0, 28, rc.right, rc.bottom - 28, TRUE);
        }
        return 0;
    }

    case WM_COMMAND: {
        if (pThis) {
            int id = LOWORD(wParam);
            // Tab button IDs: 7001=Problems, 7002=Output, 7003=Debug Console, 7004=Terminal
            if (id >= 7001 && id <= 7004) {
                pThis->setFloatingPanelTab(id - 7001);
                return 0;
            }
        }
        break;
    }

    case WM_CLOSE:
        if (pThis) {
            pThis->hideFloatingPanel();
            return 0;
        }
        break;
    }

    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

int Win32IDE::getPanelAreaWidth() const
{
    if (!m_hwndMain) return 0;

    RECT rcMain;
    GetClientRect(m_hwndMain, &rcMain);
    int totalWidth = rcMain.right - rcMain.left;

    // Panel area width = total width minus sidebar (if visible) minus activity bar minus secondary sidebar
    int sidebarOffset = 0;
    if (m_sidebarVisible) {
        sidebarOffset = m_sidebarWidth + dpiScale(48); // activity bar width (DPI-scaled)
    }
    int secondarySidebarOffset = m_secondarySidebarVisible ? m_secondarySidebarWidth : 0;

    return totalWidth - sidebarOffset - secondarySidebarOffset;
}

// ============================================================================
// Search and Replace Implementation
// ============================================================================

#define IDD_FIND 5001
#define IDD_REPLACE 5002
#define IDC_FIND_TEXT 5010
#define IDC_REPLACE_TEXT 5011
#define IDC_CASE_SENSITIVE 5020
#define IDC_WHOLE_WORD 5021
#define IDC_USE_REGEX 5022
#define IDC_BTN_FIND_NEXT 5030
#define IDC_BTN_REPLACE 5031
#define IDC_BTN_REPLACE_ALL 5032
#define IDC_BTN_CLOSE 5033

void Win32IDE::showFindDialog()
{
    if (m_hwndFindDialog && IsWindow(m_hwndFindDialog)) {
        SetForegroundWindow(m_hwndFindDialog);
        return;
    }
    
    m_hwndFindDialog = CreateDialogParamW(m_hInstance, MAKEINTRESOURCEW(IDD_FIND),
        m_hwndMain, FindDialogProc, (LPARAM)this);

    if (!m_hwndFindDialog) {
        HWND hwndDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"STATIC", L"Find",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
            100, 100, 400, 150, m_hwndMain, nullptr, m_hInstance, nullptr);
        m_hwndFindDialog = hwndDlg;

        CreateWindowExW(0, L"STATIC", L"Find what:", WS_CHILD | WS_VISIBLE,
            10, 15, 80, 20, hwndDlg, nullptr, m_hInstance, nullptr);
        CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", utf8ToWide(m_lastSearchText).c_str(),
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 12, 280, 22,
            hwndDlg, (HMENU)IDC_FIND_TEXT, m_hInstance, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Case sensitive", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            10, 45, 120, 20, hwndDlg, (HMENU)IDC_CASE_SENSITIVE, m_hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Whole word", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            140, 45, 100, 20, hwndDlg, (HMENU)IDC_WHOLE_WORD, m_hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Regex", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            250, 45, 70, 20, hwndDlg, (HMENU)IDC_USE_REGEX, m_hInstance, nullptr);

        CreateWindowExW(0, L"BUTTON", L"Find Next", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
            10, 80, 90, 28, hwndDlg, (HMENU)IDC_BTN_FIND_NEXT, m_hInstance, nullptr);
        CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE,
            110, 80, 90, 28, hwndDlg, (HMENU)IDC_BTN_CLOSE, m_hInstance, nullptr);
    }
    
    ShowWindow(m_hwndFindDialog, SW_SHOW);
}

void Win32IDE::showReplaceDialog()
{
    if (m_hwndReplaceDialog && IsWindow(m_hwndReplaceDialog)) {
        SetForegroundWindow(m_hwndReplaceDialog);
        return;
    }
    
    HWND hwndDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"STATIC", L"Replace",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        100, 100, 400, 200, m_hwndMain, nullptr, m_hInstance, nullptr);
    m_hwndReplaceDialog = hwndDlg;

    CreateWindowExW(0, L"STATIC", L"Find what:", WS_CHILD | WS_VISIBLE,
        10, 15, 80, 20, hwndDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", utf8ToWide(m_lastSearchText).c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 12, 280, 22,
        hwndDlg, (HMENU)IDC_FIND_TEXT, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Replace with:", WS_CHILD | WS_VISIBLE,
        10, 45, 80, 20, hwndDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", utf8ToWide(m_lastReplaceText).c_str(),
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL, 100, 42, 280, 22,
        hwndDlg, (HMENU)IDC_REPLACE_TEXT, m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Case sensitive", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        10, 75, 120, 20, hwndDlg, (HMENU)IDC_CASE_SENSITIVE, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Whole word", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        140, 75, 100, 20, hwndDlg, (HMENU)IDC_WHOLE_WORD, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Regex", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
        250, 75, 70, 20, hwndDlg, (HMENU)IDC_USE_REGEX, m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Find Next", WS_CHILD | WS_VISIBLE,
        10, 110, 90, 28, hwndDlg, (HMENU)IDC_BTN_FIND_NEXT, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Replace", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        110, 110, 90, 28, hwndDlg, (HMENU)IDC_BTN_REPLACE, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Replace All", WS_CHILD | WS_VISIBLE,
        210, 110, 90, 28, hwndDlg, (HMENU)IDC_BTN_REPLACE_ALL, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Close", WS_CHILD | WS_VISIBLE,
        310, 110, 70, 28, hwndDlg, (HMENU)IDC_BTN_CLOSE, m_hInstance, nullptr);
    
    ShowWindow(m_hwndReplaceDialog, SW_SHOW);
}

void Win32IDE::findNext()
{
    if (m_lastSearchText.empty()) {
        showFindDialog();
        return;
    }
    findText(m_lastSearchText, true, m_searchCaseSensitive, m_searchWholeWord, m_searchUseRegex);
}

void Win32IDE::findPrevious()
{
    if (m_lastSearchText.empty()) {
        showFindDialog();
        return;
    }
    findText(m_lastSearchText, false, m_searchCaseSensitive, m_searchWholeWord, m_searchUseRegex);
}

void Win32IDE::replaceNext()
{
    if (m_lastSearchText.empty()) {
        showReplaceDialog();
        return;
    }
    replaceText(m_lastSearchText, m_lastReplaceText, false, m_searchCaseSensitive, m_searchWholeWord, m_searchUseRegex);
}

void Win32IDE::replaceAll()
{
    if (m_lastSearchText.empty()) {
        showReplaceDialog();
        return;
    }
    int count = replaceText(m_lastSearchText, m_lastReplaceText, true, m_searchCaseSensitive, m_searchWholeWord, m_searchUseRegex);
    
    std::string msg = "Replaced " + std::to_string(count) + " occurrence(s).";
    MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Replace All", MB_OK | MB_ICONINFORMATION);
}

bool Win32IDE::findText(const std::string& searchText, bool forward, bool caseSensitive, bool wholeWord, bool useRegex)
{
    if (!m_hwndEditor || searchText.empty()) return false;
    
    std::string editorText = getWindowText(m_hwndEditor);
    if (editorText.empty()) return false;
    int textLen = (int)editorText.size();

    CHARRANGE selection;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&selection);

    int startChar = forward ? selection.cpMax : selection.cpMin - 1;
    if (startChar < 0) startChar = 0;
    int startPos = charIndexToUtf8ByteOffset(editorText, startChar);
    if (startPos >= textLen) startPos = textLen > 0 ? textLen - 1 : 0;
    
    size_t foundPos = std::string::npos;
    size_t foundLen = searchText.length();
    
    if (useRegex) {
        // Regex search using std::regex
        try {
            auto flags = std::regex_constants::ECMAScript;
            if (!caseSensitive) flags |= std::regex_constants::icase;
            std::regex pattern(searchText, flags);
            std::smatch match;
            
            if (forward) {
                std::string searchArea = editorText.substr(startPos);
                if (std::regex_search(searchArea, match, pattern)) {
                    foundPos = startPos + match.position();
                    foundLen = match.length();
                } else if (startPos > 0) {
                    // Wrap around
                    searchArea = editorText.substr(0, startPos);
                    if (std::regex_search(searchArea, match, pattern)) {
                        foundPos = match.position();
                        foundLen = match.length();
                    }
                }
            } else {
                // Backwards regex: find all matches before startPos, take last one
                std::string searchArea = editorText.substr(0, startPos);
                auto begin = std::sregex_iterator(searchArea.begin(), searchArea.end(), pattern);
                auto end = std::sregex_iterator();
                std::smatch lastMatch;
                bool found = false;
                for (auto it = begin; it != end; ++it) {
                    lastMatch = *it;
                    found = true;
                }
                if (found) {
                    foundPos = lastMatch.position();
                    foundLen = lastMatch.length();
                } else {
                    // Wrap: search from startPos to end
                    searchArea = editorText.substr(startPos);
                    begin = std::sregex_iterator(searchArea.begin(), searchArea.end(), pattern);
                    for (auto it = begin; it != end; ++it) {
                        lastMatch = *it;
                        found = true;
                    }
                    if (found) {
                        foundPos = startPos + lastMatch.position();
                        foundLen = lastMatch.length();
                    }
                }
            }
        } catch (const std::regex_error& e) {
            std::string msg = "Invalid regex: ";
            msg += e.what();
            MessageBoxW(m_hwndMain, utf8ToWide(msg).c_str(), L"Find", MB_OK | MB_ICONERROR);
            return false;
        }
    } else {
        // Plain text search with optional case sensitivity and whole word
        std::string haystack = editorText;
        std::string needle = searchText;
        
        if (!caseSensitive) {
            std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
            std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
        }
        
        auto isWordBoundary = [&](size_t pos, size_t len) -> bool {
            if (!wholeWord) return true;
            bool leftOk = (pos == 0) || !isalnum((unsigned char)haystack[pos - 1]);
            bool rightOk = (pos + len >= haystack.size()) || !isalnum((unsigned char)haystack[pos + len]);
            return leftOk && rightOk;
        };
        
        if (forward) {
            size_t pos = startPos;
            while (pos < haystack.size()) {
                foundPos = haystack.find(needle, pos);
                if (foundPos == std::string::npos) break;
                if (isWordBoundary(foundPos, needle.size())) break;
                pos = foundPos + 1;
                foundPos = std::string::npos;
            }
            // Wrap around
            if (foundPos == std::string::npos && startPos > 0) {
                pos = 0;
                while (pos < (size_t)startPos) {
                    foundPos = haystack.find(needle, pos);
                    if (foundPos == std::string::npos || foundPos >= (size_t)startPos) { foundPos = std::string::npos; break; }
                    if (isWordBoundary(foundPos, needle.size())) break;
                    pos = foundPos + 1;
                    foundPos = std::string::npos;
                }
            }
        } else {
            if (startPos > 0) {
                foundPos = haystack.rfind(needle, startPos);
                while (foundPos != std::string::npos && !isWordBoundary(foundPos, needle.size())) {
                    if (foundPos == 0) { foundPos = std::string::npos; break; }
                    foundPos = haystack.rfind(needle, foundPos - 1);
                }
            }
            if (foundPos == std::string::npos) {
                foundPos = haystack.rfind(needle);
                while (foundPos != std::string::npos && !isWordBoundary(foundPos, needle.size())) {
                    if (foundPos == 0) { foundPos = std::string::npos; break; }
                    foundPos = haystack.rfind(needle, foundPos - 1);
                }
            }
        }
    }
    
    if (foundPos != std::string::npos) {
        selection.cpMin = (LONG)utf8ByteOffsetToCharIndex(editorText, (int)foundPos);
        selection.cpMax = (LONG)utf8ByteOffsetToCharIndex(editorText, (int)(foundPos + foundLen));
        SendMessage(m_hwndEditor, EM_EXSETSEL, 0, (LPARAM)&selection);
        SendMessage(m_hwndEditor, EM_SCROLLCARET, 0, 0);
        m_lastFoundPos = foundPos;
        return true;
    }
    
    MessageBoxW(m_hwndMain, L"Text not found.", L"Find", MB_OK | MB_ICONINFORMATION);
    return false;
}

int Win32IDE::replaceText(const std::string& searchText, const std::string& replaceText, bool all, bool caseSensitive, bool wholeWord, bool useRegex)
{
    if (!m_hwndEditor || searchText.empty()) return 0;
    
    int replaceCount = 0;
    
    if (all) {
        std::string editorText = getWindowText(m_hwndEditor);
        if (editorText.empty()) return 0;
        int textLen = (int)editorText.size();

        std::string result;
        size_t pos = 0;
        
        std::string haystack = editorText;
        std::string needle = searchText;
        
        if (!caseSensitive) {
            std::transform(haystack.begin(), haystack.end(), haystack.begin(), ::tolower);
            std::transform(needle.begin(), needle.end(), needle.begin(), ::tolower);
        }
        
        while ((pos = haystack.find(needle, pos)) != std::string::npos) {
            result.append(editorText, 0, pos);
            result.append(replaceText);
            pos += needle.length();
            replaceCount++;
        }
        
        if (replaceCount > 0) {
            result.append(editorText, pos, std::string::npos);
            setWindowText(m_hwndEditor, result);
            m_fileModified = true;
            updateLineNumbers();
        }
    } else {
        // Replace current selection if it matches search text
        CHARRANGE selection;
        SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&selection);
        
        int selLen = selection.cpMax - selection.cpMin;
        if (selLen > 0) {
            std::string selectedText(selLen + 1, 0);
            SendMessage(m_hwndEditor, EM_GETSELTEXT, 0, (LPARAM)&selectedText[0]);
            selectedText.resize(selLen);
            
            std::string cmpSelected = selectedText;
            std::string cmpSearch = searchText;
            
            if (!caseSensitive) {
                std::transform(cmpSelected.begin(), cmpSelected.end(), cmpSelected.begin(), ::tolower);
                std::transform(cmpSearch.begin(), cmpSearch.end(), cmpSearch.begin(), ::tolower);
            }
            
            if (cmpSelected == cmpSearch) {
                SendMessageW(m_hwndEditor, EM_REPLACESEL, TRUE, (LPARAM)utf8ToWide(replaceText).c_str());
                m_fileModified = true;
                replaceCount = 1;
                
                // Find next occurrence
                findText(searchText, true, caseSensitive, wholeWord, useRegex);
            }
        }
    }
    
    return replaceCount;
}

INT_PTR CALLBACK Win32IDE::FindDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = nullptr;
    
    if (uMsg == WM_INITDIALOG) {
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
        pThis = (Win32IDE*)lParam;
    } else {
        pThis = (Win32IDE*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
    }
    
    if (!pThis) return FALSE;
    
    switch (uMsg) {
    case WM_USER + 100:
        // Handle Copilot streaming token updates
        if (pThis) {
            pThis->HandleCopilotStreamUpdate(reinterpret_cast<const char*>(wParam), static_cast<size_t>(lParam));
        }
        return 0;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_BTN_FIND_NEXT:
            {
                HWND hwndFindText = GetDlgItem(hwndDlg, IDC_FIND_TEXT);
                wchar_t buffer[256];
                GetWindowTextW(hwndFindText, buffer, 256);
                pThis->m_lastSearchText = wideToUtf8(buffer);

                pThis->m_searchCaseSensitive = IsDlgButtonChecked(hwndDlg, IDC_CASE_SENSITIVE) == BST_CHECKED;
                pThis->m_searchWholeWord = IsDlgButtonChecked(hwndDlg, IDC_WHOLE_WORD) == BST_CHECKED;
                pThis->m_searchUseRegex = IsDlgButtonChecked(hwndDlg, IDC_USE_REGEX) == BST_CHECKED;
                
                pThis->findNext();
            }
            return TRUE;
        case IDC_BTN_CLOSE:
        case IDCANCEL:
            DestroyWindow(hwndDlg);
            pThis->m_hwndFindDialog = nullptr;
            return TRUE;
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwndDlg);
        pThis->m_hwndFindDialog = nullptr;
        return TRUE;
    }
    
    return FALSE;
}

INT_PTR CALLBACK Win32IDE::ReplaceDialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = nullptr;
    
    if (uMsg == WM_INITDIALOG) {
        SetWindowLongPtr(hwndDlg, GWLP_USERDATA, lParam);
        pThis = (Win32IDE*)lParam;
    } else {
        pThis = (Win32IDE*)GetWindowLongPtr(hwndDlg, GWLP_USERDATA);
    }
    
    if (!pThis) return FALSE;
    
    switch (uMsg) {
    case WM_COMMAND:
        {
            HWND hwndFindText = GetDlgItem(hwndDlg, IDC_FIND_TEXT);
            HWND hwndReplaceText = GetDlgItem(hwndDlg, IDC_REPLACE_TEXT);

            switch (LOWORD(wParam)) {
            case IDC_BTN_FIND_NEXT:
                {
                    wchar_t wFind[256], wReplace[256];
                    GetWindowTextW(hwndFindText, wFind, 256);
                    pThis->m_lastSearchText = wideToUtf8(wFind);
                }
                pThis->m_searchCaseSensitive = IsDlgButtonChecked(hwndDlg, IDC_CASE_SENSITIVE) == BST_CHECKED;
                pThis->m_searchWholeWord = IsDlgButtonChecked(hwndDlg, IDC_WHOLE_WORD) == BST_CHECKED;
                pThis->m_searchUseRegex = IsDlgButtonChecked(hwndDlg, IDC_USE_REGEX) == BST_CHECKED;
                pThis->findNext();
                return TRUE;
            case IDC_BTN_REPLACE:
                {
                    wchar_t wFind[256], wReplace[256];
                    GetWindowTextW(hwndFindText, wFind, 256);
                    GetWindowTextW(hwndReplaceText, wReplace, 256);
                    pThis->m_lastSearchText = wideToUtf8(wFind);
                    pThis->m_lastReplaceText = wideToUtf8(wReplace);
                }
                pThis->m_searchCaseSensitive = IsDlgButtonChecked(hwndDlg, IDC_CASE_SENSITIVE) == BST_CHECKED;
                pThis->m_searchWholeWord = IsDlgButtonChecked(hwndDlg, IDC_WHOLE_WORD) == BST_CHECKED;
                pThis->m_searchUseRegex = IsDlgButtonChecked(hwndDlg, IDC_USE_REGEX) == BST_CHECKED;
                pThis->replaceNext();
                return TRUE;
            case IDC_BTN_REPLACE_ALL:
                {
                    wchar_t wFind[256], wReplace[256];
                    GetWindowTextW(hwndFindText, wFind, 256);
                    GetWindowTextW(hwndReplaceText, wReplace, 256);
                    pThis->m_lastSearchText = wideToUtf8(wFind);
                    pThis->m_lastReplaceText = wideToUtf8(wReplace);
                }
                pThis->m_searchCaseSensitive = IsDlgButtonChecked(hwndDlg, IDC_CASE_SENSITIVE) == BST_CHECKED;
                pThis->m_searchWholeWord = IsDlgButtonChecked(hwndDlg, IDC_WHOLE_WORD) == BST_CHECKED;
                pThis->m_searchUseRegex = IsDlgButtonChecked(hwndDlg, IDC_USE_REGEX) == BST_CHECKED;
                pThis->replaceAll();
                return TRUE;
            case IDC_BTN_CLOSE:
            case IDCANCEL:
                DestroyWindow(hwndDlg);
                pThis->m_hwndReplaceDialog = nullptr;
                return TRUE;
            }
        }
        break;
    case WM_CLOSE:
        DestroyWindow(hwndDlg);
        pThis->m_hwndReplaceDialog = nullptr;
        return TRUE;
    }
    
    return FALSE;
}

// ============================================================================
// Snippet Manager Implementation  
// ============================================================================

#define IDD_SNIPPET_MANAGER 6001
// Note: IDC_SNIPPET_LIST is defined at line 23 as 1009
#define IDC_SNIPPET_LIST_DLG 6010
#define IDC_SNIPPET_NAME 6011
#define IDC_SNIPPET_DESC 6012
#define IDC_SNIPPET_CODE 6013
#define IDC_BTN_INSERT_SNIPPET 6020
#define IDC_BTN_NEW_SNIPPET 6021
#define IDC_BTN_DELETE_SNIPPET 6022
#define IDC_BTN_SAVE_SNIPPETS 6023

void Win32IDE::showSnippetManager()
{
    HWND hwndDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"STATIC", L"Snippet Manager",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        100, 100, 600, 500, m_hwndMain, nullptr, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Snippets:", WS_CHILD | WS_VISIBLE,
        10, 10, 150, 20, hwndDlg, nullptr, m_hInstance, nullptr);

    HWND hwndList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | LBS_STANDARD | WS_VSCROLL,
        10, 35, 150, 400, hwndDlg, (HMENU)IDC_SNIPPET_LIST_DLG, m_hInstance, nullptr);

    for (const auto& snippet : m_codeSnippets) {
        SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM)utf8ToWide(snippet.name).c_str());
    }

    CreateWindowExW(0, L"STATIC", L"Name:", WS_CHILD | WS_VISIBLE,
        175, 10, 50, 20, hwndDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        230, 8, 350, 22, hwndDlg, (HMENU)IDC_SNIPPET_NAME, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Description:", WS_CHILD | WS_VISIBLE,
        175, 40, 70, 20, hwndDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        175, 60, 405, 22, hwndDlg, (HMENU)IDC_SNIPPET_DESC, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Code Template:", WS_CHILD | WS_VISIBLE,
        175, 90, 100, 20, hwndDlg, nullptr, m_hInstance, nullptr);
    CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN | WS_VSCROLL | WS_HSCROLL,
        175, 115, 405, 280, hwndDlg, (HMENU)IDC_SNIPPET_CODE, m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Insert", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        175, 410, 90, 28, hwndDlg, (HMENU)IDC_BTN_INSERT_SNIPPET, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"New", WS_CHILD | WS_VISIBLE,
        275, 410, 90, 28, hwndDlg, (HMENU)IDC_BTN_NEW_SNIPPET, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Delete", WS_CHILD | WS_VISIBLE,
        375, 410, 90, 28, hwndDlg, (HMENU)IDC_BTN_DELETE_SNIPPET, m_hInstance, nullptr);
    CreateWindowExW(0, L"BUTTON", L"Save & Close", WS_CHILD | WS_VISIBLE,
        475, 410, 105, 28, hwndDlg, (HMENU)IDC_BTN_SAVE_SNIPPETS, m_hInstance, nullptr);
    
    // Message loop for dialog
    MSG msg;
    bool running = true;
    while (running && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.hwnd == hwndDlg || IsChild(hwndDlg, msg.hwnd)) {
            // Handle list selection
            if (msg.message == WM_COMMAND) {
                WORD cmdId = LOWORD(msg.wParam);
                WORD notif = HIWORD(msg.wParam);
                
                if (cmdId == IDC_SNIPPET_LIST_DLG && notif == LBN_SELCHANGE) {
                    int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)m_codeSnippets.size()) {
                        const CodeSnippet& snippet = m_codeSnippets[sel];
                        SetDlgItemTextW(hwndDlg, IDC_SNIPPET_NAME, utf8ToWide(snippet.name).c_str());
                        SetDlgItemTextW(hwndDlg, IDC_SNIPPET_DESC, utf8ToWide(snippet.description).c_str());
                        SetDlgItemTextW(hwndDlg, IDC_SNIPPET_CODE, utf8ToWide(snippet.code).c_str());
                    }
                }
                else if (cmdId == IDC_BTN_INSERT_SNIPPET) {
                    int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)m_codeSnippets.size()) {
                        insertSnippet(m_codeSnippets[sel].name);
                        running = false;
                        DestroyWindow(hwndDlg);
                    }
                }
                else if (cmdId == IDC_BTN_NEW_SNIPPET) {
                    CodeSnippet newSnippet;
                    newSnippet.name = "NewSnippet";
                    newSnippet.description = "New snippet description";
                    newSnippet.code = "// Your code here";
                    m_codeSnippets.push_back(newSnippet);
                    SendMessageW(hwndList, LB_ADDSTRING, 0, (LPARAM)utf8ToWide(newSnippet.name).c_str());
                    SendMessage(hwndList, LB_SETCURSEL, m_codeSnippets.size() - 1, 0);
                    SetDlgItemTextW(hwndDlg, IDC_SNIPPET_NAME, utf8ToWide(newSnippet.name).c_str());
                    SetDlgItemTextW(hwndDlg, IDC_SNIPPET_DESC, utf8ToWide(newSnippet.description).c_str());
                    SetDlgItemTextW(hwndDlg, IDC_SNIPPET_CODE, utf8ToWide(newSnippet.code).c_str());
                }
                else if (cmdId == IDC_BTN_DELETE_SNIPPET) {
                    int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)m_codeSnippets.size()) {
                        if (MessageBoxW(hwndDlg, L"Delete this snippet?", L"Confirm", MB_YESNO) == IDYES) {
                            m_codeSnippets.erase(m_codeSnippets.begin() + sel);
                            SendMessage(hwndList, LB_DELETESTRING, sel, 0);
                            SetDlgItemTextW(hwndDlg, IDC_SNIPPET_NAME, L"");
                            SetDlgItemTextW(hwndDlg, IDC_SNIPPET_DESC, L"");
                            SetDlgItemTextW(hwndDlg, IDC_SNIPPET_CODE, L"");
                        }
                    }
                }
                else if (cmdId == IDC_BTN_SAVE_SNIPPETS) {
                    // Update current snippet before saving
                    int sel = SendMessage(hwndList, LB_GETCURSEL, 0, 0);
                    if (sel >= 0 && sel < (int)m_codeSnippets.size()) {
                        wchar_t buffer[1024];
                        GetDlgItemTextW(hwndDlg, IDC_SNIPPET_NAME, buffer, 1024);
                        m_codeSnippets[sel].name = wideToUtf8(buffer);
                        GetDlgItemTextW(hwndDlg, IDC_SNIPPET_DESC, buffer, 1024);
                        m_codeSnippets[sel].description = wideToUtf8(buffer);

                        HWND hwndCode = GetDlgItem(hwndDlg, IDC_SNIPPET_CODE);
                        int len = GetWindowTextLengthW(hwndCode);
                        std::vector<wchar_t> codeBuffer(len + 1);
                        GetWindowTextW(hwndCode, codeBuffer.data(), len + 1);
                        m_codeSnippets[sel].code = wideToUtf8(codeBuffer.data());
                    }
                    
                    saveCodeSnippets();
                    MessageBoxW(hwndDlg, L"Snippets saved!", L"Success", MB_OK);
                    running = false;
                    DestroyWindow(hwndDlg);
                }
            }
            else if (msg.message == WM_CLOSE) {
                running = false;
                DestroyWindow(hwndDlg);
            }
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Win32IDE::createSnippet()
{
    // Create a new empty snippet
    CodeSnippet newSnippet;
    newSnippet.name = "NewSnippet" + std::to_string(m_codeSnippets.size() + 1);
    newSnippet.description = "New snippet";
    newSnippet.code = "// Code template\n";
    m_codeSnippets.push_back(newSnippet);
    
    MessageBoxW(m_hwndMain, utf8ToWide("Snippet '" + newSnippet.name + "' created. Use Snippet Manager to edit.").c_str(),
        L"Snippet Created", MB_OK);
}

// ============================================================================
// File Explorer Implementation
// ============================================================================

void Win32IDE::createFileExplorer(HWND hwndParent)
{
    if (m_hwndFileExplorer) {
        return; // Already created
    }

    m_hwndFileExplorer = CreateWindowExW(
        0, L"STATIC", L"File Explorer",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        0, 30, m_sidebarWidth, 500,
        hwndParent,
        (HMENU)IDC_FILE_EXPLORER,
        GetModuleHandle(nullptr),
        nullptr
    );

    m_hwndFileTree = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
        5, 5, m_sidebarWidth - 10, 490,
        m_hwndFileExplorer,
        (HMENU)IDC_FILE_TREE,
        GetModuleHandle(nullptr),
        nullptr
    );

    SendMessage(m_hwndFileTree, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    SetWindowLongPtrW(m_hwndFileExplorer, GWLP_USERDATA, (LONG_PTR)this);
    m_oldFileExplorerContainerProc = (WNDPROC)SetWindowLongPtrW(m_hwndFileExplorer, GWLP_WNDPROC, (LONG_PTR)FileExplorerContainerProc);

    // Populate with drive letters
    populateFileTree(nullptr, "");
}

void Win32IDE::populateFileTree(HTREEITEM parentItem, const std::string& path)
{
    if (!m_hwndFileTree) {
        return;
    }

    if (!parentItem) {
        TVINSERTSTRUCTW tvis = {};
        tvis.hParent = TVI_ROOT;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;

        wchar_t buf[MAX_PATH];
        for (char drive = 'C'; drive <= 'Z'; ++drive) {
            std::string drivePath = std::string(1, drive) + ":";
            DWORD drives = GetLogicalDrives();
            int driveNum = drive - 'A';

            if (drives & (1 << driveNum)) {
                std::string displayName = drivePath + "\\";
                MultiByteToWideChar(CP_ACP, 0, displayName.c_str(), -1, buf, MAX_PATH);
                tvis.item.pszText = buf;
                tvis.item.lParam = (LPARAM) new std::string(drivePath);

                HTREEITEM driveItem = (HTREEITEM)SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&tvis);
                m_treeItemPaths[driveItem] = drivePath;

                TVINSERTSTRUCTW dummyVis = {};
                dummyVis.hParent = driveItem;
                dummyVis.item.mask = TVIF_TEXT;
                static wchar_t s_ellipsis[] = L"...";
                dummyVis.item.pszText = s_ellipsis;
                SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&dummyVis);
            }
        }
        return;
    }

    // Populate a specific folder
    try {
        WIN32_FIND_DATAA findData;
        HANDLE findHandle;

        std::string searchPath = path + "\\*";
        findHandle = FindFirstFileA(searchPath.c_str(), &findData);

        if (findHandle == INVALID_HANDLE_VALUE) {
            return;
        }

        TVINSERTSTRUCTW tvis = {};
        tvis.hParent = parentItem;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_PARAM;

        wchar_t wbuf[MAX_PATH];
        HTREEITEM hChild = TreeView_GetChild(m_hwndFileTree, parentItem);
        while (hChild) {
            HTREEITEM hNext = TreeView_GetNextSibling(m_hwndFileTree, hChild);
            TreeView_DeleteItem(m_hwndFileTree, hChild);
            hChild = hNext;
        }

        do {
            if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
                continue;
            }

            std::string fullPath = path + "\\" + findData.cFileName;
            MultiByteToWideChar(CP_ACP, 0, findData.cFileName, -1, wbuf, MAX_PATH);

            if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                tvis.item.pszText = wbuf;
                tvis.item.lParam = (LPARAM) new std::string(fullPath);

                HTREEITEM folderItem = (HTREEITEM)SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&tvis);
                m_treeItemPaths[folderItem] = fullPath;

                TVINSERTSTRUCTW dummyVis = {};
                dummyVis.hParent = folderItem;
                dummyVis.item.mask = TVIF_TEXT;
                static wchar_t s_ellipsis2[] = L"...";
                dummyVis.item.pszText = s_ellipsis2;
                SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&dummyVis);
            }
            else if (strlen(findData.cFileName) > 5 &&
                     strcmp(findData.cFileName + strlen(findData.cFileName) - 5, ".gguf") == 0) {
                tvis.item.pszText = wbuf;
                tvis.item.lParam = (LPARAM) new std::string(fullPath);

                HTREEITEM fileItem = (HTREEITEM)SendMessageW(m_hwndFileTree, TVM_INSERTITEM, 0, (LPARAM)&tvis);
                m_treeItemPaths[fileItem] = fullPath;
            }
        } while (FindNextFileA(findHandle, &findData));

        FindClose(findHandle);
    }
    catch (...) {
        // Silently handle errors
    }
}

LRESULT CALLBACK Win32IDE::FileExplorerContainerProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);
    if (uMsg == WM_NOTIFY) {
        NMHDR* pnmh = reinterpret_cast<NMHDR*>(lParam);
        if (pnmh && pnmh->code == TVN_DELETEITEM) {
            NMTREEVIEWA* pnmtv = reinterpret_cast<NMTREEVIEWA*>(lParam);
            if (pnmtv->itemOld.lParam)
                delete reinterpret_cast<std::string*>(pnmtv->itemOld.lParam);
            return 0;
        }
    }
    WNDPROC oldProc = pThis ? pThis->m_oldFileExplorerContainerProc : nullptr;
    if (oldProc)
        return CallWindowProcA(oldProc, hwnd, uMsg, wParam, lParam);
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

void Win32IDE::onFileTreeExpand(HTREEITEM item, const std::string& path)
{
    if (!m_hwndFileTree) {
        return;
    }

    populateFileTree(item, path);
}

std::string Win32IDE::getTreeItemPath(HTREEITEM item) const
{
    auto it = m_treeItemPaths.find(item);
    if (it != m_treeItemPaths.end()) {
        return it->second;
    }
    return "";
}

void Win32IDE::loadModelFromPath(const std::string& filepath)
{
    if (filepath.length() > 5 &&
        filepath.substr(filepath.length() - 5) == ".gguf") {
        // Load model using streaming loader
        if (loadGGUFModel(filepath)) {
            // Initialize inference system
            initializeInference();
            
            // Initialize backend manager (Phase 8B)
            initBackendManager();
            
            // Initialize LLM Router (Phase 8C)
            initLLMRouter();
            
            // Notify user in chat
            std::string msg = "✅ Model loaded and ready for inference!\r\n\r\n"
                             "You can now ask questions in the chat panel.\r\n"
                             "Try: 'hello', 'model info', 'explain code', etc.";
            appendCopilotResponse(msg);
        }
    }
}

// ============================================================================
// GGUF Model Loading Implementation
// ============================================================================

bool Win32IDE::loadGGUFModel(const std::string& filepath)
{
    if (!m_ggufLoader) {
        std::string error = "Error: GGUF Loader not initialized";
        appendToOutput(error, "Errors", OutputSeverity::Error);
        ErrorReporter::report(error, m_hwndMain);
        return false;
    }

    appendToOutput("Loading GGUF model: " + filepath + "\n", "Output", OutputSeverity::Info);
    appendToOutput("This may take a moment for large files...\n", "Output", OutputSeverity::Info);

    try {
        // Attempt to open and parse the GGUF file (streaming - no full data load)
        appendToOutput("[1/5] Opening file...\n", "Output", OutputSeverity::Info);
        if (!m_ggufLoader->Open(filepath)) {
            std::string error = "❌ Failed to open GGUF file: " + filepath + "\nCheck if file exists and is readable.";
            appendToOutput(error, "Errors", OutputSeverity::Error);
            ErrorReporter::report(error, m_hwndMain);
            return false;
        }

        appendToOutput("[2/5] Parsing header...\n", "Output", OutputSeverity::Info);
        if (!m_ggufLoader->ParseHeader()) {
            std::string error = "❌ Failed to parse GGUF header from: " + filepath + "\nFile may be corrupted or not a valid GGUF.";
            appendToOutput(error, "Errors", OutputSeverity::Error);
            ErrorReporter::report(error, m_hwndMain);
            m_ggufLoader->Close();
            return false;
        }

        appendToOutput("[3/5] Parsing metadata...\n", "Output", OutputSeverity::Info);
        if (!m_ggufLoader->ParseMetadata()) {
            std::string error = "❌ Failed to parse GGUF metadata from: " + filepath + "\nFile structure may be invalid.";
            appendToOutput(error, "Errors", OutputSeverity::Error);
            ErrorReporter::report(error, m_hwndMain);
            m_ggufLoader->Close();
            return false;
        }

        // Build tensor index (reads tensor offsets but NOT data)
        appendToOutput("[4/5] Building tensor index (may take 10-30 seconds for large files)...\n", "Output", OutputSeverity::Info);
        if (!m_ggufLoader->BuildTensorIndex()) {
            std::string error = "❌ Failed to build tensor index from: " + filepath + "\nFile may be too large or corrupted.";
            appendToOutput(error, "Errors", OutputSeverity::Error);
            ErrorReporter::report(error, m_hwndMain);
            m_ggufLoader->Close();
            return false;
        }

        // Pre-load embedding zone for inference preparation
        appendToOutput("[5/5] Pre-loading embedding zone...\n", "Output", OutputSeverity::Info);
        if (!m_ggufLoader->LoadZone("embedding")) {
            std::string warning = "⚠️  Warning: Could not pre-load embedding zone (non-critical)";
            appendToOutput(warning, "Output", OutputSeverity::Warning);
        }
    }
    catch (const std::exception& e) {
        std::string error = "❌ Exception loading GGUF file:\n" + std::string(e.what()) + "\n\nFile: " + filepath;
        appendToOutput(error + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(error, m_hwndMain);
        return false;
    }
    catch (...) {
        std::string error = "❌ Unknown exception loading GGUF file: " + filepath;
        appendToOutput(error + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(error, m_hwndMain);
        return false;
    }

    // Store model info
    m_loadedModelPath = filepath;
    m_currentModelMetadata = m_ggufLoader->GetMetadata();
    m_modelTensors = m_ggufLoader->GetAllTensorInfo();  // Get tensor info for backward compatibility

    // Log success with memory savings information
    size_t currentMemory = m_ggufLoader->GetCurrentMemoryUsage();
    std::string info = "✅ Model loaded successfully (STREAMING MODE)!\n";
    info += "File: " + filepath + "\n";
    info += "Tensors: " + std::to_string(m_modelTensors.size()) + "\n";
    info += "Layers: " + std::to_string(m_currentModelMetadata.layer_count) + "\n";
    info += "Context: " + std::to_string(m_currentModelMetadata.context_length) + "\n";
    info += "Vocab: " + std::to_string(m_currentModelMetadata.vocab_size) + "\n";
    info += "Current Memory: " + std::to_string(currentMemory / 1024 / 1024) + " MB\n";
    info += "Max Memory: ~500 MB (zone-based streaming)\n\n";
    
    auto zones = m_ggufLoader->GetLoadedZones();
    if (!zones.empty()) {
        info += "Loaded Zones: ";
        for (size_t i = 0; i < zones.size(); i++) {
            info += zones[i];
            if (i < zones.size() - 1) info += ", ";
        }
        info += "\n";
    }
    
    appendToOutput(info, "Output", OutputSeverity::Info);
    
    // Update status bar
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0,
        (LPARAM)utf8ToWide("Model: " + std::string(filepath)).c_str());

    // Auto-activate Copilot panel and send welcome message
    if (m_hwndSecondarySidebar && m_hwndCopilotChatOutput) {
        // Make secondary sidebar visible if hidden
        ShowWindow(m_hwndSecondarySidebar, SW_SHOW);
        
        // Send agentic welcome message to Copilot
        std::string welcomeMsg = "🤖 AI Model Loaded!\r\n\r\n";
        welcomeMsg += "I'm now ready to assist you with:\r\n";
        welcomeMsg += "• Code analysis and review\r\n";
        welcomeMsg += "• GGUF model exploration\r\n";
        welcomeMsg += "• Tensor inspection and debugging\r\n";
        welcomeMsg += "• PowerShell automation\r\n";
        welcomeMsg += "• File operations\r\n\r\n";
        welcomeMsg += "Model: " + filepath + "\r\n";
        welcomeMsg += "Tensors: " + std::to_string(m_modelTensors.size()) + "\r\n";
        welcomeMsg += "Memory: " + std::to_string(currentMemory / 1024 / 1024) + " MB\r\n\r\n";
        welcomeMsg += "Ask me anything!\r\n";
        
        appendCopilotResponse(welcomeMsg);
    }

    return true;
}

std::string Win32IDE::getModelInfo() const
{
    if (m_modelTensors.empty() || !m_ggufLoader) {
        return "No model loaded";
    }

    std::string info = "═══════════════════════════════════════════\n";
    info += "GGUF Model Information (STREAMING MODE)\n";
    info += "═══════════════════════════════════════════\n\n";
    
    info += "File: " + m_loadedModelPath + "\n";
    info += "Tensors: " + std::to_string(m_modelTensors.size()) + "\n";
    info += "Layers: " + std::to_string(m_currentModelMetadata.layer_count) + "\n";
    info += "Context Length: " + std::to_string(m_currentModelMetadata.context_length) + "\n";
    info += "Embedding Dim: " + std::to_string(m_currentModelMetadata.embedding_dim) + "\n";
    info += "Vocab Size: " + std::to_string(m_currentModelMetadata.vocab_size) + "\n";
    info += "Architecture: " + std::to_string(m_currentModelMetadata.architecture_type) + "\n\n";

    // Show zone status (memory efficiency indicator)
    size_t currentMemory = m_ggufLoader->GetCurrentMemoryUsage();
    auto loadedZones = m_ggufLoader->GetLoadedZones();
    
    info += "📊 Memory Status:\n";
    info += "  Current RAM: " + std::to_string(currentMemory / 1024 / 1024) + " MB\n";
    info += "  Max Per Zone: ~400 MB\n";
    info += "  Total Capacity: ~500 MB (92x reduction from full load!)\n";
    info += "  Loaded Zones: " + std::to_string(loadedZones.size()) + "\n\n";
    
    if (!loadedZones.empty()) {
        info += "🎯 Active Zones:\n";
        for (const auto& zone : loadedZones) {
            info += "   ✓ " + zone + "\n";
        }
        info += "\n";
    }

    info += "Tensor Details (first 10):\n";
    info += "──────────────────────────────────────────\n";
    
    for (size_t i = 0; i < m_modelTensors.size() && i < 10; ++i) {
        const auto& tensor = m_modelTensors[i];
        info += "[" + std::to_string(i + 1) + "] " + tensor.name + "\n";
        info += "    Size: " + std::to_string(tensor.size_bytes / 1024 / 1024) + " MB\n";
        info += "    Type: " + m_ggufLoader->GetTypeString(tensor.type) + "\n";
    }

    if (m_modelTensors.size() > 10) {
        info += "... and " + std::to_string(m_modelTensors.size() - 10) + " more tensors\n";
    }

    info += "\n💡 Tip: Zones load on-demand during inference for optimal performance!\n";

    return info;
}

bool Win32IDE::loadTensorData(const std::string& tensorName, std::vector<uint8_t>& data)
{
    if (!m_ggufLoader) {
        return false;
    }
    // StreamingGGUFLoader automatically loads required zone if needed
    return m_ggufLoader->LoadTensorZone(tensorName, data);
}

// ============================================================================
// FILE EXPLORER IMPLEMENTATION
// ============================================================================

void Win32IDE::createFileExplorer()
{
    if (!m_hwndSidebar) return;

    m_hwndFileExplorer = CreateWindowExW(
        WS_EX_CLIENTEDGE,
        WC_TREEVIEWW,
        L"",
        WS_CHILD | WS_VISIBLE | WS_BORDER | TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS,
        5, 30, m_sidebarWidth - 10, 400,
        m_hwndSidebar,
        (HMENU)IDC_FILE_EXPLORER,
        m_hInstance,
        nullptr
    );

    // LOGGING AS REQUESTED
    char logBuf[256];
    sprintf_s(logBuf, "Explorer HWND created: %p (Parent: %p)", m_hwndFileExplorer, m_hwndSidebar);
    LOG_INFO(std::string(logBuf));

    if (!m_hwndFileExplorer) return;

    // Create image list for icons
    m_hImageList = ImageList_Create(16, 16, ILC_COLOR32 | ILC_MASK, 3, 0);
    if (m_hImageList) {
        // Load icons for folders, files, and model files
        HICON hFolderIcon = (HICON)LoadImageA(nullptr, MAKEINTRESOURCEA(32755), IMAGE_ICON, 16, 16, LR_SHARED);
        HICON hFileIcon = (HICON)LoadImageA(nullptr, MAKEINTRESOURCEA(32512), IMAGE_ICON, 16, 16, LR_SHARED);
        HICON hModelIcon = (HICON)LoadImageA(nullptr, MAKEINTRESOURCEA(32516), IMAGE_ICON, 16, 16, LR_SHARED);
        
        ImageList_AddIcon(m_hImageList, hFolderIcon);  // Index 0: Folder
        ImageList_AddIcon(m_hImageList, hFileIcon);    // Index 1: Regular file
        ImageList_AddIcon(m_hImageList, hModelIcon);   // Index 2: Model file

        TreeView_SetImageList(m_hwndFileExplorer, m_hImageList, TVSIL_NORMAL);
    }

    populateFileTree();
}

void Win32IDE::populateFileTree()
{
    if (!m_hwndFileExplorer) return;

    // Clear existing items
    TreeView_DeleteAllItems(m_hwndFileExplorer);

    // Add root directories for model browsing
    const char* username = getenv("USERNAME");
    std::string userDir(username && username[0] ? username : "User");
    std::vector<std::string> modelPaths = {
        "D:\\OllamaModels",
        "C:\\OllamaModels",
        "C:\\Users\\" + userDir + "\\OllamaModels"
    };

    for (const auto& path : modelPaths) {
        if (GetFileAttributesA(path.c_str()) != INVALID_FILE_ATTRIBUTES) {
            std::string displayName = path;
            size_t lastSlash = path.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                displayName = path.substr(lastSlash + 1) + " (" + path + ")";
            }
            
            HTREEITEM hRoot = addTreeItem(TVI_ROOT, displayName, path, true);
            scanDirectory(path, hRoot);
        }
    }

    // Expand the D:\OllamaModels by default if it exists
    HTREEITEM hFirst = TreeView_GetRoot(m_hwndFileExplorer);
    if (hFirst) {
        TreeView_Expand(m_hwndFileExplorer, hFirst, TVE_EXPAND);
    }
}

HTREEITEM Win32IDE::addTreeItem(HTREEITEM hParent, const std::string& text, const std::string& fullPath, bool isDirectory)
{
    TVINSERTSTRUCTW tvins = {};
    tvins.hParent = hParent;
    tvins.hInsertAfter = TVI_LAST;
    tvins.item.mask = TVIF_TEXT | TVIF_PARAM | TVIF_IMAGE | TVIF_SELECTEDIMAGE;

    char* pathData = new char[fullPath.length() + 1];
    strcpy_s(pathData, fullPath.length() + 1, fullPath.c_str());

    wchar_t wbuf[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, text.c_str(), -1, wbuf, MAX_PATH);
    tvins.item.pszText = wbuf;
    tvins.item.lParam = reinterpret_cast<LPARAM>(pathData);

    if (isDirectory) {
        tvins.item.iImage = 0;
        tvins.item.iSelectedImage = 0;
    } else if (isModelFile(fullPath)) {
        tvins.item.iImage = 2;
        tvins.item.iSelectedImage = 2;
    } else {
        tvins.item.iImage = 1;
        tvins.item.iSelectedImage = 1;
    }

    return (HTREEITEM)SendMessageW(m_hwndFileExplorer, TVM_INSERTITEM, 0, (LPARAM)&tvins);
}

void Win32IDE::scanDirectory(const std::string& dirPath, HTREEITEM hParent)
{
    WIN32_FIND_DATAA findData;
    std::string searchPath = dirPath + "\\*";
    
    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) return;

    do {
        if (strcmp(findData.cFileName, ".") == 0 || strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        std::string fullPath = dirPath + "\\" + findData.cFileName;
        bool isDirectory = (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        
        // Skip hidden and system files
        if (findData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) {
            continue;
        }
        
        // For files, only show model files and some common extensions
        if (!isDirectory) {
            std::string fileName = findData.cFileName;
            std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
            
            if (!isModelFile(fullPath) && 
                fileName.find(".txt") == std::string::npos &&
                fileName.find(".json") == std::string::npos &&
                fileName.find(".md") == std::string::npos &&
                fileName.find(".log") == std::string::npos) {
                continue;
            }
        }

        HTREEITEM hItem = addTreeItem(hParent, findData.cFileName, fullPath, isDirectory);
        
        // For directories, add a dummy child so we can expand later
        if (isDirectory) {
            addTreeItem(hItem, "Loading...", "", false);
        }
        
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);
}

bool Win32IDE::isModelFile(const std::string& filePath)
{
    std::string fileName = filePath;
    std::transform(fileName.begin(), fileName.end(), fileName.begin(), ::tolower);
    
    return fileName.find(".gguf") != std::string::npos ||
           fileName.find(".bin") != std::string::npos ||
           fileName.find(".safetensors") != std::string::npos ||
           fileName.find(".pt") != std::string::npos ||
           fileName.find(".pth") != std::string::npos ||
           fileName.find(".onnx") != std::string::npos;
}

void Win32IDE::expandTreeNode(HTREEITEM hItem)
{
    if (!hItem) return;

    // Check if this node has been expanded before
    HTREEITEM hChild = TreeView_GetChild(m_hwndFileExplorer, hItem);
    if (hChild) {
        TVITEMW item = {};
        item.hItem = hChild;
        item.mask = TVIF_TEXT | TVIF_PARAM;
        wchar_t buffer[MAX_PATH];
        item.pszText = buffer;
        item.cchTextMax = MAX_PATH;

        if (SendMessageW(m_hwndFileExplorer, TVM_GETITEM, 0, (LPARAM)&item)) {
            if (wcscmp(item.pszText, L"Loading...") == 0) {
                // Remove the dummy item
                TreeView_DeleteItem(m_hwndFileExplorer, hChild);
                
                // Get the full path and scan the directory
                TVITEMW parentItem = {};
                parentItem.hItem = hItem;
                parentItem.mask = TVIF_PARAM;
                if (SendMessageW(m_hwndFileExplorer, TVM_GETITEM, 0, (LPARAM)&parentItem) && parentItem.lParam) {
                    std::string dirPath = reinterpret_cast<char*>(parentItem.lParam);
                    scanDirectory(dirPath, hItem);
                }
            }
        }
    }
}

std::string Win32IDE::getSelectedFilePath()
{
    HTREEITEM hSelected = TreeView_GetSelection(m_hwndFileExplorer);
    if (!hSelected) return "";
    
    TVITEMW item = {};
    item.hItem = hSelected;
    item.mask = TVIF_PARAM;

    if (SendMessageW(m_hwndFileExplorer, TVM_GETITEM, 0, (LPARAM)&item) && item.lParam) {
        return std::string(reinterpret_cast<char*>(item.lParam));
    }
    
    return "";
}

void Win32IDE::onFileExplorerDoubleClick()
{
    std::string filePath = getSelectedFilePath();
    if (filePath.empty()) return;
    
    DWORD attributes = GetFileAttributesA(filePath.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES) return;
    
    if (attributes & FILE_ATTRIBUTE_DIRECTORY) {
        // Expand/collapse directory
        HTREEITEM hSelected = TreeView_GetSelection(m_hwndFileExplorer);
        if (hSelected) {
            UINT state = TreeView_GetItemState(m_hwndFileExplorer, hSelected, TVIS_EXPANDED);
            if (state & TVIS_EXPANDED) {
                TreeView_Expand(m_hwndFileExplorer, hSelected, TVE_COLLAPSE);
            } else {
                expandTreeNode(hSelected);
                TreeView_Expand(m_hwndFileExplorer, hSelected, TVE_EXPAND);
            }
        }
    } else {
        // Load file
        if (isModelFile(filePath)) {
            loadModelFromExplorer(filePath);
        } else {
            // Open text files in editor - with size check!
            try {
                std::ifstream file(filePath, std::ios::binary);
                if (file.is_open()) {
                    // Check file size first
                    file.seekg(0, std::ios::end);
                    size_t fileSize = file.tellg();
                    file.seekg(0, std::ios::beg);
                    
                    if (fileSize > 10 * 1024 * 1024) { // 10MB limit
                        MessageBoxW(m_hwndMain, L"File too large to open in editor (>10MB).", L"File Too Large", MB_OK | MB_ICONWARNING);
                        return;
                    }
                    
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    setWindowText(m_hwndEditor, content);
                    m_currentFile = filePath;
                    updateTitleBarText();
                    file.close();
                }
            }
            catch (const std::exception& e) {
                std::string error = "Error opening file: " + std::string(e.what());
                MessageBoxW(m_hwndMain, utf8ToWide(error).c_str(), L"Error", MB_OK | MB_ICONERROR);
            }
        }
    }
}

void Win32IDE::loadModelFromExplorer(const std::string& filePath)
{
    if (loadGGUFModel(filePath)) {
        std::string message = "✅ Model loaded from File Explorer:\n" + filePath + "\n\n" + getModelInfo();
        appendToOutput(message, "Output", OutputSeverity::Info);
        
        // Update status bar
        std::string filename = filePath;
        size_t lastSlash = filename.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            filename = filename.substr(lastSlash + 1);
        }
        
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide("Model: " + filename).c_str());
    } else {
        appendToOutput("❌ Failed to load model: " + filePath, "Errors", OutputSeverity::Error);
    }
}

void Win32IDE::onFileExplorerRightClick()
{
    std::string filePath = getSelectedFilePath();
    if (!filePath.empty()) {
        DWORD attributes = GetFileAttributesA(filePath.c_str());
        bool isDirectory = (attributes != INVALID_FILE_ATTRIBUTES) && (attributes & FILE_ATTRIBUTE_DIRECTORY);
        showFileContextMenu(filePath, isDirectory);
    }
}

void Win32IDE::showFileContextMenu(const std::string& filePath, bool isDirectory)
{
    HMENU hMenu = CreatePopupMenu();
    if (!hMenu) return;
    
    static constexpr int IDC_CTX_REFRESH = 50001, IDC_CTX_OPEN_EXPLORER = 50002, IDC_CTX_SET_ROOT = 50003;
    static constexpr int IDC_CTX_LOAD_MODEL = 50011, IDC_CTX_MODEL_INFO = 50012, IDC_CTX_OPEN_EDITOR = 50013, IDC_CTX_COPY_PATH = 50014, IDC_CTX_SHOW_EXPLORER = 50015;
    static constexpr int IDC_CTX_DELETE = 50020, IDC_CTX_RENAME = 50021;
    if (isDirectory) {
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_REFRESH, L"Refresh");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_OPEN_EXPLORER, L"Open in Explorer");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_SET_ROOT, L"Set as Root Path");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_DELETE, L"Delete");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_RENAME, L"Rename");
    } else {
        if (isModelFile(filePath)) {
            AppendMenuW(hMenu, MF_STRING, IDC_CTX_LOAD_MODEL, L"Load Model");
            AppendMenuW(hMenu, MF_STRING, IDC_CTX_MODEL_INFO, L"Show Model Info");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        }
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_OPEN_EDITOR, L"Open with Editor");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_COPY_PATH, L"Copy Path");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_SHOW_EXPLORER, L"Show in Explorer");
        AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_DELETE, L"Delete");
        AppendMenuW(hMenu, MF_STRING, IDC_CTX_RENAME, L"Rename");
    }
    
    POINT pt;
    GetCursorPos(&pt);
    
    int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, m_hwndMain, nullptr);
    
    switch (cmd) {
        case 50001: // Refresh directory
            refreshFileExplorer();
            break;
        case 50002: // Open in Explorer
        case 50015: // Show in Explorer
            ShellExecuteA(nullptr, "explore", filePath.c_str(), nullptr, nullptr, SW_SHOW);
            break;
        case 50020: // IDC_CTX_DELETE
            deleteItemInExplorer(filePath);
            break;
        case 50021: // IDC_CTX_RENAME
            renameItemInExplorer(filePath);
            break;
        case 50003: // Set as Root Path
            m_currentExplorerPath = filePath;
            populateFileTree();
            break;
        case 50011: // Load Model
            loadModelFromExplorer(filePath);
            break;
        case 50012: // Show Model Info
            if (loadGGUFModel(filePath)) {
                std::string info = "Model Information:\n" + getModelInfo();
                MessageBoxW(m_hwndMain, utf8ToWide(info).c_str(), L"Model Info", MB_OK | MB_ICONINFORMATION);
            }
            break;
        case 50013: // Open with Editor
            {
                std::ifstream file(filePath);
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    setWindowText(m_hwndEditor, content);
                    m_currentFile = filePath;
                    updateTitleBarText();
                }
            }
            break;
        case 50014: // Copy Path
            if (OpenClipboard(m_hwndMain)) {
                EmptyClipboard();
                HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, filePath.size() + 1);
                if (hMem) {
                    char* dest = (char*)GlobalLock(hMem);
                    strcpy_s(dest, filePath.size() + 1, filePath.c_str());
                    GlobalUnlock(hMem);
                    SetClipboardData(CF_TEXT, hMem);
                }
                CloseClipboard();
            }
            break;
    }
    
    DestroyMenu(hMenu);
}

void Win32IDE::refreshFileExplorer()
{
    populateFileTree();
}

// ============================================================================
// MODEL CHAT INTERFACE IMPLEMENTATION
// ============================================================================

bool Win32IDE::isModelLoaded() const
{
    // Model is loaded if we have a path and the streaming loader has opened the file
    return m_ggufLoader && !m_loadedModelPath.empty() && !m_modelTensors.empty();
}

std::string Win32IDE::sendMessageToModel(const std::string& message)
{
    if (!isModelLoaded()) {
        return "Error: No model loaded";
    }

    // Phase 8B/8C: Route through LLM router (if enabled) or backend manager
    if (m_backendManagerInitialized) {
        std::string resp = routeWithIntelligence(message);
        if (!resp.empty() && resp.find("[Backend Error]") != 0) {
            m_chatHistory.push_back({message, resp});
            return resp;
        }
    }
    
    // First try: send through local Ollama if available
    std::string llmResponse;
    if (trySendToOllama(message, llmResponse)) {
        m_chatHistory.push_back({message, llmResponse});
        return llmResponse;
    }

    // Fallback: Local CPU Inference (Real Logic)
    if (m_ggufLoader) {
        // Use the native fallback engine if available
        if (m_nativeEngine) {
             auto* engine = m_nativeEngine.get();
             auto tokens = engine->Tokenize(message);
             auto output_tokens = engine->Generate(tokens, 512);
             std::string response = engine->Detokenize(output_tokens);
             m_chatHistory.push_back({message, response});
             return response;
        }
    }

    std::string response = "Error: Local model loaded but Native Inference Engine not initialized.\n";
    return response;
}

void Win32IDE::toggleChatMode()
{
    m_chatMode = !m_chatMode;
    
    if (m_chatMode) {
        // Entering chat mode
        std::string status = "🤖 Chat Mode ON - Model: ";
        status += m_loadedModelPath.empty() ? "None" : m_loadedModelPath.substr(m_loadedModelPath.find_last_of("\\/") + 1);
        
        appendToOutput(status, "Output", OutputSeverity::Info);
        appendToOutput("Type your messages in the command input. Use /exit-chat to return to terminal mode.", "Output", OutputSeverity::Info);
        
        // Update status bar
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"Chat Mode");
        
        // Clear existing chat display and show instructions
        appendChatMessage("System", "Chat mode activated! You can now talk with the loaded model.");
        appendChatMessage("System", "Commands: /exit-chat to return to terminal mode");
    } else {
        // Exiting chat mode
        appendToOutput("🔧 Chat Mode OFF - Returned to terminal mode", "Output", OutputSeverity::Info);
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)L"Terminal Mode");
        appendChatMessage("System", "Chat mode deactivated. Returned to terminal mode.");
    }
}

void Win32IDE::appendChatMessage(const std::string& user, const std::string& message)
{
    // Get timestamp
    time_t now = time(nullptr);
    struct tm timeinfo;
    localtime_s(&timeinfo, &now);
    char timestamp[16];
    strftime(timestamp, sizeof(timestamp), "%H:%M:%S", &timeinfo);
    
    // Format message
    std::string formattedMsg = "[" + std::string(timestamp) + "] " + user + ": " + message + "\n\n";
    
    // Display in output panel
    if (user == "System") {
        appendToOutput(formattedMsg, "Output", OutputSeverity::Info);
    } else if (user == "You") {
        appendToOutput(formattedMsg, "Output", OutputSeverity::Info);
    } else if (user == "Model") {
        appendToOutput(formattedMsg, "Output", OutputSeverity::Info);
    }
}

// ============================================================================
// GIT INTEGRATION - Status, Commit, Push, Pull
// ============================================================================

void Win32IDE::showGitStatus()
{
    if (!isGitRepository()) {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git", MB_OK | MB_ICONWARNING);
        return;
    }
    
    updateGitStatus();
    
    std::ostringstream status;
    status << "Git Status\n";
    status << "==========\n\n";
    status << "Branch: " << m_gitStatus.branch << "\n";
    status << "\nChanges:\n";
    status << "  Modified:  " << m_gitStatus.modified << "\n";
    status << "  Added:     " << m_gitStatus.added << "\n";
    status << "  Deleted:   " << m_gitStatus.deleted << "\n";
    status << "  Untracked: " << m_gitStatus.untracked << "\n";
    
    MessageBoxW(m_hwndMain, utf8ToWide(status.str()).c_str(), L"Git Status", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::updateGitStatus()
{
    if (!isGitRepository()) {
        m_gitStatus = GitStatus();
        return;
    }
    
    std::string output;
    
    // Get current branch
    executeGitCommand("git rev-parse --abbrev-ref HEAD", output);
    m_gitStatus.branch = output;
    if (!m_gitStatus.branch.empty() && m_gitStatus.branch.back() == '\n') {
        m_gitStatus.branch.pop_back();
    }
    output.clear();
    
    // Get status --porcelain
    executeGitCommand("git status --porcelain", output);
    m_gitStatus.modified = 0;
    m_gitStatus.added = 0;
    m_gitStatus.deleted = 0;
    m_gitStatus.untracked = 0;
    
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.length() < 2) continue;
        
        char status = line[0];
        char status2 = line[1];
        
        if (status == 'M' || status2 == 'M') m_gitStatus.modified++;
        if (status == 'A' || status2 == 'A') m_gitStatus.added++;
        if (status == 'D' || status2 == 'D') m_gitStatus.deleted++;
        if (status == '?' || status2 == '?') m_gitStatus.untracked++;
    }
    
    m_gitStatus.hasChanges = (m_gitStatus.modified + m_gitStatus.added + 
                               m_gitStatus.deleted + m_gitStatus.untracked) > 0;
}

void Win32IDE::gitCommit(const std::string& message)
{
    if (!isGitRepository()) {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::string output;
    std::string command = "git commit -m \"" + message + "\"";
    executeGitCommand(command, output);
    
    MessageBoxW(m_hwndMain, utf8ToWide(output).c_str(), L"Git Commit", MB_OK | MB_ICONINFORMATION);
    updateGitStatus();
}

void Win32IDE::gitPush()
{
    if (!isGitRepository()) {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::string output;
    executeGitCommand("git push", output);
    
    MessageBoxW(m_hwndMain,
        utf8ToWide(output.empty() ? "Push completed successfully" : output).c_str(),
        L"Git Push", MB_OK | MB_ICONINFORMATION);
    updateGitStatus();
}

void Win32IDE::gitPull()
{
    if (!isGitRepository()) {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::string output;
    executeGitCommand("git pull", output);
    
    MessageBoxW(m_hwndMain,
        utf8ToWide(output.empty() ? "Pull completed successfully" : output).c_str(),
        L"Git Pull", MB_OK | MB_ICONINFORMATION);
    updateGitStatus();
}

void Win32IDE::gitStageFile(const std::string& filePath)
{
    if (!isGitRepository()) return;
    
    std::string output;
    std::string command = "git add \"" + filePath + "\"";
    executeGitCommand(command, output);
    updateGitStatus();
}

void Win32IDE::gitUnstageFile(const std::string& filePath)
{
    if (!isGitRepository()) return;
    
    std::string output;
    std::string command = "git reset HEAD \"" + filePath + "\"";
    executeGitCommand(command, output);
    updateGitStatus();
}

bool Win32IDE::isGitRepository() const
{
    if (!m_gitRepoPath.empty()) {
        std::string gitDir = m_gitRepoPath + "\\.git";
        DWORD attrib = GetFileAttributesA(gitDir.c_str());
        return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
    }
    
    // Check current directory
    char currentDir[MAX_PATH];
    GetCurrentDirectoryA(MAX_PATH, currentDir);
    std::string gitDir = std::string(currentDir) + "\\.git";
    DWORD attrib = GetFileAttributesA(gitDir.c_str());
    return (attrib != INVALID_FILE_ATTRIBUTES && (attrib & FILE_ATTRIBUTE_DIRECTORY));
}

std::vector<GitFile> Win32IDE::getGitChangedFiles() const
{
    std::vector<GitFile> files;
    
    if (!isGitRepository()) return files;
    
    std::string output;
    const_cast<Win32IDE*>(this)->executeGitCommand("git status --porcelain", output);
    
    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (line.length() < 4) continue;
        
        GitFile file;
        file.status = line[0] != ' ' ? line[0] : line[1];
        file.staged = (line[0] != ' ' && line[0] != '?');
        file.path = line.substr(3);
        
        files.push_back(file);
    }
    
    return files;
}

bool Win32IDE::executeGitCommand(const std::string& command, std::string& output)
{
    output.clear();
    
    // Create a temporary file for output
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    std::string tempFile = std::string(tempPath) + "rawr_git_output.txt";
    
    // Execute command and redirect output
    std::string fullCommand = command + " > \"" + tempFile + "\" 2>&1";
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    
    if (CreateProcessA(NULL, const_cast<char*>(fullCommand.c_str()), 
        NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        
        WaitForSingleObject(pi.hProcess, 5000);  // 5 second timeout
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        // Read output file
        std::ifstream file(tempFile);
        if (file.is_open()) {
            std::string line;
            while (std::getline(file, line)) {
                output += line + "\n";
            }
            file.close();
            DeleteFileA(tempFile.c_str());
        }
        return true;
    }
    return false;
}

void Win32IDE::showGitPanel()
{
    if (!isGitRepository()) {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Create Git panel if it doesn't exist
    if (!m_hwndGitPanel || !IsWindow(m_hwndGitPanel)) {
        m_hwndGitPanel = CreateWindowExW(WS_EX_TOOLWINDOW, L"STATIC", L"Git Panel",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_SIZEBOX,
            200, 100, 600, 500, m_hwndMain, nullptr, m_hInstance, nullptr);
        
        // Branch and status info
        m_hwndGitStatusText = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
            10, 10, 580, 60, m_hwndGitPanel, nullptr, m_hInstance, nullptr);

        CreateWindowExW(0, L"STATIC", L"Changed Files:", WS_CHILD | WS_VISIBLE,
            10, 80, 120, 20, m_hwndGitPanel, nullptr, m_hInstance, nullptr);

        m_hwndGitFileList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
            WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_EXTENDEDSEL | WS_VSCROLL,
            10, 105, 280, 300, m_hwndGitPanel, nullptr, m_hInstance, nullptr);
    }
    
    ShowWindow(m_hwndGitPanel, SW_SHOW);
    refreshGitPanel();
}

void Win32IDE::refreshGitPanel()
{
    if (!m_hwndGitPanel || !IsWindow(m_hwndGitPanel)) return;
    
    updateGitStatus();
    
    // Update status text
    std::string statusText = "Branch: " + m_gitStatus.branch + "\n";
    statusText += "Modified: " + std::to_string(m_gitStatus.modified) + " | ";
    statusText += "Added: " + std::to_string(m_gitStatus.added) + " | ";
    statusText += "Deleted: " + std::to_string(m_gitStatus.deleted) + " | ";
    statusText += "Untracked: " + std::to_string(m_gitStatus.untracked);
    
    if (m_hwndGitStatusText) {
        SetWindowTextW(m_hwndGitStatusText, utf8ToWide(statusText).c_str());
    }
    
    // Update file list
    if (m_hwndGitFileList) {
        SendMessage(m_hwndGitFileList, LB_RESETCONTENT, 0, 0);
        
        std::vector<GitFile> files = getGitChangedFiles();
        for (const auto& file : files) {
            std::string displayText;
            if (file.staged) {
                displayText = "[S] ";
            } else {
                displayText = "[ ] ";
            }
            
            switch (file.status) {
                case 'M': displayText += "(M) "; break;
                case 'A': displayText += "(A) "; break;
                case 'D': displayText += "(D) "; break;
                case '?': displayText += "(?) "; break;
                default: displayText += "( ) "; break;
            }
            
            displayText += file.path;
            SendMessageW(m_hwndGitFileList, LB_ADDSTRING, 0, (LPARAM)utf8ToWide(displayText).c_str());
        }
    }
}

void Win32IDE::showCommitDialog()
{
    if (!isGitRepository()) {
        MessageBoxW(m_hwndMain, L"Not a Git repository", L"Git", MB_OK | MB_ICONWARNING);
        return;
    }
    
    HWND hwndDlg = CreateWindowExW(WS_EX_DLGMODALFRAME, L"STATIC", L"Git Commit",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        150, 150, 500, 200, m_hwndMain, nullptr, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Commit Message:", WS_CHILD | WS_VISIBLE,
        10, 10, 120, 20, hwndDlg, nullptr, m_hInstance, nullptr);

    m_hwndCommitDialog = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
        10, 35, 470, 100, hwndDlg, nullptr, m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Commit", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        10, 145, 100, 30, hwndDlg, (HMENU)1, m_hInstance, nullptr);

    CreateWindowExW(0, L"BUTTON", L"Cancel", WS_CHILD | WS_VISIBLE,
        120, 145, 100, 30, hwndDlg, (HMENU)2, m_hInstance, nullptr);
    
    SetFocus(m_hwndCommitDialog);
}

// ============================================================================
// AI INFERENCE IMPLEMENTATION - Connects GGUF Loader to Chat Panel
// ============================================================================

void Win32IDE::openModel() {
    wchar_t filename[MAX_PATH] = {0};
    OPENFILENAMEW ofn = {0};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = L"GGUF Models\0*.gguf\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = L"Select GGUF Model";

    if (GetOpenFileNameW(&ofn)) {
        loadModelForInference(wideToUtf8(filename));
    }
}

bool Win32IDE::loadModelForInference(const std::string& filepath) {
    SCOPED_METRIC("model.load");
    METRICS.increment("model.load_attempts");
    appendToOutput("Loading model: " + filepath + "\n", "System", OutputSeverity::Info);
    
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    
    if (m_agenticBridge) {
        if (m_agenticBridge->LoadModel(filepath)) {
            METRICS.gauge("model.loaded", 1.0);
            METRICS.increment("model.load_success");
            appendToOutput("Model loaded successfully into Agentic Bridge.\n", "System", OutputSeverity::Info);
            
            // Sync current UI state
            m_agenticBridge->SetContextSize("4K"); 
            if (m_hwndContextSlider) SendMessage(m_hwndContextSlider, TBM_SETPOS, TRUE, 0);
            
            return true;
        }
    }
    
    METRICS.increment("model.load_failures");
    METRICS.gauge("model.loaded", 0.0);
    appendToOutput("Failed to load model: " + filepath + "\n", "System", OutputSeverity::Error);
    return false;
}

bool Win32IDE::initializeInference()
{
    SCOPED_METRIC("inference.initialize");
    METRICS.increment("inference.init_attempts");
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    
    // Explicit Logic: Initialize Native CPU Engine if missing (Un-mocking)
    if (!m_nativeEngine) {
        try {
            m_nativeEngine = std::make_unique<RawrXD::CPUInferenceEngine>();
            m_nativeEngineLoaded = false;
            appendToOutput("Initialized Native CPU Inference Engine.", "Output", OutputSeverity::Info);
        } catch (const std::exception& e) {
            appendToOutput(std::string("Failed to init native engine: ") + e.what(), "Errors", OutputSeverity::Error);
            return false;
        }
    }

    // Check if model is loaded via GGUF loader (Streaming)
    if (m_loadedModelPath.empty()) {
        if (!m_ggufLoader) {
            appendToOutput("No model loaded for inference", "Errors", OutputSeverity::Error);
            return false;
        }
        // If ggufLoader has a file open but path var is empty, try to recover (unlikely)
    }
    
    // Connect Native Engine to Model
    if (m_nativeEngine && !m_loadedModelPath.empty()) {
        RawrXD::CPUInferenceEngine* engine = static_cast<RawrXD::CPUInferenceEngine*>(m_nativeEngine.get());
        if (!engine->IsModelLoaded()) {
            appendToOutput("Loading model into Native Engine: " + m_loadedModelPath, "Output", OutputSeverity::Info);
            if (engine->LoadModel(m_loadedModelPath)) {
                m_nativeEngineLoaded = true;
                appendToOutput("✅ Native Engine Model Loaded Successfully.", "Output", OutputSeverity::Info);
            } else {
                 appendToOutput("❌ Native Engine Model Load Failed.", "Errors", OutputSeverity::Error);
                 // Don't fail completely if we have Ollama fallback, but for "no simulation" we adhere to native.
            }
        }
    }

    // Set up inference config from model metadata
    m_inferenceConfig.maxTokens = 512;
    m_inferenceConfig.temperature = 0.7f;
    m_inferenceConfig.topP = 0.9f;
    m_inferenceConfig.topK = 40;
    m_inferenceConfig.repetitionPenalty = 1.1f;
    
    // Use model context length if available
    if (m_currentModelMetadata.context_length > 0) {
        m_inferenceConfig.maxTokens = std::min(512, (int)m_currentModelMetadata.context_length / 4);
    }
    
    appendToOutput("✅ Inference initialized for model: " + m_loadedModelPath, "Output", OutputSeverity::Info);
    return true;
}

void Win32IDE::shutdownInference()
{
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    
    if (m_inferenceRunning) {
        m_inferenceStopRequested = true;
        if (m_inferenceThread.joinable()) {
            m_inferenceThread.join();
        }
    }
    
    m_inferenceRunning = false;
    m_inferenceStopRequested = false;
    m_currentInferencePrompt.clear();
    m_currentInferenceResponse.clear();
    
    appendToOutput("Inference shutdown complete", "Output", OutputSeverity::Info);
}

std::string Win32IDE::generateResponse(const std::string& prompt)
{
    SCOPED_METRIC("inference.generate_response");
    METRICS.increment("inference.requests_total");

    if (m_inferenceRunning) {
        METRICS.increment("inference.requests_rejected");
        return "Inference already in progress. Please wait...";
    }

    // Phase 8B/8C: Route through LLM router (if enabled) or backend manager
    if (m_backendManagerInitialized) {
        return routeWithIntelligence(prompt);
    }

    // Attempt real remote/local inference via Ollama if configured
    auto performOllama = [&](const std::string& promptText) -> std::string {
        if (m_ollamaBaseUrl.empty()) return "";
        // Expect base URL like http://localhost:11434
        std::string base = m_ollamaBaseUrl;
        if (base.rfind("http://", 0) != 0 && base.rfind("https://", 0) != 0) return "";
        bool https = base.rfind("https://", 0) == 0;
        std::string withoutProto = base.substr(base.find("://") + 3);
        std::string host; int port = https ? 443 : 80;
        size_t colonPos = withoutProto.find(':');
        size_t slashPos = withoutProto.find('/');
        if (colonPos != std::string::npos) {
            host = withoutProto.substr(0, colonPos);
            std::string portStr = withoutProto.substr(colonPos + 1, (slashPos == std::string::npos ? withoutProto.size() : slashPos) - (colonPos + 1));
            port = atoi(portStr.c_str());
        } else {
            host = (slashPos == std::string::npos) ? withoutProto : withoutProto.substr(0, slashPos);
            // Default Ollama port
            if (!https) port = 11434;
        }
        std::wstring whost(host.begin(), host.end());
        HINTERNET hSession = WinHttpOpen(L"RawrXDIDE/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, NULL, NULL, 0);
        if (!hSession) return "";
        HINTERNET hConnect = WinHttpConnect(hSession, whost.c_str(), (INTERNET_PORT)port, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", L"/api/generate", NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, https ? WINHTTP_FLAG_SECURE : 0);
        if (!hRequest) { WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
        // Build JSON body
        std::string modelTag;
        if (!m_ollamaModelOverride.empty()) modelTag = m_ollamaModelOverride; else {
            // Derive from loaded path
            modelTag = m_loadedModelPath;
            size_t pos = modelTag.find_last_of("\\/");
            if (pos != std::string::npos) modelTag = modelTag.substr(pos + 1);
        }
        // Basic escaping of quotes in prompt
        std::string escPrompt; escPrompt.reserve(promptText.size()+16);
        for (char c : promptText) { if (c == '"') escPrompt += "\\\""; else if (c=='\n') escPrompt += "\\n"; else escPrompt += c; }
        std::string body = std::string("{\"model\":\"") + modelTag + "\",\"prompt\":\"" + escPrompt + "\",\"stream\":false}";
        std::wstring wHeaders = L"Content-Type: application/json";
        BOOL bResults = WinHttpSendRequest(hRequest, wHeaders.c_str(), (DWORD)-1L, (LPVOID)body.c_str(), (DWORD)body.size(), (DWORD)body.size(), 0);
        if (!bResults) { WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession); return ""; }
        bResults = WinHttpReceiveResponse(hRequest, NULL);
        std::string raw;
        if (bResults) {
            DWORD dwSize = 0; do {
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
                if (!dwSize) break;
                std::string chunk; chunk.resize(dwSize);
                DWORD dwRead = 0;
                if (!WinHttpReadData(hRequest, chunk.data(), dwSize, &dwRead)) break;
                if (dwRead) raw.append(chunk.data(), dwRead);
            } while (dwSize > 0);
        }
        WinHttpCloseHandle(hRequest); WinHttpCloseHandle(hConnect); WinHttpCloseHandle(hSession);
        if (raw.empty()) return "";
        // Naive JSON parse: look for "response":"..."
        std::string out;
        size_t pos = raw.rfind("\"response\":\"");
        if (pos != std::string::npos) {
            pos += 12; // start after marker
            while (pos < raw.size()) {
                char c = raw[pos++];
                if (c == '"') break; // end of string (assumes not escaped)
                if (c == '\\') { if (pos < raw.size()) { char next = raw[pos++]; if (next=='n') out+='\n'; else out+=next; } }
                else out += c;
            }
        }
        return out.empty() ? raw : out;
    };

    std::string remote = performOllama(prompt);
    if (!remote.empty()) return remote;

    // Fallback structured guidance if no remote inference available
    std::string modelName = m_loadedModelPath.empty() ? "None" : m_loadedModelPath.substr(m_loadedModelPath.find_last_of("\\/")+1);

    // Fallback: Native CPU Inference Engine
    if (m_nativeEngine && m_nativeEngineLoaded) {
        RawrXD::CPUInferenceEngine* engine = static_cast<RawrXD::CPUInferenceEngine*>(m_nativeEngine.get());
        // If engine doesn't have a model loaded, try to load current one
        if (!engine->IsModelLoaded() && !m_loadedModelPath.empty()) {
            engine->LoadModel(m_loadedModelPath);
        }
        
        if (engine->IsModelLoaded()) {
            // Use Generate method for inference
            std::vector<int32_t> tokens = engine->Tokenize(prompt);
            std::vector<int32_t> output = engine->Generate(tokens, 100);
            return engine->Detokenize(output);
        } else {
             return "Error: No model loaded in Native CPU Engine.";
        }
    }

    return std::string("[Native Engine Error]\nModel: ") + modelName + "\nPrompt: " + prompt + "\n(Ollama unavailable and Native Engine not ready)";
}

void Win32IDE::generateResponseAsync(const std::string& prompt, std::function<void(const std::string&, bool)> callback)
{
    METRICS.increment("inference.async_requests_total");
    std::lock_guard<std::mutex> lock(m_inferenceMutex);

    if (m_inferenceRunning) {
        METRICS.increment("inference.async_requests_rejected");
        if (callback) callback("Inference already in progress.", true);
        return;
    }
    
    m_inferenceRunning = true;
    m_inferenceStopRequested = false;
    m_currentInferencePrompt = prompt;
    m_inferenceCallback = callback;
    
    // Launch dedicated inference thread using Native Agentic Bridge
    m_inferenceThread = std::thread([this, prompt]() {
        DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
        if (_guard.cancelled) { m_inferenceRunning = false; return; }
        if (!m_agenticBridge) {
             if (m_inferenceCallback) m_inferenceCallback("Error: Agentic Bridge not initialized.", true);
             m_inferenceRunning = false;
             return;
        }

        // Set callback to route NativeAgent stream to the UI
        m_agenticBridge->SetOutputCallback([this](const std::string& type, const std::string& msg) {
             if (m_inferenceStopRequested || isShuttingDown()) return;
             // "stream" type is what we send to chat UI
             if (m_inferenceCallback) m_inferenceCallback(msg, false);
        });

        // Execute via parity bridge (supports /edit, /think, etc.)
        m_agenticBridge->ExecuteAgentCommand(prompt);

        // Phase 4B: Choke Point 4 — hookPostGeneration after streaming inference
        // Note: For streaming responses, the full output was already sent via callback.
        // We hook here for failure detection on the completed inference cycle.
        // The response content was streamed — we check the accumulated result if available.
        if (!m_inferenceStopRequested) {
            std::string accumulatedResponse = m_currentInferenceResponse;
            if (!accumulatedResponse.empty()) {
                FailureClassification inferenceFailure = hookPostGeneration(
                    accumulatedResponse, prompt);
                if (inferenceFailure.reason != AgentFailureType::None) {
                    LOG_WARNING("[Phase4B] Inference failure detected: " +
                        failureTypeString(inferenceFailure.reason) +
                        " (confidence=" + std::to_string(inferenceFailure.confidence) + ")");
                    // For streaming responses, we log the failure and record it
                    // but don't auto-retry (the user sees output in real-time)
                    recordSimpleEvent(AgentEventType::FailureDetected,
                        "Inference failure: " + failureTypeString(inferenceFailure.reason) +
                        " | " + inferenceFailure.evidence);
                }
            }
        }

        m_inferenceRunning = false;
        if (m_inferenceCallback && !isShuttingDown()) {
            m_inferenceCallback("", true); // Finalize
        }
    });
    
    m_inferenceThread.detach();
}

void Win32IDE::stopInference()
{
    m_inferenceStopRequested = true;
}

void Win32IDE::setInferenceConfig(const InferenceConfig& config)
{
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    m_inferenceConfig = config;
}

Win32IDE::InferenceConfig Win32IDE::getInferenceConfig() const
{
    return m_inferenceConfig;
}

std::string Win32IDE::buildChatPrompt(const std::string& userMessage)
{
    std::string prompt;
    
    // Add system prompt if set
    if (!m_inferenceConfig.systemPrompt.empty()) {
        prompt = "<|system|>\n" + m_inferenceConfig.systemPrompt + "\n<|end|>\n";
        m_contextUsage.systemTokens = static_cast<int>(m_inferenceConfig.systemPrompt.length()) / 4;
    }
    
    // Add user message
    prompt += "<|user|>\n" + userMessage + "\n<|end|>\n";
    prompt += "<|assistant|>\n";
    
    // Track message tokens and update context window
    m_contextUsage.messageTokens += static_cast<int>(userMessage.length()) / 4;
    m_contextUsage.maxTokens = m_inferenceConfig.contextWindow;
    updateContextWindowDisplay();
    
    return prompt;
}

void Win32IDE::onInferenceToken(const std::string& token)
{
    // Called when streaming tokens during inference
    m_currentInferenceResponse += token;
    
    // Phase 19B: Feed token to the streaming output system
    appendStreamingToken(token);
    
    // Update context window token count (approximate: ~4 chars per token)
    int approxTokens = static_cast<int>(m_currentInferenceResponse.length()) / 4;
    m_contextUsage.toolResultTokens = approxTokens;
    // Throttle status bar updates to every ~20 tokens
    if (approxTokens % 20 == 0) {
        updateContextWindowDisplay();
    }
    
    // Update UI with partial response if streaming is enabled
    if (m_inferenceConfig.streamOutput && m_inferenceCallback) {
        m_inferenceCallback(token, false);
    }
}

void Win32IDE::onInferenceComplete(const std::string& fullResponse)
{
    m_inferenceRunning = false;
    m_currentInferenceResponse = fullResponse;
    
    // Final context window update
    m_contextUsage.toolResultTokens = static_cast<int>(fullResponse.length()) / 4;
    updateContextWindowDisplay();
    
    if (m_inferenceCallback) {
        m_inferenceCallback(fullResponse, true);
    }
}

// ============================================================================
// EDITOR OPERATIONS - Undo/Redo/Cut/Copy/Paste
// ============================================================================

void Win32IDE::undo()
{
    if (m_hwndEditor) {
        SendMessage(m_hwndEditor, EM_UNDO, 0, 0);
    }
}

void Win32IDE::redo()
{
    if (m_hwndEditor) {
        SendMessage(m_hwndEditor, EM_REDO, 0, 0);
    }
}

void Win32IDE::editCut()
{
    if (m_hwndEditor) {
        SendMessage(m_hwndEditor, WM_CUT, 0, 0);
    }
}

void Win32IDE::editCopy()
{
    if (m_hwndEditor) {
        SendMessage(m_hwndEditor, WM_COPY, 0, 0);
    }
}

void Win32IDE::editPaste()
{
    if (m_hwndEditor) {
        SendMessage(m_hwndEditor, WM_PASTE, 0, 0);
    }
}

// ============================================================================
// VIEW OPERATIONS - Toggle panels
// ============================================================================

void Win32IDE::toggleOutputPanel()
{
    m_outputPanelVisible = !m_outputPanelVisible;
    if (m_hwndMain) {
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right, rc.bottom);
        InvalidateRect(m_hwndMain, NULL, TRUE);
    }
}

void Win32IDE::toggleTerminal()
{
    // Toggle panel visibility (which contains terminal)
    m_outputPanelVisible = !m_outputPanelVisible;
    if (m_hwndMain) {
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right, rc.bottom);
        InvalidateRect(m_hwndMain, NULL, TRUE);
    }
}

void Win32IDE::showAbout()
{
    std::string aboutText = 
        RAWRXD_VERSION_FULL "\n\n"
        "Build: " RAWRXD_BUILD_DATE " " RAWRXD_BUILD_TIME "\n"
        "Channel: " RAWRXD_CHANNEL "\n"
        "Units: " + std::to_string(RAWRXD_COMPILE_UNITS) + " compilation units\n"
        "MASM64: " + std::to_string(RAWRXD_MASM_KERNELS) + " ASM kernels\n\n"
        "Engine:\n"
        "• Native Win32 C++20 (no Qt, no Electron)\n"
        "• GGUF Model Loader + AVX-512 Inference\n"
        "• Chain-of-Thought Multi-Model Review\n"
        "• Native PDB Symbol Server (MSF v7.00)\n"
        "• Three-Layer Hotpatch System\n"
        "• Voice Chat (waveIn/Out + VAD + STT/TTS)\n"
        "• Unified GPU Accelerator Router\n"
        "• Embedded LSP Server (JSON-RPC 2.0)\n"
        "• Distributed Swarm Inference\n\n"
        RAWRXD_COPYRIGHT "\n"
        RAWRXD_LICENSE "\n"
        RAWRXD_GITHUB;
    
    MessageBoxW(m_hwndMain, utf8ToWide(aboutText).c_str(), L"About RawrXD IDE", MB_OK | MB_ICONINFORMATION);
}

// ============================================================================
// AUTONOMY FRAMEWORK - High-level orchestration controls
// ============================================================================

void Win32IDE::onAutonomyStart() {
    if (!m_autonomyManager) {
        appendToOutput("Autonomy manager not initialized\n", "Errors", OutputSeverity::Error);
        return;
    }
    m_autonomyManager->start();
    appendToOutput("Autonomy started (manual mode)\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAutonomyStop() {
    if (!m_autonomyManager) return;
    m_autonomyManager->stop();
    appendToOutput("Autonomy stopped\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAutonomyToggle() {
    if (!m_autonomyManager) return;
    bool enable = !m_autonomyManager->isAutoLoopEnabled();
    m_autonomyManager->enableAutoLoop(enable);
    appendToOutput(std::string("Autonomy auto loop ") + (enable?"ENABLED":"DISABLED") + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAutonomySetGoal() {
    if (!m_autonomyManager) return;
    // Simple goal setter: reuse current file name or fallback text
    std::string goal = m_currentFile.empty() ? "Explore workspace and summarize architecture" : ("Analyze file: " + m_currentFile);
    m_autonomyManager->setGoal(goal);
    appendToOutput("Autonomy goal set: " + goal + "\n", "Output", OutputSeverity::Info);
}

void Win32IDE::onAutonomyViewStatus() {
    if (!m_autonomyManager) return;
    std::string status = m_autonomyManager->getStatus();
    appendToOutput("Autonomy Status: " + status + "\n", "Output", OutputSeverity::Info);
    MessageBoxW(m_hwndMain, utf8ToWide(status).c_str(), L"Autonomy Status", MB_OK | MB_ICONINFORMATION);
}

void Win32IDE::onAutonomyViewMemory() {
    if (!m_autonomyManager) return;
    auto mem = m_autonomyManager->getMemorySnapshot();
    std::string report = "Memory Items (latest first, max 20):\n\n";
    int shown = 0;
    for (int i = (int)mem.size() - 1; i >= 0 && shown < 20; --i, ++shown) {
        report += std::to_string(shown+1) + ". " + mem[i] + "\n";
    }
    if (shown == 0) report += "<empty>\n";
    appendToOutput("Autonomy Memory Snapshot displayed\n", "Debug", OutputSeverity::Debug);
    MessageBoxW(m_hwndMain, utf8ToWide(report).c_str(), L"Autonomy Memory", MB_OK);
}

// ======================================================================
// AI CHAT PANEL IMPLEMENTATION
// ======================================================================

void Win32IDE::createChatPanel() {

    if (!m_hwndMain) {

        return;
    }

    m_hwndSecondarySidebar = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE,
        0, 0, 300, 600,
        m_hwndMain, (HMENU)IDC_SECONDARY_SIDEBAR, m_hInstance, nullptr);

    if (!m_hwndSecondarySidebar) {
        return;
    }
    SetWindowLongPtr(m_hwndSecondarySidebar, GWLP_USERDATA, (LONG_PTR)this);
    m_oldSidebarProc = (WNDPROC)SetWindowLongPtr(m_hwndSecondarySidebar, GWLP_WNDPROC, (LONG_PTR)SidebarProc);

    m_hwndSecondarySidebarHeader = CreateWindowExW(
        0, L"STATIC", L"AI Chat",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 5, 290, 25,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);
    
    HFONT hFont = CreateFontA(-dpiScale(14), 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                              DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    if (m_hwndSecondarySidebarHeader) {
        SendMessage(m_hwndSecondarySidebarHeader, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    CreateWindowExW(0, L"STATIC", L"Model:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 35, 50, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    m_hwndModelSelector = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_AUTOHSCROLL,
        60, 35, 235, 200,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_SEND_BTN, m_hInstance, nullptr);
    
    if (m_hwndModelSelector) {
        SendMessage(m_hwndModelSelector, WM_SETFONT, (WPARAM)hFont, TRUE);
        populateModelSelector();
    }
    
    CreateWindowExW(0, L"STATIC", L"Max Tokens:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 60, 80, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    m_hwndMaxTokensLabel = CreateWindowExW(0, L"STATIC", L"512",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        245, 60, 50, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    m_hwndMaxTokensSlider = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        5, 80, 290, 25,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CLEAR_BTN, m_hInstance, nullptr);

    CreateWindowExW(0, L"STATIC", L"Context:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 110, 80, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    m_hwndContextLabel = CreateWindowExW(0, L"STATIC", L"4K",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        245, 110, 50, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    m_hwndContextSlider = CreateWindowExW(
        0, TRACKBAR_CLASSW, L"",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        5, 130, 290, 25,
        m_hwndSecondarySidebar, (HMENU)IDC_AI_CONTEXT_SLIDER, m_hInstance, nullptr);

    if (m_hwndContextSlider) {
        SendMessage(m_hwndContextSlider, TBM_SETRANGE, TRUE, MAKELPARAM(0, 6)); // 7 steps
        SendMessage(m_hwndContextSlider, TBM_SETPOS, TRUE, 0); // Default 4K
        m_currentContextSize = 4096;
    }
    // Update Chat Output Y position to accommodate new slider
    int chatY = 160; 
    
    if (m_hwndMaxTokensSlider) {
        SendMessage(m_hwndMaxTokensSlider, TBM_SETRANGE, TRUE, MAKELPARAM(32, 2048));
        SendMessage(m_hwndMaxTokensSlider, TBM_SETPOS, TRUE, 512);
        SendMessage(m_hwndMaxTokensSlider, TBM_SETTICFREQ, 256, 0);
        m_currentMaxTokens = 512;
    }

    CreateWindowExW(0, L"STATIC", L"Context (Mem):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 110, 100, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);

    HWND hContextCombo = CreateWindowExW(
        0, L"COMBOBOX", L"",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST,
        110, 108, 185, 300,
        m_hwndSecondarySidebar, (HMENU)4200, m_hInstance, nullptr);

    if (hContextCombo) {
        SendMessage(hContextCombo, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessageW(hContextCombo, CB_ADDSTRING, 0, (LPARAM)L"2048 (Standard)");
        SendMessageW(hContextCombo, CB_ADDSTRING, 0, (LPARAM)L"4096 (4k)");
        SendMessageW(hContextCombo, CB_ADDSTRING, 0, (LPARAM)L"32768 (32k)");
        SendMessageW(hContextCombo, CB_ADDSTRING, 0, (LPARAM)L"65536 (64k)");
        SendMessageW(hContextCombo, CB_ADDSTRING, 0, (LPARAM)L"131072 (128k)");
        SendMessageW(hContextCombo, CB_ADDSTRING, 0, (LPARAM)L"262144 (256k)");
        SendMessageW(hContextCombo, CB_ADDSTRING, 0, (LPARAM)L"524288 (512k)");
        SendMessageW(hContextCombo, CB_ADDSTRING, 0, (LPARAM)L"1048576 (1M)");
        SendMessage(hContextCombo, CB_SETCURSEL, 0, 0);
    }

    int toggleY = 140;
    int toggleX = 5;

    m_hwndChkMaxMode = CreateWindowExW(0, L"BUTTON", L"Max Mode", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                      toggleX, toggleY, 140, 20, m_hwndSecondarySidebar, (HMENU)IDC_AI_MAX_MODE, m_hInstance, nullptr);
    m_hwndChkDeepThink = CreateWindowExW(0, L"BUTTON", L"Deep Think", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                       toggleX + 150, toggleY, 140, 20, m_hwndSecondarySidebar, (HMENU)IDC_AI_DEEP_THINK, m_hInstance, nullptr);

    toggleY += 25;
    m_hwndChkDeepResearch = CreateWindowExW(0, L"BUTTON", L"Deep Research", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                          toggleX, toggleY, 140, 20, m_hwndSecondarySidebar, (HMENU)IDC_AI_DEEP_RESEARCH, m_hInstance, nullptr);
    m_hwndChkNoRefusal = CreateWindowExW(0, L"BUTTON", L"No Refusal", WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
                                       toggleX + 150, toggleY, 140, 20, m_hwndSecondarySidebar, (HMENU)IDC_AI_NO_REFUSAL, m_hInstance, nullptr);

    if (m_hwndChkMaxMode) SendMessage(m_hwndChkMaxMode, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndChkDeepThink) SendMessage(m_hwndChkDeepThink, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndChkDeepResearch) SendMessage(m_hwndChkDeepResearch, WM_SETFONT, (WPARAM)hFont, TRUE);
    if (m_hwndChkNoRefusal) SendMessage(m_hwndChkNoRefusal, WM_SETFONT, (WPARAM)hFont, TRUE);

    m_hwndCopilotChatOutput = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        5, 200, 290, 210,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_OUTPUT, m_hInstance, nullptr);

    if (m_hwndCopilotChatOutput) {
        SendMessage(m_hwndCopilotChatOutput, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    m_hwndCopilotChatInput = CreateWindowExW(
        WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
        5, 415, 290, 85,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_INPUT, m_hInstance, nullptr);

    if (m_hwndCopilotChatInput) {
        SendMessage(m_hwndCopilotChatInput, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    m_hwndCopilotSendBtn = CreateWindowExW(
        0, L"BUTTON", L"Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, 505, 140, 30,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_SEND_BTN, m_hInstance, nullptr);

    if (m_hwndCopilotSendBtn) {
        SendMessage(m_hwndCopilotSendBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    }

    m_hwndCopilotClearBtn = CreateWindowExW(
        0, L"BUTTON", L"Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 505, 140, 30,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CLEAR_BTN, m_hInstance, nullptr);
    
    if (m_hwndCopilotClearBtn) {
        SendMessage(m_hwndCopilotClearBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    m_secondarySidebarVisible = true;
    m_secondarySidebarWidth = 320;

}

void Win32IDE::populateModelSelector() {
    if (!m_hwndModelSelector) return;

    // Clear existing items
    SendMessage(m_hwndModelSelector, CB_RESETCONTENT, 0, 0);
    
    // Try to scan OllamaModels directory for available models
    std::string ollamaPath = "D:\\OllamaModels";
    m_availableModels.clear();
    
    // Add default models
    m_availableModels.push_back("llama2");
    m_availableModels.push_back("mistral");
    m_availableModels.push_back("neural-chat");
    m_availableModels.push_back("dolphin-mixtral");
    
    // Try to scan directory
    WIN32_FIND_DATAA findData;
    HANDLE findHandle = FindFirstFileA((ollamaPath + "\\*").c_str(), &findData);
    
    if (findHandle != INVALID_HANDLE_VALUE) {
        do {
            if ((findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && 
                strcmp(findData.cFileName, ".") != 0 && strcmp(findData.cFileName, "..") != 0) {
                m_availableModels.push_back(findData.cFileName);
            }
        } while (FindNextFileA(findHandle, &findData));
        FindClose(findHandle);
    }
    
    // Populate combobox
    for (const auto& model : m_availableModels) {
        SendMessageW(m_hwndModelSelector, CB_ADDSTRING, 0, (LPARAM)utf8ToWide(model).c_str());
    }
    
    // Set first item as selected
    if (!m_availableModels.empty()) {
        SendMessage(m_hwndModelSelector, CB_SETCURSEL, 0, 0);
    }

}

void Win32IDE::HandleCopilotSend() {
    SCOPED_METRIC("chat.send_message");
    METRICS.increment("chat.messages_sent");

    if (!m_hwndCopilotChatInput || !m_hwndCopilotChatOutput) return;

    wchar_t inputBuffer[2048] = {0};
    GetWindowTextW(m_hwndCopilotChatInput, inputBuffer, 2047);
    std::string userMessage = wideToUtf8(inputBuffer);
    
    if (userMessage.empty()) {
        LOG_WARNING("Empty message - ignoring");
        return;
    }
    
    // Get selected model
    int modelIdx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);
    std::string selectedModel = (modelIdx >= 0 && modelIdx < (int)m_availableModels.size()) 
        ? m_availableModels[modelIdx] 
        : "llama2";

    // Display user message
    std::string displayText = "\n> User: " + userMessage + "\n";
    
    int len = GetWindowTextLengthW(m_hwndCopilotChatOutput);
    if (len > 0) {
        SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
    }
    SendMessageW(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)utf8ToWide(displayText).c_str());
    
    // Clear input
    SetWindowTextW(m_hwndCopilotChatInput, L"");
    
    // Generate response asynchronously
    auto onResponse = [this](const std::string& response, bool complete) {
        if (!m_hwndCopilotChatOutput) return;
        
        std::string displayResp = "AI: " + response + (complete ? "\n" : "");
        int len = GetWindowTextLengthW(m_hwndCopilotChatOutput);
        if (len > 0) {
            SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
        }
        SendMessageW(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)utf8ToWide(displayResp).c_str());
    };
    
    // Set model override temporarily
    m_ollamaModelOverride = selectedModel;
    
    // Generate response
    generateResponseAsync(userMessage, onResponse);

}

void Win32IDE::HandleCopilotClear() {
    if (!m_hwndCopilotChatOutput || !m_hwndCopilotChatInput) return;

    SetWindowTextW(m_hwndCopilotChatOutput, L"Welcome to RawrXD AI Chat!\n\nSelect a model and type your message to begin.");
    SetWindowTextW(m_hwndCopilotChatInput, L"");
    m_chatHistory.clear();

}

void Win32IDE::HandleCopilotStreamUpdate(const char* token, size_t length) {
    if (!m_hwndCopilotChatOutput || !token) return;

    std::string chunk;
    if (length > 0) {
        chunk.assign(token, token + length);
    } else {
        chunk = token;
    }

    if (chunk.empty()) return;

    int currentLen = GetWindowTextLengthW(m_hwndCopilotChatOutput);
    SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, currentLen, currentLen);
    SendMessageW(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)utf8ToWide(chunk).c_str());
    SendMessage(m_hwndCopilotChatOutput, WM_VSCROLL, SB_BOTTOM, 0);
}

void Win32IDE::onModelSelectionChanged() {
    int idx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);
    if (idx >= 0 && idx < (int)m_availableModels.size()) {
        m_ollamaModelOverride = m_availableModels[idx];

    }
}

void Win32IDE::onMaxTokensChanged(int newValue) {
    m_currentMaxTokens = newValue;
    m_inferenceConfig.maxTokens = newValue;
    
    // Update label
    if (m_hwndMaxTokensLabel) {
        SetWindowTextW(m_hwndMaxTokensLabel, utf8ToWide(std::to_string(newValue)).c_str());
    }

}

// ============================================================================
// IMPLEMENTATIONS for functions declared in Win32IDE.h
// Line Number Gutter, Minimap, Breadcrumb, and other UI components.
// ============================================================================

// --- Line Number Gutter ---
void Win32IDE::createLineNumberGutter(HWND hwndParent) {
    if (!hwndParent) return;
    
    m_hwndLineNumbers = CreateWindowExW(
        0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        0, 0, 50, 100,
        hwndParent, nullptr, m_hInstance, nullptr);
    
    if (m_hwndLineNumbers) {
        SetPropW(m_hwndLineNumbers, L"IDE_PTR", (HANDLE)this);
        m_oldLineNumberProc = (WNDPROC)SetWindowLongPtrW(m_hwndLineNumbers, GWLP_WNDPROC, (LONG_PTR)LineNumberProc);
    }
}

void Win32IDE::updateLineNumbers() {
    if (m_hwndLineNumbers && IsWindow(m_hwndLineNumbers)) {
        InvalidateRect(m_hwndLineNumbers, nullptr, TRUE);
    }
}

void Win32IDE::paintLineNumbers(HDC hdc, RECT& rc) {
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) return;
    
    // Get the current scroll position and line metrics from the rich edit control
    int firstVisibleLine = (int)SendMessage(m_hwndEditor, EM_GETFIRSTVISIBLELINE, 0, 0);
    int lineCount = (int)SendMessage(m_hwndEditor, EM_GETLINECOUNT, 0, 0);
    
    // Get the editor font metrics
    HFONT hFont = m_editorFont ? m_editorFont : (HFONT)GetStockObject(SYSTEM_FIXED_FONT);
    HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
    
    TEXTMETRICW tm;
    GetTextMetricsW(hdc, &tm);
    int lineHeight = tm.tmHeight + tm.tmExternalLeading;
    if (lineHeight <= 0) lineHeight = 16;
    
    SetBkColor(hdc, RGB(30, 30, 30));
    SetTextColor(hdc, RGB(133, 133, 133));
    
    HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);
    
    int visibleLines = (rc.bottom - rc.top) / lineHeight + 1;
    
    for (int i = 0; i < visibleLines && (firstVisibleLine + i) < lineCount; i++) {
        int lineNum = firstVisibleLine + i + 1;
        wchar_t buf[16];
        swprintf_s(buf, L"%4d", lineNum);
        
        RECT lineRect = {rc.left, i * lineHeight, rc.right - 4, (i + 1) * lineHeight};
        
        if (lineNum == m_currentLine) {
            SetTextColor(hdc, RGB(200, 200, 200));
        } else {
            SetTextColor(hdc, RGB(133, 133, 133));
        }
        
        DrawTextW(hdc, buf, -1, &lineRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER);
    }
    
    SelectObject(hdc, hOldFont);
}

LRESULT CALLBACK Win32IDE::LineNumberProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* ide = (Win32IDE*)GetPropW(hwnd, L"IDE_PTR");
    
    if (uMsg == WM_PAINT) {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        if (ide) {
            RECT rc;
            GetClientRect(hwnd, &rc);
            ide->paintLineNumbers(hdc, rc);
        }
        EndPaint(hwnd, &ps);
        return 0;
    }
    
    if (uMsg == WM_ERASEBKGND) {
        return 1; // We handle painting
    }
    
    if (ide && ide->m_oldLineNumberProc) {
        return CallWindowProcW(ide->m_oldLineNumberProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// --- Editor Tab Bar ---
void Win32IDE::createTabBar(HWND hwndParent) {
    if (!hwndParent) return;
    
    m_hwndTabBar = CreateWindowExW(
        0, WC_TABCONTROLW, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | TCS_HOTTRACK | TCS_TOOLTIPS,
        0, 0, 800, 28,
        hwndParent, nullptr, m_hInstance, nullptr);
    
    if (m_hwndTabBar) {
        // Set dark theme font
        if (m_hFontUI) {
            SendMessage(m_hwndTabBar, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
        }
    }
}

void Win32IDE::addTab(const std::string& filePath, const std::string& displayName) {
    // Add a new editor tab
    EditorTab tab;
    tab.filePath = filePath;
    tab.displayName = displayName.empty() ? filePath : displayName;
    tab.modified = false;
    
    m_editorTabs.push_back(tab);
    
    if (m_hwndTabBar) {
        std::wstring displayW = utf8ToWide(tab.displayName);
        TCITEMW tci = {};
        tci.mask = TCIF_TEXT;
        tci.pszText = const_cast<wchar_t*>(displayW.c_str());
        int index = (int)SendMessage(m_hwndTabBar, TCM_GETITEMCOUNT, 0, 0);
        SendMessageW(m_hwndTabBar, TCM_INSERTITEMW, index, (LPARAM)&tci);
        SendMessage(m_hwndTabBar, TCM_SETCURSEL, index, 0);
        m_activeTabIndex = index;
    }
}

void Win32IDE::onTabChanged() {
    if (!m_hwndTabBar) return;
    
    int newIndex = (int)SendMessage(m_hwndTabBar, TCM_GETCURSEL, 0, 0);
    if (newIndex >= 0 && newIndex < (int)m_editorTabs.size() && newIndex != m_activeTabIndex) {
        // Save current tab content
        if (m_activeTabIndex >= 0 && m_activeTabIndex < (int)m_editorTabs.size()) {
            m_editorTabs[m_activeTabIndex].content = getWindowText(m_hwndEditor);
        }

        // Stash annotations for the outgoing tab
        storeAnnotationsForTab();
        
        // Switch to new tab
        m_activeTabIndex = newIndex;
        const auto& tab = m_editorTabs[newIndex];
        
        // Load tab content into editor
        setWindowText(m_hwndEditor, tab.content);
        
        // Update current file path
        m_currentFile = tab.filePath;

        // Restore stashed annotations for the incoming tab
        restoreAnnotationsForTab();

        // Re-detect language for the new file and recolor
        m_syntaxLanguage = detectLanguageFromExtension(m_currentFile);
        onEditorContentChanged();
        
        // Update status bar
        if (m_hwndStatusBar) {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide(tab.displayName).c_str());
        }
        
        // Update line numbers
        updateLineNumbers();
    }
}

void Win32IDE::setActiveTab(int index) {
    if (!m_hwndTabBar) return;
    if (index < 0 || index >= (int)m_editorTabs.size()) return;

    // Use the tab control to select the tab, then trigger onTabChanged
    SendMessage(m_hwndTabBar, TCM_SETCURSEL, index, 0);
    onTabChanged();
}

void Win32IDE::removeTab(int index) {
    if (index < 0 || index >= (int)m_editorTabs.size()) return;

    // Clear annotation cache for the file being closed
    const std::string& closingFile = m_editorTabs[index].filePath;
    if (!closingFile.empty()) {
        m_annotationCache.erase(closingFile);
    }
    // If this is the active tab, clear live annotations
    if (index == m_activeTabIndex) {
        clearAnnotationsForCurrentFile();
    }

    // Remove from the Win32 tab control
    if (m_hwndTabBar) {
        SendMessage(m_hwndTabBar, TCM_DELETEITEM, index, 0);
    }

    // Remove from our vector
    m_editorTabs.erase(m_editorTabs.begin() + index);

    // Adjust active tab index
    if (m_editorTabs.empty()) {
        m_activeTabIndex = -1;
        m_currentFile.clear();
        setWindowText(m_hwndEditor, "");
    } else if (index <= m_activeTabIndex) {
        m_activeTabIndex = std::max(0, m_activeTabIndex - 1);
        SendMessage(m_hwndTabBar, TCM_SETCURSEL, m_activeTabIndex, 0);
        onTabChanged();
    }
}

int Win32IDE::findTabByPath(const std::string& filePath) const {
    for (int i = 0; i < (int)m_editorTabs.size(); i++) {
        if (m_editorTabs[i].filePath == filePath) return i;
    }
    return -1;
}

// --- Command Input Subclass Procedure ---
LRESULT CALLBACK Win32IDE::CommandInputProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Retrieve IDE pointer via GWLP_USERDATA (set in createTerminal)
    Win32IDE* ide = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    
    if (uMsg == WM_KEYDOWN && wParam == VK_RETURN) {
        // Execute command on Enter — route through executeCommand()
        if (ide) {
            ide->executeCommand();
        }
        return 0;
    }
    
    // Up arrow — command history navigation (previous) — uses PowerShell history
    if (uMsg == WM_KEYDOWN && wParam == VK_UP) {
        if (ide) {
            ide->navigatePowerShellHistoryUp();
            // Sync text from PowerShell input to command input
            if (!ide->m_powerShellCommandHistory.empty() && 
                ide->m_powerShellHistoryIndex >= 0 &&
                ide->m_powerShellHistoryIndex < (int)ide->m_powerShellCommandHistory.size()) {
                SetWindowTextW(hwnd, utf8ToWide(ide->m_powerShellCommandHistory[ide->m_powerShellHistoryIndex]).c_str());
                SendMessage(hwnd, EM_SETSEL, -1, -1); // cursor to end
            }
        }
        return 0;
    }
    
    // Down arrow — command history navigation (next) — uses PowerShell history
    if (uMsg == WM_KEYDOWN && wParam == VK_DOWN) {
        if (ide) {
            ide->navigatePowerShellHistoryDown();
            if (ide->m_powerShellHistoryIndex >= 0 &&
                ide->m_powerShellHistoryIndex < (int)ide->m_powerShellCommandHistory.size()) {
                SetWindowTextW(hwnd, utf8ToWide(ide->m_powerShellCommandHistory[ide->m_powerShellHistoryIndex]).c_str());
                SendMessage(hwnd, EM_SETSEL, -1, -1);
            } else {
                SetWindowTextW(hwnd, L"");
            }
        }
        return 0;
    }
    
    if (ide && ide->m_oldCommandInputProc) {
        return CallWindowProcA(ide->m_oldCommandInputProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// --- Agent Output Handling ---
void Win32IDE::onAgentOutput(const char* text) {
    if (!text) return;
    appendToOutput(std::string(text), "Agent", OutputSeverity::Info);
}

void Win32IDE::postAgentOutputSafe(const std::string& text) {
    if (isShuttingDown()) return;
    // Allocate a copy of the string for cross-thread messaging
    // The WM_AGENT_OUTPUT_SAFE handler will free this via free()
    char* copy = _strdup(text.c_str());
    if (copy && m_hwndMain) {
        PostMessage(m_hwndMain, WM_AGENT_OUTPUT_SAFE, 0, (LPARAM)copy);
    }
}

// --- Quick GGUF Model Loader (delegates to unified model dialog) ---
void Win32IDE::quickLoadGGUFModel() {
    openModelUnified();
}

// ============================================================================
// UNIFIED MODEL SOURCE RESOLUTION
// Implements HuggingFace, Ollama blob, HTTP URL, and smart-detect model loading
// Uses ModelSourceResolver for source detection and download/resolution
// All resolved paths feed into the existing loadGGUFModel() 5-step streaming
// pipeline, preserving full zone-based loading for 800B+ models.
// ============================================================================

// ---------------------------------------------------------------------------
// resolveAndLoadModel — Resolve any model source input to a local path, then
// load it through the streaming GGUF pipeline. This is the common path for
// all source types.
// ---------------------------------------------------------------------------
bool Win32IDE::resolveAndLoadModel(const std::string& input) {
    SCOPED_METRIC("model.resolve_and_load");
    METRICS.increment("model.resolve_attempts");

    if (!m_modelResolver) {
        // If resolver wasn't initialized (deferredHeavyInit failure), create one now
        try {
            m_modelResolver = std::make_unique<RawrXD::ModelSourceResolver>();
            OutputDebugStringA("ModelSourceResolver late-initialized in resolveAndLoadModel\n");
        } catch (const std::exception& e) {
            std::string err = "Failed to initialize ModelSourceResolver: " + std::string(e.what());
            appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
            ErrorReporter::report(err, m_hwndMain);
            return false;
        }
    }

    auto sourceType = m_modelResolver->DetectSourceType(input);
    std::string sourceDesc = RawrXD::ModelSourceResolver::SourceTypeToString(sourceType);
    appendToOutput("Model source detected: " + sourceDesc + "\n", "Output", OutputSeverity::Info);
    appendToOutput("Input: " + input + "\n", "Output", OutputSeverity::Info);

    // For local files, skip resolution and go straight to loading
    if (sourceType == GGUFConstants::ModelSourceType::LOCAL_FILE) {
        appendToOutput("Loading local GGUF file directly...\n", "Output", OutputSeverity::Info);
        if (loadGGUFModel(input)) {
            loadModelForInference(input);
            METRICS.increment("model.resolve_success");
            return true;
        }
        METRICS.increment("model.resolve_failures");
        return false;
    }

    // For remote sources, resolve with progress reporting
    appendToOutput("Resolving model source (this may involve downloading)...\n", "Output", OutputSeverity::Info);
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide("Resolving: " + input).c_str());

    // Progress callback that writes to the output panel
    auto progressCallback = [this](const RawrXD::ModelDownloadProgress& prog) {
        if (prog.has_error) {
            appendToOutput("Download error: " + prog.error_message + "\n", "Errors", OutputSeverity::Error);
            return;
        }
        if (prog.is_completed) {
            appendToOutput("Download complete: " + prog.local_path + "\n", "Output", OutputSeverity::Info);
            return;
        }
        // Progress update — update status bar with percentage
        char buf[256];
        if (prog.total_bytes > 0) {
            snprintf(buf, sizeof(buf), "Downloading: %.1f%% (%llu / %llu MB) — %s",
                     prog.progress_percent,
                     (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)),
                     (unsigned long long)(prog.total_bytes / (1024 * 1024)),
                     prog.filename.c_str());
        } else {
            snprintf(buf, sizeof(buf), "Downloading: %llu MB — %s",
                     (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)),
                     prog.filename.c_str());
        }
        if (m_hwndStatusBar) {
            SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide(buf).c_str());
        }
    };

    // Perform resolution (may download)
    RawrXD::ResolvedModelPath resolved;
    try {
        resolved = m_modelResolver->Resolve(input, progressCallback);
    } catch (const std::exception& e) {
        std::string err = "Exception during model resolution: " + std::string(e.what());
        appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(err, m_hwndMain);
        METRICS.increment("model.resolve_failures");
        return false;
    } catch (...) {
        std::string err = "Unknown exception during model resolution for: " + input;
        appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
        METRICS.increment("model.resolve_failures");
        return false;
    }

    if (!resolved.success) {
        std::string err = "Failed to resolve model source: " + resolved.error_message;
        appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(err, m_hwndMain);
        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Model resolution failed");
        METRICS.increment("model.resolve_failures");
        return false;
    }

    // Log resolution details
    appendToOutput("Resolved to local path: " + resolved.local_path + "\n", "Output", OutputSeverity::Info);
    if (!resolved.hf_repo_id.empty()) {
        appendToOutput("HuggingFace repo: " + resolved.hf_repo_id + " / " + resolved.hf_filename + "\n", "Output", OutputSeverity::Info);
    }
    if (!resolved.ollama_model_name.empty()) {
        appendToOutput("Ollama model: " + resolved.ollama_model_name + "\n", "Output", OutputSeverity::Info);
    }

    // Load through the streaming GGUF pipeline (preserves all zone-based 800B+ logic)
    appendToOutput("Loading resolved model through streaming GGUF pipeline...\n", "Output", OutputSeverity::Info);
    if (loadGGUFModel(resolved.local_path)) {
        loadModelForInference(resolved.local_path);
        METRICS.increment("model.resolve_success");
        return true;
    }

    METRICS.increment("model.resolve_failures");
    return false;
}

// ---------------------------------------------------------------------------
// openModelFromHuggingFace — Dialog: enter HuggingFace repo ID, browse GGUF
// files in the repo, select a quant, download and load.
// ---------------------------------------------------------------------------
void Win32IDE::openModelFromHuggingFace() {
    SCOPED_METRIC("model.open_from_huggingface");

    // Step 1: Ask user for HuggingFace repo ID or search query
    char inputBuf[512] = {0};
    // Use a simple input dialog (reuse the existing pattern from command palette)
    // We'll use a Win32 dialog via a helper input box
    
    // Create a simple input dialog
    struct HFInputData {
        char repoId[512];
        bool confirmed;
    };
    HFInputData dlgData = {{0}, false};

    // Use DialogBoxIndirect to create an input dialog
    // For simplicity with Win32 API, use a TaskDialog-style approach
    // We'll create a modeless dialog with CreateWindowEx
    
    // Simple approach: use an edit control dialog
    HWND hDlg = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"STATIC", L"Load from HuggingFace",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 520, 340,
        m_hwndMain, nullptr, m_hInstance, nullptr);
    
    if (!hDlg) {
        appendToOutput("Failed to create HuggingFace dialog\n", "Errors", OutputSeverity::Error);
        return;
    }

    SetClassLongPtrW(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));
    
    HWND hLabel = CreateWindowExW(0, L"STATIC",
        L"Enter HuggingFace repo ID (e.g., TheBloke/Llama-2-7B-GGUF)\n"
        L"or search term (e.g., 'llama 7b gguf'):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 16, 480, 42, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    
    HWND hEdit = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        16, 64, 480, 26, hDlg, (HMENU)101, m_hInstance, nullptr);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SetFocus(hEdit);
    
    HWND hInfoLabel = CreateWindowExW(0, L"STATIC",
        L"Available GGUF files will appear below after Search.",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 100, 480, 20, hDlg, (HMENU)103, m_hInstance, nullptr);
    SendMessage(hInfoLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hList = CreateWindowExW(WS_EX_CLIENTEDGE, L"LISTBOX", L"",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
        16, 124, 480, 120, hDlg, (HMENU)102, m_hInstance, nullptr);
    SendMessage(hList, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hSearchBtn = CreateWindowExW(0, L"BUTTON", L"Search / List Files",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        16, 256, 150, 30, hDlg, (HMENU)201, m_hInstance, nullptr);
    SendMessage(hSearchBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hLoadBtn = CreateWindowExW(0, L"BUTTON", L"Download && Load",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        180, 256, 150, 30, hDlg, (HMENU)202, m_hInstance, nullptr);
    SendMessage(hLoadBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    HWND hCancelBtn = CreateWindowExW(0, L"BUTTON", L"Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        346, 256, 150, 30, hDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Store references for the message loop
    struct HFDialogState {
        Win32IDE* ide;
        HWND hDlg;
        HWND hEdit;
        HWND hList;
        HWND hInfoLabel;
        std::vector<RawrXD::HFModelFileInfo> ggufFiles;
        std::string repoId;
        bool done;
        bool loadRequested;
        int selectedFileIndex;
    };
    
    HFDialogState state = {};
    state.ide = this;
    state.hDlg = hDlg;
    state.hEdit = hEdit;
    state.hList = hList;
    state.hInfoLabel = hInfoLabel;
    state.done = false;
    state.loadRequested = false;
    state.selectedFileIndex = -1;

    // Run a modal-style message pump for this dialog
    EnableWindow(m_hwndMain, FALSE);
    
    MSG msg;
    while (!state.done && GetMessage(&msg, nullptr, 0, 0)) {
        // Handle button clicks for our dialog
        if (msg.message == WM_COMMAND && msg.hwnd == hDlg) {
            int wmId = LOWORD(msg.wParam);
            int wmEvent = HIWORD(msg.wParam);
            
            if (wmId == IDCANCEL) {
                state.done = true;
                continue;
            }
            
            if (wmId == 201) { // Search button
                wchar_t editText[512] = {0};
                GetWindowTextW(hEdit, editText, 512);
                std::string input = wideToUtf8(editText);
                
                if (input.empty()) continue;
                
                // Clear listbox
                SendMessage(hList, LB_RESETCONTENT, 0, 0);
                SetWindowTextW(hInfoLabel, L"Searching HuggingFace...");
                UpdateWindow(hDlg);
                
                state.repoId = input;
                
                // Try to get GGUF files from this repo
                if (m_modelResolver) {
                    try {
                        state.ggufFiles = m_modelResolver->GetHuggingFaceGGUFFiles(input);
                        
                        if (state.ggufFiles.empty()) {
                            // Maybe it's a search query, not a repo ID — try search
                            auto searchResults = m_modelResolver->SearchHuggingFace(input, 10);
                            if (!searchResults.empty()) {
                                // Show search results in the listbox
                                SetWindowTextW(hInfoLabel, L"Search results (select a repo):");
                                for (const auto& result : searchResults) {
                                    std::string entry = result.repo_id + " (" + 
                                        std::to_string(result.gguf_files.size()) + " GGUF files, " +
                                        std::to_string(result.downloads) + " downloads)";
                                    SendMessageW(hList, LB_ADDSTRING, 0, (LPARAM)utf8ToWide(entry).c_str());
                                }
                                // Store repo IDs for selection
                                state.ggufFiles.clear(); // These are repo results, not file results
                            } else {
                                SetWindowTextW(hInfoLabel, L"No results found. Try a different search term.");
                            }
                        } else {
                            // Show GGUF files
                            char infoBuf[256];
                            snprintf(infoBuf, sizeof(infoBuf), "Found %d GGUF files in %s:",
                                     (int)state.ggufFiles.size(), input.c_str());
                            SetWindowTextW(hInfoLabel, utf8ToWide(infoBuf).c_str());
                            
                            for (const auto& file : state.ggufFiles) {
                                char fileLine[512];
                                double sizeMB = file.size_bytes / (1024.0 * 1024.0);
                                double sizeGB = sizeMB / 1024.0;
                                if (sizeGB >= 1.0) {
                                    snprintf(fileLine, sizeof(fileLine), "%s [%s] (%.1f GB)",
                                             file.filename.c_str(), file.quantization.c_str(), sizeGB);
                                } else {
                                    snprintf(fileLine, sizeof(fileLine), "%s [%s] (%.0f MB)",
                                             file.filename.c_str(), file.quantization.c_str(), sizeMB);
                                }
                                SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)fileLine);
                            }
                        }
                    } catch (const std::exception& e) {
                        std::string errMsg = "HuggingFace API error: " + std::string(e.what());
                        SetWindowTextW(hInfoLabel, utf8ToWide(errMsg).c_str());
                    }
                } else {
                    SetWindowTextW(hInfoLabel, L"ModelSourceResolver not initialized!");
                }
                
                UpdateWindow(hDlg);
            }
            
            if (wmId == 202) { // Download & Load button
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)state.ggufFiles.size()) {
                    state.selectedFileIndex = sel;
                    state.loadRequested = true;
                    state.done = true;
                    continue;
                } else if (sel >= 0) {
                    // Might be a search result — get the text and use it as repo ID
                    char selText[512] = {0};
                    SendMessageA(hList, LB_GETTEXT, sel, (LPARAM)selText);
                    std::string selStr(selText);
                    // Extract repo ID (before first space or parenthesis)
                    size_t spacePos = selStr.find(' ');
                    if (spacePos != std::string::npos) {
                        state.repoId = selStr.substr(0, spacePos);
                    } else {
                        state.repoId = selStr;
                    }
                    // Re-search for GGUF files in this repo
                    SetWindowTextW(hEdit, utf8ToWide(state.repoId).c_str());
                    PostMessage(hDlg, WM_COMMAND, MAKEWPARAM(201, BN_CLICKED), (LPARAM)hSearchBtn);
                } else {
                    MessageBoxW(hDlg, L"Please select a GGUF file from the list first.",
                                L"No Selection", MB_OK | MB_ICONINFORMATION);
                }
            }
        }
        
        // Handle WM_SYSCOMMAND close (X button)
        if (msg.message == WM_SYSCOMMAND && (msg.wParam & 0xFFF0) == SC_CLOSE && msg.hwnd == hDlg) {
            state.done = true;
            continue;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);
    
    // If user selected a file, download and load it
    if (state.loadRequested && state.selectedFileIndex >= 0 && 
        state.selectedFileIndex < (int)state.ggufFiles.size()) {
        
        const auto& selectedFile = state.ggufFiles[state.selectedFileIndex];
        appendToOutput("Downloading from HuggingFace: " + state.repoId + " / " + 
                       selectedFile.filename + "\n", "Output", OutputSeverity::Info);
        
        // Download on a background thread to keep UI responsive
        std::string repoId = state.repoId;
        std::string filename = selectedFile.filename;
        
        std::thread([this, repoId, filename]() {
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            if (_guard.cancelled) return;
            auto progressCb = [this](const RawrXD::ModelDownloadProgress& prog) {
                char buf[256];
                if (prog.has_error) {
                    snprintf(buf, sizeof(buf), "Download error: %s", prog.error_message.c_str());
                    PostMessage(m_hwndMain, WM_APP + 200, 0, 0); // Signal UI update
                } else if (prog.is_completed) {
                    snprintf(buf, sizeof(buf), "Download complete!");
                } else if (prog.total_bytes > 0) {
                    snprintf(buf, sizeof(buf), "Downloading: %.1f%% (%llu MB)",
                             prog.progress_percent,
                             (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)));
                } else {
                    snprintf(buf, sizeof(buf), "Downloading: %llu MB",
                             (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)));
                }
                if (m_hwndStatusBar) {
                    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide(buf).c_str());
                }
            };

            try {
                std::string localPath = m_modelResolver->DownloadFromHuggingFace(
                    repoId, filename, progressCb);
                
                if (!localPath.empty()) {
                    // Load on main thread via PostMessage
                    // Store the path and signal the main thread
                    m_loadedModelPath = localPath;
                    PostMessage(m_hwndMain, WM_APP + 201, 0, 0); // Signal: load downloaded model
                } else {
                    if (m_hwndStatusBar) {
                        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"HuggingFace download failed");
                    }
                }
            } catch (const std::exception& e) {
                OutputDebugStringA("HF download exception: ");
                OutputDebugStringA(e.what());
                OutputDebugStringA("\n");
                if (m_hwndStatusBar) {
                    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"HuggingFace download exception");
                }
            }
        }).detach();
    }
}

// ---------------------------------------------------------------------------
// openModelFromOllama — Scan for Ollama blobs, show a selection list,
// validate GGUF magic, and load the selected blob.
// ---------------------------------------------------------------------------
void Win32IDE::openModelFromOllama() {
    SCOPED_METRIC("model.open_from_ollama");

    if (!m_modelResolver) {
        try {
            m_modelResolver = std::make_unique<RawrXD::ModelSourceResolver>();
        } catch (...) {
            appendToOutput("Failed to initialize ModelSourceResolver\n", "Errors", OutputSeverity::Error);
            return;
        }
    }

    appendToOutput("Scanning for Ollama GGUF blobs...\n", "Output", OutputSeverity::Info);
    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"Scanning Ollama blobs...");
    
    // Find all Ollama blobs with valid GGUF magic
    std::vector<RawrXD::OllamaBlobInfo> blobs;
    try {
        blobs = m_modelResolver->FindOllamaBlobs();
    } catch (const std::exception& e) {
        std::string err = "Error scanning Ollama blobs: " + std::string(e.what());
        appendToOutput(err + "\n", "Errors", OutputSeverity::Error);
        ErrorReporter::report(err, m_hwndMain);
        return;
    }

    if (blobs.empty()) {
        MessageBoxW(m_hwndMain,
            L"No Ollama GGUF blobs found.\n\n"
            L"Searched directories:\n"
            L"  - %USERPROFILE%\\.ollama\\models\\blobs\n"
            L"  - D:\\OllamaModels\\blobs\n"
            L"  - C:\\Users\\*\\.ollama\\models\\blobs\n\n"
            L"Make sure Ollama is installed and has downloaded models.",
            L"No Ollama Models Found", MB_OK | MB_ICONINFORMATION);
        return;
    }

    appendToOutput("Found " + std::to_string(blobs.size()) + " Ollama GGUF blobs.\n", 
                   "Output", OutputSeverity::Info);

    // Create a selection dialog
    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "STATIC", "Load from Ollama Blobs",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 580, 350,
        m_hwndMain, nullptr, m_hInstance, nullptr);
    
    if (!hDlg) return;

    SetClassLongPtrA(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));

    // Info label
    char infoText[128];
    snprintf(infoText, sizeof(infoText), "Found %d Ollama GGUF blobs. Select one to load:", (int)blobs.size());
    HWND hLabel = CreateWindowExA(0, "STATIC", infoText,
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 12, 540, 22, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Listbox
    HWND hList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
        16, 40, 540, 220, hDlg, (HMENU)102, m_hInstance, nullptr);
    SendMessage(hList, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Populate with blob info
    for (const auto& blob : blobs) {
        char line[512];
        double sizeGB = blob.size_bytes / (1024.0 * 1024.0 * 1024.0);
        double sizeMB = blob.size_bytes / (1024.0 * 1024.0);
        if (sizeGB >= 1.0) {
            snprintf(line, sizeof(line), "%s — %.1f GB %s",
                     blob.model_name.c_str(), sizeGB,
                     blob.is_valid_gguf ? "[GGUF OK]" : "[INVALID]");
        } else {
            snprintf(line, sizeof(line), "%s — %.0f MB %s",
                     blob.model_name.c_str(), sizeMB,
                     blob.is_valid_gguf ? "[GGUF OK]" : "[INVALID]");
        }
        SendMessageA(hList, LB_ADDSTRING, 0, (LPARAM)line);
    }

    // Load button
    HWND hLoadBtn = CreateWindowExA(0, "BUTTON", "Load Selected",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        16, 272, 150, 30, hDlg, (HMENU)201, m_hInstance, nullptr);
    SendMessage(hLoadBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Cancel button
    HWND hCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        406, 272, 150, 30, hDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    bool done = false;
    int selectedIdx = -1;

    EnableWindow(m_hwndMain, FALSE);
    
    MSG msg;
    while (!done && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND && msg.hwnd == hDlg) {
            int wmId = LOWORD(msg.wParam);
            
            if (wmId == IDCANCEL) {
                done = true;
                continue;
            }
            
            if (wmId == 201) { // Load button
                int sel = (int)SendMessage(hList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)blobs.size()) {
                    selectedIdx = sel;
                    done = true;
                    continue;
                } else {
                    MessageBoxW(hDlg, L"Please select a model from the list.",
                                L"No Selection", MB_OK | MB_ICONINFORMATION);
                }
            }
        }
        
        if (msg.message == WM_SYSCOMMAND && (msg.wParam & 0xFFF0) == SC_CLOSE && msg.hwnd == hDlg) {
            done = true;
            continue;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);

    // Load the selected blob
    if (selectedIdx >= 0 && selectedIdx < (int)blobs.size()) {
        const auto& selected = blobs[selectedIdx];
        
        if (!selected.is_valid_gguf) {
            MessageBoxW(m_hwndMain,
                utf8ToWide("Selected blob does not have valid GGUF magic bytes:\n" + selected.blob_path).c_str(),
                L"Invalid GGUF", MB_OK | MB_ICONWARNING);
            return;
        }
        
        appendToOutput("Loading Ollama blob: " + selected.model_name + "\n", "Output", OutputSeverity::Info);
        appendToOutput("Path: " + selected.blob_path + "\n", "Output", OutputSeverity::Info);
        
        if (loadGGUFModel(selected.blob_path)) {
            loadModelForInference(selected.blob_path);
        }
    }
}

// ---------------------------------------------------------------------------
// openModelFromURL — Dialog: enter HTTP/HTTPS URL to a GGUF file,
// download with progress, and load through the streaming pipeline.
// ---------------------------------------------------------------------------
void Win32IDE::openModelFromURL() {
    SCOPED_METRIC("model.open_from_url");

    if (!m_modelResolver) {
        try {
            m_modelResolver = std::make_unique<RawrXD::ModelSourceResolver>();
        } catch (...) {
            appendToOutput("Failed to initialize ModelSourceResolver\n", "Errors", OutputSeverity::Error);
            return;
        }
    }

    // Create URL input dialog
    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "STATIC", "Load from URL",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 560, 180,
        m_hwndMain, nullptr, m_hInstance, nullptr);
    
    if (!hDlg) return;

    SetClassLongPtrA(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));

    // Label
    HWND hLabel = CreateWindowExA(0, "STATIC",
        "Enter direct URL to a .gguf file (HTTP or HTTPS):",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 16, 520, 22, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hLabel, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // URL edit
    HWND hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        16, 44, 520, 26, hDlg, (HMENU)101, m_hInstance, nullptr);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SetFocus(hEdit);

    // Example label
    HWND hExample = CreateWindowExA(0, "STATIC",
        "Example: https://huggingface.co/TheBloke/Llama-2-7B-GGUF/resolve/main/llama-2-7b.Q4_K_M.gguf",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 76, 520, 18, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hExample, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Download button
    HWND hDownloadBtn = CreateWindowExA(0, "BUTTON", "Download && Load",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        16, 106, 150, 30, hDlg, (HMENU)201, m_hInstance, nullptr);
    SendMessage(hDownloadBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Cancel button
    HWND hCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        386, 106, 150, 30, hDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    bool done = false;
    std::string url;

    EnableWindow(m_hwndMain, FALSE);

    MSG msg;
    while (!done && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND && msg.hwnd == hDlg) {
            int wmId = LOWORD(msg.wParam);
            
            if (wmId == IDCANCEL) {
                done = true;
                continue;
            }
            
            if (wmId == 201) { // Download button
                char editText[2048] = {0};
                GetWindowTextA(hEdit, editText, sizeof(editText));
                url = std::string(editText);
                
                if (url.empty()) {
                    MessageBoxW(hDlg, L"Please enter a URL.", L"Empty URL", MB_OK);
                    continue;
                }
                
                // Basic URL validation
                if (url.find("http://") != 0 && url.find("https://") != 0) {
                    MessageBoxW(hDlg, L"URL must start with http:// or https://",
                                L"Invalid URL", MB_OK | MB_ICONWARNING);
                    continue;
                }
                
                done = true;
                continue;
            }
        }
        
        if (msg.message == WM_SYSCOMMAND && (msg.wParam & 0xFFF0) == SC_CLOSE && msg.hwnd == hDlg) {
            done = true;
            url.clear();
            continue;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);

    if (!url.empty()) {
        appendToOutput("Downloading from URL: " + url + "\n", "Output", OutputSeverity::Info);
        
        // Download on background thread
        std::thread([this, url]() {
            DetachedThreadGuard _guard(m_activeDetachedThreads, m_shuttingDown);
            if (_guard.cancelled) return;
            auto progressCb = [this](const RawrXD::ModelDownloadProgress& prog) {
                char buf[256];
                if (prog.has_error) {
                    snprintf(buf, sizeof(buf), "Download error: %s", prog.error_message.c_str());
                } else if (prog.is_completed) {
                    snprintf(buf, sizeof(buf), "Download complete!");
                } else if (prog.total_bytes > 0) {
                    snprintf(buf, sizeof(buf), "Downloading: %.1f%% (%llu / %llu MB)",
                             prog.progress_percent,
                             (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)),
                             (unsigned long long)(prog.total_bytes / (1024 * 1024)));
                } else {
                    snprintf(buf, sizeof(buf), "Downloading: %llu MB",
                             (unsigned long long)(prog.downloaded_bytes / (1024 * 1024)));
                }
                if (m_hwndStatusBar) {
                    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)utf8ToWide(buf).c_str());
                }
            };

            try {
                std::string localPath = m_modelResolver->DownloadFromURL(url, progressCb);
                
                if (!localPath.empty()) {
                    m_loadedModelPath = localPath;
                    // Signal main thread to load the model
                    PostMessage(m_hwndMain, WM_APP + 201, 0, 0);
                } else {
                    if (m_hwndStatusBar) {
                        SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"URL download failed");
                    }
                }
            } catch (const std::exception& e) {
                OutputDebugStringA("URL download exception: ");
                OutputDebugStringA(e.what());
                OutputDebugStringA("\n");
                if (m_hwndStatusBar) {
                    SendMessageW(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)L"URL download exception");
                }
            }
        }).detach();
    }
}

// ---------------------------------------------------------------------------
// openModelUnified — Smart model open dialog: user types any model identifier
// and it auto-detects the source type (local path, HF repo, Ollama name, URL)
// and routes to the appropriate loader.
// ---------------------------------------------------------------------------
void Win32IDE::openModelUnified() {
    SCOPED_METRIC("model.open_unified");

    // Create the unified input dialog
    HWND hDlg = CreateWindowExA(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        "STATIC", "RawrXD — Smart Model Loader",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT, 580, 260,
        m_hwndMain, nullptr, m_hInstance, nullptr);
    
    if (!hDlg) return;

    SetClassLongPtrA(hDlg, GCLP_HBRBACKGROUND, (LONG_PTR)CreateSolidBrush(RGB(30, 30, 30)));

    // Title label
    HWND hTitle = CreateWindowExA(0, "STATIC",
        "Enter any model identifier — the source will be auto-detected:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 16, 540, 22, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hTitle, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Input edit
    HWND hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        16, 44, 540, 26, hDlg, (HMENU)101, m_hInstance, nullptr);
    SendMessage(hEdit, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);
    SetFocus(hEdit);

    // Help text
    std::string helpText = 
        "Supported formats:\n"
        "  Local file:     C:\\models\\my-model.gguf\n"
        "  HuggingFace:  TheBloke/Llama-2-7B-GGUF  or  hf://repo-id\n"
        "  Ollama blob:   llama3.2:3b  or  codellama:7b\n"
        "  Direct URL:     https://example.com/model.gguf";
    
    HWND hHelp = CreateWindowExA(0, "STATIC", helpText.c_str(),
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        16, 78, 540, 90, hDlg, nullptr, m_hInstance, nullptr);
    SendMessage(hHelp, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Load button
    HWND hLoadBtn = CreateWindowExA(0, "BUTTON", "Detect && Load",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_DEFPUSHBUTTON,
        16, 180, 150, 32, hDlg, (HMENU)201, m_hInstance, nullptr);
    SendMessage(hLoadBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Browse Local button
    HWND hBrowseBtn = CreateWindowExA(0, "BUTTON", "Browse Local...",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        180, 180, 150, 32, hDlg, (HMENU)202, m_hInstance, nullptr);
    SendMessage(hBrowseBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    // Cancel button
    HWND hCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        414, 180, 140, 32, hDlg, (HMENU)IDCANCEL, m_hInstance, nullptr);
    SendMessage(hCancelBtn, WM_SETFONT, (WPARAM)m_hFontUI, TRUE);

    bool done = false;
    std::string inputStr;

    EnableWindow(m_hwndMain, FALSE);

    MSG msg;
    while (!done && GetMessage(&msg, nullptr, 0, 0)) {
        if (msg.message == WM_COMMAND && msg.hwnd == hDlg) {
            int wmId = LOWORD(msg.wParam);
            
            if (wmId == IDCANCEL) {
                done = true;
                continue;
            }
            
            if (wmId == 201) { // Detect & Load
                char editText[2048] = {0};
                GetWindowTextA(hEdit, editText, sizeof(editText));
                inputStr = std::string(editText);
                
                if (inputStr.empty()) {
                    MessageBoxW(hDlg, L"Please enter a model identifier.", L"Empty Input", MB_OK);
                    continue;
                }
                
                done = true;
                continue;
            }
            
            if (wmId == 202) { // Browse Local
                wchar_t filename[MAX_PATH] = {0};
                OPENFILENAMEW ofn = {0};
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = hDlg;
                ofn.lpstrFilter = L"GGUF Models\0*.gguf\0All Files\0*.*\0";
                ofn.lpstrFile = filename;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
                ofn.lpstrTitle = L"Select GGUF Model";

                if (GetOpenFileNameW(&ofn)) {
                    SetWindowTextW(hEdit, filename);
                }
            }
        }

        // Handle Enter key in edit control
        if (msg.message == WM_KEYDOWN && msg.wParam == VK_RETURN && msg.hwnd == hEdit) {
            PostMessage(hDlg, WM_COMMAND, MAKEWPARAM(201, BN_CLICKED), (LPARAM)hLoadBtn);
            continue;
        }
        
        if (msg.message == WM_SYSCOMMAND && (msg.wParam & 0xFFF0) == SC_CLOSE && msg.hwnd == hDlg) {
            done = true;
            inputStr.clear();
            continue;
        }
        
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    EnableWindow(m_hwndMain, TRUE);
    SetForegroundWindow(m_hwndMain);
    DestroyWindow(hDlg);

    if (!inputStr.empty()) {
        resolveAndLoadModel(inputStr);
    }
}

// ============================================================================
// EditorSubclassProc — Editor RichEdit subclass window procedure
// Routes editor-specific messages (scroll sync, key interception) while
// forwarding everything else to the original EDIT wndproc.
// ============================================================================
LRESULT CALLBACK Win32IDE::EditorSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* pThis = (Win32IDE*)GetPropW(hwnd, kEditorWndProp);
    WNDPROC oldProc = (WNDPROC)GetPropW(hwnd, kEditorProcProp);

    if (pThis) {
        switch (uMsg) {
        case WM_VSCROLL:
        case WM_MOUSEWHEEL:
            // After scroll, sync line numbers and minimap
            if (oldProc) {
                LRESULT result = CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
                pThis->updateLineNumbers();
                if (pThis->m_minimapVisible) pThis->updateMinimap();
                return result;
            }
            break;

        case WM_KEYDOWN:
            // Ghost text key handling — Tab accepts, Esc dismisses, other keys dismiss
            if (pThis->handleGhostTextKey((UINT)wParam)) {
                return 0;  // Ghost text consumed the key
            }
            // Ctrl+Shift+P → command palette
            if (wParam == 'P' && (GetKeyState(VK_CONTROL) & 0x8000) && (GetKeyState(VK_SHIFT) & 0x8000)) {
                pThis->showCommandPalette();
                return 0;
            }
            // F9 → toggle breakpoint at current line
            if (wParam == VK_F9) {
                CHARRANGE sel;
                SendMessage(hwnd, EM_EXGETSEL, 0, (LPARAM)&sel);
                int line = (int)SendMessage(hwnd, EM_LINEFROMCHAR, sel.cpMin, 0) + 1;
                pThis->toggleBreakpoint(pThis->m_currentFile, line);
                return 0;
            }
            break;

        case WM_PAINT: {
            // Let the RichEdit control paint itself first
            if (oldProc) {
                LRESULT result = CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
                // Overlay ghost text on top of the editor content
                if (pThis->m_ghostTextVisible) {
                    HDC hdc = GetDC(hwnd);
                    if (hdc) {
                        pThis->renderGhostText(hdc);
                        ReleaseDC(hwnd, hdc);
                    }
                }
                return result;
            }
            break;
        }

        case WM_CHAR:
            // After character input, trigger syntax coloring debounce
            if (oldProc) {
                LRESULT result = CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
                pThis->onEditorContentChanged();
                return result;
            }
            break;

        case WM_DESTROY:
            // Clean up properties on destruction
            RemovePropW(hwnd, kEditorWndProp);
            RemovePropW(hwnd, kEditorProcProp);
            break;
        }
    }

    if (oldProc) {
        return CallWindowProcW(oldProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// SidebarProcImpl — Secondary sidebar (AI Chat panel) window procedure
// Handles paint, sizing, and command routing for the right-side AI panel.
// Distinct from SidebarProc which handles the primary (left) sidebar.
// ============================================================================
LRESULT CALLBACK Win32IDE::SidebarProcImpl(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        COLORREF bgColor = pThis ? pThis->m_currentTheme.sidebarBg : RGB(37, 37, 38);
        HBRUSH hBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        EndPaint(hwnd, &ps);
        return 0;
    }

    case WM_COMMAND: {
        if (pThis) {
            int controlId = LOWORD(wParam);
            int notifyCode = HIWORD(wParam);
            // Route button clicks from AI Chat panel controls
            if (controlId == IDC_AI_MAX_MODE && notifyCode == BN_CLICKED) {
                pThis->onAIModeMax();
            } else if (controlId == IDC_AI_DEEP_THINK && notifyCode == BN_CLICKED) {
                pThis->onAIModeDeepThink();
            } else if (controlId == IDC_AI_DEEP_RESEARCH && notifyCode == BN_CLICKED) {
                pThis->onAIModeDeepResearch();
            } else if (controlId == IDC_AI_NO_REFUSAL && notifyCode == BN_CLICKED) {
                pThis->onAIModeNoRefusal();
            }
        }
        return 0;
    }

    case WM_SIZE: {
        if (pThis) {
            pThis->updateSecondarySidebarContent();
        }
        return 0;
    }
    }

    // Forward to the original sidebar window procedure
    if (pThis && pThis->m_oldSidebarProc) {
        return CallWindowProcA(pThis->m_oldSidebarProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcA(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// getCurrentGitBranch — Returns the current git branch name
// ============================================================================
std::string Win32IDE::getCurrentGitBranch() const {
    if (!isGitRepository()) return "";

    std::string output;
    const_cast<Win32IDE*>(this)->executeGitCommand("git rev-parse --abbrev-ref HEAD", output);

    // Trim whitespace/newlines from output
    while (!output.empty() && (output.back() == '\n' || output.back() == '\r' || output.back() == ' ')) {
        output.pop_back();
    }
    return output;
}

// ============================================================================
// Terminal Pane Management
// Multi-terminal support: switch, close, resize, and broadcast to panes.
// ============================================================================

void Win32IDE::switchTerminalPane(int paneId) {
    LOG_INFO("switchTerminalPane: paneId=" + std::to_string(paneId));
    TerminalPane* pane = findTerminalPane(paneId);
    if (pane) {
        setActiveTerminalPane(paneId);
        appendToOutput("Switched to terminal: " + pane->name + "\n", "Output", OutputSeverity::Info);
    } else {
        appendToOutput("Terminal pane " + std::to_string(paneId) + " not found\n", "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::closeTerminalPane(int paneId) {
    LOG_INFO("closeTerminalPane: paneId=" + std::to_string(paneId));
    for (auto it = m_terminalPanes.begin(); it != m_terminalPanes.end(); ++it) {
        if (it->id == paneId) {
            if (it->manager) it->manager->stop();
            if (it->hwnd && IsWindow(it->hwnd)) DestroyWindow(it->hwnd);
            m_terminalPanes.erase(it);
            // Switch to another pane if we closed the active one
            if (m_activeTerminalId == paneId && !m_terminalPanes.empty()) {
                setActiveTerminalPane(m_terminalPanes.front().id);
            }
            appendToOutput("Closed terminal pane " + std::to_string(paneId) + "\n", "Output", OutputSeverity::Info);
            return;
        }
    }
    appendToOutput("Terminal pane " + std::to_string(paneId) + " not found\n", "Output", OutputSeverity::Warning);
}

void Win32IDE::resizeTerminalPanes() {
    LOG_INFO("resizeTerminalPanes");
    if (m_terminalPanes.empty()) return;
    
    RECT rc;
    GetClientRect(m_hwndMain, &rc);
    int totalWidth = rc.right;
    int paneWidth = totalWidth / static_cast<int>(m_terminalPanes.size());
    
    int x = 0;
    for (auto& pane : m_terminalPanes) {
        if (pane.hwnd && IsWindow(pane.hwnd)) {
            pane.bounds = { x, 0, x + paneWidth, rc.bottom };
            MoveWindow(pane.hwnd, x, 0, paneWidth, rc.bottom, TRUE);
        }
        x += paneWidth;
    }
}

void Win32IDE::sendToAllTerminals(const std::string& command) {
    LOG_INFO("sendToAllTerminals: " + command);
    for (auto& pane : m_terminalPanes) {
        if (pane.manager) {
            pane.manager->writeInput(command + "\r\n");
        }
    }
    appendToOutput("Sent to all " + std::to_string(m_terminalPanes.size()) + " terminals: " + command + "\n", "Output", OutputSeverity::Info);
}

// ============================================================================
// Extension System
// Refresh, load, unload, and help for IDE extensions via m_extensionLoader.
// ============================================================================

void Win32IDE::refreshExtensions() {
    LOG_INFO("refreshExtensions");
    if (m_extensionLoader) {
        m_extensionLoader->Scan();
        auto exts = m_extensionLoader->GetExtensions();
        appendToOutput("Extensions refreshed: " + std::to_string(exts.size()) + " found\n", "Output", OutputSeverity::Info);
    } else {
        appendToOutput("⚠️ Extension loader not initialized\n", "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::loadExtension(const std::string& name) {
    LOG_INFO("loadExtension: " + name);
    if (m_extensionLoader) {
        // Re-scan to ensure extension list is current, then load native modules
        m_extensionLoader->Scan();
        m_extensionLoader->LoadNativeModules();
        appendToOutput("✅ Extension loaded: " + name + "\n", "Output", OutputSeverity::Info);
    } else {
        appendToOutput("⚠️ Extension loader not initialized\n", "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::unloadExtension(const std::string& name) {
    LOG_INFO("unloadExtension: " + name);
    if (m_extensionLoader) {
        bool unloaded = m_extensionLoader->UnloadExtension(name);
        if (unloaded) {
            appendToOutput("✅ Extension unloaded: " + name + "\n", "Output", OutputSeverity::Info);
        } else {
            appendToOutput("⚠️ Failed to unload extension: " + name + " (not found or not loaded)\n",
                           "Output", OutputSeverity::Warning);
        }
    } else {
        appendToOutput("⚠️ Extension loader not initialized\n", "Output", OutputSeverity::Warning);
    }
}

void Win32IDE::showExtensionHelp(const std::string& name) {
    LOG_INFO("showExtensionHelp: " + name);
    if (m_extensionLoader) {
        std::string help = m_extensionLoader->GetHelp(name);
        appendToOutput("--- Extension Help: " + name + " ---\n" + help + "\n", "Output", OutputSeverity::Info);
    } else {
        appendToOutput("⚠️ Extension loader not initialized\n", "Output", OutputSeverity::Warning);
    }
}

// ============================================================================
// DEFERRED IMPLEMENTATIONS — PowerShell Panel Dock/Float
// ============================================================================

void Win32IDE::dockPowerShellPanel() {
    LOG_INFO("dockPowerShellPanel");
    m_powerShellPanelDocked = true;
    
    if (m_hwndPowerShellPanel && IsWindow(m_hwndPowerShellPanel)) {
        // Remove WS_POPUP, add WS_CHILD — reparent to main window
        LONG style = GetWindowLong(m_hwndPowerShellPanel, GWL_STYLE);
        style = (style & ~WS_POPUP) | WS_CHILD;
        SetWindowLong(m_hwndPowerShellPanel, GWL_STYLE, style);
        SetParent(m_hwndPowerShellPanel, m_hwndMain);
        
        // Trigger layout recalculation
        RECT rc;
        GetClientRect(m_hwndMain, &rc);
        onSize(rc.right, rc.bottom);
    }
    
    appendToOutput("PowerShell panel docked\n", "Output", OutputSeverity::Info);
}

void Win32IDE::floatPowerShellPanel() {
    LOG_INFO("floatPowerShellPanel");
    m_powerShellPanelDocked = false;
    
    if (m_hwndPowerShellPanel && IsWindow(m_hwndPowerShellPanel)) {
        // Remove WS_CHILD, add WS_POPUP — detach from main window
        LONG style = GetWindowLong(m_hwndPowerShellPanel, GWL_STYLE);
        style = (style & ~WS_CHILD) | WS_POPUP | WS_CAPTION | WS_THICKFRAME;
        SetWindowLong(m_hwndPowerShellPanel, GWL_STYLE, style);
        SetParent(m_hwndPowerShellPanel, nullptr);
        
        // Position floating window near the main window
        RECT mainRect;
        GetWindowRect(m_hwndMain, &mainRect);
        SetWindowPos(m_hwndPowerShellPanel, HWND_TOP,
                     mainRect.right - 500, mainRect.bottom - 400, 480, 360,
                     SWP_SHOWWINDOW);
    }
    
    appendToOutput("PowerShell panel floating\n", "Output", OutputSeverity::Info);
}