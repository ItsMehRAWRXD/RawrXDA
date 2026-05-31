#pragma once
#include <windows.h>
#include <vector>
#include <string>

namespace RawrXD {
namespace IDE {

struct AttentionPoint {
    int line;
    int column;
    float weight; // 0.0 to 1.0
};

class NeuralHeatmapRenderer {
public:
    NeuralHeatmapRenderer(HWND targetWnd);
    ~NeuralHeatmapRenderer();

    // Update heatmap with new attention weights from local model
    void updateAttentionData(const std::vector<AttentionPoint>& points);

    // Render overlay using DirectWrite/GDI+
    void render(HDC hdc, const RECT& clientRect);

    // Toggle visibility
    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }

private:
    HWND m_hWnd;
    std::vector<AttentionPoint> m_attentionPoints;
    bool m_visible = false;
    
    // Internal GDI+ state would be here
    void drawHeatPoint(HDC hdc, const AttentionPoint& pt);
};

} // namespace IDE
} // namespace RawrXD
