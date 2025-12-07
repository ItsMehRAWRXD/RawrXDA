// Chat Workspace - Agentic chat interface
#include "chat_workspace.h"
#include <QVBoxLayout>
#include <QLabel>

ChatWorkspace::ChatWorkspace(QWidget* parent) : QWidget(parent) {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Chat Workspace"));
}
