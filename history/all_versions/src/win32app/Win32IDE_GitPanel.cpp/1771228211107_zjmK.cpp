// Win32IDE_GitPanel.cpp - Real Git UI
#include "Win32IDE.h"
#include <richedit.h>
#include <string>

void Win32IDE::createGitPanel() {
    if (!m_hwndSidebarContent) return;
    
    RECT rc;
    GetClientRect(m_hwndSidebarContent, &rc);
    
    // Git status output
    m_hwndGitStatus = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        0, 30, rc.right, rc.bottom - 100,
        m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    // Action buttons
    CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, rc.bottom - 65, 80, 25, m_hwndSidebarContent, (HMENU)IDM_GIT_STATUS, m_hInstance, nullptr);
    
    CreateWindowExA(0, "BUTTON", "Commit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        90, rc.bottom - 65, 80, 25, m_hwndSidebarContent, (HMENU)IDM_GIT_COMMIT, m_hInstance, nullptr);
    
    CreateWindowExA(0, "BUTTON", "Push", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        175, rc.bottom - 65, 80, 25, m_hwndSidebarContent, (HMENU)IDM_GIT_PUSH, m_hInstance, nullptr);
    
    refreshGitStatus();
}

void Win32IDE::refreshGitStatus() {
    if (!m_hwndGitStatus) return;
    
    SetWindowTextA(m_hwndGitStatus, "");
    
    // Run git status
    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;
    
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);
    
    STARTUPINFOA si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;
    
    PROCESS_INFORMATION pi = {};
    std::string cmd = "git status";
    
    // Get working directory (git repo root)
    const char* workDir = m_gitRepoPath.empty() ? nullptr : m_gitRepoPath.c_str();
    
    if (CreateProcessA(nullptr, (LPSTR)cmd.c_str(), nullptr, nullptr, TRUE,
                       0, nullptr, workDir, &si, &pi)) {
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
        
        if (output.empty()) {
            output = "No git repository found or git not installed";
        }
        SetWindowTextA(m_hwndGitStatus, output.c_str());
    } else {
        SetWindowTextA(m_hwndGitStatus, "Failed to execute git command\r\nMake sure git is installed and in PATH");
    }
    
    CloseHandle(hReadPipe);
}
