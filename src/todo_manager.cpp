// TODO Manager - Task and TODO tracking
#include "todo_manager.h"
#include <QUuid>
#include <QDebug>

TodoManager::TodoManager(QObject* parent) : QObject(parent) {
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
}

void TodoManager::completeTodo(const QString& id) {
    for (int i = 0; i < todos_.size(); ++i) {
        if (todos_[i].id == id) {
            todos_[i].isCompleted = true;
            todos_[i].completed = QDateTime::currentDateTime();
            emit todoCompleted(id);
            break;
        }
    }
}

void TodoManager::removeTodo(const QString& id) {
    for (int i = 0; i < todos_.size(); ++i) {
        if (todos_[i].id == id) {
            todos_.removeAt(i);
            emit todoRemoved(id);
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