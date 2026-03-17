/**
 * \file ghost_text_renderer.h
 * \brief Cursor-style inline ghost text with diff preview
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once

#include <QWidget>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QPainter>
#include <QTextCursor>
#include <QString>
#include <QTimer>
#include <QMap>

namespace RawrXD {

/**
 * \brief Ghost text decoration for inline completions
 */
struct GhostTextDecoration {
    int line;                   // Line number (0-indexed)
    int column;                 // Column position
    QString text;               // Ghost text to display
    QString type;               // "completion", "diff", "suggestion"
    QColor color;              // Ghost text color
    bool multiline = false;    // Multi-line ghost text
    QStringList lines;         // For multi-line ghost text
};

/**
 * \brief Diff preview decoration
 */
struct DiffDecoration {
    int startLine;             // Start line (0-indexed)
    int endLine;               // End line (0-indexed)
    QString oldText;           // Original text
    QString newText;           // Suggested text
    QString type;              // "add", "remove", "modify"
};

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

} // namespace RawrXD
