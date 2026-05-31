// ============================================================================
// advanced_docking_system.hpp - VS Code-Compatible Docking Layout Engine
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

namespace Json = nlohmann;

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
constexpr int MIN_SIDEBAR_WIDTH = 150;
constexpr int MAX_SIDEBAR_WIDTH = 500;
constexpr int MIN_BOTTOM_PANEL_HEIGHT = 100;
constexpr int MAX_BOTTOM_PANEL_HEIGHT = 600;
constexpr int TAB_BAR_HEIGHT = 28;
constexpr int PANEL_TITLE_HEIGHT = 24;

// ============================================================================
// Forward Declarations
// ============================================================================
class DockingManager;
class DockingPanel;
class TabGroup;
class TabItem;

// ============================================================================
// Docking Configuration
// ============================================================================
struct DockingConfig {
    // Sidebar dimensions
    int leftSidebarWidth = DEFAULT_SIDEBAR_WIDTH;
    int rightSidebarWidth = DEFAULT_SIDEBAR_WIDTH;
    PanelState leftSidebarState = PanelState::Expanded;
    PanelState rightSidebarState = PanelState::Expanded;
    
    // Bottom panel
    int bottomPanelHeight = DEFAULT_BOTTOM_PANEL_HEIGHT;
    PanelState bottomPanelState = PanelState::Expanded;
    
    // Tab behavior
    bool showTabCloseButtons = true;
    bool enableTabDragDrop = true;
    bool showTabIcons = true;
    TabGroupOrientation tabOrientation = TabGroupOrientation::Horizontal;
    
    // Behavior
    bool autoHideSidebars = false;
    bool restoreLayoutOnStartup = true;
    std::string layoutFilePath = "docking_layout.json";
    
    json toJson() const;
    static DockingConfig fromJson(const json& j);
};

// ============================================================================
// Tab Item (Document Tab)
// ============================================================================
struct TabItem {
    std::string id;
    std::string title;
    std::string tooltip;
    HWND contentHwnd = nullptr;
    bool isModified = false;
    bool isActive = false;
    HICON icon = nullptr;
    
    // Callbacks
    std::function<void()> onClose;
    std::function<void()> onActivate;
    
    TabItem(const std::string& id_, const std::string& title_) 
        : id(id_), title(title_) {}
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
    std::shared_ptr<TabItem> getActiveTab() const;
    std::shared_ptr<TabItem> getTab(const std::string& tabId) const;
    size_t getTabCount() const { return tabs_.size(); }
    
    // UI
    void create(HWND parentHwnd, const RECT& rect);
    void destroy();
    void layout(const RECT& rect);
    void redraw();
    
    // Serialization
    json serialize() const;
    void deserialize(const json& j);
    
    // Hit testing
    std::string hitTestTab(int x, int y) const;
    bool hitTestCloseButton(int x, int y) const;
    
    // Drag-drop
    void startDrag(const std::string& tabId, int x, int y);
    void endDrag(bool cancel = false);
    bool isDragging() const { return isDragging_; }
    
private:
    void createTabBar();
    void destroyTabBar();
    void updateTabBar();
    void drawTab(HDC hdc, size_t index, const RECT& tabRect);
    void drawCloseButton(HDC hdc, const RECT& closeRect);
    RECT getTabRect(size_t index) const;
    RECT getCloseButtonRect(size_t index) const;
    
    DockingManager* manager_;
    std::string id_;
    std::vector<std::shared_ptr<TabItem>> tabs_;
    size_t activeTabIndex_ = 0;
    
    HWND hwnd_ = nullptr;
    HWND tabBarHwnd_ = nullptr;
    HWND contentHwnd_ = nullptr;
    
    // Drag-drop state
    bool isDragging_ = false;
    std::string draggedTabId_;
    int dragStartX_ = 0;
    int dragStartY_ = 0;
    
    // GDI resources
    HFONT tabFont_ = nullptr;
    HBRUSH activeTabBrush_ = nullptr;
    HBRUSH inactiveTabBrush_ = nullptr;
    HPEN tabBorderPen_ = nullptr;
};

// ============================================================================
// Docking Panel (Side/Bottom Panels)
// ============================================================================
class DockingPanel {
public:
    DockingPanel(DockingManager* manager, DockZone zone, const std::string& id);
    ~DockingPanel();
    
    // Lifecycle
    void create(HWND parentHwnd);
    void destroy();
    void layout(const RECT& rect);
    
    // State
    void setState(PanelState state);
    PanelState getState() const { return state_; }
    void toggle();
    void collapse();
    void expand();
    void autoHide();
    void show();
    void hide();
    
    // Content
    void setContent(HWND contentHwnd);
    HWND getContent() const { return contentHwnd_; }
    void setTitle(const std::string& title);
    std::string getTitle() const { return title_; }
    
    // Sizing
    void setSize(const SIZE& newSize);
    SIZE getSize() const { return size_; }
    int getWidth() const { return size_.cx; }
    int getHeight() const { return size_.cy; }
    
    // Hit testing
    bool hitTestResizeHandle(int x, int y) const;
    bool hitTestTitleBar(int x, int y) const;
    bool hitTestCollapseButton(int x, int y) const;
    
    // Serialization
    json serialize() const;
    void deserialize(const json& j);
    
    // Zone
    DockZone getZone() const { return zone_; }
    
private:
    void createTitleBar();
    void destroyTitleBar();
    void updateTitleBar();
    void drawTitleBar(HDC hdc);
    void drawResizeHandle(HDC hdc);
    
    DockingManager* manager_;
    DockZone zone_;
    std::string id_;
    std::string title_;
    PanelState state_ = PanelState::Expanded;
    SIZE size_ = { DEFAULT_SIDEBAR_WIDTH, 0 };
    
    HWND hwnd_ = nullptr;
    HWND titleBarHwnd_ = nullptr;
    HWND contentHwnd_ = nullptr;
    
    // Auto-hide
    bool isAutoHideVisible_ = false;
    UINT autoHideTimer_ = 0;
    
    // GDI resources
    HFONT titleFont_ = nullptr;
    HBRUSH titleBarBrush_ = nullptr;
    HCURSOR resizeCursor_ = nullptr;
};

// ============================================================================
// Docking Manager (Main Controller)
// ============================================================================
class DockingManager {
public:
    DockingManager();
    ~DockingManager();
    
    // Lifecycle
    bool initialize(HWND mainHwnd);
    void shutdown();
    bool isInitialized() const { return initialized_; }
    
    // Layout
    void updateLayout();
    void updateLayout(const RECT& clientRect);
    RECT getCenterRect() const;  // Available space for main editor
    
    // Panel management
    DockingPanel* createPanel(DockZone zone, const std::string& id);
    void destroyPanel(const std::string& id);
    DockingPanel* getPanel(DockZone zone) const;
    DockingPanel* getPanel(const std::string& id) const;
    void togglePanel(DockZone zone);
    void setPanelState(DockZone zone, PanelState state);
    
    // Tab groups
    TabGroup* createTabGroup(const std::string& id);
    void destroyTabGroup(const std::string& id);
    TabGroup* getTabGroup(const std::string& id) const;
    TabGroup* getMainTabGroup() const { return mainTabGroup_.get(); }
    
    // Configuration
    void setConfig(const DockingConfig& config);
    DockingConfig getConfig() const { return config_; }
    
    // Serialization
    void saveLayout();
    void restoreLayout();
    void resetLayout();
    
    // Hit testing
    DockZone hitTestZone(int x, int y) const;
    bool hitTestSplitter(int x, int y, DockZone* zone) const;
    
    // Splitter dragging
    void startSplitterDrag(DockZone zone, int x, int y);
    void endSplitterDrag();
    void updateSplitterDrag(int x, int y);
    bool isSplitterDragging() const { return isSplitterDragging_; }
    
    // Events
    void onSize();
    void onMouseMove(int x, int y);
    void onLButtonDown(int x, int y);
    void onLButtonUp(int x, int y);
    void onLButtonDblClk(int x, int y);
    
    // Callbacks
    using LayoutChangedCallback = std::function<void()>;
    void setLayoutChangedCallback(LayoutChangedCallback cb) { layoutChangedCallback_ = cb; }
    
private:
    void notifyLayoutChanged();
    void createGdiResources();
    void destroyGdiResources();
    void drawSplitter(HDC hdc, DockZone zone, const RECT& rect);
    RECT getPanelRect(DockZone zone) const;
    RECT getSplitterRect(DockZone zone) const;
    int getSplitterWidth() const { return 4; }
    
    HWND mainHwnd_ = nullptr;
    bool initialized_ = false;
    DockingConfig config_;
    RECT clientRect_ = {};
    
    // Panels
    std::map<DockZone, std::unique_ptr<DockingPanel>> panels_;
    std::map<std::string, DockZone> panelIdToZone_;
    
    // Tab groups
    std::map<std::string, std::unique_ptr<TabGroup>> tabGroups_;
    std::unique_ptr<TabGroup> mainTabGroup_;
    
    // Splitter dragging
    bool isSplitterDragging_ = false;
    DockZone draggedSplitterZone_ = DockZone::None;
    int dragStartX_ = 0;
    int dragStartY_ = 0;
    int dragStartSize_ = 0;
    
    // GDI resources
    HFONT tabFont_ = nullptr;
    HBRUSH panelBrush_ = nullptr;
    HCURSOR hCursorResizeWE_ = nullptr;
    HCURSOR hCursorResizeNS_ = nullptr;
    
    // Callbacks
    LayoutChangedCallback layoutChangedCallback_;
};

// ============================================================================
// Global Access
// ============================================================================
DockingManager* getDockingManager();

} // namespace UI
} // namespace RawrXD
