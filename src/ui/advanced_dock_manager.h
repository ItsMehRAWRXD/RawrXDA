/**
 * @file advanced_dock_manager.h
 * @brief Advanced Docking Manager - VSCode-Class Layout System
 * 
 * Implements:
 * - Tab groups with drag-drop reordering
 * - Side panels (left/right/bottom)
 * - Bottom terminal panel
 * - Split views with resizable sashes
 * - Panel state persistence
 * 
 * @author RawrXD UI Team
 * @version 2.0.0
 */

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>

namespace RawrXD {
namespace UI {

// ============================================================================
// Docking Types
// ============================================================================

enum class DockArea {
    Center,     // Main editor area with tabs
    Left,       // Left side panel
    Right,      // Right side panel
    Bottom,     // Bottom panel (terminal, output)
    Floating    // Floating window
};

enum class SplitDirection {
    Horizontal, // Side-by-side
    Vertical    // Stacked
};

enum class PanelState {
    Hidden,
    Visible,
    Minimized,
    Maximized
};

// ============================================================================
// Panel Descriptor
// ============================================================================

struct PanelDescriptor {
    std::string id;
    std::string title;
    std::string icon;
    DockArea defaultArea;
    int defaultWidth = 250;
    int defaultHeight = 200;
    int minWidth = 100;
    int minHeight = 100;
    bool canClose = true;
    bool canFloat = true;
    bool isTransient = false; // Don't persist
};

// ============================================================================
// Tab Group
// ============================================================================

struct TabInfo {
    std::string id;
    std::string title;
    std::string tooltip;
    bool isModified = false;
    bool isPinned = false;
    int order = 0;
    HWND contentHwnd = nullptr;
};

class TabGroup {
public:
    TabGroup(const std::string& id);
    ~TabGroup();

    // Tab management
    void addTab(const TabInfo& tab);
    void removeTab(const std::string& tabId);
    void moveTab(const std::string& tabId, int newIndex);
    void activateTab(const std::string& tabId);
    void pinTab(const std::string& tabId, bool pinned);
    
    // Queries
    std::vector<TabInfo> getTabs() const;
    std::string getActiveTab() const;
    bool hasTab(const std::string& tabId) const;
    int getTabIndex(const std::string& tabId) const;
    
    // Drag-drop support
    void beginDrag(const std::string& tabId);
    void endDrag(bool accepted);
    bool isDragging() const;
    
    // Rendering
    void render(HDC hdc, const RECT& bounds);
    void renderTabs(HDC hdc, const RECT& tabStripBounds);
    void renderContent(HDC hdc, const RECT& contentBounds);
    
    // Events
    std::function<void(const std::string&)> onTabActivated;
    std::function<void(const std::string&)> onTabClosed;
    std::function<void(const std::string&, DockArea)> onTabMoved;

private:
    std::string id_;
    std::vector<TabInfo> tabs_;
    std::string activeTabId_;
    std::string draggedTabId_;
    bool isDragging_ = false;
    int tabHeight_ = 24;
    int closeButtonWidth_ = 16;
};

// ============================================================================
// Side Panel
// ============================================================================

class SidePanel {
public:
    SidePanel(DockArea area, int defaultWidth);
    ~SidePanel();

    // Panel management
    void addPanel(const PanelDescriptor& desc, HWND contentHwnd);
    void removePanel(const std::string& panelId);
    void showPanel(const std::string& panelId);
    void hidePanel(const std::string& panelId);
    void togglePanel(const std::string& panelId);
    
    // Queries
    std::vector<PanelDescriptor> getPanels() const;
    std::vector<std::string> getVisiblePanels() const;
    bool isPanelVisible(const std::string& panelId) const;
    int getWidth() const { return currentWidth_; }
    
    // Resizing
    void setWidth(int width);
    void beginResize();
    void endResize();
    bool isResizing() const;
    
    // Rendering
    void render(HDC hdc, const RECT& bounds);
    void renderHeader(HDC hdc, const RECT& headerBounds);
    void renderContent(HDC hdc, const RECT& contentBounds);
    
    // Events
    std::function<void(const std::string&)> onPanelActivated;
    std::function<void(int)> onWidthChanged;

private:
    DockArea area_;
    int defaultWidth_;
    int currentWidth_;
    int minWidth_ = 100;
    int maxWidth_ = 500;
    bool isResizing_ = false;
    std::map<std::string, PanelDescriptor> panels_;
    std::map<std::string, HWND> panelHwnds_;
    std::map<std::string, PanelState> panelStates_;
    std::string activePanelId_;
};

// ============================================================================
// Bottom Panel (Terminal/Output)
// ============================================================================

class BottomPanel {
public:
    BottomPanel(int defaultHeight = 200);
    ~BottomPanel();

    // Panel management
    void addPanel(const PanelDescriptor& desc, HWND contentHwnd);
    void removePanel(const std::string& panelId);
    void showPanel(const std::string& panelId);
    void hidePanel(const std::string& panelId);
    void togglePanel(const std::string& panelId);
    void maximizePanel(const std::string& panelId);
    void restorePanel(const std::string& panelId);
    
    // Queries
    std::vector<PanelDescriptor> getPanels() const;
    std::string getActivePanel() const;
    int getHeight() const { return currentHeight_; }
    bool isVisible() const { return isVisible_; }
    bool isMaximized() const { return isMaximized_; }
    
    // Resizing
    void setHeight(int height);
    void beginResize();
    void endResize();
    bool isResizing() const;
    
    // Toggle visibility
    void show();
    void hide();
    void toggle();
    
    // Rendering
    void render(HDC hdc, const RECT& bounds);
    void renderTabs(HDC hdc, const RECT& tabStripBounds);
    void renderContent(HDC hdc, const RECT& contentBounds);
    
    // Events
    std::function<void(const std::string&)> onPanelActivated;
    std::function<void(int)> onHeightChanged;
    std::function<void(bool)> onVisibilityChanged;

private:
    int defaultHeight_;
    int currentHeight_;
    int minHeight_ = 100;
    int maxHeight_ = 600;
    bool isVisible_ = true;
    bool isMaximized_ = false;
    bool isResizing_ = false;
    std::map<std::string, PanelDescriptor> panels_;
    std::map<std::string, HWND> panelHwnds_;
    std::map<std::string, PanelState> panelStates_;
    std::string activePanelId_;
    int tabHeight_ = 24;
};

// ============================================================================
// Split Container
// ============================================================================

struct SplitPane {
    std::string id;
    std::unique_ptr<TabGroup> tabGroup;
    std::unique_ptr<SplitPane> firstChild;
    std::unique_ptr<SplitPane> secondChild;
    SplitDirection splitDirection;
    float splitRatio = 0.5f; // 0.0 to 1.0
    int splitSize = 4; // Sash width
    bool isCollapsed = false;
};

class SplitContainer {
public:
    SplitContainer();
    ~SplitContainer();

    // Split operations
    void split(const std::string& paneId, SplitDirection direction);
    void unsplit(const std::string& paneId);
    void setSplitRatio(const std::string& paneId, float ratio);
    
    // Pane management
    TabGroup* getPane(const std::string& paneId);
    TabGroup* createPane(const std::string& parentId);
    void removePane(const std::string& paneId);
    
    // Layout
    void layout(const RECT& bounds);
    void render(HDC hdc);
    
    // Sash dragging
    void beginSashDrag(const std::string& paneId, POINT pt);
    void updateSashDrag(POINT pt);
    void endSashDrag();
    bool isSashDragging() const;
    
    // Serialization
    std::string serialize() const;
    void deserialize(const std::string& data);

private:
    std::unique_ptr<SplitPane> root_;
    std::string draggedSashId_;
    POINT dragStart_;
    float dragStartRatio_;
    
    void layoutPane(SplitPane* pane, const RECT& bounds);
    void renderPane(HDC hdc, SplitPane* pane);
    SplitPane* findPane(SplitPane* root, const std::string& id);
};

// ============================================================================
// Advanced Dock Manager
// ============================================================================

class AdvancedDockManager {
public:
    AdvancedDockManager();
    ~AdvancedDockManager();

    // Initialization
    void initialize(HWND mainHwnd);
    void shutdown();

    // Main areas
    TabGroup* getCenterArea() { return centerTabs_.get(); }
    SidePanel* getLeftPanel() { return leftPanel_.get(); }
    SidePanel* getRightPanel() { return rightPanel_.get(); }
    BottomPanel* getBottomPanel() { return bottomPanel_.get(); }
    SplitContainer* getSplitContainer() { return splitContainer_.get(); }

    // Layout
    void layout(const RECT& bounds);
    void render(HDC hdc);
    
    // Panel registration
    void registerPanel(const PanelDescriptor& desc);
    void unregisterPanel(const std::string& panelId);
    
    // Show/hide panels
    void showPanel(const std::string& panelId, DockArea area);
    void hidePanel(const std::string& panelId);
    void togglePanel(const std::string& panelId, DockArea area);
    
    // State persistence
    void saveState(const std::string& filePath);
    void loadState(const std::string& filePath);
    void resetToDefault();
    
    // Events
    std::function<void(const std::string&, DockArea)> onPanelMoved;
    std::function<void(const std::string&)> onLayoutChanged;

private:
    HWND mainHwnd_;
    std::unique_ptr<TabGroup> centerTabs_;
    std::unique_ptr<SidePanel> leftPanel_;
    std::unique_ptr<SidePanel> rightPanel_;
    std::unique_ptr<BottomPanel> bottomPanel_;
    std::unique_ptr<SplitContainer> splitContainer_;
    std::map<std::string, PanelDescriptor> registeredPanels_;
    
    void calculateLayout(const RECT& bounds, 
                        RECT& centerRect,
                        RECT& leftRect,
                        RECT& rightRect,
                        RECT& bottomRect);
};

// ============================================================================
// C API for FFI
// ============================================================================

extern "C" {
    __declspec(dllexport) void* DockManager_Create();
    __declspec(dllexport) void DockManager_Destroy(void* manager);
    __declspec(dllexport) void DockManager_Initialize(void* manager, HWND hwnd);
    __declspec(dllexport) void DockManager_Layout(void* manager, RECT* bounds);
    __declspec(dllexport) void DockManager_Render(void* manager, HDC hdc);
    __declspec(dllexport) void DockManager_ShowPanel(void* manager, const char* panelId, int area);
    __declspec(dllexport) void DockManager_HidePanel(void* manager, const char* panelId);
    __declspec(dllexport) void DockManager_SaveState(void* manager, const char* filePath);
    __declspec(dllexport) void DockManager_LoadState(void* manager, const char* filePath);
}

} // namespace UI
} // namespace RawrXD
