// Chat Workspace - Agentic chat interface
#include "chat_workspace.h"
#include <QVBoxLayout>
#include <QLabel>

ChatWorkspace::ChatWorkspace(QWidget* parent) : QWidget(parent) {
    // Lightweight constructor - defer Qt widget creation
}

void ChatWorkspace::initialize() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Chat Workspace"));
}
