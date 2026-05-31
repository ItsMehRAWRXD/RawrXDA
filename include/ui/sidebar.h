#pragma once
/**
 * @file sidebar.h
 * @brief Collapsible sidebar with activity bar
 * Batch 4 - Item 51: Sidebar
 */

#include <string>
#include <vector>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

enum class SidebarView {
    Explorer,
    Search,
    SourceControl,
    Debug,
    Extensions,
    Testing,
    Custom
};

struct SidebarItem {
    std::string id;
    std::string label;
    std::string icon;
    HICON hIcon;
    SidebarView view;
    bool visible;
    bool enabled;
    int order;
    std::function<void()> clickHandler;
};

struct SidebarPanel {
    std::string id;
    std::string title;
    HWND content;
    int width;
    bool visible;
    bool collapsed;
};

class Sidebar {
public:
    Sidebar();
    ~Sidebar();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void setWidth(int width);
    int getWidth() const;
    void resize(int width, int height);

    // Activity bar (left)
    void addActivityItem(const SidebarItem& item);
    void removeActivityItem(const std::string& itemId);
    void setActivityItemVisible(const std::string& itemId, bool visible);
    void setActivityItemEnabled(const std::string& itemId, bool enabled);
    void selectActivityItem(const std::string& itemId);
    std::string getSelectedActivityItem() const;

    // Panels
    void addPanel(const SidebarPanel& panel);
    void removePanel(const std::string& panelId);
    void showPanel(const std::string& panelId);
    void hidePanel(const std::string& panelId);
    void collapsePanel(const std::string& panelId);
    void expandPanel(const std::string& panelId);
    bool isPanelVisible(const std::string& panelId) const;
    bool isPanelCollapsed(const std::string& panelId) const;

    // Default panels
    void addExplorerPanel();
    void addSearchPanel();
    void addSourceControlPanel();
    void addDebugPanel();
    void addExtensionsPanel();

    // Toggle
    void toggle();
    void show();
    void hide();
    bool isVisible() const;

    // Collapse/Expand all
    void collapseAll();
    void expandAll();

    // Content
    void setPanelContent(const std::string& panelId, HWND content);
    HWND getPanelContent(const std::string& panelId) const;

    // Events
    using SelectionCallback = std::function<void(const std::string& itemId)>;
    using VisibilityCallback = std::function<void(bool visible)>;
    void onActivityItemSelected(SelectionCallback callback);
    void onVisibilityChanged(VisibilityCallback callback);

    // Persistence
    void saveState(const std::string& path);
    void loadState(const std::string& path);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    HWND m_activityBar{nullptr};
    HWND m_panelContainer{nullptr};
    int m_width{250};
    int m_activityBarWidth{48};
    bool m_visible{true};
    std::string m_selectedItem;
    std::vector<SidebarItem> m_activityItems;
    std::vector<SidebarPanel> m_panels;

    SelectionCallback m_selectionCallback;
    VisibilityCallback m_visibilityCallback;

    void createActivityBar();
    void createPanelContainer();
    void layout();
    void updateActivityBar();
    void updatePanels();
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
Sidebar& getSidebar();

} // namespace RawrXD::UI
