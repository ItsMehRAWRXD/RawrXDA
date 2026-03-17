#include "paint/paint_app.h"
#include "paint/canvas_widget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QFileDialog>
#include <QColorDialog>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QComboBox>
#include <QPushButton>

namespace ig {

PaintApp::PaintApp(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Paint - Image Editor");
    setWindowIcon(QIcon());
    
    setupUI();
    createMenuBar();
    createToolbar();
    createCanvas(1024, 768);
    
    resize(1280, 900);
}

void PaintApp::setupUI() {
    QWidget* centralWidget = new QWidget(this);
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // Tool palette on the left
    QHBoxLayout* topLayout = new QHBoxLayout();
    
    // Tool selector
    QLabel* toolLabel = new QLabel("Tool:");
    m_toolCombo = new QComboBox();
    m_toolCombo->addItem("Pencil", static_cast<int>(DrawTool::Pencil));
    m_toolCombo->addItem("Brush", static_cast<int>(DrawTool::Brush));
    m_toolCombo->addItem("Eraser", static_cast<int>(DrawTool::Eraser));
    m_toolCombo->addItem("Line", static_cast<int>(DrawTool::Line));
    m_toolCombo->addItem("Rectangle", static_cast<int>(DrawTool::Rectangle));
    m_toolCombo->addItem("Circle", static_cast<int>(DrawTool::Circle));
    m_toolCombo->addItem("Fill Bucket", static_cast<int>(DrawTool::Fill));
    m_toolCombo->addItem("Color Picker", static_cast<int>(DrawTool::Picker));
    
    connect(m_toolCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &PaintApp::onToolSelected);
    
    // Brush size
    QLabel* sizeLabel = new QLabel("Size:");
    m_sizeSlider = new QSlider(Qt::Horizontal);
    m_sizeSlider->setRange(1, 50);
    m_sizeSlider->setValue(5);
    m_sizeSlider->setMaximumWidth(150);
    
    m_sizeLabel = new QLabel("5");
    m_sizeLabel->setMaximumWidth(30);
    
    connect(m_sizeSlider, QOverload<int>::of(&QSlider::valueChanged),
            this, &PaintApp::onBrushSizeChanged);
    
    // Color picker
    m_colorButton = new QPushButton("Foreground Color");
    connect(m_colorButton, &QPushButton::clicked, this, &PaintApp::onColorPicked);
    
    // Opacity
    QLabel* opacityLabel = new QLabel("Opacity:");
    m_opacitySpinBox = new QSpinBox();
    m_opacitySpinBox->setRange(0, 100);
    m_opacitySpinBox->setValue(100);
    m_opacitySpinBox->setSuffix("%");
    m_opacitySpinBox->setMaximumWidth(80);
    
    topLayout->addWidget(toolLabel);
    topLayout->addWidget(m_toolCombo);
    topLayout->addSpacing(20);
    topLayout->addWidget(sizeLabel);
    topLayout->addWidget(m_sizeSlider);
    topLayout->addWidget(m_sizeLabel);
    topLayout->addSpacing(20);
    topLayout->addWidget(m_colorButton);
    topLayout->addSpacing(20);
    topLayout->addWidget(opacityLabel);
    topLayout->addWidget(m_opacitySpinBox);
    topLayout->addStretch();
    
    mainLayout->addLayout(topLayout);
    mainLayout->addSpacing(10);
    
    setCentralWidget(centralWidget);
}

void PaintApp::createMenuBar() {
    QMenuBar* menuBar = new QMenuBar(this);
    setMenuBar(menuBar);
    
    // File menu
    QMenu* fileMenu = menuBar->addMenu("&File");
    
    QAction* newAction = fileMenu->addAction("&New");
    connect(newAction, &QAction::triggered, this, &PaintApp::onNew);
    
    QAction* openAction = fileMenu->addAction("&Open");
    connect(openAction, &QAction::triggered, this, &PaintApp::onOpen);
    
    QAction* saveAction = fileMenu->addAction("&Save");
    connect(saveAction, &QAction::triggered, this, &PaintApp::onSave);
    
    QAction* saveAsAction = fileMenu->addAction("Save &As...");
    connect(saveAsAction, &QAction::triggered, this, &PaintApp::onSaveAs);
    
    fileMenu->addSeparator();
    
    QAction* exportPngAction = fileMenu->addAction("Export as PN&G");
    connect(exportPngAction, &QAction::triggered, this, &PaintApp::onExportPNG);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = fileMenu->addAction("E&xit");
    connect(exitAction, &QAction::triggered, this, &QWidget::close);
    
    // Edit menu
    QMenu* editMenu = menuBar->addMenu("&Edit");
    
    QAction* undoAction = editMenu->addAction("&Undo");
    undoAction->setShortcut(QKeySequence::Undo);
    connect(undoAction, &QAction::triggered, this, &PaintApp::onUndo);
    
    QAction* redoAction = editMenu->addAction("&Redo");
    redoAction->setShortcut(QKeySequence::Redo);
    connect(redoAction, &QAction::triggered, this, &PaintApp::onRedo);
    
    editMenu->addSeparator();
    
    QAction* clearAction = editMenu->addAction("&Clear");
    connect(clearAction, &QAction::triggered, this, &PaintApp::onClearAll);
}

void PaintApp::createToolbar() {
    // Optional: add toolbar if needed
}

void PaintApp::createCanvas(int width, int height) {
    m_canvasWidget = new CanvasWidget(width, height, this);
    
    QWidget* centralWidget = this->centralWidget();
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(centralWidget->layout());
    if (layout) {
        layout->addWidget(m_canvasWidget, 1);
    }
}

void PaintApp::onToolSelected(int index) {
    m_currentTool = static_cast<DrawTool>(m_toolCombo->itemData(index).toInt());
    m_canvasWidget->setDrawTool(m_currentTool);
}

void PaintApp::onBrushSizeChanged(int size) {
    m_brushSize = size;
    m_sizeLabel->setText(QString::number(size));
    m_canvasWidget->setBrushSize(size);
}

void PaintApp::onColorPicked() {
    QColor qcolor = QColorDialog::getColor(Qt::black, this, "Select Foreground Color");
    if (qcolor.isValid()) {
        m_foregroundColor = Color::rgba(
            qcolor.redF(),
            qcolor.greenF(),
            qcolor.blueF(),
            qcolor.alphaF()
        );
        m_canvasWidget->setForegroundColor(m_foregroundColor);
        
        // Update button color
        m_colorButton->setStyleSheet(
            QString("background-color: %1; color: white;").arg(qcolor.name())
        );
    }
}

void PaintApp::onNew() {
    // Create new canvas
    QWidget* centralWidget = this->centralWidget();
    QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(centralWidget->layout());
    if (layout && m_canvasWidget) {
        layout->removeWidget(m_canvasWidget);
        m_canvasWidget->deleteLater();
    }
    createCanvas(1024, 768);
}

void PaintApp::onOpen() {
    QString filePath = QFileDialog::getOpenFileName(this,
        "Open Image", "", "PNG Images (*.png);;BMP Images (*.bmp);;All Files (*)");
    
    if (!filePath.isEmpty()) {
        m_currentFilePath = filePath.toStdString();
        // TODO: Implement image loading
    }
}

void PaintApp::onSave() {
    if (m_currentFilePath.empty()) {
        onSaveAs();
    } else {
        m_canvasWidget->exportPNG(m_currentFilePath);
    }
}

void PaintApp::onSaveAs() {
    QString filePath = QFileDialog::getSaveFileName(this,
        "Save Image", "", "PNG Images (*.png);;BMP Images (*.bmp)");
    
    if (!filePath.isEmpty()) {
        m_currentFilePath = filePath.toStdString();
        if (filePath.endsWith(".png")) {
            m_canvasWidget->exportPNG(m_currentFilePath);
        } else {
            m_canvasWidget->exportBMP(m_currentFilePath);
        }
    }
}

void PaintApp::onUndo() {
    m_canvasWidget->undo();
}

void PaintApp::onRedo() {
    m_canvasWidget->redo();
}

void PaintApp::onClear() {
    m_canvasWidget->clear(m_backgroundColor);
}

void PaintApp::onClearAll() {
    m_canvasWidget->clearAll();
}

void PaintApp::onExportPNG() {
    QString filePath = QFileDialog::getSaveFileName(this,
        "Export as PNG", "", "PNG Images (*.png)");
    
    if (!filePath.isEmpty()) {
        m_canvasWidget->exportPNG(filePath.toStdString());
    }
}

} // namespace ig
