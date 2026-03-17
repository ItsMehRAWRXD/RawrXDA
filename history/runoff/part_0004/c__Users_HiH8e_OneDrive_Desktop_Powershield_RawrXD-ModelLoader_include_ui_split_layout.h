#pragma once
#include <windows.h>
#include <vector>

namespace RawrXD {
namespace UI {

enum class Orientation { Horizontal, Vertical };

struct Pane {
    HWND hwnd = nullptr;
    float ratio = 0.33f; // relative size
};

/*
 * IDE Layout (requested):
 * ┌─────────────────┬──────────────────┬─────────────────┐
 * │ FILE EXPLORER   │   Code Editor    │   AI Chat       │
 * │                 │                  │   Transcript    │
 * │                 │                  │                 │
 * ├─────────────────┴──────────────────┼─────────────────┤
 * │ TERMINAL/PWSH                      │ User Chat Input │
 * └────────────────────────────────────┴─────────────────┘
 */
class SplitLayout {
public:
    explicit SplitLayout(HWND parent);
    
    // Top row: 3 panes (file browser, editor, AI chat)
    void setTopPanes(const std::vector<Pane>& panes);
    
    // Bottom row: 2 panes (terminal left, user chat right)
    void setBottomPane(HWND hwnd); // legacy single-pane bottom
    void setBottomPanes(HWND leftPane, HWND rightPane, float leftRatio = 0.6f);
    
    void setBottomHeight(int height); // pixels
    void onResize(int width, int height);
    
    // Splitter drag support
    bool isOnHorizontalSplitter(int x, int y) const;
    bool isOnVerticalSplitter(int x, int y, int& splitterIndex) const;
    void startDragHorizontal(int y);
    void startDragVertical(int index, int x);
    void updateDrag(int x, int y);
    void endDrag();
    bool isDragging() const { return m_dragging; }
    
    // Get splitter rects for hit testing
    RECT getHorizontalSplitterRect() const;

private:
    HWND m_parent;
    std::vector<Pane> m_top;
    
    // Bottom panes: left (terminal) and right (user chat)
    HWND m_bottomLeft = nullptr;
    HWND m_bottomRight = nullptr;
    float m_bottomLeftRatio = 0.6f;
    
    // Legacy single bottom pane (if setBottomPane used)
    HWND m_bottom = nullptr;
    
    int m_bottomHeight = 200;
    int m_padding = 4;
    int m_splitterSize = 6;
    
    // Drag state
    bool m_dragging = false;
    bool m_draggingHorizontal = false;
    int m_dragSplitterIndex = -1;
    int m_dragStartPos = 0;
    
    // Cached layout dimensions
    int m_lastWidth = 0;
    int m_lastHeight = 0;
};

} // namespace UI
} // namespace RawrXD
