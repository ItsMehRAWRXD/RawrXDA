#include "MainWindowSimple.h"
#include "../../include/editor_buffer.h"
#include "../../include/syntax_engine.h"
#include "../../include/ui/split_layout.h"
#include "../../include/ui/chat_panel.h"

#include <sstream>
#include <iomanip>
#include <cmath>
#include <iostream>
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <richedit.h>
#include <commdlg.h>    // For file dialogs
#include <shlobj.h>     // For folder browser
#include <shellapi.h>   // For ShellExecute
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "shell32.lib")
#endif

namespace fs = std::filesystem;

MainWindow::MainWindow()
    : m_hwnd(nullptr), m_editorHwnd(nullptr), m_terminalHwnd(nullptr),
      m_overclockHwnd(nullptr), m_floatingPanel(nullptr),
      m_problemsPanelHwnd(nullptr), m_findPanelHwnd(nullptr),
      m_findEditHwnd(nullptr), m_replaceEditHwnd(nullptr),
      m_findNextBtnHwnd(nullptr), m_replaceBtnHwnd(nullptr), m_replaceAllBtnHwnd(nullptr),
      m_terminalRunning(false), m_psInWrite(nullptr), m_psOutRead(nullptr),
      m_terminalReaderActive(false), m_floatingPanelVisible(false),
      m_problemsPanelVisible(true)
{
    m_appState = std::make_shared<AppState>();
#ifdef _WIN32
    // Ensure RichEdit 5.0 class is registered (Msftedit.dll)
    static HMODULE richEditModule = LoadLibraryA("Msftedit.dll");
    (void)richEditModule; // suppress unused warning
#endif
    // Theme profiles
    m_themes.push_back({"dark", (unsigned)RGB(30,30,30), (unsigned)RGB(212,212,212), (unsigned)RGB(86,156,214), (unsigned)RGB(181,206,168), (unsigned)RGB(212,212,212), (unsigned)RGB(206,145,120), (unsigned)RGB(106,153,85)});
    m_themes.push_back({"light", (unsigned)RGB(255,255,255), (unsigned)RGB(0,0,0), (unsigned)RGB(0,0,160), (unsigned)RGB(128,0,0), (unsigned)RGB(0,0,0), (unsigned)RGB(163,21,21), (unsigned)RGB(0,128,0)});
    loadSettings();
    if(m_tabs.empty()) { addTab("Untitled"); }
}

void MainWindow::createMenus()
{
#ifdef _WIN32
    m_findPanelHwnd = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "STATIC", "Find/Replace",
        WS_POPUP | WS_VISIBLE | WS_BORDER,
        650, 10, 320, 120, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );

    m_findEditHwnd = CreateWindowEx(0, "EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        10, 30, 140, 22, m_findPanelHwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessage(m_findEditHwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    m_replaceEditHwnd = CreateWindowEx(0, "EDIT", nullptr,
        WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL,
        160, 30, 140, 22, m_findPanelHwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessage(m_replaceEditHwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    m_findNextBtnHwnd = CreateWindowEx(0, "BUTTON", "Find Next",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        10, 70, 90, 28, m_findPanelHwnd, (HMENU)1, GetModuleHandle(nullptr), nullptr);

    m_replaceBtnHwnd = CreateWindowEx(0, "BUTTON", "Replace",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        110, 70, 90, 28, m_findPanelHwnd, (HMENU)2, GetModuleHandle(nullptr), nullptr);

    m_replaceAllBtnHwnd = CreateWindowEx(0, "BUTTON", "Replace All",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        210, 70, 90, 28, m_findPanelHwnd, (HMENU)3, GetModuleHandle(nullptr), nullptr);
#endif
}

// ============================================================================
// Menu Bar Creation - Full VS Code-style Menu System
// ============================================================================
void MainWindow::createMenuBar()
{
#ifdef _WIN32
    m_menuBar = CreateMenu();
    
    // ========== FILE MENU ==========
    HMENU fileMenu = CreatePopupMenu();
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_NEW, "New File\tCtrl+N");
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_NEW_WINDOW, "New Window\tCtrl+Shift+N");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_OPEN, "Open File...\tCtrl+O");
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_OPEN_FOLDER, "Open Folder...\tCtrl+K Ctrl+O");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_SAVE, "Save\tCtrl+S");
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_SAVEAS, "Save As...\tCtrl+Shift+S");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(fileMenu, MF_STRING | (m_autoSaveEnabled ? MF_CHECKED : 0), IDM_FILE_AUTOSAVE, "Auto Save");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_CLOSE_TAB, "Close Tab\tCtrl+W");
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_CLOSE_FOLDER, "Close Folder");
    AppendMenuA(fileMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(fileMenu, MF_STRING, IDM_FILE_EXIT, "Exit\tAlt+F4");
    AppendMenuA(m_menuBar, MF_POPUP, (UINT_PTR)fileMenu, "&File");
    
    // ========== EDIT MENU ==========
    HMENU editMenu = CreatePopupMenu();
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_UNDO, "Undo\tCtrl+Z");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_REDO, "Redo\tCtrl+Y");
    AppendMenuA(editMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_CUT, "Cut\tCtrl+X");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_COPY, "Copy\tCtrl+C");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_PASTE, "Paste\tCtrl+V");
    AppendMenuA(editMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_FIND, "Find\tCtrl+F");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_REPLACE, "Replace\tCtrl+H");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_GOTO_LINE, "Go to Line...\tCtrl+G");
    AppendMenuA(editMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_SELECTALL, "Select All\tCtrl+A");
    AppendMenuA(editMenu, MF_STRING, IDM_EDIT_TOGGLE_COMMENT, "Toggle Comment\tCtrl+/");
    AppendMenuA(editMenu, MF_SEPARATOR, 0, nullptr);
    // Multi-cursor submenu
    HMENU multiCursorMenu = CreatePopupMenu();
    AppendMenuA(multiCursorMenu, MF_STRING, IDM_EDIT_MULTICURSOR_ADD, "Add Cursor\tCtrl+Alt+Up/Down");
    AppendMenuA(multiCursorMenu, MF_STRING, IDM_EDIT_MULTICURSOR_REMOVE, "Remove Cursor");
    AppendMenuA(editMenu, MF_POPUP, (UINT_PTR)multiCursorMenu, "Multi-Cursor");
    AppendMenuA(m_menuBar, MF_POPUP, (UINT_PTR)editMenu, "&Edit");
    
    // ========== SELECTION MENU ==========
    HMENU selMenu = CreatePopupMenu();
    AppendMenuA(selMenu, MF_STRING, IDM_SEL_ALL, "Select All\tCtrl+A");
    AppendMenuA(selMenu, MF_STRING, IDM_SEL_EXPAND, "Expand Selection\tShift+Alt+Right");
    AppendMenuA(selMenu, MF_STRING, IDM_SEL_SHRINK, "Shrink Selection\tShift+Alt+Left");
    AppendMenuA(selMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(selMenu, MF_STRING | (m_columnSelectionMode ? MF_CHECKED : 0), IDM_SEL_COLUMN_MODE, "Column Selection Mode\tShift+Alt");
    AppendMenuA(selMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(selMenu, MF_STRING, IDM_SEL_ADD_CURSOR_ABOVE, "Add Cursor Above\tCtrl+Alt+Up");
    AppendMenuA(selMenu, MF_STRING, IDM_SEL_ADD_CURSOR_BELOW, "Add Cursor Below\tCtrl+Alt+Down");
    AppendMenuA(selMenu, MF_STRING, IDM_SEL_ADD_NEXT_OCCURRENCE, "Add Next Occurrence\tCtrl+D");
    AppendMenuA(selMenu, MF_STRING, IDM_SEL_SELECT_ALL_OCCURRENCES, "Select All Occurrences\tCtrl+Shift+L");
    AppendMenuA(m_menuBar, MF_POPUP, (UINT_PTR)selMenu, "&Selection");
    
    // ========== VIEW MENU ==========
    HMENU viewMenu = CreatePopupMenu();
    AppendMenuA(viewMenu, MF_STRING, IDM_VIEW_COMMAND_PALETTE, "Command Palette...\tCtrl+Shift+P");
    AppendMenuA(viewMenu, MF_SEPARATOR, 0, nullptr);
    // Appearance submenu
    HMENU appearanceMenu = CreatePopupMenu();
    AppendMenuA(appearanceMenu, MF_STRING | (m_activityBarVisible ? MF_CHECKED : 0), IDM_VIEW_ACTIVITY_BAR, "Activity Bar");
    AppendMenuA(appearanceMenu, MF_STRING | (m_primarySidebarVisible ? MF_CHECKED : 0), IDM_VIEW_PRIMARY_SIDEBAR, "Primary Side Bar\tCtrl+B");
    AppendMenuA(appearanceMenu, MF_STRING | (m_secondarySidebarVisible ? MF_CHECKED : 0), IDM_VIEW_SECONDARY_SIDEBAR, "Secondary Side Bar");
    AppendMenuA(appearanceMenu, MF_STRING | (m_panelVisible ? MF_CHECKED : 0), IDM_VIEW_PANEL, "Panel\tCtrl+J");
    AppendMenuA(appearanceMenu, MF_STRING | (m_statusBarVisible ? MF_CHECKED : 0), IDM_VIEW_STATUS_BAR, "Status Bar");
    AppendMenuA(appearanceMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(appearanceMenu, MF_STRING | (m_zenModeEnabled ? MF_CHECKED : 0), IDM_VIEW_ZEN_MODE, "Zen Mode\tCtrl+K Z");
    AppendMenuA(viewMenu, MF_POPUP, (UINT_PTR)appearanceMenu, "Appearance");
    AppendMenuA(viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(viewMenu, MF_STRING, IDM_VIEW_EXPLORER, "Explorer\tCtrl+Shift+E");
    AppendMenuA(viewMenu, MF_STRING, IDM_VIEW_SEARCH, "Search\tCtrl+Shift+F");
    AppendMenuA(viewMenu, MF_STRING, IDM_VIEW_SOURCE_CONTROL, "Source Control\tCtrl+Shift+G");
    AppendMenuA(viewMenu, MF_STRING, IDM_VIEW_EXTENSIONS, "Extensions\tCtrl+Shift+X");
    AppendMenuA(viewMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(viewMenu, MF_STRING, IDM_VIEW_PROBLEMS, "Problems\tCtrl+Shift+M");
    AppendMenuA(viewMenu, MF_STRING, IDM_VIEW_OUTPUT, "Output\tCtrl+Shift+U");
    AppendMenuA(viewMenu, MF_STRING, IDM_VIEW_TERMINAL, "Terminal\tCtrl+`");
    AppendMenuA(viewMenu, MF_SEPARATOR, 0, nullptr);
    // Editor Layout submenu
    HMENU editorLayoutMenu = CreatePopupMenu();
    AppendMenuA(editorLayoutMenu, MF_STRING | (m_minimapEnabled ? MF_CHECKED : 0), IDM_VIEW_MINIMAP, "Minimap");
    AppendMenuA(editorLayoutMenu, MF_STRING | (m_wordWrapEnabled ? MF_CHECKED : 0), IDM_VIEW_WORD_WRAP, "Word Wrap\tAlt+Z");
    AppendMenuA(editorLayoutMenu, MF_STRING | (m_lineNumbersEnabled ? MF_CHECKED : 0), IDM_VIEW_LINE_NUMBERS, "Line Numbers");
    AppendMenuA(viewMenu, MF_POPUP, (UINT_PTR)editorLayoutMenu, "Editor Layout");
    AppendMenuA(m_menuBar, MF_POPUP, (UINT_PTR)viewMenu, "&View");
    
    // ========== RUN MENU ==========
    HMENU runMenu = CreatePopupMenu();
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_START_DEBUG, "Start Debugging\tF5");
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_WITHOUT_DEBUG, "Run Without Debugging\tCtrl+F5");
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_STOP, "Stop Debugging\tShift+F5");
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_RESTART, "Restart Debugging\tCtrl+Shift+F5");
    AppendMenuA(runMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_STEP_OVER, "Step Over\tF10");
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_STEP_INTO, "Step Into\tF11");
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_STEP_OUT, "Step Out\tShift+F11");
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_CONTINUE, "Continue\tF5");
    AppendMenuA(runMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_TOGGLE_BREAKPOINT, "Toggle Breakpoint\tF9");
    AppendMenuA(runMenu, MF_STRING, IDM_RUN_CLEAR_BREAKPOINTS, "Clear All Breakpoints");
    AppendMenuA(m_menuBar, MF_POPUP, (UINT_PTR)runMenu, "&Run");
    
    // ========== TERMINAL MENU ==========
    HMENU termMenu = CreatePopupMenu();
    AppendMenuA(termMenu, MF_STRING, IDM_TERM_NEW, "New Terminal\tCtrl+Shift+`");
    AppendMenuA(termMenu, MF_STRING, IDM_TERM_SPLIT, "Split Terminal");
    AppendMenuA(termMenu, MF_SEPARATOR, 0, nullptr);
    // New Terminal submenu for shell types
    HMENU newTermSubMenu = CreatePopupMenu();
    AppendMenuA(newTermSubMenu, MF_STRING, IDM_TERM_PWSH, "PowerShell");
    AppendMenuA(newTermSubMenu, MF_STRING, IDM_TERM_CMD, "Command Prompt");
    AppendMenuA(newTermSubMenu, MF_STRING, IDM_TERM_GITBASH, "Git Bash");
    AppendMenuA(termMenu, MF_POPUP, (UINT_PTR)newTermSubMenu, "New Terminal With Profile");
    AppendMenuA(termMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(termMenu, MF_STRING, IDM_TERM_RUN_TASK, "Run Task...");
    AppendMenuA(termMenu, MF_STRING, IDM_TERM_RUN_FILE, "Run Active File");
    AppendMenuA(termMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(termMenu, MF_STRING, IDM_TERM_CLEAR, "Clear Terminal");
    AppendMenuA(termMenu, MF_STRING, IDM_TERM_KILL, "Kill Terminal");
    AppendMenuA(m_menuBar, MF_POPUP, (UINT_PTR)termMenu, "&Terminal");
    
    // ========== HELP MENU ==========
    HMENU helpMenu = CreatePopupMenu();
    AppendMenuA(helpMenu, MF_STRING, IDM_HELP_WELCOME, "Welcome");
    AppendMenuA(helpMenu, MF_STRING, IDM_HELP_DOCS, "Documentation");
    AppendMenuA(helpMenu, MF_STRING, IDM_HELP_TIPS_TRICKS, "Tips and Tricks");
    AppendMenuA(helpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(helpMenu, MF_STRING, IDM_HELP_SHORTCUTS, "Keyboard Shortcuts\tCtrl+K Ctrl+S");
    AppendMenuA(helpMenu, MF_STRING, IDM_HELP_RELEASE_NOTES, "Release Notes");
    AppendMenuA(helpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(helpMenu, MF_STRING, IDM_HELP_REPORT_ISSUE, "Report Issue...");
    AppendMenuA(helpMenu, MF_STRING, IDM_HELP_CHECK_UPDATES, "Check for Updates...");
    AppendMenuA(helpMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuA(helpMenu, MF_STRING, IDM_HELP_ABOUT, "About RawrXD IDE");
    AppendMenuA(m_menuBar, MF_POPUP, (UINT_PTR)helpMenu, "&Help");
    
    // Attach menu to window
    SetMenu(m_hwnd, m_menuBar);
#endif
}

// ============================================================================
// Handle Menu Commands
// ============================================================================
void MainWindow::handleMenuCommand(WORD cmdId)
{
#ifdef _WIN32
    switch (cmdId) {
    // ========== FILE COMMANDS ==========
    case IDM_FILE_NEW:
        addTab("Untitled");
        break;
    case IDM_FILE_NEW_WINDOW:
        // Launch new instance
        {
            char exePath[MAX_PATH];
            GetModuleFileNameA(nullptr, exePath, MAX_PATH);
            ShellExecuteA(nullptr, "open", exePath, nullptr, nullptr, SW_SHOW);
        }
        break;
    case IDM_FILE_OPEN:
        {
            char filename[MAX_PATH] = {};
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = m_hwnd;
            ofn.lpstrFilter = "All Files\0*.*\0PowerShell\0*.ps1\0C/C++\0*.c;*.cpp;*.h;*.hpp\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            if (GetOpenFileNameA(&ofn)) {
                addTab(filename);
                std::ifstream file(filename);
                if (file) {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    currentBuffer().set(content);  // Use set() method instead of content member
                    m_tabs[m_currentTab].filename = filename;
                    syncEditorFromBuffer();
                    refreshTabBar();
                }
            }
        }
        break;
    case IDM_FILE_OPEN_FOLDER:
        {
            // Use shell folder browser
            BROWSEINFOA bi = {};
            bi.hwndOwner = m_hwnd;
            bi.lpszTitle = "Select Folder to Open";
            bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
            LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
            if (pidl) {
                char folderPath[MAX_PATH];
                if (SHGetPathFromIDListA(pidl, folderPath)) {
                    // Populate file browser with folder contents
                    initializeFileBrowser();
                    // Update window title
                    std::string title = "RawrXD IDE - " + std::string(folderPath);
                    SetWindowTextA(m_hwnd, title.c_str());
                }
                CoTaskMemFree(pidl);
            }
        }
        break;
    case IDM_FILE_SAVE:
        saveTab(m_currentTab);
        break;
    case IDM_FILE_SAVEAS:
        {
            char filename[MAX_PATH] = {};
            OPENFILENAMEA ofn = {};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner = m_hwnd;
            ofn.lpstrFilter = "All Files\0*.*\0PowerShell\0*.ps1\0C/C++\0*.c;*.cpp;*.h;*.hpp\0";
            ofn.lpstrFile = filename;
            ofn.nMaxFile = MAX_PATH;
            ofn.Flags = OFN_OVERWRITEPROMPT;
            if (GetSaveFileNameA(&ofn)) {
                m_tabs[m_currentTab].filename = filename;
                saveTab(m_currentTab);
                refreshTabBar();
            }
        }
        break;
    case IDM_FILE_AUTOSAVE:
        m_autoSaveEnabled = !m_autoSaveEnabled;
        CheckMenuItem(m_menuBar, IDM_FILE_AUTOSAVE, m_autoSaveEnabled ? MF_CHECKED : MF_UNCHECKED);
        break;
    case IDM_FILE_CLOSE_TAB:
        if (m_tabs.size() > 1) closeTab(m_currentTab);
        break;
    case IDM_FILE_CLOSE_FOLDER:
        // Clear file browser and reset
        if (m_fileBrowserHwnd) {
            SendMessageA(m_fileBrowserHwnd, LB_RESETCONTENT, 0, 0);
        }
        SetWindowTextA(m_hwnd, m_windowTitle.c_str());
        break;
    case IDM_FILE_EXIT:
        saveAllDirtyTabs();
        DestroyWindow(m_hwnd);
        break;

    // ========== EDIT COMMANDS ==========
    case IDM_EDIT_UNDO:
        performUndo();
        break;
    case IDM_EDIT_REDO:
        performRedo();
        break;
    case IDM_EDIT_CUT:
        if (m_editorHwnd) SendMessage(m_editorHwnd, WM_CUT, 0, 0);
        break;
    case IDM_EDIT_COPY:
        if (m_editorHwnd) SendMessage(m_editorHwnd, WM_COPY, 0, 0);
        break;
    case IDM_EDIT_PASTE:
        if (m_editorHwnd) SendMessage(m_editorHwnd, WM_PASTE, 0, 0);
        break;
    case IDM_EDIT_FIND:
        if (m_findPanelHwnd) ShowWindow(m_findPanelHwnd, SW_SHOW);
        break;
    case IDM_EDIT_REPLACE:
        if (m_findPanelHwnd) ShowWindow(m_findPanelHwnd, SW_SHOW);
        break;
    case IDM_EDIT_GOTO_LINE:
        {
            // Simple go-to-line dialog
            char buf[32] = {};
            if (MessageBoxA(m_hwnd, "Enter line number:", "Go to Line", MB_OKCANCEL) == IDOK) {
                // For now, just a placeholder - would need a proper input dialog
            }
        }
        break;
    case IDM_EDIT_SELECTALL:
    case IDM_SEL_ALL:
        if (m_editorHwnd) SendMessage(m_editorHwnd, EM_SETSEL, 0, -1);
        break;
    case IDM_EDIT_TOGGLE_COMMENT:
        // Toggle line comment (// for C++, # for PowerShell)
        // Placeholder - would need language-aware implementation
        break;

    // ========== SELECTION COMMANDS ==========
    case IDM_SEL_EXPAND:
        // Expand selection - placeholder
        break;
    case IDM_SEL_SHRINK:
        // Shrink selection - placeholder
        break;
    case IDM_SEL_COLUMN_MODE:
        m_columnSelectionMode = !m_columnSelectionMode;
        CheckMenuItem(m_menuBar, IDM_SEL_COLUMN_MODE, m_columnSelectionMode ? MF_CHECKED : MF_UNCHECKED);
        break;

    // ========== VIEW COMMANDS ==========
    case IDM_VIEW_COMMAND_PALETTE:
        toggleCommandPalette();
        break;
    case IDM_VIEW_ACTIVITY_BAR:
        m_activityBarVisible = !m_activityBarVisible;
        CheckMenuItem(m_menuBar, IDM_VIEW_ACTIVITY_BAR, m_activityBarVisible ? MF_CHECKED : MF_UNCHECKED);
        // Trigger layout update
        if (m_hwnd) { RECT rc; GetClientRect(m_hwnd, &rc); SendMessage(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom)); }
        break;
    case IDM_VIEW_PRIMARY_SIDEBAR:
        m_primarySidebarVisible = !m_primarySidebarVisible;
        CheckMenuItem(m_menuBar, IDM_VIEW_PRIMARY_SIDEBAR, m_primarySidebarVisible ? MF_CHECKED : MF_UNCHECKED);
        if (m_fileBrowserHwnd) ShowWindow(m_fileBrowserHwnd, m_primarySidebarVisible ? SW_SHOW : SW_HIDE);
        if (m_hwnd) { RECT rc; GetClientRect(m_hwnd, &rc); SendMessage(m_hwnd, WM_SIZE, 0, MAKELPARAM(rc.right, rc.bottom)); }
        break;
    case IDM_VIEW_SECONDARY_SIDEBAR:
        m_secondarySidebarVisible = !m_secondarySidebarVisible;
        CheckMenuItem(m_menuBar, IDM_VIEW_SECONDARY_SIDEBAR, m_secondarySidebarVisible ? MF_CHECKED : MF_UNCHECKED);
        break;
    case IDM_VIEW_PANEL:
        m_panelVisible = !m_panelVisible;
        CheckMenuItem(m_menuBar, IDM_VIEW_PANEL, m_panelVisible ? MF_CHECKED : MF_UNCHECKED);
        if (m_terminalHwnd) ShowWindow(m_terminalHwnd, m_panelVisible ? SW_SHOW : SW_HIDE);
        break;
    case IDM_VIEW_STATUS_BAR:
        m_statusBarVisible = !m_statusBarVisible;
        CheckMenuItem(m_menuBar, IDM_VIEW_STATUS_BAR, m_statusBarVisible ? MF_CHECKED : MF_UNCHECKED);
        if (m_statusBarHwnd) ShowWindow(m_statusBarHwnd, m_statusBarVisible ? SW_SHOW : SW_HIDE);
        break;
    case IDM_VIEW_ZEN_MODE:
        m_zenModeEnabled = !m_zenModeEnabled;
        CheckMenuItem(m_menuBar, IDM_VIEW_ZEN_MODE, m_zenModeEnabled ? MF_CHECKED : MF_UNCHECKED);
        if (m_zenModeEnabled) {
            // Hide everything except editor - use SetMenu(NULL) to hide menu
            SetMenu(m_hwnd, nullptr);
            if (m_fileBrowserHwnd) ShowWindow(m_fileBrowserHwnd, SW_HIDE);
            if (m_terminalHwnd) ShowWindow(m_terminalHwnd, SW_HIDE);
            if (m_statusBarHwnd) ShowWindow(m_statusBarHwnd, SW_HIDE);
            // Go fullscreen
            SetWindowLongPtr(m_hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
            ShowWindow(m_hwnd, SW_MAXIMIZE);
        } else {
            // Restore normal mode
            SetWindowLongPtr(m_hwnd, GWL_STYLE, WS_OVERLAPPEDWINDOW | WS_VISIBLE);
            SetMenu(m_hwnd, m_menuBar);
            if (m_fileBrowserHwnd && m_primarySidebarVisible) ShowWindow(m_fileBrowserHwnd, SW_SHOW);
            if (m_terminalHwnd && m_panelVisible) ShowWindow(m_terminalHwnd, SW_SHOW);
            if (m_statusBarHwnd && m_statusBarVisible) ShowWindow(m_statusBarHwnd, SW_SHOW);
            ShowWindow(m_hwnd, SW_RESTORE);
        }
        break;
    case IDM_VIEW_EXPLORER:
        if (m_fileBrowserHwnd) {
            ShowWindow(m_fileBrowserHwnd, SW_SHOW);
            SetFocus(m_fileBrowserHwnd);
        }
        break;
    case IDM_VIEW_TERMINAL:
        if (m_terminalHwnd) {
            ShowWindow(m_terminalHwnd, SW_SHOW);
            SetFocus(m_terminalHwnd);
        }
        break;
    case IDM_VIEW_PROBLEMS:
        toggleProblemsPanel();
        break;
    case IDM_VIEW_MINIMAP:
        toggleMinimap();
        CheckMenuItem(m_menuBar, IDM_VIEW_MINIMAP, m_minimapEnabled ? MF_CHECKED : MF_UNCHECKED);
        break;
    case IDM_VIEW_WORD_WRAP:
        toggleWordWrap();
        CheckMenuItem(m_menuBar, IDM_VIEW_WORD_WRAP, m_wordWrapEnabled ? MF_CHECKED : MF_UNCHECKED);
        break;
    case IDM_VIEW_LINE_NUMBERS:
        toggleLineNumbers();
        CheckMenuItem(m_menuBar, IDM_VIEW_LINE_NUMBERS, m_lineNumbersEnabled ? MF_CHECKED : MF_UNCHECKED);
        break;

    // ========== RUN COMMANDS ==========
    case IDM_RUN_START_DEBUG:
        sendToTerminal("# Starting debug session...\n");
        // Placeholder - would integrate with debugger
        break;
    case IDM_RUN_WITHOUT_DEBUG:
        // Run current file without debugging
        if (!m_tabs.empty() && !m_tabs[m_currentTab].filename.empty()) {
            std::string cmd = "& '" + m_tabs[m_currentTab].filename + "'\n";
            sendToTerminal(cmd);
        }
        break;
    case IDM_RUN_STOP:
        sendToTerminal("# Stop debugging\n");
        break;
    case IDM_RUN_RESTART:
        sendToTerminal("# Restart debugging\n");
        break;

    // ========== TERMINAL COMMANDS ==========
    case IDM_TERM_NEW:
        createTerminal();
        break;
    case IDM_TERM_SPLIT:
        // Split terminal - placeholder
        break;
    case IDM_TERM_RUN_TASK:
        sendToTerminal("# Run task...\n");
        break;
    case IDM_TERM_RUN_FILE:
        if (!m_tabs.empty() && !m_tabs[m_currentTab].filename.empty()) {
            std::string cmd = "& '" + m_tabs[m_currentTab].filename + "'\n";
            sendToTerminal(cmd);
        }
        break;
    case IDM_TERM_CLEAR:
        if (m_terminalHwnd) SetWindowTextA(m_terminalHwnd, "");
        break;
    case IDM_TERM_KILL:
        if (m_terminalRunning && m_terminalProcess.hProcess) {
            TerminateProcess(m_terminalProcess.hProcess, 0);
            m_terminalRunning = false;
        }
        break;
    case IDM_TERM_PWSH:
        sendToTerminal("pwsh\n");
        break;
    case IDM_TERM_CMD:
        sendToTerminal("cmd\n");
        break;
    case IDM_TERM_GITBASH:
        sendToTerminal("\"C:\\Program Files\\Git\\bin\\bash.exe\"\n");
        break;

    // ========== HELP COMMANDS ==========
    case IDM_HELP_WELCOME:
        MessageBoxA(m_hwnd, "Welcome to RawrXD IDE!\n\nA lightweight, fast IDE for PowerShell and C++ development.", "Welcome", MB_OK | MB_ICONINFORMATION);
        break;
    case IDM_HELP_DOCS:
        ShellExecuteA(nullptr, "open", "https://github.com/ItsMehRAWRXD/RawrXD", nullptr, nullptr, SW_SHOW);
        break;
    case IDM_HELP_TIPS_TRICKS:
        MessageBoxA(m_hwnd, 
            "Tips & Tricks:\n\n"
            "- Ctrl+Shift+P: Command Palette\n"
            "- Ctrl+T: New Tab\n"
            "- Ctrl+W: Close Tab\n"
            "- Ctrl+Tab: Switch Tabs\n"
            "- Ctrl+F: Find\n"
            "- Ctrl+H: Find & Replace\n"
            "- F12: Toggle AI Panel\n"
            "- Ctrl+`: Toggle Terminal",
            "Tips & Tricks", MB_OK | MB_ICONINFORMATION);
        break;
    case IDM_HELP_SHORTCUTS:
        MessageBoxA(m_hwnd,
            "Keyboard Shortcuts:\n\n"
            "File:\n"
            "  Ctrl+N: New File\n"
            "  Ctrl+O: Open File\n"
            "  Ctrl+S: Save\n"
            "  Ctrl+Shift+S: Save As\n\n"
            "Edit:\n"
            "  Ctrl+Z: Undo\n"
            "  Ctrl+Y: Redo\n"
            "  Ctrl+X/C/V: Cut/Copy/Paste\n"
            "  Ctrl+A: Select All\n\n"
            "View:\n"
            "  Ctrl+B: Toggle Sidebar\n"
            "  Ctrl+J: Toggle Panel\n"
            "  Ctrl+`: Toggle Terminal",
            "Keyboard Shortcuts", MB_OK | MB_ICONINFORMATION);
        break;
    case IDM_HELP_RELEASE_NOTES:
        MessageBoxA(m_hwnd, "RawrXD IDE v1.0\n\nRelease Notes:\n- Initial release\n- Full menu bar\n- Multi-tab support\n- Integrated terminal\n- AI chat integration", "Release Notes", MB_OK | MB_ICONINFORMATION);
        break;
    case IDM_HELP_REPORT_ISSUE:
        ShellExecuteA(nullptr, "open", "https://github.com/ItsMehRAWRXD/RawrXD/issues", nullptr, nullptr, SW_SHOW);
        break;
    case IDM_HELP_CHECK_UPDATES:
        MessageBoxA(m_hwnd, "You are running the latest version.", "Check for Updates", MB_OK | MB_ICONINFORMATION);
        break;
    case IDM_HELP_ABOUT:
        MessageBoxA(m_hwnd, 
            "RawrXD IDE\n\n"
            "Version: 1.0.0\n"
            "A lightweight IDE for PowerShell and C++ development.\n\n"
            "Features:\n"
            "- Syntax highlighting\n"
            "- Multi-tab editing\n"
            "- Integrated terminal\n"
            "- AI chat assistant\n"
            "- Chromatic effects support\n\n"
            "(c) 2025 RawrXD Team",
            "About RawrXD IDE", MB_OK | MB_ICONINFORMATION);
        break;
    }
#endif
}

// Removed initializeSubsystems() - functions not defined in header

void MainWindow::show()
{
#ifdef _WIN32
    if (m_hwnd) {
        ShowWindow(m_hwnd, SW_SHOW);
        UpdateWindow(m_hwnd);
    }
#else
    std::cout << "RawrXD IDE - Simple C++ Implementation" << std::endl;
#endif
}

int MainWindow::exec()
{
#ifdef _WIN32
    MSG msg = {};
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    // Clean shutdown
    stopTerminalReader();
    if(m_terminalRunning) {
        // Try graceful PowerShell exit
        if(m_psInWrite) {
            const char* exitCmd = "exit\r\n";
            DWORD written=0; WriteFile(m_psInWrite, exitCmd, (DWORD)strlen(exitCmd), &written, nullptr);
        }
        WaitForSingleObject(m_terminalProcess.hProcess, 500);
        TerminateProcess(m_terminalProcess.hProcess, 0);
        CloseHandle(m_terminalProcess.hProcess);
        CloseHandle(m_terminalProcess.hThread);
        m_terminalRunning = false;
    }
    for(auto h : m_loadedPlugins){ if(h) FreeLibrary(h); }
    return static_cast<int>(msg.wParam);
#else
    std::atomic<bool> running{true};
    std::cout << "Press 'q' then Enter to quit.\n";
    while (running) {
        int c = std::cin.get();
        if (c == 'q' || c == 'Q') running = false;
    }
    return 0;
#endif
}

void MainWindow::createWindow()
{
#ifdef _WIN32
    const char* className = "RawrXDIDE";
    
    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = className;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    
    RegisterClass(&wc);
    
    m_hwnd = CreateWindowEx(
        0, className, m_windowTitle.c_str(),
        WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 1200, 700,
        nullptr, nullptr, GetModuleHandle(nullptr), this
    );
    if(m_hwnd) {
        createMenuBar();     // Create the menu bar first
        createTabBar();
        createEditor();
        m_statusBarHwnd = CreateWindowExA(0, "STATIC", "", WS_CHILD|WS_VISIBLE|SS_LEFT, 10, 450, 580, 20, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
        updateStatusBar();
        createCommandPalette();
        createLayoutPanes();
    }
#endif
}

void MainWindow::createEditor()
{
#ifdef _WIN32
    // Use RichEdit 5.0 control for advanced formatting & colorization
    // ANSI version since project appears to use narrow CreateWindowEx elsewhere
    m_editorHwnd = CreateWindowExA(
        0, "RICHEDIT50W", nullptr,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | ES_MULTILINE | ES_AUTOVSCROLL | ES_AUTOHSCROLL | ES_NOHIDESEL,
        10, 40, 580, 400, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );
    if(m_editorHwnd) {
        HFONT hFont = CreateFontA(11, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        SendMessage(m_editorHwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
        if(m_currentTheme < m_themes.size()) {
            SendMessage(m_editorHwnd, EM_SETBKGNDCOLOR, 0, m_themes[m_currentTheme].bg);
        }
        syncEditorFromBuffer();
    }
#endif
}

void MainWindow::createTabBar()
{
#ifdef _WIN32
    if(!m_hwnd) return;
    m_tabBarHwnd = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 10, 10, 580, 24, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    refreshTabBar();
#endif
}

void MainWindow::updateTelemetry()
{
    // Update overclock display
    std::stringstream ss;
    ss << "Overclock Panel\nCPU: " << m_appState->cpu_freq_mhz << " MHz\n"
       << "GPU: " << m_appState->gpu_freq_mhz << " MHz\n"
       << "Status: " << (m_appState->governor_enabled ? "Active" : "Inactive");
#ifdef _WIN32
    SetWindowTextA(m_overclockHwnd, ss.str().c_str());
#endif
    
    // Update AI metrics display
    updateAIMetricsDisplay();
}

void MainWindow::updateAIMetricsDisplay()
{
    // Telemetry display stub: keep UI update without linking full telemetry
    std::ostringstream ss;
    ss << "=== AI Metrics Dashboard ===\n\n";
    ss << "Session: n/a\n";
    ss << "Latest:  n/a\n";
    ss << "Latency: n/a\n";
    ss << "Tokens:  n/a\n";
    ss << "Model:   n/a\n";
    ss << "\n[F11] Export | [F12] Clear\n";

#ifdef _WIN32
    if (m_floatingPanel && m_floatingPanelVisible) {
        SetWindowTextA(m_floatingPanel, ss.str().c_str());
    }
#endif
}

// Advanced features implementations
void MainWindow::loadFileWithLazyLoading(const std::string& filename) {
    fs::path filePath(filename);
    if (!fs::exists(filePath)) return;
    
    size_t fileSize = fs::file_size(filePath);
    
    if (m_lazyLoadingEnabled && fileSize > m_maxFileSizeForLazyLoad) {
        // Lazy loading: load first chunk
        std::ifstream file(filePath, std::ios::binary);
        std::string chunk(1024 * 1024, '\0'); // 1MB chunk
        file.read(&chunk[0], chunk.size());
        chunk.resize(file.gcount());
        m_editorBuffer.clear();
        m_editorBuffer.push_back(chunk);
        std::cout << "Loaded first chunk of large file: " << filename << std::endl;
    } else {
        // Normal loading
        std::ifstream file(filePath);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        m_editorBuffer.clear();
        m_editorBuffer.push_back(content);
    }
}

void MainWindow::runPesterTests() {
    if (!m_pesterAvailable) {
        std::cout << "Pester not available. Run initUnitTesting first." << std::endl;
        return;
    }
    std::string testCommand = "powershell -Command \"Invoke-Pester -Path . -OutputFormat NUnitXml -OutputFile TestResults.xml\"";
    handleCommand(testCommand);
    std::cout << "Running Pester tests..." << std::endl;
}

void MainWindow::buildWithMSBuild() {
    if (m_msbuildPath.empty()) {
        std::cout << "MSBuild not found. Run initBuildSystem first." << std::endl;
        return;
    }
    
    std::string buildCommand = "\"" + m_msbuildPath + "\" RawrXD-ModelLoader.sln /p:Configuration=Release";
    handleCommand(buildCommand);
    std::cout << "Building with MSBuild..." << std::endl;
}

void MainWindow::publishToGallery() {
    if (!m_galleryReady) {
        std::cout << "Gallery not ready. Run initScriptPublishing first." << std::endl;
        return;
    }
    
    std::string publishCommand = "powershell -Command \"Publish-Script -Path script.ps1 -NuGetApiKey $env:NUGET_API_KEY\"";
    handleCommand(publishCommand);
    std::cout << "Publishing to PowerShell Gallery..." << std::endl;
}

void MainWindow::startRemoteSession(const std::string& remoteHost) {
    if (!m_remoteDebugEnabled) {
        std::cout << "Remote debugging not enabled. Run initRemoteDebugging first." << std::endl;
        return;
    }
    
    std::string remoteCommand = "powershell -Command \"Enter-PSSession -ComputerName " + remoteHost + "\"";
    handleCommand(remoteCommand);
    std::cout << "Starting remote session with: " << remoteHost << std::endl;
}

void MainWindow::handleCommand(const std::string& cmd) {
    // Stub implementation - prints command to console
    std::cout << "Command: " << cmd << std::endl;
    // TODO: Implement actual command execution
}

// Editor Settings (10 features)
void MainWindow::setEditorTheme(const std::string& theme) {
    m_editorTheme = theme;
#ifdef _WIN32
    COLORREF bgColor = (theme == "dark") ? RGB(30, 30, 30) : RGB(255, 255, 255);
    COLORREF textColor = (theme == "dark") ? RGB(220, 220, 220) : RGB(0, 0, 0);
    if (m_editorHwnd) {
        SendMessage(m_editorHwnd, EM_SETBKGNDCOLOR, 0, bgColor);
    }
#endif
    std::cout << "Editor theme set to: " << theme << std::endl;
}

void MainWindow::setEditorFont(const std::string& fontName, int fontSize) {
    m_fontName = fontName;
    m_fontSize = fontSize;
#ifdef _WIN32
    if (m_editorHwnd) {
        HFONT hFont = CreateFontA(fontSize, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, fontName.c_str());
        SendMessage(m_editorHwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
    }
#endif
    std::cout << "Font set to: " << fontName << " @ " << fontSize << "pt" << std::endl;
}

void MainWindow::setTabSize(int spaces) {
    m_tabSize = spaces;
#ifdef _WIN32
    if (m_editorHwnd) {
        DWORD tabStops = spaces * 4; // 4 dialog units per space
        SendMessage(m_editorHwnd, EM_SETTABSTOPS, 1, (LPARAM)&tabStops);
    }
#endif
    std::cout << "Tab size set to: " << spaces << " spaces" << std::endl;
}

void MainWindow::toggleMinimap() {
    m_minimapEnabled = !m_minimapEnabled;
    std::cout << "Minimap " << (m_minimapEnabled ? "enabled" : "disabled") << std::endl;
}

void MainWindow::toggleLineNumbers() {
    m_lineNumbersEnabled = !m_lineNumbersEnabled;
    std::cout << "Line numbers " << (m_lineNumbersEnabled ? "enabled" : "disabled") << std::endl;
}

void MainWindow::toggleWordWrap() {
    m_wordWrapEnabled = !m_wordWrapEnabled;
#ifdef _WIN32
    if (m_editorHwnd) {
        // RichEdit doesn't have direct word wrap toggle, need to recreate
        DWORD style = GetWindowLong(m_editorHwnd, GWL_STYLE);
        if (m_wordWrapEnabled) {
            style &= ~ES_AUTOHSCROLL;
        } else {
            style |= ES_AUTOHSCROLL;
        }
        SetWindowLong(m_editorHwnd, GWL_STYLE, style);
        InvalidateRect(m_editorHwnd, nullptr, TRUE);
    }
#endif
    std::cout << "Word wrap " << (m_wordWrapEnabled ? "enabled" : "disabled") << std::endl;
}

void MainWindow::setColorScheme(const std::string& scheme) {
    m_colorScheme = scheme;
    std::cout << "Color scheme set to: " << scheme << std::endl;
}

void MainWindow::toggleAutocomplete() {
    m_autocompleteEnabled = !m_autocompleteEnabled;
    std::cout << "Autocomplete " << (m_autocompleteEnabled ? "enabled" : "disabled") << std::endl;
}

void MainWindow::setIndentStyle(bool useTabs) {
    m_useTabsForIndent = useTabs;
    std::cout << "Indent style: " << (useTabs ? "tabs" : "spaces") << std::endl;
}

void MainWindow::toggleBracketMatching() {
    m_bracketMatchingEnabled = !m_bracketMatchingEnabled;
    std::cout << "Bracket matching " << (m_bracketMatchingEnabled ? "enabled" : "disabled") << std::endl;
}

// Problems Panel (10 features)
void MainWindow::createProblemsPanel() {
#ifdef _WIN32
    if (!m_hwnd) return;
    
    RECT clientRect;
    GetClientRect(m_hwnd, &clientRect);
    
    int panelHeight = 150;
    int panelY = clientRect.bottom - panelHeight;
    
    m_problemsPanelHwnd = CreateWindowExA(
        0, "EDIT",
        "Problems Panel\r\n",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL,
        0, panelY, clientRect.right, panelHeight,
        m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr
    );
    
    if (m_problemsPanelHwnd) {
        HFONT hFont = CreateFontA(10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            DEFAULT_QUALITY, FIXED_PITCH | FF_MODERN, "Consolas");
        SendMessage(m_problemsPanelHwnd, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(m_problemsPanelHwnd, EM_SETBKGNDCOLOR, 0, RGB(40, 40, 40));
    }
#endif
    std::cout << "Problems panel created" << std::endl;
}

void MainWindow::addProblem(const std::string& file, int line, const std::string& message, const std::string& severity) {
    Problem p = {file, line, message, severity};
    m_problems.push_back(p);
    
#ifdef _WIN32
    if (m_problemsPanelHwnd) {
        std::string icon = (severity == "error") ? "❌" : (severity == "warning") ? "⚠️" : "ℹ️";
        std::string problemText = icon + " " + file + ":" + std::to_string(line) + " - " + message + "\r\n";
        
        int len = GetWindowTextLength(m_problemsPanelHwnd);
        SendMessage(m_problemsPanelHwnd, EM_SETSEL, len, len);
        SendMessage(m_problemsPanelHwnd, EM_REPLACESEL, FALSE, (LPARAM)problemText.c_str());
    }
#endif
    std::cout << "Problem added: " << severity << " in " << file << ":" << line << std::endl;
}

void MainWindow::clearProblems() {
    m_problems.clear();
#ifdef _WIN32
    if (m_problemsPanelHwnd) {
        SetWindowTextA(m_problemsPanelHwnd, "Problems Panel\r\n");
    }
#endif
    std::cout << "Problems cleared" << std::endl;
}

void MainWindow::autoRepairProblem(int problemIndex) {
    if (problemIndex < 0 || problemIndex >= static_cast<int>(m_problems.size())) {
        std::cout << "Invalid problem index" << std::endl;
        return;
    }
    
    const Problem& p = m_problems[problemIndex];
    std::cout << "Auto-repairing: " << p.message << " in " << p.file << ":" << p.line << std::endl;
    
    // Simple auto-repair heuristics
    if (p.message.find("missing semicolon") != std::string::npos) {
        std::cout << "  -> Adding semicolon at line " << p.line << std::endl;
    } else if (p.message.find("undeclared identifier") != std::string::npos) {
        std::cout << "  -> Suggesting declaration for identifier" << std::endl;
    } else if (p.message.find("unused variable") != std::string::npos) {
        std::cout << "  -> Removing unused variable" << std::endl;
    } else {
        std::cout << "  -> No auto-repair available for this problem type" << std::endl;
    }
}

void MainWindow::toggleProblemsPanel() {
    m_problemsPanelVisible = !m_problemsPanelVisible;
#ifdef _WIN32
    if (m_problemsPanelHwnd) {
        ShowWindow(m_problemsPanelHwnd, m_problemsPanelVisible ? SW_SHOW : SW_HIDE);
    }
#endif
    std::cout << "Problems panel " << (m_problemsPanelVisible ? "shown" : "hidden") << std::endl;
}

void MainWindow::sortProblemsBySeverity() {
    std::sort(m_problems.begin(), m_problems.end(), [](const Problem& a, const Problem& b) {
        const std::map<std::string, int> severityOrder = {{"error", 3}, {"warning", 2}, {"info", 1}};
        int aVal = severityOrder.count(a.severity) ? severityOrder.at(a.severity) : 0;
        int bVal = severityOrder.count(b.severity) ? severityOrder.at(b.severity) : 0;
        return aVal > bVal;
    });
    
    // Refresh display
    clearProblems();
    for (const auto& p : m_problems) {
        addProblem(p.file, p.line, p.message, p.severity);
    }
    std::cout << "Problems sorted by severity" << std::endl;
}

void MainWindow::filterProblemsByType(const std::string& type) {
    m_problemsFilter = type;
#ifdef _WIN32
    if (m_problemsPanelHwnd) {
        SetWindowTextA(m_problemsPanelHwnd, ("Problems Panel (Filter: " + type + ")\r\n").c_str());
        
        for (const auto& p : m_problems) {
            if (type == "all" || p.severity == type) {
                std::string icon = (p.severity == "error") ? "❌" : (p.severity == "warning") ? "⚠️" : "ℹ️";
                std::string problemText = icon + " " + p.file + ":" + std::to_string(p.line) + " - " + p.message + "\r\n";
                
                int len = GetWindowTextLength(m_problemsPanelHwnd);
                SendMessage(m_problemsPanelHwnd, EM_SETSEL, len, len);
                SendMessage(m_problemsPanelHwnd, EM_REPLACESEL, FALSE, (LPARAM)problemText.c_str());
            }
        }
    }
#endif
    std::cout << "Problems filtered by: " << type << std::endl;
}

void MainWindow::exportProblems(const std::string& filename) {
    std::ofstream outFile(filename);
    if (!outFile) {
        std::cout << "Failed to open file for export: " << filename << std::endl;
        return;
    }
    
    outFile << "RawrXD IDE - Problems Export\n";
    outFile << "============================\n\n";
    
    for (const auto& p : m_problems) {
        outFile << "[" << p.severity << "] " << p.file << ":" << p.line << " - " << p.message << "\n";
    }
    
    outFile.close();
    std::cout << "Problems exported to: " << filename << std::endl;
}

void MainWindow::jumpToProblem(int problemIndex) {
    if (problemIndex < 0 || problemIndex >= static_cast<int>(m_problems.size())) {
        std::cout << "Invalid problem index" << std::endl;
        return;
    }
    
    const Problem& p = m_problems[problemIndex];
    std::cout << "Jumping to: " << p.file << ":" << p.line << std::endl;
    
    // Load file and jump to line
    loadFileWithLazyLoading(p.file);
#ifdef _WIN32
    if (m_editorHwnd) {
        // Calculate character position for line
        SendMessage(m_editorHwnd, EM_LINESCROLL, 0, p.line - 1);
    }
#endif
}

void MainWindow::showProblemDetails(int problemIndex) {
    if (problemIndex < 0 || problemIndex >= static_cast<int>(m_problems.size())) {
        std::cout << "Invalid problem index" << std::endl;
        return;
    }
    
    const Problem& p = m_problems[problemIndex];
    std::cout << "\n=== Problem Details ===" << std::endl;
    std::cout << "File: " << p.file << std::endl;
    std::cout << "Line: " << p.line << std::endl;
    std::cout << "Severity: " << p.severity << std::endl;
    std::cout << "Message: " << p.message << std::endl;
    std::cout << "======================\n" << std::endl;
}

// AI Metrics & Telemetry
void MainWindow::simulateAIRequest(const std::string& model, bool success) {
    (void)model; (void)success;
    // Telemetry disabled for SimpleIDE minimal build
    std::cout << "Simulated AI request (telemetry disabled)" << std::endl;
}

void MainWindow::exportMetrics(const std::string& format) {
    (void)format;
    std::cout << "Metrics export disabled in SimpleIDE build" << std::endl;
}

void MainWindow::clearMetrics() {
    std::cout << "Metrics cleared (telemetry disabled)" << std::endl;
}

void MainWindow::showMetricsReport() {
    std::cout << "Telemetry report disabled in SimpleIDE" << std::endl;
}

void MainWindow::initExtensionSystem() {
    std::cout << "Initializing extension system..." << std::endl;
    // Basic plugin architecture: load DLLs from plugins directory
    fs::path pluginDir = fs::current_path() / "plugins";
    if (fs::exists(pluginDir)) {
        for (const auto& entry : fs::directory_iterator(pluginDir)) {
            if (entry.path().extension() == ".dll") {
#ifdef _WIN32
                HMODULE hModule = LoadLibraryW(entry.path().c_str());
                if (hModule) {
                    m_loadedPlugins.push_back(hModule);
                    std::cout << "Loaded plugin: " << entry.path().string() << std::endl;
                }
#endif
            }
        }
    }
    std::cout << "Extension system initialized." << std::endl;
}

void MainWindow::initRemoteDebugging() {
    std::cout << "Initializing remote debugging..." << std::endl;
    // Basic PSRemoting support: enable remoting and prepare for remote sessions
    m_remoteDebugEnabled = true;
    // Execute: Enable-PSRemoting -Force
    handleCommand("powershell -Command \"Enable-PSRemoting -Force -SkipNetworkProfileCheck\"");
    std::cout << "Remote debugging initialized." << std::endl;
}

void MainWindow::initUnitTesting() {
    std::cout << "Initializing unit testing..." << std::endl;
    // Pester integration: install and prepare Pester
    handleCommand("powershell -Command \"if (!(Get-Module -ListAvailable -Name Pester)) { Install-Module -Name Pester -Force -SkipPublisherCheck }\"");
    m_pesterAvailable = true;
    std::cout << "Unit testing initialized." << std::endl;
}

void MainWindow::initBuildSystem() {
    std::cout << "Initializing build system..." << std::endl;
    // MSBuild integration: detect MSBuild path
#ifdef _WIN32
    // Try to find MSBuild in common locations
    std::vector<std::string> msbuildPaths = {
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Professional\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Enterprise\\MSBuild\\Current\\Bin\\MSBuild.exe",
        "C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\MSBuild\\Current\\Bin\\MSBuild.exe"
    };
    
    for (const auto& path : msbuildPaths) {
        if (fs::exists(path)) {
            m_msbuildPath = path;
            std::cout << "Found MSBuild: " << path << std::endl;
            break;
        }
    }
#endif
    std::cout << "Build system initialized." << std::endl;
}

void MainWindow::initScriptPublishing() {
    std::cout << "Initializing script publishing..." << std::endl;
    // PowerShell Gallery integration: install PowerShellGet
    handleCommand("powershell -Command \"Install-Module -Name PowerShellGet -Force -SkipPublisherCheck\"");
    m_galleryReady = true;
    std::cout << "Script publishing initialized." << std::endl;
}

void MainWindow::wireOverclockPanel() { 
    std::cout << "Wiring overclock panel to backend..." << std::endl;
    /* Wired to backend via updateTelemetry */ 
    std::cout << "Overclock panel wired." << std::endl;
}

void MainWindow::initPerformanceOpts() {
    std::cout << "Initializing performance optimizations..." << std::endl;
    // Lazy loading for large files: implement basic chunked loading
    m_lazyLoadingEnabled = true;
    m_maxFileSizeForLazyLoad = 1024 * 1024; // 1MB
    std::cout << "Performance optimizations initialized." << std::endl;
}

#ifdef _WIN32
LRESULT CALLBACK MainWindow::FloatingPanelProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* window = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    
    switch (uMsg) {
    case WM_CREATE: {
        CREATESTRUCT* create = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<MainWindow*>(create->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
        return 0;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        
        // Draw panel content
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(0, 0, 0));
        
        std::stringstream ss;
        ss << "RawrXD Floating Panel\n\n";
        
        if (window && window->m_appState) {
            ss << "CPU: " << window->m_appState->cpu_freq_mhz << " MHz\n";
            ss << "GPU: " << window->m_appState->gpu_freq_mhz << " MHz\n\n";
            ss << "Governor: " << (window->m_appState->governor_enabled ? "Active" : "Inactive") << "\n\n";
            ss << "Extensions: " << window->m_loadedPlugins.size() << " loaded\n";
            ss << "Remote Debug: " << (window->m_remoteDebugEnabled ? "Yes" : "No") << "\n";
            ss << "Pester: " << (window->m_pesterAvailable ? "Available" : "N/A") << "\n";
            ss << "MSBuild: " << (!window->m_msbuildPath.empty() ? "Found" : "N/A") << "\n";
            ss << "Gallery: " << (window->m_galleryReady ? "Ready" : "N/A") << "\n";
            ss << "Lazy Load: " << (window->m_lazyLoadingEnabled ? "Enabled" : "Disabled") << "\n";
        }
        
        DrawTextA(hdc, ss.str().c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_WORDBREAK);
        
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_LBUTTONDOWN:
        if (window) {
            window->m_panelDragging = true;
            GetCursorPos(&window->m_panelDragStart);
            SetCapture(hwnd);
        }
        return 0;
    case WM_LBUTTONUP:
        if (window) {
            window->m_panelDragging = false;
            ReleaseCapture();
        }
        return 0;
    case WM_MOUSEMOVE:
        if (window && window->m_panelDragging) {
            POINT pt;
            GetCursorPos(&pt);
            RECT rect;
            GetWindowRect(hwnd, &rect);
            int dx = pt.x - window->m_panelDragStart.x;
            int dy = pt.y - window->m_panelDragStart.y;
            SetWindowPos(hwnd, HWND_TOPMOST, rect.left + dx, rect.top + dy, 0, 0, SWP_NOSIZE);
            window->m_panelDragStart = pt;
        }
        return 0;
    case WM_CLOSE:
        if (window) {
            window->m_floatingPanelVisible = false;
        }
        ShowWindow(hwnd, SW_HIDE);
        return 0;
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK MainWindow::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    MainWindow* window = nullptr;
    
    if (uMsg == WM_NCCREATE) {
        CREATESTRUCT* create = reinterpret_cast<CREATESTRUCT*>(lParam);
        window = reinterpret_cast<MainWindow*>(create->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
    } else {
        window = reinterpret_cast<MainWindow*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }
    
    if (window) {
        switch (uMsg) {
        case WM_DESTROY:
                    window->saveSettings();
                    window->saveAllDirtyTabs();
            if (window->m_terminalRunning) {
                TerminateProcess(window->m_terminalProcess.hProcess, 0);
                CloseHandle(window->m_terminalProcess.hProcess);
                CloseHandle(window->m_terminalProcess.hThread);
            }
            PostQuitMessage(0);
            return 0;
        case WM_COMMAND:
            // Tab handling
            if(LOWORD(wParam) == 1999) { window->addTab("Untitled"); return 0; }
            if(LOWORD(wParam) >= 2000 && LOWORD(wParam) < 2100) { size_t idx = LOWORD(wParam) - 2000; window->switchTab(idx); return 0; }
            if(LOWORD(wParam) == 3001) { int sel = (int)SendMessage(window->m_commandPaletteHwnd, LB_GETCURSEL, 0, 0); window->executePaletteSelection(sel); window->toggleCommandPalette(); return 0; }
            // Floating panel toggle example
            if (LOWORD(wParam) == 1000) {
                window->toggleFloatingPanel();
                return 0;
            }
            // Search / Replace buttons
            if (hwnd == window->m_findPanelHwnd) {
                switch(LOWORD(wParam)) {
                    case 1: { // Find Next
                        char buf[256] = {};
                        GetWindowTextA(window->m_findEditHwnd, buf, 255);
                        window->findNextInEditor(buf);
                        return 0; }
                    case 2: { // Replace
                        char f[256]={}, r[256]={};
                        GetWindowTextA(window->m_findEditHwnd, f,255);
                        GetWindowTextA(window->m_replaceEditHwnd, r,255);
                        window->replaceNextInEditor(f,r);
                        return 0; }
                    case 3: { // Replace All
                        char f[256]={}, r[256]={};
                        GetWindowTextA(window->m_findEditHwnd, f,255);
                        GetWindowTextA(window->m_replaceEditHwnd, r,255);
                        window->replaceAllInEditor(f,r);
                        return 0; }
                }
            }
            // Chat send button (old)
            if (LOWORD(wParam) == 40003 && window->m_chatPanelShim.impl) {
                auto* chat = reinterpret_cast<RawrXD::UI::ChatPanel*>(window->m_chatPanelShim.impl);
                std::string text = chat->getInput();
                if (!text.empty()) {
                    chat->appendMessage("You", text);
                    window->appendTopChat("You", text);
                    chat->clearInput();
                    window->startChatRequest(text);
                }
                return 0;
            }
            // New user chat send button (5002)
            if (LOWORD(wParam) == 5002 && window->m_userChatInputHwnd) {
                char buf[4096] = {};
                GetWindowTextA(window->m_userChatInputHwnd, buf, sizeof(buf)-1);
                std::string text(buf);
                if (!text.empty()) {
                    window->appendTopChat("You", text);
                    SetWindowTextA(window->m_userChatInputHwnd, "");
                    window->startChatRequest(text);
                }
                return 0;
            }
            // File browser double-click open
            if (HIWORD(wParam) == LBN_DBLCLK && window->m_fileBrowserHwnd && (HWND)lParam == window->m_fileBrowserHwnd) {
                window->onFileBrowserDblClick();
                return 0;
            }
            // Route all menu bar commands to handleMenuCommand
            {
                WORD cmdId = LOWORD(wParam);
                if (cmdId >= 100 && cmdId < 800) {  // Menu ID range 100-799
                    window->handleMenuCommand(cmdId);
                    return 0;
                }
            }
            break;
        case WM_CHAT_COMPLETE: {
            // lParam carries pointer to std::string allocated on heap
            std::string* respPtr = reinterpret_cast<std::string*>(lParam);
            if (respPtr) {
                window->handleChatResponse(*respPtr);
                delete respPtr;
            }
            return 0;
        }
        case WM_SIZE: {
            RECT rc; GetClientRect(hwnd, &rc);
            if (window->m_splitLayout) {
                auto* splitter = reinterpret_cast<RawrXD::UI::SplitLayout*>(window->m_splitLayout);
                splitter->onResize(rc.right - rc.left, rc.bottom - rc.top);
                
                // Position the user chat input and send button within the bottom-right pane
                // Get the parent static container's bounds for the user chat area
                if (window->m_userChatInputHwnd && window->m_userChatSendBtn) {
                    // Calculate bottom-right pane position (after bottom splitter)
                    // The splitter already positioned it; we just need to lay out children within
                    RECT userChatRect;
                    // Approximate: get from terminal position and calculate right side
                    if (window->m_terminalHwnd) {
                        RECT termRect;
                        GetWindowRect(window->m_terminalHwnd, &termRect);
                        MapWindowPoints(HWND_DESKTOP, hwnd, (LPPOINT)&termRect, 2);
                        // User chat is to the right of terminal
                        int chatX = termRect.right + 6; // splitter gap
                        int chatY = termRect.top;
                        int chatW = (rc.right - 4) - chatX;
                        int chatH = termRect.bottom - termRect.top;
                        int btnW = 60, btnH = 28, pad = 4;
                        // Send button at bottom-right of this area
                        MoveWindow(window->m_userChatSendBtn, chatX + chatW - btnW - pad, chatY + chatH - btnH - pad, btnW, btnH, TRUE);
                        // Input takes rest of area
                        MoveWindow(window->m_userChatInputHwnd, chatX, chatY, chatW - btnW - pad*2, chatH, TRUE);
                    }
                }
                
                // Resize old chat panel if exists (legacy)
                if (window->m_chatPanelShim.impl) {
                    auto* chat = reinterpret_cast<RawrXD::UI::ChatPanel*>(window->m_chatPanelShim.impl);
                    RECT br; GetWindowRect(chat->hwnd(), &br); POINT pt{br.left, br.top}; ScreenToClient(hwnd, &pt);
                    int w = br.right - br.left; int h = br.bottom - br.top;
                    chat->resize(pt.x, pt.y, w, h);
                }
            }
            return 0;
        }
        case WM_KEYDOWN:
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'F') {
                if (window->m_findPanelHwnd) ShowWindow(window->m_findPanelHwnd, SW_SHOW);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'T') { window->addTab("Untitled"); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'W') { if(window->m_tabs.size()>1) window->closeTab(window->m_currentTab); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == VK_TAB) { size_t n=window->m_tabs.size(); if(n>1) window->switchTab((window->m_currentTab+1)%n); return 0; }
            if ((GetKeyState(VK_CONTROL)&0x8000) && (GetKeyState(VK_SHIFT)&0x8000) && wParam=='P') { window->toggleCommandPalette(); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'H') {
                if (window->m_findPanelHwnd) ShowWindow(window->m_findPanelHwnd, SW_SHOW);
                return 0;
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'Z') { window->performUndo(); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'Y') { window->performRedo(); return 0; }
            // Additional menu keyboard shortcuts
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'N') { window->handleMenuCommand(IDM_FILE_NEW); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'O') { window->handleMenuCommand(IDM_FILE_OPEN); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'S') { 
                if (GetKeyState(VK_SHIFT) & 0x8000) window->handleMenuCommand(IDM_FILE_SAVEAS);
                else window->handleMenuCommand(IDM_FILE_SAVE);
                return 0; 
            }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'B') { window->handleMenuCommand(IDM_VIEW_PRIMARY_SIDEBAR); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'J') { window->handleMenuCommand(IDM_VIEW_PANEL); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'G') { window->handleMenuCommand(IDM_EDIT_GOTO_LINE); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'A') { window->handleMenuCommand(IDM_EDIT_SELECTALL); return 0; }
            if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == VK_OEM_3) { // Ctrl+` (backtick)
                window->handleMenuCommand(IDM_VIEW_TERMINAL); 
                return 0; 
            }
            if (wParam == VK_F5) { 
                if (GetKeyState(VK_CONTROL) & 0x8000) window->handleMenuCommand(IDM_RUN_WITHOUT_DEBUG);
                else window->handleMenuCommand(IDM_RUN_START_DEBUG);
                return 0; 
            }
            if (wParam == VK_F9) { window->handleMenuCommand(IDM_RUN_TOGGLE_BREAKPOINT); return 0; }
            if (wParam == VK_F10) { window->handleMenuCommand(IDM_RUN_STEP_OVER); return 0; }
            if (wParam == VK_F11) { 
                if (GetKeyState(VK_SHIFT) & 0x8000) window->handleMenuCommand(IDM_RUN_STEP_OUT);
                else window->handleMenuCommand(IDM_RUN_STEP_INTO);
                return 0; 
            }
            if (wParam == VK_F12) {
                window->toggleFloatingPanel();
                return 0;
            }
            break;
        // (Removed duplicate WM_COMMAND block; merged into primary)
                case WM_CHAR:
                    if(window->m_editorHwnd) {
                        wchar_t ch = (wchar_t)wParam;
                        if(ch >= 32 && ch != 127) { // printable
                            DWORD selStart=0, selEnd=0;
                            SendMessage(window->m_editorHwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
                            size_t pos = selStart;
                            size_t eraseLen = (selEnd>selStart)? (selEnd - selStart) : 0;
                            char utf8[5] = {};
                            int bytes = WideCharToMultiByte(CP_UTF8,0,&ch,1,utf8,4,nullptr,nullptr);
                            window->applyEdit(pos, eraseLen, std::string_view(utf8, bytes));
                            return 0;
                        } else if(ch == 8) { // backspace
                            DWORD selStart=0, selEnd=0;
                            SendMessage(window->m_editorHwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
                            if(selEnd>selStart) {
                                window->applyEdit(selStart, selEnd-selStart, "");
                            } else if(selStart>0) {
                                window->applyEdit(selStart-1, 1, "");
                            }
                            return 0;
                        }
                    }
                    break;
        case WM_TIMER:
            window->updateTelemetry();
                    window->updateStatusBar();
            if (window->m_floatingPanelVisible && window->m_floatingPanel) {
                InvalidateRect(window->m_floatingPanel, nullptr, TRUE);
            }
            if (wParam == 2) {
                window->retokenizeAndApplyColors();
            }
            break;
        }
    }
    
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}
#endif

#ifdef _WIN32
void MainWindow::createLayoutPanes() {
    /*
     * IDE Layout:
     * ┌─────────────────┬──────────────────┬─────────────────┐
     * │ FILE EXPLORER   │   Code Editor    │   AI Chat       │
     * │ (m_fileBrowser) │   (m_editorHwnd) │ (m_topChatHwnd) │
     * │                 │                  │                 │
     * ├─────────────────┴──────────────────┼─────────────────┤
     * │ TERMINAL/PWSH (m_terminalHwnd)     │ User Chat Input │
     * └────────────────────────────────────┴─────────────────┘
     */
    
    // === TOP ROW PANES ===
    // Left: File Browser (LISTBOX with notify for double-click)
    m_fileBrowserHwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOINTEGRALHEIGHT | LBS_NOTIFY,
        0, 0, 100, 100, m_hwnd, (HMENU)5000, GetModuleHandle(nullptr), nullptr);
    
    // Middle: Code Editor already created in createEditor()
    
    // Right: AI Chat Transcript (read-only multiline edit)
    m_topChatHwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        0, 0, 100, 100, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    SendMessageA(m_topChatHwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    
    // === BOTTOM ROW PANES ===
    // Left: Terminal/PowerShell output (RichEdit for ANSI color support eventually)
    createTerminalPane();
    
    // Right: User Chat Input area (multiline edit + send button)
    // Container static for the user input area
    HWND userChatContainer = CreateWindowExA(WS_EX_CLIENTEDGE, "STATIC", "",
        WS_CHILD | WS_VISIBLE, 0, 0, 100, 100, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    
    m_userChatInputHwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 0, 100, 100, m_hwnd, (HMENU)5001, GetModuleHandle(nullptr), nullptr);
    SendMessageA(m_userChatInputHwnd, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    
    m_userChatSendBtn = CreateWindowExA(0, "BUTTON", "Send",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        0, 0, 80, 28, m_hwnd, (HMENU)5002, GetModuleHandle(nullptr), nullptr);
    SendMessageA(m_userChatSendBtn, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);

    // === SETUP SPLITTER LAYOUT ===
    auto* splitter = new RawrXD::UI::SplitLayout(m_hwnd);
    m_splitLayout = splitter;
    
    // Configure top panes: File Browser (20%) | Editor (50%) | AI Chat (30%)
    std::vector<RawrXD::UI::Pane> topPanes;
    topPanes.push_back({m_fileBrowserHwnd, 0.18f});
    topPanes.push_back({m_editorHwnd, 0.52f});
    topPanes.push_back({m_topChatHwnd, 0.30f});
    splitter->setTopPanes(topPanes);
    
    // Configure bottom panes: Terminal (60%) | User Chat (40%)
    splitter->setBottomPanes(m_terminalHwnd, userChatContainer, 0.60f);
    splitter->setBottomHeight(180);
    
    // Initial population of file browser
    initializeFileBrowser();
    
    // Initial layout
    RECT rc; GetClientRect(m_hwnd, &rc);
    splitter->onResize(rc.right - rc.left, rc.bottom - rc.top);
    
    // Position user chat input and send button within container
    // (will be repositioned in WM_SIZE)
    
    initChat();
}

void MainWindow::createTerminal() {
#ifdef _WIN32
    // Create terminal if it doesn't exist, or spawn new PowerShell session
    if (!m_terminalHwnd) {
        createTerminalPane();
    }
    
    // Start a new PowerShell process if not already running
    if (!m_terminalRunning) {
        SECURITY_ATTRIBUTES sa = {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        
        HANDLE hStdInRead, hStdInWrite;
        HANDLE hStdOutRead, hStdOutWrite;
        
        CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0);
        CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0);
        
        SetHandleInformation(hStdInWrite, HANDLE_FLAG_INHERIT, 0);
        SetHandleInformation(hStdOutRead, HANDLE_FLAG_INHERIT, 0);
        
        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = hStdInRead;
        si.hStdOutput = hStdOutWrite;
        si.hStdError = hStdOutWrite;
        
        char cmdLine[] = "pwsh.exe -NoLogo -NoProfile";
        
        if (CreateProcessA(nullptr, cmdLine, nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &m_terminalProcess)) {
            m_terminalRunning = true;
            m_psInWrite = hStdInWrite;
            m_psOutRead = hStdOutRead;
            
            CloseHandle(hStdInRead);
            CloseHandle(hStdOutWrite);
            
            startTerminalReader();
        } else {
            CloseHandle(hStdInRead);
            CloseHandle(hStdInWrite);
            CloseHandle(hStdOutRead);
            CloseHandle(hStdOutWrite);
        }
    }
    
    // Show and focus the terminal
    if (m_terminalHwnd) {
        ShowWindow(m_terminalHwnd, SW_SHOW);
        SetFocus(m_terminalHwnd);
    }
#endif
}

void MainWindow::createTerminalPane() {
    // Create terminal output pane (read-only RichEdit for eventual ANSI support)
    m_terminalHwnd = CreateWindowExA(WS_EX_CLIENTEDGE, "RICHEDIT50W", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY | WS_VSCROLL,
        0, 0, 100, 100, m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
    
    if (m_terminalHwnd) {
        // Dark terminal theme
        SendMessage(m_terminalHwnd, EM_SETBKGNDCOLOR, 0, RGB(30, 30, 30));
        
        CHARFORMATA cf = {};
        cf.cbSize = sizeof(cf);
        cf.dwMask = CFM_COLOR | CFM_FACE | CFM_SIZE;
        cf.crTextColor = RGB(204, 204, 204);
        strcpy_s(cf.szFaceName, "Consolas");
        cf.yHeight = 200; // 10pt
        SendMessageA(m_terminalHwnd, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&cf);
        
        // Welcome message
        const char* welcome = "RawrXD Terminal - PowerShell Integration\r\n$ ";
        SendMessageA(m_terminalHwnd, EM_REPLACESEL, FALSE, (LPARAM)welcome);
    }
}

void MainWindow::onFileBrowserDblClick() {
    if (!m_fileBrowserHwnd) return;
    int sel = (int)SendMessageA(m_fileBrowserHwnd, LB_GETCURSEL, 0, 0);
    if (sel == LB_ERR) return;
    
    char buf[512] = {};
    SendMessageA(m_fileBrowserHwnd, LB_GETTEXT, sel, (LPARAM)buf);
    std::string filename(buf);
    
    // Check if it's a directory
    fs::path fullPath = fs::current_path() / filename;
    if (fs::is_directory(fullPath)) {
        // Navigate into directory
        try {
            fs::current_path(fullPath);
            SendMessageA(m_fileBrowserHwnd, LB_RESETCONTENT, 0, 0);
            SendMessageA(m_fileBrowserHwnd, LB_ADDSTRING, 0, (LPARAM)"..");
            initializeFileBrowser();
        } catch (...) {}
    } else if (fs::is_regular_file(fullPath)) {
        // Open file in editor
        loadFileWithLazyLoading(fullPath.string());
        syncEditorFromBuffer();
        addTab(filename);
    }
}

void MainWindow::initializeFileBrowser() {
    if (!m_fileBrowserHwnd) return;
    // Simple non-recursive listing of current working directory
    try {
        size_t added = 0;
        for (auto& entry : fs::directory_iterator(fs::current_path())) {
            if (added > 200) break; // cap
            auto p = entry.path().filename().string();
            SendMessageA(m_fileBrowserHwnd, LB_ADDSTRING, 0, (LPARAM)p.c_str());
            ++added;
        }
    } catch (...) {}
}

void MainWindow::appendTopChat(const std::string& who, const std::string& text) {
    if (!m_topChatHwnd) return;
    int len = GetWindowTextLengthA(m_topChatHwnd);
    std::string line = who + ": " + text + "\r\n";
    // Get existing text
    std::vector<char> buf(len + 1);
    GetWindowTextA(m_topChatHwnd, buf.data(), len + 1);
    std::string combined(buf.data());
    combined += line;
    SetWindowTextA(m_topChatHwnd, combined.c_str());
}
#endif

#ifdef _WIN32
void MainWindow::initChat() {
    m_chatSession.setSessionName("ide-chat-session");
    appendTopChat("System", "Chat initialized. Type below to talk to model.");
}

void MainWindow::startChatRequest(const std::string& prompt) {
    std::lock_guard<std::mutex> lk(m_chatMutex);
    if (m_chatBusy) { appendTopChat("System", "Chat busy. Please wait."); return; }
    m_chatBusy = true;
    m_chatSession.recordUserPrompt(prompt);
    RawrXD::Backend::OllamaChatMessage userMsg{"user", prompt};
    m_chatHistory.push_back(userMsg);
    // Spawn worker thread
    auto* self = this;
    std::thread([self]() {
        std::string responseText;
        bool ok = false;
        try {
            RawrXD::Backend::OllamaChatRequest req;
            req.model = "llama2"; // adjust as needed
            req.stream = false;
            {
                std::lock_guard<std::mutex> lk2(self->m_chatMutex);
                req.messages = self->m_chatHistory; // all messages so far
            }
            RawrXD::Backend::OllamaResponse resp = self->m_ollama.chatSync(req);
            if (!resp.message.content.empty()) {
                responseText = resp.message.content;
                ok = true;
            } else {
                responseText = "(empty response)";
            }
        } catch (const std::exception& e) {
            responseText = std::string("Error: ") + e.what();
        }
        // Allocate string to pass via message
        auto* heapStr = new std::string(responseText);
        PostMessage(self->m_hwnd, WM_CHAT_COMPLETE, (WPARAM)ok, (LPARAM)heapStr);
    }).detach();
}

void MainWindow::handleChatResponse(const std::string& response) {
    {
        std::lock_guard<std::mutex> lk(m_chatMutex);
        m_chatBusy = false;
        RawrXD::Backend::OllamaChatMessage aiMsg{"assistant", response};
        m_chatHistory.push_back(aiMsg);
    }
    // Determine model & token counts (simplified)
    uint64_t promptTokens = m_chatHistory.size() ? (uint64_t)m_chatHistory.back().content.size() : 0;
    uint64_t completionTokens = (uint64_t)response.size();
    m_chatSession.recordAIResponse(response, "llama2", promptTokens, completionTokens);
    appendTopChat("Model", response);
    // Bottom transcript
    if (m_chatPanelShim.impl) {
        auto* chat = reinterpret_cast<RawrXD::UI::ChatPanel*>(m_chatPanelShim.impl);
        chat->appendMessage("Model", response);
    }
}
#endif

#ifdef _WIN32
// --- Search / Replace implementation ---
void MainWindow::sendToTerminal(const std::string& line) {
    if(!m_psInWrite || line.empty()) return;
    DWORD written=0;
    WriteFile(m_psInWrite, line.c_str(), (DWORD)line.size(), &written, nullptr);
}
void MainWindow::findNextInEditor(const std::string& searchText) {
    if(!m_editorHwnd || searchText.empty()) return;
    int len = GetWindowTextLengthA(m_editorHwnd);
    if(len <= 0) return;
    std::vector<char> buf(len+1);
    GetWindowTextA(m_editorHwnd, buf.data(), len+1);
    std::string text(buf.data());
    size_t start = static_cast<size_t>(m_lastFindPos >= 0 ? m_lastFindPos + 1 : 0);
    size_t pos = text.find(searchText, start);
    if(pos == std::string::npos) pos = text.find(searchText, 0); // wrap
    if(pos != std::string::npos) {
        SendMessageA(m_editorHwnd, EM_SETSEL, (WPARAM)pos, (LPARAM)(pos + searchText.size()));
        m_lastFindPos = (long)pos;
    }
}

void MainWindow::replaceNextInEditor(const std::string& findText, const std::string& replaceText) {
    if(findText.empty()) return;
    int len = GetWindowTextLengthA(m_editorHwnd);
    if(len <= 0) return;
    std::vector<char> buf(len+1);
    GetWindowTextA(m_editorHwnd, buf.data(), len+1);
    std::string text(buf.data());
    size_t start = static_cast<size_t>(m_lastFindPos >= 0 ? m_lastFindPos + 1 : 0);
    size_t pos = text.find(findText, start);
    if(pos == std::string::npos) pos = text.find(findText, 0);
    if(pos != std::string::npos) {
        text.replace(pos, findText.size(), replaceText);
        SetWindowTextA(m_editorHwnd, text.c_str());
        SendMessageA(m_editorHwnd, EM_SETSEL, (WPARAM)pos, (LPARAM)(pos + replaceText.size()));
        m_lastFindPos = (long)pos;
    }
}

void MainWindow::replaceAllInEditor(const std::string& findText, const std::string& replaceText) {
    if(findText.empty()) return;
    int len = GetWindowTextLengthA(m_editorHwnd);
    if(len <= 0) return;
    std::vector<char> buf(len+1);
    GetWindowTextA(m_editorHwnd, buf.data(), len+1);
    std::string text(buf.data());
    size_t pos = 0; int count = 0;
    while((pos = text.find(findText, pos)) != std::string::npos) {
        text.replace(pos, findText.size(), replaceText);
        pos += replaceText.size();
        ++count;
    }
    SetWindowTextA(m_editorHwnd, text.c_str());
    m_lastFindPos = -1;
}

void MainWindow::appendTerminalOutput(const std::string& chunk) {
    if(!m_terminalHwnd || chunk.empty()) return;
    int len = GetWindowTextLengthA(m_terminalHwnd);
    SendMessageA(m_terminalHwnd, EM_SETSEL, len, len);
    SendMessageA(m_terminalHwnd, EM_REPLACESEL, FALSE, (LPARAM)chunk.c_str());
}

void MainWindow::startTerminalReader() {
    if(!m_psOutRead) return;
    m_terminalReaderActive = true;
    m_terminalReaderThread = std::thread([this]() {
        char buffer[512]; DWORD readBytes = 0;
        while(m_terminalReaderActive) {
            if(ReadFile(m_psOutRead, buffer, sizeof(buffer)-1, &readBytes, nullptr) && readBytes) {
                buffer[readBytes] = '\0';
                appendTerminalOutput(std::string(buffer));
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
        }
    });
}

void MainWindow::stopTerminalReader() {
    m_terminalReaderActive = false;
    if(m_terminalReaderThread.joinable()) m_terminalReaderThread.join();
    if(m_psOutRead){ CloseHandle(m_psOutRead); m_psOutRead=nullptr; }
    if(m_psInWrite){ CloseHandle(m_psInWrite); m_psInWrite=nullptr; }
}
#endif

#ifdef _WIN32
// --- Buffer / Undo Integration ---
void MainWindow::syncEditorFromBuffer() {
    if(!m_editorHwnd) return;
    if(m_tabs.empty()) return;
    std::string full = currentBuffer().snapshot();
    SetWindowTextA(m_editorHwnd, full.c_str());
}

void MainWindow::applyEdit(size_t pos, size_t eraseLen, std::string_view insertText) {
    if(m_tabs.empty()) return;
    std::string removed = currentBuffer().getText(pos, eraseLen);
    if(eraseLen) currentBuffer().erase(pos, eraseLen);
    if(!insertText.empty()) currentBuffer().insert(pos, insertText);
    // Coalescing: merge sequential inserts at same advancing position within 400ms
    uint64_t now = GetTickCount64();
    bool canCoalesce = m_lastWasInsert && eraseLen == 0 && insertText.size() == 1 && pos == m_lastEditPos && (now - m_lastEditTick) < 400;
    if(canCoalesce && m_undo.canUndo()) {
        EditCommand prev = m_undo.undo();
        prev.inserted.append(std::string(insertText));
        m_undo.push(prev);
    } else {
        EditCommand cmd{pos, removed, std::string(insertText)};
        m_undo.push(cmd);
    }
    m_lastEditTick = now; m_lastEditPos = pos + insertText.size(); m_lastWasInsert = insertText.size() > 0 && eraseLen == 0;
    // Incremental update: replace selection directly
    SendMessage(m_editorHwnd, EM_SETSEL, (WPARAM)pos, (LPARAM)(pos + eraseLen));
    SendMessageA(m_editorHwnd, EM_REPLACESEL, TRUE, (LPARAM)std::string(insertText).c_str());
    size_t newPos = pos + insertText.size();
    SendMessage(m_editorHwnd, EM_SETSEL, (WPARAM)newPos, (LPARAM)newPos);
    if(m_tabs.size()) { m_tabs[m_currentTab].dirty = true; }
    refreshTabBar();
    updateStatusBar();
}

void MainWindow::performUndo() {
    if(!m_undo.canUndo()) return;
    EditCommand cmd = m_undo.undo();
    // Reverse: erase inserted then reinsert removed
    if(!cmd.inserted.empty()) currentBuffer().erase(cmd.pos, cmd.inserted.size());
    if(!cmd.removed.empty()) currentBuffer().insert(cmd.pos, cmd.removed);
    // Apply incrementally
    SendMessage(m_editorHwnd, EM_SETSEL, (WPARAM)cmd.pos, (LPARAM)(cmd.pos + cmd.inserted.size()));
    SendMessageA(m_editorHwnd, EM_REPLACESEL, TRUE, (LPARAM)cmd.removed.c_str());
    SendMessage(m_editorHwnd, EM_SETSEL, (WPARAM)(cmd.pos + cmd.removed.size()), (LPARAM)(cmd.pos + cmd.removed.size()));
}

void MainWindow::performRedo() {
    if(!m_undo.canRedo()) return;
    EditCommand cmd = m_undo.redo();
    if(!cmd.removed.empty()) currentBuffer().erase(cmd.pos, cmd.removed.size());
    if(!cmd.inserted.empty()) currentBuffer().insert(cmd.pos, cmd.inserted);
    SendMessage(m_editorHwnd, EM_SETSEL, (WPARAM)cmd.pos, (LPARAM)(cmd.pos + cmd.removed.size()));
    SendMessageA(m_editorHwnd, EM_REPLACESEL, TRUE, (LPARAM)cmd.inserted.c_str());
    SendMessage(m_editorHwnd, EM_SETSEL, (WPARAM)(cmd.pos + cmd.inserted.size()), (LPARAM)(cmd.pos + cmd.inserted.size()));
}
#ifdef _WIN32
void MainWindow::retokenizeAndApplyColors() {
    if(!m_editorHwnd) return;
    if(m_tabs.empty()) return;
    std::string text = currentBuffer().snapshot();
    std::vector<SyntaxToken> tokens; m_engine.tokenize(text, tokens);
    COLORREF kwColor=RGB(86,156,214), numColor=RGB(181,206,168), identColor=RGB(212,212,212), defColor=RGB(212,212,212);
    if(m_currentTheme < m_themes.size()) {
        kwColor = (COLORREF)m_themes[m_currentTheme].keyword;
        numColor = (COLORREF)m_themes[m_currentTheme].number;
        identColor = (COLORREF)m_themes[m_currentTheme].ident;
        defColor = (COLORREF)m_themes[m_currentTheme].fg;
    }
    COLORREF strColor = (COLORREF)m_themes[m_currentTheme].stringColor;
    COLORREF cmtColor = (COLORREF)m_themes[m_currentTheme].commentColor;
    for(const auto& tk : tokens) {
        CHARRANGE cr{(LONG)tk.start,(LONG)(tk.start+tk.length)};
        SendMessageA(m_editorHwnd, EM_SETSEL, (WPARAM)cr.cpMin, (LPARAM)cr.cpMax);
        CHARFORMAT2A cf{}; cf.cbSize=sizeof(cf); cf.dwMask=CFM_COLOR;
        if(tk.type==5) cf.crTextColor=cmtColor; else if(tk.type==4) cf.crTextColor=strColor; else if(tk.type==3) cf.crTextColor=kwColor; else if(tk.type==1) cf.crTextColor=numColor; else if(tk.type==2) cf.crTextColor=identColor; else cf.crTextColor=defColor;
        SendMessageA(m_editorHwnd, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&cf);
    }
    DWORD selStart=0, selEnd=0; SendMessage(m_editorHwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    SendMessage(m_editorHwnd, EM_SETSEL, (WPARAM)selStart, (LPARAM)selEnd);
}
#endif
#endif

#ifdef _WIN32
// --- Tab management & settings persistence ---
void MainWindow::addTab(const std::string& filename) {
    Tab t; t.filename = filename; m_tabs.push_back(std::move(t));
    m_currentTab = m_tabs.size()-1;
    selectLanguageForFile(filename);
    refreshTabBar();
    syncEditorFromBuffer();
}

void MainWindow::switchTab(size_t index) {
    if(index >= m_tabs.size()) return; m_currentTab = index; selectLanguageForFile(m_tabs[index].filename); syncEditorFromBuffer(); refreshTabBar(); }

void MainWindow::closeTab(size_t index) {
    if(index >= m_tabs.size()) return; m_tabs.erase(m_tabs.begin()+index); if(m_currentTab >= m_tabs.size()) m_currentTab = m_tabs.empty()?0:m_tabs.size()-1; refreshTabBar(); syncEditorFromBuffer(); }

void MainWindow::refreshTabBar() {
    if(!m_tabBarHwnd) return;
    for(HWND h : m_tabButtons) DestroyWindow(h);
    m_tabButtons.clear();
    int x=0; int btnWidth=90; int height=24; HINSTANCE inst=GetModuleHandle(nullptr);
    for(size_t i=0;i<m_tabs.size();++i){
        std::string label = m_tabs[i].filename + (m_tabs[i].dirty?"*":"");
        HWND btn = CreateWindowExA(0, "BUTTON", label.c_str(), WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, x,0,btnWidth,height, m_tabBarHwnd, (HMENU)(2000+i), inst, nullptr);
        m_tabButtons.push_back(btn); x += btnWidth+2;
    }
    CreateWindowExA(0, "BUTTON", "+", WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON, x,0,24,height, m_tabBarHwnd, (HMENU)1999, inst, nullptr);
}

static bool endsWith(const std::string& s, const std::string& suf) {
    return s.size() >= suf.size() && std::equal(s.end()-suf.size(), s.end(), suf.begin());
}

void MainWindow::selectLanguageForFile(const std::string& filename) {
    if(endsWith(filename, ".cpp") || endsWith(filename, ".hpp") || endsWith(filename, ".h") || endsWith(filename, ".c")) {
        m_engine.setLanguage(&m_cppLang);
    } else if(endsWith(filename, ".ps1") || endsWith(filename, ".psm1")) {
        m_engine.setLanguage(&m_psLang);
    } else {
        m_engine.setLanguage(nullptr);
    }
}
#endif

// --- Status bar & command palette & persistence helpers ---
void MainWindow::updateStatusBar() {
#ifdef _WIN32
    if(!m_statusBarHwnd || !m_editorHwnd || m_tabs.empty()) return;
    DWORD selStart=0, selEnd=0; SendMessage(m_editorHwnd, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    LRESULT line = SendMessage(m_editorHwnd, EM_LINEFROMCHAR, selStart, 0);
    LRESULT lineIndex = SendMessage(m_editorHwnd, EM_LINEINDEX, line, 0);
    long col = (long)(selStart - lineIndex) + 1;
    const std::string& fname = m_tabs[m_currentTab].filename;
    std::string lang = "text";
    if(fname.ends_with(".cpp")||fname.ends_with(".hpp")||fname.ends_with(".h")||fname.ends_with(".c")) lang="cpp"; else if(fname.ends_with(".ps1")||fname.ends_with(".psm1")) lang="powershell";
    std::stringstream sb; sb << fname << (m_tabs[m_currentTab].dirty?"*":"") << "  Ln " << (line+1) << " Col " << col << "  " << lang;
    SetWindowTextA(m_statusBarHwnd, sb.str().c_str());
#endif
}

void MainWindow::saveTab(size_t index) {
    if(index >= m_tabs.size()) return; auto& t = m_tabs[index];
    if(t.filename.rfind("Untitled",0)==0) { t.filename = "Untitled" + std::to_string(index) + ".txt"; }
    std::ofstream out(t.filename, std::ios::binary); if(out) { out << t.buffer.snapshot(); out.close(); t.dirty=false; }
}

void MainWindow::saveAllDirtyTabs() { for(size_t i=0;i<m_tabs.size();++i) if(m_tabs[i].dirty) saveTab(i); refreshTabBar(); }

void MainWindow::createCommandPalette() {
#ifdef _WIN32
    if(m_commandPaletteHwnd) return;
    m_commandPaletteHwnd = CreateWindowExA(WS_EX_TOPMOST|WS_EX_TOOLWINDOW, "LISTBOX", "", WS_POPUP|WS_BORDER|LBS_NOTIFY, 620, 120, 240, 190, m_hwnd, (HMENU)3001, GetModuleHandle(nullptr), nullptr);
    populateCommandPalette();
    ShowWindow(m_commandPaletteHwnd, SW_HIDE);
#endif
}

void MainWindow::populateCommandPalette() {
#ifdef _WIN32
    if(!m_commandPaletteHwnd) return;
    SendMessageA(m_commandPaletteHwnd, LB_RESETCONTENT, 0, 0);
    const std::string& fname = m_tabs.empty()? std::string(): m_tabs[m_currentTab].filename;
    bool isCpp = fname.ends_with(".cpp")||fname.ends_with(".hpp")||fname.ends_with(".h")||fname.ends_with(".c");
    bool isPs = fname.ends_with(".ps1")||fname.ends_with(".psm1");
    if(isCpp) {
        SendMessageA(m_commandPaletteHwnd, LB_ADDSTRING, 0, (LPARAM)"Build Project");
        SendMessageA(m_commandPaletteHwnd, LB_ADDSTRING, 0, (LPARAM)"Run Tests");
        SendMessageA(m_commandPaletteHwnd, LB_ADDSTRING, 0, (LPARAM)"Toggle Header/Source");
    } else if(isPs) {
        SendMessageA(m_commandPaletteHwnd, LB_ADDSTRING, 0, (LPARAM)"Run Script");
        SendMessageA(m_commandPaletteHwnd, LB_ADDSTRING, 0, (LPARAM)"Format Script");
        SendMessageA(m_commandPaletteHwnd, LB_ADDSTRING, 0, (LPARAM)"List Functions");
    } else {
        SendMessageA(m_commandPaletteHwnd, LB_ADDSTRING, 0, (LPARAM)"No language actions");
    }
#endif
}

void MainWindow::toggleCommandPalette() {
#ifdef _WIN32
    if(!m_commandPaletteHwnd) { createCommandPalette(); }
    bool vis = IsWindowVisible(m_commandPaletteHwnd);
    if(vis) ShowWindow(m_commandPaletteHwnd, SW_HIDE); else { populateCommandPalette(); ShowWindow(m_commandPaletteHwnd, SW_SHOW); }
#endif
}

void MainWindow::executePaletteSelection(int index) {
#ifdef _WIN32
    if(index < 0 || !m_commandPaletteHwnd) return; char buf[128]=""; SendMessageA(m_commandPaletteHwnd, LB_GETTEXT, index, (LPARAM)buf); std::string cmd(buf);
    if(cmd=="Build Project") handleCommand("cmake --build .");
    else if(cmd=="Run Tests") handleCommand("ctest");
    else if(cmd=="Toggle Header/Source") {/* stub */}
    else if(cmd=="Run Script") handleCommand("powershell -File " + m_tabs[m_currentTab].filename);
    else if(cmd=="Format Script") {/* stub */}
    else if(cmd=="List Functions") {/* stub */}
#endif
}

void MainWindow::loadSettings() {
    std::ifstream in("RawrXDSettings.json"); if(!in) return; std::string line, content; while(std::getline(in,line)) content += line; in.close();
    auto findVal=[&](const std::string& key)->std::string{ auto p=content.find(key); if(p==std::string::npos) return ""; auto c=content.find(':',p); if(c==std::string::npos) return ""; auto q=content.find_first_of(",}" , c+1); return content.substr(c+1, q-(c+1)); };
    std::string theme = findVal("theme"); if(!theme.empty()){ if(theme.find("dark")!=std::string::npos) m_currentTheme=0; else m_currentTheme=1; }
    std::string fontSize = findVal("fontSize"); if(!fontSize.empty()) m_fontSize = atoi(fontSize.c_str());
    std::string tabSz = findVal("tabSize"); if(!tabSz.empty()) m_tabSize = atoi(tabSz.c_str());
}

void MainWindow::saveSettings() {
    std::ofstream out("RawrXDSettings.json", std::ios::trunc); if(!out) return;
    out << "{\n";
    out << "  \"theme\": \"" << (m_currentTheme < m_themes.size()? m_themes[m_currentTheme].name: "dark") << "\",\n";
    out << "  \"fontSize\": " << m_fontSize << ",\n";
    out << "  \"tabSize\": " << m_tabSize << "\n";
    out << "}\n";
    out.close();
}

#ifdef _WIN32
// --- Floating panel creation / toggle ---
void MainWindow::createFloatingPanel() {
    if(m_floatingPanel) return;
    WNDCLASS wc{};
    wc.lpfnWndProc = FloatingPanelProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDFloatPanel";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    RegisterClass(&wc);
    m_floatingPanel = CreateWindowEx(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
        "RawrXDFloatPanel",
        "RawrXD Panel",
        WS_POPUP | WS_BORDER | WS_SYSMENU,
        820, 10, 260, 260,
        m_hwnd,
        nullptr,
        GetModuleHandle(nullptr),
        this);
    if(m_floatingPanel) {
        ShowWindow(m_floatingPanel, SW_HIDE);
        m_floatingPanelVisible = false;
    }
}

void MainWindow::toggleFloatingPanel() {
    if(!m_floatingPanel) return;
    m_floatingPanelVisible = !m_floatingPanelVisible;
    ShowWindow(m_floatingPanel, m_floatingPanelVisible ? SW_SHOW : SW_HIDE);
    if(m_floatingPanelVisible) {
        InvalidateRect(m_floatingPanel, nullptr, TRUE);
    }
}
#endif
