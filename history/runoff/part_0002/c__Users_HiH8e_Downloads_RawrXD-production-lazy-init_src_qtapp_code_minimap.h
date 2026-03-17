#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include <QPainter>
#include <QTimer>
#include <memory>

namespace RawrXD {

/**
 * \brief Code minimap widget - displays a scaled overview of document
 * 
 * Shows the entire document in miniature form on the right side of the editor.
 * Allows quick navigation via clicking and displays a viewport indicator.
 * 
 * Features:
 * - Real-time synchronization with editor
 * - Viewport indicator showing visible area
 * - Click-to-navigate functionality
 * - Configurable width and zoom level
 * - Light/dark theme support
 */
class CodeMinimap : public QWidget {
    Q_OBJECT

public:
    explicit CodeMinimap(QPlainTextEdit* editor, QWidget* parent = nullptr);
    ~CodeMinimap() override = default;

    /**
     * Set the text editor to display minimap for
     */
    void setEditor(QPlainTextEdit* editor);

    /**
     * Get current editor
     */
    QPlainTextEdit* editor() const { return m_editor; }

    /**
     * Set minimap width (default: 120px)
     */
    void setWidth(int width);

    /**
     * Get minimap width
     */
    int width() const { return m_minimapWidth; }

    /**
     * Enable/disable minimap
     */
    void setEnabled(bool enabled);

    /**
     * Check if minimap is enabled
     */
    bool isEnabled() const { return m_enabled; }

    /**
     * Set line height for rendering (in pixels)
     */
    void setLineHeight(int height) { m_lineHeight = height; }

    /**
     * Get line height
     */
    int lineHeight() const { return m_lineHeight; }

    /**
     * Set zoom factor (default: 1.0)
     */
    void setZoomFactor(double factor) { m_zoomFactor = factor; }

    /**
     * Get zoom factor
     */
    double zoomFactor() const { return m_zoomFactor; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private slots:
    void onEditorTextChanged();
    void onEditorScrolled();
    void updateViewport();

private:
    void drawMinimap(QPainter& painter);
    void drawViewportIndicator(QPainter& painter);
    void handleNavigationClick(int y);
    int getLineCountFromEditor() const;
    int getFirstVisibleLine() const;
    int getLastVisibleLine() const;

    QPlainTextEdit* m_editor = nullptr;
    bool m_enabled = true;
    int m_minimapWidth = 120;
    int m_lineHeight = 2;
    double m_zoomFactor = 1.0;
    QTimer* m_updateTimer = nullptr;
    int m_cachedLineCount = 0;
    QColor m_textColor{180, 180, 180};
    QColor m_viewportColor{0, 122, 204};
    QColor m_viewportAlpha{0, 122, 204, 30};
    bool m_forwardingWheel = false;
};

} // namespace RawrXD
