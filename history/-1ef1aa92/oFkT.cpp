#include "editor_with_minimap.h"
#include <QHBoxLayout>
#include <QPlainTextEdit>

namespace RawrXD {

EditorWithMinimap::EditorWithMinimap(QWidget* parent)
    : QWidget(parent)
{
    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create text editor
    m_editor = new QPlainTextEdit(this);
    m_editor->setStyleSheet(
        "QPlainTextEdit { "
        "background-color: #1e1e1e; "
        "color: #e0e0e0; "
        "font-family: 'Consolas', monospace; "
        "font-size: 11pt; "
        "border: none; "
        "}"
    );
    layout->addWidget(m_editor, 1);

    // Create minimap
    m_minimap = new CodeMinimap(m_editor, this);
    layout->addWidget(m_minimap, 0);

    setLayout(layout);
}

void EditorWithMinimap::setMinimapEnabled(bool enabled)
{
    if (m_minimap) {
        m_minimap->setEnabled(enabled);
    }
}

bool EditorWithMinimap::isMinimapEnabled() const
{
    if (m_minimap) {
        return m_minimap->isEnabled();
    }
    return false;
}

void EditorWithMinimap::setMinimapWidth(int width)
{
    if (m_minimap) {
        m_minimap->setWidth(width);
    }
}

int EditorWithMinimap::minimapWidth() const
{
    if (m_minimap) {
        return m_minimap->width();
    }
    return 120;
}

void EditorWithMinimap::replace(const QString& oldText, const QString& newText) {
    if (m_editor) {
        QTextCursor cursor = m_editor->textCursor();
        cursor.beginEditBlock();
        QString content = m_editor->toPlainText();
        content.replace(oldText, newText);
        m_editor->setPlainText(content);
        cursor.endEditBlock();
    }
}

} // namespace RawrXD
