#pragma once

#include <QWidget>
#include <QHBoxLayout>

namespace RawrXD {

class AgenticTextEdit;
class CodeMinimap;

/**
 * @brief Container widget that combines a text editor with a minimap
 * 
 * Provides a horizontal layout with the editor on the left and minimap on the right.
 * This is the widget that should be added to tabs instead of raw AgenticTextEdit.
 */
class EditorWithMinimap : public QWidget
{
    Q_OBJECT

public:
    explicit EditorWithMinimap(QWidget* parent = nullptr);
    ~EditorWithMinimap() override = default;

    /**
     * @brief Get the text editor
     */
    AgenticTextEdit* editor() const { return m_editor; }

    /**
     * @brief Get the minimap widget
     */
    CodeMinimap* minimap() const { return m_minimap; }

    /**
     * @brief Set minimap visibility
     */
    void setMinimapVisible(bool visible);

    /**
     * @brief Check if minimap is visible
     */
    bool isMinimapVisible() const;

private:
    AgenticTextEdit* m_editor;
    CodeMinimap* m_minimap;
    QHBoxLayout* m_layout;
};

} // namespace RawrXD
