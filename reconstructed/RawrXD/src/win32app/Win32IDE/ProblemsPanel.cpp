// Win32IDE_ProblemsPanel.cpp - Problems Panel
#include "Win32IDE.h"
#include <commctrl.h>

void Win32IDE::createProblemsPanel() {
    if (!m_hwndSidebar) return;
    
    m_hwndProblemsList = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        5, 5, m_sidebarWidth - 10, 490,
        m_hwndSidebar, (HMENU)IDC_PANEL_PROBLEMS_LIST, m_hInstance, nullptr);
    
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    
    col.pszText = (LPSTR)"Severity";
    col.cx = 60;
    ListView_InsertColumn(m_hwndProblemsList, 0, &col);
    
    col.pszText = (LPSTR)"File";
    col.cx = 120;
    ListView_InsertColumn(m_hwndProblemsList, 1, &col);
    
    col.pszText = (LPSTR)"Line";
    col.cx = 40;
    ListView_InsertColumn(m_hwndProblemsList, 2, &col);
    
    col.pszText = (LPSTR)"Message";
    col.cx = 250;
    ListView_InsertColumn(m_hwndProblemsList, 3, &col);
}

void Win32IDE::addProblem(const std::string& severity, const std::string& file, int line, const std::string& message) {
    if (!m_hwndProblemsList) return;
    
    LVITEMA item = {};
    item.mask = LVIF_TEXT;
    item.iItem = ListView_GetItemCount(m_hwndProblemsList);
    item.pszText = (LPSTR)severity.c_str();
    int idx = ListView_InsertItem(m_hwndProblemsList, &item);
    
    ListView_SetItemText(m_hwndProblemsList, idx, 1, (LPSTR)file.c_str());
    
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
