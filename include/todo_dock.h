#pragma once

// C++20 / Win32. Todo dock; no Qt. Use HWND in impl.

#include <string>
#include <functional>

struct TodoItem;
class TodoManager;

class TodoDock
{
public:
    using OpenFileRequestedFn = std::function<void(const std::string& filePath, const std::string& todoId)>;

    explicit TodoDock(TodoManager* todoManager);
    void setOnOpenFileRequested(OpenFileRequestedFn f) { m_onOpenFileRequested = std::move(f); }
    void initialize();
    void refreshTodos();

    void* getWidgetHandle() const { return m_handle; }

private:
    void setupUI();
    void loadTodos();
    void onTodoAdded(const TodoItem& todo);
    void onTodoCompleted(const std::string& id);
    void onTodoRemoved(const std::string& id);

    void* m_handle = nullptr;
    TodoManager* todoManager_ = nullptr;
    OpenFileRequestedFn m_onOpenFileRequested;
};
