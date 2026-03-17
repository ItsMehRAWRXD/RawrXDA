#pragma once

#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace RawrXD {
namespace UI {

struct Pane {
    HWND hwnd{nullptr};
    float ratio{1.0f};
};

class SplitLayout {
public:
    explicit SplitLayout(HWND parent);

    void setTopPanes(const std::vector<Pane>& panes);
    void setBottomPane(HWND hwnd);
    void setBottomPanes(HWND leftPane, HWND rightPane, float leftRatio);
    void setBottomHeight(int height);

    void onResize(int width, int height);
    RECT getHorizontalSplitterRect() const;
    bool isOnHorizontalSplitter(int x, int y) const;
    bool isOnVerticalSplitter(int x, int y, int& splitterIndex) const;

    void startDragHorizontal(int y);
    void startDragVertical(int index, int x);
    void updateDrag(int x, int y);
    void endDrag();

private:
    HWND m_parent{nullptr};
    std::vector<Pane> m_top;
    HWND m_bottom{nullptr};
    HWND m_bottomLeft{nullptr};
    HWND m_bottomRight{nullptr};
    float m_bottomLeftRatio{0.7f};

    int m_bottomHeight{220};
    int m_splitterSize{6};
    int m_padding{6};

    int m_lastWidth{0};
    int m_lastHeight{0};

    bool m_dragging{false};
    bool m_draggingHorizontal{false};
    int m_dragStartPos{0};
    int m_dragSplitterIndex{-1};
};

} // namespace UI
} // namespace RawrXD
