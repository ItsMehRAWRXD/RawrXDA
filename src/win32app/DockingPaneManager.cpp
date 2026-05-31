/**
 * @file DockingPaneManager.cpp
 * @brief Advanced Docking Manager Implementation
 * 
 * Implements VS Code-class docking with tab groups, side panels, and bottom panels.
 * 
 * @author RawrXD UI Team
 * @version 2.0.0
 */

#include "DockingPaneManager.h"
#include <windowsx.h>
#include <commctrl.h>
#include <algorithm>
#include <cstring>

#pragma comment(lib, "comctl32.lib")

namespace RawrXD {
namespace UI {

// ============================================================================
// TabGroup Implementation
// ============================================================================

TabGroup::TabGroup(const std::wstring& groupId) : groupId_(groupId) {
    rect_ = {0, 0, 0, 0};
}

void TabGroup::addTab(const TabInfo& tab) {
    tabs_.push_back(tab);
    if (activeTabIndex_ == -1) {
        activeTabIndex_ = 0;
        tabs_[0].active = true;
    }
    recalculateLayout();
}

void TabGroup::removeTab(HWND hwnd) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [hwnd](const TabInfo& t) { return t.hwnd == hwnd; });
    
    if (it != tabs_.end()) {
        int index = static_cast<int>(std::distance(tabs_.begin(), it));
        tabs_.erase(it);
        
        if (activeTabIndex_ == index) {
            activeTabIndex_ = tabs_.empty() ? -1 : 0;
            if (activeTabIndex_ >= 0) {
                tabs_[activeTabIndex_].active = true;
            }
        } else if (activeTabIndex_ > index) {
            activeTabIndex_--;
        }
        
        recalculateLayout();
    }
}

void TabGroup::activateTab(HWND hwnd) {
    for (size_t i = 0; i < tabs_.size(); ++i) {
        tabs_[i].active = (tabs_[i].hwnd == hwnd);
        if (tabs_[i].hwnd == hwnd) {
            activeTabIndex_ = static_cast<int>(i);
        }
    }
    recalculateLayout();
}

void TabGroup::moveTab(HWND hwnd, int newIndex) {
    auto it = std::find_if(tabs_.begin(), tabs_.end(),
        [hwnd](const TabInfo& t) { return t.hwnd == hwnd; });
    
    if (it != tabs_.end() && newIndex >= 0 && newIndex < static_cast<int>(tabs_.size())) {
        TabInfo tab = *it;
        tabs_.erase(it);
        tabs_.insert(tabs_.begin() + newIndex, tab);
        recalculateLayout();
    }
}

void TabGroup::updateTabTitle(HWND hwnd, const std::wstring& title) {
    for (auto& tab : tabs_) {
        if (tab.hwnd == hwnd) {
            tab.title = title;
            break;
        }
    }
}

void TabGroup::setTabModified(HWND hwnd, bool modified) {
    for (auto& tab : tabs_) {
        if (tab.hwnd == hwnd) {
            tab.modified = modified;
            break;
        }
    }
}

void TabGroup::setRect(const RECT& rect) {
    rect_ = rect;
    recalculateLayout();
}

void TabGroup::recalculateLayout() {
    if (tabs_.empty() || activeTabIndex_ < 0) return;
    
    // Position the active tab's content
    RECT contentRect = rect_;
    contentRect.top += tabHeight_;
    
    if (activeTabIndex_ >= 0 && activeTabIndex_ < static_cast<int>(tabs_.size())) {
        SetWindowPos(tabs_[activeTabIndex_].hwnd, nullptr,
            contentRect.left, contentRect.top,
            contentRect.right - contentRect.left,
            contentRect.bottom - contentRect.top,
            SWP_NOZORDER | SWP_SHOWWINDOW);
        
        // Hide inactive tabs
        for (size_t i = 0; i < tabs_.size(); ++i) {
            if (i != static_cast<size_t>(activeTabIndex_)) {
                ShowWindow(tabs_[i].hwnd, SW_HIDE);
            }
        }
    }
}

void TabGroup::draw(HDC hdc) {
    if (tabs_.empty()) return;
    
    // Draw tab bar background
    RECT tabBarRect = {rect_.left, rect_.top, rect_.right, rect_.top + tabHeight_};
    FillRect(hdc, &tabBarRect, (HBRUSH)(COLOR_3DFACE + 1));
    
    // Draw tabs
    int x = rect_.left;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        const TabInfo& tab = tabs_[i];
        
        // Calculate tab width based on title
        SIZE textSize;
        GetTextExtentPoint32(hdc, tab.title.c_str(), static_cast<int>(tab.title.length()), 
            &textSize);
        int tabWidth = textSize.cx + tabPadding_ * 2 + closeButtonWidth_;
        
        RECT tabRect = {x, rect_.top, x + tabWidth, rect_.top + tabHeight_};
        
        // Draw tab background
        if (tab.active) {
            FillRect(hdc, &tabRect, (HBRUSH)(COLOR_WINDOW + 1));
        } else if (i == static_cast<size_t>(hoveredTabIndex_)) {
            FillRect(hdc, &tabRect, (HBRUSH)(COLOR_3DHILIGHT + 1));
        }
        
        // Draw tab text
        SetBkMode(hdc, TRANSPARENT);
        RECT textRect = {x + tabPadding_, rect_.top + 2, 
                        x + tabWidth - closeButtonWidth_, rect_.top + tabHeight_};
        
        // Modified indicator
        std::wstring displayTitle = tab.modified ? tab.title + L" *" : tab.title;
        DrawText(hdc, displayTitle.c_str(), -1, &textRect, 
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        // Draw close button (X)
        if (tabs_.size() > 1 || tab.active) {
            RECT closeRect = {x + tabWidth - closeButtonWidth_, rect_.top + 4,
                            x + tabWidth - 4, rect_.top + tabHeight_ - 4};
            DrawText(hdc, L"×", 1, &closeRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        
        x += tabWidth;
    }
    
    // Draw border line under tabs
    HPEN hPen = CreatePen(PS_SOLID, 1, GetSysColor(COLOR_3DSHADOW));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, rect_.left, rect_.top + tabHeight_, nullptr);
    LineTo(hdc, rect_.right, rect_.top + tabHeight_);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
}

HWND TabGroup::hitTest(int x, int y) const {
    if (y < rect_.top || y >= rect_.top + tabHeight_) {
        return nullptr;
    }
    
    int tabX = rect_.left;
    for (const auto& tab : tabs_) {
        SIZE textSize;
        HDC hdc = GetDC(nullptr);
        GetTextExtentPoint32(hdc, tab.title.c_str(), static_cast<int>(tab.title.length()), 
            &textSize);
        ReleaseDC(nullptr, hdc);
        
        int tabWidth = textSize.cx + tabPadding_ * 2 + closeButtonWidth_;
        if (x >= tabX && x < tabX + tabWidth) {
            return tab.hwnd;
        }
        tabX += tabWidth;
    }
    
    return nullptr;
}

int TabGroup::hitTestTab(int x, int y) const {
    if (y < rect_.top || y >= rect_.top + tabHeight_) {
        return -1;
    }
    
    int tabX = rect_.left;
    for (size_t i = 0; i < tabs_.size(); ++i) {
        SIZE textSize;
        HDC hdc = GetDC(nullptr);
        GetTextExtentPoint32(hdc, tabs_[i].title.c_str(), 
            static_cast<int>(tabs_[i].title.length()), 
            &textSize);
        ReleaseDC(nullptr, hdc);
        
        int tabWidth = textSize.cx + tabPadding_ * 2 + closeButtonWidth_;
        if (x >= tabX && x < tabX + tabWidth) {
            return static_cast<int>(i);
        }
        tabX += tabWidth;
    }
    
    return -1;
}

HWND TabGroup::getActiveTab() const {
    if (activeTabIndex_ >= 0 && activeTabIndex_ < static_cast<int>(tabs_.size())) {
        return tabs_[activeTabIndex_].hwnd;
    }
    return nullptr;
}

// ============================================================================
// SidePanel Implementation
// ============================================================================

SidePanel::SidePanel(DockPosition position, const DockingConfig& config)
    : position_(position), config_(config) {
    rect_ = {0, 0, 0, 0};
    splitterPos_ = config.defaultWidth;
}

void SidePanel::addPanel(HWND hwnd, const std::wstring& title, int priority) {
    PanelInfo info = {hwnd, title, priority, true, nullptr};
    panels_.push_back(info);
    
    // Sort by priority
    std::sort(panels_.begin(), panels_.end(),
        [](const PanelInfo& a, const PanelInfo& b) {
            return a.priority > b.priority;
        });
    
    recalculateLayout();
}

void SidePanel::removePanel(HWND hwnd) {
    panels_.erase(std::remove_if(panels_.begin(), panels_.end(),
        [hwnd](const PanelInfo& p) { return p.hwnd == hwnd; }), panels_.end());
    recalculateLayout();
}

void SidePanel::showPanel(HWND hwnd) {
    for (auto& panel : panels_) {
        if (panel.hwnd == hwnd) {
            panel.visible = true;
            break;
        }
    }
    recalculateLayout();
}

void SidePanel::hidePanel(HWND hwnd) {
    for (auto& panel : panels_) {
        if (panel.hwnd == hwnd) {
            panel.visible = false;
            break;
        }
    }
    recalculateLayout();
}

void SidePanel::collapse() {
    state_ = PanelState::Collapsed;
    recalculateLayout();
}

void SidePanel::expand() {
    state_ = PanelState::Expanded;
    recalculateLayout();
}

void SidePanel::toggle() {
    if (state_ == PanelState::Collapsed) {
        expand();
    } else {
        collapse();
    }
}

void SidePanel::setRect(const RECT& rect) {
    rect_ = rect;
    recalculateLayout();
}

void SidePanel::setSplitterPos(int pos) {
    splitterPos_ = std::max(config_.minWidth, std::min(pos, 
        (position_ == DockPosition::Left || position_ == DockPosition::Right) ? 
        rect_.right - rect_.left - config_.minWidth : rect_.bottom - rect_.top - config_.minHeight));
    recalculateLayout();
}

void SidePanel::recalculateLayout() {
    if (panels_.empty()) return;
    
    if (state_ == PanelState::Collapsed) {
        // Show only collapse button
        for (auto& panel : panels_) {
            ShowWindow(panel.hwnd, SW_HIDE);
        }
        return;
    }
    
    // Calculate available space
    int availableSpace = (position_ == DockPosition::Left || position_ == DockPosition::Right) ?
        rect_.right - rect_.left : rect_.bottom - rect_.top;
    
    int panelCount = static_cast<int>(std::count_if(panels_.begin(), panels_.end(),
        [](const PanelInfo& p) { return p.visible; }));
    
    if (panelCount == 0) return;
    
    // Layout visible panels
    int panelSize = availableSpace / panelCount;
    int currentPos = (position_ == DockPosition::Left || position_ == DockPosition::Right) ? 
        rect_.left : rect_.top;
    
    for (auto& panel : panels_) {
        if (!panel.visible) {
            ShowWindow(panel.hwnd, SW_HIDE);
            continue;
        }
        
        if (position_ == DockPosition::Left || position_ == DockPosition::Right) {
            SetWindowPos(panel.hwnd, nullptr, currentPos, rect_.top,
                        panelSize, rect_.bottom - rect_.top,
                        SWP_NOZORDER | SWP_SHOWWINDOW);
            currentPos += panelSize;
        } else {
            SetWindowPos(panel.hwnd, nullptr, rect_.left, currentPos,
                        rect_.right - rect_.left, panelSize,
                        SWP_NOZORDER | SWP_SHOWWINDOW);
            currentPos += panelSize;
        }
    }
}

void SidePanel::draw(HDC hdc) {
    if (state_ == PanelState::Collapsed) {
        // Draw collapsed bar
        FillRect(hdc, &rect_, (HBRUSH)(COLOR_3DFACE + 1));
        
        // Draw expand button
        RECT btnRect = rect_;
        if (position_ == DockPosition::Left) {
            btnRect.right = btnRect.left + collapsedSize_;
        } else if (position_ == DockPosition::Right) {
            btnRect.left = btnRect.right - collapsedSize_;
        }
        
        // Draw arrow
        SetBkMode(hdc, TRANSPARENT);
        const wchar_t* arrow = (position_ == DockPosition::Left) ? L"▶" : L"◀";
        DrawText(hdc, arrow, 1, &btnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        return;
    }
    
    // Draw splitter
    HPEN hPen = CreatePen(PS_SOLID, 4, GetSysColor(COLOR_3DSHADOW));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    
    if (position_ == DockPosition::Left) {
        MoveToEx(hdc, rect_.right - 2, rect_.top, nullptr);
        LineTo(hdc, rect_.right - 2, rect_.bottom);
    } else if (position_ == DockPosition::Right) {
        MoveToEx(hdc, rect_.left + 2, rect_.top, nullptr);
        LineTo(hdc, rect_.left + 2, rect_.bottom);
    }
    
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    
    // Draw panel headers
    int headerHeight = 24;
    int y = rect_.top;
    for (const auto& panel : panels_) {
        if (!panel.visible) continue;
        
        RECT headerRect = {rect_.left, y, rect_.right, y + headerHeight};
        FillRect(hdc, &headerRect, (HBRUSH)(COLOR_3DFACE + 1));
        
        SetBkMode(hdc, TRANSPARENT);
        DrawText(hdc, panel.title.c_str(), -1, &headerRect,
                DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        
        y += headerHeight;
    }
}

HWND SidePanel::hitTest(int x, int y) const {
    for (const auto& panel : panels_) {
        if (panel.visible) {
            RECT rect;
            GetWindowRect(panel.hwnd, &rect);
            MapWindowPoints(HWND_DESKTOP, GetParent(panel.hwnd), (LPPOINT)&rect, 2);
            
            if (x >= rect.left && x < rect.right &&
                y >= rect.top && y < rect.bottom) {
                return panel.hwnd;
            }
        }
    }
    return nullptr;
}

bool SidePanel::hitTestSplitter(int x, int y) const {
    if (state_ == PanelState::Collapsed) return false;
    
    const int splitterWidth = 4;
    if (position_ == DockPosition::Left) {
        return (x >= rect_.right - splitterWidth && x <= rect_.right);
    } else if (position_ == DockPosition::Right) {
        return (x >= rect_.left && x <= rect_.left + splitterWidth);
    }
    return false;
}

bool SidePanel::hitTestCollapseButton(int x, int y) const {
    if (state_ != PanelState::Collapsed) return false;
    
    RECT btnRect = rect_;
    if (position_ == DockPosition::Left) {
        btnRect.right = btnRect.left + collapsedSize_;
    } else if (position_ == DockPosition::Right) {
        btnRect.left = btnRect.right - collapsedSize_;
    }
    
    return (x >= btnRect.left && x < btnRect.right &&
            y >= btnRect.top && y < btnRect.bottom);
}

int SidePanel::getWidth() const {
    if (state_ == PanelState::Collapsed) {
        return collapsedSize_;
    }
    return (position_ == DockPosition::Left || position_ == DockPosition::Right) ?
        splitterPos_ : rect_.right - rect_.left;
}

int SidePanel::getHeight() const {
    if (state_ == PanelState::Collapsed) {
        return rect_.bottom - rect_.top;
    }
    return (position_ == DockPosition::Left || position_ == DockPosition::Right) ?
        rect_.bottom - rect_.top : splitterPos_;
}

// ============================================================================
// BottomPanel Implementation
// ============================================================================

BottomPanel::BottomPanel(const DockingConfig& config) : config_(config) {
    rect_ = {0, 0, 0, 0};
    currentHeight_ = config.defaultHeight;
}

void BottomPanel::addPanel(HWND hwnd, const std::wstring& title, const std::wstring& panelId) {
    BottomPanelInfo info = {hwnd, title, panelId, true, {0, 0, 0, 0}};
    panels_[panelId] = info;
    
    if (activePanelId_.empty()) {
        activePanelId_ = panelId;
    }
    
    recalculateLayout();
}

void BottomPanel::removePanel(const std::wstring& panelId) {
    panels_.erase(panelId);
    if (activePanelId_ == panelId) {
        activePanelId_ = panels_.empty() ? L"" : panels_.begin()->first;
    }
    recalculateLayout();
}

void BottomPanel::showPanel(const std::wstring& panelId) {
    auto it = panels_.find(panelId);
    if (it != panels_.end()) {
        it->second.visible = true;
        recalculateLayout();
    }
}

void BottomPanel::hidePanel(const std::wstring& panelId) {
    auto it = panels_.find(panelId);
    if (it != panels_.end()) {
        it->second.visible = false;
        recalculateLayout();
    }
}

void BottomPanel::maximizePanel(const std::wstring& panelId) {
    maximized_ = true;
    currentHeight_ = maximizedHeight_;
    activePanelId_ = panelId;
    recalculateLayout();
}

void BottomPanel::restorePanel() {
    maximized_ = false;
    currentHeight_ = config_.defaultHeight;
    recalculateLayout();
}

void BottomPanel::setRect(const RECT& rect) {
    rect_ = rect;
    recalculateLayout();
}

void BottomPanel::setHeight(int height) {
    currentHeight_ = std::max(config_.minHeight, std::min(height, maximizedHeight_));
    recalculateLayout();
}

void BottomPanel::recalculateLayout() {
    if (!visible_ || panels_.empty()) return;
    
    // Calculate panel area
    int panelTop = rect_.bottom - currentHeight_;
    RECT panelRect = {rect_.left, panelTop, rect_.right, rect_.bottom};
    
    // Position tab bar
    int tabHeight = 24;
    RECT tabBarRect = {panelRect.left, panelRect.top, panelRect.right, panelRect.top + tabHeight};
    
    // Position content area
    RECT contentRect = {panelRect.left, panelRect.top + tabHeight,
                         panelRect.right, panelRect.bottom};
    
    // Layout active panel
    auto it = panels_.find(activePanelId_);
    if (it != panels_.end() && it->second.visible) {
        SetWindowPos(it->second.hwnd, nullptr,
            contentRect.left, contentRect.top,
            contentRect.right - contentRect.left,
            contentRect.bottom - contentRect.top,
            SWP_NOZORDER | SWP_SHOWWINDOW);
        
        // Hide other panels
        for (auto& [id, panel] : panels_) {
            if (id != activePanelId_) {
                ShowWindow(panel.hwnd, SW_HIDE);
            }
        }
    }
    
    // Store rect for hit testing
    rect_ = panelRect;
}

void BottomPanel::draw(HDC hdc) {
    if (!visible_) return;
    
    // Draw background
    FillRect(hdc, &rect_, (HBRUSH)(COLOR_3DFACE + 1));
    
    // Draw resize handle at top
    HPEN hPen = CreatePen(PS_SOLID, 4, GetSysColor(COLOR_3DSHADOW));
    HPEN hOldPen = (HPEN)SelectObject(hdc, hPen);
    MoveToEx(hdc, rect_.left, rect_.top, nullptr);
    LineTo(hdc, rect_.right, rect_.top);
    SelectObject(hdc, hOldPen);
    DeleteObject(hPen);
    
    // Draw tab bar
    int tabHeight = 24;
    RECT tabBarRect = {rect_.left, rect_.top, rect_.right, rect_.top + tabHeight};
    FillRect(hdc, &tabBarRect, (HBRUSH)(COLOR_3DFACE + 1));
    
    // Draw tabs
    int x = rect_.left + 4;
    for (const auto& [id, panel] : panels_) {
        if (!panel.visible) continue;
        
        SIZE textSize;
        GetTextExtentPoint32(hdc, panel.title.c_str(), 
            static_cast<int>(panel.title.length()), 
            &textSize);
        int tabWidth = textSize.cx + 16;
        
        RECT tabRect = {x, rect_.top + 2, x + tabWidth, rect_.top + tabHeight - 2};
        
        if (id == activePanelId_) {
            FillRect(hdc, &tabRect, (HBRUSH)(COLOR_WINDOW + 1));
        }
        
        SetBkMode(hdc, TRANSPARENT);
        DrawText(hdc, panel.title.c_str(), -1, &tabRect,
                DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        
        x += tabWidth + 4;
    }
    
    // Draw maximize/restore button
    RECT btnRect = {rect_.right - 60, rect_.top + 2, rect_.right - 30, rect_.top + tabHeight - 2};
    const wchar_t* btnText = maximized_ ? L"🗗" : L"🗖";
    DrawText(hdc, btnText, 1, &btnRect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}

HWND BottomPanel::hitTest(int x, int y) const {
    if (!visible_) return nullptr;
    
    auto it = panels_.find(activePanelId_);
    if (it != panels_.end() && it->second.visible) {
        RECT rect;
        GetWindowRect(it->second.hwnd, &rect);
        MapWindowPoints(HWND_DESKTOP, GetParent(it->second.hwnd), (LPPOINT)&rect, 2);
        
        if (x >= rect.left && x < rect.right &&
            y >= rect.top && y < rect.bottom) {
            return it->second.hwnd;
        }
    }
    return nullptr;
}

bool BottomPanel::hitTestResizeHandle(int x, int y) const {
    if (!visible_) return false;
    return (y >= rect_.top - 4 && y <= rect_.top + 4);
}

// ============================================================================
// AdvancedDockingManager Implementation
// ============================================================================

AdvancedDockingManager::AdvancedDockingManager() = default;

AdvancedDockingManager::~AdvancedDockingManager() {
    shutdown();
}

bool AdvancedDockingManager::initialize(HWND hWndMain) {
    hWndMain_ = hWndMain;
    
    // Create default panels
    DockingConfig sideConfig;
    sideConfig.defaultWidth = 250;
    sideConfig.minWidth = 150;
    
    leftPanel_ = std::make_unique<SidePanel>(DockPosition::Left, sideConfig);
    rightPanel_ = std::make_unique<SidePanel>(DockPosition::Right, sideConfig);
    
    DockingConfig bottomConfig;
    bottomConfig.defaultHeight = 250;
    bottomConfig.minHeight = 100;
    bottomPanel_ = std::make_unique<BottomPanel>(bottomConfig);
    
    return true;
}

void AdvancedDockingManager::shutdown() {
    tabGroups_.clear();
    leftPanel_.reset();
    rightPanel_.reset();
    bottomPanel_.reset();
    hWndMain_ = nullptr;
}

void AdvancedDockingManager::setMainAreaRect(const RECT& rect) {
    mainAreaRect_ = rect;
    recalculateLayout();
}

void AdvancedDockingManager::recalculateLayout() {
    if (!hWndMain_) return;
    
    // Calculate available space
    RECT available = mainAreaRect_;
    
    // Reserve space for side panels
    int leftWidth = leftPanel_ ? leftPanel_->getWidth() : 0;
    int rightWidth = rightPanel_ ? rightPanel_->getWidth() : 0;
    int bottomHeight = bottomPanel_ && bottomPanel_->isVisible() ? 
        bottomPanel_->getHeight() : 0;
    
    // Position left panel
    if (leftPanel_) {
        RECT leftRect = {available.left, available.top,
                        available.left + leftWidth, available.bottom - bottomHeight};
        leftPanel_->setRect(leftRect);
    }
    
    // Position right panel
    if (rightPanel_) {
        RECT rightRect = {available.right - rightWidth, available.top,
                         available.right, available.bottom - bottomHeight};
        rightPanel_->setRect(rightRect);
    }
    
    // Position bottom panel
    if (bottomPanel_) {
        RECT bottomRect = {available.left + leftWidth, 
                          available.bottom - bottomHeight,
                          available.right - rightWidth, 
                          available.bottom};
        bottomPanel_->setRect(bottomRect);
    }
    
    // Position tab groups in main area
    RECT mainRect = {available.left + leftWidth, available.top,
                    available.right - rightWidth, available.bottom - bottomHeight};
    
    for (auto& [id, group] : tabGroups_) {
        group->setRect(mainRect);
    }
    
    notifyLayoutChanged();
}

void AdvancedDockingManager::draw(HDC hdc) {
    // Draw side panels
    if (leftPanel_) leftPanel_->draw(hdc);
    if (rightPanel_) rightPanel_->draw(hdc);
    if (bottomPanel_) bottomPanel_->draw(hdc);
    
    // Draw tab groups
    for (auto& [id, group] : tabGroups_) {
        group->draw(hdc);
    }
}

TabGroup* AdvancedDockingManager::createTabGroup(const std::wstring& groupId) {
    auto group = std::make_unique<TabGroup>(groupId);
    TabGroup* ptr = group.get();
    tabGroups_[groupId] = std::move(group);
    
    // Set initial rect
    if (!(mainAreaRect_.left == 0 && mainAreaRect_.right == 0)) {
        ptr->setRect(mainAreaRect_);
    }
    
    return ptr;
}

void AdvancedDockingManager::destroyTabGroup(const std::wstring& groupId) {
    tabGroups_.erase(groupId);
}

TabGroup* AdvancedDockingManager::getTabGroup(const std::wstring& groupId) {
    auto it = tabGroups_.find(groupId);
    if (it != tabGroups_.end()) {
        return it->second.get();
    }
    return nullptr;
}

void AdvancedDockingManager::moveTabBetweenGroups(HWND hwnd, 
                                                  const std::wstring& fromGroup,
                                                  const std::wstring& toGroup) {
    TabGroup* src = getTabGroup(fromGroup);
    TabGroup* dst = getTabGroup(toGroup);
    
    if (src && dst) {
        // Find tab info
        for (const auto& tab : src->getTabs()) {
            if (tab.hwnd == hwnd) {
                dst->addTab(tab);
                src->removeTab(hwnd);
                break;
            }
        }
    }
}

void AdvancedDockingManager::toggleLeftPanel() {
    if (leftPanel_) {
        leftPanel_->toggle();
        recalculateLayout();
    }
}

void AdvancedDockingManager::toggleRightPanel() {
    if (rightPanel_) {
        rightPanel_->toggle();
        recalculateLayout();
    }
}

void AdvancedDockingManager::setLeftPanelWidth(int width) {
    if (leftPanel_) {
        leftPanel_->setSplitterPos(width);
        recalculateLayout();
    }
}

void AdvancedDockingManager::setRightPanelWidth(int width) {
    if (rightPanel_) {
        rightPanel_->setSplitterPos(width);
        recalculateLayout();
    }
}

void AdvancedDockingManager::toggleBottomPanel() {
    if (bottomPanel_) {
        bottomPanel_->setVisible(!bottomPanel_->isVisible());
        recalculateLayout();
    }
}

void AdvancedDockingManager::setBottomPanelHeight(int height) {
    if (bottomPanel_) {
        bottomPanel_->setHeight(height);
        recalculateLayout();
    }
}

void AdvancedDockingManager::maximizeBottomPanel() {
    if (bottomPanel_) {
        bottomPanel_->maximizePanel(bottomPanel_->getActivePanel() ? 
            *bottomPanel_->getActivePanel() : L"");
        recalculateLayout();
    }
}

void AdvancedDockingManager::restoreBottomPanel() {
    if (bottomPanel_) {
        bottomPanel_->restorePanel();
        recalculateLayout();
    }
}

AdvancedDockingManager::HitTestInfo AdvancedDockingManager::hitTest(int x, int y) const {
    HitTestInfo info = {HitTestResult::None, nullptr, L"", -1, DockPosition::Center};
    
    // Check tab groups
    for (auto& [id, group] : tabGroups_) {
        HWND hwnd = group->hitTest(x, y);
        if (hwnd) {
            info.result = HitTestResult::Tab;
            info.hwnd = hwnd;
            info.panelId = id;
            return info;
        }
        
        int tabIndex = group->hitTestTab(x, y);
        if (tabIndex >= 0) {
            info.result = HitTestResult::Tab;
            info.tabIndex = tabIndex;
            info.panelId = id;
            return info;
        }
    }
    
    // Check side panels
    if (leftPanel_) {
        if (leftPanel_->hitTestSplitter(x, y)) {
            info.result = HitTestResult::SidePanelSplitter;
            info.panelPosition = DockPosition::Left;
            return info;
        }
        if (leftPanel_->hitTestCollapseButton(x, y)) {
            info.result = HitTestResult::CollapseButton;
            info.panelPosition = DockPosition::Left;
            return info;
        }
    }
    
    if (rightPanel_) {
        if (rightPanel_->hitTestSplitter(x, y)) {
            info.result = HitTestResult::SidePanelSplitter;
            info.panelPosition = DockPosition::Right;
            return info;
        }
        if (rightPanel_->hitTestCollapseButton(x, y)) {
            info.result = HitTestResult::CollapseButton;
            info.panelPosition = DockPosition::Right;
            return info;
        }
    }
    
    // Check bottom panel
    if (bottomPanel_) {
        if (bottomPanel_->hitTestResizeHandle(x, y)) {
            info.result = HitTestResult::BottomPanelResize;
            return info;
        }
    }
    
    return info;
}

void AdvancedDockingManager::beginSplitterDrag(DockPosition panel, int pos) {
    draggingSplitter_ = true;
    draggedPanel_ = panel;
    dragStartPos_ = pos;
    
    if (panel == DockPosition::Left && leftPanel_) {
        dragStartSplitterPos_ = leftPanel_->getSplitterPos();
    } else if (panel == DockPosition::Right && rightPanel_) {
        dragStartSplitterPos_ = rightPanel_->getSplitterPos();
    } else if (panel == DockPosition::Bottom && bottomPanel_) {
        dragStartSplitterPos_ = bottomPanel_->getHeight();
    }
}

void AdvancedDockingManager::updateSplitterDrag(int pos) {
    if (!draggingSplitter_) return;
    
    int delta = pos - dragStartPos_;
    int newPos = dragStartSplitterPos_ + delta;
    
    if (draggedPanel_ == DockPosition::Left && leftPanel_) {
        leftPanel_->setSplitterPos(newPos);
    } else if (draggedPanel_ == DockPosition::Right && rightPanel_) {
        rightPanel_->setSplitterPos(newPos);
    } else if (draggedPanel_ == DockPosition::Bottom && bottomPanel_) {
        bottomPanel_->setHeight(newPos);
    }
    
    recalculateLayout();
}

void AdvancedDockingManager::endSplitterDrag() {
    draggingSplitter_ = false;
}

std::wstring AdvancedDockingManager::serializeLayout() const {
    // Simple JSON-like serialization
    std::wstring json = L"{";
    
    // Serialize side panel widths
    if (leftPanel_) {
        json += L"\"leftWidth\":" + std::to_wstring(leftPanel_->getWidth()) + L",";
    }
    if (rightPanel_) {
        json += L"\"rightWidth\":" + std::to_wstring(rightPanel_->getWidth()) + L",";
    }
    if (bottomPanel_) {
        json += L"\"bottomHeight\":" + std::to_wstring(bottomPanel_->getHeight()) + L",";
        json += L"\"bottomVisible\":" + std::wstring(bottomPanel_->isVisible() ? L"true" : L"false") + L",";
    }
    
    // Remove trailing comma
    if (json.back() == L',') {
        json.pop_back();
    }
    
    json += L"}";
    return json;
}

bool AdvancedDockingManager::deserializeLayout(const std::wstring& layoutJson) {
    // Simple parsing - in real implementation use proper JSON parser
    // This is a stub for the concept
    return true;
}

void AdvancedDockingManager::saveLayoutToSettings() {
    std::wstring layout = serializeLayout();
    // Save to settings (implementation depends on settings system)
}

void AdvancedDockingManager::loadLayoutFromSettings() {
    // Load from settings (implementation depends on settings system)
}

void AdvancedDockingManager::notifyLayoutChanged() {
    if (layoutChangedCallback_) {
        layoutChangedCallback_();
    }
}

} // namespace UI
} // namespace RawrXD
