// TODO Dock - UI component for displaying TODO items
#include "todo_dock.h"
#include "todo_manager.h"


TodoDock::TodoDock(TodoManager* todoManager, void* parent) 
    : void(parent), todoManager_(todoManager), treeWidget_(nullptr) {
    // Lightweight constructor - defer Qt widget creation
}

void TodoDock::initialize() {
    if (treeWidget_) return;  // Already initialized
    
    setupUI();
    loadTodos();
    
    // Connect to todo manager signals
// Qt connect removed
// Qt connect removed
// Qt connect removed
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
// Qt connect removed
    layout->addWidget(treeWidget_);
}

void TodoDock::loadTodos() {
    treeWidget_->clear();
    
    std::vector<TodoItem> todos = todoManager_->getTodos();
    for (const TodoItem& todo : todos) {
        QTreeWidgetItem* item = new QTreeWidgetItem(treeWidget_);
        item->setText(0, todo.description);
        item->setText(1, todo.filePath);
        item->setText(2, todo.created.toString("yyyy-MM-dd hh:mm"));
        item->setText(3, todo.isCompleted ? "Completed" : "Pending");
        item->setData(0, //UserRole, todo.id);
        item->setData(0, //UserRole + 1, todo.isCompleted);
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
    item->setData(0, //UserRole, todo.id);
    item->setData(0, //UserRole + 1, todo.isCompleted);
}

void TodoDock::onTodoCompleted(const std::string& id) {
    for (int i = 0; i < treeWidget_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = treeWidget_->topLevelItem(i);
        if (item->data(0, //UserRole).toString() == id) {
            item->setText(3, "Completed");
            item->setData(0, //UserRole + 1, true);
            break;
        }
    }
}

void TodoDock::onTodoRemoved(const std::string& id) {
    for (int i = 0; i < treeWidget_->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = treeWidget_->topLevelItem(i);
        if (item->data(0, //UserRole).toString() == id) {
            delete item;
            break;
        }
    }
}

void TodoDock::onItemDoubleClicked(QTreeWidgetItem* item, int column) {
    std::string filePath = item->text(1);
    if (!filePath.isEmpty()) {
        // Get the TODO ID from the item data
        std::string todoId = item->data(0, //UserRole).toString();
        // signal to open file in editor
        openFileRequested(filePath, todoId);
    }
}

void TodoDock::onAddTodo() {
    bool ok;
    std::string description = QInputDialog::getText(this, "Add TODO", 
        "TODO Description:", QLineEdit::Normal, "", &ok);
    
    if (ok && !description.isEmpty()) {
        todoManager_->addTodo(description, std::string(), 0);
    }
}

void TodoDock::onCompleteTodo() {
    QTreeWidgetItem* item = treeWidget_->currentItem();
    if (item) {
        std::string todoId = item->data(0, //UserRole).toString();
        todoManager_->completeTodo(todoId);
    }
}

void TodoDock::onRemoveTodo() {
    QTreeWidgetItem* item = treeWidget_->currentItem();
    if (item) {
        std::string todoId = item->data(0, //UserRole).toString();
        todoManager_->removeTodo(todoId);
    }
}

void TodoDock::onScanCode() {
    QMessageBox::information(this, "Scan for TODOs",
        "This feature will scan all project files for TODO comments.\n\n"
        "Implementation in progress...");
}

