// ============================================================================
// WindowManager.h — DIrect Win32 Window Lifecycle & Message Routing
// ============================================================================
// FIX #6: Real IDE Operations
// Purpose:
//   - Manage IDE window creation, sizing, showing/hiding
//   - Handle WM_* messages for window events (resize, focus, close)
//   - Route messages to appropriate handler (editor, terminal, panels)
//   - Maintain window state (visible, maximized, active editor tab, etc.)
// ============================================================================
#pragma once

#include <windows.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <atomic>
#include <mutex>

namespace RawrXD {
namespace Win32App {

// ============================================================================
// WindowManager — Win32 window lifecycle and message dispatch
// ============================================================================
class WindowManager {
public:
    static WindowManager& Instance();

    // Window lifecycle
    bool Initialize();
    void Shutdown();
    void ShowWindow();
    void HideWindow();
    void Maximize();
    void Minimize();
    void RestoreWindow();

    // Window properties
    HWND GetHWND() const { return m_hwnd; }
    bool IsVisible() const { return m_isVisible; }
    bool IsMaximized() const { return m_isMaximized; }
    bool IsFocused() const { return m_isFocused; }

    // Message routing
    LRESULT HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

    // Panel management
    bool ShowPanel(const std::string& panelName);
    bool HidePanel(const std::string& panelName);
    bool IsPanelVisible(const std::string& panelName) const;

    // Editor window reference
    void SetActiveTab(int tabIndex);
    int GetActiveTab() const { return m_activeTabIndex; }

    // Layout constraints
    struct WindowRect {
        int x, y, width, height;
    };
    WindowRect GetWindowRect() const;
    void SetWindowRect(const WindowRect& rect);

private:
    WindowManager();
    ~WindowManager();

    // Private message handlers
    void OnSize(int width, int height);
    void OnClose();
    void OnFocus();
    void OnBlur();
    void OnCommand(int cmdId);

    HWND m_hwnd = nullptr;
    std::atomic<bool> m_isVisible = false;
    std::atomic<bool> m_isMaximized = false;
    std::atomic<bool> m_isFocused = false;
    int m_activeTabIndex = 0;

    std::unordered_map<std::string, bool> m_panelVisibility;
    mutable std::mutex m_panelMutex;

    WindowRect m_savedRect = {100, 100, 1200, 800};
};

} // namespace Win32App
} // namespace RawrXD
