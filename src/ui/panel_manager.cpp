#include "ui/panel_manager.h"

namespace RawrXD::UI {

PanelManager::PanelManager() = default;
PanelManager::~PanelManager() {
    shutdown();
}

bool PanelManager::initialize(HWND parent) {
    m_parent = parent;

    // Create panel manager window
    WNDCLASSEX wc = {0};
    wc.cbSize = sizeof(WNDCLASSEX);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = GetModuleHandle(nullptr);
    wc.lpszClassName = "RawrXDPanelManager";
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);

    RegisterClassEx(&wc);

    m_hwnd = CreateWindowEx(0, "RawrXDPanelManager", "Panel Manager",
                            WS_CHILD | WS_CLIPCHILDREN,
                            0, 0, 800, m_height,
                            parent, nullptr, GetModuleHandle(nullptr), this);

    if (!m_hwnd) return false;

    createTabBar();

    // Add default panels
    addTerminalPanel();
    addOutputPanel();
    addProblemsPanel();
    addDebugConsolePanel();
    addSearchResultsPanel();

    return true;
}

void PanelManager::shutdown() {
    if (m_hwnd) {
        DestroyWindow(m_hwnd);
        m_hwnd = nullptr;
    }
}

HWND PanelManager::getHandle() const {
    return m_hwnd;
}

void PanelManager::setHeight(int height) {
    m_height = height;
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, 0, height, SWP_NOMOVE | SWP_NOZORDER);
    }
}

int PanelManager::getHeight() const {
    return m_height;
}

void PanelManager::resize(int width, int height) {
    if (m_hwnd) {
        SetWindowPos(m_hwnd, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER);
        layout();
    }
}

void PanelManager::addPanel(const Panel& panel) {
    m_panels.push_back(panel);
    updateTabBar();
}

void PanelManager::removePanel(const std::string& panelId) {
    m_panels.erase(std::remove_if(m_panels.begin(), m_panels.end(),
        [&panelId](const Panel& panel) { return panel.id == panelId; }), m_panels.end());
    updateTabBar();
}

void PanelManager::showPanel(const std::string& panelId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.visible = true;
            m_activePanel = panelId;
            break;
        }
    }
    showPanelContent(panelId);
    updateTabBar();
}

void PanelManager::hidePanel(const std::string& panelId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.visible = false;
            break;
        }
    }
    updateTabBar();
}

void PanelManager::togglePanel(const std::string& panelId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            if (panel.visible) {
                hidePanel(panelId);
            } else {
                showPanel(panelId);
            }
            break;
        }
    }
}

bool PanelManager::isPanelVisible(const std::string& panelId) const {
    for (const auto& panel : m_panels) {
        if (panel.id == panelId) {
            return panel.visible;
        }
    }
    return false;
}

void PanelManager::addTab(const std::string& panelId, const PanelTab& tab) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.tabs.push_back(tab);
            break;
        }
    }
    updateTabBar();
}

void PanelManager::removeTab(const std::string& panelId, const std::string& tabId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.tabs.erase(std::remove_if(panel.tabs.begin(), panel.tabs.end(),
                [&tabId](const PanelTab& tab) { return tab.id == tabId; }), panel.tabs.end());
            break;
        }
    }
    updateTabBar();
}

void PanelManager::activateTab(const std::string& panelId, const std::string& tabId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            panel.activeTab = tabId;
            for (auto& tab : panel.tabs) {
                tab.active = (tab.id == tabId);
            }
            break;
        }
    }
    updateTabBar();

    if (m_tabChangeCallback) {
        m_tabChangeCallback(panelId, tabId);
    }
}

std::string PanelManager::getActiveTab(const std::string& panelId) const {
    for (const auto& panel : m_panels) {
        if (panel.id == panelId) {
            return panel.activeTab;
        }
    }
    return "";
}

void PanelManager::nextTab(const std::string& panelId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            if (panel.tabs.empty()) break;

            size_t currentIndex = 0;
            for (size_t i = 0; i < panel.tabs.size(); ++i) {
                if (panel.tabs[i].id == panel.activeTab) {
                    currentIndex = i;
                    break;
                }
            }

            size_t nextIndex = (currentIndex + 1) % panel.tabs.size();
            activateTab(panelId, panel.tabs[nextIndex].id);
            break;
        }
    }
}

void PanelManager::previousTab(const std::string& panelId) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            if (panel.tabs.empty()) break;

            size_t currentIndex = 0;
            for (size_t i = 0; i < panel.tabs.size(); ++i) {
                if (panel.tabs[i].id == panel.activeTab) {
                    currentIndex = i;
                    break;
                }
            }

            size_t prevIndex = (currentIndex + panel.tabs.size() - 1) % panel.tabs.size();
            activateTab(panelId, panel.tabs[prevIndex].id);
            break;
        }
    }
}

void PanelManager::addTerminalPanel() {
    Panel panel;
    panel.id = "terminal";
    panel.title = "Terminal";
    panel.height = m_height;
    panel.visible = false;
    addPanel(panel);
}

void PanelManager::addOutputPanel() {
    Panel panel;
    panel.id = "output";
    panel.title = "Output";
    panel.height = m_height;
    panel.visible = false;
    addPanel(panel);
}

void PanelManager::addProblemsPanel() {
    Panel panel;
    panel.id = "problems";
    panel.title = "Problems";
    panel.height = m_height;
    panel.visible = false;
    addPanel(panel);
}

void PanelManager::addDebugConsolePanel() {
    Panel panel;
    panel.id = "debugConsole";
    panel.title = "Debug Console";
    panel.height = m_height;
    panel.visible = false;
    addPanel(panel);
}

void PanelManager::addSearchResultsPanel() {
    Panel panel;
    panel.id = "searchResults";
    panel.title = "Search Results";
    panel.height = m_height;
    panel.visible = false;
    addPanel(panel);
}

void PanelManager::setPanelContent(const std::string& panelId, const std::string& tabId, HWND content) {
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            for (auto& tab : panel.tabs) {
                if (tab.id == tabId) {
                    tab.content = content;
                    break;
                }
            }
            break;
        }
    }
}

HWND PanelManager::getPanelContent(const std::string& panelId, const std::string& tabId) const {
    for (const auto& panel : m_panels) {
        if (panel.id == panelId) {
            for (const auto& tab : panel.tabs) {
                if (tab.id == tabId) {
                    return tab.content;
                }
            }
        }
    }
    return nullptr;
}

void PanelManager::toggle() {
    m_visible = !m_visible;
    ShowWindow(m_hwnd, m_visible ? SW_SHOW : SW_HIDE);

    if (m_visibilityCallback) {
        m_visibilityCallback(m_visible);
    }
}

void PanelManager::show() {
    m_visible = true;
    ShowWindow(m_hwnd, SW_SHOW);

    if (m_visibilityCallback) {
        m_visibilityCallback(true);
    }
}

void PanelManager::hide() {
    m_visible = false;
    ShowWindow(m_hwnd, SW_HIDE);

    if (m_visibilityCallback) {
        m_visibilityCallback(false);
    }
}

bool PanelManager::isVisible() const {
    return m_visible;
}

void PanelManager::maximize() {
    if (!m_maximized) {
        m_normalHeight = m_height;
        m_height = 400; // Maximized height
        m_maximized = true;
        resize(800, m_height);
    }
}

void PanelManager::restore() {
    if (m_maximized) {
        m_height = m_normalHeight;
        m_maximized = false;
        resize(800, m_height);
    }
}

bool PanelManager::isMaximized() const {
    return m_maximized;
}

void PanelManager::onTabChanged(TabChangeCallback callback) {
    m_tabChangeCallback = callback;
}

void PanelManager::onVisibilityChanged(VisibilityCallback callback) {
    m_visibilityCallback = callback;
}

void PanelManager::appendOutput(const std::string& panelId, const std::string& text) {
    // Append text to output panel
}

void PanelManager::clearOutput(const std::string& panelId) {
    // Clear output panel
}

void PanelManager::setOutputFont(const std::string& panelId, HFONT font) {
    // Set output panel font
}

void PanelManager::addProblem(const std::string& severity,
                               const std::string& message,
                               const std::string& file,
                               int line,
                               int column) {
    // Add problem to problems panel
}

void PanelManager::clearProblems() {
    // Clear all problems
}

void PanelManager::filterProblems(const std::string& severity) {
    // Filter problems by severity
}

void PanelManager::createTabBar() {
    m_tabBar = CreateWindowEx(0, "SysTabControl32", "",
                              WS_CHILD | WS_VISIBLE | TCS_TABS | TCS_SINGLELINE,
                              0, 0, 800, 25,
                              m_hwnd, nullptr, GetModuleHandle(nullptr), nullptr);
}

void PanelManager::layout() {
    if (!m_hwnd) return;

    RECT rect;
    GetClientRect(m_hwnd, &rect);

    // Position tab bar
    if (m_tabBar) {
        SetWindowPos(m_tabBar, nullptr, 0, 0, rect.right, 25, SWP_NOZORDER);
    }

    // Position panel content
    for (auto& panel : m_panels) {
        if (panel.visible && panel.tabs.size() > 0) {
            for (auto& tab : panel.tabs) {
                if (tab.content && tab.active) {
                    SetWindowPos(tab.content, nullptr, 0, 25, rect.right, rect.bottom - 25, SWP_NOZORDER);
                }
            }
        }
    }
}

void PanelManager::updateTabBar() {
    // Update tab bar with current panels
    if (!m_tabBar) return;

    // Clear existing tabs
    TabCtrl_DeleteAllItems(m_tabBar);

    // Add tabs for visible panels
    for (const auto& panel : m_panels) {
        if (panel.visible) {
            TCITEM tie = {0};
            tie.mask = TCIF_TEXT;
            tie.pszText = const_cast<char*>(panel.title.c_str());
            TabCtrl_InsertItem(m_tabBar, TabCtrl_GetItemCount(m_tabBar), &tie);
        }
    }
}

void PanelManager::showPanelContent(const std::string& panelId) {
    // Show content for the active panel
    for (auto& panel : m_panels) {
        if (panel.id == panelId) {
            for (auto& tab : panel.tabs) {
                if (tab.content) {
                    ShowWindow(tab.content, tab.active ? SW_SHOW : SW_HIDE);
                }
            }
        } else {
            // Hide other panels
            for (auto& tab : panel.tabs) {
                if (tab.content) {
                    ShowWindow(tab.content, SW_HIDE);
                }
            }
        }
    }
}

LRESULT PanelManager::handleMessage(UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_SIZE:
            layout();
            return 0;

        case WM_NOTIFY: {
            LPNMHDR pnmh = (LPNMHDR)lParam;
            if (pnmh->hwndFrom == m_tabBar && pnmh->code == TCN_SELCHANGE) {
                int sel = TabCtrl_GetCurSel(m_tabBar);
                // Activate corresponding panel
            }
            return 0;
        }
    }

    return DefWindowProc(m_hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK PanelManager::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    PanelManager* manager = nullptr;

    if (msg == WM_CREATE) {
        CREATESTRUCT* cs = reinterpret_cast<CREATESTRUCT*>(lParam);
        manager = reinterpret_cast<PanelManager*>(cs->lpCreateParams);
        SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(manager));
    } else {
        manager = reinterpret_cast<PanelManager*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
    }

    if (manager) {
        return manager->handleMessage(msg, wParam, lParam);
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Global instance
PanelManager& getPanelManager() {
    static PanelManager manager;
    return manager;
}

} // namespace RawrXD::UI
