// Win32IDE_SidebarPanels.cpp - Problems, Git, Search, Extensions panels
#include "Win32IDE.h"
#include <commctrl.h>
#include <richedit.h>

// Create Problems panel with ListView
void Win32IDE::createProblemsPanel() {
    if (m_hwndSidebarContent) {
        DestroyWindow(m_hwndSidebarContent);
    }
    
    m_hwndSidebarContent = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 
        0, 0, m_sidebarWidth, 600, m_hwndSidebar, nullptr, m_hInstance, nullptr);
    
    m_hwndProblemsList = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL,
        5, 30, m_sidebarWidth - 10, 550,
        m_hwndSidebarContent, (HMENU)IDC_PANEL_PROBLEMS_LIST, m_hInstance, nullptr);
    
    ListView_SetExtendedListViewStyle(m_hwndProblemsList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
    
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 40;
    col.pszText = (LPSTR)"Sev";
    ListView_InsertColumn(m_hwndProblemsList, 0, &col);
    
    col.cx = 200;
    col.pszText = (LPSTR)"File";
    ListView_InsertColumn(m_hwndProblemsList, 1, &col);
    
    col.cx = 50;
    col.pszText = (LPSTR)"Line";
    ListView_InsertColumn(m_hwndProblemsList, 2, &col);
    
    col.cx = 300;
    col.pszText = (LPSTR)"Message";
    ListView_InsertColumn(m_hwndProblemsList, 3, &col);
}

// Create Git panel with status + commit UI
void Win32IDE::createGitPanel() {
    if (m_hwndSidebarContent) {
        DestroyWindow(m_hwndSidebarContent);
    }
    
    m_hwndSidebarContent = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE,
        0, 0, m_sidebarWidth, 600, m_hwndSidebar, nullptr, m_hInstance, nullptr);
    
    CreateWindowExA(0, "STATIC", "Changes:", WS_CHILD | WS_VISIBLE,
        5, 5, 200, 20, m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    m_hwndGitChangesList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
        5, 30, m_sidebarWidth - 10, 250,
        m_hwndSidebarContent, (HMENU)IDC_GIT_CHANGES_LIST, m_hInstance, nullptr);
    
    CreateWindowExA(0, "STATIC", "Commit Message:", WS_CHILD | WS_VISIBLE,
        5, 290, 200, 20, m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    m_hwndGitCommitMsg = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_AUTOVSCROLL | WS_VSCROLL,
        5, 315, m_sidebarWidth - 10, 100,
        m_hwndSidebarContent, (HMENU)IDC_GIT_COMMIT_MSG, m_hInstance, nullptr);
    
    m_hwndGitCommitBtn = CreateWindowExA(0, "BUTTON", "Commit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, 425, 100, 30, m_hwndSidebarContent, (HMENU)IDC_GIT_COMMIT_BTN, m_hInstance, nullptr);
    
    CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        110, 425, 100, 30, m_hwndSidebarContent, (HMENU)IDC_GIT_REFRESH_BTN, m_hInstance, nullptr);
    
    refreshGitStatus();
}

// Create Search panel with input + results
void Win32IDE::createSearchPanel() {
    if (m_hwndSidebarContent) {
        DestroyWindow(m_hwndSidebarContent);
    }
    
    m_hwndSidebarContent = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE,
        0, 0, m_sidebarWidth, 600, m_hwndSidebar, nullptr, m_hInstance, nullptr);
    
    CreateWindowExA(0, "STATIC", "Search:", WS_CHILD | WS_VISIBLE,
        5, 5, 100, 20, m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    m_hwndSearchInput = CreateWindowExA(WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        5, 30, m_sidebarWidth - 60, 25,
        m_hwndSidebarContent, (HMENU)IDC_SEARCH_INPUT, m_hInstance, nullptr);
    
    CreateWindowExA(0, "BUTTON", "Go", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        m_sidebarWidth - 50, 30, 45, 25,
        m_hwndSidebarContent, (HMENU)IDC_SEARCH_BTN, m_hInstance, nullptr);
    
    CreateWindowExA(0, "STATIC", "Results:", WS_CHILD | WS_VISIBLE,
        5, 65, 100, 20, m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    m_hwndSearchResults = CreateWindowExA(WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL | WS_VSCROLL,
        5, 90, m_sidebarWidth - 10, 500,
        m_hwndSidebarContent, (HMENU)IDC_SEARCH_RESULTS, m_hInstance, nullptr);
    
    ListView_SetExtendedListViewStyle(m_hwndSearchResults, LVS_EX_FULLROWSELECT);
    
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.cx = 200;
    col.pszText = (LPSTR)"File";
    ListView_InsertColumn(m_hwndSearchResults, 0, &col);
    
    col.cx = 50;
    col.pszText = (LPSTR)"Line";
    ListView_InsertColumn(m_hwndSearchResults, 1, &col);
    
    col.cx = 300;
    col.pszText = (LPSTR)"Text";
    ListView_InsertColumn(m_hwndSearchResults, 2, &col);
}

// Create Extensions view with installed + available lists
void Win32IDE::createExtensionsPanel() {
    if (m_hwndSidebarContent) {
        DestroyWindow(m_hwndSidebarContent);
    }
    
    m_hwndSidebarContent = CreateWindowExA(0, "STATIC", "", WS_CHILD | WS_VISIBLE,
        0, 0, m_sidebarWidth, 600, m_hwndSidebar, nullptr, m_hInstance, nullptr);
    
    CreateWindowExA(0, "STATIC", "Installed Extensions:", WS_CHILD | WS_VISIBLE,
        5, 5, 200, 20, m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    m_hwndExtensionsList = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
        5, 30, m_sidebarWidth - 10, 250,
        m_hwndSidebarContent, (HMENU)IDC_EXTENSIONS_LIST, m_hInstance, nullptr);
    
    SendMessageA(m_hwndExtensionsList, LB_ADDSTRING, 0, (LPARAM)"C/C++ IntelliSense");
    SendMessageA(m_hwndExtensionsList, LB_ADDSTRING, 0, (LPARAM)"Python Language Server");
    SendMessageA(m_hwndExtensionsList, LB_ADDSTRING, 0, (LPARAM)"Git Integration");
    SendMessageA(m_hwndExtensionsList, LB_ADDSTRING, 0, (LPARAM)"MASM64 Syntax");
    
    CreateWindowExA(0, "STATIC", "Available:", WS_CHILD | WS_VISIBLE,
        5, 295, 200, 20, m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    HWND hwndAvailable = CreateWindowExA(WS_EX_CLIENTEDGE, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | LBS_STANDARD | LBS_NOINTEGRALHEIGHT | WS_VSCROLL,
        5, 320, m_sidebarWidth - 10, 250,
        m_hwndSidebarContent, (HMENU)IDC_EXTENSIONS_AVAILABLE, m_hInstance, nullptr);
    
    SendMessageA(hwndAvailable, LB_ADDSTRING, 0, (LPARAM)"Rust Analyzer");
    SendMessageA(hwndAvailable, LB_ADDSTRING, 0, (LPARAM)"Go Language Support");
    SendMessageA(hwndAvailable, LB_ADDSTRING, 0, (LPARAM)"JavaScript/TypeScript");
}

// Switch sidebar view based on activity bar button
void Win32IDE::setSidebarView(int viewID) {
    m_currentSidebarView = (SidebarView)viewID;
    
    switch (viewID) {
        case IDC_ACTBAR_EXPLORER:
            if (m_hwndSidebarContent) ShowWindow(m_hwndSidebarContent, SW_SHOW);
            break;
        case IDC_ACTBAR_SEARCH:
            createSearchPanel();
            break;
        case IDC_ACTBAR_SCM:
            createGitPanel();
            break;
        case IDC_ACTBAR_DEBUG:
            createProblemsPanel();
            break;
        case IDC_ACTBAR_EXTENSIONS:
            createExtensionsPanel();
            break;
    }
    
    InvalidateRect(m_hwndSidebar, nullptr, TRUE);
}

// Refresh git status from git command
void Win32IDE::refreshGitStatus() {
    if (!m_hwndGitChangesList) return;
    
    SendMessageA(m_hwndGitChangesList, LB_RESETCONTENT, 0, 0);
    
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    std::string cmd = "git status --short";
    
    if (CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, TRUE,
                       0, nullptr, m_gitRepoPath.c_str(), &si, &pi)) {
        CloseHandle(hWritePipe);
        
        char buffer[4096];
        DWORD bytesRead;
        std::string output;
        
        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
            buffer[bytesRead] = '\0';
            output += buffer;
        }
        
        WaitForSingleObject(pi.hProcess, 2000);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        
        // Parse output lines
        size_t pos = 0;
        while ((pos = output.find('\n')) != std::string::npos) {
            std::string line = output.substr(0, pos);
            if (!line.empty()) {
                SendMessageA(m_hwndGitChangesList, LB_ADDSTRING, 0, (LPARAM)line.c_str());
            }
            output = output.substr(pos + 1);
        }
    } else {
        CloseHandle(hWritePipe);
    }
    
    CloseHandle(hReadPipe);
}
