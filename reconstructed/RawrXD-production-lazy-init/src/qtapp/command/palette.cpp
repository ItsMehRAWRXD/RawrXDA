#include "command_palette.hpp"
#include "integration/ProdIntegration.h"
#include <QApplication>
#include <QScreen>
#include <QVBoxLayout>
#include <QElapsedTimer>
#include <QDebug>

CommandPalette::CommandPalette(QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint | Qt::Popup)
{
    setupUI();
    applyDarkTheme();
    
    m_searchBox->installEventFilter(this);
    m_resultsList->installEventFilter(this);
}

void CommandPalette::setupUI() {
    RAWRXD_TIMED_FUNC();
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(10, 10, 10, 10);
    layout->setSpacing(10);
    
    m_searchBox = new QLineEdit(this);
    m_searchBox->setPlaceholderText("Type a command...");
    m_searchBox->setMinimumWidth(500);
    layout->addWidget(m_searchBox);
    
    m_resultsList = new QListWidget(this);
    m_resultsList->setMinimumHeight(300);
    layout->addWidget(m_resultsList);
    
    m_hintLabel = new QLabel("Use up/down to navigate, Enter to select, Esc to close", this);
    m_hintLabel->setStyleSheet("color: #888; font-size: 10px;");
    layout->addWidget(m_hintLabel);
    
    connect(m_searchBox, &QLineEdit::textChanged, this, &CommandPalette::onSearchTextChanged);
    connect(m_resultsList, &QListWidget::itemActivated, this, &CommandPalette::onItemActivated);
}

void CommandPalette::registerCommand(const Command& cmd) {
    m_commands[cmd.id] = cmd;
}

void CommandPalette::show() {
    updateResults("");
    m_searchBox->clear();
    m_searchBox->setFocus();

    // Size and position before showing to avoid a zero-sized popup
    adjustSize();
    if (parentWidget()) {
        auto parentCenter = parentWidget()->geometry().center();
        move(parentCenter.x() - width() / 2, parentCenter.y() - height() / 2);
    } else {
        auto screenCenter = QApplication::primaryScreen()->geometry().center();
        move(screenCenter.x() - width() / 2, screenCenter.y() - height() / 2);
    }

    QWidget::show();
    raise();
    activateWindow();
}

void CommandPalette::hide() {
    QWidget::hide();
}

void CommandPalette::onSearchTextChanged(const QString& text) {
    updateResults(text);
}

void CommandPalette::onItemActivated(QListWidgetItem* item) {
    if (!item) return;
    executeSelectedCommand();
}

void CommandPalette::executeSelectedCommand() {
    RAWRXD_TIMED_FUNC();
    auto* item = m_resultsList->currentItem();
    if (!item) return;
    
    QString cmdId = item->data(Qt::UserRole).toString();
    if (m_commands.contains(cmdId)) {
        if (m_commands[cmdId].action) {
            m_commands[cmdId].action();
        }
        emit commandExecuted(cmdId);
    }
    hide();
}

void CommandPalette::updateResults(const QString& filter) {
    RAWRXD_TIMED_FUNC();
    QElapsedTimer timer;
    timer.start();
    
    m_resultsList->clear();
    
    for (auto it = m_commands.begin(); it != m_commands.end(); ++it) {
        if (filter.isEmpty() || it->label.contains(filter, Qt::CaseInsensitive)) {
            auto* item = new QListWidgetItem(it->label, m_resultsList);
            item->setData(Qt::UserRole, it->id);
            if (!it->category.isEmpty()) {
                item->setText(QString("[%1] %2").arg(it->category).arg(it->label));
            }
        }
    }
    
    if (m_resultsList->count() > 0) {
        m_resultsList->setCurrentRow(0);
    }

    qDebug() << "CommandPalette updated results for" << filter << "in" << timer.nsecsElapsed() / 1000.0 << "microseconds";
}

void CommandPalette::applyDarkTheme() {
    setStyleSheet(
        "QWidget { background-color: #1e1e1e; color: #d4d4d4; border: 1px solid #3a3a3a; font-family: 'Consolas', 'Cascadia Code', monospace; font-size: 12px; }"
        "QLineEdit { background-color: #2d2d30; border: 1px solid #007acc; padding: 6px 8px; color: #ffffff; selection-background-color: #007acc; border-radius: 4px; }"
        "QListWidget { background-color: #1e1e1e; border: 1px solid #2b2b2b; outline: none; }"
        "QListWidget::item { padding: 8px 10px; border-bottom: 1px solid #2b2b2b; }"
        "QListWidget::item:selected { background-color: #094771; color: #ffffff; }"
        "QLabel { color: #9e9e9e; }"
    );
}

bool CommandPalette::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Escape) {
            hide();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            executeSelectedCommand();
            return true;
        }
        if (keyEvent->key() == Qt::Key_Up) {
            int row = m_resultsList->currentRow();
            if (row > 0) m_resultsList->setCurrentRow(row - 1);
            return true;
        }
        if (keyEvent->key() == Qt::Key_Down) {
            int row = m_resultsList->currentRow();
            if (row < m_resultsList->count() - 1) m_resultsList->setCurrentRow(row + 1);
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}

void CommandPalette::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hide();
    } else {
        QWidget::keyPressEvent(event);
    }
}

