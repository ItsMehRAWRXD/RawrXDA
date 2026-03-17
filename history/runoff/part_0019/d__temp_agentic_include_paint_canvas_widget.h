#pragma once
/*  canvas_widget.h - Qt Canvas Widget for painting
    Renders ig::Canvas to Qt screen, handles mouse/brush events
*/

#include <QWidget>
#include <QMouseEvent>
#include <memory>
#include "primitives.h"
#include "drawing.h"

namespace ig {

enum class DrawTool;

class CanvasWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit CanvasWidget(int width, int height, QWidget* parent = nullptr);
    
    void setDrawTool(DrawTool tool);
    void setBrushSize(int size);
    void setForegroundColor(const Color& color);
    void setBackgroundColor(const Color& color);
    void setOpacity(float opacity);
    
    void clear(const Color& color);
    void clearAll();
    void undo();
    void redo();
    
    Canvas& getCanvas();
    const Canvas& getCanvas() const;
    
    void exportPNG(const std::string& path);
    void exportBMP(const std::string& path);
    
signals:
    void canvasChanged();
    
protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    
private:
    void updateBuffer();
    
    std::unique_ptr<Canvas> m_canvas;
    std::vector<Canvas> m_undoStack;
    std::vector<Canvas> m_redoStack;
    
    DrawTool m_currentTool = DrawTool::Pencil;
    Color m_foregroundColor = Color::rgb(0, 0, 0);
    Color m_backgroundColor = Color::rgb(1, 1, 1);
    int m_brushSize = 5;
    float m_opacity = 1.0f;
    
    // Drawing state
    bool m_isDrawing = false;
    float m_lastX = 0, m_lastY = 0;
    std::vector<std::pair<float,float>> m_polygonPoints;
};

} // namespace ig
