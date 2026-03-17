/**
 * @file CommandPalette.cpp
 * @brief VS Code-style command palette widget implementation
 */

#include "CommandPalette.hpp"
#include <QApplication>
#include <QScreen>

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent)
{
    setupUI();
    setWindowFlags(Qt::FramelessWindowHint | Qt::Tool | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    
    // Default commands
    m_allCommands = {
        ">File: New File",
        ">File: Open File...",
        ">File: Save",
        ">File: Save As...",
        ">Edit: Undo",
        ">Edit: Redo",
        ">Edit: Find",
        ">Edit: Replace",
        ">View: Command Palette",
        ">View: Toggle Terminal",
        ">View: Toggle Sidebar",
        ">Run: Start Debugging",
        ">Run: Run Without Debugging",
        ">Terminal: New Terminal",
        ">Git: Commit",
        ">Git: Push",
        ">AI: Explain Code",
        ">AI: Fix Code",
        ">AI: Refactor Code",
        ">AI: Load Model",
        ">Model: Load GGUF",
        ">Model: Switch Backend"
    };
    
    setCommands(m_allCommands);
}

void CommandPalette::setupUI() {
    setFixedWidth(600);
    setMaximumHeight(400);
    
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    
    setStyleSheet(
        "CommandPalette { background-color: #252526; border: 1px solid #3e3e42; }"
    );
    
    // Header
    m_headerLabel = new QLabel("Command Palette", this);
    m_headerLabel->setStyleSheet(
        "QLabel { color: #cccccc; background-color: #3c3c3c; padding: 8px; font-weight: bold; }"
    );
    layout->addWidget(m_headerLabel);
    
    // Search input
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText("> Type a command...");
    m_searchEdit->setStyleSheet(
        "QLineEdit { background-color: #3c3c3c; color: #ffffff; border: none; "
        "padding: 8px; font-size: 14px; }"
    );
    layout->addWidget(m_searchEdit);
    
    // Command list
    m_commandList = new QListWidget(this);
    m_commandList->setStyleSheet(
        "QListWidget { background-color: #252526; color: #cccccc; border: none; }"
        "QListWidget::item { padding: 8px; }"
        "QListWidget::item:hover { background-color: #094771; }"
        "QListWidget::item:selected { background-color: #094771; }"
    );
    layout->addWidget(m_commandList);
    
    // Connections
    connect(m_searchEdit, &QLineEdit::textChanged, this, &CommandPalette::onSearchTextChanged);
    connect(m_commandList, &QListWidget::itemActivated, this, &CommandPalette::onItemActivated);
    
    m_searchEdit->installEventFilter(this);
}

void CommandPalette::show() {
    m_searchEdit->clear();
    filterCommands("");
    
    // Position at top center of parent or screen
    if (parentWidget()) {
        QPoint pos = parentWidget()->mapToGlobal(QPoint(
            (parentWidget()->width() - width()) / 2,
            50
        ));
        move(pos);
    } else {
        QScreen* screen = QApplication::primaryScreen();
        if (screen) {
            QRect geo = screen->availableGeometry();
            move((geo.width() - width()) / 2, geo.top() + 50);
        }
    }
    
    QWidget::show();
    raise();
    activateWindow();
    m_searchEdit->setFocus();
}

void CommandPalette::hide() {
    QWidget::hide();
    emit closed();
}

void CommandPalette::setCommands(const QStringList& commands) {
    m_allCommands = commands;
    filterCommands(m_searchEdit->text());
}

void CommandPalette::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
        return;
    }
    QWidget::keyPressEvent(event);
}

bool CommandPalette::eventFilter(QObject* obj, QEvent* event) {
    if (obj == m_searchEdit && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Down) {
            m_commandList->setFocus();
            if (m_commandList->count() > 0) {
                m_commandList->setCurrentRow(0);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Up) {
            m_commandList->setFocus();
            if (m_commandList->count() > 0) {
                m_commandList->setCurrentRow(m_commandList->count() - 1);
            }
            return true;
        } else if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (m_commandList->count() > 0) {
                onItemActivated(m_commandList->item(0));
            }
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void CommandPalette::onSearchTextChanged(const QString& text) {
    filterCommands(text);
}

void CommandPalette::onItemActivated(QListWidgetItem* item) {
    if (item) {
        QString cmd = item->text();
        hide();
        emit commandTriggered(cmd);
    }
}

void CommandPalette::filterCommands(const QString& filter) {
    m_commandList->clear();
    
    QString lowerFilter = filter.toLower();
    
    for (const QString& cmd : m_allCommands) {
        if (filter.isEmpty() || cmd.toLower().contains(lowerFilter)) {
            m_commandList->addItem(cmd);
        }
    }
    
    // Auto-select first item
    if (m_commandList->count() > 0) {
        m_commandList->setCurrentRow(0);
    }
    
    // Resize based on items
    int itemHeight = 35;
    int maxItems = 10;
    int visibleItems = qMin(m_commandList->count(), maxItems);
    m_commandList->setFixedHeight(visibleItems * itemHeight);
    adjustSize();
}
