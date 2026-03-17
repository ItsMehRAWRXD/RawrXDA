/**
 * \file ghost_text_renderer.h
 * \brief Cursor-style inline ghost text with diff preview
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once

// C++20 Qt-free data structs. For Qt widget implementation, include Qt headers and GhostTextRenderer class.
#include <string>
#include <vector>

namespace RawrXD {

/** RGBA color (replaces QColor) */
struct GhostTextColor {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

/**
 * \brief Ghost text decoration for inline completions
 */
struct GhostTextDecoration {
    int line = 0;
    int column = 0;
    std::string text;
    std::string type;           // "completion", "diff", "suggestion"
    GhostTextColor color;
    bool multiline = false;
    std::vector<std::string> lines;
};

/**
 * \brief Diff preview decoration
 */
struct DiffDecoration {
    int startLine = 0;
    int endLine = 0;
    std::string oldText;
    std::string newText;
    std::string type;           // "add", "remove", "modify"
};

#ifndef RAWR_USE_QT
/**
 * \brief Stub for Win32 build (no Qt). Ghost text uses inline state in Win32IDE.
 */
class GhostTextRenderer {
public:
    GhostTextRenderer() = default;
    GhostTextRenderer(void* /*editor*/, void* /*parent*/) {}
    void initialize() {}
    void setEditorHwnd(void* /*hwnd*/) {}
};
#else
#include <QWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPainter>
#include <QTextCursor>
#include <QString>
#include <QTimer>
#include <QMap>

/**
 * \brief Ghost text renderer overlay for QPlainTextEdit
 * 
 * Features:
 * - Cursor-style inline ghost text
 * - Real-time streaming completions
 * - Multi-line ghost text with proper indentation
 * - Diff preview (green additions, red deletions)
 * - Fade-in/fade-out animations
 * - Tab to accept, Esc to dismiss
 */
class GhostTextRenderer : public QWidget
{
    Q_OBJECT

public:
    explicit GhostTextRenderer(QPlainTextEdit* editor, QWidget* parent = nullptr);
    ~GhostTextRenderer() override = default;

    /**
     * Two-phase initialization
     */
    void initialize();

    /**
     * Show ghost text at cursor position
     */
    void showGhostText(const QString& text, const QString& type = "completion");

    /**
     * Show multi-line ghost text
     */
    void showMultilineGhost(const QStringList& lines);

    /**
     * Update ghost text (for streaming)
     */
    void updateGhostText(const QString& additionalText);

    /**
     * Show diff preview
     */
    void showDiffPreview(int startLine, int endLine, const QString& oldText, const QString& newText);

    /**
     * Clear all ghost text
     */
    void clearGhostText();

    /**
     * Clear diff preview
     */
    void clearDiffPreview();

    /**
     * Accept current ghost text (insert into editor)
     */
    void acceptGhostText();

    /**
     * Get current ghost text
     */
    QString getCurrentGhostText() const { return m_currentGhostText; }

    /**
     * Check if ghost text is visible
     */
    bool hasGhostText() const { return !m_currentGhostText.isEmpty(); }

signals:
    /**
     * Ghost text accepted by user
     */
    void ghostTextAccepted(const QString& text);

    /**
     * Ghost text dismissed
     */
    void ghostTextDismissed();

    /**
     * Diff accepted
     */
    void diffAccepted(const QString& newText);

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    void renderGhostText(QPainter& painter);
    void renderDiffPreview(QPainter& painter);
    void updateOverlayGeometry();
    QPoint getCursorPosition() const;
    int getLineHeight() const;
    QFont getEditorFont() const;
    void fadeIn();
    void fadeOut();

    QPlainTextEdit* m_editor{};
    
    QString m_currentGhostText;
    GhostTextDecoration m_ghostDecoration;
    QVector<DiffDecoration> m_diffDecorations;
    
    qreal m_opacity = 1.0;
    QTimer* m_fadeTimer{};
    bool m_fading = false;
    
    QColor m_ghostColor{128, 128, 128, 180};  // Gray with transparency
    QColor m_addColor{0, 255, 0, 100};        // Green for additions
    QColor m_removeColor{255, 0, 0, 100};     // Red for deletions
};

#endif // RAWR_USE_QT (Qt GhostTextRenderer class)

} // namespace RawrXD
