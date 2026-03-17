#pragma once

#include <QWidget>
#include <QScrollBar>

class QPlainTextEdit;
class QTextDocument;

namespace RawrXD {

/**
 * @brief Code minimap widget that shows a scaled overview of the document
 * 
 * Displays the entire document content in a miniature view on the right side
 * of the editor, allowing quick navigation and visual overview of code structure.
 */
class CodeMinimap : public QWidget
{
    Q_OBJECT

public:
    explicit CodeMinimap(QWidget* parent = nullptr);
    ~CodeMinimap() override = default;

    /**
     * @brief Set the text editor to display in the minimap
     * @param editor The QPlainTextEdit to monitor and display
     */
    void setEditor(QPlainTextEdit* editor);

    /**
     * @brief Get the current editor
     */
    QPlainTextEdit* editor() const { return m_editor; }

    /**
     * @brief Set whether the minimap is visible
     */
    void setMinimapVisible(bool visible);

    /**
     * @brief Check if minimap is visible
     */
    bool isMinimapVisible() const { return m_visible; }

    /**
     * @brief Set the scale factor for text rendering
     * @param scale Scale factor (default 0.1 = 10% of original size)
     */
    void setScale(double scale);

    /**
     * @brief Get current scale factor
     */
    double scale() const { return m_scale; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void updateContent();
    void updateViewport();
    void onEditorScroll(int value);

private:
    void scrollToPosition(int y);
    QRect calculateVisibleRect() const;
    
    QPlainTextEdit* m_editor{nullptr};
    QTextDocument* m_document{nullptr};
    double m_scale{0.08};  // 8% scale for readability
    bool m_visible{true};
    bool m_dragging{false};
    int m_dragStartY{0};
};

} // namespace RawrXD
