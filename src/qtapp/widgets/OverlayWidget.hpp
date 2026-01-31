#pragma once


/**
 * @brief Production-Grade Ghost Text Overlay Widget for AI Code Assistance
 * 
 * Displays AI-generated code suggestions as semi-transparent overlay text
 * positioned over the code editor. Supports:
 * - Transparent rendering with alpha blending
 * - Auto-sync with parent editor size/position
 * - Customizable ghost text styling
 * - Fade-in/fade-out animations
 * - Keyboard-aware dismissal
 * 
 * Usage:
 *   OverlayWidget* overlay = new OverlayWidget(codeEditor);
 *   overlay->setGhostText("// AI suggestion: use const here");
 *   
 * Production Features:
 * - Mouse-transparent (doesn't block editor interaction)
 * - Minimal performance overhead
 * - Respects editor theme colors
 * - Structured logging for debugging
 */
class OverlayWidget : public void {

public:
    explicit OverlayWidget(void* parent = nullptr);

    void setGhostText(const std::string& text);
    void clear();
    
    // Styling
    void setOpacity(int alpha); // 0-255
    void setFadeEnabled(bool enabled);
    
    // Position override (default: auto-track parent)
    void setCustomPosition(const void*& pos);
    void resetPosition(); // Return to auto-tracking

protected:
    void paintEvent(void*  event) override;
    bool eventFilter(void* watched, QEvent* event) override;

private:
    void onFadeTimerTimeout();

private:
    void updatePositionAndSize();
    void logEvent(const std::string& event, const std::string& detail = std::string());
    
    std::string m_ghostText;
    int m_opacity = 120; // Default semi-transparent
    bool m_fadeEnabled = false;
    bool m_customPositionSet = false;
    void* m_customPosition;
    
    void** m_fadeTimer = nullptr;
    int m_currentFadeAlpha = 0;
};

