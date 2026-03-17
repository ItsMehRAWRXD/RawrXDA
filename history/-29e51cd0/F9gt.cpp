#include "editor_with_minimap.h"
#include "../agentic_text_edit.h"
#include "code_minimap.h"
#include <QHBoxLayout>

namespace RawrXD {

EditorWithMinimap::EditorWithMinimap(QWidget* parent)
    : QWidget(parent)
{
    m_layout = new QHBoxLayout(this);
    m_layout->setContentsMargins(0, 0, 0, 0);
    m_layout->setSpacing(0);

    // Create editor
    m_editor = new AgenticTextEdit(this);
    m_layout->addWidget(m_editor);

    // Create minimap
    m_minimap = new CodeMinimap(this);
    m_minimap->setEditor(m_editor);
    m_layout->addWidget(m_minimap);

    setLayout(m_layout);
}

void EditorWithMinimap::setMinimapVisible(bool visible)
{
    if (m_minimap) {
        m_minimap->setMinimapVisible(visible);
    }
}

bool EditorWithMinimap::isMinimapVisible() const
{
    return m_minimap && m_minimap->isMinimapVisible();
}

} // namespace RawrXD
