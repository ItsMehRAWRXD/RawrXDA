#pragma once

// C++20, no Qt. Todo list with callbacks.

#include <string>
#include <vector>
#include <chrono>
#include <functional>

struct TodoItem {
    std::string id;
    std::string description;
    std::string filePath;
    int lineNumber = -1;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point completed;
    bool isCompleted = false;
};

class TodoManager
{
public:
    using TodoAddedFn    = std::function<void(const TodoItem&)>;
    using TodoCompletedFn = std::function<void(const std::string& id)>;
    using TodoRemovedFn   = std::function<void(const std::string& id)>;

    TodoManager() = default;
    virtual ~TodoManager() = default;

    void setOnTodoAdded(TodoAddedFn f)    { m_onAdded = std::move(f); }
    void setOnTodoCompleted(TodoCompletedFn f) { m_onCompleted = std::move(f); }
    void setOnTodoRemoved(TodoRemovedFn f) { m_onRemoved = std::move(f); }

    void addTodo(const std::string& description, const std::string& filePath = "", int lineNumber = -1);
    void completeTodo(const std::string& id);
    void removeTodo(const std::string& id);

    std::vector<TodoItem> getTodos() const;
    std::vector<TodoItem> getPendingTodos() const;
    std::vector<TodoItem> getCompletedTodos() const;

private:
    void saveTodos();
    void loadTodos();

    std::vector<TodoItem> todos_;
    TodoAddedFn    m_onAdded;
    TodoCompletedFn m_onCompleted;
    TodoRemovedFn   m_onRemoved;
};
