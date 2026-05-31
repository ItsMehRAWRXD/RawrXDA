#include "ui/sidebar.h"

namespace RawrXD::UI {

Sidebar::Sidebar() = default;
Sidebar::~Sidebar() {
    shutdown();
}

bool Sidebar::initialize(HWND parent) {
    m_parent = parent;

    // Create sidebar window
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDSidebar";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, "RawrXDSidebar", "Sidebar",
                            WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN,
                            0, 0, m_width, 600,
                            parent, nullptr, GetModuleHandle(nullptr), this);

    if (!m_hwnd) return false;

    createActivityBar();
    createPanelContainer();

    // Add default panels
    addExplorerPanel();
    addSearchPanel();
    addSourceControlPanel();
    addDebugPanel();
    addExtensionsPanel();

    return true;
}

void Sidebar::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

HWND Sidebar::getHandle() const {
    return m_hwnd;
}

void Sidebar::setWidth(int width) {
    m_width = width;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, 0, SWP_NOMOVE | SWP_NOZORDER);
        layout();
    }
}

int Sidebar::getWidth() const {
    return m_width;
}

void Sidebar::resize(int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
        layout();
    }
}

void Sidebar::addActivityItem(const SidebarItem& item) {
    m_activityItems.push_back(item);
    updateActivityBar();
}

void Sidebar::removeActivityItem(const std::string& itemId) {
    m_activityItems.erase(std::remove_if(m_activityItems.begin(), m_activityItems.end(),
        [&itemId](const SidebarItem& item) { return item.id == itemId; }), m_activityItems.end());
    updateActivityBar();
}

void Sidebar::setActivityItemVisible(const std::string& itemId, bool visible) {
    for (auto& item : m_activityItems) {
        if (item.id == itemId) {
            item.visible = visible;
            break;
        }
    }
    updateActivityBar();
}

void Sidebar::setActivityItemEnabled(const std::string& itemId, bool enabled) {
    for (auto& item : m_activityItems) {
        if (item.id == itemId) {
            item.enabled = enabled;
            break;
        }
    }
    updateActivityBar();
}

void Sidebar::selectActivityItem(const std::string& itemId) {
    m_selectedItem = itemId;
    updateActivityBar();
    updatePanels();

    if (m_selectionCallback) {
        m_selectionCallback(itemId);
    }
}

std::string Sidebar::getSelectedActivityItem() const {
    return m_selectedItem;
}

void Sidebar::addPanel(const SidebarPanel& panel) {
    m_panels.push_back(panel);
    updatePanels();
}

void Sidebar::removePanel(const std::string& panelId) {
    m_panels.erase(std::remove_if(m_panels.begin(), m_panels.end(),
        [&panelId](const SidebarPanel& panel) { return panel.id == panelId; }), m_panels.end());
    updatePanels();
}

void Sidebar::showPanel(const std::string& panelId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.visible = true;
            break;
        }
    }
    updatePanels();
}

void Sidebar::hidePanel(const std::string& panelId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.visible = false;
            break;
        }
    }
    updatePanels();
}

void Sidebar::collapsePanel(const std::string& panelId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.collapsed = true;
            break;
        }
    }
    updatePanels();
}

void Sidebar::expandPanel(const std::string& panelId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.collapsed = false;
            break;
        }
    }
    updatePanels();
}

bool Sidebar::isPanelVisible(const std::string& panelId) const {
    for (const auto& panel : m_panels) {
        if (panel.id == panelId) {
            return panel.visible;
        }
    }
    return false;
}

bool Sidebar::isPanelCollapsed(const std::string& panelId) const {
    for (const auto& panel : m_panels) {
        if (panel.id == panelId) {
            return panel.collapsed;
        }
    }
    return false;
}

void Sidebar::addExplorerPanel() {
    SidebarPanel panel;
    panel.id = "explorer";
    panel.title = "Explorer";
    panel.width = m_width - m_activityBarWidth;
    panel.visible = true;
    panel.collapsed = false;
    addPanel(panel);

    SidebarItem item;
    item.id = "explorer";
    item.label = "Explorer";
    item.view = SidebarView::Explorer;
    item.visible = true;
    item.enabled = true;
    addActivityItem(item);
}

void Sidebar::addSearchPanel() {
    SidebarPanel panel;
    panel.id = "search";
    panel.title = "Search";
    panel.width = m_width - m_activityBarWidth;
    panel.visible = false;
    panel.collapsed = false;
    addPanel(panel);

    SidebarItem item;
    item.id = "search";
    item.label = "Search";
    item.view = SidebarView::Search;
    item.visible = true;
    item.enabled = true;
    addActivityItem(item);
}

void Sidebar::addSourceControlPanel() {
    SidebarPanel panel;
    panel.id = "sourceControl";
    panel.title = "Source Control";
    panel.width = m_width - m_activityBarWidth;
    panel.visible = false;
    panel.collapsed = false;
    addPanel(panel);

    SidebarItem item;
    item.id = "sourceControl";
    item.label = "Source Control";
    item.view = SidebarView::SourceControl;
    item.visible = true;
    item.enabled = true;
    addActivityItem(item);
}

void Sidebar::addDebugPanel() {
    SidebarPanel panel;
    panel.id = "debug";
    panel.title = "Debug";
    panel.width = m_width - m_activityBarWidth;
    panel.visible = false;
    panel.collapsed = false;
    addPanel(panel);

    SidebarItem item;
    item.id = "debug";
    item.label = "Debug";
    item.view = SidebarView::Debug;
    item.visible = true;
    item.enabled = true;
    addActivityItem(item);
}

void Sidebar::addExtensionsPanel() {
    SidebarPanel panel;
    panel.id = "extensions";
    panel.title = "Extensions";
    panel.width = m_width - m_activityBarWidth;
    panel.visible = false;
    panel.collapsed = false;
    addPanel(panel);

    SidebarItem item;
    item.id = "extensions";
    item.label = "Extensions";
    item.view = SidebarView::Extensions;
    item.visible = true;
    item.enabled = true;
    addActivityItem(item);
}

void Sidebar::toggle() {
    m_visible = !m_visible;
    ShowWindow(m_hwnd, m_visible ? SW_SHOW : SW_HIDE);

    if (m_visibilityCallback) {
        m_visibilityCallback(m_visible);
    }
}

void Sidebar::show() {
    m_visible = true;
    ShowWindow(m_hwnd, SW_SHOW);

    if (m_visibilityCallback) {
        m_visibilityCallback(true);
    }
}

void Sidebar::hide() {
    m_visible = false;
    ShowWindow(m_hwnd, SW_HIDE);

    if (m_visibilityCallback) {
        m_visibilityCallback(false);
    }
}

bool Sidebar::isVisible() const {
    return m_visible;
}

void Sidebar::collapseAll() {
    for (auto& panel : m_panels) {
        panel.collapsed = true;
    }
    updatePanels();
}

void Sidebar::expandAll() {
    for (auto& panel : m_panels) {
        panel.collapsed = false;
    }
    updatePanels();
}

void Sidebar::setPanelContent(const std::string& panelId, HWND content) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.content = content;
            break;
        }
    }
}

HWND Sidebar::getPanelContent(const std::string& panelId) const {
    for (const auto& panel : m_panels) {
        if (panel.id == panelId) {
            return panel.content;
        }
    }
    return nullptr;
}

void Sidebar::onActivityItemSelected(SelectionCallback callback) {
    m_selectionCallback = callback;
}

void Sidebar::onVisibilityChanged(VisibilityCallback callback) {
    m_visibilityCallback = callback;
}

void Sidebar::saveState(const std::string& path) {
    // Save sidebar state to file
}

void Sidebar::loadState(const std::string& path) {
    // Load sidebar state from file
}

void Sidebar::createActivityBar() {
    m_activityBar = CreateWindowEx(0, "BUTTON", "",
                                   WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
                                   0, 0, m_activityBarWidth, 600,
                                   m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
}

void Sidebar::createPanelContainer() {
    m_panelContainer = CreateWindowEx(0, "STATIC", "",
                                      WS_CHILD | WS_VISIBLE,
                                      m_activityBarWidth, 0, m_width - m_activityBarWidth, 600,
                                      m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
}

void Sidebar::layout() {
    if (!m_hwnd) return;

    RECT rect;
    GetClientRect(m_hwnd, &rect);

    // Position activity bar
    if (m_activityBar) {
        SetWindowPos(m_activityBar, nullptr, 0, 0, m_activityBarWidth, rect.bottom,
                     SWP_NOZORDER);
    }

    // Position panel container
    if (m_panelContainer) {
        SetWindowPos(m_panelContainer, nullptr, m_activityBarWidth, 0,
                     rect.right - m_activityBarWidth, rect.bottom, SWP_NOZORDER);
    }
}

void Sidebar::updateActivityBar() {
    // Redraw activity bar
    if (m_activityBar) {
        InvalidateRect(m_activityBar, nullptr, TRUE);
    }
}

void Sidebar::updatePanels() {
    // Show/hide panels based on selection
    for (const auto& panel : m_panels) {
        if (panel.content) {
            ShowWindow(panel.content, panel.visible ? SW_SHOW : SW_HIDE);
        }
    }
}

LRESULT Sidebar::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            layout();
            return 0;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(m_hwnd, &ps);
            // Draw sidebar background
            EndPaint(m_hwnd, &ps);
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK Sidebar::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    Sidebar* sidebar = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        sidebar = reinterpret_cast<Sidebar*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(sidebar));
    } else {
        sidebar = reinterpret_cast<Sidebar*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (sidebar) {
        return sidebar->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Global instance
Sidebar& getSidebar() {
    static Sidebar sidebar;
    return sidebar;
}

} // namespace RawrXD::UI
