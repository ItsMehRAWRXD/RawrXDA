// Win32IDE_ExtensionsPanel.cpp - Extensions View
#include "Win32IDE.h"
#include <commctrl.h>

void Win32IDE::createExtensionsView() {
    if (!m_hwndSidebarContent) return;
    
    RECT rc;
    GetClientRect(m_hwndSidebarContent, &rc);
    
    // Title
    CreateWindowExA(0, "STATIC", "Built-in Extensions",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 5, rc.right - 10, 20,
        m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    // Extensions list
    m_hwndExtensionsList = CreateWindowExA(
        WS_EX_CLIENTEDGE, WC_LISTVIEWA, "",
        WS_CHILD | WS_VISIBLE | LVS_REPORT | LVS_SINGLESEL,
        0, 30, rc.right, rc.bottom - 30,
        m_hwndSidebarContent, nullptr, m_hInstance, nullptr);
    
    LVCOLUMNA col = {};
    col.mask = LVCF_TEXT | LVCF_WIDTH;
    col.pszText = (LPSTR)"Name";
    col.cx = 200;
    ListView_InsertColumn(m_hwndExtensionsList, 0, &col);
    
    col.pszText = (LPSTR)"Status";
    col.cx = 100;
    ListView_InsertColumn(m_hwndExtensionsList, 1, &col);
    
    // Add some placeholder extensions
    LVITEMA item = {};
    item.mask = LVIF_TEXT;
    item.pszText = (LPSTR)"C/C++ Support";
    int idx = ListView_InsertItem(m_hwndExtensionsList, &item);
    ListView_SetItemText(m_hwndExtensionsList, idx, 1, (LPSTR)"Built-in");
    
    item.pszText = (LPSTR)"Git Integration";
    idx = ListView_InsertItem(m_hwndExtensionsList, &item);
    ListView_SetItemText(m_hwndExtensionsList, idx, 1, (LPSTR)"Built-in");
    
    item.pszText = (LPSTR)"MASM64 Assembly";
    idx = ListView_InsertItem(m_hwndExtensionsList, &item);
    ListView_SetItemText(m_hwndExtensionsList, idx, 1, (LPSTR)"Built-in");
    
    item.pszText = (LPSTR)"PowerShell Support";
    idx = ListView_InsertItem(m_hwndExtensionsList, &item);
    ListView_SetItemText(m_hwndExtensionsList, idx, 1, (LPSTR)"Built-in");
    
    item.pszText = (LPSTR)"Live Compiler Output";
    idx = ListView_InsertItem(m_hwndExtensionsList, &item);
    ListView_SetItemText(m_hwndExtensionsList, idx, 1, (LPSTR)"Built-in");
    
    item.pszText = (LPSTR)"Dumpbin Integration";
    idx = ListView_InsertItem(m_hwndExtensionsList, &item);
    ListView_SetItemText(m_hwndExtensionsList, idx, 1, (LPSTR)"Built-in");
}

void Win32IDE::loadInstalledExtensions() {
    // Extensions already loaded in createExtensionsView
}
