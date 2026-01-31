#pragma once


class TodoManager;
struct TodoItem;

class TodoDock : public void {

public:
    explicit TodoDock(TodoManager* todoManager, void* parent = nullptr);
    void initialize();
    
public:
    void refreshTodos();
    

    void openFileRequested(const std::string& filePath, const std::string& todoId);
    
private:
    void onTodoAdded(const TodoItem& todo);
    void onTodoCompleted(const std::string& id);
    void onTodoRemoved(const std::string& id);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onAddTodo();
    void onCompleteTodo();
    void onRemoveTodo();
    void onScanCode();
    
private:
    void setupUI();
    void loadTodos();
    
    QTreeWidget* treeWidget_;
    TodoManager* todoManager_;
};

