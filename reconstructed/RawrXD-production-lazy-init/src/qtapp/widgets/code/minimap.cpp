/**
 * @file code_minimap.cpp
 * @brief Implementation of CodeMinimap - Code minimap visualization
 */

#include "code_minimap.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QMouseEvent>

CodeMinimap::CodeMinimap(QWidget* parent)
    : QWidget(parent), mCurrentLine(0)
{
    setupUI();
    setWindowTitle("Code Minimap");
    setMinimumWidth(100);
}

CodeMinimap::~CodeMinimap() = default;

void CodeMinimap::setupUI()
{
    mMainLayout = new QVBoxLayout(this);
    mLabel = new QLabel("Minimap", this);
    mMainLayout->addWidget(mLabel);
    setStyleSheet("background-color: #f5f5f5; border: 1px solid #ccc;");
}

void CodeMinimap::onCodeUpdated(const QString& code)
{
    mCodeContent = code;
    update();
}

void CodeMinimap::onPositionChanged(int line)
{
    mCurrentLine = line;
    update();
}

void CodeMinimap::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw minimap representation
    int lineCount = mCodeContent.count('\n') + 1;
    if (lineCount > 0) {
        int lineHeight = height() / lineCount;
        if (lineHeight < 1) lineHeight = 1;
        
        // Draw all lines as gray rectangles
        painter.fillRect(0, 0, width(), height(), QColor(240, 240, 240));
        
        // Draw current line indicator
        painter.fillRect(0, mCurrentLine * lineHeight, width(), lineHeight, QColor(100, 150, 255));
    }
}

void CodeMinimap::mousePressEvent(QMouseEvent* event)
{
    if (mCurrentLine >= 0) {
        int lineCount = mCodeContent.count('\n') + 1;
        if (lineCount > 0) {
            int lineHeight = height() / lineCount;
            if (lineHeight > 0) {
                int newLine = event->y() / lineHeight;
                emit lineSelected(newLine);
            }
        }
    }
}

void CodeMinimap::mouseMoveEvent(QMouseEvent* event)
{
    mousePressEvent(event);
}
