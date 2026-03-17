#pragma once
// <QWidget> removed (Qt-free build)
// <QString> removed (Qt-free build)
// <QTimer> removed (Qt-free build)

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
class OverlayWidget  {
    /* Q_OBJECT */
    
public:
    explicit OverlayWidget(QWidget* parent = nullptr);

    void setGhostText(const QString& text);
    void clear();
    
    // Styling
    void setOpacity(int alpha); // 0-255
    void setFadeEnabled(bool enabled);
    
    // Position override (default: auto-track parent)
    void setCustomPosition(const QPoint& pos);
    void resetPosition(); // Return to auto-tracking

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onFadeTimerTimeout();

private:
    void updatePositionAndSize();
    void logEvent(const QString& event, const QString& detail = QString());
    
    QString m_ghostText;
    int m_opacity = 120; // Default semi-transparent
    bool m_fadeEnabled = false;
    bool m_customPositionSet = false;
    QPoint m_customPosition;
    
    QTimer* m_fadeTimer = nullptr;
    int m_currentFadeAlpha = 0;
};
