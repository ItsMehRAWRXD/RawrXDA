#pragma once

#include <QWidget>
#include <QPlainTextEdit>
#include "code_minimap.h"

namespace RawrXD {

/**
 * \brief Editor container with integrated minimap
 * 
 * Combines a text editor with a minimap on the right side.
 * Provides a unified interface for embedding in tab widgets.
 */
class EditorWithMinimap : public QWidget {
    Q_OBJECT

public:
    explicit EditorWithMinimap(QWidget* parent = nullptr);
    ~EditorWithMinimap() override = default;

    /**
     * Get the text editor widget
     */
    QPlainTextEdit* editor() const { return m_editor; }

    /**
     * Get the minimap widget
     */
    CodeMinimap* minimap() const { return m_minimap; }

    /**
     * Enable/disable minimap
     */
    void setMinimapEnabled(bool enabled);

    /**
     * Check if minimap is enabled
     */
    bool isMinimapEnabled() const;

    /**
     * Set minimap width
     */
    void setMinimapWidth(int width);

    /**
     * Get minimap width
     */
    int minimapWidth() const;

private:
    QPlainTextEdit* m_editor = nullptr;
    CodeMinimap* m_minimap = nullptr;
};

} // namespace RawrXD
