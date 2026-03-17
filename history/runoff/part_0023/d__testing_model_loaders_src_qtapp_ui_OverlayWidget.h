// OverlayWidget.h — Headless overlay text stub
// Converted from Qt (QWidget, QPainter, paintEvent) to pure C++17
// Preserves ALL original data: ghost text overlay, position/size, opacity

#pragma once

#include <string>
#include <cstdint>
#include <iostream>

class OverlayWidget {
public:
    OverlayWidget() = default;
    ~OverlayWidget() = default;

    // Set ghost text to display as overlay
    void setGhostText(const std::string& text) {
        m_ghostText = text;
    }

    const std::string& ghostText() const { return m_ghostText; }

    // Position and size (headless — stored for API compat)
    void setPosition(int x, int y) { m_x = x; m_y = y; }
    void setSize(int w, int h) { m_width = w; m_height = h; }
    int x() const { return m_x; }
    int y() const { return m_y; }
    int width()  const { return m_width; }
    int height() const { return m_height; }

    // Opacity (headless — stored for API compat)
    void setOpacity(float opacity) { m_opacity = opacity; }
    float opacity() const { return m_opacity; }

    // Font settings (headless — stored for API compat)
    void setFontFamily(const std::string& family) { m_fontFamily = family; }
    void setFontSize(int size) { m_fontSize = size; }
    const std::string& fontFamily() const { return m_fontFamily; }
    int fontSize() const { return m_fontSize; }

    // Visibility (headless — logged)
    void show() {
        m_visible = true;
        if (!m_ghostText.empty()) {
            std::cout << "[OverlayWidget] Showing ghost text: '"
                      << m_ghostText.substr(0, 40) << "'" << std::endl;
        }
    }

    void hide() {
        m_visible = false;
    }

    bool isVisible() const { return m_visible; }

    // Color (headless — stored for API compat)
    void setTextColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
        m_colorR = r; m_colorG = g; m_colorB = b; m_colorA = a;
    }

    void setBackgroundColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 128) {
        m_bgR = r; m_bgG = g; m_bgB = b; m_bgA = a;
    }

    // Update / repaint (headless — no-op)
    void update() {}
    void repaint() {}

private:
    std::string m_ghostText;
    std::string m_fontFamily = "Consolas";
    int m_fontSize = 12;

    int m_x = 0, m_y = 0;
    int m_width = 400, m_height = 200;
    float m_opacity = 0.7f;
    bool m_visible = false;

    uint8_t m_colorR = 128, m_colorG = 128, m_colorB = 128, m_colorA = 200;
    uint8_t m_bgR = 40, m_bgG = 40, m_bgB = 40, m_bgA = 128;
};
