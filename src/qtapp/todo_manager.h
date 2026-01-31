#pragma once


struct TodoItem {
    std::string id;
    std::string description;
    std::string filePath;
    int lineNumber;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point completed;
    bool isCompleted;
};

class TodoManager : public void {

public:
    explicit TodoManager(void* parent = nullptr);
    virtual ~TodoManager() = default;
    
    void addTodo(const std::string& description, const std::string& filePath = "", int lineNumber = -1);
    void completeTodo(const std::string& id);
    void removeTodo(const std::string& id);
    std::vector<TodoItem> getTodos() const;
    std::vector<TodoItem> getPendingTodos() const;
    std::vector<TodoItem> getCompletedTodos() const;
    

    void todoAdded(const TodoItem& todo);
    void todoCompleted(const std::string& id);
    void todoRemoved(const std::string& id);
    
private:
    void saveTodos();
    void loadTodos();
    
    std::vector<TodoItem> todos_;
};
