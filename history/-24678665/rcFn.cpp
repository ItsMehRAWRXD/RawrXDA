// TODO Manager - Task and TODO tracking
#include "todo_manager.h"
#include <QUuid>
#include <QDebug>
#include <QSettings>

TodoManager::TodoManager(QObject* parent) : QObject(parent) {
    // Load saved TODOs on construction
    QSettings settings("RawrXD", "AgenticIDE");
    int count = settings.beginReadArray("todos");
    
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        TodoItem todo;
        todo.id = settings.value("id").toString();
        todo.description = settings.value("description").toString();
        todo.filePath = settings.value("filePath").toString();
        todo.lineNumber = settings.value("lineNumber", 0).toInt();
        todo.created = settings.value("created").toDateTime();
        todo.completed = settings.value("completed").toDateTime();
        todo.isCompleted = settings.value("isCompleted", false).toBool();
        
        todos_.append(todo);
    }
    settings.endArray();
}

void TodoManager::addTodo(const QString& description, const QString& filePath, int lineNumber) {
    TodoItem todo;
    todo.id = QUuid::createUuid().toString();
    todo.description = description;
    todo.filePath = filePath;
    todo.lineNumber = lineNumber;
    todo.created = QDateTime::currentDateTime();
    todo.isCompleted = false;
    
    todos_.append(todo);
    emit todoAdded(todo);
    
    // Save to settings
    saveTodos();
}

void TodoManager::completeTodo(const QString& id) {
    for (int i = 0; i < todos_.size(); ++i) {
        if (todos_[i].id == id) {
            todos_[i].isCompleted = true;
            todos_[i].completed = QDateTime::currentDateTime();
            emit todoCompleted(id);
            
            // Save to settings
            saveTodos();
            break;
        }
    }
}

void TodoManager::removeTodo(const QString& id) {
    for (int i = 0; i < todos_.size(); ++i) {
        if (todos_[i].id == id) {
            todos_.removeAt(i);
            emit todoRemoved(id);
            
            // Save to settings
            saveTodos();
            break;
        }
    }
}

QList<TodoItem> TodoManager::getTodos() const {
    return todos_;
}

QList<TodoItem> TodoManager::getPendingTodos() const {
    QList<TodoItem> pending;
    for (const TodoItem& todo : todos_) {
        if (!todo.isCompleted) {
            pending.append(todo);
        }
    }
    return pending;
}

QList<TodoItem> TodoManager::getCompletedTodos() const {
    QList<TodoItem> completed;
    for (const TodoItem& todo : todos_) {
        if (todo.isCompleted) {
            completed.append(todo);
        }
    }
    return completed;
}

void TodoManager::saveTodos() {
    QSettings settings("RawrXD", "AgenticIDE");
    settings.beginWriteArray("todos");
    
    for (int i = 0; i < todos_.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("id", todos_[i].id);
        settings.setValue("description", todos_[i].description);
        settings.setValue("filePath", todos_[i].filePath);
        settings.setValue("lineNumber", todos_[i].lineNumber);
        settings.setValue("created", todos_[i].created);
        settings.setValue("completed", todos_[i].completed);
        settings.setValue("isCompleted", todos_[i].isCompleted);
    }
    
    settings.endArray();
}

QList<TodoItem> TodoManager::getCompletedTodos() const {
    QList<TodoItem> completed;
    for (const TodoItem& todo : todos_) {
        if (todo.isCompleted) {
            completed.append(todo);
        }
    }
    return completed;
}