// TODO Manager - Production-Ready Task and TODO tracking
// Features: Persistent storage, structured logging, error handling, metrics
#include "todo_manager.h"


#include <iostream>
#include <iomanip>

// Register custom type with Qt meta-type system AFTER class definition
(TodoItem)

// Logging helper with timestamps
static void LogTodoOperation(const std::string& operation, const std::string& details) {
    auto now = std::chrono::system_clock::time_point::currentDateTime();
    std::string timestamp = now.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

TodoManager::TodoManager(void* parent) : void(parent) {
    LogTodoOperation("INIT", "Initializing TodoManager");
    
    try {
        // Ensure configuration directory exists
        std::string configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        std::filesystem::path dir(configPath);
        if (!dir.exists()) {
            if (!dir.mkpath(".")) {
                LogTodoOperation("ERROR", "Failed to create config directory: " + configPath);
                return;
            }
            LogTodoOperation("INFO", "Created config directory: " + configPath);
        }
        
        // Load saved TODOs from persistent storage
        loadTodos();
        LogTodoOperation("LOAD", std::string("Successfully loaded %1 TODO items")));
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", std::string("Initialization failed: %1")));
    }
}

void TodoManager::addTodo(const std::string& description, const std::string& filePath, int lineNumber) {
    try {
        // Validate inputs
        if (description.empty()) {
            LogTodoOperation("WARN", "Attempted to add TODO with empty description");
            return;
        }
        
        TodoItem todo;
        todo.id = QUuid::createUuid().toString();
        todo.description = description;
        todo.filePath = filePath;
        todo.lineNumber = lineNumber;
        todo.created = std::chrono::system_clock::time_point::currentDateTime();
        todo.isCompleted = false;
        
        todos_.append(todo);
        
        // Log with detailed information
        LogTodoOperation("ADD", 
            std::string("Created TODO [%1]: %2 (file=%3, line=%4)")
            ));
        
        todoAdded(todo);
        
        // Persist immediately
        saveTodos();
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", std::string("Failed to add TODO: %1")));
    }
}

void TodoManager::completeTodo(const std::string& id) {
    try {
        // Validate input
        if (id.empty()) {
            LogTodoOperation("WARN", "Attempted to complete TODO with empty ID");
            return;
        }
        
        for (int i = 0; i < todos_.size(); ++i) {
            if (todos_[i].id == id) {
                todos_[i].isCompleted = true;
                todos_[i].completed = std::chrono::system_clock::time_point::currentDateTime();
                
                // Log completion with timestamp
                LogTodoOperation("COMPLETE", 
                    std::string("Completed TODO [%1]: %2 (duration=%3 seconds)")
                    )));
                
                todoCompleted(id);
                
                // Persist immediately
                saveTodos();
                return;
            }
        }
        
        // TODO not found
        LogTodoOperation("WARN", std::string("TODO not found for completion: %1"));
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", std::string("Failed to complete TODO: %1")));
    }
}

void TodoManager::removeTodo(const std::string& id) {
    try {
        // Validate input
        if (id.empty()) {
            LogTodoOperation("WARN", "Attempted to remove TODO with empty ID");
            return;
        }
        
        for (int i = 0; i < todos_.size(); ++i) {
            if (todos_[i].id == id) {
                std::string description = todos_[i].description;
                todos_.removeAt(i);
                
                LogTodoOperation("REMOVE", 
                    std::string("Removed TODO [%1]: %2"));
                
                todoRemoved(id);
                
                // Persist immediately
                saveTodos();
                return;
            }
        }
        
        // TODO not found
        LogTodoOperation("WARN", std::string("TODO not found for removal: %1"));
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", std::string("Failed to remove TODO: %1")));
    }
}

std::vector<TodoItem> TodoManager::getTodos() const {
    LogTodoOperation("QUERY", std::string("Retrieved %1 total TODOs")));
    return todos_;
}

std::vector<TodoItem> TodoManager::getPendingTodos() const {
    std::vector<TodoItem> pending;
    for (const TodoItem& todo : todos_) {
        if (!todo.isCompleted) {
            pending.append(todo);
        }
    }
    LogTodoOperation("QUERY", std::string("Retrieved %1 pending TODOs")));
    return pending;
}

std::vector<TodoItem> TodoManager::getCompletedTodos() const {
    std::vector<TodoItem> completed;
    for (const TodoItem& todo : todos_) {
        if (todo.isCompleted) {
            completed.append(todo);
        }
    }
    LogTodoOperation("QUERY", std::string("Retrieved %1 completed TODOs")));
    return completed;
}

// Production-ready persistence utilities with comprehensive logging
void TodoManager::saveTodos() {
    try {
        // Save TODO items to persistent storage
        void* settings("RawrXD", "AgenticIDE");
        
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
        auto now = std::chrono::system_clock::time_point::currentDateTime();
        std::string timestamp = now.toString("yyyy-MM-dd hh:mm:ss.zzz");
                    );
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", std::string("Failed to save TODOs: %1")));
    }
}

void TodoManager::loadTodos() {
    try {
        // Load TODO items from persistent storage
        void* settings("RawrXD", "AgenticIDE");
        
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
            if (!todo.id.empty() && !todo.description.empty()) {
                todos_.append(todo);
                validCount++;
            } else {
                LogTodoOperation("WARN", std::string("Skipped invalid TODO entry at index %1"));
            }
        }
        settings.endArray();
        
        // Log load operation with statistics
        auto now = std::chrono::system_clock::time_point::currentDateTime();
        std::string timestamp = now.toString("yyyy-MM-dd hh:mm:ss.zzz");
                    ;
        
    } catch (const std::exception& e) {
        LogTodoOperation("ERROR", std::string("Failed to load TODOs: %1")));
    }
}


