// Win32IDE_ProblemsPanel.cpp - Real Problems Panel
#include "Win32IDE.h"
#include <commctrl.h>

void Win32IDE::createProblemsPanel() {
    if (!m_hwndSidebarContent) return;
    
    RECT rc;
    GetClientRect(m_hwndSidebarContent, &rc);
    
    // ListView for problems
    m_hwndProblemsList = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        0, 30, rc.right, rc.bottom - 30,
        m_hwndSidebarContent, (HMENU)IDC_PANEL_PROBLEMS_LIST, m_hInstance, nullptr);
    
    // Columns
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    
    col.pszText = (LPSTR)"!";
    col.cx = 30;
    ListView_InsertColumn(m_hwndProblemsList, 0, &col);
    
    col.pszText = (LPSTR)"Path";
    col.cx = 350;
    ListView_InsertColumn(m_hwndProblemsList, 1, &col);
    
    col.pszText = (LPSTR)"Line";
    col.cx = 50;
    ListView_InsertColumn(m_hwndProblemsList, 2, &col);
    
    col.pszText = (LPSTR)"Message";
    col.cx = 400;
    ListView_InsertColumn(m_hwndProblemsList, 3, &col);
}

void Win32IDE::addProblem(const std::string& severity, const std::string& file, int line, const std::string& message) {
    if (!m_hwndProblemsList) return;
    
    // Convert file path to breadcrumb style (folder/folder/file.ext)
    std::string breadcrumb = file;
    
    // Normalize slashes
    for (char& c : breadcrumb) {
        if (c == '\\') c = '/';
    }
    
    // Strip drive letter if present (D: -> just the path)
    size_t colonPos = breadcrumb.find(':');
    if (colonPos != std::string::npos && colonPos < 3) {
        breadcrumb = breadcrumb.substr(colonPos + 1);
        if (!breadcrumb.empty() && breadcrumb[0] == '/') {
            breadcrumb = breadcrumb.substr(1);
        }
    }
    
    LVITEMA item = {};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(m_hwndProblemsList);
    item.pszText = (LPSTR)severity.c_str();
    int idx = ListView_InsertItem(m_hwndProblemsList, &item);
    
    ListView_SetItemText(m_hwndProblemsList, idx, 1, (LPSTR)breadcrumb.c_str());
    
    char lineStr[16];
    sprintf_s(lineStr, "%d", line);
    ListView_SetItemText(m_hwndProblemsList, idx, 2, lineStr);
    
    ListView_SetItemText(m_hwndProblemsList, idx, 3, (LPSTR)message.c_str());
}

void Win32IDE::clearProblems() {
    if (m_hwndProblemsList) {
        ListView_DeleteAllItems(m_hwndProblemsList);
    }
}
