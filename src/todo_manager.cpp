// TODO Manager - Production-Ready Task and TODO tracking
// Features: Persistent storage, structured logging, error handling, metrics
#include "todo_manager.h"
#include <QUuid>
#include <QDebug>
#include <QSettings>
#include <QDateTime>
#include <QStandardPaths>
#include <QDir>
#include <QMetaType>
#include <iostream>
#include <iomanip>

// Register custom type with Qt meta-type system AFTER class definition
Q_DECLARE_METATYPE(TodoItem)

// Logging helper with timestamps
static void LogTodoOperation(const QString& operation, const QString& details) {
    auto now = QDateTime::currentDateTime();
    QString timestamp = now.toString("yyyy-MM-dd hh:mm:ss.zzz");
    qDebug() << QString("[%1] [TodoManager] %2 - %3").arg(timestamp, operation, details);
}

TodoManager::TodoManager(QObject* parent) : QObject(parent) {
    LogTodoOperation("INIT", "Initializing TodoManager");
    
    try {
        // Ensure configuration directory exists
        QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir dir(configPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                LogTodoOperation("ERROR", "Failed to create config directory: " + configPath);
                return;
            }
            LogTodoOperation("INFO", "Created config directory: " + configPath);
        }
        
        // Load saved TODOs from persistent storage
        loadTodos();
        LogTodoOperation("LOAD", QString("Successfully loaded %1 TODO items").arg(todos_.size()));
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", QString("Initialization failed: %1").arg(e.what()));
    }
}

void TodoManager::addTodo(const QString& description, const QString& filePath, int lineNumber) {
    try {
        // Validate inputs
        if (description.isEmpty()) {
            LogTodoOperation("WARN", "Attempted to add TODO with empty description");
            return;
        }
        
        TodoItem todo;
        todo.id = QUuid::createUuid().toString();
        todo.description = description;
        todo.filePath = filePath;
        todo.lineNumber = lineNumber;
        todo.created = QDateTime::currentDateTime();
        todo.isCompleted = false;
        
        todos_.append(todo);
        
        // Log with detailed information
        LogTodoOperation("ADD", 
            QString("Created TODO [%1]: %2 (file=%3, line=%4)")
            .arg(todo.id, description, filePath, QString::number(lineNumber)));
        
        emit todoAdded(todo);
        
        // Persist immediately
        saveTodos();
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", QString("Failed to add TODO: %1").arg(e.what()));
    }
}

void TodoManager::completeTodo(const QString& id) {
    try {
        // Validate input
        if (id.isEmpty()) {
            LogTodoOperation("WARN", "Attempted to complete TODO with empty ID");
            return;
        }
        
        for (int i = 0; i < todos_.size(); ++i) {
            if (todos_[i].id == id) {
                todos_[i].isCompleted = true;
                todos_[i].completed = QDateTime::currentDateTime();
                
                // Log completion with timestamp
                LogTodoOperation("COMPLETE", 
                    QString("Completed TODO [%1]: %2 (duration=%3 seconds)")
                    .arg(id, todos_[i].description, 
                    QString::number(todos_[i].created.secsTo(todos_[i].completed))));
                
                emit todoCompleted(id);
                
                // Persist immediately
                saveTodos();
                return;
            }
        }
        
        // TODO not found
        LogTodoOperation("WARN", QString("TODO not found for completion: %1").arg(id));
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", QString("Failed to complete TODO: %1").arg(e.what()));
    }
}

void TodoManager::removeTodo(const QString& id) {
    try {
        // Validate input
        if (id.isEmpty()) {
            LogTodoOperation("WARN", "Attempted to remove TODO with empty ID");
            return;
        }
        
        for (int i = 0; i < todos_.size(); ++i) {
            if (todos_[i].id == id) {
                QString description = todos_[i].description;
                todos_.removeAt(i);
                
                LogTodoOperation("REMOVE", 
                    QString("Removed TODO [%1]: %2").arg(id, description));
                
                emit todoRemoved(id);
                
                // Persist immediately
                saveTodos();
                return;
            }
        }
        
        // TODO not found
        LogTodoOperation("WARN", QString("TODO not found for removal: %1").arg(id));
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", QString("Failed to remove TODO: %1").arg(e.what()));
    }
}

QList<TodoItem> TodoManager::getTodos() const {
    LogTodoOperation("QUERY", QString("Retrieved %1 total TODOs").arg(todos_.size()));
    return todos_;
}

QList<TodoItem> TodoManager::getPendingTodos() const {
    QList<TodoItem> pending;
    for (const TodoItem& todo : todos_) {
        if (!todo.isCompleted) {
            pending.append(todo);
        }
    }
    LogTodoOperation("QUERY", QString("Retrieved %1 pending TODOs").arg(pending.size()));
    return pending;
}

QList<TodoItem> TodoManager::getCompletedTodos() const {
    QList<TodoItem> completed;
    for (const TodoItem& todo : todos_) {
        if (todo.isCompleted) {
            completed.append(todo);
        }
    }
    LogTodoOperation("QUERY", QString("Retrieved %1 completed TODOs").arg(completed.size()));
    return completed;
}

// Production-ready persistence utilities with comprehensive logging
void TodoManager::saveTodos() {
    try {
        // Save TODO items to persistent storage
        QSettings settings("RawrXD", "AgenticIDE");
        
        // Clear previous data
        settings.remove("todos");
        
        // Write array with proper indexing
        settings.beginWriteArray("todos", todos_.size());
        for (int i = 0; i < todos_.size(); ++i) {
            settings.setArrayIndex(i);
            const TodoItem& todo = todos_[i];
            
            settings.setValue("id", todo.id);
            settings.setValue("description", todo.description);
            settings.setValue("filePath", todo.filePath);
            settings.setValue("lineNumber", todo.lineNumber);
            settings.setValue("created", todo.created);
            settings.setValue("completed", todo.completed);
            settings.setValue("isCompleted", todo.isCompleted);
        }
        settings.endArray();
        
        // Ensure settings are synced to disk
        settings.sync();
        
        // Log save operation with timestamp
        auto now = QDateTime::currentDateTime();
        QString timestamp = now.toString("yyyy-MM-dd hh:mm:ss.zzz");
        qDebug() << QString("[%1] [TodoManager] PERSIST - Saved %2 TODO items to persistent storage")
                    .arg(timestamp).arg(todos_.size());
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", QString("Failed to save TODOs: %1").arg(e.what()));
    }
}

void TodoManager::loadTodos() {
    try {
        // Load TODO items from persistent storage
        QSettings settings("RawrXD", "AgenticIDE");
        
        int size = settings.beginReadArray("todos");
        todos_.clear();
        todos_.reserve(size);
        
        int validCount = 0;
        for (int i = 0; i < size; ++i) {
            settings.setArrayIndex(i);
            
            TodoItem todo;
            todo.id = settings.value("id").toString();
            todo.description = settings.value("description").toString();
            todo.filePath = settings.value("filePath").toString();
            todo.lineNumber = settings.value("lineNumber", -1).toInt();
            todo.created = settings.value("created").toDateTime();
            todo.completed = settings.value("completed").toDateTime();
            todo.isCompleted = settings.value("isCompleted", false).toBool();
            
            // Validate loaded data
            if (!todo.id.isEmpty() && !todo.description.isEmpty()) {
                todos_.append(todo);
                validCount++;
            } else {
                LogTodoOperation("WARN", QString("Skipped invalid TODO entry at index %1").arg(i));
            }
        }
        settings.endArray();
        
        // Log load operation with statistics
        auto now = QDateTime::currentDateTime();
        QString timestamp = now.toString("yyyy-MM-dd hh:mm:ss.zzz");
        qDebug() << QString("[%1] [TodoManager] LOAD - Loaded %2 TODO items from persistent storage (valid=%3, skipped=%4)")
                    .arg(timestamp).arg(size).arg(validCount).arg(size - validCount);
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", QString("Failed to load TODOs: %1").arg(e.what()));
    }
}
