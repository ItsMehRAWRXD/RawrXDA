#include "Win32IDE.h"
#include "CommandRegistry.hpp"
#include "CommandIDs.hpp"
#include "IDELogger.h"
// ...existing code...
#define IDC_EDITOR 50001
#define IDC_TERMINAL 50002
#define IDC_COMMAND_INPUT 50003
#define IDC_STATUS_BAR 50004
#define IDC_OUTPUT_TABS 50005
#define IDC_MINIMAP 50006
#define IDC_MODULE_BROWSER 50007
#define IDC_HELP_PANEL 50008
#define IDC_SNIPPET_LIST 50009
#define IDC_CLIPBOARD_HISTORY 50010
#define IDC_OUTPUT_TEXT 50011
#define IDC_OUTPUT_EDIT_GENERAL 50012
#define IDC_OUTPUT_EDIT_ERRORS 50013
#define IDC_OUTPUT_EDIT_DEBUG 50014
#define IDC_OUTPUT_EDIT_FIND 50015
#define IDC_SPLITTER 50016
#define IDC_SEVERITY_FILTER 50017
#define IDC_TITLE_TEXT 50018
#define IDC_BTN_MINIMIZE 50019
#define IDC_BTN_MAXIMIZE 50020
#define IDC_BTN_CLOSE 50021
#define IDC_BTN_GITHUB 50022
#define IDC_BTN_MICROSOFT 50023
#define IDC_BTN_SETTINGS 50024
#define IDC_FILE_EXPLORER 50025
#define IDC_FILE_TREE 50026

// Remap IDM to CommandRegistry IDs
#define IDM_FILE_NEW                RawrXD::Win32::CommandID::FILE_NEW
#define IDM_FILE_OPEN               RawrXD::Win32::CommandID::FILE_OPEN
#define IDM_FILE_SAVE               RawrXD::Win32::CommandID::FILE_SAVE
#define IDM_FILE_SAVEAS             RawrXD::Win32::CommandID::FILE_SAVE_AS
#define IDM_FILE_LOAD_MODEL         RawrXD::Win32::CommandID::FILE_OPEN // Reuse open
#define IDM_FILE_EXIT               RawrXD::Win32::CommandID::FILE_EXIT

#define IDM_EDIT_UNDO               RawrXD::Win32::CommandID::EDIT_UNDO
#define IDM_EDIT_REDO               RawrXD::Win32::CommandID::EDIT_REDO
#define IDM_EDIT_CUT                RawrXD::Win32::CommandID::EDIT_CUT
#define IDM_EDIT_COPY               RawrXD::Win32::CommandID::EDIT_COPY
#define IDM_EDIT_PASTE              RawrXD::Win32::CommandID::EDIT_PASTE
#define IDM_EDIT_FIND               RawrXD::Win32::CommandID::EDIT_FIND
#define IDM_EDIT_REPLACE            RawrXD::Win32::CommandID::EDIT_REPLACE

#define IDM_VIEW_MINIMAP            RawrXD::Win32::CommandID::VIEW_MINIMAP
#define IDM_VIEW_OUTPUT_PANEL       RawrXD::Win32::CommandID::VIEW_OUTPUT
#define IDM_VIEW_SIDEBAR            RawrXD::Win32::CommandID::VIEW_EXPLORER
#define IDM_VIEW_TERMINAL           RawrXD::Win32::CommandID::VIEW_TERMINAL

#define IDM_TERMINAL_POWERSHELL     RawrXD::Win32::CommandID::TERMINAL_NEW_POWERSHELL
#define IDM_TERMINAL_CMD            RawrXD::Win32::CommandID::TERMINAL_NEW_CMD
#define IDM_TERMINAL_STOP           RawrXD::Win32::CommandID::TERMINAL_KILL
#define IDM_TERMINAL_CLEAR_ALL      RawrXD::Win32::CommandID::TERMINAL_CLEAR

#define IDM_HELP_ABOUT              RawrXD::Win32::CommandID::HELP_ABOUT
#define IDM_HELP_CMDREF             RawrXD::Win32::CommandID::HELP_SHORTCUTS

// IDC Activity Bar
#define IDC_ACTIVITY_BAR            51100
#define IDC_ACTBAR_EXPLORER         51101
#define IDC_ACTBAR_SEARCH           51102
#define IDC_ACTBAR_SCM              51103
#define IDC_ACTBAR_DEBUG            51104
#define IDC_ACTBAR_EXTENSIONS       51105
#define IDC_ACTBAR_SETTINGS         51106

// IDC Secondary Sidebar
#define IDC_SECONDARY_SIDEBAR       51200
#define IDC_COPILOT_CHAT_INPUT      51202
#define IDC_COPILOT_CHAT_OUTPUT     51203
#define IDC_COPILOT_SEND_BTN        51204
#define IDC_COPILOT_CLEAR_BTN       51205

// IDC Panels
#define IDC_PANEL_CONTAINER         51300
#define IDC_PANEL_TERMINAL          51302
#define IDC_PANEL_OUTPUT            51303

// Status Bar IDs
#define IDC_STATUS_BRANCH           51401
#define IDC_STATUS_SYNC             51402
#define IDC_STATUS_ERRORS           51403
#define IDC_STATUS_WARNINGS         51404
#define IDC_STATUS_LINE_COL         51405
#define IDC_STATUS_SPACES           51406
#define IDC_STATUS_ENCODING         51407
#define IDC_STATUS_EOL              51408
#define IDC_STATUS_LANGUAGE         51409
#define IDC_STATUS_COPILOT          51410
#define IDC_STATUS_NOTIFICATIONS    51411

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

#define IDM_FILE_NEW 2001
#define IDM_FILE_OPEN 2002
#define IDM_FILE_SAVE 2003
#define IDM_FILE_SAVEAS 2004
#define IDM_FILE_LOAD_MODEL 2006
#define IDM_FILE_EXIT 2005

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
#define IDM_TOOLS_SCHEMATIC_STUDIO 3053
#define IDM_TOOLS_LAYOUT_APPLY 3054

#define IDM_HELP_ABOUT 4001
#define IDM_HELP_CMDREF 4002
#define IDM_HELP_PSDOCS 4003
#define IDM_HELP_SEARCH 4004

#define IDT_LAYOUT_WATCH 9001

// Agent menu IDs
#define IDM_AGENT_START_LOOP 4100
#define IDM_AGENT_EXECUTE_CMD 4101
#define IDM_AGENT_CONFIGURE_MODEL 4102
#define IDM_AGENT_VIEW_TOOLS 4103
#define IDM_AGENT_VIEW_STATUS 4104
#define IDM_AGENT_STOP 4105

// Command Palette control IDs
#define IDC_CMDPAL_CONTAINER 1500
#define IDC_CMDPAL_INPUT 1501
#define IDC_CMDPAL_LIST 1502

Win32IDE::Win32IDE(HINSTANCE hInstance)
        : m_hInstance(hInstance), m_hwndMain(nullptr), m_hwndEditor(nullptr),
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
      m_moduleListDirty(true), m_backgroundBrush(nullptr), m_editorFont(nullptr),
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
    , m_hwndCommandPalette(nullptr), m_hwndCommandPaletteInput(nullptr), m_hwndCommandPaletteList(nullptr), m_commandPaletteVisible(false)
    , m_hwndModelSelector(nullptr), m_hwndMaxTokensSlider(nullptr), m_hwndMaxTokensLabel(nullptr)
    , m_currentMaxTokens(512)
    , m_layoutLivePath(), m_layoutLastWrite{0, 0}, m_layoutWatchEnabled(false), m_layoutWatchTimer(0)
{
    registerCommands();
    // DIAGNOSTIC: Constructor entry (using cross-platform path resolution)
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\CONSTRUCTOR_START.txt";
        std::ofstream diag(diagPath);
        diag << "Win32IDE constructor entered" << std::endl;
    }

    // Initialize logger ABSOLUTELY FIRST - with fallback error handling
    try {
        std::string logPath = PathResolver::getLogsPath().toStdString() + "\\RawrXD_IDE.log";
        IDELogger::getInstance().initialize(logPath);
        LOG_INFO("=== Win32IDE constructor started ===");
        LOG_INFO("Logger initialized successfully");
    } catch (const std::exception& e) {
        OutputDebugStringA("FATAL: Logger initialization failed: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        // Continue without logging
    } catch (...) {
        OutputDebugStringA("FATAL: Logger initialization failed with unknown exception\n");
    }
    
    // DIAGNOSTIC: After logger section
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_LOGGER.txt";
        std::ofstream diag(diagPath);
        diag << "Logger section skipped" << std::endl;
    }
    
    // Prepare DirectX renderer with safety wrapper
    try {
        LOG_DEBUG("Creating TransparentRenderer...");
        m_renderer = std::make_unique<TransparentRenderer>();
        LOG_INFO("TransparentRenderer created successfully");
    } catch (const std::exception& e) {
        LOG_CRITICAL(std::string("TransparentRenderer creation failed: ") + e.what());
        OutputDebugStringA("ERROR: TransparentRenderer failed: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        m_renderer = nullptr; // Use null renderer
    } catch (...) {
        LOG_CRITICAL("TransparentRenderer creation failed with unknown exception");
        OutputDebugStringA("ERROR: TransparentRenderer failed with unknown exception\n");
        m_renderer = nullptr;
    }
    
    // DIAGNOSTIC: After renderer
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_RENDERER.txt";
        std::ofstream diag(diagPath);
        diag << "Renderer created: " << (m_renderer ? "SUCCESS" : "NULL") << std::endl;
    }
    
    // Initialize PowerShell state with safety
    try {
        LOG_DEBUG("Initializing PowerShell state...");
        initializePowerShellState();
        LOG_INFO("PowerShell state initialized");
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("PowerShell state init failed: ") + e.what());
        OutputDebugStringA("ERROR: PowerShell init failed\n");
    } catch (...) {
        LOG_ERROR("PowerShell state init failed with unknown exception");
        OutputDebugStringA("ERROR: PowerShell init failed\n");
    }
    
    // DIAGNOSTIC: After PowerShell
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_POWERSHELL.txt";
        std::ofstream diag(diagPath);
        diag << "PowerShell state initialized" << std::endl;
    }
    
    // Initialize default theme
    try {
        LOG_DEBUG("Resetting to default theme...");
        resetToDefaultTheme();
        LOG_DEBUG("Theme reset complete");
    } catch (...) {
        LOG_ERROR("Theme reset failed");
        OutputDebugStringA("ERROR: Theme reset failed\n");
    }
    
    // DIAGNOSTIC: After theme
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_THEME.txt";
        std::ofstream diag(diagPath);
        diag << "Theme reset complete" << std::endl;
    }
    
    // Load code snippets
    try {
        LOG_DEBUG("Loading code snippets...");
        loadCodeSnippets();
        LOG_DEBUG("Code snippets loaded");
    } catch (...) {
        LOG_ERROR("Code snippets loading failed");
        OutputDebugStringA("ERROR: Code snippets loading failed\n");
    }
    
    // DIAGNOSTIC: After snippets
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_SNIPPETS.txt";
        std::ofstream diag(diagPath);
        diag << "Code snippets loaded" << std::endl;
    }
    
    // Initialize profiling frequency
    QueryPerformanceFrequency(&m_profilingFreq);
    LOG_DEBUG("Profiling frequency initialized");
    
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
    m_ollamaModelOverride.clear();

    // Initialize settings manager and load persisted preferences
    try {
        SettingsManager& settings = SettingsManager::getInstance();
        settings.initialize();
        LOG_DEBUG("SettingsManager initialized");
        
        // Initialize inference settings manager
        InferenceSettingsManager& inferenceSettings = InferenceSettingsManager::getInstance();
        inferenceSettings.initialize();
        LOG_DEBUG("InferenceSettingsManager initialized");
        
        // Load UI preferences
        settings.loadUILayout(m_outputTabHeight, m_selectedOutputTab, m_terminalHeight, m_outputPanelVisible);
        LOG_DEBUG("UI layout preferences loaded");
        
        // Load filter preferences
        m_severityFilterLevel = settings.loadSeverityFilter();
        LOG_DEBUG("Severity filter loaded");
        
        // Load inference preferences (legacy - now handled by InferenceSettingsManager)
        std::string modelTag, tokenLimit;
        settings.loadInferenceSettings(m_ollamaBaseUrl, modelTag, m_useStreamingLoader, tokenLimit);
        if (!modelTag.empty()) {
            m_ollamaModelOverride = modelTag;
        }
        LOG_DEBUG("Inference settings loaded");
        
        // Load renderer preference
        m_useVulkanRenderer = settings.loadRendererPreference();
        LOG_DEBUG("Renderer preference loaded");
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Settings loading failed: ") + e.what());
        // Continue with defaults
    } catch (...) {
        LOG_ERROR("Settings loading failed with unknown exception");
        // Continue with defaults
    }
    
    // DIAGNOSTIC: After settings load
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_SETTINGS.txt";
        std::ofstream diag(diagPath);
        diag << "Settings loaded from ide_settings.ini" << std::endl;
    }

    // Initialize GGUF loader based on saved preference
    if (m_useStreamingLoader) {
        m_ggufLoader = std::make_unique<StreamingGGUFLoader>();
        // appendToOutput("Using Streaming GGUF Loader (index + zone streaming)\n", "Output", OutputSeverity::Info);  // DISABLED - no windows yet!
    } else {
        m_ggufLoader = std::make_unique<GGUFLoader>();
        // appendToOutput("Using Standard GGUF Loader\n", "Output", OutputSeverity::Info);  // DISABLED - no windows yet!
    }
    
    // DIAGNOSTIC: After GGUF loader
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_GGUF.txt";
        std::ofstream diag(diagPath);
        diag << "GGUF loader created: " << (m_useStreamingLoader ? "STREAMING" : "STANDARD") << std::endl;
    }
    
    // Constructor diagnostic
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\IDE_CONSTRUCTOR_COMPLETED.txt";
        std::ofstream constructorDone(diagPath);
        constructorDone << "Win32IDE constructor finished successfully" << std::endl;
        constructorDone.close();
    }
}

void Win32IDE::initializeEditorSurface()
{
    if (!m_hwndEditor || m_editorHooksInstalled) return;

    SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);
    SetPropW(m_hwndEditor, kEditorWndProp, this);

    WNDPROC original = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(m_hwndEditor, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditorSubclassProc)));
    SetPropW(m_hwndEditor, kEditorProcProp, reinterpret_cast<HANDLE>(original));

    HideCaret(m_hwndEditor);
    m_editorHooksInstalled = true;

    syncEditorToGpuSurface();
}

void Win32IDE::syncEditorToGpuSurface()
{
    if (!m_gpuTextEnabled || !m_rendererReady || !m_renderer) return;
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) return;
    if (m_editorRect.right <= m_editorRect.left || m_editorRect.bottom <= m_editorRect.top) return;

    int length = GetWindowTextLengthW(m_hwndEditor);
    std::wstring text;
    if (length > 0) {
        std::wstring buffer(static_cast<size_t>(length) + 1, L'\0');
        int copied = GetWindowTextW(m_hwndEditor, buffer.data(), length + 1);
        if (copied >= 0) {
            buffer.resize(static_cast<size_t>(copied));
            text = std::move(buffer);
        }
    }

    CHARRANGE range{};
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);
    size_t caretIndex = static_cast<size_t>(std::max<LONG>(0, range.cpMax));
    if (caretIndex > text.size()) {
        caretIndex = text.size();
    }

    size_t caretLine = 0;
    size_t caretColumn = 0;
    size_t limit = (std::min)(caretIndex, text.size());
    for (size_t i = 0; i < limit; ++i) {
        wchar_t ch = text[i];
        if (ch == L'\r') continue;
        if (ch == L'\n') {
            ++caretLine;
            caretColumn = 0;
        } else {
            ++caretColumn;
        }
    }

    if (!m_renderer) {
        // If user preference requests Vulkan and it's available, attempt to create it
        if (m_useVulkanRenderer) {
#ifdef ENABLE_VULKAN
            IRenderer* vkr = CreateVulkanRenderer();
            if (vkr) m_renderer.reset(vkr);
#endif
        }
        // Default to DirectX/D3D renderer if Vulkan not chosen
        if (!m_renderer) {
            m_renderer = std::make_unique<TransparentRenderer>();
        }
    }
    if (m_renderer) {
        m_renderer->updateEditorText(text, m_editorRect, caretIndex, caretLine, caretColumn);
    }
    InvalidateRect(m_hwndMain, &m_editorRect, FALSE);
}

LRESULT CALLBACK Win32IDE::EditorSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* ide = reinterpret_cast<Win32IDE*>(GetPropW(hwnd, kEditorWndProp));
    WNDPROC original = reinterpret_cast<WNDPROC>(GetPropW(hwnd, kEditorProcProp));

    switch (uMsg) {
    case WM_PAINT:
    case WM_ERASEBKGND:
        return 0;
    case WM_SETFOCUS:
        HideCaret(hwnd);
        break;
    case WM_DESTROY:
        if (original) {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
        }
        RemovePropW(hwnd, kEditorWndProp);
        RemovePropW(hwnd, kEditorProcProp);
        if (ide) {
            ide->m_editorHooksInstalled = false;
        }
        break;
    default:
        break;
    }

    if (original) {
        return CallWindowProcW(original, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

Win32IDE::~Win32IDE()
{
    for (auto& pane : m_terminalPanes) {
        if (pane.manager) {
            pane.manager->stop();
        }
    }
    
    // Cleanup theme resources
    if (m_backgroundBrush) {
        DeleteObject(m_backgroundBrush);
    }
    if (m_editorFont) {
        DeleteObject(m_editorFont);
    }
    
    // Save snippets and theme
    saveCodeSnippets();
    saveTheme("current");
    
    // Persist all settings using SettingsManager
    try {
        SettingsManager& settings = SettingsManager::getInstance();
        
        // Save UI layout
        settings.saveUILayout(m_outputTabHeight, m_selectedOutputTab, m_terminalHeight, m_outputPanelVisible);
        
        // Save filter preferences
        settings.saveSeverityFilter(m_severityFilterLevel);
        
        // Save inference settings
        settings.saveInferenceSettings(m_ollamaBaseUrl, m_ollamaModelOverride, 
                                      m_useStreamingLoader, std::to_string(m_currentMaxTokens));
        
        // Save renderer preference
        settings.saveRendererPreference(m_useVulkanRenderer);
        
        // Explicitly save to disk
        settings.save();
        LOG_INFO("All settings saved successfully");
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to save settings: ") + e.what());
    } catch (...) {
        LOG_ERROR("Failed to save settings with unknown exception");
    }
}

bool Win32IDE::createWindow()
{
    // ULTRA-EARLY diagnostic file write (using cross-platform path resolution)
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\createWindow_CALLED.txt";
        std::ofstream diag(diagPath);
        diag << "createWindow() entered" << std::endl;
        diag.close();
    }
    
    LOG_INFO("createWindow() called");
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = "RawrXD_IDE_Class";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));  // Solid dark background, NOT transparent
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;  // Force full redraw on resize

    if (!RegisterClassA(&wc)) {
        DWORD err = GetLastError();
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\RegisterClass_FAILED.txt";
        std::ofstream diagErr(diagPath);
        diagErr << "RegisterClassA failed with error: " << err << std::endl;
        diagErr.close();
        LOG_ERROR("Failed to register window class");
        return false;
    }
    LOG_DEBUG("Window class registered successfully");

    // NO WS_EX_LAYERED - prevents transparency issues
    m_hwndMain = CreateWindowA("RawrXD_IDE_Class", "RawrXD IDE",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                              CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
                              nullptr, nullptr, m_hInstance, this);

    if (!m_hwndMain) {
        DWORD err = GetLastError();
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\CreateWindow_FAILED.txt";
        std::ofstream diagErr(diagPath);
        diagErr << "CreateWindowA failed with error: " << err << std::endl;
        diagErr.close();
        LOG_ERROR("Failed to create main window");
        return false;
    }
    
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\Window_CREATED_SUCCESS.txt";
        std::ofstream diag(diagPath);
        diag << "Window created: HWND = " << (void*)m_hwndMain << std::endl;
        diag.close();
    }
    
    LOG_INFO("Main window created successfully");

    // Center the window on the primary monitor and bring to front
    RECT rc{};
    GetWindowRect(m_hwndMain, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;
    if (x < 0) x = 0; if (y < 0) y = 0;
    MoveWindow(m_hwndMain, x, y, winW, winH, FALSE);
    SetForegroundWindow(m_hwndMain);
    return true;
}

void Win32IDE::showWindow()
{
    LOG_INFO("showWindow() called");
    if (m_hwndMain) {
        ShowWindow(m_hwndMain, SW_SHOW);
        UpdateWindow(m_hwndMain);
        SetForegroundWindow(m_hwndMain);
        LOG_DEBUG("Window shown and updated");
    } else {
        LOG_ERROR("Cannot show window - m_hwndMain is null");
    }
}

int Win32IDE::runMessageLoop()
{
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // Integrated Keyboard Shortcut Handling
        if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            
            // Route through CommandRegistry before standard dispatch
            if (handleKeyboardShortcutViaRegistry((UINT)msg.wParam, ctrl, shift, alt)) {
                continue; 
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK Win32IDE::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Win32IDE*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    } else {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT Win32IDE::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        onCreate(hwnd);
        return 0;

    case WM_DESTROY:
        onDestroy();
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        onSize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_TIMER:
        if (wParam == IDT_LAYOUT_WATCH) {
            checkLayoutLiveUpdates();
            return 0;
        }
        break;

    case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            // Activity Bar CIRCULAR buttons implementation
            if (pdis->CtlID >= 61001 && pdis->CtlID <= 61005) {
                HDC hdc = pdis->hDC;
                RECT rc = pdis->rcItem;
                bool selected = (pdis->itemState & ODS_SELECTED);
                bool active = false;
                
                if (m_currentSidebarView == SidebarView::Explorer && pdis->CtlID == 61001) active = true;
                if (m_currentSidebarView == SidebarView::Search && pdis->CtlID == 61002) active = true;
                if (m_currentSidebarView == SidebarView::SourceControl && pdis->CtlID == 61003) active = true;
                if (m_currentSidebarView == SidebarView::RunDebug && pdis->CtlID == 61004) active = true;
                if (m_currentSidebarView == SidebarView::Extensions && pdis->CtlID == 61005) active = true;

                // Circular background drawing - per "Everything in a circle" requirement
                HBRUSH hbg = CreateSolidBrush(RGB(51, 51, 51));
                FillRect(hdc, &rc, hbg);
                DeleteObject(hbg);

                int margin = 4;
                HBRUSH hCircle = CreateSolidBrush(active ? RGB(0, 122, 204) : (selected ? RGB(80, 80, 80) : RGB(65, 65, 65)));
                HPEN hPen = CreatePen(PS_SOLID, 1, active ? RGB(0, 122, 204) : RGB(90, 90, 90));
                SelectObject(hdc, hCircle);
                SelectObject(hdc, hPen);
                Ellipse(hdc, rc.left + margin, rc.top + margin, rc.right - margin, rc.bottom - margin);
                DeleteObject(hCircle);
                DeleteObject(hPen);

                // Icon drawing (Simplified mapping for production stability)
                const char* icon = " ";
                switch(pdis->CtlID) {
                    case 61001: icon = "E"; break;
                    case 61002: icon = "S"; break;
                    case 61003: icon = "G"; break;
                    case 61004: icon = "D"; break;
                    case 61005: icon = "X"; break;
                }
                
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                DrawTextA(hdc, icon, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
        }
        break;

    case WM_NOTIFY:
        {
            LPNMHDR hdr = (LPNMHDR)lParam;
            if (hdr->hwndFrom == m_hwndOutputTabs && hdr->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(m_hwndOutputTabs);
                const char* keys[] = {"Output","Errors","Debug","Find Results"};
                if (sel >= 0 && sel < 4) {
                    m_activeOutputTab = keys[sel];
                    m_selectedOutputTab = sel;
                    for (auto& kv : m_outputWindows) {
                        ShowWindow(kv.second, kv.first == m_activeOutputTab ? SW_SHOW : SW_HIDE);
                    }
                }
                return 0;
            }
            
            // Handle TreeView notifications
            if (hdr->hwndFrom == m_hwndFileTree) {
                switch (hdr->code) {
                    case TVN_ITEMEXPANDING:
                        {
                            LPNMTREEVIEWA pnmtv = (LPNMTREEVIEWA)lParam;
                            if (pnmtv->action == TVE_EXPAND) {
                                std::string path = getTreeItemPath(pnmtv->itemNew.hItem);
                                if (!path.empty()) {
                                    onFileTreeExpand(pnmtv->itemNew.hItem, path);
                                }
                            }
                        }
                        return 0;
                    
                    case NM_DBLCLK:
                        {
                            HTREEITEM hItem = TreeView_GetSelection(m_hwndFileTree);
                            if (hItem) {
                                std::string path = getTreeItemPath(hItem);
                                if (!path.empty()) {
                                    loadModelFromPath(path);
                                }
                            }
                        }
                        return 0;
                }
            }
        }
        break;

    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            POINT pt = {x, y};
            for (auto& pane : m_terminalPanes) {
                if (PtInRect(&pane.bounds, pt)) {
                    setActiveTerminalPane(pane.id);
                    break;
                }
            }
            if (m_hwndSplitter) {
                RECT splitterRect;
                GetWindowRect(m_hwndSplitter, &splitterRect);
                POINT pt = {x, y};
                ClientToScreen(hwnd, &pt);
                if (PtInRect(&splitterRect, pt)) {
                    m_splitterDragging = true;
                    m_splitterY = y;
                    SetCapture(hwnd);
                    return 0;
                }
            }
        }
        break;

    case WM_LBUTTONUP:
        if (m_splitterDragging) {
            m_splitterDragging = false;
            ReleaseCapture();
            return 0;
        }
        break;

    case WM_MOUSEMOVE:
        {
            int y = HIWORD(lParam);
            if (m_splitterDragging) {
                int delta = y - m_splitterY;
                m_terminalHeight += delta;
                m_outputTabHeight -= delta;
                if (m_terminalHeight < 50) {
                    m_outputTabHeight += (m_terminalHeight - 50);
                    m_terminalHeight = 50;
                }
                if (m_outputTabHeight < 50) {
                    m_terminalHeight += (m_outputTabHeight - 50);
                    m_outputTabHeight = 50;
                }
                m_splitterY = y;
                RECT rect;
                GetClientRect(hwnd, &rect);
                onSize(rect.right, rect.bottom);
                return 0;
            } else if (m_hwndSplitter) {
                RECT splitterRect;
                GetWindowRect(m_hwndSplitter, &splitterRect);
                POINT pt = {LOWORD(lParam), y};
                ClientToScreen(hwnd, &pt);
                if (PtInRect(&splitterRect, pt)) {
                    SetCursor(LoadCursor(NULL, IDC_SIZENS));
                    return 0;
                }
            }
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Fill background with solid color to prevent transparency
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &rect, bgBrush);
            DeleteObject(bgBrush);
            
            EndPaint(hwnd, &ps);
            
            if (m_rendererReady && m_renderer) {
                m_renderer->render();
            }
            LOG_DEBUG("WM_PAINT processed");
            return 0;
        }
        
    case WM_ERASEBKGND:
        {
            // Paint background ourselves to prevent flicker and transparency
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &rect, bgBrush);
            DeleteObject(bgBrush);
            LOG_DEBUG("WM_ERASEBKGND processed");
            return 1;  // Tell Windows we handled it
        }

    case WM_USER + 100:
        // Handle Copilot streaming token updates
        {
            std::string* pToken = reinterpret_cast<std::string*>(wParam);
            if (pToken && m_hwndCopilotChatOutput) {
                LOG_DEBUG("Received streaming token: " + pToken->substr(0, 50));
                
                // Append token to chat output
                int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
                if (len > 0) {
                    SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
                }
                SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)pToken->c_str());
                
                // Auto-scroll to bottom
                SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
                
                delete pToken;  // Clean up dynamically allocated token
            }
        }
        return 0;

    case WM_COMMAND:
        {
            int id = LOWORD(wParam);
            int notifyCode = HIWORD(wParam);

            // Unified command routing via CommandRegistry
            if (routeCommandViaRegistry(id)) {
                return 0;
            }

            // Fallback for control-specific handling
            switch (id) {
                // Copilot controls
                case IDC_COPILOT_SEND_BTN: HandleCopilotSend(); return 0;
                case IDC_COPILOT_CLEAR_BTN: HandleCopilotClear(); return 0;
                
                // Debugger button commands
                case IDC_DEBUGGER_BTN_CONTINUE: resumeExecution(); return 0;
                case IDC_DEBUGGER_BTN_STEP_OVER: stepOverExecution(); return 0;
                case IDC_DEBUGGER_BTN_STEP_INTO: stepIntoExecution(); return 0;
                case IDC_DEBUGGER_BTN_STEP_OUT: stepOutExecution(); return 0;
                case IDC_DEBUGGER_BTN_STOP: stopDebugger(); return 0;
            }
        }
        break;


    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    // Ensure a return value when message was handled but no explicit return above
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// =====================================================================================
// Ollama integration helpers (HTTP POST via WinHTTP)
// =====================================================================================

static std::wstring utf8_to_wide(const std::string& s)
{
    if (s.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &wstr[0], size_needed);
    return wstr;
}

static std::string wide_to_utf8(const std::wstring& ws)
{
    if (ws.empty()) return std::string();
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), NULL, 0, NULL, NULL);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, ws.c_str(), (int)ws.size(), &str[0], size_needed, NULL, NULL);
    return str;
}

// Derive a plausible Ollama model tag from a GGUF filepath (basename without extension)
static std::string derive_model_from_path(const std::string& filepath)
{
    if (filepath.empty()) return std::string();
    size_t slash = filepath.find_last_of("\\/");
    std::string name = (slash == std::string::npos) ? filepath : filepath.substr(slash + 1);
    size_t dot = name.find_last_of('.');
    if (dot != std::string::npos) name = name.substr(0, dot);
    return name; // keep as-is; user models often match basename
}

// Minimal JSON escaper for strings
static std::string json_escape(const std::string& s)
{
    std::string out; out.reserve(s.size() + 16);
    for (char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if ((unsigned char)c < 0x20) {
                    char buf[7];
                    sprintf_s(buf, "\\u%04x", (unsigned char)c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

// POST to http://localhost:11434/api/generate with { model, prompt, stream:false }
bool Win32IDE::trySendToOllama(const std::string& prompt, std::string& outResponse)
{
    HINTERNET hSession = WinHttpOpen(L"RawrXD-IDE/1.0",
                                     WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                     WINHTTP_NO_PROXY_NAME,
                                     WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        appendToOutput("Ollama: WinHttpOpen failed", "Errors", OutputSeverity::Error);
        return false;
    }

    // Parse base URL (supports http/https and optional path prefix)
    std::string base = m_ollamaBaseUrl.empty() ? std::string("http://localhost:11434") : m_ollamaBaseUrl;
    std::wstring wbase = utf8_to_wide(base);
    URL_COMPONENTS uc{}; uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}; wchar_t path[1024]{}; wchar_t scheme[16]{};
    uc.lpszHostName = host; uc.dwHostNameLength = _countof(host);
    uc.lpszUrlPath = path; uc.dwUrlPathLength = _countof(path);
    uc.lpszScheme = scheme; uc.dwSchemeLength = _countof(scheme);
    if (!WinHttpCrackUrl(wbase.c_str(), (DWORD)wbase.size(), 0, &uc)) {
        WinHttpCloseHandle(hSession);
        appendToOutput("Ollama: Invalid base URL", "Errors", OutputSeverity::Error);
        return false;
    }

    INTERNET_PORT port = (uc.nPort != 0) ? uc.nPort : (uc.nScheme == INTERNET_SCHEME_HTTPS ? 443 : 80);
    bool useHttps = (uc.nScheme == INTERNET_SCHEME_HTTPS);

    HINTERNET hConnect = WinHttpConnect(hSession, host, port, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        appendToOutput("Ollama: WinHttpConnect failed", "Errors", OutputSeverity::Error);
        return false;
    }

    std::wstring prefix = path; // may be empty or e.g. "/v1"
    if (!prefix.empty() && prefix.back() == L'/') prefix.pop_back();
    std::wstring requestPath = prefix + L"/api/generate";

    DWORD flags = useHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", requestPath.c_str(),
                                            NULL, WINHTTP_NO_REFERER,
                                            WINHTTP_DEFAULT_ACCEPT_TYPES,
                                            flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        appendToOutput("Ollama: WinHttpOpenRequest failed", "Errors", OutputSeverity::Error);
        return false;
    }

    std::string model = m_ollamaModelOverride.empty() ? derive_model_from_path(m_loadedModelPath) : m_ollamaModelOverride;
    if (model.empty()) model = "llama2";
    std::string body = std::string("{") +
        "\"model\":\"" + json_escape(model) + "\"," +
        "\"prompt\":\"" + json_escape(prompt) + "\"," +
        "\"stream\":false" +
        "}";

    std::wstring headers = L"Content-Type: application/json\r\n";
    BOOL bResults = WinHttpSendRequest(hRequest,
                                       headers.c_str(), (DWORD)headers.length(),
                                       (LPVOID)body.data(), (DWORD)body.size(),
                                       (DWORD)body.size(), 0);
    if (!bResults) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        appendToOutput("Ollama: WinHttpSendRequest failed", "Errors", OutputSeverity::Error);
        return false;
    }

    bResults = WinHttpReceiveResponse(hRequest, NULL);
    if (!bResults) {
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        appendToOutput("Ollama: WinHttpReceiveResponse failed", "Errors", OutputSeverity::Error);
        return false;
    }

    std::string response;
    DWORD dwSize = 0;
    do {
        dwSize = 0;
        if (!WinHttpQueryDataAvailable(hRequest, &dwSize)) break;
        if (dwSize == 0) break;
        std::string chunk(dwSize, '\0');
        DWORD dwDownloaded = 0;
        if (!WinHttpReadData(hRequest, &chunk[0], dwSize, &dwDownloaded)) break;
        chunk.resize(dwDownloaded);
        response += chunk;
    } while (dwSize > 0);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (response.empty()) {
        appendToOutput("Ollama: Empty response", "Errors", OutputSeverity::Error);
        return false;
    }

    // Extract the response content
    std::string extracted;
    const std::string key = "\"response\":\"";
    size_t kpos = response.find(key);
    if (kpos != std::string::npos) {
        size_t s = kpos + key.size();
        std::string buf;
        for (size_t i = s; i < response.size(); ++i) {
            char c = response[i];
            if (c == '\\') { if (i + 1 < response.size()) { buf += response[i + 1]; ++i; } else { buf += c; } }
            else if (c == '"') { break; }
            else { buf += c; }
        }
        extracted = buf;
    }

    outResponse = extracted.empty() ? response : extracted;
    return true;
}

void Win32IDE::onCreate(HWND hwnd)
{
    LOG_INFO("onCreate() started - initializing IDE components");
    
    // Initialize CommandRegistry (Phase-1 integration)
    try {
        CommandRegistry::instance().initialize(hwnd);
        LOG_INFO("CommandRegistry initialized successfully");
    } catch (const std::exception& e) {
        LOG_ERROR("CommandRegistry initialization failed: " + std::string(e.what()));
    }
    
    // Load common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES | ICC_TREEVIEW_CLASSES;
    InitCommonControlsEx(&icex);
    LOG_DEBUG("Common controls initialized");

    // Load Rich Edit
    LoadLibraryA("riched20.dll");
    LOG_DEBUG("RichEdit library loaded");

    LOG_DEBUG("Creating menu bar...");
    createMenuBar(hwnd);
    LOG_DEBUG("Creating toolbar...");
    createToolbar(hwnd);
    LOG_DEBUG("Creating sidebar...");
    createSidebar(hwnd);
    LOG_DEBUG("Creating editor...");
    createEditor(hwnd);
    LOG_DEBUG("Creating terminal...");
    createTerminal(hwnd);
    
    // Create splitter bar between terminal and output
    m_hwndSplitter = CreateWindowExA(0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_NOTIFY,
        0, 0, 100, 4, hwnd, (HMENU)IDC_SPLITTER, m_hInstance, nullptr);
    LOG_DEBUG("Splitter created");
    
    LOG_DEBUG("Creating output tabs...");
    createOutputTabs();
    LOG_DEBUG("Creating minimap...");
    createMinimap();
    LOG_DEBUG("Creating status bar...");
    createStatusBar(hwnd);
    LOG_DEBUG("Creating file explorer...");
    createFileExplorer();
    
    // Create dedicated PowerShell panel (always available)
    LOG_DEBUG("Creating PowerShell panel...");
    createPowerShellPanel();
    
    // Create debugger panel
    LOG_DEBUG("Creating debugger UI...");
    createDebuggerUI();
    
    // Create AI Chat panel (right sidebar)
    LOG_DEBUG("Creating AI Chat panel...");
    createChatPanel();
    
    // Apply theme
    LOG_DEBUG("Applying theme...");
    applyTheme();

    // Set initial layout
    RECT rect;
    GetClientRect(hwnd, &rect);
    LOG_DEBUG("Setting initial layout");
    onSize(rect.right - rect.left, rect.bottom - rect.top);
    updateMenuEnableStates();

    initializeLayoutLive();

    if (m_renderer) {
        LOG_DEBUG("Initializing renderer...");
        m_rendererReady = m_renderer->initialize(hwnd);
        if (m_rendererReady) {
            m_renderer->setClearColor(0.01f, 0.02f, 0.05f, 0.25f);
            m_renderer->render();
            syncEditorToGpuSurface();
            LOG_INFO("Renderer initialized successfully");
        } else {
            LOG_ERROR("Renderer initialization failed");
        }
    } else {
        LOG_ERROR("Renderer is null - cannot initialize");
    }

    // Initialize Agentic Bridge if not already
    if (!m_agenticBridge) {
        initializeAgenticBridge();
    }
    // Initialize Autonomy Manager
    if (!m_autonomyManager) {
        m_autonomyManager = std::make_unique<AutonomyManager>(m_agenticBridge.get());
        LOG_INFO("AutonomyManager instantiated (idle)");
    }

    // Initialize Enterprise License System
    initEnterpriseLicense();
    
    LOG_INFO("onCreate() completed");
}

// duplicate removed

void Win32IDE::onDestroy()
{
    appendToOutput("onDestroy() called - shutting down IDE\\n", "Output", OutputSeverity::Info);
    
    // Shutdown CommandRegistry (Phase-1 integration)
    try {
        // CommandRegistry cleanup if needed - currently a singleton that remains alive
        LOG_INFO("CommandRegistry shutdown");
    } catch (const std::exception& e) {
        LOG_ERROR("CommandRegistry shutdown error: " + std::string(e.what()));
    }
    
    stopTerminal();
    appendToOutput("onDestroy() completed\\n", "Output", OutputSeverity::Info);
}

void Win32IDE::initializeLayoutLive()
{
    LOG_INFO("Initializing layout live updates");
    m_layoutWatchEnabled = true;
    m_layoutWatchTimer = 0;
    m_layoutTargets.clear();

    char modulePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    PathRemoveFileSpecA(modulePath);

    m_layoutLivePath = std::string(modulePath) + "\\layout.live.json";
    registerLayoutTargets();
    exportLayoutSnapshot(m_layoutLivePath);

    WIN32_FILE_ATTRIBUTE_DATA attrs{};
    if (GetFileAttributesExA(m_layoutLivePath.c_str(), GetFileExInfoStandard, &attrs)) {
        m_layoutLastWrite = attrs.ftLastWriteTime;
    }

    m_layoutWatchTimer = SetTimer(m_hwndMain, IDT_LAYOUT_WATCH, 750, nullptr);
    if (m_layoutWatchTimer == 0) {
        LOG_WARNING("Failed to start layout watch timer");
        m_layoutWatchEnabled = false;
    }
}

void Win32IDE::registerLayoutTargets()
{
    m_layoutTargets.clear();
    if (m_hwndActivityBar) m_layoutTargets["activity_bar"] = m_hwndActivityBar;
    if (m_hwndSidebar) m_layoutTargets["primary_sidebar"] = m_hwndSidebar;
    if (m_hwndFileTree) m_layoutTargets["file_tree"] = m_hwndFileTree;
    if (m_hwndEditor) m_layoutTargets["editor"] = m_hwndEditor;
    if (m_hwndMinimap) m_layoutTargets["minimap"] = m_hwndMinimap;
    if (m_hwndPanelContainer) m_layoutTargets["panel_container"] = m_hwndPanelContainer;
    if (m_hwndStatusBar) m_layoutTargets["status_bar"] = m_hwndStatusBar;
    if (m_hwndSecondarySidebar) m_layoutTargets["secondary_sidebar"] = m_hwndSecondarySidebar;
}

void Win32IDE::exportLayoutSnapshot(const std::string& path)
{
    LOG_INFO("Exporting layout snapshot: " + path);
    if (m_layoutTargets.empty()) {
        registerLayoutTargets();
    }

    nlohmann::json data;
    data["version"] = 1;
    data["source"] = "RawrXD-Win32IDE";
    data["nodes"] = nlohmann::json::array();
    data["wires"] = nlohmann::json::array();

    for (const auto& entry : m_layoutTargets) {
        const std::string& id = entry.first;
        HWND hwnd = entry.second;
        if (!IsWindow(hwnd)) continue;

        RECT rect{};
        GetWindowRect(hwnd, &rect);
        MapWindowPoints(nullptr, m_hwndMain, reinterpret_cast<POINT*>(&rect), 2);

        uint64_t caps = 0;
        if (id.find("sidebar") != std::string::npos || id.find("panel") != std::string::npos) caps |= 0x1;
        if (id.find("file") != std::string::npos) caps |= 0x2;
        if (id.find("editor") != std::string::npos) caps |= 0x4;
        if (id.find("status") != std::string::npos || id.find("panel") != std::string::npos) caps |= 0x8;
        if (id.find("activity") != std::string::npos) caps |= 0x10;
        if (id.find("status") != std::string::npos) caps |= 0x20;

        nlohmann::json node;
        node["id"] = id;
        node["label"] = id;
        node["caps"] = caps;
        node["state"] = "pending";
        node["x"] = rect.left;
        node["y"] = rect.top;
        node["w"] = rect.right - rect.left;
        node["h"] = rect.bottom - rect.top;
        node["marks"] = nlohmann::json::array();
        data["nodes"].push_back(node);
    }

    std::ofstream out(path);
    if (!out.is_open()) {
        LOG_ERROR("Failed to write layout snapshot: " + path);
        return;
    }
    out << data.dump(2);
    out.flush();
}

void Win32IDE::applyLayoutOverrides(const std::string& path)
{
    if (path.empty()) return;
    std::ifstream in(path);
    if (!in.is_open()) {
        LOG_WARNING("Layout override not found: " + path);
        return;
    }

    nlohmann::json data;
    try {
        in >> data;
    } catch (const std::exception& ex) {
        LOG_ERROR(std::string("Layout JSON parse error: ") + ex.what());
        return;
    }

    if (!data.contains("nodes")) return;
    for (const auto& node : data["nodes"]) {
        std::string id = node.value("id", "");
        std::string state = node.value("state", "pending");
        if (state == "rejected") continue;

        auto it = m_layoutTargets.find(id);
        if (it == m_layoutTargets.end()) continue;
        HWND hwnd = it->second;
        if (!IsWindow(hwnd)) continue;

        int x = node.value("x", 0);
        int y = node.value("y", 0);
        int w = node.value("w", 0);
        int h = node.value("h", 0);

        SetWindowPos(hwnd, nullptr, x, y, w, h, SWP_NOZORDER | SWP_NOACTIVATE);
    }

    LOG_INFO("Applied layout overrides from " + path);
}

void Win32IDE::checkLayoutLiveUpdates()
{
    if (!m_layoutWatchEnabled || m_layoutLivePath.empty()) return;

    WIN32_FILE_ATTRIBUTE_DATA attrs{};
    if (!GetFileAttributesExA(m_layoutLivePath.c_str(), GetFileExInfoStandard, &attrs)) {
        return;
    }

    if (CompareFileTime(&attrs.ftLastWriteTime, &m_layoutLastWrite) > 0) {
        m_layoutLastWrite = attrs.ftLastWriteTime;
        applyLayoutOverrides(m_layoutLivePath);
    }
}

void Win32IDE::launchSchematicStudio()
{
    // Enterprise license enforcement gate for SchematicStudio IDE
    auto licResult = LicenseEnforcementGate::getInstance().gateSchematicStudio();
    if (!licResult.success) {
        LOG_WARNING("License gate blocked SchematicStudio: " + licResult.detail);
        MessageBoxA(m_hwndMain, (std::string("SchematicStudio requires Enterprise license.\n\n") + licResult.detail).c_str(),
                    "RawrXD License", MB_OK | MB_ICONINFORMATION);
        return;
    }

    exportLayoutSnapshot(m_layoutLivePath);

    char modulePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, modulePath, MAX_PATH);
    PathRemoveFileSpecA(modulePath);

    std::string studioExe = std::string(modulePath) + "\\RawrXD-SchematicStudio.exe";
    std::string cmd = "\"" + studioExe + "\" --input \"" + m_layoutLivePath +
                      "\" --output \"" + m_layoutLivePath + "\" --auto-save --auto-save-ms 500";

    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        LOG_ERROR("Failed to launch Schematic Studio: " + studioExe);
        return;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

void Win32IDE::onSize(int width, int height)
{
    appendToOutput("onSize() called: width=" + std::to_string(width) + " height=" + std::to_string(height) + "\n", 
                   "Debug", OutputSeverity::Debug);
    
    if (!m_hwndToolbar || !m_hwndEditor || m_terminalPanes.empty() || !m_hwndStatusBar) {
        appendToOutput("onSize() - missing critical windows (toolbar/editor/terminals/statusbar)\n", 
                       "Debug", OutputSeverity::Warning);
        return;
    }

    RECT toolbarRect;
    GetWindowRect(m_hwndToolbar, &toolbarRect);
    int toolbarHeight = toolbarRect.bottom - toolbarRect.top;

    RECT statusRect;
    GetWindowRect(m_hwndStatusBar, &statusRect);
    int statusHeight = statusRect.bottom - statusRect.top;

    MoveWindow(m_hwndToolbar, 0, 0, width, toolbarHeight, TRUE);
    layoutTitleBar(width);

    int availableHeight = height - toolbarHeight - statusHeight - 30; // 30 for command input

    // Calculate sidebar offset (Activity Bar + Primary Sidebar if visible)
    int sidebarOffset = 48; // Activity Bar width
    if (m_sidebarVisible) {
        sidebarOffset += m_sidebarWidth;
    }
    
    appendToOutput("onSize() - sidebarOffset=" + std::to_string(sidebarOffset) + 
                   " sidebarVisible=" + (m_sidebarVisible ? "true" : "false") + "\n",
                   "Debug", OutputSeverity::Debug);
    
    // Position Activity Bar and Sidebar
    if (m_hwndActivityBar) {
        MoveWindow(m_hwndActivityBar, 0, toolbarHeight, 48, height - toolbarHeight - statusHeight, TRUE);
    }
    if (m_hwndSidebar && m_sidebarVisible) {
        MoveWindow(m_hwndSidebar, 48, toolbarHeight, m_sidebarWidth, height - toolbarHeight - statusHeight, TRUE);
        resizeSidebar(m_sidebarWidth, height - toolbarHeight - statusHeight);
        
        // Position file explorer within sidebar
        if (m_hwndFileExplorer) {
            MoveWindow(m_hwndFileExplorer, 5, 30, m_sidebarWidth - 10, height - toolbarHeight - statusHeight - 40, TRUE);
        }
    }

    // Position editor (shifted right by sidebar)
    int editorWidth = width - sidebarOffset;
    MoveWindow(m_hwndEditor, sidebarOffset, toolbarHeight, editorWidth, m_editorHeight, TRUE);
    m_editorRect.left = sidebarOffset;
    m_editorRect.top = toolbarHeight;
    m_editorRect.right = sidebarOffset + editorWidth;
    m_editorRect.bottom = toolbarHeight + m_editorHeight;

    layoutTerminalPanes(editorWidth, toolbarHeight + m_editorHeight, m_terminalHeight);

    // Position splitter bar
    int splitterHeight = 4;
    if (m_hwndSplitter && m_outputPanelVisible) {
        int splitterY = toolbarHeight + m_editorHeight + m_terminalHeight;
        MoveWindow(m_hwndSplitter, sidebarOffset, splitterY, editorWidth, splitterHeight, TRUE);
    }

    // Position output tabs area just above command input (if visible)
    int tabBarHeight = 24;
    if (m_hwndOutputTabs && m_outputPanelVisible) {
        int tabsY = toolbarHeight + m_editorHeight + m_terminalHeight + 4;
        MoveWindow(m_hwndOutputTabs, sidebarOffset, tabsY, editorWidth - 150, tabBarHeight, TRUE);
        if (m_hwndSeverityFilter) {
            MoveWindow(m_hwndSeverityFilter, sidebarOffset + editorWidth - 145, tabsY + 2, 140, tabBarHeight - 4, TRUE);
        }
        int editY = tabsY + tabBarHeight;
        int editH = m_outputTabHeight - tabBarHeight - 4;
        for (auto& kv : m_outputWindows) {
            MoveWindow(kv.second, sidebarOffset, editY, editorWidth, editH, TRUE);
        }
    }

    // Position command input
    int outputOffset = m_outputPanelVisible ? m_outputTabHeight : 0;
    int powerShellOffset = (m_powerShellPanelVisible && m_powerShellPanelDocked) ? m_powerShellPanelHeight : 0;
    
    MoveWindow(m_hwndCommandInput, sidebarOffset, toolbarHeight + m_editorHeight + m_terminalHeight + outputOffset,
               editorWidth, 30, TRUE);

    // Position dedicated PowerShell panel (docked at bottom)
    if (m_hwndPowerShellPanel && m_powerShellPanelVisible && m_powerShellPanelDocked) {
        int psTop = height - statusHeight - m_powerShellPanelHeight;
        MoveWindow(m_hwndPowerShellPanel, 0, psTop, width, m_powerShellPanelHeight, TRUE);
        layoutPowerShellPanel();
    }

    // Position status bar (full width)
    MoveWindow(m_hwndStatusBar, 0, height - statusHeight, width, statusHeight, TRUE);

    syncEditorToGpuSurface();

    if (m_rendererReady && m_renderer) {
        UINT w = width > 0 ? static_cast<UINT>(width) : 1u;
        UINT h = height > 0 ? static_cast<UINT>(height) : 1u;
        m_renderer->resize(w, h);
        m_renderer->render();
    }
    
    appendToOutput("onSize() completed - all windows repositioned\\n", "Debug", OutputSeverity::Debug);
}

void Win32IDE::onCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    // Handle severity filter combobox
    if (hwndCtl == m_hwndSeverityFilter && codeNotify == CBN_SELCHANGE) {
        m_severityFilterLevel = SendMessage(m_hwndSeverityFilter, CB_GETCURSEL, 0, 0);
        // Optionally refresh/filter the currently displayed output
        return;
    }
    
    switch (id) {
    case IDM_FILE_NEW:
        newFile();
        break;
    case IDM_FILE_OPEN:
        openFile();
        break;
    case IDM_FILE_LOAD_MODEL: {
        OPENFILENAMEA ofn = {};
        char szFile[260] = {0};
        
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hwnd;
        ofn.lpstrFile = szFile;
        ofn.nMaxFile = sizeof(szFile);
        ofn.lpstrFilter = "GGUF Models\0*.gguf\0All Files\0*.*\0";
        ofn.nFilterIndex = 1;
        ofn.lpstrFileTitle = nullptr;
        ofn.nMaxFileTitle = 0;
        ofn.lpstrInitialDir = nullptr;
        ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
        
        if (GetOpenFileNameA(&ofn)) {
            if (
                
                loadGGUFModel(szFile)) {
                // Model loaded successfully, show info in output
                appendToOutput("\n" + getModelInfo(), "Output", OutputSeverity::Info);
            }
        }
        break;
    }
    case IDM_FILE_SAVE:
        saveFile();
        break;
    case IDM_FILE_SAVEAS:
        saveFileAs();
        break;
    case IDM_FILE_EXIT:
        DestroyWindow(hwnd);
        break;
        
    // Edit menu
    case IDM_EDIT_SNIPPET:
        showSnippetManager();
        break;
    case IDM_EDIT_FIND:
        showFindDialog();
        break;
    case IDM_EDIT_REPLACE:
        showReplaceDialog();
        break;
    case IDM_EDIT_FIND_NEXT:
        findNext();
        break;
    case IDM_EDIT_FIND_PREV:
        findPrevious();
        break;
    case IDM_EDIT_COPY_FORMAT:
        copyWithFormatting();
        break;
    case IDM_EDIT_PASTE_PLAIN:
        pasteWithoutFormatting();
        break;
    case IDM_EDIT_CLIPBOARD_HISTORY:
        showClipboardHistory();
        break;
        
    // View menu
    case IDM_VIEW_MINIMAP:
        toggleMinimap();
        break;
    case IDM_VIEW_OUTPUT_TABS:
        // Toggle output tabs visibility
        break;
    case IDM_VIEW_MODULE_BROWSER:
        showModuleBrowser();
        break;
    case IDM_VIEW_THEME_EDITOR:
        showThemeEditor();
        break;
    case IDM_VIEW_FLOATING_PANEL:
        toggleFloatingPanel();
        break;
    case IDM_VIEW_OUTPUT_PANEL:
        m_outputPanelVisible = !m_outputPanelVisible;
        if (m_hwndOutputTabs) ShowWindow(m_hwndOutputTabs, m_outputPanelVisible ? SW_SHOW : SW_HIDE);
        if (m_hwndSplitter) ShowWindow(m_hwndSplitter, m_outputPanelVisible ? SW_SHOW : SW_HIDE);
        if (m_hwndSeverityFilter) ShowWindow(m_hwndSeverityFilter, m_outputPanelVisible ? SW_SHOW : SW_HIDE);
        for (auto& kv : m_outputWindows) {
            ShowWindow(kv.second, (kv.first == m_activeOutputTab && m_outputPanelVisible) ? SW_SHOW : SW_HIDE);
        }
        RECT rect; GetClientRect(m_hwndMain, &rect);
        onSize(rect.right, rect.bottom);
        break;
    case IDM_VIEW_USE_STREAMING_LOADER:
        {
            m_useStreamingLoader = !m_useStreamingLoader;
            if (m_ggufLoader) {
                m_ggufLoader->Close();
            }
            if (m_useStreamingLoader) {
                m_ggufLoader = std::make_unique<StreamingGGUFLoader>();
                appendToOutput("Switching to Streaming GGUF Loader\n", "Output", OutputSeverity::Info);
            } else {
                m_ggufLoader = std::make_unique<GGUFLoader>();
                appendToOutput("Switching to Standard GGUF Loader\n", "Output", OutputSeverity::Info);
            }
            // Write preference to settings file
            std::ifstream in("ide_settings.ini");
            std::vector<std::string> lines;
            std::string l;
            bool found = false;
            if (in.is_open()) {
                while (std::getline(in, l)) {
                    if (l.rfind("useStreamingLoader=", 0) == 0) {
                        l = std::string("useStreamingLoader=") + (m_useStreamingLoader ? "1" : "0");
                        found = true;
                    }
                    lines.push_back(l);
                }
                in.close();
            }
            if (!found) {
                lines.push_back(std::string("useStreamingLoader=") + (m_useStreamingLoader ? "1" : "0"));
            }
            std::ofstream out("ide_settings.ini");
            if (out.is_open()) {
                for (const auto& ln : lines) out << ln << std::endl;
                out.close();
            }
            // Update menu check
            CheckMenuItem(m_hMenu, IDM_VIEW_USE_STREAMING_LOADER, MF_BYCOMMAND | (m_useStreamingLoader ? MF_CHECKED : MF_UNCHECKED));
        }
        break;
    case IDM_VIEW_USE_VULKAN_RENDERER:
        {
            m_useVulkanRenderer = !m_useVulkanRenderer;
            // Swap renderer only if enabled
            if (m_useVulkanRenderer) {
#ifdef ENABLE_VULKAN
                IRenderer* vkr = CreateVulkanRenderer();
                if (vkr) {
                    m_renderer.reset(vkr);
                    appendToOutput("Vulkan renderer selected (experimental).\n", "Output", OutputSeverity::Info);
                } else {
                    appendToOutput("Vulkan renderer not available. Falling back to D3D\n", "Output", OutputSeverity::Warning);
                    m_useVulkanRenderer = false;
                }
#else
                appendToOutput("Vulkan not compiled in. Rebuild with ENABLE_VULKAN=ON to enable.", "Output", OutputSeverity::Warning);
                m_useVulkanRenderer = false;
#endif
            } else {
                m_renderer = std::make_unique<TransparentRenderer>();
                appendToOutput("Vulkan renderer disabled. Using DirectX renderer.\n", "Output", OutputSeverity::Info);
            }

            // Persist
            std::ifstream in("ide_settings.ini");
            std::vector<std::string> lines;
            std::string l;
            bool found = false;
            if (in.is_open()) {
                while (std::getline(in, l)) {
                    if (l.rfind("useVulkanRenderer=", 0) == 0) {
                        l = std::string("useVulkanRenderer=") + (m_useVulkanRenderer ? "1" : "0");
                        found = true;
                    }
                    lines.push_back(l);
                }
                in.close();
            }
            if (!found) lines.push_back(std::string("useVulkanRenderer=") + (m_useVulkanRenderer ? "1" : "0"));
            std::ofstream out("ide_settings.ini");
            if (out.is_open()) {
                for (const auto& ln : lines) out << ln << std::endl;
            }
            CheckMenuItem(m_hMenu, IDM_VIEW_USE_VULKAN_RENDERER, MF_BYCOMMAND | (m_useVulkanRenderer ? MF_CHECKED : MF_UNCHECKED));
        }
        break;
        
    case IDM_TERMINAL_POWERSHELL:
        startPowerShell();
        break;
    case IDM_TERMINAL_CMD:
        startCommandPrompt();
        break;
    case IDM_TERMINAL_STOP:
        stopTerminal();
        break;
    case IDM_TERMINAL_SPLIT_H:
        splitTerminalHorizontal();
        break;
    case IDM_TERMINAL_SPLIT_V:
        splitTerminalVertical();
        break;
    case IDM_TERMINAL_CLEAR_ALL:
        clearAllTerminals();
        break;
        
    // Tools menu
    case IDM_TOOLS_PROFILE_START:
        startProfiling();
        break;
    case IDM_TOOLS_PROFILE_STOP:
        stopProfiling();
        break;
    case IDM_TOOLS_PROFILE_RESULTS:
        showProfileResults();
        break;
    case IDM_TOOLS_ANALYZE_SCRIPT:
        analyzeScript();
        break;
    case IDM_TOOLS_SCHEMATIC_STUDIO:
        launchSchematicStudio();
        break;
    case IDM_TOOLS_LAYOUT_APPLY:
        applyLayoutOverrides(m_layoutLivePath);
        break;
        
    // Modules menu
    case IDM_MODULES_REFRESH:
        refreshModuleList();
        break;
    case IDM_MODULES_IMPORT:
        importModule();
        break;
    case IDM_MODULES_EXPORT:
        exportModule();
        break;
        
    case IDM_HELP_ABOUT:
        MessageBoxA(hwnd, "RawrXD IDE v2.0\nEnhanced C++ IDE with:\n• Themes & Customization\n• Code Snippets\n• Integrated Help\n• Performance Profiling\n• Module Management\n• Enhanced Output\n• Minimap\n• Clipboard History", "About", MB_OK);
        break;
    case IDM_HELP_CMDREF:
        showCommandReference();
        break;
    case IDM_HELP_PSDOCS:
        showPowerShellDocs();
        break;
    case IDM_HELP_SEARCH:
        searchHelp("");
        break;
    
    // Agent menu
    case IDM_AGENT_START_LOOP:
        onAgentStartLoop();
        break;
    case IDM_AGENT_EXECUTE_CMD:
        onAgentExecuteCommand();
        break;
    case IDM_AGENT_CONFIGURE_MODEL:
        onAgentConfigureModel();
        break;
    case IDM_AGENT_VIEW_TOOLS:
        onAgentViewTools();
        break;
    case IDM_AGENT_VIEW_STATUS:
        onAgentViewStatus();
        break;
    case IDM_AGENT_STOP:
        onAgentStop();
        break;

    // Autonomy menu
    case IDM_AUTONOMY_START:
        onAutonomyStart();
        break;
    case IDM_AUTONOMY_STOP:
        onAutonomyStop();
        break;
    case IDM_AUTONOMY_TOGGLE:
        onAutonomyToggle();
        break;
    case IDM_AUTONOMY_SET_GOAL:
        onAutonomySetGoal();
        break;
    case IDM_AUTONOMY_STATUS:
        onAutonomyViewStatus();
        break;
    case IDM_AUTONOMY_MEMORY:
        onAutonomyViewMemory();
        break;

    // Git menu
    case IDM_GIT_STATUS:
        showGitStatus();
        break;
    case IDM_GIT_COMMIT:
        showCommitDialog();
        break;
    case IDM_GIT_PUSH:
        gitPush();
        break;
    case IDM_GIT_PULL:
        gitPull();
        break;
    case IDM_GIT_PANEL:
        showGitPanel();
        break;

    case IDC_BTN_MINIMIZE:
        ShowWindow(m_hwndMain, SW_MINIMIZE);
        break;
    case IDC_BTN_MAXIMIZE:
        if (IsZoomed(m_hwndMain)) {
            ShowWindow(m_hwndMain, SW_RESTORE);
        } else {
            ShowWindow(m_hwndMain, SW_MAXIMIZE);
        }
        break;
    case IDC_BTN_CLOSE:
        PostMessage(m_hwndMain, WM_CLOSE, 0, 0);
        break;
    case IDC_BTN_GITHUB:
        MessageBoxA(m_hwndMain, "GitHub account options coming soon.", "Account", MB_OK | MB_ICONINFORMATION);
        break;
    case IDC_BTN_MICROSOFT:
        MessageBoxA(m_hwndMain, "Microsoft account options coming soon.", "Account", MB_OK | MB_ICONINFORMATION);
        break;
    case IDC_BTN_SETTINGS:
        showThemeEditor();
        break;
        
    case IDC_COMMAND_INPUT:
        if (codeNotify == EN_CHANGE) {
            // Handle command input changes if needed
        }
        break;
    case IDC_EDITOR:
        if (codeNotify == EN_CHANGE || codeNotify == EN_SELCHANGE) {
            syncEditorToGpuSurface();
        }
        break;
        
    case IDC_SEVERITY_FILTER:
        if (codeNotify == CBN_SELCHANGE) {
            m_severityFilterLevel = SendMessageA(m_hwndSeverityFilter, CB_GETCURSEL, 0, 0);
        }
        break;
    }
    updateMenuEnableStates();
}

void Win32IDE::createMenuBar(HWND hwnd)
{
    LOG_INFO("createMenuBar() called");
    m_hMenu = CreateMenu();
    if (!m_hMenu) {
        LOG_ERROR("Failed to create menu bar");
        return;
    }
    LOG_DEBUG("Menu bar created, populating menu items...");

    // File menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_NEW, "&New");
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_OPEN, "&Open");
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_SAVE, "&Save");
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, "Save &As");
    AppendMenuA(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_LOAD_MODEL, "Load &Model (GGUF)...");
    AppendMenuA(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_EXIT, "E&xit");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "&File");
    
    // Edit menu
    HMENU hEditMenu = CreatePopupMenu();
    AppendMenuA(hEditMenu, MF_STRING, IDM_EDIT_FIND, "&Find...\tCtrl+F");
    AppendMenuA(hEditMenu, MF_STRING, IDM_EDIT_REPLACE, "&Replace...\tCtrl+H");
    AppendMenuA(hEditMenu, MF_STRING, IDM_EDIT_FIND_NEXT, "Find &Next\tF3");
    AppendMenuA(hEditMenu, MF_STRING, IDM_EDIT_FIND_PREV, "Find &Previous\tShift+F3");
    AppendMenuA(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hEditMenu, MF_STRING, IDM_EDIT_SNIPPET, "Insert &Snippet...");
    AppendMenuA(hEditMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hEditMenu, MF_STRING, IDM_EDIT_COPY_FORMAT, "Copy with &Formatting");
    AppendMenuA(hEditMenu, MF_STRING, IDM_EDIT_PASTE_PLAIN, "Paste &Plain Text");
    AppendMenuA(hEditMenu, MF_STRING, IDM_EDIT_CLIPBOARD_HISTORY, "Clipboard &History...");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hEditMenu, "&Edit");
    
    // View menu
    HMENU hViewMenu = CreatePopupMenu();
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_MINIMAP, "&Minimap");
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_OUTPUT_TABS, "&Output Tabs");
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_OUTPUT_PANEL, "Output &Panel");
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_MODULE_BROWSER, "Module &Browser");
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_FLOATING_PANEL, "&Floating Panel");
    AppendMenuA(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_THEME_EDITOR, "&Theme Editor...");
    AppendMenuA(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_USE_STREAMING_LOADER, "Use Streaming Loader (Low Memory)");
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_USE_VULKAN_RENDERER, "Enable Vulkan Renderer (experimental)");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hViewMenu, "&View");

    // Terminal menu
    HMENU hTerminalMenu = CreatePopupMenu();
    AppendMenuA(hTerminalMenu, MF_STRING, IDM_TERMINAL_POWERSHELL, "&PowerShell");
    AppendMenuA(hTerminalMenu, MF_STRING, IDM_TERMINAL_CMD, "&Command Prompt");
    AppendMenuA(hTerminalMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hTerminalMenu, MF_STRING, IDM_TERMINAL_STOP, "&Stop Terminal");
    AppendMenuA(hTerminalMenu, MF_STRING, IDM_TERMINAL_SPLIT_H, "Split &Horizontal\tCtrl+Shift+H");
    AppendMenuA(hTerminalMenu, MF_STRING, IDM_TERMINAL_SPLIT_V, "Split &Vertical\tCtrl+Shift+V");
    AppendMenuA(hTerminalMenu, MF_STRING, IDM_TERMINAL_CLEAR_ALL, "&Clear All Terminals");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hTerminalMenu, "&Terminal");
    
    // Tools menu
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_START, "Start &Profiling");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_STOP, "Stop P&rofiling");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_RESULTS, "Profile &Results...");
    AppendMenuA(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_ANALYZE_SCRIPT, "&Analyze Script");
    AppendMenuA(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_SCHEMATIC_STUDIO, "Layout &Studio...");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_LAYOUT_APPLY, "&Apply Layout Overrides");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hToolsMenu, "&Tools");
    
    // Modules menu
    HMENU hModulesMenu = CreatePopupMenu();
    AppendMenuA(hModulesMenu, MF_STRING, IDM_MODULES_REFRESH, "&Refresh List");
    AppendMenuA(hModulesMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hModulesMenu, MF_STRING, IDM_MODULES_IMPORT, "&Import Module...");
    AppendMenuA(hModulesMenu, MF_STRING, IDM_MODULES_EXPORT, "&Export Module...");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hModulesMenu, "&Modules");

    // Help menu
    HMENU hHelpMenu = CreatePopupMenu();
    AppendMenuA(hHelpMenu, MF_STRING, IDM_HELP_CMDREF, "Command &Reference");
    AppendMenuA(hHelpMenu, MF_STRING, IDM_HELP_PSDOCS, "PowerShell &Documentation");
    AppendMenuA(hHelpMenu, MF_STRING, IDM_HELP_SEARCH, "&Search Help...");
    AppendMenuA(hHelpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hHelpMenu, MF_STRING, IDM_HELP_ABOUT, "&About");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hHelpMenu, "&Help");

    // Git menu
    HMENU hGitMenu = CreatePopupMenu();
    AppendMenuA(hGitMenu, MF_STRING, IDM_GIT_STATUS, "&Status\tCtrl+G");
    AppendMenuA(hGitMenu, MF_STRING, IDM_GIT_COMMIT, "&Commit...\tCtrl+Shift+C");
    AppendMenuA(hGitMenu, MF_STRING, IDM_GIT_PUSH, "&Push");
    AppendMenuA(hGitMenu, MF_STRING, IDM_GIT_PULL, "P&ull");
    AppendMenuA(hGitMenu, MF_STRING, IDM_GIT_PANEL, "&Git Panel\tCtrl+Shift+G");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hGitMenu, "&Git");

    // Agent menu (existing agentic bridge operations)
    HMENU hAgentMenu = CreatePopupMenu();
    AppendMenuA(hAgentMenu, MF_STRING, IDM_AGENT_START_LOOP, "Start &Agent Loop");
    AppendMenuA(hAgentMenu, MF_STRING, IDM_AGENT_EXECUTE_CMD, "&Execute Command...");
    AppendMenuA(hAgentMenu, MF_STRING, IDM_AGENT_CONFIGURE_MODEL, "&Configure Model...");
    AppendMenuA(hAgentMenu, MF_STRING, IDM_AGENT_VIEW_TOOLS, "View &Tools");
    AppendMenuA(hAgentMenu, MF_STRING, IDM_AGENT_VIEW_STATUS, "View &Status");
    AppendMenuA(hAgentMenu, MF_STRING, IDM_AGENT_STOP, "&Stop Agent");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hAgentMenu, "&Agent");

    // Autonomy menu (new high-level autonomous orchestration)
    HMENU hAutonomyMenu = CreatePopupMenu();
    AppendMenuA(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_TOGGLE, "&Toggle Auto Loop");
    AppendMenuA(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_START, "&Start Autonomy");
    AppendMenuA(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_STOP, "Sto&p Autonomy");
    AppendMenuA(hAutonomyMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_SET_GOAL, "Set &Goal...");
    AppendMenuA(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_STATUS, "Show &Status");
    AppendMenuA(hAutonomyMenu, MF_STRING, IDM_AUTONOMY_MEMORY, "Show &Memory Snapshot");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hAutonomyMenu, "&Autonomy");

    SetMenu(hwnd, m_hMenu);
    LOG_INFO("createMenuBar() completed - all menus attached");
}

void Win32IDE::createToolbar(HWND hwnd)
{
    LOG_INFO("createToolbar() called");
    m_hwndToolbar = CreateWindowExA(0, TOOLBARCLASSNAMEA, nullptr,
                                   WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT,
                                   0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);

    if (m_hwndToolbar) {
        LOG_DEBUG("Toolbar created successfully");
        SendMessage(m_hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM)sizeof(TBBUTTON), 0);
        SendMessage(m_hwndToolbar, TB_AUTOSIZE, 0, 0);
        LOG_DEBUG("Creating title bar controls");
        createTitleBarControls();
        updateTitleBarText();
        LOG_INFO("createToolbar() completed");
    } else {
        LOG_ERROR("Failed to create toolbar");
    }
}

void Win32IDE::createTitleBarControls()
{
    DWORD labelStyle = WS_CHILD | WS_VISIBLE | SS_CENTER | SS_NOPREFIX;
    m_hwndTitleLabel = CreateWindowExA(0, "STATIC", "RawrXD IDE", labelStyle,
                                      0, 0, 200, 24, m_hwndToolbar, (HMENU)IDC_TITLE_TEXT, m_hInstance, nullptr);

    DWORD buttonStyle = WS_CHILD | WS_VISIBLE | BS_FLAT;
    auto createButton = [&](HWND& target, int controlId, const char* caption) {
        target = CreateWindowExA(0, "BUTTON", caption, buttonStyle,
                                 0, 0, 32, 24, m_hwndToolbar, (HMENU)controlId, m_hInstance, nullptr);
    };

    createButton(m_hwndBtnGitHub, IDC_BTN_GITHUB, "GH");
    createButton(m_hwndBtnMicrosoft, IDC_BTN_MICROSOFT, "MS");
    createButton(m_hwndBtnSettings, IDC_BTN_SETTINGS, "Gear");
    createButton(m_hwndBtnMinimize, IDC_BTN_MINIMIZE, "-");
    createButton(m_hwndBtnMaximize, IDC_BTN_MAXIMIZE, "[]");
    createButton(m_hwndBtnClose, IDC_BTN_CLOSE, "X");

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
        SetWindowTextA(m_hwndTitleLabel, composed.c_str());
        m_lastTitleBarText = composed;
    }
}

void Win32IDE::createEditor(HWND hwnd)
{
    LOG_INFO("createEditor() called");
    m_hwndEditor = CreateWindowExA(WS_EX_CLIENTEDGE, RICHEDIT_CLASSA, "",
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_WANTRETURN,
                                  0, 0, 0, 0, hwnd, (HMENU)IDC_EDITOR, m_hInstance, nullptr);
    if (!m_hwndEditor) {
        LOG_ERROR("Failed to create editor window");
        return;
    }
    LOG_DEBUG("Editor window created successfully");

    // Set default font and colors
    CHARFORMAT2A cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE | CFM_COLOR;
    cf.yHeight = 200; // 10 points
    cf.crTextColor = RGB(220, 220, 220); // Light gray text
    strcpy(cf.szFaceName, "Consolas");
    SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    
    // Set background color to dark
    SendMessage(m_hwndEditor, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
    LOG_DEBUG("Editor background set to RGB(30,30,30)");
    
    // Enable editing
    SendMessage(m_hwndEditor, EM_SETREADONLY, FALSE, 0);

    LOG_DEBUG("Initializing editor surface");
    initializeEditorSurface();
    LOG_INFO("createEditor() completed");
}

void Win32IDE::createTerminal(HWND hwnd)
{
    LOG_INFO("createTerminal() called");
    if (m_terminalPanes.empty()) {
        LOG_DEBUG("Creating initial PowerShell terminal pane");
        createTerminalPane(Win32TerminalManager::PowerShell, "PowerShell");
    } else {
        LOG_DEBUG("Terminal panes already exist, activating front pane");
        setActiveTerminalPane(m_terminalPanes.front().id);
    }

    // Create command input
    LOG_DEBUG("Creating command input window");
    m_hwndCommandInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                        0, 0, 0, 0, hwnd, (HMENU)IDC_COMMAND_INPUT, m_hInstance, nullptr);
    if (!m_hwndCommandInput) {
        LOG_ERROR("Failed to create command input window");
    } else {
        LOG_DEBUG("Command input window created successfully");
    }
    LOG_INFO("createTerminal() completed");
}

int Win32IDE::createTerminalPane(Win32TerminalManager::ShellType shellType, const std::string& name)
{
    HWND hwnd = CreateWindowExA(WS_EX_CLIENTEDGE, RICHEDIT_CLASSA, "",
                                WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                0, 0, 0, 0, m_hwndMain, nullptr, m_hInstance, nullptr);

    CHARFORMAT2A cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE;
    cf.yHeight = 180; // 9 points
    strcpy(cf.szFaceName, "Consolas");
    SendMessage(hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

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
        onTerminalOutput(paneId, output);
    };
    pane.manager->onError = [this, paneId](const std::string& error) {
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
    if (width <= 0 || height <= 0 || m_terminalPanes.empty()) return;
    int count = static_cast<int>(m_terminalPanes.size());
    if (count == 1) {
        auto& pane = m_terminalPanes[0];
        MoveWindow(pane.hwnd, 0, top, width, height, TRUE);
        pane.bounds = {0, top, width, top + height};
        return;
    }

    if (m_terminalSplitHorizontal) {
        int paneHeight = height / count;
        int y = top;
        for (int i = 0; i < count; ++i) {
            int currentHeight = (i == count - 1) ? (height - paneHeight * (count - 1)) : paneHeight;
            auto& pane = m_terminalPanes[i];
            MoveWindow(pane.hwnd, 0, y, width, currentHeight, TRUE);
            pane.bounds = {0, y, width, y + currentHeight};
            y += currentHeight;
        }
    } else {
        int paneWidth = width / count;
        int x = 0;
        for (int i = 0; i < count; ++i) {
            int currentWidth = (i == count - 1) ? (width - paneWidth * (count - 1)) : paneWidth;
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
    LOG_INFO("createStatusBar() called");
    m_hwndStatusBar = CreateWindowExA(0, STATUSCLASSNAMEA, "",
                                     WS_CHILD | WS_VISIBLE,
                                     0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS_BAR, m_hInstance, nullptr);
    if (!m_hwndStatusBar) {
        LOG_ERROR("Failed to create status bar");
        return;
    }
    LOG_DEBUG("Status bar created successfully");

    int parts[] = {200, 400, -1};
    SendMessage(m_hwndStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Ready");
    LOG_DEBUG("Status bar initialized with 'Ready' text");
    LOG_INFO("createStatusBar() completed");
}

void Win32IDE::createSidebar(HWND hwnd)
{
    // Create the primary sidebar (left panel)
    m_hwndSidebar = CreateWindowExA(
        0,
        "STATIC",
        "Explorer",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        48, 30, m_sidebarWidth, 500,
        hwnd,
        nullptr,
        m_hInstance,
        nullptr
    );
    
    if (m_hwndSidebar) {
        // Create activity bar (icon strip on far left)
        m_hwndActivityBar = CreateWindowExA(
            0,
            "STATIC",
            "",
            WS_CHILD | WS_VISIBLE,
            0, 30, 48, 500,
            hwnd,
            nullptr,
            m_hInstance,
            nullptr
        );
    }
}

void Win32IDE::newFile()
{
    appendToOutput("File > New clicked\n", "Output", OutputSeverity::Info);
    if (m_fileModified) {
        int result = MessageBoxA(m_hwndMain, "File has been modified. Save changes?", "Save", MB_YESNOCANCEL);
        if (result == IDCANCEL) {
            appendToOutput("File > New cancelled by user\n", "Output", OutputSeverity::Info);
            return;
        }
        if (result == IDYES && !saveFile()) {
            appendToOutput("File > New - save failed, operation aborted\n", "Output", OutputSeverity::Warning);
            return;
        }
    }

    SetWindowTextA(m_hwndEditor, "");
    m_currentFile.clear();
    m_fileModified = false;
    updateTitleBarText();
    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"New file");
    updateMenuEnableStates();
    syncEditorToGpuSurface();
    appendToOutput("New file created successfully\n", "Output", OutputSeverity::Info);
}

void Win32IDE::openFile()
{
    appendToOutput("File > Open clicked\n", "Output", OutputSeverity::Info);
    if (m_fileModified) {
        int result = MessageBoxA(m_hwndMain, "File has been modified. Save changes?", "Save", MB_YESNOCANCEL);
        if (result == IDCANCEL) {
            appendToOutput("File > Open cancelled by user\n", "Output", OutputSeverity::Info);
            return;
        }
        if (result == IDYES && !saveFile()) {
            appendToOutput("File > Open - save failed, operation aborted\n", "Output", OutputSeverity::Warning);
            return;
        }
    }

    OPENFILENAMEA ofn;
    char szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0C++ Files\0*.cpp;*.h\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileNameA(&ofn)) {
        appendToOutput("Opening file: " + std::string(szFile) + "\n", "Output", OutputSeverity::Info);
        try {
            std::ifstream file(szFile);
            if (file) {
                std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                SetWindowTextA(m_hwndEditor, content.c_str());
                m_currentFile = szFile;
                m_fileModified = false;
                setCurrentDirectoryFromFile(m_currentFile);
                updateTitleBarText();
                SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File opened");
                updateMenuEnableStates();
                syncEditorToGpuSurface();
                appendToOutput("File opened successfully (" + std::to_string(content.size()) + " bytes)\n", "Output", OutputSeverity::Info);
            } else {
                appendToOutput("Failed to open file: " + std::string(szFile) + "\n", "Errors", OutputSeverity::Error);
                MessageBoxA(m_hwndMain, "Failed to open file", "Error", MB_OK | MB_ICONERROR);
            }
        } catch (const std::exception& e) {
            appendToOutput("Exception opening file: " + std::string(e.what()) + "\n", "Errors", OutputSeverity::Error);
            MessageBoxA(m_hwndMain, e.what(), "Error", MB_OK | MB_ICONERROR);
        }
    } else {
        appendToOutput("File > Open cancelled by user (no file selected)\n", "Output", OutputSeverity::Info);
    }
}

bool Win32IDE::saveFile()
{
    if (m_currentFile.empty()) {
        appendToOutput("File > Save - no current file, showing Save As dialog\n", "Output", OutputSeverity::Info);
        return saveFileAs();
    }

    appendToOutput("Saving file: " + m_currentFile + "\n", "Output", OutputSeverity::Info);
    try {
        std::string content = getWindowText(m_hwndEditor);
        std::ofstream file(m_currentFile);
        if (file) {
            file << content;
            m_fileModified = false;
            updateTitleBarText();
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File saved");
            appendToOutput("File saved successfully (" + std::to_string(content.size()) + " bytes)\n", "Output", OutputSeverity::Info);
            return true;
        }
        appendToOutput("Failed to open file for writing: " + m_currentFile + "\n", "Errors", OutputSeverity::Error);
        MessageBoxA(m_hwndMain, "Failed to save file", "Error", MB_OK | MB_ICONERROR);
    } catch (const std::exception& e) {
        appendToOutput("Exception saving file: " + std::string(e.what()) + "\n", "Errors", OutputSeverity::Error);
        MessageBoxA(m_hwndMain, e.what(), "Error", MB_OK | MB_ICONERROR);
    }
    return false;
}

bool Win32IDE::saveFileAs()
{
    appendToOutput("File > Save As clicked\n", "Output", OutputSeverity::Info);
    OPENFILENAMEA ofn;
    char szFile[260] = {0};

    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = "All Files\0*.*\0C++ Files\0*.cpp;*.h\0";
    ofn.nFilterIndex = 1;
    ofn.lpstrFileTitle = nullptr;
    ofn.nMaxFileTitle = 0;
    ofn.lpstrInitialDir = nullptr;
    ofn.Flags = OFN_OVERWRITEPROMPT;

    if (GetSaveFileNameA(&ofn)) {
        m_currentFile = szFile;
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
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)"PowerShell");
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
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)"CMD");
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
    SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)"Stopped");
    updateMenuEnableStates();
    appendToOutput("Terminal stopped.\n", "Output", OutputSeverity::Info);
}

void Win32IDE::executeCommand()
{
    std::string command = getWindowText(m_hwndCommandInput);
    if (command.empty()) return;

    SetWindowTextA(m_hwndCommandInput, "");
    
    // Check if we're in chat mode with a loaded model
    if (m_chatMode && isModelLoaded()) {
        // Send to model for inference
        appendChatMessage("You", command);
        
        std::string response = sendMessageToModel(command);
        if (!response.empty()) {
            appendChatMessage("Model", response);
        } else {
            appendChatMessage("System", "Error: Model failed to generate response");
        }
        return;
    }
    
    // Check for special commands
    if (command == "/chat" || command == "/model") {
        if (isModelLoaded()) {
            toggleChatMode();
        } else {
            appendToOutput("No model loaded. Please load a .gguf model first using File > Load Model or the File Explorer.", "Output", OutputSeverity::Warning);
        }
        return;
    }
    
    if (command == "/exit-chat") {
        if (m_chatMode) {
            toggleChatMode();
        }
        return;
    }
    
    // Otherwise, send to terminal as before
    TerminalPane* pane = getActiveTerminalPane();
    if (pane && pane->manager && pane->manager->isRunning()) {
        command += "\n";
        pane->manager->writeInput(command);
    }
}

void Win32IDE::onTerminalOutput(int paneId, const std::string& output)
{
    TerminalPane* pane = findTerminalPane(paneId);
    if (!pane || !pane->hwnd) return;
    appendText(pane->hwnd, output);
    appendToOutput(output, "Debug", OutputSeverity::Info);
}

void Win32IDE::onTerminalError(int paneId, const std::string& error)
{
    TerminalPane* pane = findTerminalPane(paneId);
    if (!pane || !pane->hwnd) return;
    appendText(pane->hwnd, error);
    appendToOutput(error, "Errors", OutputSeverity::Error);
}

std::string Win32IDE::getWindowText(HWND hwnd)
{
    int length = GetWindowTextLengthA(hwnd);
    std::string text(length + 1, '\0');
    GetWindowTextA(hwnd, &text[0], length + 1);
    text.resize(length);
    return text;
}

void Win32IDE::setWindowText(HWND hwnd, const std::string& text)
{
    SetWindowTextA(hwnd, text.c_str());
    if (hwnd == m_hwndEditor) {
        syncEditorToGpuSurface();
    }
}

void Win32IDE::appendText(HWND hwnd, const std::string& text)
{
    // Get current text length
    GETTEXTLENGTHEX gtl;
    gtl.flags = GTL_DEFAULT;
    gtl.codepage = CP_ACP;
    LONG length = SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);

    // Set selection to end
    SendMessage(hwnd, EM_SETSEL, length, length);

    // Replace selection with new text
    SETTEXTEX st;
    st.flags = ST_DEFAULT;
    st.codepage = CP_ACP;
    SendMessage(hwnd, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)text.c_str());

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
        MessageBoxA(m_hwndMain, "Theme saved successfully", "Theme Manager", MB_OK);
    }
}

void Win32IDE::applyTheme()
{
    // Apply theme to main editor
    SendMessage(m_hwndEditor, EM_SETBKGNDCOLOR, 0, m_currentTheme.backgroundColor);
    
    // Set text colors
    CHARFORMAT2 cf;
    ZeroMemory(&cf, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = m_currentTheme.textColor;
    SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    
    // Apply to terminal panes
    for (auto& pane : m_terminalPanes) {
        if (!pane.hwnd) continue;
        SendMessage(pane.hwnd, EM_SETBKGNDCOLOR, 0, m_currentTheme.backgroundColor);
        SendMessage(pane.hwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
    }
    
    // Force redraw
    InvalidateRect(m_hwndMain, NULL, TRUE);
    updateMenuEnableStates();
}

void Win32IDE::showThemeEditor()
{
    const char* themes[] = {"Dark", "Light", "Blue", "Green"};
    int result = 0; // Simple selection for now
    
    std::string message = "Select Theme:\n0 - Dark\n1 - Light\n2 - Blue\n3 - Green";
    
    switch (MessageBoxA(m_hwndMain, message.c_str(), "Theme Selection", MB_OKCANCEL)) {
        case IDOK:
            // For simplicity, cycle through predefined themes
            m_currentTheme.backgroundColor = RGB(30, 30, 30);  // Dark
            m_currentTheme.textColor = RGB(220, 220, 220);
            m_currentTheme.selectionColor = RGB(51, 153, 255);
            m_currentTheme.lineNumberColor = RGB(128, 128, 128);
            applyTheme();
            break;
    }
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
            
            // Enhanced placeholder replacement - handle multiple placeholders
            size_t pos = 0;
            int placeholderIndex = 1;
            
            // Replace all placeholders in order
            while ((pos = content.find("${" + std::to_string(placeholderIndex) + ":")) != std::string::npos) {
                size_t endPos = content.find("}", pos);
                if (endPos != std::string::npos) {
                    std::string placeholder = content.substr(pos, endPos - pos + 1);
                    std::string defaultValue = placeholder.substr(placeholder.find(":") + 1);
                    defaultValue = defaultValue.substr(0, defaultValue.length() - 1); // Remove closing brace
                    
                    // For now, just use the default value - in a full implementation,
                    // this would prompt the user for input
                    content.replace(pos, endPos - pos + 1, defaultValue);
                } else {
                    break; // Malformed placeholder
                }
                placeholderIndex++;
            }
            
            // Handle special placeholders
            // ${TM_SELECTED_TEXT} - selected text
            if (start != end) {
                std::string selectedText = getWindowText(m_hwndEditor).substr(start, end - start);
                size_t selPos = content.find("${TM_SELECTED_TEXT}");
                if (selPos != std::string::npos) {
                    content.replace(selPos, 18, selectedText); // 18 = length of "${TM_SELECTED_TEXT}"
                }
            } else {
                // Remove TM_SELECTED_TEXT placeholder if no selection
                size_t selPos = content.find("${TM_SELECTED_TEXT}");
                if (selPos != std::string::npos) {
                    content.replace(selPos, 18, "");
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
        
    MessageBoxA(m_hwndMain, reference.c_str(), "PowerShell Reference", MB_OK);
}

// Output / Clipboard / Minimap / Profiling implementations
void Win32IDE::createOutputTabs()
{
    if (m_hwndOutputTabs) return;

    RECT client{}; GetClientRect(m_hwndMain, &client);
    int tabBarHeight = 24;

    m_hwndOutputTabs = CreateWindowExA(0, WC_TABCONTROLA, "",
        WS_CHILD | WS_VISIBLE | TCS_TABS,
        0, 0, client.right - 150, tabBarHeight,
        m_hwndMain, (HMENU)IDC_OUTPUT_TABS, m_hInstance, nullptr);

    // Add severity filter dropdown
    m_hwndSeverityFilter = CreateWindowExA(0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
        client.right - 145, 2, 140, 100,
        m_hwndMain, (HMENU)IDC_SEVERITY_FILTER, m_hInstance, nullptr);
    SendMessageA(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)"All Messages");
    SendMessageA(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)"Info & Above");
    SendMessageA(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)"Warnings & Errors");
    SendMessageA(m_hwndSeverityFilter, CB_ADDSTRING, 0, (LPARAM)"Errors Only");
    SendMessageA(m_hwndSeverityFilter, CB_SETCURSEL, m_severityFilterLevel, 0);

    struct TabDef { const char* text; int id; const char* key; };
    TabDef defs[] = {
        {"Output", IDC_OUTPUT_EDIT_GENERAL, "Output"},
        {"Errors", IDC_OUTPUT_EDIT_ERRORS,  "Errors"},
        {"Debug",  IDC_OUTPUT_EDIT_DEBUG,   "Debug"},
        {"Find Results", IDC_OUTPUT_EDIT_FIND, "Find Results"}
    };

    for (int i = 0; i < 4; ++i) {
        TCITEMA tie{}; tie.mask = TCIF_TEXT; tie.pszText = const_cast<char*>(defs[i].text);
        TabCtrl_InsertItem(m_hwndOutputTabs, i, &tie);

        HWND hEdit = CreateWindowExA(WS_EX_CLIENTEDGE, RICHEDIT_CLASSA, "",
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
    HWND hEdit = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
        0, tabBarHeight, client.right, m_outputTabHeight - tabBarHeight,
        m_hwndMain, nullptr, m_hInstance, nullptr);
    ShowWindow(hEdit, SW_HIDE);
    m_outputWindows[name] = hEdit;
}

void Win32IDE::appendToOutput(const std::string& text, const std::string& tabName, OutputSeverity severity)
{
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
        SetWindowTextA(it->second, "");
    }
}

void Win32IDE::formatOutput(const std::string& text, COLORREF color, const std::string& tabName)
{ 
    std::string target = tabName.empty() ? m_activeOutputTab : tabName;
    auto it = m_outputWindows.find(target);
    if (it == m_outputWindows.end()) return;
    
    HWND hwnd = it->second;
    GETTEXTLENGTHEX gtl{}; gtl.flags = GTL_DEFAULT; gtl.codepage = CP_ACP;
    LONG len = SendMessage(hwnd, EM_GETTEXTLENGTHEX, (WPARAM)&gtl, 0);
    SendMessage(hwnd, EM_SETSEL, len, len);
    
    CHARFORMAT2A cf{};
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_COLOR;
    cf.crTextColor = color;
    SendMessage(hwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    
    SETTEXTEX st{}; st.flags = ST_SELECTION; st.codepage = CP_ACP;
    SendMessage(hwnd, EM_SETTEXTEX, (WPARAM)&st, (LPARAM)text.c_str());
}

void Win32IDE::copyWithFormatting()
{
    // Simplified: copy selected plain text and store in history (vector<string>)
    CHARRANGE range;
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);
    if (range.cpMax <= range.cpMin) return;
    LONG len = range.cpMax - range.cpMin;
    std::vector<char> buffer(len + 1); TEXTRANGEA tr; tr.chrg = range; tr.lpstrText = buffer.data();
    SendMessage(m_hwndEditor, EM_GETTEXTRANGE, 0, (LPARAM)&tr);
    std::string text(buffer.data());
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
    MessageBoxA(m_hwndMain, msg.c_str(), "Clipboard History", MB_OK);
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
    
    m_hwndMinimap = CreateWindowExA(
        0, "STATIC", "",
        WS_CHILD | WS_VISIBLE | SS_OWNERDRAW,
        minimapX, minimapY, m_minimapWidth, minimapHeight,
        m_hwndMain, nullptr, m_hInstance, nullptr
    );
    
    if (m_hwndMinimap) {
        SetWindowLongPtrA(m_hwndMinimap, GWLP_USERDATA, (LONG_PTR)this);
    }
    
    updateMinimap();
}

void Win32IDE::updateMinimap()
{
    if (!m_hwndMinimap || !m_minimapVisible || !m_hwndEditor) return;
    
    // Get editor content
    int textLen = GetWindowTextLengthA(m_hwndEditor);
    if (textLen == 0) {
        m_minimapLines.clear();
        InvalidateRect(m_hwndMinimap, nullptr, TRUE);
        return;
    }
    
    std::string text(textLen + 1, '\0');
    GetWindowTextA(m_hwndEditor, &text[0], textLen + 1);
    text.resize(textLen);
    
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
    MessageBoxA(m_hwndMain, msg.c_str(), "Profiling", MB_OK);
}

void Win32IDE::analyzeScript()
{
    std::string script = getWindowText(m_hwndEditor);
    size_t lines = std::count(script.begin(), script.end(), '\n') + (script.empty() ? 0 : 1);
    std::string msg = "Script lines: " + std::to_string(lines);
    MessageBoxA(m_hwndMain, msg.c_str(), "Analyze Script", MB_OK);
}

void Win32IDE::measureExecutionTime() { /* reserved */ }

// Module Management - Dynamic PowerShell module discovery
void Win32IDE::refreshModuleList()
{
    LOG_FUNCTION();
    auto start = std::chrono::high_resolution_clock::now();
    
    m_modules.clear();
    
    // Execute PowerShell command to get all available modules
    TerminalPane* pane = getActiveTerminalPane();
    if (pane && pane->manager && pane->manager->isRunning()) {
        // Request module list via Get-Module -ListAvailable
        std::string command = "Get-Module -ListAvailable | Select-Object Name,Version,Description,Path | Format-List\n";
        LOG_INFO("Executing PowerShell module discovery command");
        pane->manager->writeInput(command);
        
        // Give PowerShell time to respond (async - results will come via terminal output)
        appendToOutput("Refreshing module list... (check terminal output)\n", "Output", OutputSeverity::Info);
    } else {
        LOG_WARNING("No active PowerShell session - using default module list");
        
        // Fallback: Add common default modules when PowerShell not available
        m_modules.push_back({"Microsoft.PowerShell.Management","7.0.0.0","Management cmdlets","",true});
        m_modules.push_back({"Microsoft.PowerShell.Utility","7.0.0.0","Utility cmdlets","",true});
        m_modules.push_back({"Microsoft.PowerShell.Security","7.0.0.0","Security cmdlets","",true});
        m_modules.push_back({"PSReadLine","2.3.4","Command line editing and history","",false});
        m_modules.push_back({"PowerShellGet","2.2.5","Package management for PowerShell","",false});
        m_modules.push_back({"PackageManagement","1.4.8.1","Software package management","",false});
        
        appendToOutput("Using default module list (start PowerShell for dynamic discovery)\n", "Output", OutputSeverity::Warning);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    LOG_INFO("Module list refresh completed in " + std::to_string(duration) + "ms");
}

void Win32IDE::showModuleBrowser()
{
    std::string msg = "Modules:\n\n";
    for (auto& m : m_modules) {
        msg += m.name + " (" + m.version + ")" + (m.loaded?" [Loaded]":" [Available]") + "\n";
    }
    MessageBoxA(m_hwndMain, msg.c_str(), "Module Browser", MB_OK);
}

void Win32IDE::loadModule(const std::string& moduleName)
{
    for (auto& m : m_modules) if (m.name == moduleName) m.loaded = true;
}

void Win32IDE::unloadModule(const std::string& moduleName)
{
    for (auto& m : m_modules) if (m.name == moduleName) m.loaded = false;
}

void Win32IDE::importModule()
{
    // Show file dialog to select module
    OPENFILENAMEA ofn = {};
    char szFile[MAX_PATH] = "";
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = m_hwndMain;
    ofn.lpstrFilter = "PowerShell Modules (*.psm1;*.psd1)\0*.psm1;*.psd1\0All Files (*.*)\0*.*\0";
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    ofn.lpstrTitle = "Import Module";
    
    if (GetOpenFileNameA(&ofn)) {
        std::string modulePath = szFile;
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
        MessageBoxA(m_hwndMain, "No modules loaded. Refresh module list first.", "Export Module", MB_OK | MB_ICONINFORMATION);
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
    
    if (MessageBoxA(m_hwndMain, moduleList.c_str(), "Export Module", MB_YESNO | MB_ICONQUESTION) == IDYES) {
        // Find first loaded module
        for (const auto& mod : m_modules) {
            if (mod.loaded) {
                // Show save dialog
                OPENFILENAMEA ofn = {};
                char szFile[MAX_PATH] = "";
                strncpy_s(szFile, (mod.name + ".psm1").c_str(), MAX_PATH);
                
                ofn.lStructSize = sizeof(ofn);
                ofn.hwndOwner = m_hwndMain;
                ofn.lpstrFilter = "PowerShell Module (*.psm1)\0*.psm1\0PowerShell Data (*.psd1)\0*.psd1\0";
                ofn.lpstrFile = szFile;
                ofn.nMaxFile = MAX_PATH;
                ofn.Flags = OFN_OVERWRITEPROMPT;
                ofn.lpstrTitle = "Export Module";
                
                if (GetSaveFileNameA(&ofn)) {
                    std::string savePath = szFile;
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

// ============================================================================
// AI INFERENCE IMPLEMENTATION - Connects GGUF Loader to Chat Panel
// ============================================================================

bool Win32IDE::initializeInference()
{
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    
    // Check if model is loaded
    if (m_loadedModelPath.empty() || !m_ggufLoader) {
        appendToOutput("No model loaded for inference", "Errors", OutputSeverity::Error);
        return false;
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
    if (m_inferenceRunning) {
        return "Inference already in progress. Please wait...";
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

    // Enhanced fallback with local inference capabilities
    std::string modelName = m_loadedModelPath.empty() ? "(no model)" : m_loadedModelPath.substr(m_loadedModelPath.find_last_of("\\/")+1);
    
    // TODO: Phase 2 - Implement IGGUFLoader::isModelLoaded() and generateResponse() methods
    // if (!m_loadedModelPath.empty() && m_ggufLoader && m_ggufLoader->isModelLoaded()) {
    //     try {
    //         std::string localPrompt = "Briefly answer: " + prompt;
    //         std::string localResponse = m_ggufLoader->generateResponse(localPrompt);
    //         if (!localResponse.empty()) {
    //             return "[Local Inference]\n" + localResponse;
    //         }
    //     } catch (const std::exception& e) {
    //         LOG_ERROR("Local inference failed: " + std::string(e.what()));
    //     }
    // }
    
    // Fallback to structured guidance if no local inference available
    std::string guidance = "[AI Assistant Unavailable]\n\n";
    guidance += "Model: " + modelName + "\n";
    guidance += "Prompt: " + prompt + "\n\n";
    guidance += "To enable AI assistance:\n";
    guidance += "1. Load a GGUF model file (File → Load Model)\n";
    guidance += "2. Start Ollama server (" + m_ollamaBaseUrl + ")\n\n";
    guidance += "Suggested actions:\n";
    guidance += "- Check model compatibility\n";
    guidance += "- Verify server connectivity\n";
    guidance += "- Review system requirements\n\n";
    guidance += "[End of Guidance]";
    
    return guidance;
}

void Win32IDE::generateResponseAsync(const std::string& prompt, std::function<void(const std::string&, bool)> callback)
{
    if (m_inferenceRunning) {
        if (callback) callback("Inference already in progress.", true);
        return;
    }
    
    m_inferenceRunning = true;
    m_inferenceStopRequested = false;
    m_currentInferencePrompt = prompt;
    m_inferenceCallback = callback;
    
    m_inferenceThread = std::thread([this]() {
        std::string response = generateResponse(m_currentInferencePrompt);
        m_currentInferenceResponse = response;
        m_inferenceRunning = false;
        
        if (m_inferenceCallback) {
            m_inferenceCallback(response, true);
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
    }
    
    // Add user message
    prompt += "<|user|>\n" + userMessage + "\n<|end|>\n";
    prompt += "<|assistant|>\n";
    
    return prompt;
}

void Win32IDE::onInferenceToken(const std::string& token)
{
    // Called when streaming tokens during inference
    m_currentInferenceResponse += token;
    
    // Update UI with partial response if streaming is enabled
    if (m_inferenceConfig.streamOutput && m_inferenceCallback) {
        m_inferenceCallback(token, false);
    }
}

void Win32IDE::onInferenceComplete(const std::string& fullResponse)
{
    m_inferenceRunning = false;
    m_currentInferenceResponse = fullResponse;
    
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
        "RawrXD Win32 IDE\n\n"
        "Version: 1.0.0\n"
        "Build: " __DATE__ " " __TIME__ "\n\n"
        "Features:\n"
        "• Native Win32 UI\n"
        "• GGUF Model Support\n"
        "• PowerShell Integration\n"
        "• Git Integration\n"
        "• AI Chat via Ollama\n"
        "• Syntax Highlighting\n"
        "• Multi-Terminal Support\n\n"
        "GitHub: ItsMehRAWRXD/RawrXD";
    
    MessageBoxA(m_hwndMain, aboutText.c_str(), "About RawrXD IDE", MB_OK | MB_ICONINFORMATION);
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
    MessageBoxA(m_hwndMain, status.c_str(), "Autonomy Status", MB_OK | MB_ICONINFORMATION);
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
    MessageBoxA(m_hwndMain, report.c_str(), "Autonomy Memory", MB_OK);
}

// ======================================================================
// AI CHAT PANEL IMPLEMENTATION
// ======================================================================

void Win32IDE::createChatPanel() {
    LOG_DEBUG("Creating AI Chat panel...");
    
    if (!m_hwndMain) {
        LOG_ERROR("Cannot create chat panel - main window is null");
        return;
    }

    // Create secondary sidebar container (right side)
    m_hwndSecondarySidebar = CreateWindowExA(
        WS_EX_CLIENTEDGE, "STATIC", "AI Chat",
        WS_CHILD | WS_VISIBLE,
        0, 0, 300, 600,
        m_hwndMain, (HMENU)IDC_SECONDARY_SIDEBAR, m_hInstance, nullptr);
    
    if (!m_hwndSecondarySidebar) {
        LOG_ERROR("Failed to create secondary sidebar");
        return;
    }
    
    // Create header with title
    m_hwndSecondarySidebarHeader = CreateWindowExA(
        0, "STATIC", "AI Chat",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 5, 290, 25,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);
    
    HFONT hFont = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                              DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    if (m_hwndSecondarySidebarHeader) {
        SendMessage(m_hwndSecondarySidebarHeader, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Model Selection Label
    CreateWindowExA(0, "STATIC", "Model:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 35, 50, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);
    
    // Model Selector Combobox
    m_hwndModelSelector = CreateWindowExA(
        0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_AUTOHSCROLL,
        60, 35, 235, 200,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_SEND_BTN, m_hInstance, nullptr);
    
    if (m_hwndModelSelector) {
        SendMessage(m_hwndModelSelector, WM_SETFONT, (WPARAM)hFont, TRUE);
        populateModelSelector();
    }
    
    // Max Tokens Label
    CreateWindowExA(0, "STATIC", "Max Tokens:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 60, 80, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);
    
    // Max Tokens Label (value display)
    m_hwndMaxTokensLabel = CreateWindowExA(0, "STATIC", "512",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        245, 60, 50, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);
    
    // Max Tokens Slider
    m_hwndMaxTokensSlider = CreateWindowExA(
        0, "TRACKBAR_CLASS", "",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        5, 80, 290, 25,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CLEAR_BTN, m_hInstance, nullptr);
    
    if (m_hwndMaxTokensSlider) {
        SendMessage(m_hwndMaxTokensSlider, TBM_SETRANGE, TRUE, MAKELPARAM(32, 2048));
        SendMessage(m_hwndMaxTokensSlider, TBM_SETPOS, TRUE, 512);
        SendMessage(m_hwndMaxTokensSlider, TBM_SETTICFREQ, 256, 0);
        m_currentMaxTokens = 512;
    }
    
    // Chat Output Textbox
    m_hwndCopilotChatOutput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        5, 110, 290, 300,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_OUTPUT, m_hInstance, nullptr);
    
    if (m_hwndCopilotChatOutput) {
        SendMessage(m_hwndCopilotChatOutput, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Chat Input Textbox
    m_hwndCopilotChatInput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
        5, 415, 290, 85,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_INPUT, m_hInstance, nullptr);
    
    if (m_hwndCopilotChatInput) {
        SendMessage(m_hwndCopilotChatInput, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Send Button
    m_hwndCopilotSendBtn = CreateWindowExA(
        0, "BUTTON", "Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, 505, 140, 30,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_SEND_BTN, m_hInstance, nullptr);
    
    if (m_hwndCopilotSendBtn) {
        SendMessage(m_hwndCopilotSendBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Clear Button
    m_hwndCopilotClearBtn = CreateWindowExA(
        0, "BUTTON", "Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 505, 140, 30,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CLEAR_BTN, m_hInstance, nullptr);
    
    if (m_hwndCopilotClearBtn) {
        SendMessage(m_hwndCopilotClearBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    m_secondarySidebarVisible = true;
    m_secondarySidebarWidth = 300;
    LOG_INFO("AI Chat panel created successfully");
}

void Win32IDE::populateModelSelector() {
    if (!m_hwndModelSelector) return;
    
    LOG_DEBUG("Populating model selector...");
    
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
        SendMessageA(m_hwndModelSelector, CB_ADDSTRING, 0, (LPARAM)model.c_str());
    }
    
    // Set first item as selected
    if (!m_availableModels.empty()) {
        SendMessage(m_hwndModelSelector, CB_SETCURSEL, 0, 0);
    }
    
    LOG_DEBUG("Model selector populated with " + std::to_string(m_availableModels.size()) + " models");
}

void Win32IDE::HandleCopilotSend() {
    if (!m_hwndCopilotChatInput || !m_hwndCopilotChatOutput) return;
    
    LOG_DEBUG("Handling Copilot Send button...");
    
    // Get input text
    char inputBuffer[2048] = {0};
    GetWindowTextA(m_hwndCopilotChatInput, inputBuffer, sizeof(inputBuffer) - 1);
    std::string userMessage(inputBuffer);
    
    if (userMessage.empty()) {
        LOG_WARNING("Empty message - ignoring");
        return;
    }
    
    // Get selected model
    int modelIdx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);
    std::string selectedModel = (modelIdx >= 0 && modelIdx < (int)m_availableModels.size()) 
        ? m_availableModels[modelIdx] 
        : "llama2";
    
    LOG_INFO("Sending message to model: " + selectedModel);
    
    // Display user message
    std::string displayText = "\n> User: " + userMessage + "\n";
    
    // Append to output
    int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
    if (len > 0) {
        SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
    }
    SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)displayText.c_str());
    
    // Clear input
    SetWindowTextA(m_hwndCopilotChatInput, "");
    
    // Generate response asynchronously
    auto onResponse = [this](const std::string& response, bool complete) {
        if (!m_hwndCopilotChatOutput) return;
        
        std::string displayResp = "AI: " + response + (complete ? "\n" : "");
        int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
        if (len > 0) {
            SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
        }
        SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)displayResp.c_str());
    };
    
    // Set model override temporarily
    m_ollamaModelOverride = selectedModel;
    
    // Generate response
    generateResponseAsync(userMessage, onResponse);
    
    LOG_DEBUG("Message sent and awaiting response");
}

void Win32IDE::HandleCopilotClear() {
    if (!m_hwndCopilotChatOutput || !m_hwndCopilotChatInput) return;
    
    LOG_DEBUG("Clearing chat panel...");
    
    SetWindowTextA(m_hwndCopilotChatOutput, "Welcome to RawrXD AI Chat!\n\nSelect a model and type your message to begin.");
    SetWindowTextA(m_hwndCopilotChatInput, "");
    m_chatHistory.clear();
    
    LOG_INFO("Chat panel cleared");
}

void Win32IDE::onModelSelectionChanged() {
    int idx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);
    if (idx >= 0 && idx < (int)m_availableModels.size()) {
        m_ollamaModelOverride = m_availableModels[idx];
        LOG_INFO("Model changed to: " + m_ollamaModelOverride);
    }
}

void Win32IDE::onMaxTokensChanged(int newValue) {
    m_currentMaxTokens = newValue;
    m_inferenceConfig.maxTokens = newValue;
    
    // Update label
    if (m_hwndMaxTokensLabel) {
        SetWindowTextA(m_hwndMaxTokensLabel, std::to_string(newValue).c_str());
    }
    
    LOG_DEBUG("Max tokens changed to: " + std::to_string(newValue));
}

void Win32IDE::registerCommands()
{
    // Register all commands with the CommandRegistry
    CommandRegistry& registry = CommandRegistry::instance();
    
    // File menu commands
    registry.registerCommand(IDM_FILE_NEW, "File.New", "New File", "Ctrl+N", "Create a new file", true);
    registry.registerCommand(IDM_FILE_OPEN, "File.Open", "Open File", "Ctrl+O", "Open an existing file", true);
    registry.registerCommand(IDM_FILE_SAVE, "File.Save", "Save File", "Ctrl+S", "Save the current file", true);
    registry.registerCommand(IDM_FILE_SAVEAS, "File.SaveAs", "Save File As", "Ctrl+Shift+S", "Save the current file with a new name", true);
    registry.registerCommand(IDM_FILE_LOAD_MODEL, "File.LoadModel", "Load Model", "Ctrl+Shift+L", "Load a GGUF model", true);
    registry.registerCommand(IDM_FILE_EXIT, "File.Exit", "Exit", "Alt+F4", "Exit the application", true);
    
    // Edit menu commands
    registry.registerCommand(IDM_EDIT_UNDO, "Edit.Undo", "Undo", "Ctrl+Z", "Undo the last action", true);
    registry.registerCommand(IDM_EDIT_REDO, "Edit.Redo", "Redo", "Ctrl+Y", "Redo the last undone action", true);
    registry.registerCommand(IDM_EDIT_CUT, "Edit.Cut", "Cut", "Ctrl+X", "Cut the selected text", true);
    registry.registerCommand(IDM_EDIT_COPY, "Edit.Copy", "Copy", "Ctrl+C", "Copy the selected text", true);
    registry.registerCommand(IDM_EDIT_PASTE, "Edit.Paste", "Paste", "Ctrl+V", "Paste text from clipboard", true);
    registry.registerCommand(IDM_EDIT_FIND, "Edit.Find", "Find", "Ctrl+F", "Find text in the editor", true);
    registry.registerCommand(IDM_EDIT_REPLACE, "Edit.Replace", "Replace", "Ctrl+H", "Replace text in the editor", true);
    
    // View menu commands
    registry.registerCommand(IDM_VIEW_MINIMAP, "View.Minimap", "Toggle Minimap", "Ctrl+M", "Show or hide the minimap", true);
    registry.registerCommand(IDM_VIEW_OUTPUT_TABS, "View.OutputTabs", "Toggle Output Tabs", "Ctrl+T", "Show or hide the output tabs", true);
    registry.registerCommand(IDM_VIEW_MODULE_BROWSER, "View.ModuleBrowser", "Show Module Browser", "Ctrl+Shift+E", "Show the module browser", true);
    registry.registerCommand(IDM_VIEW_THEME_EDITOR, "View.ThemeEditor", "Edit Themes", "Ctrl+K", "Edit the application theme", true);
    registry.registerCommand(IDM_VIEW_FLOATING_PANEL, "View.FloatingPanel", "Toggle Floating Panel", "Ctrl+Shift+P", "Show or hide the floating panel", true);
    registry.registerCommand(IDM_VIEW_OUTPUT_PANEL, "View.OutputPanel", "Toggle Output Panel", "Ctrl+Shift+O", "Show or hide the output panel", true);
    registry.registerCommand(IDM_VIEW_USE_STREAMING_LOADER, "View.UseStreamingLoader", "Use Streaming Loader", "Ctrl+Shift+L", "Enable or disable streaming loader", true);
    registry.registerCommand(IDM_VIEW_USE_VULKAN_RENDERER, "View.UseVulkanRenderer", "Enable Vulkan Renderer", "Ctrl+Shift+V", "Enable or disable Vulkan renderer", true);
    
    // Terminal menu commands
    registry.registerCommand(IDM_TERMINAL_POWERSHELL, "Terminal.NewPowershell", "New PowerShell Tab", "Ctrl+Shift+T", "Open a new PowerShell terminal tab", true);
    registry.registerCommand(IDM_TERMINAL_CMD, "Terminal.NewCmd", "New Command Prompt Tab", "Ctrl+Shift+C", "Open a new Command Prompt terminal tab", true);
    registry.registerCommand(IDM_TERMINAL_STOP, "Terminal.Kill", "Stop Terminal", "Ctrl+Shift+X", "Stop the active terminal", true);
    registry.registerCommand(IDM_TERMINAL_SPLIT_H, "Terminal.SplitHorizontal", "Split Terminal Horizontally", "Ctrl+Shift+H", "Split the terminal pane horizontally", true);
    registry.registerCommand(IDM_TERMINAL_SPLIT_V, "Terminal.SplitVertical", "Split Terminal Vertically", "Ctrl+Shift+V", "Split the terminal pane vertically", true);
    registry.registerCommand(IDM_TERMINAL_CLEAR_ALL, "Terminal.ClearAll", "Clear All Terminals", "Ctrl+Shift+K", "Clear the output of all terminals", true);
    
    // Tools menu commands
    registry.registerCommand(IDM_TOOLS_PROFILE_START, "Tools.ProfileStart", "Start Profiling", "Ctrl+Alt+P", "Start performance profiling", true);
    registry.registerCommand(IDM_TOOLS_PROFILE_STOP, "Tools.ProfileStop", "Stop Profiling", "Ctrl+Alt+S", "Stop performance profiling", true);
    registry.registerCommand(IDM_TOOLS_PROFILE_RESULTS, "Tools.ProfileResults", "Show Profile Results", "Ctrl+Alt+R", "Show the results of performance profiling", true);
    registry.registerCommand(IDM_TOOLS_ANALYZE_SCRIPT, "Tools.AnalyzeScript", "Analyze Script", "Ctrl+Alt+A", "Analyze the current script", true);
    registry.registerCommand(IDM_TOOLS_SCHEMATIC_STUDIO, "Tools.SchematicStudio", "Open Schematic Studio", "Ctrl+Alt+L", "Open the layout in Schematic Studio", true);
    registry.registerCommand(IDM_TOOLS_LAYOUT_APPLY, "Tools.LayoutApply", "Apply Layout Overrides", "Ctrl+Alt+Y", "Apply layout overrides from file", true);
    
    // Modules menu commands
    registry.registerCommand(IDM_MODULES_REFRESH, "Modules.Refresh", "Refresh Module List", "Ctrl+Shift+R", "Refresh the list of available modules", true);
    registry.registerCommand(IDM_MODULES_IMPORT, "Modules.Import", "Import Module", "Ctrl+Shift+I", "Import a PowerShell module", true);
    registry.registerCommand(IDM_MODULES_EXPORT, "Modules.Export", "Export Module", "Ctrl+Shift+E", "Export the current module", true);
    
    // Help menu commands
    registry.registerCommand(IDM_HELP_ABOUT, "Help.About", "About", "F1", "Show information about the IDE", true);
    registry.registerCommand(IDM_HELP_CMDREF, "Help.CommandReference", "Command Reference", "F2", "Show the command reference", true);
    registry.registerCommand(IDM_HELP_PSDOCS, "Help.PowerShellDocs", "PowerShell Documentation", "F3", "Open PowerShell documentation", true);
    registry.registerCommand(IDM_HELP_SEARCH, "Help.Search", "Search Help", "F4", "Search the help system", true);
    
    // Agent menu commands
    registry.registerCommand(IDM_AGENT_START_LOOP, "Agent.StartLoop", "Start Agent Loop", "Ctrl+Alt+A", "Start the agent loop", true);
    registry.registerCommand(IDM_AGENT_EXECUTE_CMD, "Agent.ExecuteCommand", "Execute Command", "Ctrl+Alt+E", "Execute a command in the agent", true);
    registry.registerCommand(IDM_AGENT_CONFIGURE_MODEL, "Agent.ConfigureModel", "Configure Model", "Ctrl+Alt+C", "Configure the agent model", true);
    registry.registerCommand(IDM_AGENT_VIEW_TOOLS, "Agent.ViewTools", "View Tools", "Ctrl+Alt+T", "View the agent tools", true);
    registry.registerCommand(IDM_AGENT_VIEW_STATUS, "Agent.ViewStatus", "View Status", "Ctrl+Alt+S", "View the agent status", true);
    registry.registerCommand(IDM_AGENT_STOP, "Agent.Stop", "Stop Agent", "Ctrl+Alt+X", "Stop the agent", true);

    // Autonomy menu commands
    registry.registerCommand(IDM_AUTONOMY_START, "Autonomy.Start", "Start Autonomy", "Ctrl+Shift+A", "Start the autonomy system", true);
    registry.registerCommand(IDM_AUTONOMY_STOP, "Autonomy.Stop", "Stop Autonomy", "Ctrl+Shift+T", "Stop the autonomy system", true);
    registry.registerCommand(IDM_AUTONOMY_TOGGLE, "Autonomy.Toggle", "Toggle Auto Loop", "Ctrl+Shift+G", "Enable or disable the auto loop", true);
    registry.registerCommand(IDM_AUTONOMY_SET_GOAL, "Autonomy.SetGoal", "Set Goal", "Ctrl+Shift+M", "Set the autonomy goal", true);
    registry.registerCommand(IDM_AUTONOMY_STATUS, "Autonomy.Status", "Show Status", "Ctrl+Shift+H", "Show the autonomy status", true);
    registry.registerCommand(IDM_AUTONOMY_MEMORY, "Autonomy.Memory", "Show Memory", "Ctrl+Shift+J", "Show the autonomy memory snapshot", true);
}

Win32IDE::Win32IDE(HINSTANCE hInstance)
        : m_hInstance(hInstance), m_hwndMain(nullptr), m_hwndEditor(nullptr),
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
      m_moduleListDirty(true), m_backgroundBrush(nullptr), m_editorFont(nullptr),
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
    , m_hwndCommandPalette(nullptr), m_hwndCommandPaletteInput(nullptr), m_hwndCommandPaletteList(nullptr), m_commandPaletteVisible(false)
    , m_hwndModelSelector(nullptr), m_hwndMaxTokensSlider(nullptr), m_hwndMaxTokensLabel(nullptr)
    , m_currentMaxTokens(512)
    , m_layoutLivePath(), m_layoutLastWrite{0, 0}, m_layoutWatchEnabled(false), m_layoutWatchTimer(0)
{
    registerCommands();
    // DIAGNOSTIC: Constructor entry (using cross-platform path resolution)
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\CONSTRUCTOR_START.txt";
        std::ofstream diag(diagPath);
        diag << "Win32IDE constructor entered" << std::endl;
    }

    // Initialize logger ABSOLUTELY FIRST - with fallback error handling
    try {
        std::string logPath = PathResolver::getLogsPath().toStdString() + "\\RawrXD_IDE.log";
        IDELogger::getInstance().initialize(logPath);
        LOG_INFO("=== Win32IDE constructor started ===");
        LOG_INFO("Logger initialized successfully");
    } catch (const std::exception& e) {
        OutputDebugStringA("FATAL: Logger initialization failed: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        // Continue without logging
    } catch (...) {
        OutputDebugStringA("FATAL: Logger initialization failed with unknown exception\n");
    }
    
    // DIAGNOSTIC: After logger section
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_LOGGER.txt";
        std::ofstream diag(diagPath);
        diag << "Logger section skipped" << std::endl;
    }
    
    // Prepare DirectX renderer with safety wrapper
    try {
        LOG_DEBUG("Creating TransparentRenderer...");
        m_renderer = std::make_unique<TransparentRenderer>();
        LOG_INFO("TransparentRenderer created successfully");
    } catch (const std::exception& e) {
        LOG_CRITICAL(std::string("TransparentRenderer creation failed: ") + e.what());
        OutputDebugStringA("ERROR: TransparentRenderer failed: ");
        OutputDebugStringA(e.what());
        OutputDebugStringA("\n");
        m_renderer = nullptr; // Use null renderer
    } catch (...) {
        LOG_CRITICAL("TransparentRenderer creation failed with unknown exception");
        OutputDebugStringA("ERROR: TransparentRenderer failed with unknown exception\n");
        m_renderer = nullptr;
    }
    
    // DIAGNOSTIC: After renderer
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_RENDERER.txt";
        std::ofstream diag(diagPath);
        diag << "Renderer created: " << (m_renderer ? "SUCCESS" : "NULL") << std::endl;
    }
    
    // Initialize PowerShell state with safety
    try {
        LOG_DEBUG("Initializing PowerShell state...");
        initializePowerShellState();
        LOG_INFO("PowerShell state initialized");
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("PowerShell state init failed: ") + e.what());
        OutputDebugStringA("ERROR: PowerShell init failed\n");
    } catch (...) {
        LOG_ERROR("PowerShell state init failed with unknown exception");
        OutputDebugStringA("ERROR: PowerShell init failed\n");
    }
    
    // DIAGNOSTIC: After PowerShell
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_POWERSHELL.txt";
        std::ofstream diag(diagPath);
        diag << "PowerShell state initialized" << std::endl;
    }
    
    // Initialize default theme
    try {
        LOG_DEBUG("Resetting to default theme...");
        resetToDefaultTheme();
        LOG_DEBUG("Theme reset complete");
    } catch (...) {
        LOG_ERROR("Theme reset failed");
        OutputDebugStringA("ERROR: Theme reset failed\n");
    }
    
    // DIAGNOSTIC: After theme
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_THEME.txt";
        std::ofstream diag(diagPath);
        diag << "Theme reset complete" << std::endl;
    }
    
    // Load code snippets
    try {
        LOG_DEBUG("Loading code snippets...");
        loadCodeSnippets();
        LOG_DEBUG("Code snippets loaded");
    } catch (...) {
        LOG_ERROR("Code snippets loading failed");
        OutputDebugStringA("ERROR: Code snippets loading failed\n");
    }
    
    // DIAGNOSTIC: After snippets
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_SNIPPETS.txt";
        std::ofstream diag(diagPath);
        diag << "Code snippets loaded" << std::endl;
    }
    
    // Initialize profiling frequency
    QueryPerformanceFrequency(&m_profilingFreq);
    LOG_DEBUG("Profiling frequency initialized");
    
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
    m_ollamaModelOverride.clear();

    // Initialize settings manager and load persisted preferences
    try {
        SettingsManager& settings = SettingsManager::getInstance();
        settings.initialize();
        LOG_DEBUG("SettingsManager initialized");
        
        // Initialize inference settings manager
        InferenceSettingsManager& inferenceSettings = InferenceSettingsManager::getInstance();
        inferenceSettings.initialize();
        LOG_DEBUG("InferenceSettingsManager initialized");
        
        // Load UI preferences
        settings.loadUILayout(m_outputTabHeight, m_selectedOutputTab, m_terminalHeight, m_outputPanelVisible);
        LOG_DEBUG("UI layout preferences loaded");
        
        // Load filter preferences
        m_severityFilterLevel = settings.loadSeverityFilter();
        LOG_DEBUG("Severity filter loaded");
        
        // Load inference preferences (legacy - now handled by InferenceSettingsManager)
        std::string modelTag, tokenLimit;
        settings.loadInferenceSettings(m_ollamaBaseUrl, modelTag, m_useStreamingLoader, tokenLimit);
        if (!modelTag.empty()) {
            m_ollamaModelOverride = modelTag;
        }
        LOG_DEBUG("Inference settings loaded");
        
        // Load renderer preference
        m_useVulkanRenderer = settings.loadRendererPreference();
        LOG_DEBUG("Renderer preference loaded");
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Settings loading failed: ") + e.what());
        // Continue with defaults
    } catch (...) {
        LOG_ERROR("Settings loading failed with unknown exception");
        // Continue with defaults
    }
    
    // DIAGNOSTIC: After settings load
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_SETTINGS.txt";
        std::ofstream diag(diagPath);
        diag << "Settings loaded from ide_settings.ini" << std::endl;
    }

    // Initialize GGUF loader based on saved preference
    if (m_useStreamingLoader) {
        m_ggufLoader = std::make_unique<StreamingGGUFLoader>();
        // appendToOutput("Using Streaming GGUF Loader (index + zone streaming)\n", "Output", OutputSeverity::Info);  // DISABLED - no windows yet!
    } else {
        m_ggufLoader = std::make_unique<GGUFLoader>();
        // appendToOutput("Using Standard GGUF Loader\n", "Output", OutputSeverity::Info);  // DISABLED - no windows yet!
    }
    
    // DIAGNOSTIC: After GGUF loader
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\AFTER_GGUF.txt";
        std::ofstream diag(diagPath);
        diag << "GGUF loader created: " << (m_useStreamingLoader ? "STREAMING" : "STANDARD") << std::endl;
    }
    
    // Constructor diagnostic
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\IDE_CONSTRUCTOR_COMPLETED.txt";
        std::ofstream constructorDone(diagPath);
        constructorDone << "Win32IDE constructor finished successfully" << std::endl;
        constructorDone.close();
    }
}

void Win32IDE::initializeEditorSurface()
{
    if (!m_hwndEditor || m_editorHooksInstalled) return;

    SendMessage(m_hwndEditor, EM_SETEVENTMASK, 0, ENM_CHANGE | ENM_SELCHANGE);
    SetPropW(m_hwndEditor, kEditorWndProp, this);

    WNDPROC original = reinterpret_cast<WNDPROC>(
        SetWindowLongPtr(m_hwndEditor, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(EditorSubclassProc)));
    SetPropW(m_hwndEditor, kEditorProcProp, reinterpret_cast<HANDLE>(original));

    HideCaret(m_hwndEditor);
    m_editorHooksInstalled = true;

    syncEditorToGpuSurface();
}

void Win32IDE::syncEditorToGpuSurface()
{
    if (!m_gpuTextEnabled || !m_rendererReady || !m_renderer) return;
    if (!m_hwndEditor || !IsWindow(m_hwndEditor)) return;
    if (m_editorRect.right <= m_editorRect.left || m_editorRect.bottom <= m_editorRect.top) return;

    int length = GetWindowTextLengthW(m_hwndEditor);
    std::wstring text;
    if (length > 0) {
        std::wstring buffer(static_cast<size_t>(length) + 1, L'\0');
        int copied = GetWindowTextW(m_hwndEditor, buffer.data(), length + 1);
        if (copied >= 0) {
            buffer.resize(static_cast<size_t>(copied));
            text = std::move(buffer);
        }
    }

    CHARRANGE range{};
    SendMessage(m_hwndEditor, EM_EXGETSEL, 0, (LPARAM)&range);
    size_t caretIndex = static_cast<size_t>(std::max<LONG>(0, range.cpMax));
    if (caretIndex > text.size()) {
        caretIndex = text.size();
    }

    size_t caretLine = 0;
    size_t caretColumn = 0;
    size_t limit = (std::min)(caretIndex, text.size());
    for (size_t i = 0; i < limit; ++i) {
        wchar_t ch = text[i];
        if (ch == L'\r') continue;
        if (ch == L'\n') {
            ++caretLine;
            caretColumn = 0;
        } else {
            ++caretColumn;
        }
    }

    if (!m_renderer) {
        // If user preference requests Vulkan and it's available, attempt to create it
        if (m_useVulkanRenderer) {
#ifdef ENABLE_VULKAN
            IRenderer* vkr = CreateVulkanRenderer();
            if (vkr) m_renderer.reset(vkr);
#endif
        }
        // Default to DirectX/D3D renderer if Vulkan not chosen
        if (!m_renderer) {
            m_renderer = std::make_unique<TransparentRenderer>();
        }
    }
    if (m_renderer) {
        m_renderer->updateEditorText(text, m_editorRect, caretIndex, caretLine, caretColumn);
    }
    InvalidateRect(m_hwndMain, &m_editorRect, FALSE);
}

LRESULT CALLBACK Win32IDE::EditorSubclassProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    auto* ide = reinterpret_cast<Win32IDE*>(GetPropW(hwnd, kEditorWndProp));
    WNDPROC original = reinterpret_cast<WNDPROC>(GetPropW(hwnd, kEditorProcProp));

    switch (uMsg) {
    case WM_PAINT:
    case WM_ERASEBKGND:
        return 0;
    case WM_SETFOCUS:
        HideCaret(hwnd);
        break;
    case WM_DESTROY:
        if (original) {
            SetWindowLongPtr(hwnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(original));
        }
        RemovePropW(hwnd, kEditorWndProp);
        RemovePropW(hwnd, kEditorProcProp);
        if (ide) {
            ide->m_editorHooksInstalled = false;
        }
        break;
    default:
        break;
    }

    if (original) {
        return CallWindowProcW(original, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProcW(hwnd, uMsg, wParam, lParam);
}

Win32IDE::~Win32IDE()
{
    for (auto& pane : m_terminalPanes) {
        if (pane.manager) {
            pane.manager->stop();
        }
    }
    
    // Cleanup theme resources
    if (m_backgroundBrush) {
        DeleteObject(m_backgroundBrush);
    }
    if (m_editorFont) {
        DeleteObject(m_editorFont);
    }
    
    // Save snippets and theme
    saveCodeSnippets();
    saveTheme("current");
    
    // Persist all settings using SettingsManager
    try {
        SettingsManager& settings = SettingsManager::getInstance();
        
        // Save UI layout
        settings.saveUILayout(m_outputTabHeight, m_selectedOutputTab, m_terminalHeight, m_outputPanelVisible);
        
        // Save filter preferences
        settings.saveSeverityFilter(m_severityFilterLevel);
        
        // Save inference settings
        settings.saveInferenceSettings(m_ollamaBaseUrl, m_ollamaModelOverride, 
                                      m_useStreamingLoader, std::to_string(m_currentMaxTokens));
        
        // Save renderer preference
        settings.saveRendererPreference(m_useVulkanRenderer);
        
        // Explicitly save to disk
        settings.save();
        LOG_INFO("All settings saved successfully");
        
    } catch (const std::exception& e) {
        LOG_ERROR(std::string("Failed to save settings: ") + e.what());
    } catch (...) {
        LOG_ERROR("Failed to save settings with unknown exception");
    }
}

bool Win32IDE::createWindow()
{
    // ULTRA-EARLY diagnostic file write (using cross-platform path resolution)
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\createWindow_CALLED.txt";
        std::ofstream diag(diagPath);
        diag << "createWindow() entered" << std::endl;
        diag.close();
    }
    
    LOG_INFO("createWindow() called");
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = "RawrXD_IDE_Class";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = CreateSolidBrush(RGB(30, 30, 30));  // Solid dark background, NOT transparent
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;  // Force full redraw on resize

    if (!RegisterClassA(&wc)) {
        DWORD err = GetLastError();
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\RegisterClass_FAILED.txt";
        std::ofstream diagErr(diagPath);
        diagErr << "RegisterClassA failed with error: " << err << std::endl;
        diagErr.close();
        LOG_ERROR("Failed to register window class");
        return false;
    }
    LOG_DEBUG("Window class registered successfully");

    // NO WS_EX_LAYERED - prevents transparency issues
    m_hwndMain = CreateWindowA("RawrXD_IDE_Class", "RawrXD IDE",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                              CW_USEDEFAULT, CW_USEDEFAULT, 1280, 800,
                              nullptr, nullptr, m_hInstance, this);

    if (!m_hwndMain) {
        DWORD err = GetLastError();
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\CreateWindow_FAILED.txt";
        std::ofstream diagErr(diagPath);
        diagErr << "CreateWindowA failed with error: " << err << std::endl;
        diagErr.close();
        LOG_ERROR("Failed to create main window");
        return false;
    }
    
    {
        std::string diagPath = PathResolver::getDesktopPath().toStdString() + "\\Window_CREATED_SUCCESS.txt";
        std::ofstream diag(diagPath);
        diag << "Window created: HWND = " << (void*)m_hwndMain << std::endl;
        diag.close();
    }
    
    LOG_INFO("Main window created successfully");

    // Center the window on the primary monitor and bring to front
    RECT rc{};
    GetWindowRect(m_hwndMain, &rc);
    int winW = rc.right - rc.left;
    int winH = rc.bottom - rc.top;
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenW - winW) / 2;
    int y = (screenH - winH) / 2;
    if (x < 0) x = 0; if (y < 0) y = 0;
    MoveWindow(m_hwndMain, x, y, winW, winH, FALSE);
    SetForegroundWindow(m_hwndMain);
    return true;
}

void Win32IDE::showWindow()
{
    LOG_INFO("showWindow() called");
    if (m_hwndMain) {
        ShowWindow(m_hwndMain, SW_SHOW);
        UpdateWindow(m_hwndMain);
        SetForegroundWindow(m_hwndMain);
        LOG_DEBUG("Window shown and updated");
    } else {
        LOG_ERROR("Cannot show window - m_hwndMain is null");
    }
}

int Win32IDE::runMessageLoop()
{
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        // Integrated Keyboard Shortcut Handling
        if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN) {
            bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
            bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
            
            // Route through CommandRegistry before standard dispatch
            if (handleKeyboardShortcutViaRegistry((UINT)msg.wParam, ctrl, shift, alt)) {
                continue; 
            }
        }

        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}

LRESULT CALLBACK Win32IDE::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = nullptr;

    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* pCreate = (CREATESTRUCT*)lParam;
        pThis = (Win32IDE*)pCreate->lpCreateParams;
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)pThis);
    } else {
        pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
    }

    if (pThis) {
        return pThis->handleMessage(hwnd, uMsg, wParam, lParam);
    } else {
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT Win32IDE::handleMessage(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg) {
    case WM_CREATE:
        onCreate(hwnd);
        return 0;

    case WM_DESTROY:
        onDestroy();
        PostQuitMessage(0);
        return 0;

    case WM_SIZE:
        onSize(LOWORD(lParam), HIWORD(lParam));
        return 0;

    case WM_TIMER:
        if (wParam == IDT_LAYOUT_WATCH) {
            checkLayoutLiveUpdates();
            return 0;
        }
        break;

    case WM_DRAWITEM:
        {
            LPDRAWITEMSTRUCT pdis = (LPDRAWITEMSTRUCT)lParam;
            // Activity Bar CIRCULAR buttons implementation
            if (pdis->CtlID >= 61001 && pdis->CtlID <= 61005) {
                HDC hdc = pdis->hDC;
                RECT rc = pdis->rcItem;
                bool selected = (pdis->itemState & ODS_SELECTED);
                bool active = false;
                
                if (m_currentSidebarView == SidebarView::Explorer && pdis->CtlID == 61001) active = true;
                if (m_currentSidebarView == SidebarView::Search && pdis->CtlID == 61002) active = true;
                if (m_currentSidebarView == SidebarView::SourceControl && pdis->CtlID == 61003) active = true;
                if (m_currentSidebarView == SidebarView::RunDebug && pdis->CtlID == 61004) active = true;
                if (m_currentSidebarView == SidebarView::Extensions && pdis->CtlID == 61005) active = true;

                // Circular background drawing - per "Everything in a circle" requirement
                HBRUSH hbg = CreateSolidBrush(RGB(51, 51, 51));
                FillRect(hdc, &rc, hbg);
                DeleteObject(hbg);

                int margin = 4;
                HBRUSH hCircle = CreateSolidBrush(active ? RGB(0, 122, 204) : (selected ? RGB(80, 80, 80) : RGB(65, 65, 65)));
                HPEN hPen = CreatePen(PS_SOLID, 1, active ? RGB(0, 122, 204) : RGB(90, 90, 90));
                SelectObject(hdc, hCircle);
                SelectObject(hdc, hPen);
                Ellipse(hdc, rc.left + margin, rc.top + margin, rc.right - margin, rc.bottom - margin);
                DeleteObject(hCircle);
                DeleteObject(hPen);

                // Icon drawing (Simplified mapping for production stability)
                const char* icon = " ";
                switch(pdis->CtlID) {
                    case 61001: icon = "E"; break;
                    case 61002: icon = "S"; break;
                    case 61003: icon = "G"; break;
                    case 61004: icon = "D"; break;
                    case 61005: icon = "X"; break;
                }
                
                SetTextColor(hdc, RGB(255, 255, 255));
                SetBkMode(hdc, TRANSPARENT);
                DrawTextA(hdc, icon, 1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                return TRUE;
            }
        }
        break;

    case WM_NOTIFY:
        {
            LPNMHDR hdr = (LPNMHDR)lParam;
            if (hdr->hwndFrom == m_hwndOutputTabs && hdr->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(m_hwndOutputTabs);
                const char* keys[] = {"Output","Errors","Debug","Find Results"};
                if (sel >= 0 && sel < 4) {
                    m_activeOutputTab = keys[sel];
                    m_selectedOutputTab = sel;
                    for (auto& kv : m_outputWindows) {
                        ShowWindow(kv.second, kv.first == m_activeOutputTab ? SW_SHOW : SW_HIDE);
                    }
                }
                return 0;
            }
            
            // Handle TreeView notifications
            if (hdr->hwndFrom == m_hwndFileTree) {
                switch (hdr->code) {
                    case TVN_ITEMEXPANDING:
                        {
                            LPNMTREEVIEWA pnmtv = (LPNMTREEVIEWA)lParam;
                            if (pnmtv->action == TVE_EXPAND) {
                                std::string path = getTreeItemPath(pnmtv->itemNew.hItem);
                                if (!path.empty()) {
                                    onFileTreeExpand(pnmtv->itemNew.hItem, path);
                                }
                            }
                        }
                        return 0;
                    
                    case NM_DBLCLK:
                        {
                            HTREEITEM hItem = TreeView_GetSelection(m_hwndFileTree);
                            if (hItem) {
                                std::string path = getTreeItemPath(hItem);
                                if (!path.empty()) {
                                    loadModelFromPath(path);
                                }
                            }
                        }
                        return 0;
                }
            }
        }
        break;

    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam), y = HIWORD(lParam);
            POINT pt = {x, y};
            for (auto& pane : m_terminalPanes) {
                if (PtInRect(&pane.bounds, pt)) {
                    setActiveTerminalPane(pane.id);
                    break;
                }
            }
            if (m_hwndSplitter) {
                RECT splitterRect;
                GetWindowRect(m_hwndSplitter, &splitterRect);
                POINT pt = {x, y};
                ClientToScreen(hwnd, &pt);
                if (PtInRect(&splitterRect, pt)) {
                    m_splitterDragging = true;
                    m_splitterY = y;
                    SetCapture(hwnd);
                    return 0;
                }
            }
        }
        break;

    case WM_LBUTTONUP:
        if (m_splitterDragging) {
            m_splitterDragging = false;
            ReleaseCapture();
            return 0;
        }
        break;

    case WM_MOUSEMOVE:
        {
            int y = HIWORD(lParam);
            if (m_splitterDragging) {
                int delta = y - m_splitterY;
                m_terminalHeight += delta;
                m_outputTabHeight -= delta;
                if (m_terminalHeight < 50) {
                    m_outputTabHeight += (m_terminalHeight - 50);
                    m_terminalHeight = 50;
                }
                if (m_outputTabHeight < 50) {
                    m_terminalHeight += (m_outputTabHeight - 50);
                    m_outputTabHeight = 50;
                }
                m_splitterY = y;
                RECT rect;
                GetClientRect(hwnd, &rect);
                onSize(rect.right, rect.bottom);
                return 0;
            } else if (m_hwndSplitter) {
                RECT splitterRect;
                GetWindowRect(m_hwndSplitter, &splitterRect);
                POINT pt = {LOWORD(lParam), y};
                ClientToScreen(hwnd, &pt);
                if (PtInRect(&splitterRect, pt)) {
                    SetCursor(LoadCursor(NULL, IDC_SIZENS));
                    return 0;
                }
            }
        }
        break;

    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            // Fill background with solid color to prevent transparency
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &rect, bgBrush);
            DeleteObject(bgBrush);
            
            EndPaint(hwnd, &ps);
            
            if (m_rendererReady && m_renderer) {
                m_renderer->render();
            }
            LOG_DEBUG("WM_PAINT processed");
            return 0;
        }
        
    case WM_ERASEBKGND:
        {
            // Paint background ourselves to prevent flicker and transparency
            HDC hdc = (HDC)wParam;
            RECT rect;
            GetClientRect(hwnd, &rect);
            HBRUSH bgBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &rect, bgBrush);
            DeleteObject(bgBrush);
            LOG_DEBUG("WM_ERASEBKGND processed");
            return 1;  // Tell Windows we handled it
        }

    case WM_USER + 100:
        // Handle Copilot streaming token updates
        {
            std::string* pToken = reinterpret_cast<std::string*>(wParam);
            if (pToken && m_hwndCopilotChatOutput) {
                LOG_DEBUG("Received streaming token: " + pToken->substr(0, 50));
                
                // Append token to chat output
                int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
                if (len > 0) {
                    SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
                }
                SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)pToken->c_str());
                
                // Auto-scroll to bottom
                SendMessage(m_hwndCopilotChatOutput, EM_SCROLLCARET, 0, 0);
                
                delete pToken;  // Clean up dynamically allocated token
            }
        }
        return 0;

// ============================================================================
// GIT INTEGRATION - Status, Commit, Push, Pull
// ============================================================================

void Win32IDE::showGitStatus()
{
    if (!isGitRepository()) {
        MessageBoxA(m_hwndMain, "Not a Git repository", "Git", MB_OK | MB_ICONWARNING);
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
    
    MessageBoxA(m_hwndMain, status.str().c_str(), "Git Status", MB_OK | MB_ICONINFORMATION);
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
        MessageBoxA(m_hwndMain, "Not a Git repository", "Git Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::string output;
    std::string command = "git commit -m \"" + message + "\"";
    executeGitCommand(command, output);
    
    MessageBoxA(m_hwndMain, output.c_str(), "Git Commit", MB_OK | MB_ICONINFORMATION);
    updateGitStatus();
}

void Win32IDE::gitPush()
{
    if (!isGitRepository()) {
        MessageBoxA(m_hwndMain, "Not a Git repository", "Git Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::string output;
    executeGitCommand("git push", output);
    
    MessageBoxA(m_hwndMain, 
        output.empty() ? "Push completed successfully" : output.c_str(), 
        "Git Push", MB_OK | MB_ICONINFORMATION);
    updateGitStatus();
}

void Win32IDE::gitPull()
{
    if (!isGitRepository()) {
        MessageBoxA(m_hwndMain, "Not a Git repository", "Git Error", MB_OK | MB_ICONERROR);
        return;
    }
    
    std::string output;
    executeGitCommand("git pull", output);
    
    MessageBoxA(m_hwndMain, 
        output.empty() ? "Pull completed successfully" : output.c_str(), 
        "Git Pull", MB_OK | MB_ICONINFORMATION);
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
        MessageBoxA(m_hwndMain, "Not a Git repository", "Git", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Create Git panel if it doesn't exist
    if (!m_hwndGitPanel || !IsWindow(m_hwndGitPanel)) {
        m_hwndGitPanel = CreateWindowExA(WS_EX_TOOLWINDOW, "STATIC", "Git Panel",
            WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_SIZEBOX,
            200, 100, 600, 500, m_hwndMain, nullptr, m_hInstance, nullptr);
        
        // Branch and status info
        m_hwndGitStatusText = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
            WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY,
            10, 10, 580, 60, m_hwndGitPanel, nullptr, m_hInstance, nullptr);
        
        // Changed files list
        CreateWindowExA(0, "STATIC", "Changed Files:", WS_CHILD | WS_VISIBLE,
            10, 80, 120, 20, m_hwndGitPanel, nullptr, m_hInstance, nullptr);
        
        m_hwndGitFileList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
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
        SetWindowTextA(m_hwndGitStatusText, statusText.c_str());
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
            SendMessageA(m_hwndGitFileList, LB_ADDSTRING, 0, (LPARAM)displayText.c_str());
        }
    }
}

void Win32IDE::showCommitDialog()
{
    if (!isGitRepository()) {
        MessageBoxA(m_hwndMain, "Not a Git repository", "Git", MB_OK | MB_ICONWARNING);
        return;
    }
    
    // Simple commit dialog using InputBox-style approach
    HWND hwndDlg = CreateWindowExA(WS_EX_DLGMODALFRAME, "STATIC", "Git Commit",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        150, 150, 500, 200, m_hwndMain, nullptr, m_hInstance, nullptr);
    
    CreateWindowExA(0, "STATIC", "Commit Message:", WS_CHILD | WS_VISIBLE,
        10, 10, 120, 20, hwndDlg, nullptr, m_hInstance, nullptr);
    
    m_hwndCommitDialog = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
        10, 35, 470, 100, hwndDlg, nullptr, m_hInstance, nullptr);
    
    HWND hwndCommitBtn = CreateWindowExA(0, "BUTTON", "Commit", WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON,
        10, 145, 100, 30, hwndDlg, (HMENU)1, m_hInstance, nullptr);
    
    HWND hwndCancelBtn = CreateWindowExA(0, "BUTTON", "Cancel", WS_CHILD | WS_VISIBLE,
        120, 145, 100, 30, hwndDlg, (HMENU)2, m_hInstance, nullptr);
    
    SetFocus(m_hwndCommitDialog);
}

// ============================================================================
// AI INFERENCE IMPLEMENTATION - Connects GGUF Loader to Chat Panel
// ============================================================================

bool Win32IDE::initializeInference()
{
    std::lock_guard<std::mutex> lock(m_inferenceMutex);
    
    // Check if model is loaded
    if (m_loadedModelPath.empty() || !m_ggufLoader) {
        appendToOutput("No model loaded for inference", "Errors", OutputSeverity::Error);
        return false;
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
    if (m_inferenceRunning) {
        return "Inference already in progress. Please wait...";
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

    // Enhanced fallback with local inference capabilities
    std::string modelName = m_loadedModelPath.empty() ? "(no model)" : m_loadedModelPath.substr(m_loadedModelPath.find_last_of("\\/")+1);
    
    // TODO: Phase 2 - Implement IGGUFLoader::isModelLoaded() and generateResponse() methods
    // if (!m_loadedModelPath.empty() && m_ggufLoader && m_ggufLoader->isModelLoaded()) {
    //     try {
    //         std::string localPrompt = "Briefly answer: " + prompt;
    //         std::string localResponse = m_ggufLoader->generateResponse(localPrompt);
    //         if (!localResponse.empty()) {
    //             return "[Local Inference]\n" + localResponse;
    //         }
    //     } catch (const std::exception& e) {
    //         LOG_ERROR("Local inference failed: " + std::string(e.what()));
    //     }
    // }
    
    // Fallback to structured guidance if no local inference available
    std::string guidance = "[AI Assistant Unavailable]\n\n";
    guidance += "Model: " + modelName + "\n";
    guidance += "Prompt: " + prompt + "\n\n";
    guidance += "To enable AI assistance:\n";
    guidance += "1. Load a GGUF model file (File → Load Model)\n";
    guidance += "2. Start Ollama server (" + m_ollamaBaseUrl + ")\n\n";
    guidance += "Suggested actions:\n";
    guidance += "- Check model compatibility\n";
    guidance += "- Verify server connectivity\n";
    guidance += "- Review system requirements\n\n";
    guidance += "[End of Guidance]";
    
    return guidance;
}

void Win32IDE::generateResponseAsync(const std::string& prompt, std::function<void(const std::string&, bool)> callback)
{
    if (m_inferenceRunning) {
        if (callback) callback("Inference already in progress.", true);
        return;
    }
    
    m_inferenceRunning = true;
    m_inferenceStopRequested = false;
    m_currentInferencePrompt = prompt;
    m_inferenceCallback = callback;
    
    m_inferenceThread = std::thread([this]() {
        std::string response = generateResponse(m_currentInferencePrompt);
        m_currentInferenceResponse = response;
        m_inferenceRunning = false;
        
        if (m_inferenceCallback) {
            m_inferenceCallback(response, true);
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
    }
    
    // Add user message
    prompt += "<|user|>\n" + userMessage + "\n<|end|>\n";
    prompt += "<|assistant|>\n";
    
    return prompt;
}

void Win32IDE::onInferenceToken(const std::string& token)
{
    // Called when streaming tokens during inference
    m_currentInferenceResponse += token;
    
    // Update UI with partial response if streaming is enabled
    if (m_inferenceConfig.streamOutput && m_inferenceCallback) {
        m_inferenceCallback(token, false);
    }
}

void Win32IDE::onInferenceComplete(const std::string& fullResponse)
{
    m_inferenceRunning = false;
    m_currentInferenceResponse = fullResponse;
    
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
        "RawrXD Win32 IDE\n\n"
        "Version: 1.0.0\n"
        "Build: " __DATE__ " " __TIME__ "\n\n"
        "Features:\n"
        "• Native Win32 UI\n"
        "• GGUF Model Support\n"
        "• PowerShell Integration\n"
        "• Git Integration\n"
        "• AI Chat via Ollama\n"
        "• Syntax Highlighting\n"
        "• Multi-Terminal Support\n\n"
        "GitHub: ItsMehRAWRXD/RawrXD";
    
    MessageBoxA(m_hwndMain, aboutText.c_str(), "About RawrXD IDE", MB_OK | MB_ICONINFORMATION);
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
    MessageBoxA(m_hwndMain, status.c_str(), "Autonomy Status", MB_OK | MB_ICONINFORMATION);
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
    MessageBoxA(m_hwndMain, report.c_str(), "Autonomy Memory", MB_OK);
}

// ======================================================================
// AI CHAT PANEL IMPLEMENTATION
// ======================================================================

void Win32IDE::createChatPanel() {
    LOG_DEBUG("Creating AI Chat panel...");
    
    if (!m_hwndMain) {
        LOG_ERROR("Cannot create chat panel - main window is null");
        return;
    }

    // Create secondary sidebar container (right side)
    m_hwndSecondarySidebar = CreateWindowExA(
        WS_EX_CLIENTEDGE, "STATIC", "",
        WS_CHILD | WS_VISIBLE,
        0, 0, 300, 600,
        m_hwndMain, (HMENU)IDC_SECONDARY_SIDEBAR, m_hInstance, nullptr);
    
    if (!m_hwndSecondarySidebar) {
        LOG_ERROR("Failed to create secondary sidebar");
        return;
    }
    
    // Create header with title
    m_hwndSecondarySidebarHeader = CreateWindowExA(
        0, "STATIC", "AI Chat",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 5, 290, 25,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);
    
    HFONT hFont = CreateFontA(14, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, ANSI_CHARSET, 
                              OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, 
                              DEFAULT_PITCH | FF_DONTCARE, "Segoe UI");
    if (m_hwndSecondarySidebarHeader) {
        SendMessage(m_hwndSecondarySidebarHeader, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Model Selection Label
    CreateWindowExA(0, "STATIC", "Model:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 35, 50, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);
    
    // Model Selector Combobox
    m_hwndModelSelector = CreateWindowExA(
        0, "COMBOBOX", "",
        WS_CHILD | WS_VISIBLE | CBS_DROPDOWN | CBS_AUTOHSCROLL,
        60, 35, 235, 200,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_SEND_BTN, m_hInstance, nullptr);
    
    if (m_hwndModelSelector) {
        SendMessage(m_hwndModelSelector, WM_SETFONT, (WPARAM)hFont, TRUE);
        populateModelSelector();
    }
    
    // Max Tokens Label
    CreateWindowExA(0, "STATIC", "Max Tokens:",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 60, 80, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);
    
    // Max Tokens Label (value display)
    m_hwndMaxTokensLabel = CreateWindowExA(0, "STATIC", "512",
        WS_CHILD | WS_VISIBLE | SS_RIGHT,
        245, 60, 50, 18,
        m_hwndSecondarySidebar, nullptr, m_hInstance, nullptr);
    
    // Max Tokens Slider
    m_hwndMaxTokensSlider = CreateWindowExA(
        0, "TRACKBAR_CLASS", "",
        WS_CHILD | WS_VISIBLE | TBS_HORZ | TBS_NOTICKS,
        5, 80, 290, 25,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CLEAR_BTN, m_hInstance, nullptr);
    
    if (m_hwndMaxTokensSlider) {
        SendMessage(m_hwndMaxTokensSlider, TBM_SETRANGE, TRUE, MAKELPARAM(32, 2048));
        SendMessage(m_hwndMaxTokensSlider, TBM_SETPOS, TRUE, 512);
        SendMessage(m_hwndMaxTokensSlider, TBM_SETTICFREQ, 256, 0);
        m_currentMaxTokens = 512;
    }
    
    // Chat Output Textbox
    m_hwndCopilotChatOutput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        5, 110, 290, 300,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_OUTPUT, m_hInstance, nullptr);
    
    if (m_hwndCopilotChatOutput) {
        SendMessage(m_hwndCopilotChatOutput, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Chat Input Textbox
    m_hwndCopilotChatInput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL,
        5, 415, 290, 85,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CHAT_INPUT, m_hInstance, nullptr);
    
    if (m_hwndCopilotChatInput) {
        SendMessage(m_hwndCopilotChatInput, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Send Button
    m_hwndCopilotSendBtn = CreateWindowExA(
        0, "BUTTON", "Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, 505, 140, 30,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_SEND_BTN, m_hInstance, nullptr);
    
    if (m_hwndCopilotSendBtn) {
        SendMessage(m_hwndCopilotSendBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    // Clear Button
    m_hwndCopilotClearBtn = CreateWindowExA(
        0, "BUTTON", "Clear",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        150, 505, 140, 30,
        m_hwndSecondarySidebar, (HMENU)IDC_COPILOT_CLEAR_BTN, m_hInstance, nullptr);
    
    if (m_hwndCopilotClearBtn) {
        SendMessage(m_hwndCopilotClearBtn, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
    
    m_secondarySidebarVisible = true;
    m_secondarySidebarWidth = 300;
    LOG_INFO("AI Chat panel created successfully");
}

void Win32IDE::populateModelSelector() {
    if (!m_hwndModelSelector) return;
    
    LOG_DEBUG("Populating model selector...");
    
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
        SendMessageA(m_hwndModelSelector, CB_ADDSTRING, 0, (LPARAM)model.c_str());
    }
    
    // Set first item as selected
    if (!m_availableModels.empty()) {
        SendMessage(m_hwndModelSelector, CB_SETCURSEL, 0, 0);
    }
    
    LOG_DEBUG("Model selector populated with " + std::to_string(m_availableModels.size()) + " models");
}

void Win32IDE::HandleCopilotSend() {
    if (!m_hwndCopilotChatInput || !m_hwndCopilotChatOutput) return;
    
    LOG_DEBUG("Handling Copilot Send button...");
    
    // Get input text
    char inputBuffer[2048] = {0};
    GetWindowTextA(m_hwndCopilotChatInput, inputBuffer, sizeof(inputBuffer) - 1);
    std::string userMessage(inputBuffer);
    
    if (userMessage.empty()) {
        LOG_WARNING("Empty message - ignoring");
        return;
    }
    
    // Get selected model
    int modelIdx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);
    std::string selectedModel = (modelIdx >= 0 && modelIdx < (int)m_availableModels.size()) 
        ? m_availableModels[modelIdx] 
        : "llama2";
    
    LOG_INFO("Sending message to model: " + selectedModel);
    
    // Display user message
    std::string displayText = "\n> User: " + userMessage + "\n";
    
    // Append to output
    int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
    if (len > 0) {
        SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
    }
    SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)displayText.c_str());
    
    // Clear input
    SetWindowTextA(m_hwndCopilotChatInput, "");
    
    // Generate response asynchronously
    auto onResponse = [this](const std::string& response, bool complete) {
        if (!m_hwndCopilotChatOutput) return;
        
        std::string displayResp = "AI: " + response + (complete ? "\n" : "");
        int len = GetWindowTextLengthA(m_hwndCopilotChatOutput);
        if (len > 0) {
            SendMessage(m_hwndCopilotChatOutput, EM_SETSEL, len, len);
        }
        SendMessageA(m_hwndCopilotChatOutput, EM_REPLACESEL, FALSE, (LPARAM)displayResp.c_str());
    };
    
    // Set model override temporarily
    m_ollamaModelOverride = selectedModel;
    
    // Generate response
    generateResponseAsync(userMessage, onResponse);
    
    LOG_DEBUG("Message sent and awaiting response");
}

void Win32IDE::HandleCopilotClear() {
    if (!m_hwndCopilotChatOutput || !m_hwndCopilotChatInput) return;
    
    LOG_DEBUG("Clearing chat panel...");
    
    SetWindowTextA(m_hwndCopilotChatOutput, "Welcome to RawrXD AI Chat!\n\nSelect a model and type your message to begin.");
    SetWindowTextA(m_hwndCopilotChatInput, "");
    m_chatHistory.clear();
    
    LOG_INFO("Chat panel cleared");
}

void Win32IDE::onModelSelectionChanged() {
    int idx = (int)SendMessage(m_hwndModelSelector, CB_GETCURSEL, 0, 0);
    if (idx >= 0 && idx < (int)m_availableModels.size()) {
        m_ollamaModelOverride = m_availableModels[idx];
        LOG_INFO("Model changed to: " + m_ollamaModelOverride);
    }
}

void Win32IDE::onMaxTokensChanged(int newValue) {
    m_currentMaxTokens = newValue;
    m_inferenceConfig.maxTokens = newValue;
    
    // Update label
    if (m_hwndMaxTokensLabel) {
        SetWindowTextA(m_hwndMaxTokensLabel, std::to_string(newValue).c_str());
    }
    
    LOG_DEBUG("Max tokens changed to: " + std::to_string(newValue));
}



// ============================================================================
// Enterprise License System Integration
// ============================================================================
void Win32IDE::initEnterpriseLicense() {
    LOG_INFO("Initializing Enterprise License System...");

    auto& mgr = EnterpriseLicenseManager::getInstance();
    LicenseResult result = mgr.initialize();
    if (!result.success) {
        LOG_ERROR(std::string("License init failed: ") + result.detail);
        return;
    }

    // Try to load saved license from config directory
    std::string licenseFile = "rawrxd.license";
    LicenseResult loadResult = mgr.loadLicense(licenseFile);
    if (loadResult.success) {
        LOG_INFO(std::string("License loaded: ") + mgr.getLicenseSummary());
    } else {
        LOG_INFO("No license file found - running in Community tier");
    }

    // Register callback for tier changes  
    mgr.registerLicenseChangeCallback([this](LicenseTier oldTier, LicenseTier newTier) {
        LOG_INFO(std::string("License tier changed: ") +
                 LicenseTierToString(oldTier) + " -> " + LicenseTierToString(newTier));
        if (m_featureRegistryPanel) {
            m_featureRegistryPanel->refreshLicenseStatus();
        }
    });

    // Log feature audit at startup
    std::string summary = mgr.getLicenseSummary();
    LOG_INFO(summary);

    auto unlocked = mgr.getUnlockedFeatures();
    auto locked = mgr.getLockedFeatures();
    auto missing = mgr.getMissingFeatures();

    LOG_INFO("Features unlocked: " + std::to_string(unlocked.size()) +
             " | locked: " + std::to_string(locked.size()) +
             " | not-implemented: " + std::to_string(missing.size()));

    LOG_INFO("Enterprise License System initialized");
}

void Win32IDE::showFeatureRegistryPanel() {
    if (!m_featureRegistryPanel) {
        m_featureRegistryPanel = std::make_unique<FeatureRegistryPanel>();
        RECT rc;
        GetClientRect(m_hwnd, &rc);
        int panelWidth = 650;
        int panelX = rc.right - panelWidth;
        m_featureRegistryPanel->create(m_hwnd, panelX, 0, panelWidth, rc.bottom);
        m_featureRegistryPanel->onFeatureSelected([this](EnterpriseFeature feature) {
            auto& mgr = EnterpriseLicenseManager::getInstance();
            bool isUnlocked = mgr.isFeatureUnlocked(feature);
            std::string msg = std::string(EnterpriseFeatureToString(feature)) +
                              (isUnlocked ? " [UNLOCKED]" : " [LOCKED - requires " +
                              std::string(LicenseTierToString(mgr.getRequiredTier(feature))) + "]");
            LOG_INFO("Feature selected: " + msg);
        });
    }
    m_featureRegistryPanel->show();
    m_featureRegistryPanel->refreshFeatures();
}

void Win32IDE::hideFeatureRegistryPanel() {
    if (m_featureRegistryPanel) {
        m_featureRegistryPanel->hide();
    }
}
