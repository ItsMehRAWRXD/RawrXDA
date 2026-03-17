#pragma once
/*
 * ActivityBarButton.h - Individual button for the Activity Bar (Stub)
 * Qt dependencies removed
 */

#include "../QtGUIStubs.hpp"
#include <string>

class ActivityBarButton : public QWidget {
public:
    explicit ActivityBarButton(const std::string& tooltip = "", void* parent = nullptr) : QWidget() {}
    ~ActivityBarButton() override = default;

    void setActive(bool active) { m_isActive = active; }
    bool isActive() const { return m_isActive; }

    void setHovered(bool hovered) { m_isHovered = hovered; }
    bool isHovered() const { return m_isHovered; }

protected:
    void paintEvent(void* event) override {}
    void enterEvent(void* event) override { m_isHovered = true; }
    void leaveEvent(void* event) override { m_isHovered = false; }
    void mousePressEvent(void* event) override { m_isPressed = true; }
    void mouseReleaseEvent(void* event) override { m_isPressed = false; }

private:
    bool m_isActive = false;
    bool m_isHovered = false;
    bool m_isPressed = false;

    static constexpr int BUTTON_SIZE = 48;
};



