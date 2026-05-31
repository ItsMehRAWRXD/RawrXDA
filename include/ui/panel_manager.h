#pragma once
/**
 * @file panel_manager.h
 * @brief Bottom panel with tabs (terminal, output, problems, etc.)
 * Batch 4 - Item 52: Panel manager
 */

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

enum class PanelType {
    Terminal,
    Output,
    Problems,
    DebugConsole,
    SearchResults,
    Custom
};

struct PanelTab {
    std::string id;
    std::string label;
    std::string icon;
    PanelType type;
    HWND content;
    bool visible;
    bool active;
    int order;
    std::function<void()> activationHandler;
};

struct Panel {
    std::string id;
    std::string title;
    int height;
    int minHeight;
    int maxHeight;
    bool visible;
    bool maximized;
    std::vector<PanelTab> tabs;
    std::string activeTab;
};

class PanelManager {
public:
    PanelManager();
    ~PanelManager();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void setHeight(int height);
    int getHeight() const;
    void resize(int width, int height);

    // Panels
    void addPanel(const Panel& panel);
    void removePanel(const std::string& panelId);
    void showPanel(const std::string& panelId);
    void hidePanel(const std::string& panelId);
    void togglePanel(const std::string& panelId);
    bool isPanelVisible(const std::string& panelId) const;

    // Tabs
    void addTab(const std::string& panelId, const PanelTab& tab);
    void removeTab(const std::string& panelId, const std::string& tabId);
    void activateTab(const std::string& panelId, const std::string& tabId);
    std::string getActiveTab(const std::string& panelId) const;
    void nextTab(const std::string& panelId);
    void previousTab(const std::string& panelId);

    // Default panels
    void addTerminalPanel();
    void addOutputPanel();
    void addProblemsPanel();
    void addDebugConsolePanel();
    void addSearchResultsPanel();

    // Content
    void setPanelContent(const std::string& panelId, const std::string& tabId, HWND content);
    HWND getPanelContent(const std::string& panelId, const std::string& tabId) const;

    // Toggle
    void toggle();
    void show();
    void hide();
    bool isVisible() const;

    // Maximize/Restore
    void maximize();
    void restore();
    bool isMaximized() const;

    // Events
    using TabChangeCallback = std::function<void(const std::string& panelId, const std::string& tabId)>;
    using VisibilityCallback = std::function<void(bool visible)>;
    void onTabChanged(TabChangeCallback callback);
    void onVisibilityChanged(VisibilityCallback callback);

    // Output
    void appendOutput(const std::string& panelId, const std::string& text);
    void clearOutput(const std::string& panelId);
    void setOutputFont(const std::string& panelId, HFONT font);

    // Problems
    void addProblem(const std::string& severity,
                    const std::string& message,
                    const std::string& file,
                    int line,
                    int column);
    void clearProblems();
    void filterProblems(const std::string& severity);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    HWND m_tabBar{nullptr};
    int m_height{200};
    int m_normalHeight{200};
    bool m_visible{false};
    bool m_maximized{false};
    std::vector<Panel> m_panels;
    std::string m_activePanel;

    TabChangeCallback m_tabChangeCallback;
    VisibilityCallback m_visibilityCallback;

    void createTabBar();
    void layout();
    void updateTabBar();
    void showPanelContent(const std::string& panelId);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
PanelManager& getPanelManager();

} // namespace RawrXD::UI
