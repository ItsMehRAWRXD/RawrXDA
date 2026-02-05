#include "ActivityBarButton.h"
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionButton>
#include <QMouseEvent>

ActivityBarButton::ActivityBarButton(const QString& tooltip, QWidget* parent)
    : QToolButton(parent)
    , m_isActive(false)
    , m_isHovered(false)
    , m_isPressed(false)
{
    setToolTip(tooltip);
    setIconSize(QSize(24, 24));
    setFixedSize(BUTTON_SIZE, BUTTON_SIZE);
    setStyleSheet("QToolButton { background-color: transparent; border: none; }");
}

ActivityBarButton::~ActivityBarButton()
{
}

void ActivityBarButton::setActive(bool active)
{
    if (m_isActive != active) {
        m_isActive = active;
        update();
    }
}

void ActivityBarButton::setHovered(bool hovered)
{
    if (m_isHovered != hovered) {
        m_isHovered = hovered;
        update();
    }
}

void ActivityBarButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    QColor bgColor;
    if (m_isPressed || m_isActive) {
        bgColor = QColor(0x2D, 0x2D, 0x2D);  // Slightly lighter
    } else if (m_isHovered) {
        bgColor = QColor(0x2D, 0x2D, 0x2D);  // Hover color
    } else {
        bgColor = QColor(0x33, 0x33, 0x33);  // Standard background
    }
    
    painter.fillRect(rect(), bgColor);
    
    // Draw left active indicator (blue line)
    if (m_isActive) {
        painter.fillRect(0, 0, ACTIVE_INDICATOR_WIDTH, height(), 
                        QColor(ACTIVE_INDICATOR_COLOR));
    }
    
    // Draw icon
    if (!icon().isNull()) {
        QIcon::Mode mode = m_isActive ? QIcon::Active : 
                          (m_isHovered ? QIcon::Selected : QIcon::Normal);
        QIcon::State state = m_isPressed ? QIcon::On : QIcon::Off;
        QPixmap pm = icon().pixmap(iconSize(), mode, state);
        
        // Center the icon
        int x = (width() - pm.width()) / 2;
        int y = (height() - pm.height()) / 2;
        painter.drawPixmap(x, y, pm);
    }
}

void ActivityBarButton::enterEvent(QEnterEvent* event)
{
    Q_UNUSED(event);
    setHovered(true);
}

void ActivityBarButton::leaveEvent(QEvent* event)
{
    Q_UNUSED(event);
    setHovered(false);
}

void ActivityBarButton::mousePressEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    m_isPressed = true;
    update();
}

void ActivityBarButton::mouseReleaseEvent(QMouseEvent* event)
{
    Q_UNUSED(event);
    m_isPressed = false;
    setActive(true);  // Make this button the active one
    update();
    clicked();  // Emit clicked signal
}
