// ============================================================================
// advanced_docking_system.h - VS Code-Compatible Docking Layout Engine
// ============================================================================
// Features:
//   - Tab Groups (multi-document tabs with drag-drop)
//   - Side Bar toggles (left/right panels)
//   - Collapsible Bottom Panels (Terminal/Output/Debug)
//   - Split-pane layout with proportional resizing
//   - State persistence across sessions
//
// Architecture: Dock-and-anchor pattern with layout serialization
// ============================================================================

#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <functional>
#include <nlohmann/json.hpp>

// Alias for convenience
using JsonValue = nlohmann::json;

namespace RawrXD {
namespace UI {

// ============================================================================
// Docking Enums and Constants
// ============================================================================
enum class DockZone {
    None = 0,
    Left,
    Right,
    Bottom,
    Center,  // Main editor area
    Floating
};

enum class PanelState {
    Hidden = 0,
    Collapsed,
    Expanded,
    AutoHide
};

enum class TabGroupOrientation {
    Horizontal = 0,
    Vertical
};

// Default dimensions (pixels)
constexpr int DEFAULT_SIDEBAR_WIDTH = 250;
constexpr int DEFAULT_BOTTOM_PANEL_HEIGHT = 200;
constexpr int MIN_PANEL_SIZE = 100;
constexpr int TAB_HEIGHT = 28;
constexpr int GRIPPER_WIDTH = 4;

// ============================================================================
// Forward Declarations
// ============================================================================
class DockingPanel;
class TabGroup;
class SplitContainer;
class DockingManager;

// ============================================================================
// Docking Configuration
// ============================================================================
struct DockingConfig {
    // Side bars
    int leftSidebarWidth = DEFAULT_SIDEBAR_WIDTH;
    int rightSidebarWidth = DEFAULT_SIDEBAR_WIDTH;
    PanelState leftSidebarState = PanelState::Expanded;
    PanelState rightSidebarState = PanelState::Collapsed;
    
    // Bottom panel
    int bottomPanelHeight = DEFAULT_BOTTOM_PANEL_HEIGHT;
    PanelState bottomPanelState = PanelState::Collapsed;
    
    // Tab groups
    bool showTabCloseButtons = true;
    bool enableTabDragDrop = true;
    bool showTabIcons = true;
    TabGroupOrientation tabOrientation = TabGroupOrientation::Horizontal;
    
    // Behavior
    bool autoHideSidebars = false;
    bool restoreLayoutOnStartup = true;
    std::string layoutFilePath = "docking_layout.json";
    
    JsonValue toJson() const;
    static DockingConfig fromJson(const JsonValue& json);
};

// ============================================================================
// Tab Item (Document Tab)
// ============================================================================
struct TabItem {
    std::string id;
    std::string title;
    std::string tooltip;
    std::string iconPath;
    bool isModified = false;
    bool isPinned = false;
    HWND contentHwnd = nullptr;
    std::function<void()> onCloseCallback;
    std::function<void()> onActivateCallback;
    
    TabItem() = default;
    TabItem(const std::string& id_, const std::string& title_, HWND hwnd = nullptr)
        : id(id_), title(title_), contentHwnd(hwnd) {}
};

// ============================================================================
// Tab Group (Multi-Document Container)
// ============================================================================
class TabGroup {
public:
    TabGroup(DockingManager* manager, const std::string& id);
    ~TabGroup();
    
    // Tab management
    size_t addTab(std::shared_ptr<TabItem> tab);
    void removeTab(const std::string& tabId);
    void activateTab(const std::string& tabId);
    void moveTab(size_t fromIndex, size_t toIndex);
    void mergeWith(TabGroup* other);
    
    // Properties
    std::shared_ptr<TabItem> getActiveTab() const;
    std::shared_ptr<TabItem> getTab(const std::string& tabId) const;
    size_t getTabCount() const { return tabs_.size(); }
    bool isEmpty() const { return tabs_.empty(); }
    
    // UI
    void create(HWND parentHwnd, const RECT& rect);
    void destroy();
    void layout(const RECT& rect);
    void paint(HDC hdc);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Serialization
    JsonValue serialize() const;
    void deserialize(const JsonValue& json);

    // Drag-drop
    bool beginDrag(size_t tabIndex);
    void endDrag(bool cancelled);
    bool isDragging() const { return isDragging_; }
    
private:
    DockingManager* manager_;
    std::string id_;
    std::vector<std::shared_ptr<TabItem>> tabs_;
    size_t activeTabIndex_ = 0;
    HWND hwnd_ = nullptr;
    HWND tabBarHwnd_ = nullptr;
    HWND contentHwnd_ = nullptr;
    
    // Drag-drop state
    bool isDragging_ = false;
    size_t dragTabIndex_ = 0;
    POINT dragStartPos_;
    
    // UI helpers
    void createTabBar();
    void updateTabBar();
    RECT getContentRect() const;
    int getTabBarHeight() const;
    size_t hitTestTab(int x, int y) const;
    void drawTab(HDC hdc, size_t index, const RECT& rect, bool active);
};

// ============================================================================
// Docking Panel (Side Bar / Bottom Panel)
// ============================================================================
class DockingPanel {
public:
    DockingPanel(DockingManager* manager, DockZone zone, const std::string& id);
    ~DockingPanel();
    
    // State
    void setState(PanelState state);
    PanelState getState() const { return state_; }
    void toggle();
    void expand();
    void collapse();
    void hide();
    
    // Size
    void setSize(int size);
    int getSize() const { return size_; }
    int getActualSize() const;
    
    // Content
    void setContent(HWND hwnd);
    HWND getContent() const { return contentHwnd_; }
    void setTitle(const std::string& title) { title_ = title; }
    const std::string& getTitle() const { return title_; }
    
    // Tab groups (for multi-view panels)
    TabGroup* createTabGroup(const std::string& id);
    void removeTabGroup(const std::string& id);
    TabGroup* getTabGroup(const std::string& id) const;
    
    // UI
    void create(HWND parentHwnd);
    void destroy();
    void layout(const RECT& rect);
    void paint(HDC hdc);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Hit testing
    bool hitTestGripper(int x, int y) const;
    bool hitTestCollapseButton(int x, int y) const;

    // Serialization
    JsonValue serialize() const;
    void deserialize(const JsonValue& json);

private:
    DockingManager* manager_;
    DockZone zone_;
    std::string id_;
    std::string title_;
    PanelState state_ = PanelState::Expanded;
    int size_ = DEFAULT_SIDEBAR_WIDTH;
    HWND hwnd_ = nullptr;
    HWND contentHwnd_ = nullptr;
    HWND titleBarHwnd_ = nullptr;
    std::map<std::string, std::unique_ptr<TabGroup>> tabGroups_;
    
    // UI helpers
    void createTitleBar();
    void updateTitleBar();
    RECT getContentRect() const;
    void drawGripper(HDC hdc);
    void drawCollapseButton(HDC hdc);
};

// ============================================================================
// Split Container (Resizable Splitter)
// ============================================================================
class SplitContainer {
public:
    enum class Orientation {
        Horizontal = 0,  // Left/Right split
        Vertical         // Top/Bottom split
    };
    
    SplitContainer(DockingManager* manager, Orientation orient);
    ~SplitContainer();
    
    // Panes
    void setFirstPane(TabGroup* pane);
    void setSecondPane(TabGroup* pane);
    void setFirstPane(SplitContainer* pane);
    void setSecondPane(SplitContainer* pane);
    
    // Split ratio (0.0 - 1.0)
    void setSplitRatio(float ratio);
    float getSplitRatio() const { return splitRatio_; }
    
    // UI
    void create(HWND parentHwnd, const RECT& rect);
    void destroy();
    void layout(const RECT& rect);
    void paint(HDC hdc);
    LRESULT handleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Hit testing
    bool hitTestSplitter(int x, int y) const;
    void beginSplitterDrag(int x, int y);
    void updateSplitterDrag(int x, int y);
    void endSplitterDrag();
    bool isSplitterDragging() const { return isDragging_; }
    
private:
    DockingManager* manager_;
    Orientation orientation_;
    float splitRatio_ = 0.5f;
    HWND hwnd_ = nullptr;
    
    // Child panes (variant: TabGroup or SplitContainer)
    enum class PaneType { None, TabGroup, SplitContainer };
    PaneType firstType_ = PaneType::None;
    PaneType secondType_ = PaneType::None;
    union {
        TabGroup* tabGroup_;
        SplitContainer* splitContainer_;
    } firstPane_;
    union {
        TabGroup* tabGroup_;
        SplitContainer* splitContainer_;
    } secondPane_;
    
    // Dragging
    bool isDragging_ = false;
    POINT dragStartPos_;
    float dragStartRatio_;
    
    // Helpers
    RECT getFirstPaneRect(const RECT& total) const;
    RECT getSecondPaneRect(const RECT& total) const;
    RECT getSplitterRect(const RECT& total) const;
    int getSplitterThickness() const { return GRIPPER_WIDTH; }
};

// ============================================================================
// Docking Manager (Main Controller)
// ============================================================================
class DockingManager {
public:
    static DockingManager& instance();
    
    // Initialization
    bool initialize(HWND mainHwnd);
    void shutdown();
    
    // Layout
    void updateLayout();
    void saveLayout();
    void restoreLayout();
    void resetLayout();
    
    // Panels
    DockingPanel* getPanel(DockZone zone) const;
    DockingPanel* createPanel(DockZone zone, const std::string& id);
    void destroyPanel(const std::string& id);
    void togglePanel(DockZone zone);
    void showPanel(DockZone zone);
    void hidePanel(DockZone zone);
    
    // Tab groups
    TabGroup* getMainTabGroup() { return mainTabGroup_.get(); }
    TabGroup* createTabGroup(const std::string& id);
    void destroyTabGroup(const std::string& id);
    TabGroup* findTabGroup(const std::string& id) const;
    
    // Split containers
    SplitContainer* createSplitContainer(SplitContainer::Orientation orient);
    void destroySplitContainer(SplitContainer* container);
    
    // Configuration
    DockingConfig& getConfig() { return config_; }
    void setConfig(const DockingConfig& config) { config_ = config; }
    
    // Event callbacks
    using LayoutChangedCallback = std::function<void()>;
    void onLayoutChanged(LayoutChangedCallback cb) { layoutChangedCallbacks_.push_back(cb); }
    
    // Window procedure
    LRESULT handleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
    
    // Helpers
    HCURSOR getResizeCursor(DockZone zone) const;
    HFONT getTabFont() const { return tabFont_; }
    HBRUSH getPanelBrush() const { return panelBrush_; }
    COLORREF getTabActiveColor() const { return RGB(30, 30, 30); }
    COLORREF getTabInactiveColor() const { return RGB(45, 45, 45); }
    
private:
    DockingManager() = default;
    ~DockingManager() = default;
    DockingManager(const DockingManager&) = delete;
    DockingManager& operator=(const DockingManager&) = delete;
    
    bool initialized_ = false;
    HWND mainHwnd_ = nullptr;
    DockingConfig config_;
    
    // Panels
    std::map<DockZone, std::unique_ptr<DockingPanel>> panels_;
    std::map<std::string, std::unique_ptr<TabGroup>> tabGroups_;
    std::vector<std::unique_ptr<SplitContainer>> splitContainers_;
    std::unique_ptr<TabGroup> mainTabGroup_;
    
    // GDI resources
    HFONT tabFont_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HCURSOR hCursorResizeWE_ = nullptr;
    HCURSOR hCursorResizeNS_ = nullptr;
    
    // Callbacks
    std::vector<LayoutChangedCallback> layoutChangedCallbacks_;
    
    // Layout calculation
    RECT getMainClientRect() const;
    RECT getLeftPanelRect(const RECT& total) const;
    RECT getRightPanelRect(const RECT& total) const;
    RECT getBottomPanelRect(const RECT& total) const;
    RECT getCenterRect(const RECT& total) const;
    
    // Resource management
    void createGdiResources();
    void destroyGdiResources();
    void notifyLayoutChanged();
};

// ============================================================================
// Utility Functions
// ============================================================================
std::string dockZoneToString(DockZone zone);
DockZone stringToDockZone(const std::string& str);
std::string panelStateToString(PanelState state);
PanelState stringToPanelState(const std::string& str);

} // namespace UI
} // namespace RawrXD
