// Win32IDE_GitPanel.cpp - Real Git UI
#include "Win32IDE.h"
#include <richedit.h>
#include <string>

void Win32IDE::createGitPanel() {
    if (!m_hwndSidebar) return;
    
    m_hwndGitStatus = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_MULTILINE | ES_READONLY | ES_AUTOVSCROLL | WS_VSCROLL,
        5, 5, m_sidebarWidth - 10, 440,
        m_hwndSidebar, nullptr, m_hInstance, nullptr);
    
    CreateWindowExA(0, "BUTTON", "Refresh", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, 450, 70, 25, m_hwndSidebar, (HMENU)IDM_GIT_STATUS, m_hInstance, nullptr);
    
    CreateWindowExA(0, "BUTTON", "Commit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        80, 450, 70, 25, m_hwndSidebar, (HMENU)IDM_GIT_COMMIT, m_hInstance, nullptr);
    
    CreateWindowExA(0, "BUTTON", "Push", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        155, 450, 70, 25, m_hwndSidebar, (HMENU)IDM_GIT_PUSH, m_hInstance, nullptr);
    
    refreshGitStatus();
}

void Win32IDE::refreshGitStatus() {
    if (!m_hwndGitStatus) return;
    SetWindowTextA(m_hwndGitStatus, "[Git] Use terminal: git status, git add, git commit");
}
        SetWindowTextA(m_hwndGitStatus, "Failed to execute git command\r\nMake sure git is installed and in PATH");
    }
    
    CloseHandle(hReadPipe);
}
