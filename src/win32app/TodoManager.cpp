// Win32 Todo Integration Implementation
// Bridges PowerShell todo system with Win32IDE

#include "TodoManager.h"
#include <shlobj.h>
#include <fstream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <ctime>
#include <iomanip>

namespace RawrXD {
namespace Todos {

// Resolve %APPDATA%\RawrXD (create dir); return empty on failure.
static std::string getRawrXDAppDataDir() {
    char buf[MAX_PATH] = {};
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buf) != S_OK)
        return {};
    std::string dir = std::string(buf) + "\\RawrXD";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

// Default script path for PowerShell todo integration (optional).
static std::string getTodoScriptPath() {
    const char* env = getenv("RAWRXD_TODO_SCRIPT");
    if (env && env[0]) return env;
    std::string dir = getRawrXDAppDataDir();
    if (dir.empty()) return {};
    CreateDirectoryA((dir + "\\scripts").c_str(), nullptr);
    return dir + "\\scripts\\todo_manager.ps1";
}

// ═══════════════════════════════════════════════════════════════════════════════
// TodoManager Implementation
// ═══════════════════════════════════════════════════════════════════════════════

TodoManager::TodoManager(const std::string& storagePath)
    : storagePath_(storagePath)
    , maxItems_(25)
    , pipeHandle_(INVALID_HANDLE_VALUE)
    , watchThread_(NULL)
    , stopWatch_(false) {
    if (storagePath_.empty()) {
        std::string dir = getRawrXDAppDataDir();
        storagePath_ = dir.empty() ? "todos.json" : (dir + "\\todos.json");
    }
    stats_.totalCreated = 0;
    stats_.totalCompleted = 0;
    stats_.totalDeleted = 0;
    stats_.agenticCreated = 0;
    stats_.userCreated = 0;
    stats_.parsedCreated = 0;
    
    Load();
}

TodoManager::~TodoManager() {
    StopPipeServer();
}

bool TodoManager::Load() {
    std::ifstream file(storagePath_);
    if (!file.is_open()) {
        return false;
    }
    
    try {
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
        json data = json::parse(content);
        
        items_.clear();
        
        if (data.contains("Items")) {
            for (const auto& itemJson : data["Items"]) {
                items_.push_back(ParseTodoFromJson(itemJson));
            }
        }
        
        if (data.contains("Statistics")) {
            const auto& statsJson = data["Statistics"];
            stats_.totalCreated = statsJson.value("TotalCreated", 0);
            stats_.totalCompleted = statsJson.value("TotalCompleted", 0);
            stats_.totalDeleted = statsJson.value("TotalDeleted", 0);
            stats_.agenticCreated = statsJson.value("AgenticCreated", 0);
            stats_.userCreated = statsJson.value("UserCreated", 0);
            stats_.parsedCreated = statsJson.value("ParsedCreated", 0);
        }
        
        return true;
    }
    catch (const std::exception& e) {
        return false;
    }
}

bool TodoManager::Save() {
    json data;
    data["Version"] = "1.0.0";
    data["MaxItems"] = maxItems_;
    
    // Statistics
    data["Statistics"]["TotalCreated"] = stats_.totalCreated;
    data["Statistics"]["TotalCompleted"] = stats_.totalCompleted;
    data["Statistics"]["TotalDeleted"] = stats_.totalDeleted;
    data["Statistics"]["AgenticCreated"] = stats_.agenticCreated;
    data["Statistics"]["UserCreated"] = stats_.userCreated;
    data["Statistics"]["ParsedCreated"] = stats_.parsedCreated;
    
    // Items
    data["Items"] = json::array();
    for (const auto& todo : items_) {
        data["Items"].push_back(TodoToJson(todo));
    }
    
    // Get current time
    auto now = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    data["SavedAt"] = oss.str();
    
    std::ofstream file(storagePath_);
    if (!file.is_open()) {
        return false;
    }
    
    file << data.dump(2);
    return true;
}

bool TodoManager::Reload() {
    return Load();
}

bool TodoManager::AddTodo(const std::string& text, const std::string& priority, const std::string& source) {
    if (!CanAdd()) {
        return false;
    }
    
    TodoItem todo;
    todo.id = GetNextId();
    todo.text = text;
    todo.priority = priority;
    todo.status = "pending";
    todo.category = "general";
    todo.source = source;
    
    auto now = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    todo.createdAt = oss.str();
    todo.updatedAt = oss.str();
    
    items_.push_back(todo);
    stats_.totalCreated++;
    
    if (source == "agentic") stats_.agenticCreated++;
    else if (source == "user") stats_.userCreated++;
    else if (source == "parsed") stats_.parsedCreated++;
    
    Save();
    SendUpdateNotification();
    
    return true;
}

bool TodoManager::CompleteTodo(int id) {
    auto* todo = GetById(id);
    if (!todo) {
        return false;
    }
    
    todo->status = "completed";
    
    auto now = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    todo->updatedAt = oss.str();
    
    stats_.totalCompleted++;
    
    Save();
    SendUpdateNotification();
    
    return true;
}

bool TodoManager::UpdateTodo(int id, const json& updates) {
    auto* todo = GetById(id);
    if (!todo) {
        return false;
    }
    
    if (updates.contains("text") && updates["text"].is_string()) {
        todo->text = updates["text"].get<std::string>();
    }
    if (updates.contains("priority") && updates["priority"].is_string()) {
        todo->priority = updates["priority"].get<std::string>();
    }
    if (updates.contains("status") && updates["status"].is_string()) {
        todo->status = updates["status"].get<std::string>();
    }
    if (updates.contains("category") && updates["category"].is_string()) {
        todo->category = updates["category"].get<std::string>();
    }
    
    auto now = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    todo->updatedAt = oss.str();
    
    Save();
    SendUpdateNotification();
    
    return true;
}

bool TodoManager::DeleteTodo(int id) {
    auto it = std::find_if(items_.begin(), items_.end(),
        [id](const TodoItem& todo) { return todo.id == id; });
    
    if (it == items_.end()) {
        return false;
    }
    
    items_.erase(it);
    stats_.totalDeleted++;
    
    Save();
    SendUpdateNotification();
    
    return true;
}

bool TodoManager::ClearAll() {
    items_.clear();
    Save();
    SendUpdateNotification();
    return true;
}

std::vector<TodoItem> TodoManager::ParseCommand(const std::string& command) {
    std::vector<TodoItem> todos;
    
    // Use PowerShell parser for consistency
    std::vector<std::string> args = {
        "-Operation", "parse",
        "-Command", "\"" + command + "\""
    };
    
    // Execute PowerShell command and parse output
    // In real implementation, capture stdout and parse results
    
    // Fallback: C++ parsing
    auto texts = TodoCommandParser::Parse(command);
    for (const auto& text : texts) {
        TodoItem todo;
        todo.text = text;
        todo.source = "parsed";
        todo.priority = "Medium";
        todo.status = "pending";
        todos.push_back(todo);
    }
    
    return todos;
}

std::vector<TodoItem> TodoManager::CreateAgenticTodos(const std::string& context) {
    AgenticTodoCreator creator(context);
    return creator.Generate();
}

std::vector<TodoItem> TodoManager::GetByStatus(const std::string& status) const {
    std::vector<TodoItem> result;
    std::copy_if(items_.begin(), items_.end(), std::back_inserter(result),
        [&status](const TodoItem& todo) { return todo.status == status; });
    return result;
}

TodoItem* TodoManager::GetById(int id) {
    auto it = std::find_if(items_.begin(), items_.end(),
        [id](const TodoItem& todo) { return todo.id == id; });
    return it != items_.end() ? &(*it) : nullptr;
}

bool TodoManager::StartPipeServer() {
    if (pipeHandle_ != INVALID_HANDLE_VALUE) {
        return true;
    }
    
    std::wstring pipeName = L"\\\\.\\pipe\\RawrXD_Todos";
    
    pipeHandle_ = CreateNamedPipeW(
        pipeName.c_str(),
        PIPE_ACCESS_DUPLEX,
        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
        1,
        4096,
        4096,
        0,
        NULL
    );
    
    if (pipeHandle_ == INVALID_HANDLE_VALUE) {
        return false;
    }
    
    // Start server thread
    watchThread_ = CreateThread(NULL, 0, PipeServerThreadProc, this, 0, NULL);
    
    return true;
}

void TodoManager::StopPipeServer() {
    stopWatch_ = true;
    
    if (pipeHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(pipeHandle_);
        pipeHandle_ = INVALID_HANDLE_VALUE;
    }
    
    if (watchThread_ != NULL) {
        WaitForSingleObject(watchThread_, 1000);
        CloseHandle(watchThread_);
        watchThread_ = NULL;
    }
}

bool TodoManager::SendUpdateNotification() {
    if (updateCallback_) {
        updateCallback_();
    }
    return true;
}

int TodoManager::GetNextId() const {
    if (items_.empty()) {
        return 1;
    }
    
    int maxId = 0;
    for (const auto& todo : items_) {
        if (todo.id > maxId) {
            maxId = todo.id;
        }
    }
    
    return maxId + 1;
}

TodoItem TodoManager::ParseTodoFromJson(const json& j) {
    TodoItem todo;
    todo.id = j.value("Id", 0);
    todo.text = j.value("Text", "");
    todo.priority = j.value("Priority", "Medium");
    todo.status = j.value("Status", "pending");
    todo.category = j.value("Category", "general");
    todo.source = j.value("Source", "user");
    todo.createdAt = j.value("CreatedAt", "");
    todo.updatedAt = j.value("UpdatedAt", "");
    todo.estimatedMinutes = j.value("EstimatedMinutes", 0);
    todo.actualMinutes = j.value("ActualMinutes", 0);
    
    if (j.contains("Tags") && j["Tags"].is_array()) {
        for (const auto& tag : j["Tags"]) {
            todo.tags.push_back(tag.get<std::string>());
        }
    }
    
    return todo;
}

json TodoManager::TodoToJson(const TodoItem& todo) const {
    json j;
    j["Id"] = todo.id;
    j["Text"] = todo.text;
    j["Priority"] = todo.priority;
    j["Status"] = todo.status;
    j["Category"] = todo.category;
    j["Source"] = todo.source;
    j["CreatedAt"] = todo.createdAt;
    j["UpdatedAt"] = todo.updatedAt;
    j["Tags"] = todo.tags;
    j["EstimatedMinutes"] = todo.estimatedMinutes;
    j["ActualMinutes"] = todo.actualMinutes;
    return j;
}

bool TodoManager::ExecutePowerShellCommand(const std::string& operation, const std::vector<std::string>& args) {
    std::string scriptPath = getTodoScriptPath();
    if (scriptPath.empty() || GetFileAttributesA(scriptPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        return false;
    std::ostringstream cmd;
    cmd << "powershell.exe -ExecutionPolicy Bypass -File \"" << scriptPath << "\" ";
    cmd << "-Operation " << operation;
    
    for (const auto& arg : args) {
        cmd << " " << arg;
    }
    
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };
    
    if (!CreateProcessA(NULL, const_cast<char*>(cmd.str().c_str()), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        return false;
    }
    
    WaitForSingleObject(pi.hProcess, INFINITE);
    
    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    
    return exitCode == 0;
}

DWORD WINAPI TodoManager::PipeServerThreadProc(LPVOID param) {
    auto* manager = static_cast<TodoManager*>(param);
    
    while (!manager->stopWatch_) {
        BOOL connected = ConnectNamedPipe(manager->pipeHandle_, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        
        if (connected) {
            char buffer[4096];
            DWORD bytesRead;
            constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buffer) - 1);
            
            if (ReadFile(manager->pipeHandle_, buffer, kMaxChunk, &bytesRead, NULL) && bytesRead > 0) {
                const size_t safeBytes = (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                buffer[safeBytes] = '\0';
                
                // Process command from PowerShell
                try {
                    json command = json::parse(buffer);
                    // Handle commands...
                }
                catch (...) {
                    // Invalid JSON
                }
            }
            
            DisconnectNamedPipe(manager->pipeHandle_);
        }
        
        Sleep(100);
    }
    
    return 0;
}

DWORD WINAPI TodoManager::FileWatchThreadProc(LPVOID param) {
    auto* manager = static_cast<TodoManager*>(param);
    std::string watchDir = manager->storagePath_;
    size_t sep = watchDir.find_last_of("/\\");
    if (sep != std::string::npos)
        watchDir.resize(sep);
    else
        watchDir = ".";

    HANDLE hDir = CreateFileA(
        watchDir.c_str(),
        FILE_LIST_DIRECTORY,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        NULL,
        OPEN_EXISTING,
        FILE_FLAG_BACKUP_SEMANTICS,
        NULL
    );
    
    if (hDir == INVALID_HANDLE_VALUE) {
        return 1;
    }
    
    char buffer[4096];
    DWORD bytesReturned;
    
    while (!manager->stopWatch_) {
        if (ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), FALSE,
            FILE_NOTIFY_CHANGE_LAST_WRITE, &bytesReturned, NULL, NULL)) {
            
            manager->Reload();
            manager->SendUpdateNotification();
        }
    }
    
    CloseHandle(hDir);
    return 0;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TodoCommandParser Implementation
// ═══════════════════════════════════════════════════════════════════════════════

std::vector<std::string> TodoCommandParser::Parse(const std::string& command) {
    // Try numbered list first
    auto todos = ParseNumberedList(command);
    if (!todos.empty()) {
        return todos;
    }
    
    // Try bullet list
    todos = ParseBulletList(command);
    if (!todos.empty()) {
        return todos;
    }
    
    // Try comma-separated
    return ParseCommaSeparated(command);
}

std::vector<std::string> TodoCommandParser::ParseNumberedList(const std::string& text) {
    std::vector<std::string> todos;
    std::regex pattern(R"(\d+\.\s*([^0-9]+?)(?=\d+\.|$))");
    
    auto begin = std::sregex_iterator(text.begin(), text.end(), pattern);
    auto end = std::sregex_iterator();
    
    for (auto it = begin; it != end; ++it) {
        std::string todo = (*it)[1].str();
        // Trim whitespace
        todo.erase(0, todo.find_first_not_of(" \t\r\n"));
        todo.erase(todo.find_last_not_of(" \t\r\n") + 1);
        
        if (!todo.empty()) {
            todos.push_back(todo);
        }
    }
    
    return todos;
}

std::vector<std::string> TodoCommandParser::ParseBulletList(const std::string& text) {
    std::vector<std::string> todos;
    std::regex pattern(R"([-•]\s*([^-•]+))");
    
    auto begin = std::sregex_iterator(text.begin(), text.end(), pattern);
    auto end = std::sregex_iterator();
    
    for (auto it = begin; it != end; ++it) {
        std::string todo = (*it)[1].str();
        todo.erase(0, todo.find_first_not_of(" \t\r\n"));
        todo.erase(todo.find_last_not_of(" \t\r\n") + 1);
        
        if (!todo.empty()) {
            todos.push_back(todo);
        }
    }
    
    return todos;
}

std::vector<std::string> TodoCommandParser::ParseCommaSeparated(const std::string& text) {
    std::vector<std::string> todos;
    std::istringstream stream(text);
    std::string todo;
    
    while (std::getline(stream, todo, ',')) {
        todo.erase(0, todo.find_first_not_of(" \t\r\n"));
        todo.erase(todo.find_last_not_of(" \t\r\n") + 1);
        
        if (!todo.empty()) {
            todos.push_back(todo);
        }
    }
    
    return todos;
}

// ═══════════════════════════════════════════════════════════════════════════════
// AgenticTodoCreator Implementation
// ═══════════════════════════════════════════════════════════════════════════════

AgenticTodoCreator::AgenticTodoCreator(const std::string& context)
    : context_(context) {
}

std::vector<TodoItem> AgenticTodoCreator::Generate() {
    std::vector<TodoItem> todos;
    std::string type = DetectType();
    
    auto templates = GetTemplateForType(type);
    
    for (const auto& text : templates) {
        TodoItem todo;
        todo.text = text;
        todo.source = "agentic";
        todo.category = type;
        todo.priority = "Medium";
        todo.status = "pending";
        todos.push_back(todo);
    }
    
    // Add context-specific todos
    auto contextTodos = GenerateContextSpecific();
    todos.insert(todos.end(), contextTodos.begin(), contextTodos.end());
    
    return todos;
}

std::string AgenticTodoCreator::DetectType() const {
    std::string lower = context_;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
    if (lower.find("api") != std::string::npos || lower.find("rest") != std::string::npos) return "api";
    if (lower.find("bug") != std::string::npos || lower.find("fix") != std::string::npos) return "bug";
    if (lower.find("refactor") != std::string::npos) return "refactor";
    if (lower.find("model") != std::string::npos || lower.find("train") != std::string::npos) return "model";
    
    return "feature";
}

std::vector<std::string> AgenticTodoCreator::GetTemplateForType(const std::string& type) const {
    if (type == "api") {
        return {
            "Design API endpoints and routes",
            "Implement request/response models",
            "Add authentication and authorization",
            "Write API documentation",
            "Add error handling and validation",
            "Create unit tests for API"
        };
    }
    else if (type == "bug") {
        return {
            "Reproduce and document bug",
            "Identify root cause",
            "Implement fix",
            "Add regression test",
            "Verify fix in all scenarios"
        };
    }
    else if (type == "model") {
        return {
            "Define model architecture",
            "Prepare training dataset",
            "Implement training pipeline",
            "Run training and validation",
            "Evaluate model performance",
            "Deploy model to production"
        };
    }
    else {
        return {
            "Analyze requirements",
            "Design architecture",
            "Implement core functionality",
            "Write tests",
            "Update documentation",
            "Perform code review"
        };
    }
}

std::vector<TodoItem> AgenticTodoCreator::GenerateContextSpecific() const {
    std::vector<TodoItem> todos;
    
    // Extract specific items from context using regex
    std::regex implementRegex(R"(implement (\w+))", std::regex::icase);
    std::smatch match;
    
    if (std::regex_search(context_, match, implementRegex)) {
        TodoItem todo;
        todo.text = "Implement " + match[1].str() + " functionality";
        todo.source = "agentic";
        todo.priority = "High";
        todo.status = "pending";
        todos.push_back(todo);
    }
    
    return todos;
}

} // namespace Todos
} // namespace RawrXD
