#include "AgenticUIBridge.h"
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD {
namespace Agentic {

// ============================================================================
// CONSTRUCTION / DESTRUCTION
// ============================================================================

AgenticUIBridge::AgenticUIBridge() = default;

AgenticUIBridge::~AgenticUIBridge() {
    shutdown();
}

// ============================================================================
// LIFECYCLE
// ============================================================================

bool AgenticUIBridge::initialize(HWND hwndParent, HINSTANCE hInstance) {
    m_hwndParent = hwndParent;
    m_hInstance = hInstance;

    // Create fonts
    m_hFontTitle = CreateFontA(16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    m_hFontBody = CreateFontA(14, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                              DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                              CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");
    m_hFontSmall = CreateFontA(12, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                               DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                               CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS, "Segoe UI");

    return true;
}

void AgenticUIBridge::shutdown() {
    destroySuggestionPanel();

    if (m_hFontTitle) DeleteObject(m_hFontTitle);
    if (m_hFontBody) DeleteObject(m_hFontBody);
    if (m_hFontSmall) DeleteObject(m_hFontSmall);

    m_hwndParent = nullptr;
    m_hInstance = nullptr;
}

// ============================================================================
// MODE CONTROL
// ============================================================================

void AgenticUIBridge::toggleMode() {
    if (!m_router) return;

    AgenticMode current = m_router->getMode();
    AgenticMode next;
    
    switch (current) {
        case AgenticMode::Passive: next = AgenticMode::Suggestive; break;
        case AgenticMode::Suggestive: next = AgenticMode::Autonomous; break;
        case AgenticMode::Autonomous: next = AgenticMode::Passive; break;
        default: next = AgenticMode::Passive; break;
    }
    
    setMode(next);
}

void AgenticUIBridge::setMode(AgenticMode mode) {
    if (!m_router) return;
    m_router->setMode(mode);
    updateStatusBar();
    
    if (m_modeToggleCb) {
        m_modeToggleCb(mode);
    }

    // Show notification
    AgenticNotification notif;
    notif.id = "mode_change_" + std::to_string(GetTickCount());
    notif.title = "Agentic Mode";
    switch (mode) {
        case AgenticMode::Passive:
            notif.message = "Passive mode: IDE only responds to explicit requests";
            notif.type = AgenticNotificationType::Info;
            break;
        case AgenticMode::Suggestive:
            notif.message = "Suggestive mode: IDE will propose changes for approval";
            notif.type = AgenticNotificationType::Suggestion;
            break;
        case AgenticMode::Autonomous:
            notif.message = "Autonomous mode: IDE will execute safe actions automatically";
            notif.type = AgenticNotificationType::Warning;
            notif.autoDismiss = false;
            break;
    }
    showNotification(notif);
}

AgenticMode AgenticUIBridge::getCurrentMode() const {
    if (!m_router) return AgenticMode::Passive;
    return m_router->getMode();
}

// ============================================================================
// NOTIFICATION SYSTEM
// ============================================================================

void AgenticUIBridge::showNotification(const AgenticNotification& notification) {
    m_activeNotifications.push_back(notification);
    createNotificationPopup(notification);
    
    if (m_notificationCb) {
        m_notificationCb(notification);
    }

    // Auto-dismiss
    if (notification.autoDismiss) {
        SetTimer(nullptr, 0, notification.autoDismissSeconds * 1000,
            (TIMERPROC)[](HWND hwnd, UINT msg, UINT_PTR idEvent, DWORD time) {
                KillTimer(hwnd, idEvent);
                // Notification would be dismissed here
            });
    }
}

void AgenticUIBridge::showSuggestion(const AgenticSuggestion& suggestion) {
    AgenticNotification notif;
    notif.id = suggestion.id;
    notif.title = suggestion.title;
    notif.message = suggestion.description;
    notif.type = AgenticNotificationType::Suggestion;
    notif.dismissible = true;
    notif.autoDismiss = false;
    
    showNotification(notif);
    
    // Also add to suggestion panel
    addSuggestionToList(suggestion);
}

void AgenticUIBridge::showActionResult(const AgenticAction& action, bool success) {
    AgenticNotification notif;
    notif.id = "action_result_" + action.id;
    notif.title = success ? "Action Completed" : "Action Failed";
    notif.message = action.description;
    notif.type = success ? AgenticNotificationType::ActionCompleted : AgenticNotificationType::ActionFailed;
    notif.autoDismiss = true;
    notif.autoDismissSeconds = 5;
    
    showNotification(notif);
}

void AgenticUIBridge::dismissNotification(const std::string& id) {
    auto it = std::remove_if(m_activeNotifications.begin(), m_activeNotifications.end(),
        [&id](const AgenticNotification& n) { return n.id == id; });
    m_activeNotifications.erase(it, m_activeNotifications.end());
}

void AgenticUIBridge::dismissAllNotifications() {
    m_activeNotifications.clear();
}

// ============================================================================
// STATUS BAR
// ============================================================================

void AgenticUIBridge::updateStatusBar() {
    if (!m_hwndStatusBar || !m_router) return;

    AgenticMode mode = m_router->getMode();
    std::string status;
    
    switch (mode) {
        case AgenticMode::Passive:
            status = "Agentic: Passive | Click to enable suggestions";
            break;
        case AgenticMode::Suggestive:
            status = "Agentic: Suggestive | Review suggestions in panel";
            break;
        case AgenticMode::Autonomous:
            status = "Agentic: Autonomous | Auto-executing safe actions";
            break;
    }

    // Add pending count
    if (m_router->hasPendingSuggestions()) {
        auto suggestions = m_router->getPendingSuggestions();
        status += " | " + std::to_string(suggestions.size()) + " pending";
    }

    SetWindowTextA(m_hwndStatusBar, status.c_str());
}

// ============================================================================
// SUGGESTION PANEL
// ============================================================================

void AgenticUIBridge::showSuggestionPanel() {
    if (!m_hwndSuggestionPanel) {
        createSuggestionPanel();
    }
    if (m_hwndSuggestionPanel) {
        ShowWindow(m_hwndSuggestionPanel, SW_SHOW);
        m_suggestionPanelVisible = true;
        populateSuggestionList();
    }
}

void AgenticUIBridge::hideSuggestionPanel() {
    if (m_hwndSuggestionPanel) {
        ShowWindow(m_hwndSuggestionPanel, SW_HIDE);
        m_suggestionPanelVisible = false;
    }
}

void AgenticUIBridge::toggleSuggestionPanel() {
    if (m_suggestionPanelVisible) {
        hideSuggestionPanel();
    } else {
        showSuggestionPanel();
    }
}

void AgenticUIBridge::createSuggestionPanel() {
    if (!m_hwndParent) return;

    RECT rc;
    GetClientRect(m_hwndParent, &rc);

    m_hwndSuggestionPanel = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        "STATIC", "Agentic Suggestions",
        WS_CHILD | WS_VISIBLE | WS_BORDER,
        rc.right - m_suggestionPanelWidth, 50,
        m_suggestionPanelWidth, rc.bottom - 100,
        m_hwndParent, nullptr, m_hInstance, nullptr
    );

    if (!m_hwndSuggestionPanel) return;

    // Create list
    m_hwndSuggestionList = CreateWindowExA(
        0, "LISTBOX", "",
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOTIFY,
        5, 30, m_suggestionPanelWidth - 10, rc.bottom - 140,
        m_hwndSuggestionPanel, nullptr, m_hInstance, nullptr
    );

    // Title
    HWND hwndTitle = CreateWindowExA(
        0, "STATIC", "Agentic Suggestions",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        5, 5, m_suggestionPanelWidth - 10, 20,
        m_hwndSuggestionPanel, nullptr, m_hInstance, nullptr
    );
    SendMessage(hwndTitle, WM_SETFONT, (WPARAM)m_hFontTitle, TRUE);

    // Buttons
    HWND hwndApprove = CreateWindowExA(
        0, "BUTTON", "Approve",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        5, rc.bottom - 105, 100, 25,
        m_hwndSuggestionPanel, (HMENU)1001, m_hInstance, nullptr
    );
    SendMessage(hwndApprove, WM_SETFONT, (WPARAM)m_hFontBody, TRUE);

    HWND hwndReject = CreateWindowExA(
        0, "BUTTON", "Reject",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        110, rc.bottom - 105, 100, 25,
        m_hwndSuggestionPanel, (HMENU)1002, m_hInstance, nullptr
    );
    SendMessage(hwndReject, WM_SETFONT, (WPARAM)m_hFontBody, TRUE);

    HWND hwndDismiss = CreateWindowExA(
        0, "BUTTON", "Dismiss All",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
        215, rc.bottom - 105, 100, 25,
        m_hwndSuggestionPanel, (HMENU)1003, m_hInstance, nullptr
    );
    SendMessage(hwndDismiss, WM_SETFONT, (WPARAM)m_hFontBody, TRUE);
}

void AgenticUIBridge::destroySuggestionPanel() {
    if (m_hwndSuggestionPanel) {
        DestroyWindow(m_hwndSuggestionPanel);
        m_hwndSuggestionPanel = nullptr;
        m_hwndSuggestionList = nullptr;
    }
}

void AgenticUIBridge::populateSuggestionList() {
    if (!m_hwndSuggestionList || !m_router) return;

    SendMessage(m_hwndSuggestionList, LB_RESETCONTENT, 0, 0);
    m_displayedSuggestions.clear();

    auto suggestions = m_router->getPendingSuggestions();
    for (const auto& sug : suggestions) {
        addSuggestionToList(sug);
    }
}

void AgenticUIBridge::addSuggestionToList(const AgenticSuggestion& suggestion) {
    if (!m_hwndSuggestionList) return;

    m_displayedSuggestions.push_back(suggestion);
    std::string display = suggestion.title + " - " + suggestion.filePath;
    SendMessageA(m_hwndSuggestionList, LB_ADDSTRING, 0, (LPARAM)display.c_str());
}

void AgenticUIBridge::removeSuggestionFromList(const std::string& id) {
    auto it = std::remove_if(m_displayedSuggestions.begin(), m_displayedSuggestions.end(),
        [&id](const AgenticSuggestion& s) { return s.id == id; });
    m_displayedSuggestions.erase(it, m_displayedSuggestions.end());
    populateSuggestionList();
}

// ============================================================================
// NOTIFICATION POPUP
// ============================================================================

void AgenticUIBridge::createNotificationPopup(const AgenticNotification& notification) {
    // Simple implementation: use a tooltip-style window
    // In production, this would be a custom layered window with animations
    
    HWND hwndNotif = CreateWindowExA(
        WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_LAYERED,
        "STATIC", notification.title.c_str(),
        WS_POPUP | WS_BORDER,
        CW_USEDEFAULT, CW_USEDEFAULT,
        m_notificationWidth, m_notificationHeight,
        nullptr, nullptr, m_hInstance, nullptr
    );

    if (!hwndNotif) return;

    positionNotificationPopup(hwndNotif);
    ShowWindow(hwndNotif, SW_SHOWNA);
    animateNotificationIn(hwndNotif);

    // Auto-dismiss timer
    if (notification.autoDismiss) {
        SetTimer(hwndNotif, 1, notification.autoDismissSeconds * 1000, nullptr);
    }
}

void AgenticUIBridge::positionNotificationPopup(HWND hwnd) {
    // Position in bottom-right corner of screen
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    int x = screenWidth - m_notificationWidth - m_notificationMargin;
    int y = screenHeight - m_notificationHeight - m_notificationMargin;

    // Offset by existing notifications
    y -= (int)(m_activeNotifications.size() * (m_notificationHeight + m_notificationMargin));

    SetWindowPos(hwnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
}

void AgenticUIBridge::animateNotificationIn(HWND hwnd) {
    // Simple fade in
    SetLayeredWindowAttributes(hwnd, 0, 0, LWA_ALPHA);
    
    for (int alpha = 0; alpha <= 255; alpha += 25) {
        SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
        UpdateWindow(hwnd);
        Sleep(10);
    }
}

void AgenticUIBridge::animateNotificationOut(HWND hwnd) {
    for (int alpha = 255; alpha >= 0; alpha -= 25) {
        SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA);
        UpdateWindow(hwnd);
        Sleep(10);
    }
    DestroyWindow(hwnd);
}

// ============================================================================
// DRAWING
// ============================================================================

void AgenticUIBridge::drawSuggestionItem(HDC hdc, const RECT& rect, 
                                            const AgenticSuggestion& suggestion, bool selected) {
    // Background
    HBRUSH brush = CreateSolidBrush(selected ? COLOR_ACCENT : COLOR_BG_LIGHT);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);

    // Text
    SetTextColor(hdc, COLOR_TEXT);
    SetBkMode(hdc, TRANSPARENT);

    RECT titleRect = rect;
    titleRect.left += 5;
    titleRect.top += 2;
    titleRect.bottom = titleRect.top + 18;
    
    SelectObject(hdc, m_hFontTitle);
    DrawTextA(hdc, suggestion.title.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT descRect = rect;
    descRect.left += 5;
    descRect.top += 20;
    descRect.bottom -= 2;
    
    SelectObject(hdc, m_hFontSmall);
    DrawTextA(hdc, suggestion.description.c_str(), -1, &descRect, 
              DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);

    // Confidence indicator
    if (suggestion.confidence > 0.8f) {
        RECT confRect = rect;
        confRect.right -= 5;
        confRect.top += 2;
        confRect.bottom = confRect.top + 14;
        
        std::string confStr = std::to_string((int)(suggestion.confidence * 100)) + "%";
        SetTextColor(hdc, COLOR_SUCCESS);
        DrawTextA(hdc, confStr.c_str(), -1, &confRect, DT_RIGHT | DT_SINGLELINE);
    }
}

void AgenticUIBridge::drawNotification(HDC hdc, const RECT& rect, const AgenticNotification& notification) {
    COLORREF bgColor = COLOR_BG_DARK;
    switch (notification.type) {
        case AgenticNotificationType::Suggestion: bgColor = RGB(40, 40, 60); break;
        case AgenticNotificationType::Warning: bgColor = RGB(60, 50, 30); break;
        case AgenticNotificationType::ActionCompleted: bgColor = RGB(30, 60, 40); break;
        case AgenticNotificationType::ActionFailed: bgColor = RGB(60, 30, 30); break;
        default: break;
    }

    HBRUSH brush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);

    // Border
    HPEN pen = CreatePen(PS_SOLID, 2, COLOR_ACCENT);
    SelectObject(hdc, pen);
    Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
    DeleteObject(pen);

    // Text
    SetTextColor(hdc, COLOR_TEXT);
    SetBkMode(hdc, TRANSPARENT);

    RECT titleRect = rect;
    titleRect.left += 10;
    titleRect.top += 5;
    titleRect.bottom = titleRect.top + 20;
    
    SelectObject(hdc, m_hFontTitle);
    DrawTextA(hdc, notification.title.c_str(), -1, &titleRect, DT_LEFT | DT_SINGLELINE | DT_END_ELLIPSIS);

    RECT msgRect = rect;
    msgRect.left += 10;
    msgRect.top += 25;
    msgRect.right -= 10;
    msgRect.bottom -= 5;
    
    SelectObject(hdc, m_hFontBody);
    DrawTextA(hdc, notification.message.c_str(), -1, &msgRect, DT_LEFT | DT_WORDBREAK | DT_END_ELLIPSIS);
}

// ============================================================================
// WINDOW PROCEDURE
// ============================================================================

LRESULT CALLBACK AgenticUIBridge::SuggestionPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    AgenticUIBridge* pThis = (AgenticUIBridge*)GetWindowLongPtrA(hwnd, GWLP_USERDATA);

    switch (msg) {
        case WM_CREATE:
            return 0;

        case WM_COMMAND:
            if (LOWORD(wParam) == 1001 && pThis) { // Approve
                // Get selected suggestion and approve
                int sel = (int)SendMessage(pThis->m_hwndSuggestionList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)pThis->m_displayedSuggestions.size()) {
                    auto& sug = pThis->m_displayedSuggestions[sel];
                    if (pThis->m_router) {
                        pThis->m_router->approveAction(sug.id);
                    }
                    pThis->removeSuggestionFromList(sug.id);
                }
                return 0;
            }
            if (LOWORD(wParam) == 1002 && pThis) { // Reject
                int sel = (int)SendMessage(pThis->m_hwndSuggestionList, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)pThis->m_displayedSuggestions.size()) {
                    auto& sug = pThis->m_displayedSuggestions[sel];
                    if (pThis->m_router) {
                        pThis->m_router->rejectAction(sug.id);
                    }
                    pThis->removeSuggestionFromList(sug.id);
                }
                return 0;
            }
            if (LOWORD(wParam) == 1003 && pThis) { // Dismiss All
                if (pThis->m_router) {
                    pThis->m_router->clearSuggestions();
                }
                pThis->m_displayedSuggestions.clear();
                SendMessage(pThis->m_hwndSuggestionList, LB_RESETCONTENT, 0, 0);
                return 0;
            }
            break;

        case WM_DRAWITEM:
            if (wParam == 0 && pThis) { // Listbox
                LPDRAWITEMSTRUCT dis = (LPDRAWITEMSTRUCT)lParam;
                if (dis->itemID >= 0 && dis->itemID < (DWORD)pThis->m_displayedSuggestions.size()) {
                    pThis->drawSuggestionItem(dis->hDC, dis->rcItem, 
                        pThis->m_displayedSuggestions[dis->itemID],
                        (dis->itemState & ODS_SELECTED) != 0);
                }
                return TRUE;
            }
            break;

        case WM_DESTROY:
            pThis->m_hwndSuggestionPanel = nullptr;
            pThis->m_hwndSuggestionList = nullptr;
            return 0;
    }

    return DefWindowProcA(hwnd, msg, wParam, lParam);
}

} // namespace Agentic
} // namespace RawrXD
