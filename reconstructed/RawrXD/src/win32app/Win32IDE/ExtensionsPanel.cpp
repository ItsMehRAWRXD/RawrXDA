// Win32IDE_ExtensionsPanel.cpp - Extensions View
#include "Win32IDE.h"
#include <commctrl.h>

void Win32IDE::createExtensionsView() {
    if (!m_hwndSidebar) return;
    
    CreateWindowExA(0, "STATIC", "Built-in Extensions",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 5, m_sidebarWidth - 10, 20,
        m_hwndSidebar, nullptr, m_hInstance, nullptr);
    
    m_hwndExtensionsList = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        5, 30, m_sidebarWidth - 10, 465,
        m_hwndSidebar, nullptr, m_hInstance, nullptr);
    
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPSTR)"Name";
    col.cx = 150;
    ListView_InsertColumn(m_hwndExtensionsList, 0, &col);
    
    col.pszText = (LPSTR)"Status";
    col.cx = 100;
    ListView_InsertColumn(m_hwndExtensionsList, 1, &col);
}

void Win32IDE::loadInstalledExtensions() {
    if (!m_hwndExtensionsList) return;
}
