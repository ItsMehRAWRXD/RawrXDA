/**
 * @file whiteboard_widget.h
 * @brief Header for WhiteboardWidget - Interactive drawing canvas with tools
 */

#pragma once

#include <QWidget>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QColor>
#include <QPixmap>
#include <QStack>

class QVBoxLayout;
class QHBoxLayout;
class QToolBar;
class QPushButton;
class QComboBox;
class QSpinBox;
class QColorDialog;
class QGraphicsPathItem;

class WhiteboardScene : public QGraphicsScene {
    Q_OBJECT
public:
    explicit WhiteboardScene(QObject* parent = nullptr);
    
    void setDrawingMode(const QString& mode);
    void setColor(const QColor& color);
    void setPenWidth(int width);
    void clearBoard();
    void saveBoardToImage(const QString& filename);
    QPixmap getCurrentImage() const;
    
signals:
    void boardModified();
    void drawingModeChanged(const QString& mode);
    
protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    
private:
    QString mCurrentMode;  // "pen", "eraser", "rectangle", "circle", "line"
    QColor mCurrentColor;
    int mPenWidth;
    QPointF mLastPoint;
    bool mIsDrawing;
};

class WhiteboardWidget : public QWidget {
    Q_OBJECT
    
public:
    explicit WhiteboardWidget(QWidget* parent = nullptr);
    ~WhiteboardWidget();
    
public slots:
    void onPenTool();
    void onEraserTool();
    void onRectangleTool();
    void onCircleTool();
    void onLineTool();
    void onColorSelection();
    void onPenWidthChanged(int width);
    void onClearBoard();
    void onSaveBoard();
    void onLoadBoard();
    void onUndo();
    void onRedo();
    
signals:
    void boardUpdated(const QPixmap& image);
    void toolChanged(const QString& tool);
    
private:
    void setupUI();
    void createToolbar();
    void connectSignals();
    void restoreState();
    void saveState();
    
    // UI Components
    QVBoxLayout* mMainLayout;
    QHBoxLayout* mToolbarLayout;
    QToolBar* mToolbar;
    QGraphicsView* mGraphicsView;
    WhiteboardScene* mScene;
    
    // Tool buttons
    QPushButton* mPenButton;
    QPushButton* mEraserButton;
    QPushButton* mRectButton;
    QPushButton* mCircleButton;
    QPushButton* mLineButton;
    QPushButton* mColorButton;
    QPushButton* mClearButton;
    QPushButton* mSaveButton;
    QPushButton* mLoadButton;
    QPushButton* mUndoButton;
    QPushButton* mRedoButton;
    
    QSpinBox* mPenWidthSpinBox;
    QColor mCurrentColor;
    
    // Undo/Redo stacks
    QStack<QPixmap> mUndoStack;
    QStack<QPixmap> mRedoStack;
};

