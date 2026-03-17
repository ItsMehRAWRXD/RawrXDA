// ============================================================================
// Win32IDE_DragDropTabs.cpp — Tier 1 Cosmetic #8: Drag-and-Drop File Tabs
// ============================================================================
// Chrome-style tab reordering via mouse drag:
//   - Click and hold on a tab to start dragging
//   - Visual drop indicator shows target position
//   - Release to reorder
//   - Tear-off detection (future: undock to new window)
//
// Pattern:  Tab bar subclass with mouse capture, no exceptions
// Threading: UI thread only
// ============================================================================

#include "Win32IDE.h"
#include <sstream>
#include <algorithm>

#ifndef RAWRXD_LOG_INFO
#define RAWRXD_LOG_INFO(msg) do { \
    std::ostringstream _oss; _oss << "[INFO] " << msg << "\n"; \
    OutputDebugStringA(_oss.str().c_str()); \
} while(0)
#endif

// Minimum drag distance before initiating drag (avoid accidental drags)
static const int TAB_DRAG_THRESHOLD = 5;

// Drop indicator colors
static const COLORREF TAB_DROP_INDICATOR_COLOR = RGB(0, 122, 204);
static const COLORREF TAB_DRAG_GHOST_BG = RGB(60, 60, 60);

// ============================================================================
// TAB BAR DRAG SUBCLASS PROCEDURE
// ============================================================================

LRESULT CALLBACK Win32IDE::TabBarDragProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    Win32IDE* pThis = (Win32IDE*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

    switch (uMsg) {
    case WM_LBUTTONDOWN:
    {
        if (!pThis) break;

        POINT pt;
        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);

        // Hit test: find which tab was clicked
        TCHITTESTINFO hti;
        hti.pt = pt;
        int tabIndex = TabCtrl_HitTest(hwnd, &hti);

        if (tabIndex >= 0) {
            pThis->m_tabDragIndex = tabIndex;
            pThis->m_tabDragStart = pt;
            pThis->m_tabDragStarted = false;
            pThis->m_tabDragging = true;
            SetCapture(hwnd);
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        if (!pThis || !pThis->m_tabDragging) break;

        POINT pt;
        pt.x = LOWORD(lParam);
        pt.y = HIWORD(lParam);

        // Check if we've exceeded the drag threshold
        if (!pThis->m_tabDragStarted) {
            int dx = abs(pt.x - pThis->m_tabDragStart.x);
            int dy = abs(pt.y - pThis->m_tabDragStart.y);
            if (dx >= TAB_DRAG_THRESHOLD || dy >= TAB_DRAG_THRESHOLD) {
                pThis->m_tabDragStarted = true;
                SetCursor(LoadCursor(nullptr, IDC_HAND));
            } else {
                break;
            }
        }

        pThis->onTabDragMove(pt);
        break;
    }

    case WM_LBUTTONUP:
    {
        if (!pThis || !pThis->m_tabDragging) break;

        ReleaseCapture();
        pThis->onTabDragEnd();
        break;
    }

    case WM_CAPTURECHANGED:
    {
        if (pThis && pThis->m_tabDragging) {
            pThis->m_tabDragging = false;
            pThis->m_tabDragStarted = false;
            pThis->m_tabDragIndex = -1;
            pThis->m_tabDragTarget = -1;
            InvalidateRect(hwnd, nullptr, TRUE);
        }
        break;
    }

    case WM_PAINT:
    {
        // Let the default tab control paint first
        LRESULT result = CallWindowProc(pThis ? pThis->m_oldTabBarProc : DefWindowProc,
                                        hwnd, uMsg, wParam, lParam);

        // Overlay drop indicator if dragging
        if (pThis && pThis->m_tabDragStarted && pThis->m_tabDragTarget >= 0) {
            HDC hdc = GetDC(hwnd);
            pThis->paintTabDropIndicator(hdc, pThis->m_tabDragTarget);
            ReleaseDC(hwnd, hdc);
        }

        return result;
    }
    }

    // Call original proc
    if (pThis && pThis->m_oldTabBarProc) {
        return CallWindowProc(pThis->m_oldTabBarProc, hwnd, uMsg, wParam, lParam);
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

// ============================================================================
// TAB DRAG BEGIN — Initialize drag state
// ============================================================================

void Win32IDE::onTabDragBegin(int tabIndex, POINT pt)
{
    m_tabDragIndex = tabIndex;
    m_tabDragStart = pt;
    m_tabDragStarted = false;
    m_tabDragging = true;
    m_tabDragTarget = -1;
}

// ============================================================================
// TAB DRAG MOVE — Update drop target based on mouse position
// ============================================================================

void Win32IDE::onTabDragMove(POINT pt)
{
    if (!m_hwndTabBar || !m_tabDragStarted) return;

    // Hit test to find which tab position we're hovering over
    TCHITTESTINFO hti;
    hti.pt = pt;
    int hoverTab = TabCtrl_HitTest(m_hwndTabBar, &hti);

    // If not hovering over any tab, determine closest edge
    if (hoverTab < 0) {
        int tabCount = TabCtrl_GetItemCount(m_hwndTabBar);
        if (tabCount > 0) {
            // Check if we're past the last tab
            RECT lastTabRect;
            TabCtrl_GetItemRect(m_hwndTabBar, tabCount - 1, &lastTabRect);
            if (pt.x >= lastTabRect.right) {
                hoverTab = tabCount; // drop at end
            } else if (pt.x <= 0) {
                hoverTab = 0; // drop at beginning
            }
        }
    }

    int oldTarget = m_tabDragTarget;
    m_tabDragTarget = hoverTab;

    // Repaint if target changed
    if (oldTarget != m_tabDragTarget) {
        InvalidateRect(m_hwndTabBar, nullptr, TRUE);
    }
}

// ============================================================================
// TAB DRAG END — Perform reorder if target differs from source
// ============================================================================

void Win32IDE::onTabDragEnd()
{
    m_tabDragging = false;
    SetCursor(LoadCursor(nullptr, IDC_ARROW));

    if (!m_tabDragStarted || m_tabDragIndex < 0 || m_tabDragTarget < 0) {
        m_tabDragStarted = false;
        m_tabDragIndex = -1;
        m_tabDragTarget = -1;
        InvalidateRect(m_hwndTabBar, nullptr, TRUE);
        return;
    }

    int from = m_tabDragIndex;
    int to = m_tabDragTarget;

    // Adjust target: if dragging right, account for removal shift
    if (to > from) to--;

    if (from != to && from >= 0 && to >= 0) {
        reorderTab(from, to);
    }

    m_tabDragStarted = false;
    m_tabDragIndex = -1;
    m_tabDragTarget = -1;
    InvalidateRect(m_hwndTabBar, nullptr, TRUE);
}

// ============================================================================
// REORDER TAB — Move tab from one index to another
// ============================================================================

void Win32IDE::reorderTab(int fromIndex, int toIndex)
{
    if (fromIndex < 0 || fromIndex >= (int)m_editorTabs.size()) return;
    if (toIndex < 0) toIndex = 0;
    if (toIndex >= (int)m_editorTabs.size()) toIndex = (int)m_editorTabs.size() - 1;
    if (fromIndex == toIndex) return;

    // Remove tab from source position
    EditorTab tab = m_editorTabs[fromIndex];
    m_editorTabs.erase(m_editorTabs.begin() + fromIndex);

    // Insert at target position
    if (toIndex > (int)m_editorTabs.size()) toIndex = (int)m_editorTabs.size();
    m_editorTabs.insert(m_editorTabs.begin() + toIndex, tab);

    // Update active tab index
    if (m_activeTabIndex == fromIndex) {
        m_activeTabIndex = toIndex;
    } else if (fromIndex < m_activeTabIndex && toIndex >= m_activeTabIndex) {
        m_activeTabIndex--;
    } else if (fromIndex > m_activeTabIndex && toIndex <= m_activeTabIndex) {
        m_activeTabIndex++;
    }

    // Rebuild the physical tab control to reflect new order
    if (m_hwndTabBar) {
        int tabCount = TabCtrl_GetItemCount(m_hwndTabBar);
        
        // Clear and re-add all tabs
        TabCtrl_DeleteAllItems(m_hwndTabBar);
        for (int i = 0; i < (int)m_editorTabs.size(); i++) {
            TCITEMA tie = { TCIF_TEXT };
            std::string label = m_editorTabs[i].displayName;
            if (m_editorTabs[i].modified) label += " *";
            tie.pszText = const_cast<char*>(label.c_str());
            TabCtrl_InsertItem(m_hwndTabBar, i, &tie);
        }

        // Restore selection
        TabCtrl_SetCurSel(m_hwndTabBar, m_activeTabIndex);
    }

    RAWRXD_LOG_INFO("Tab reordered: " << fromIndex << " -> " << toIndex);
}

// ============================================================================
// PAINT TAB DROP INDICATOR — Blue insertion line
// ============================================================================

void Win32IDE::paintTabDropIndicator(HDC hdc, int targetIndex)
{
    if (!m_hwndTabBar || targetIndex < 0) return;

    int tabCount = TabCtrl_GetItemCount(m_hwndTabBar);
    int insertX = 0;

    if (targetIndex < tabCount) {
        RECT tabRect;
        TabCtrl_GetItemRect(m_hwndTabBar, targetIndex, &tabRect);
        insertX = tabRect.left;
    } else if (tabCount > 0) {
        RECT lastRect;
        TabCtrl_GetItemRect(m_hwndTabBar, tabCount - 1, &lastRect);
        insertX = lastRect.right;
    }

    RECT clientRect;
    GetClientRect(m_hwndTabBar, &clientRect);

    // Draw vertical insertion line
    HPEN hPen = CreatePen(PS_SOLID, 3, TAB_DROP_INDICATOR_COLOR);
    HPEN oldPen = (HPEN)SelectObject(hdc, hPen);

    MoveToEx(hdc, insertX, clientRect.top + 2, nullptr);
    LineTo(hdc, insertX, clientRect.bottom - 2);

    // Draw small triangle indicator at top
    POINT triangle[3];
    triangle[0] = { insertX - 4, clientRect.top };
    triangle[1] = { insertX + 4, clientRect.top };
    triangle[2] = { insertX, clientRect.top + 5 };

    HBRUSH hBrush = CreateSolidBrush(TAB_DROP_INDICATOR_COLOR);
    HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);
    Polygon(hdc, triangle, 3);

    SelectObject(hdc, oldBrush);
    DeleteObject(hBrush);
    SelectObject(hdc, oldPen);
    DeleteObject(hPen);
}
