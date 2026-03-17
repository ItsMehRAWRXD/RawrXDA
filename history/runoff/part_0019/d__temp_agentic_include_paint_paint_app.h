#pragma once
/*  paint_app.h - Standalone Paint Application (like MS Paint)
    Qt6-based paint application with drawing tools, layers, and file I/O
*/

#include <QMainWindow>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>
#include <QColorDialog>
#include <memory>
#include "primitives.h"
#include "drawing.h"

namespace ig {

class CanvasWidget;

enum class DrawTool {
    Pencil,
    Brush,
    Eraser,
    Line,
    Rectangle,
    Circle,
    Polygon,
    Fill,
    Picker
};

class PaintApp : public QMainWindow {
    Q_OBJECT
    
public:
    explicit PaintApp(QWidget* parent = nullptr);
    ~PaintApp() = default;
    
private slots:
    void onToolSelected(int index);
    void onBrushSizeChanged(int size);
    void onColorPicked();
    void onNew();
    void onOpen();
    void onSave();
    void onSaveAs();
    void onUndo();
    void onRedo();
    void onClear();
    void onClearAll();
    void onExportPNG();
    
private:
    void setupUI();
    void createMenuBar();
    void createToolbar();
    void createCanvas(int width, int height);
    
    CanvasWidget* m_canvasWidget;
    
    // Tool settings
    QComboBox* m_toolCombo;
    QSlider* m_sizeSlider;
    QLabel* m_sizeLabel;
    QPushButton* m_colorButton;
    QSpinBox* m_opacitySpinBox;
    
    // Tool state
    DrawTool m_currentTool = DrawTool::Pencil;
    Color m_foregroundColor = Color::rgb(0, 0, 0);
    Color m_backgroundColor = Color::rgb(1, 1, 1);
    int m_brushSize = 5;
    float m_opacity = 1.0f;
    
    // File management
    std::string m_currentFilePath;
};

} // namespace ig
