#include "ui/tab_manager.h"
#include <algorithm>

namespace RawrXD::UI {

TabManager::TabManager() = default;
TabManager::~TabManager() {
    shutdown();
}

bool TabManager::initialize(HWND parent) {
    m_parent = parent;

    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDTabManager";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, "RawrXDTabManager", "Tabs",
                            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                            0, 0, 800, m_tabHeight,
                            parent, nullptr, GetModuleHandle(nullptr), this);

    return m_hwnd != nullptr;
}

void TabManager::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

HWND TabManager::getHandle() const {
    return m_hwnd;
}

void TabManager::resize(int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, m_tabHeight, SWP_NOMOVE | SWP_NOZORDER);
    }
}

void TabManager::addTab(const Tab& tab) {
    // Remove any existing preview tab
    if (!m_tabs.empty() && m_tabs.back().state == TabState::Preview) {
        m_tabs.pop_back();
    }

    m_tabs.push_back(tab);
    m_tabs.back().order = static_cast<int>(m_tabs.size()) - 1;
    activateTab(tab.id);
    redraw();
}

void TabManager::removeTab(const std::string& tabId) {
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
        [&tabId](const Tab& t) { return t.id == tabId; });

    if (it != m_tabs.end()) {
        bool wasActive = it->active;
        m_tabs.erase(it);

        if (wasActive && !m_tabs.empty()) {
            activateTab(m_tabs.back().id);
        }

        redraw();
    }
}

void TabManager::activateTab(const std::string& tabId) {
    for (auto& tab : m_tabs) {
        tab.active = (tab.id == tabId);
        if (tab.active) {
            tab.state = TabState::Normal; // Promote from preview
            tab.lastAccessed = std::chrono::steady_clock::now();
            m_activeTab = tabId;
        }
    }

    if (m_activateCallback) {
        m_activateCallback(tabId);
    }

    redraw();
}

std::optional<Tab> TabManager::getTab(const std::string& tabId) const {
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
        [&tabId](const Tab& t) { return t.id == tabId; });

    if (it != m_tabs.end()) {
        return *it;
    }

    return std::nullopt;
}

std::optional<Tab> TabManager::getActiveTab() const {
    return getTab(m_activeTab);
}

std::vector<Tab> TabManager::getAllTabs() const {
    return m_tabs;
}

size_t TabManager::getTabCount() const {
    return m_tabs.size();
}

void TabManager::setTabModified(const std::string& tabId, bool modified) {
    for (auto& tab : m_tabs) {
        if (tab.id == tabId) {
            tab.state = modified ? TabState::Modified : TabState::Normal;
            break;
        }
    }
    redraw();
}

void TabManager::setTabPinned(const std::string& tabId, bool pinned) {
    for (auto& tab : m_tabs) {
        if (tab.id == tabId) {
            tab.pinned = pinned;
            if (pinned) {
                tab.state = TabState::Normal;
            }
            break;
        }
    }
    redraw();
}

void TabManager::setTabLabel(const std::string& tabId, const std::string& label) {
    for (auto& tab : m_tabs) {
        if (tab.id == tabId) {
            tab.label = label;
            break;
        }
    }
    redraw();
}

void TabManager::nextTab() {
    if (m_tabs.empty()) return;

    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
        [this](const Tab& t) { return t.id == m_activeTab; });

    if (it != m_tabs.end()) {
        auto next = std::next(it);
        if (next == m_tabs.end()) {
            next = m_tabs.begin();
        }
        activateTab(next->id);
    }
}

void TabManager::previousTab() {
    if (m_tabs.empty()) return;

    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
        [this](const Tab& t) { return t.id == m_activeTab; });

    if (it != m_tabs.end()) {
        if (it == m_tabs.begin()) {
            activateTab(m_tabs.back().id);
        } else {
            activateTab(std::prev(it)->id);
        }
    }
}

void TabManager::closeTab(const std::string& tabId) {
    if (m_closeCallback) {
        if (!m_closeCallback(tabId)) {
            return; // Cancelled
        }
    }

    removeTab(tabId);
}

void TabManager::closeOtherTabs(const std::string& tabId) {
    std::vector<std::string> toClose;
    for (const auto& tab : m_tabs) {
        if (tab.id != tabId && !tab.pinned) {
            toClose.push_back(tab.id);
        }
    }

    for (const auto& id : toClose) {
        closeTab(id);
    }
}

void TabManager::closeTabsToRight(const std::string& tabId) {
    auto it = std::find_if(m_tabs.begin(), m_tabs.end(),
        [&tabId](const Tab& t) { return t.id == tabId; });

    if (it == m_tabs.end()) return;

    std::vector<std::string> toClose;
    for (auto jt = std::next(it); jt != m_tabs.end(); ++jt) {
        if (!jt->pinned) {
            toClose.push_back(jt->id);
        }
    }

    for (const auto& id : toClose) {
        closeTab(id);
    }
}

void TabManager::closeAllTabs() {
    std::vector<std::string> toClose;
    for (const auto& tab : m_tabs) {
        if (!tab.pinned) {
            toClose.push_back(tab.id);
        }
    }

    for (const auto& id : toClose) {
        closeTab(id);
    }
}

void TabManager::closeUnmodifiedTabs() {
    std::vector<std::string> toClose;
    for (const auto& tab : m_tabs) {
        if (tab.state != TabState::Modified && !tab.pinned) {
            toClose.push_back(tab.id);
        }
    }

    for (const auto& id : toClose) {
        closeTab(id);
    }
}

std::vector<Tab> TabManager::getModifiedTabs() const {
    std::vector<Tab> modified;
    for (const auto& tab : m_tabs) {
        if (tab.state == TabState::Modified) {
            modified.push_back(tab);
        }
    }
    return modified;
}

bool TabManager::hasModifiedTabs() const {
    for (const auto& tab : m_tabs) {
        if (tab.state == TabState::Modified) {
            return true;
        }
    }
    return false;
}

void TabManager::onTabActivated(TabChangeCallback callback) {
    m_activateCallback = callback;
}

void TabManager::onTabClosed(TabCloseCallback callback) {
    m_closeCallback = callback;
}

void TabManager::onTabMoved(TabMoveCallback callback) {
    m_moveCallback = callback;
}

void TabManager::saveSession(const std::string& path) {
    // Save tab session to file
}

void TabManager::restoreSession(const std::string& path) {
    // Restore tab session from file
}

void TabManager::redraw() {
    if (m_hwnd) {
        InvalidateRect(m_hwnd, nullptr, TRUE);
    }
}

void TabManager::drawTabs(HDC hdc) {
    RECT rect;
    GetClientRect(m_hwnd, &rect);

    // Draw background
    FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

    // Draw each tab
    int x = 0;
    for (size_t i = 0; i < m_tabs.size(); ++i) {
        RECT tabRect = getTabRect(static_cast<int>(i));
        drawTab(hdc, m_tabs[i], tabRect);
        x += tabRect.right - tabRect.left;
    }
}

void TabManager::drawTab(HDC hdc, const Tab& tab, RECT& rect) {
    // Draw tab background
    COLORREF bgColor = tab.active ? RGB(30, 30, 30) : RGB(45, 45, 48);
    HBRUSH brush = CreateSolidBrush(bgColor);
    FillRect(hdc, &rect, brush);
    DeleteObject(brush);

    // Draw border
    HPEN pen = CreatePen(PS_SOLID, 1, RGB(62, 62, 66));
    SelectObject(hdc, pen);
    MoveToEx(hdc, rect.left, rect.bottom - 1, nullptr);
    LineTo(hdc, rect.right, rect.bottom - 1);
    DeleteObject(pen);

    // Draw text
    COLORREF textColor = tab.active ? RGB(255, 255, 255) : RGB(150, 150, 150);
    SetTextColor(hdc, textColor);
    SetBkMode(hdc, TRANSPARENT);

    std::string label = tab.label;
    if (tab.state == TabState::Modified) {
        label = "• " + label;
    }

    RECT textRect = rect;
    textRect.left += 10;
    textRect.right -= 10;
    DrawText(hdc, label.c_str(), -1, &textRect, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
}

RECT TabManager::getTabRect(int index) const {
    int x = 0;
    for (int i = 0; i < index; ++i) {
        int width = std::min(m_tabMaxWidth,
            std::max(m_tabMinWidth, 800 / static_cast<int>(m_tabs.size())));
        x += width;
    }

    int width = std::min(m_tabMaxWidth,
        std::max(m_tabMinWidth, 800 / static_cast<int>(m_tabs.size())));

    RECT rect;
    rect.left = x;
    rect.top = 0;
    rect.right = x + width;
    rect.bottom = m_tabHeight;

    return rect;
}

LRESULT TabManager::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            drawTabs(hdc);
            EndPaint(m_hwnd, &ps);
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);

            for (size_t i = 0; i < m_tabs.size(); ++i) {
                RECT rect = getTabRect(static_cast<int>(i));
                if (x >= rect.left && x < rect.right &&
                    y >= rect.top && y < rect.bottom) {
                    activateTab(m_tabs[i].id);
                    break;
                }
            }
            return 0;
        }

        case WM_RBUTTONDOWN: {
            // Show context menu
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK TabManager::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    TabManager* manager = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        manager = reinterpret_cast<TabManager*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(manager));
    } else {
        manager = reinterpret_cast<TabManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (manager) {
        return manager->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Global instance
TabManager& getTabManager() {
    static TabManager manager;
    return manager;
}

} // namespace RawrXD::UI
