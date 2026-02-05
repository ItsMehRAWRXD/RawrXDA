#define NOMINMAX
#include "ui/split_layout.h"
#include <algorithm>

namespace RawrXD {
namespace UI {

SplitLayout::SplitLayout(HWND parent) : m_parent(parent) {}

void SplitLayout::setTopPanes(const std::vector<Pane>& panes) {
    m_top = panes;
}

void SplitLayout::setBottomPane(HWND hwnd) {
    m_bottom = hwnd;
    m_bottomLeft = nullptr;
    m_bottomRight = nullptr;
}

void SplitLayout::setBottomPanes(HWND leftPane, HWND rightPane, float leftRatio) {
    m_bottomLeft = leftPane;
    m_bottomRight = rightPane;
    m_bottomLeftRatio = leftRatio;
    m_bottom = nullptr; // disable legacy single-pane
}

void SplitLayout::setBottomHeight(int height) {
    if (height < 80) height = 80;
    m_bottomHeight = height;
}

void SplitLayout::onResize(int width, int height) {
    if (!m_parent) return;
    m_lastWidth = width;
    m_lastHeight = height;
    
    int pad = m_padding;
    int splitter = m_splitterSize;
    int topH = height - m_bottomHeight - splitter;
    if (topH < 100) topH = 100;

    // Layout top 3 columns (File Explorer | Editor | AI Chat)
    if (!m_top.empty()) {
        float totalRatio = 0.0f;
        for (auto& p : m_top) totalRatio += p.ratio;
        if (totalRatio <= 0.0f) totalRatio = 1.0f;

        int x = pad;
        int availW = width - 2*pad;
        for (size_t i = 0; i < m_top.size(); ++i) {
            int w = (int)((m_top[i].ratio / totalRatio) * availW);
            if (i + 1 == m_top.size()) {
                w = availW - (x - pad); // fill rest
            }
            if (m_top[i].hwnd) {
                MoveWindow(m_top[i].hwnd, x, pad, w - pad, topH - pad, TRUE);
            }
            x += w;
        }
    }

    // Bottom row starts after horizontal splitter
    int bottomY = topH + splitter;
    int bottomH = m_bottomHeight - pad;
    int bottomW = width - 2*pad;

    // Layout bottom: 2 panes (Terminal | User Chat) or legacy single pane
    if (m_bottomLeft && m_bottomRight) {
        int leftW = (int)(bottomW * m_bottomLeftRatio);
        int rightW = bottomW - leftW - splitter;
        
        MoveWindow(m_bottomLeft, pad, bottomY, leftW, bottomH, TRUE);
        MoveWindow(m_bottomRight, pad + leftW + splitter, bottomY, rightW, bottomH, TRUE);
    } else if (m_bottom) {
        MoveWindow(m_bottom, pad, bottomY, bottomW, bottomH, TRUE);
    }
}

RECT SplitLayout::getHorizontalSplitterRect() const {
    int topH = m_lastHeight - m_bottomHeight - m_splitterSize;
    if (topH < 100) topH = 100;
    RECT rc = { m_padding, topH, m_lastWidth - m_padding, topH + m_splitterSize };
    return rc;
}

bool SplitLayout::isOnHorizontalSplitter(int x, int y) const {
    RECT rc = getHorizontalSplitterRect();
    return (x >= rc.left && x <= rc.right && y >= rc.top && y <= rc.bottom);
}

bool SplitLayout::isOnVerticalSplitter(int x, int y, int& splitterIndex) const {
    // Check vertical splitters between top panes
    if (m_top.size() < 2) return false;
    
    int topH = m_lastHeight - m_bottomHeight - m_splitterSize;
    if (y < m_padding || y > topH) return false;
    
    float totalRatio = 0.0f;
    for (auto& p : m_top) totalRatio += p.ratio;
    if (totalRatio <= 0.0f) totalRatio = 1.0f;
    
    int px = m_padding;
    int availW = m_lastWidth - 2*m_padding;
    for (size_t i = 0; i < m_top.size() - 1; ++i) {
        int w = (int)((m_top[i].ratio / totalRatio) * availW);
        int splitterX = px + w - m_padding;
        if (x >= splitterX - 3 && x <= splitterX + 3) {
            splitterIndex = (int)i;
            return true;
        }
        px += w;
    }
    return false;
}

void SplitLayout::startDragHorizontal(int y) {
    m_dragging = true;
    m_draggingHorizontal = true;
    m_dragStartPos = y;
}

void SplitLayout::startDragVertical(int index, int x) {
    m_dragging = true;
    m_draggingHorizontal = false;
    m_dragSplitterIndex = index;
    m_dragStartPos = x;
}

void SplitLayout::updateDrag(int x, int y) {
    if (!m_dragging) return;
    
    if (m_draggingHorizontal) {
        int delta = y - m_dragStartPos;
        int newBottomHeight = m_bottomHeight - delta;
        if (newBottomHeight >= 80 && newBottomHeight <= m_lastHeight - 150) {
            m_bottomHeight = newBottomHeight;
            m_dragStartPos = y;
            onResize(m_lastWidth, m_lastHeight);
        }
    } else if (m_dragSplitterIndex >= 0 && m_dragSplitterIndex < (int)m_top.size() - 1) {
        // Adjust ratios for vertical splitter drag
        float totalRatio = 0.0f;
        for (auto& p : m_top) totalRatio += p.ratio;
        
        int availW = m_lastWidth - 2*m_padding;
        float pixelRatio = totalRatio / availW;
        int delta = x - m_dragStartPos;
        float ratioDelta = delta * pixelRatio;
        
        float minRatio = 0.1f;
        float newLeft = m_top[m_dragSplitterIndex].ratio + ratioDelta;
        float newRight = m_top[m_dragSplitterIndex + 1].ratio - ratioDelta;
        
        if (newLeft >= minRatio && newRight >= minRatio) {
            m_top[m_dragSplitterIndex].ratio = newLeft;
            m_top[m_dragSplitterIndex + 1].ratio = newRight;
            m_dragStartPos = x;
            onResize(m_lastWidth, m_lastHeight);
        }
    }
}

void SplitLayout::endDrag() {
    m_dragging = false;
    m_draggingHorizontal = false;
    m_dragSplitterIndex = -1;
}

} // namespace UI
} // namespace RawrXD
