#pragma once

#include <string>
#include <memory>
#include "code_minimap.h"

namespace RawrXD {

/**
 * \brief Editor container with integrated minimap
 * 
 * Combines a text editor with a minimap on the right side.
 * Provides a unified interface for embedding in tab widgets.
 */
class EditorWithMinimap {
public:
    explicit EditorWithMinimap();
    ~EditorWithMinimap() = default;

    /**
     * Get the text content
     */
    std::string getText() const { return m_text; }

    /**
     * Set the text content
     */
    void setText(const std::string& text) { m_text = text; }

    /**
     * Get the minimap widget
     */
    CodeMinimap* minimap() const { return m_minimap.get(); }

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
    std::string m_text;
    std::unique_ptr<CodeMinimap> m_minimap;
    bool m_minimapEnabled;
    int m_minimapWidth;
};

} // namespace RawrXD

private:
    QPlainTextEdit* m_editor = nullptr;
    CodeMinimap* m_minimap = nullptr;
};

} // namespace RawrXD
