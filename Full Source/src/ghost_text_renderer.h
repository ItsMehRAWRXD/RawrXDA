/**
 * \file ghost_text_renderer.h
 * \brief Cursor-style inline ghost text with diff preview
 * \author RawrXD Team
 * \date 2025-12-07
 */

#pragma once


namespace RawrXD {

/**
 * \brief Ghost text decoration for inline completions
 */
struct GhostTextDecoration {
    int line;                   // Line number (0-indexed)
    int column;                 // Column position
    std::string text;               // Ghost text to display
    std::string type;               // "completion", "diff", "suggestion"
    uint32_t color;              // Ghost text color
    bool multiline = false;    // Multi-line ghost text
    std::vector<std::string> lines;         // For multi-line ghost text
};

/**
 * \brief Diff preview decoration
 */
struct DiffDecoration {
    int startLine;             // Start line (0-indexed)
    int endLine;               // End line (0-indexed)
    std::string oldText;           // Original text
    std::string newText;           // Suggested text
    std::string type;              // "add", "remove", "modify"
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
class GhostTextRenderer : public void
{

public:
    explicit GhostTextRenderer(QPlainTextEdit* editor, void* parent = nullptr);
    ~GhostTextRenderer() override = default;

    /**
     * Two-phase initialization
     */
    void initialize();

    /**
     * Show ghost text at cursor position
     */
    void showGhostText(const std::string& text, const std::string& type = "completion");

    /**
     * Show multi-line ghost text
     */
    void showMultilineGhost(const std::vector<std::string>& lines);

    /**
     * Update ghost text (for streaming)
     */
    void updateGhostText(const std::string& additionalText);

    /**
     * Show diff preview
     */
    void showDiffPreview(int startLine, int endLine, const std::string& oldText, const std::string& newText);

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
    std::string getCurrentGhostText() const { return m_currentGhostText; }

    /**
     * Check if ghost text is visible
     */
    bool hasGhostText() const { return !m_currentGhostText.empty(); }


    /**
     * Ghost text accepted by user
     */
    void ghostTextAccepted(const std::string& text);

    /**
     * Ghost text dismissed
     */
    void ghostTextDismissed();

    /**
     * Diff accepted
     */
    void diffAccepted(const std::string& newText);

protected:
    void paintEvent(void*  event) override;
    bool eventFilter(void* obj, QEvent* event) override;

private:
    void renderGhostText(QPainter& painter);
    void renderDiffPreview(QPainter& painter);
    void updateOverlayGeometry();
    void* getCursorPosition() const;
    int getLineHeight() const;
    std::string getEditorFont() const;
    void fadeIn();
    void fadeOut();

    QPlainTextEdit* m_editor{};
    
    std::string m_currentGhostText;
    GhostTextDecoration m_ghostDecoration;
    std::vector<DiffDecoration> m_diffDecorations;
    
    qreal m_opacity = 1.0;
    void** m_fadeTimer{};
    bool m_fading = false;
    
    uint32_t m_ghostColor{128, 128, 128, 180};  // Gray with transparency
    uint32_t m_addColor{0, 255, 0, 100};        // Green for additions
    uint32_t m_removeColor{255, 0, 0, 100};     // Red for deletions
};

} // namespace RawrXD


