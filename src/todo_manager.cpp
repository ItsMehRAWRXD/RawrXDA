#include "todo_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <random>

using json = nlohmann::json;

namespace RawrXD {

// Helper for UUID generation
static std::string generateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    ss << std::hex;
    for (int i = 0; i < 8; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 4; i++) ss << dis(gen);
    ss << "-4"; // UUID version 4
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    ss << dis2(gen); // Variant
    for (int i = 0; i < 3; i++) ss << dis(gen);
    ss << "-";
    for (int i = 0; i < 12; i++) ss << dis(gen);
    return ss.str();
}

TodoManager::TodoManager(void* parent) {
    // Determine config path
    char* appdata = nullptr;
    size_t len = 0;
    if (_dupenv_s(&appdata, &len, "APPDATA") == 0 && appdata != nullptr) {
        std::filesystem::path dir(appdata);
        dir /= "RawrXD";
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
        }
        configPath_ = (dir / "todos.json").string();
        free(appdata);
    } else {
        configPath_ = "todos.json"; // Fallback to CWD
    }
    
    loadTodos();
}

TodoManager::~TodoManager() {
    saveTodos();
}

void TodoManager::addTodo(const std::string& description, const std::string& filePath, int lineNumber) {
    if (description.empty()) return;

    TodoItem todo;
    todo.id = generateUUID();
    todo.description = description;
    todo.filePath = filePath;
    todo.lineNumber = lineNumber;
    todo.created = std::chrono::system_clock::now();
    todo.isCompleted = false;

    todos_.push_back(todo);
    saveTodos();

    if (onTodoAdded) onTodoAdded(todo);
}

void TodoManager::completeTodo(const std::string& id) {
    for (auto& todo : todos_) {
        if (todo.id == id) {
            todo.isCompleted = true;
            todo.completed = std::chrono::system_clock::now();
            saveTodos();
            if (onTodoCompleted) onTodoCompleted(id);
            return;
        }
    }
}

void TodoManager::removeTodo(const std::string& id) {
    auto it = std::remove_if(todos_.begin(), todos_.end(), 
        [&id](const TodoItem& item) { return item.id == id; });

    if (it != todos_.end()) {
        todos_.erase(it, todos_.end());
        saveTodos();
        if (onTodoRemoved) onTodoRemoved(id);
    }
}

std::vector<TodoItem> TodoManager::getTodos() const {
    return todos_;
}

std::vector<TodoItem> TodoManager::getPendingTodos() const {
    std::vector<TodoItem> pending;
    for (const auto& todo : todos_) {
        if (!todo.isCompleted) pending.push_back(todo);
    }
    return pending;
}

std::vector<TodoItem> TodoManager::getCompletedTodos() const {
    std::vector<TodoItem> completed;
    for (const auto& todo : todos_) {
        if (todo.isCompleted) completed.push_back(todo);
    }
    return completed;
}

void TodoManager::saveTodos() {
    json j_list = json::array();
    for (const auto& todo : todos_) {
        json j;
        j["id"] = todo.id;
        j["description"] = todo.description;
        j["filePath"] = todo.filePath;
        j["lineNumber"] = todo.lineNumber;
        j["isCompleted"] = todo.isCompleted;
        j["created"] = std::chrono::duration_cast<std::chrono::seconds>(todo.created.time_since_epoch()).count();
        j["completed"] = std::chrono::duration_cast<std::chrono::seconds>(todo.completed.time_since_epoch()).count();
        j_list.push_back(j);
    }

    std::ofstream f(configPath_);
    if (f.is_open()) {
        f << j_list.dump(4);
    }
}

void TodoManager::loadTodos() {
    todos_.clear();
    std::ifstream f(configPath_);
    if (!f.is_open()) return;

    try {
        json j_list;
        f >> j_list;

        if (j_list.is_array()) {
            for (const auto& j : j_list) {
                TodoItem todo;
                todo.id = j.value("id", "");
                todo.description = j.value("description", "");
                todo.filePath = j.value("filePath", "");
                todo.lineNumber = j.value("lineNumber", -1);
                todo.isCompleted = j.value("isCompleted", false);
                
                long long created_ts = j.value("created", 0LL);
                long long completed_ts = j.value("completed", 0LL);
                
                todo.created = std::chrono::system_clock::time_point(std::chrono::seconds(created_ts));
                todo.completed = std::chrono::system_clock::time_point(std::chrono::seconds(completed_ts));

                todos_.push_back(todo);
            }
        }
    } catch (...) {
        // Corrupt file or parse error, start fresh
    }
}

} // namespace RawrXD



