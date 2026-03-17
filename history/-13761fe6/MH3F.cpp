#include "Win32IDE.h"
#include <commdlg.h>
#include <richedit.h>
#include <commctrl.h>
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <sstream>
#include <ctime>

#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "comctl32.lib")

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

#define IDM_FILE_NEW 2001
#define IDM_FILE_OPEN 2002
#define IDM_FILE_SAVE 2003
#define IDM_FILE_SAVEAS 2004
#define IDM_FILE_EXIT 2005

#define IDM_EDIT_SNIPPET 2010
#define IDM_EDIT_COPY_FORMAT 2011
#define IDM_EDIT_PASTE_PLAIN 2012
#define IDM_EDIT_CLIPBOARD_HISTORY 2013

#define IDM_VIEW_MINIMAP 2020
#define IDM_VIEW_OUTPUT_TABS 2021
#define IDM_VIEW_MODULE_BROWSER 2022
#define IDM_VIEW_THEME_EDITOR 2023
#define IDM_VIEW_FLOATING_PANEL 2024

#define IDM_TERMINAL_POWERSHELL 3001
#define IDM_TERMINAL_CMD 3002
#define IDM_TERMINAL_STOP 3003

#define IDM_TOOLS_PROFILE_START 3010
#define IDM_TOOLS_PROFILE_STOP 3011
#define IDM_TOOLS_PROFILE_RESULTS 3012
#define IDM_TOOLS_ANALYZE_SCRIPT 3013

#define IDM_MODULES_REFRESH 3020
#define IDM_MODULES_IMPORT 3021
#define IDM_MODULES_EXPORT 3022

#define IDM_HELP_ABOUT 4001
#define IDM_HELP_CMDREF 4002
#define IDM_HELP_PSDOCS 4003
#define IDM_HELP_SEARCH 4004

Win32IDE::Win32IDE(HINSTANCE hInstance)
    : m_hInstance(hInstance), m_hwndMain(nullptr), m_hwndEditor(nullptr),
      m_hwndTerminal(nullptr), m_hwndCommandInput(nullptr), m_hwndStatusBar(nullptr),
      m_hwndOutputTabs(nullptr), m_hwndMinimap(nullptr), m_hwndModuleBrowser(nullptr),
      m_hwndHelp(nullptr), m_hwndFloatingPanel(nullptr), m_hwndFloatingContent(nullptr),
      m_hMenu(nullptr), m_hwndToolbar(nullptr), 
      m_fileModified(false), m_editorHeight(400), m_terminalHeight(200),
      m_minimapVisible(true), m_minimapWidth(150), m_profilingActive(false),
      m_moduleListDirty(true), m_backgroundBrush(nullptr), m_editorFont(nullptr),
      m_activeOutputTab("General"), m_minimapX(650), m_outputTabHeight(200),
      m_autoSaveEnabled(false), m_defaultFileExtension("ps1")
{
    // Initialize terminal manager
    m_terminalManager = std::make_unique<Win32TerminalManager>();
    m_terminalManager->onOutput = [this](const std::string& output) { onTerminalOutput(output); };
    m_terminalManager->onError = [this](const std::string& error) { onTerminalError(error); };
    
    // Initialize default theme
    resetToDefaultTheme();
    
    // Load code snippets
    loadCodeSnippets();
    
    // Initialize profiling frequency
    QueryPerformanceFrequency(&m_profilingFreq);
    
    // Initialize clipboard history
    m_clipboardHistory.reserve(MAX_CLIPBOARD_HISTORY);
    
    // Load recent files
    loadRecentFiles();
    
    // Initialize command descriptions
    m_commandDescriptions[1001] = "Create new file";
    m_commandDescriptions[1002] = "Open existing file";
    m_commandDescriptions[1003] = "Save current file";
    m_commandDescriptions[1004] = "Save file with new name";
    m_commandDescriptions[1005] = "Save all open files";
    m_commandDescriptions[2001] = "Undo last action";
    m_commandDescriptions[2002] = "Redo last undone action";
    m_commandDescriptions[2003] = "Cut selection";
    m_commandDescriptions[2004] = "Copy selection";
    m_commandDescriptions[2005] = "Paste from clipboard";
}

Win32IDE::~Win32IDE()
{
    if (m_terminalManager) {
        m_terminalManager->stop();
    }
    
    // Save recent files before exiting
    saveRecentFiles();
    
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
}

bool Win32IDE::createWindow()
{
    WNDCLASSA wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = m_hInstance;
    wc.lpszClassName = "RawrXD_IDE_Class";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    if (!RegisterClassA(&wc)) {
        return false;
    }

    m_hwndMain = CreateWindowA("RawrXD_IDE_Class", "RawrXD IDE",
                              WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                              CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
                              nullptr, nullptr, m_hInstance, this);

    return m_hwndMain != nullptr;
}

void Win32IDE::showWindow()
{
    if (m_hwndMain) {
        ShowWindow(m_hwndMain, SW_SHOW);
        UpdateWindow(m_hwndMain);
    }
}

int Win32IDE::runMessageLoop()
{
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
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

    case WM_KEYDOWN:
        if (wParam == VK_RETURN && GetFocus() == m_hwndCommandInput) {
            executeCommand();
            return 0;
        }
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    
    return 0;
}

void Win32IDE::onCreate(HWND hwnd)
{
    // Load common controls
    INITCOMMONCONTROLSEX icex;
    icex.dwSize = sizeof(INITCOMMONCONTROLSEX);
    icex.dwICC = ICC_BAR_CLASSES | ICC_WIN95_CLASSES;
    InitCommonControlsEx(&icex);

    // Load Rich Edit
    LoadLibraryA("riched20.dll");

    createMenuBar(hwnd);
    createToolbar(hwnd);
    createEditor(hwnd);
    createTerminal(hwnd);
    createOutputTabs();
    createMinimap();
    createStatusBar(hwnd);
    
    // Apply theme
    applyTheme();

    // Set initial layout
    RECT rect;
    GetClientRect(hwnd, &rect);
    onSize(rect.right - rect.left, rect.bottom - rect.top);
}

void Win32IDE::onDestroy()
{
    stopTerminal();
}

void Win32IDE::onSize(int width, int height)
{
    if (!m_hwndToolbar || !m_hwndEditor || !m_hwndTerminal || !m_hwndStatusBar) return;

    RECT toolbarRect;
    GetWindowRect(m_hwndToolbar, &toolbarRect);
    int toolbarHeight = toolbarRect.bottom - toolbarRect.top;

    RECT statusRect;
    GetWindowRect(m_hwndStatusBar, &statusRect);
    int statusHeight = statusRect.bottom - statusRect.top;

    int availableHeight = height - toolbarHeight - statusHeight - 30; // 30 for command input

    // Position editor
    MoveWindow(m_hwndEditor, 0, toolbarHeight, width, m_editorHeight, TRUE);

    // Position terminal
    MoveWindow(m_hwndTerminal, 0, toolbarHeight + m_editorHeight, width, m_terminalHeight, TRUE);

    // Position command input
    MoveWindow(m_hwndCommandInput, 0, toolbarHeight + m_editorHeight + m_terminalHeight,
               width, 30, TRUE);

    // Position status bar
    MoveWindow(m_hwndStatusBar, 0, height - statusHeight, width, statusHeight, TRUE);
}

void Win32IDE::onCommand(HWND hwnd, int id, HWND hwndCtl, UINT codeNotify)
{
    switch (id) {
    case IDM_FILE_NEW:
        newFile();
        break;
    case IDM_FILE_OPEN:
        openFile();
        break;
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
        
    case IDM_TERMINAL_POWERSHELL:
        startPowerShell();
        break;
    case IDM_TERMINAL_CMD:
        startCommandPrompt();
        break;
    case IDM_TERMINAL_STOP:
        stopTerminal();
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
        
    case IDC_COMMAND_INPUT:
        if (codeNotify == EN_CHANGE) {
            // Handle command input changes if needed
        }
        break;
    }
}

void Win32IDE::createMenuBar(HWND hwnd)
{
    m_hMenu = CreateMenu();

    // File menu
    HMENU hFileMenu = CreatePopupMenu();
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_NEW, "&New");
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_OPEN, "&Open");
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_SAVE, "&Save");
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_SAVEAS, "Save &As");
    AppendMenuA(hFileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hFileMenu, MF_STRING, IDM_FILE_EXIT, "E&xit");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hFileMenu, "&File");
    
    // Edit menu
    HMENU hEditMenu = CreatePopupMenu();
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
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_MODULE_BROWSER, "Module &Browser");
    AppendMenuA(hViewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_FLOATING_PANEL, "&Floating Panel");
    AppendMenuA(hViewMenu, MF_STRING, IDM_VIEW_THEME_EDITOR, "&Theme Editor...");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hViewMenu, "&View");

    // Terminal menu
    HMENU hTerminalMenu = CreatePopupMenu();
    AppendMenuA(hTerminalMenu, MF_STRING, IDM_TERMINAL_POWERSHELL, "&PowerShell");
    AppendMenuA(hTerminalMenu, MF_STRING, IDM_TERMINAL_CMD, "&Command Prompt");
    AppendMenuA(hTerminalMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hTerminalMenu, MF_STRING, IDM_TERMINAL_STOP, "&Stop Terminal");
    AppendMenuA(m_hMenu, MF_POPUP, (UINT_PTR)hTerminalMenu, "&Terminal");
    
    // Tools menu
    HMENU hToolsMenu = CreatePopupMenu();
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_START, "Start &Profiling");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_STOP, "Stop P&rofiling");
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_PROFILE_RESULTS, "Profile &Results...");
    AppendMenuA(hToolsMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(hToolsMenu, MF_STRING, IDM_TOOLS_ANALYZE_SCRIPT, "&Analyze Script");
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

    SetMenu(hwnd, m_hMenu);
}

void Win32IDE::createToolbar(HWND hwnd)
{
    m_hwndToolbar = CreateWindowExA(0, TOOLBARCLASSNAMEA, nullptr,
                                   WS_CHILD | WS_VISIBLE | TBSTYLE_FLAT,
                                   0, 0, 0, 0, hwnd, nullptr, m_hInstance, nullptr);

    // Add buttons if needed
}

void Win32IDE::createEditor(HWND hwnd)
{
    m_hwndEditor = CreateWindowExA(WS_EX_CLIENTEDGE, RICHEDIT_CLASSA, "",
                                  WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL,
                                  0, 0, 0, 0, hwnd, (HMENU)IDC_EDITOR, m_hInstance, nullptr);

    // Set default font
    CHARFORMAT2A cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE;
    cf.yHeight = 200; // 10 points
    strcpy(cf.szFaceName, "Consolas");
    SendMessage(m_hwndEditor, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
}

void Win32IDE::createTerminal(HWND hwnd)
{
    m_hwndTerminal = CreateWindowExA(WS_EX_CLIENTEDGE, RICHEDIT_CLASSA, "",
                                    WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                    0, 0, 0, 0, hwnd, (HMENU)IDC_TERMINAL, m_hInstance, nullptr);

    // Set font
    CHARFORMAT2A cf;
    memset(&cf, 0, sizeof(cf));
    cf.cbSize = sizeof(cf);
    cf.dwMask = CFM_FACE | CFM_SIZE;
    cf.yHeight = 180; // 9 points
    strcpy(cf.szFaceName, "Consolas");
    SendMessage(m_hwndTerminal, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);

    // Create command input
    m_hwndCommandInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
                                        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
                                        0, 0, 0, 0, hwnd, (HMENU)IDC_COMMAND_INPUT, m_hInstance, nullptr);
}

void Win32IDE::createStatusBar(HWND hwnd)
{
    m_hwndStatusBar = CreateWindowExA(0, STATUSCLASSNAMEA, "",
                                     WS_CHILD | WS_VISIBLE,
                                     0, 0, 0, 0, hwnd, (HMENU)IDC_STATUS_BAR, m_hInstance, nullptr);

    int parts[] = {200, 400, -1};
    SendMessage(m_hwndStatusBar, SB_SETPARTS, 3, (LPARAM)parts);
    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"Ready");
}

void Win32IDE::newFile()
{
    if (m_fileModified) {
        int result = MessageBoxA(m_hwndMain, "File has been modified. Save changes?", "Save", MB_YESNOCANCEL);
        if (result == IDCANCEL) return;
        if (result == IDYES && !saveFile()) return;
    }

    SetWindowTextA(m_hwndEditor, "");
    m_currentFile.clear();
    m_fileModified = false;
    SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"New file");
}

void Win32IDE::openFile()
{
    if (m_fileModified) {
        int result = MessageBoxA(m_hwndMain, "File has been modified. Save changes?", "Save", MB_YESNOCANCEL);
        if (result == IDCANCEL) return;
        if (result == IDYES && !saveFile()) return;
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
        std::ifstream file(szFile);
        if (file) {
            std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
            SetWindowTextA(m_hwndEditor, content.c_str());
            m_currentFile = szFile;
            m_fileModified = false;
            SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File opened");
        }
    }
}

bool Win32IDE::saveFile()
{
    if (m_currentFile.empty()) {
        return saveFileAs();
    }

    std::string content = getWindowText(m_hwndEditor);
    std::ofstream file(m_currentFile);
    if (file) {
        file << content;
        m_fileModified = false;
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 0, (LPARAM)"File saved");
        return true;
    }
    return false;
}

bool Win32IDE::saveFileAs()
{
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
        return saveFile();
    }
    return false;
}

void Win32IDE::startPowerShell()
{
    stopTerminal();
    if (m_terminalManager->start(Win32TerminalManager::PowerShell)) {
        appendText(m_hwndTerminal, "PowerShell started...\n");
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)"PowerShell");
    }
}

void Win32IDE::startCommandPrompt()
{
    stopTerminal();
    if (m_terminalManager->start(Win32TerminalManager::CommandPrompt)) {
        appendText(m_hwndTerminal, "Command Prompt started...\n");
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)"CMD");
    }
}

void Win32IDE::stopTerminal()
{
    if (m_terminalManager->isRunning()) {
        m_terminalManager->stop();
        appendText(m_hwndTerminal, "\nTerminal stopped.\n");
        SendMessage(m_hwndStatusBar, SB_SETTEXT, 1, (LPARAM)"Stopped");
    }
}

void Win32IDE::executeCommand()
{
    std::string command = getWindowText(m_hwndCommandInput);
    if (!command.empty() && m_terminalManager->isRunning()) {
        command += "\n";
        m_terminalManager->writeInput(command);
        SetWindowTextA(m_hwndCommandInput, "");
    }
}

void Win32IDE::onTerminalOutput(const std::string& output)
{
    appendText(m_hwndTerminal, output);
}

void Win32IDE::onTerminalError(const std::string& error)
{
    appendText(m_hwndTerminal, error);
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
}
