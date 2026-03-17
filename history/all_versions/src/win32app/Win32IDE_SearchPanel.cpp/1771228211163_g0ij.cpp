// Win32IDE_SearchPanel.cpp - Real Search
#include "Win32IDE.h"
#include <commctrl.h>
#include <fstream>
#include <string>

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
    col.pszText = (LPSTR)"Path";
    col.cx = 300;
    ListView_InsertColumn(m_hwndSearchResults, 0, &col);
    
    col.pszText = (LPSTR)"Line";
    col.cx = 50;
    ListView_InsertColumn(m_hwndSearchResults, 1, &col);
    
    col.pszText = (LPSTR)"Match";
    col.cx = 350;
    ListView_InsertColumn(m_hwndSearchResults, 2, &col);
}

void Win32IDE::performSearch() {
    if (!m_hwndSearchInput || !m_hwndSearchResults) return;
    
    char query[256];
    GetWindowTextA(m_hwndSearchInput, query, sizeof(query));
    if (strlen(query) == 0) return;
    
    ListView_DeleteAllItems(m_hwndSearchResults);
    
    // Search recursively in src/ directory
    std::string searchRoot = m_gitRepoPath.empty() ? "." : m_gitRepoPath;
    searchDirectory(searchRoot, query, 0);
}

void Win32IDE::searchDirectory(const std::string& dir, const char* query, int depth) {
    if (depth > 10) return; // Max depth limit
    
    WIN32_FIND_DATAA findData;
    std::string pattern = dir + "\\*.*";
    HANDLE hFind = FindFirstFileA(pattern.c_str(), &findData);
    
    if (hFind == INVALID_HANDLE_VALUE) return;
    
    do {
        std::string name = findData.cFileName;
        if (name == "." || name == "..") continue;
        
        std::string fullPath = dir + "\\" + name;
        
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            // Skip common build/output directories
            if (name != "build" && name != "bin" && name != "obj" && name != ".git") {
                searchDirectory(fullPath, query, depth + 1);
            }
        } else {
            // Only search text files
            if (name.find(".cpp") != std::string::npos ||
                name.find(".h") != std::string::npos ||
                name.find(".c") != std::string::npos ||
                name.find(".asm") != std::string::npos ||
                name.find(".txt") != std::string::npos) {
                
                std::ifstream file(fullPath);
                if (!file.is_open()) continue;
                
                std::string line;
                int lineNum = 0;
                
                while (std::getline(file, line)) {
                    lineNum++;
                    if (line.find(query) != std::string::npos) {
                        // Convert to breadcrumb format
                        std::string breadcrumb = fullPath;
                        for (char& c : breadcrumb) {
                            if (c == '\\') c = '/';
                        }
                        
                        LVITEMA item = {};
                        item.mask = LVIF_TEXT;
                        item.iItem = ListView_GetItemCount(m_hwndSearchResults);
                        item.pszText = (LPSTR)breadcrumb.c_str();
                        int idx = ListView_InsertItem(m_hwndSearchResults, &item);
                        
                        char lineStr[16];
                        sprintf_s(lineStr, "%d", lineNum);
                        ListView_SetItemText(m_hwndSearchResults, idx, 1, lineStr);
                        
                        // Trim line for display
                        std::string trimmed = line;
                        if (trimmed.length() > 100) {
                            trimmed = trimmed.substr(0, 97) + "...";
                        }
                        ListView_SetItemText(m_hwndSearchResults, idx, 2, (LPSTR)trimmed.c_str());
                    }
                }
            }
        }
    } while (FindNextFileA(hFind, &findData));
    
    FindClose(hFind);
}
