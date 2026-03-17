#include "cursor_widget.h"
#include <QPainter>
#include <QPaintEvent>

CursorWidget::CursorWidget(QWidget *parent)
    : QWidget(parent)
{
    // Set a minimum size for the widget
    setMinimumSize(200, 200);
}

void CursorWidget::updateCursor(const QString &userId, const CursorInfo &info)
{
    m_cursors[userId] = info;
    update(); // Trigger a repaint
}

void CursorWidget::removeCursor(const QString &userId)
{
    m_cursors.remove(userId);
    update(); // Trigger a repaint
}

void CursorWidget::paintEvent(QPaintEvent *event)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // Draw a simple representation of the text editor area
    painter.setPen(Qt::lightGray);
    painter.drawRect(rect().adjusted(0, 0, -1, -1));

    // Draw each cursor
    for (auto it = m_cursors.constBegin(); it != m_cursors.constEnd(); ++it) {
        const CursorInfo &info = it.value();
        painter.setPen(info.color);
        // Draw a simple line for the cursor
        // In a real implementation, this would be positioned based on the text layout
        int cursorX = 10 + (info.position % 50) * 4; // Simple positioning for demo
        int cursorY = 10 + (info.position / 50) * 20;
        painter.drawLine(cursorX, cursorY, cursorX, cursorY + 15);
        // Draw the user name
        painter.drawText(cursorX + 5, cursorY + 10, info.userName);
    }
}