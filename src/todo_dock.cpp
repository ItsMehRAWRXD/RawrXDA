// TODO Dock - UI component for displaying TODO items
#include "todo_dock.h"
#include "todo_manager.h"
#include <QVBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QDateTime>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>

TodoDock::TodoDock(TodoManager* todoManager, QWidget* parent) 
    : QWidget(parent), todoManager_(todoManager), treeWidget_(nullptr) {
    // Lightweight constructor - defer Qt widget creation
}

void TodoDock::initialize() {
    if (treeWidget_) return;  // Already initialized
    
    setupUI();
    loadTodos();
    
    // Connect to todo manager signals
    connect(todoManager_, &TodoManager::todoAdded, this, &TodoDock::onTodoAdded);
    connect(todoManager_, &TodoManager::todoCompleted, this, &TodoDock::onTodoCompleted);
    connect(todoManager_, &TodoManager::todoRemoved, this, &TodoDock::onTodoRemoved);
}

void TodoDock::setupUI() {
    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    
    treeWidget_ = new QTreeWidget(this);
    treeWidget_->setHeaderLabels({"Description", "File", "Created", "Status"});
    treeWidget_->setStyleSheet(
        "QTreeWidget { background-color: #252526; color: #d4d4d4; border: none; }"
        "QTreeWidget::item:selected { background-color: #37373d; }");
    
    // Set column widths
    treeWidget_->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    treeWidget_->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    treeWidget_->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    treeWidget_->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    
    connect(treeWidget_, &QTreeWidget::itemDoubleClicked, 
            this, &TodoDock::onItemDoubleClicked);
    
    layout->addWidget(treeWidget_);
}

void TodoDock::loadTodos() {
    treeWidget_->clear();
    
    QList<TodoItem> todos = todoManager_->getTodos();
    for (const TodoItem& todo : todos) {
        QTreeWidgetItem* item = new QTreeWidgetItem(treeWidget_);
        item->setText(0, todo.description);
        item->setText(1, todo.filePath);
        item->setText(2, todo.created.toString("yyyy-MM-dd hh:mm"));
        item->setText(3, todo.isCompleted ? "Completed" : "Pending");
        item->setData(0, Qt::UserRole, todo.id);
        item->setData(0, Qt::UserRole + 1, todo.isCompleted);
    }
}

void TodoDock::refreshTodos() {
    loadTodos();
}

void TodoDock::onTodoAdded(const TodoItem& todo) {
    QTreeWidgetItem* item = new QTreeWidgetItem(treeWidget_);
    item->setText(0, todo.description);
    item->setText(1, todo.filePath);
    item->setText(2, todo.created.toString("yyyy-MM-dd hh:mm"));
    item->setText(3, todo.isCompleted ? "Completed" : "Pending");
    item->setData(0, Qt::UserRole, todo.id);
    item->setData(0, Qt::UserRole + 1, todo.isCompleted);
}

void TodoDock::onTodoCompleted(const QString& id) {
    for (int i = 0; i < treeWidget_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = treeWidget_->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == id) {
            item->setText(3, "Completed");
            item->setData(0, Qt::UserRole + 1, true);
            break;
        }
    }
}

void TodoDock::onTodoRemoved(const QString& id) {
    for (int i = 0; i < treeWidget_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = treeWidget_->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toString() == id) {
            delete item;
            break;
        }
    }
}

void TodoDock::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    QString filePath = item->text(1);
    if (!filePath.isEmpty()) {
        // Get the TODO ID from the item data
        QString todoId = item->data(0, Qt::UserRole).toString();
        // Emit signal to open file in editor
        emit openFileRequested(filePath, todoId);
    }
}

void TodoDock::onAddTodo() {
    bool ok;
    QString description = QInputDialog::getText(this, "Add TODO", 
        "TODO Description:", QLineEdit::Normal, "", &ok);
    
    if (ok && !description.isEmpty()) {
        todoManager_->addTodo(description, QString(), 0);
    }
}

void TodoDock::onCompleteTodo() {
    QTreeWidgetItem* item = treeWidget_->currentItem();
    if (item) {
        QString todoId = item->data(0, Qt::UserRole).toString();
        todoManager_->completeTodo(todoId);
    }
}

void TodoDock::onRemoveTodo() {
    QTreeWidgetItem* item = treeWidget_->currentItem();
    if (item) {
        QString todoId = item->data(0, Qt::UserRole).toString();
        todoManager_->removeTodo(todoId);
    }
}

void TodoDock::onScanCode() {
    QMessageBox::information(this, "Scan for TODOs",
        "This feature will scan all project files for TODO comments.\n\n"
        "Implementation in progress...");
}