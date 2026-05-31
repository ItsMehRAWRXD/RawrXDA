#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>

namespace RawrXD {
namespace UI {

// ============================================================================
// Docking Configuration
// ============================================================================

enum class DockPosition {
    Left,
    Right,
    Bottom,
    Center,  // Tab group
    Floating
};

enum class PanelState {
    Hidden,
    Visible,
    Collapsed,
    Expanded
};

struct DockingConfig {
    int minWidth = 200;
    int minHeight = 150;
    int defaultWidth = 300;
    int defaultHeight = 250;
    bool resizable = true;
    bool collapsible = true;
    bool showTabs = true;
    int tabHeight = 24;
};

// ============================================================================
// Tab Group
// ============================================================================

struct TabInfo {
    HWND hwnd;
    std::wstring title;
    std::wstring tooltip;
    bool modified = false;
    bool active = false;
    HICON icon = nullptr;
};

class TabGroup {
public:
    TabGroup(const std::wstring& groupId);
    
    // Tab management
    void addTab(const TabInfo& tab);
    void removeTab(HWND hwnd);
    void activateTab(HWND hwnd);
    void moveTab(HWND hwnd, int newIndex);
    void updateTabTitle(HWND hwnd, const std::wstring& title);
    void setTabModified(HWND hwnd, bool modified);
    
    // Layout
    void setRect(const RECT& rect);
    void recalculateLayout();
    void draw(HDC hdc);
    
    // Hit testing
    HWND hitTest(int x, int y) const;
    int hitTestTab(int x, int y) const;
    
    // Accessors
    const std::vector<TabInfo>& getTabs() const { return tabs_; }
    HWND getActiveTab() const;
    bool isEmpty() const { return tabs_.empty(); }
    
private:
    std::wstring groupId_;
    std::vector<TabInfo> tabs_;
    RECT rect_;
    int activeTabIndex_ = -1;
    int hoveredTabIndex_ = -1;
    
    // Tab metrics
    int tabHeight_ = 24;
    int tabPadding_ = 8;
    int closeButtonWidth_ = 16;
};

// ============================================================================
// Side Panel
// ============================================================================

class SidePanel {
public:
    SidePanel(DockPosition position, const DockingConfig& config);
    
    // Panel management
    void addPanel(HWND hwnd, const std::wstring& title, int priority = 0);
    void removePanel(HWND hwnd);
    void showPanel(HWND hwnd);
    void hidePanel(HWND hwnd);
    void collapse();
    void expand();
    void toggle();
    
    // Layout
    void setRect(const RECT& rect);
    void setSplitterPos(int pos);
    int getSplitterPos() const { return splitterPos_; }
    void recalculateLayout();
    void draw(HDC hdc);
    
    // Hit testing
    HWND hitTest(int x, int y) const;
    bool hitTestSplitter(int x, int y) const;
    bool hitTestCollapseButton(int x, int y) const;
    
    // Accessors
    DockPosition getPosition() const { return position_; }
    PanelState getState() const { return state_; }
    bool isCollapsed() const { return state_ == PanelState::Collapsed; }
    int getWidth() const;
    int getHeight() const;
    
private:
    DockPosition position_;
    DockingConfig config_;
    PanelState state_ = PanelState::Expanded;
    RECT rect_;
    int splitterPos_ = 0;
    int collapsedSize_ = 32; // Width/height when collapsed
    bool isDraggingSplitter_ = false;
    
    struct PanelInfo {
        HWND hwnd;
        std::wstring title;
        int priority;
        bool visible;
        HICON icon;
    };
    std::vector<PanelInfo> panels_;
};

// ============================================================================
// Bottom Panel (Terminal/Output)
// ============================================================================

class BottomPanel {
public:
    BottomPanel(const DockingConfig& config);
    
    // Panel management
    void addPanel(HWND hwnd, const std::wstring& title, const std::wstring& panelId);
    void removePanel(const std::wstring& panelId);
    void showPanel(const std::wstring& panelId);
    void hidePanel(const std::wstring& panelId);
    void maximizePanel(const std::wstring& panelId);
    void restorePanel();
    
    // Layout
    void setRect(const RECT& rect);
    void setHeight(int height);
    int getHeight() const { return currentHeight_; }
    void recalculateLayout();
    void draw(HDC hdc);
    
    // Hit testing
    HWND hitTest(int x, int y) const;
    bool hitTestResizeHandle(int x, int y) const;
    
    // Accessors
    bool isVisible() const { return visible_; }
    void setVisible(bool visible) { visible_ = visible; }
    bool isMaximized() const { return maximized_; }
    
private:
    DockingConfig config_;
    RECT rect_;
    int currentHeight_ = 250;
    int minimizedHeight_ = 32;
    int maximizedHeight_ = 600;
    bool visible_ = true;
    bool maximized_ = false;
    bool isDragging_ = false;
    
    struct BottomPanelInfo {
        HWND hwnd;
        std::wstring title;
        std::wstring panelId;
        bool visible;
        RECT rect;
    };
    std::map<std::wstring, BottomPanelInfo> panels_;
    std::wstring activePanelId_;
};

// ============================================================================
// Advanced Docking Manager
// ============================================================================

class AdvancedDockingManager {
public:
    AdvancedDockingManager();
    ~AdvancedDockingManager();
    
    // Initialization
    bool initialize(HWND hWndMain);
    void shutdown();
    
    // Layout management
    void setMainAreaRect(const RECT& rect);
    void recalculateLayout();
    void draw(HDC hdc);
    
    // Tab groups
    TabGroup* createTabGroup(const std::wstring& groupId);
    void destroyTabGroup(const std::wstring& groupId);
    TabGroup* getTabGroup(const std::wstring& groupId);
    void moveTabBetweenGroups(HWND hwnd, const std::wstring& fromGroup, const std::wstring& toGroup);
    
    // Side panels
    SidePanel* getLeftPanel() { return leftPanel_.get(); }
    SidePanel* getRightPanel() { return rightPanel_.get(); }
    void toggleLeftPanel();
    void toggleRightPanel();
    void setLeftPanelWidth(int width);
    void setRightPanelWidth(int width);
    
    // Bottom panel
    BottomPanel* getBottomPanel() { return bottomPanel_.get(); }
    void toggleBottomPanel();
    void setBottomPanelHeight(int height);
    void maximizeBottomPanel();
    void restoreBottomPanel();
    
    // Hit testing
    enum class HitTestResult {
        None,
        Tab,
        TabCloseButton,
        SidePanelSplitter,
        BottomPanelResize,
        CollapseButton,
        PanelHeader
    };
    
    struct HitTestInfo {
        HitTestResult result;
        HWND hwnd;
        std::wstring panelId;
        int tabIndex;
        DockPosition panelPosition;
    };
    
    HitTestInfo hitTest(int x, int y) const;
    
    // Dragging
    void beginSplitterDrag(DockPosition panel, int pos);
    void updateSplitterDrag(int pos);
    void endSplitterDrag();
    bool isDraggingSplitter() const { return draggingSplitter_; }
    
    // Serialization
    std::wstring serializeLayout() const;
    bool deserializeLayout(const std::wstring& layoutJson);
    void saveLayoutToSettings();
    void loadLayoutFromSettings();
    
    // Events
    using LayoutChangedCallback = std::function<void()>;
    void setLayoutChangedCallback(LayoutChangedCallback cb) { layoutChangedCallback_ = cb; }
    
private:
    HWND hWndMain_ = nullptr;
    RECT mainAreaRect_;
    
    std::map<std::wstring, std::unique_ptr<TabGroup>> tabGroups_;
    std::unique_ptr<SidePanel> leftPanel_;
    std::unique_ptr<SidePanel> rightPanel_;
    std::unique_ptr<BottomPanel> bottomPanel_;
    
    bool draggingSplitter_ = false;
    DockPosition draggedPanel_;
    int dragStartPos_ = 0;
    int dragStartSplitterPos_ = 0;
    
    LayoutChangedCallback layoutChangedCallback_;
    
    void notifyLayoutChanged();
};

} // namespace UI
} // namespace RawrXD

// Legacy compatibility
using DockingPaneManager = RawrXD::UI::AdvancedDockingManager;
