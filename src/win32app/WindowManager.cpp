// ============================================================================
// WindowManager.cpp — Implementation
// ============================================================================
#include "WindowManager.h"
#include <cassert>

namespace RawrXD {
namespace Win32App {

// Singleton instance
static WindowManager* g_windowManager = nullptr;

WindowManager& WindowManager::Instance() {
    if (!g_windowManager) {
        g_windowManager = new WindowManager();
    }
    return *g_windowManager;
}

WindowManager::WindowManager() {
    // Initialize panel visibility tracking
    m_panelVisibility["sidebar"] = true;
    m_panelVisibility["problems"] = true;
    m_panelVisibility["terminal"] = true;
    m_panelVisibility["output"] = true;
    m_panelVisibility["debug"] = false;
}

WindowManager::~WindowManager() {
    Shutdown();
}

bool WindowManager::Initialize() {
    // This will be fully implemented to:
    // 1. Create main window with CreateWindowExW
    // 2. Register window class if not done
    // 3. Set up message loop
    // For now, placeholder implementation
    m_isVisible = false;
    return true;
}

void WindowManager::Shutdown() {
    if (m_hwnd) {
        ::DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
    m_isVisible = false;
}

void WindowManager::ShowWindow() {
    if (m_hwnd) {
        ::ShowWindow(m_hwnd, SW_SHOW);
        m_isVisible = true;
    }
}

void WindowManager::HideWindow() {
    if (m_hwnd) {
        ::ShowWindow(m_hwnd, SW_HIDE);
        m_isVisible = false;
    }
}

void WindowManager::Maximize() {
    if (m_hwnd) {
        ::ShowWindow(m_hwnd, SW_MAXIMIZE);
        m_isMaximized = true;
    }
}

void WindowManager::Minimize() {
    if (m_hwnd) {
        ::ShowWindow(m_hwnd, SW_MINIMIZE);
        m_isMaximized = false;
    }
}

void WindowManager::RestoreWindow() {
    if (m_hwnd) {
        ::ShowWindow(m_hwnd, SW_RESTORE);
        m_isMaximized = false;
    }
}

LRESULT WindowManager::HandleWindowMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Route message to appropriate handler
    switch (msg) {
        case WM_SIZE: {
            int width = LOWORD(lParam);
            int height = HIWORD(lParam);
            OnSize(width, height);
            break;
        }
        case WM_CLOSE:
            OnClose();
            break;
        case WM_SETFOCUS:
            OnFocus();
            break;
        case WM_KILLFOCUS:
            OnBlur();
            break;
        case WM_COMMAND:
            OnCommand(LOWORD(wParam));
            break;
        default:
            return ::DefWindowProcW(hwnd, msg, wParam, lParam);
    }
    return 0;
}

bool WindowManager::ShowPanel(const std::string& panelName) {
    std::lock_guard<std::mutex> lock(m_panelMutex);
    auto it = m_panelVisibility.find(panelName);
    if (it != m_panelVisibility.end()) {
        it->second = true;
        return true;
    }
    return false;
}

bool WindowManager::HidePanel(const std::string& panelName) {
    std::lock_guard<std::mutex> lock(m_panelMutex);
    auto it = m_panelVisibility.find(panelName);
    if (it != m_panelVisibility.end()) {
        it->second = false;
        return true;
    }
    return false;
}

bool WindowManager::IsPanelVisible(const std::string& panelName) const {
    std::lock_guard<std::mutex> lock(m_panelMutex);
    auto it = m_panelVisibility.find(panelName);
    if (it != m_panelVisibility.end()) {
        return it->second;
    }
    return false;
}

void WindowManager::SetActiveTab(int tabIndex) {
    m_activeTabIndex = tabIndex;
}

WindowManager::WindowRect WindowManager::GetWindowRect() const {
    if (m_hwnd) {
        RECT rect;
        if (::GetWindowRect(m_hwnd, &rect)) {
            return {rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top};
        }
    }
    return m_savedRect;
}

void WindowManager::SetWindowRect(const WindowRect& rect) {
    m_savedRect = rect;
    if (m_hwnd) {
        ::MoveWindow(m_hwnd, rect.x, rect.y, rect.width, rect.height, TRUE);
    }
}

void WindowManager::OnSize(int width, int height) {
    m_savedRect.width = width;
    m_savedRect.height = height;
}

void WindowManager::OnClose() {
    // Handle window close event
    Shutdown();
}

void WindowManager::OnFocus() {
    m_isFocused = true;
}

void WindowManager::OnBlur() {
    m_isFocused = false;
}

void WindowManager::OnCommand(int cmdId) {
    // Route command to appropriate handler
    // This would dispatch to EditorOperations or other systems
}

} // namespace Win32App
} // namespace RawrXD
