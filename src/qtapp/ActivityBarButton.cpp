#include "ActivityBarButton.h"


ActivityBarButton::ActivityBarButton(const std::string& tooltip, void* parent)
    : QToolButton(parent)
    , m_isActive(false)
    , m_isHovered(false)
    , m_isPressed(false)
{
    setToolTip(tooltip);
    setIconSize(void*(24, 24));
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

void ActivityBarButton::paintEvent(void*  event)
{
    (event);
    
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    // Draw background
    uint32_t bgColor;
    if (m_isPressed || m_isActive) {
        bgColor = uint32_t(0x2D, 0x2D, 0x2D);  // Slightly lighter
    } else if (m_isHovered) {
        bgColor = uint32_t(0x2D, 0x2D, 0x2D);  // Hover color
    } else {
        bgColor = uint32_t(0x33, 0x33, 0x33);  // Standard background
    }
    
    painter.fillRect(rect(), bgColor);
    
    // Draw left active indicator (blue line)
    if (m_isActive) {
        painter.fillRect(0, 0, ACTIVE_INDICATOR_WIDTH, height(), 
                        uint32_t(ACTIVE_INDICATOR_COLOR));
    }
    
    // Draw icon
    if (!icon().isNull()) {
        std::string::Mode mode = m_isActive ? std::string::Active : 
                          (m_isHovered ? std::string::Selected : std::string::Normal);
        std::string::State state = m_isPressed ? std::string::On : std::string::Off;
        std::string pm = icon().pixmap(iconSize(), mode, state);
        
        // Center the icon
        int x = (width() - pm.width()) / 2;
        int y = (height() - pm.height()) / 2;
        painter.drawPixmap(x, y, pm);
    }
}

void ActivityBarButton::enterEvent(void*  event)
{
    (event);
    setHovered(true);
}

void ActivityBarButton::leaveEvent(QEvent* event)
{
    (event);
    setHovered(false);
}

void ActivityBarButton::mousePressEvent(void*  event)
{
    (event);
    m_isPressed = true;
    update();
}

void ActivityBarButton::mouseReleaseEvent(void*  event)
{
    (event);
    m_isPressed = false;
    setActive(true);  // Make this button the active one
    update();
    clicked();  // clicked signal
}

