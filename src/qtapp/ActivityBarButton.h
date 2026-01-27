#pragma once
/*
 * ActivityBarButton.h - Individual button for the Activity Bar
 * 
 * Custom button with hover/active state rendering to match VS Code style
 */

#include <QToolButton>
#include <QString>

class ActivityBarButton : public QToolButton
{
    Q_OBJECT

public:
    explicit ActivityBarButton(const QString& tooltip = "", QWidget* parent = nullptr);
    ~ActivityBarButton();

    /**
     * \brief Set whether this button is the active/selected button
     * \param active True if this button should appear active
     */
    void setActive(bool active);
    bool isActive() const { return m_isActive; }

    /**
     * \brief Set whether the button is being hovered
     * \param hovered True if button is hovered
     */
    void setHovered(bool hovered);
    bool isHovered() const { return m_isHovered; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    bool m_isActive;
    bool m_isHovered;
    bool m_isPressed;

    // VS Code color scheme
    static constexpr QRgb BACKGROUND_COLOR = 0x333333;  // RGB(51,51,51)
    static constexpr QRgb HOVER_BACKGROUND_COLOR = 0x2D2D2D;
    static constexpr QRgb ACTIVE_INDICATOR_COLOR = 0x007ACC;  // RGB(0,122,204)
    static constexpr QRgb ICON_COLOR = 0xCCCCCC;  // Light gray
    static constexpr QRgb ICON_ACTIVE_COLOR = 0xFFFFFF;  // White
    static constexpr int ACTIVE_INDICATOR_WIDTH = 3;
    static constexpr int BUTTON_SIZE = 48;
};
