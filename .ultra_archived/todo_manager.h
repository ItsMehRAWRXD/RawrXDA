#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <functional>
#include <nlohmann/json.hpp>

namespace RawrXD {

struct TodoItem {
    std::string id;
    std::string description;
    std::string filePath;
    int lineNumber;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point completed;
    bool isCompleted;
};

class TodoManager {
public:
    explicit TodoManager(void* parent = nullptr);
    virtual ~TodoManager() = default;
    
    void addTodo(const std::string& description, const std::string& filePath = "", int lineNumber = -1);
    void completeTodo(const std::string& id);
    void removeTodo(const std::string& id);
    
    std::vector<TodoItem> getTodos() const;
    std::vector<TodoItem> getPendingTodos() const;
    std::vector<TodoItem> getCompletedTodos() const;

    // Callbacks/Signals
    std::function<void(const TodoItem&)> onTodoAdded;
    std::function<void(const std::string&)> onTodoCompleted;
    std::function<void(const std::string&)> onTodoRemoved;

private:
    void saveTodos();
    void loadTodos();
    
    std::vector<TodoItem> todos_;
    std::string configPath_;
};

} // namespace RawrXD
