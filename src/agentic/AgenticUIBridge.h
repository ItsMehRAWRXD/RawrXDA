#pragma once

#include "AgenticRouterBridge.h"
#include <windows.h>
#include <string>
#include <vector>
#include <functional>

namespace RawrXD {
namespace Agentic {

// Notification types for agentic suggestions
enum class AgenticNotificationType {
    Info = 0,
    Suggestion = 1,
    Warning = 2,
    ActionCompleted = 3,
    ActionFailed = 4
};

struct AgenticNotification {
    std::string id;
    std::string title;
    std::string message;
    AgenticNotificationType type;
    std::chrono::steady_clock::time_point timestamp;
    bool dismissible = true;
    bool autoDismiss = true;
    int autoDismissSeconds = 10;
};

// UI bridge for agentic notifications and mode control
class AgenticUIBridge {
public:
    using NotificationCallback = std::function<void(const AgenticNotification&)>;
    using ModeToggleCallback = std::function<void(AgenticMode)>;

    AgenticUIBridge();
    ~AgenticUIBridge();

    // Initialize with parent window
    bool initialize(HWND hwndParent, HINSTANCE hInstance);
    void shutdown();

    // Mode control
    void setAgenticRouter(AgenticRouterBridge* router) { m_router = router; }
    void toggleMode();
    void setMode(AgenticMode mode);
    AgenticMode getCurrentMode() const;

    // Notification system
    void showNotification(const AgenticNotification& notification);
    void showSuggestion(const AgenticSuggestion& suggestion);
    void showActionResult(const AgenticAction& action, bool success);
    void dismissNotification(const std::string& id);
    void dismissAllNotifications();

    // Status bar integration
    void updateStatusBar();
    void setStatusBarHwnd(HWND hwnd) { m_hwndStatusBar = hwnd; }

    // Suggestion panel
    void showSuggestionPanel();
    void hideSuggestionPanel();
    void toggleSuggestionPanel();
    bool isSuggestionPanelVisible() const { return m_suggestionPanelVisible; }

    // Callbacks
    void onNotification(NotificationCallback cb) { m_notificationCb = cb; }
    void onModeToggle(ModeToggleCallback cb) { m_modeToggleCb = cb; }

    // Window procedure for suggestion panel
    static LRESULT CALLBACK SuggestionPanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

private:
    void createSuggestionPanel();
    void destroySuggestionPanel();
    void populateSuggestionList();
    void addSuggestionToList(const AgenticSuggestion& suggestion);
    void removeSuggestionFromList(const std::string& id);

    void createNotificationPopup(const AgenticNotification& notification);
    void positionNotificationPopup(HWND hwnd);
    void animateNotificationIn(HWND hwnd);
    void animateNotificationOut(HWND hwnd);

    // Drawing
    void drawSuggestionItem(HDC hdc, const RECT& rect, const AgenticSuggestion& suggestion, bool selected);
    void drawNotification(HDC hdc, const RECT& rect, const AgenticNotification& notification);

    // Colors
    static constexpr COLORREF COLOR_BG_DARK = RGB(30, 30, 30);
    static constexpr COLORREF COLOR_BG_LIGHT = RGB(45, 45, 45);
    static constexpr COLORREF COLOR_TEXT = RGB(220, 220, 220);
    static constexpr COLORREF COLOR_ACCENT = RGB(0, 150, 255);
    static constexpr COLORREF COLOR_SUCCESS = RGB(0, 200, 100);
    static constexpr COLORREF COLOR_WARNING = RGB(255, 180, 0);
    static constexpr COLORREF COLOR_ERROR = RGB(255, 80, 80);

    HWND m_hwndParent = nullptr;
    HWND m_hwndStatusBar = nullptr;
    HWND m_hwndSuggestionPanel = nullptr;
    HWND m_hwndSuggestionList = nullptr;
    HINSTANCE m_hInstance = nullptr;

    AgenticRouterBridge* m_router = nullptr;

    std::vector<AgenticNotification> m_activeNotifications;
    std::vector<AgenticSuggestion> m_displayedSuggestions;

    NotificationCallback m_notificationCb;
    ModeToggleCallback m_modeToggleCb;

    bool m_suggestionPanelVisible = false;
    int m_suggestionPanelWidth = 350;
    int m_notificationWidth = 300;
    int m_notificationHeight = 80;
    int m_notificationMargin = 10;

    HFONT m_hFontTitle = nullptr;
    HFONT m_hFontBody = nullptr;
    HFONT m_hFontSmall = nullptr;
};

} // namespace Agentic
} // namespace RawrXD
