// Win32IDE_SearchPanel.cpp - Real Search
#include "Win32IDE.h"
#include <commctrl.h>

void Win32IDE::createSearchPanel() {
    if (!m_hwndSidebarContent) return;
    
    RECT rc;
    GetClientRect(m_hwndSidebarContent, &rc);
    
    // Search input
    CreateWindowExA(0, "STATIC", "Search:", WS_CHILD | WS_VISIBLE,
        5, 5, 60, 20, m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    m_hwndSearchInput = CreateWindowExA(
        WS_EX_CLIENTEDGE, "EDIT", "",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
        70, 5, rc.right - 150, 20,
        m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    CreateWindowExA(0, "BUTTON", "Search", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        rc.right - 75, 5, 70, 20, m_hwndSidebarContent, (HMENU)IDM_EDIT_FIND, m_hInstance, nullptr);
    
    // Results
    m_hwndSearchResults = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        0, 30, rc.right, rc.bottom - 30,
        m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPSTR)"File";
    col.cx = 200;
    ListView_InsertColumn(m_hwndSearchResults, 0, &col);
    
    col.pszText = (LPSTR)"Line";
    col.cx = 50;
    ListView_InsertColumn(m_hwndSearchResults, 1, &col);
    
    col.pszText = (LPSTR)"Text";
    col.cx = 400;
    ListView_InsertColumn(m_hwndSearchResults, 2, &col);
}

void Win32IDE::performSearch() {
    if (!m_hwndSearchInput || !m_hwndSearchResults) return;
    
    char query[256];
    GetWindowTextA(m_hwndSearchInput, query, sizeof(query));
    if (strlen(query) == 0) return;
    
    ListView_DeleteAllItems(m_hwndSearchResults);
    
    // Search current directory
    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA("*.*", &findData);
    
    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                std::string filename = findData.cFileName;
                std::ifstream file(filename);
                std::string line;
                int lineNum = 0;
                
                while (std::getline(file, line)) {
                    lineNum++;
                    if (line.find(query) != std::string::npos) {
                        LVITEMA item = {};
                        item.mask = LVIF_TEXT;
                        item.iItem = ListView_GetItemCount(m_hwndSearchResults);
                        item.pszText = (LPSTR)filename.c_str();
                        int idx = ListView_InsertItem(m_hwndSearchResults, &item);
                        
                        char lineStr[16];
                        sprintf_s(lineStr, "%d", lineNum);
                        ListView_SetItemText(m_hwndSearchResults, idx, 1, lineStr);
                        ListView_SetItemText(m_hwndSearchResults, idx, 2, (LPSTR)line.c_str());
                    }
                }
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }
}
