#include "todo_manager.h"
#include <QSettings>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>

TodoManager::TodoManager(QObject* parent) : QObject(parent) {
    loadTodos();
}

void TodoManager::addTodo(const QString& description, const QString& filePath, int lineNumber) {
    TodoItem item;
    item.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    item.description = description;
    item.filePath = filePath;
    item.lineNumber = lineNumber;
    item.created = QDateTime::currentDateTime();
    item.isCompleted = false;
    
    todos_.append(item);
    saveTodos();
    emit todoAdded(item);
}

void TodoManager::completeTodo(const QString& id) {
    for (auto& todo : todos_) {
        if (todo.id == id && !todo.isCompleted) {
            todo.isCompleted = true;
            todo.completed = QDateTime::currentDateTime();
            saveTodos();
            emit todoCompleted(id);
            break;
        }
    }
}

void TodoManager::removeTodo(const QString& id) {
    for (int i = 0; i < todos_.size(); ++i) {
        if (todos_[i].id == id) {
            todos_.removeAt(i);
            saveTodos();
            emit todoRemoved(id);
            break;
        }
    }
}

void TodoManager::clearAllTodos() {
    if (todos_.isEmpty()) {
        return;
    }
    
    todos_.clear();
    saveTodos();
    
    // Emit removal signal for each todo that was cleared
    // This ensures UI components can update appropriately
    emit todoRemoved(QString()); // Empty string signals "all todos cleared"
}

QList<TodoItem> TodoManager::getTodos() const {
    return todos_;
}

QList<TodoItem> TodoManager::getPendingTodos() const {
    QList<TodoItem> pending;
    for (const auto& todo : todos_) {
        if (!todo.isCompleted) {
            pending.append(todo);
        }
    }
    return pending;
}

QList<TodoItem> TodoManager::getCompletedTodos() const {
    QList<TodoItem> completed;
    for (const auto& todo : todos_) {
        if (todo.isCompleted) {
            completed.append(todo);
        }
    }
    return completed;
}

void TodoManager::saveTodos() {
    QJsonArray jsonArray;
    
    for (const auto& todo : todos_) {
        QJsonObject obj;
        obj["id"] = todo.id;
        obj["description"] = todo.description;
        obj["filePath"] = todo.filePath;
        obj["lineNumber"] = todo.lineNumber;
        obj["created"] = todo.created.toString(Qt::ISODate);
        obj["isCompleted"] = todo.isCompleted;
        if (todo.isCompleted) {
            obj["completed"] = todo.completed.toString(Qt::ISODate);
        }
        jsonArray.append(obj);
    }
    
    QJsonDocument doc(jsonArray);
    QSettings settings;
    settings.setValue("todos/data", QString::fromUtf8(doc.toJson()));
}

void TodoManager::loadTodos() {
    QSettings settings;
    QString jsonData = settings.value("todos/data").toString();
    
    if (jsonData.isEmpty()) {
        return;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
    if (!doc.isArray()) {
        return;
    }
    
    QJsonArray jsonArray = doc.array();
    todos_.clear();
    
    for (const auto& value : jsonArray) {
        if (!value.isObject()) {
            continue;
        }
        
        QJsonObject obj = value.toObject();
        TodoItem item;
        item.id = obj["id"].toString();
        item.description = obj["description"].toString();
        item.filePath = obj["filePath"].toString();
        item.lineNumber = obj["lineNumber"].toInt();
        item.created = QDateTime::fromString(obj["created"].toString(), Qt::ISODate);
        item.isCompleted = obj["isCompleted"].toBool();
        
        if (item.isCompleted && obj.contains("completed")) {
            item.completed = QDateTime::fromString(obj["completed"].toString(), Qt::ISODate);
        }
        
        todos_.append(item);
    }
}
