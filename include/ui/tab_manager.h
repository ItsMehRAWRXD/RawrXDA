#pragma once
/**
 * @file tab_manager.h
 * @brief Document tabs with drag-drop and pinning
 * Batch 4 - Item 53: Tab manager
 */

#include <string>
#include <vector>
#include <optional>
#include <functional>
#include <windows.h>

namespace RawrXD::UI {

enum class TabState {
    Normal,
    Modified,
    Preview,
    Pinned
};

struct Tab {
    std::string id;
    std::string label;
    std::string path;
    std::string tooltip;
    TabState state;
    bool active;
    bool pinned;
    int order;
    HWND content;
    HICON icon;
    std::chrono::steady_clock::time_point lastAccessed;
};

struct TabGroup {
    std::string id;
    std::string name;
    std::vector<std::string> tabIds;
    bool collapsed;
};

class TabManager {
public:
    TabManager();
    ~TabManager();

    // Initialization
    bool initialize(HWND parent);
    void shutdown();

    // Window management
    HWND getHandle() const;
    void resize(int width, int height);

    // Tabs
    void addTab(const Tab& tab);
    void removeTab(const std::string& tabId);
    void activateTab(const std::string& tabId);
    std::optional<Tab> getTab(const std::string& tabId) const;
    std::optional<Tab> getActiveTab() const;
    std::vector<Tab> getAllTabs() const;
    size_t getTabCount() const;

    // Tab state
    void setTabModified(const std::string& tabId, bool modified);
    void setTabPinned(const std::string& tabId, bool pinned);
    void setTabLabel(const std::string& tabId, const std::string& label);
    void setTabTooltip(const std::string& tabId, const std::string& tooltip);

    // Navigation
    void nextTab();
    void previousTab();
    void firstTab();
    void lastTab();
    void recentTab();
    void gotoTab(int index);

    // Reordering
    void moveTab(const std::string& tabId, int newIndex);
    void moveTabToGroup(const std::string& tabId, const std::string& groupId);

    // Groups
    void createGroup(const std::string& groupId, const std::string& name);
    void removeGroup(const std::string& groupId);
    void collapseGroup(const std::string& groupId);
    void expandGroup(const std::string& groupId);

    // Preview tabs
    void openPreviewTab(const std::string& path);
    void promotePreviewTab(const std::string& tabId);
    bool isPreviewTab(const std::string& tabId) const;

    // Close operations
    void closeTab(const std::string& tabId);
    void closeOtherTabs(const std::string& tabId);
    void closeTabsToRight(const std::string& tabId);
    void closeAllTabs();
    void closeUnmodifiedTabs();

    // Dirty tabs
    std::vector<Tab> getModifiedTabs() const;
    bool hasModifiedTabs() const;

    // Events
    using TabChangeCallback = std::function<void(const std::string& tabId)>;
    using TabCloseCallback = std::function<bool(const std::string& tabId)>; // Return false to cancel
    using TabMoveCallback = std::function<void(const std::string& tabId, int newIndex)>;
    void onTabActivated(TabChangeCallback callback);
    void onTabClosed(TabCloseCallback callback);
    void onTabMoved(TabMoveCallback callback);

    // Drag and drop
    void startDrag(const std::string& tabId);
    void endDrag();
    bool isDragging() const;

    // Persistence
    void saveSession(const std::string& path);
    void restoreSession(const std::string& path);

private:
    HWND m_hwnd{nullptr};
    HWND m_parent{nullptr};
    std::vector<Tab> m_tabs;
    std::vector<TabGroup> m_groups;
    std::string m_activeTab;
    std::string m_draggedTab;
    int m_tabHeight{28};
    int m_tabMinWidth{80};
    int m_tabMaxWidth{200};

    TabChangeCallback m_activateCallback;
    TabCloseCallback m_closeCallback;
    TabMoveCallback m_moveCallback;

    void layout();
    void drawTabs(HDC hdc);
    void drawTab(HDC hdc, const Tab& tab, RECT& rect);
    int getTabIndex(const std::string& tabId) const;
    RECT getTabRect(int index) const;
    std::string hitTest(int x, int y) const;
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
};

// Global instance
TabManager& getTabManager();

} // namespace RawrXD::UI
