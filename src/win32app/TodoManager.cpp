// Win32 Todo Integration Implementation
// Bridges PowerShell todo system with Win32IDE

#include "TodoManager.h"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <regex>
#include <shlobj.h>
#include <sstream>

namespace
{
constexpr size_t kMaxTodoStorageBytes = 2u * 1024u * 1024u;
constexpr size_t kMaxTodoTextBytes = 4096;
constexpr size_t kMaxTodoFieldBytes = 128;
constexpr size_t kMaxTodoTags = 16;
constexpr size_t kMaxTodoTagBytes = 64;
constexpr size_t kMaxLoadedTodoItems = 512;

std::string clampField(const std::string& value, size_t maxBytes)
{
    if (value.size() <= maxBytes)
    {
        return value;
    }
    return value.substr(0, maxBytes);
}

int clampNonNegativeInt(int value, int maxValue)
{
    if (value < 0)
    {
        return 0;
    }
    return std::min(value, maxValue);
}

std::string toLowerCopy(const std::string& s)
{
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return out;
}

std::string readOpString(const json& cmd, const char* keyLower, const char* keyUpper)
{
    if (cmd.contains(keyLower) && cmd[keyLower].is_string())
    {
        return cmd[keyLower].get<std::string>();
    }
    if (cmd.contains(keyUpper) && cmd[keyUpper].is_string())
    {
        return cmd[keyUpper].get<std::string>();
    }
    return {};
}
}  // namespace

namespace RawrXD
{
namespace Todos
{

// Resolve %APPDATA%\RawrXD (create dir); return empty on failure.
static std::string getRawrXDAppDataDir()
{
    char buf[MAX_PATH] = {};
    if (SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buf) != S_OK)
        return {};
    std::string dir = std::string(buf) + "\\RawrXD";
    CreateDirectoryA(dir.c_str(), nullptr);
    return dir;
}

// Default script path for PowerShell todo integration (optional).
static std::string getTodoScriptPath()
{
    const char* env = getenv("RAWRXD_TODO_SCRIPT");
    if (env && env[0] && GetFileAttributesA(env) != INVALID_FILE_ATTRIBUTES)
        return env;

    char modulePath[MAX_PATH] = {};
    if (GetModuleFileNameA(nullptr, modulePath, MAX_PATH) > 0)
    {
        std::string exePath = modulePath;
        const size_t slash = exePath.find_last_of("\\/");
        const std::string exeDir = (slash == std::string::npos) ? std::string(".") : exePath.substr(0, slash);

        std::vector<std::string> candidates = {
            exeDir + "\\scripts\\todo_manager.ps1", exeDir + "\\..\\scripts\\todo_manager.ps1",
            exeDir + "\\..\\..\\scripts\\todo_manager.ps1", "d:\\rawrxd\\scripts\\todo_manager.ps1"};

        for (const auto& candidate : candidates)
        {
            if (GetFileAttributesA(candidate.c_str()) != INVALID_FILE_ATTRIBUTES)
            {
                return candidate;
            }
        }
    }

    std::string dir = getRawrXDAppDataDir();
    if (dir.empty())
        return {};
    CreateDirectoryA((dir + "\\scripts").c_str(), nullptr);
    return dir + "\\scripts\\todo_manager.ps1";
}

// ═══════════════════════════════════════════════════════════════════════════════
// TodoManager Implementation
// ═══════════════════════════════════════════════════════════════════════════════

TodoManager::TodoManager(const std::string& storagePath)
    : storagePath_(storagePath), maxItems_(25), pipeHandle_(INVALID_HANDLE_VALUE), watchThread_(NULL), stopWatch_(false)
{
    if (storagePath_.empty())
    {
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

TodoManager::~TodoManager()
{
    StopPipeServer();
}

bool TodoManager::Load()
{
    std::ifstream file(storagePath_, std::ios::binary | std::ios::ate);
    if (!file.is_open())
    {
        return false;
    }

    try
    {
        const std::streamoff fileSize = file.tellg();
        if (fileSize < 0 || static_cast<size_t>(fileSize) > kMaxTodoStorageBytes)
        {
            return false;
        }
        file.seekg(0, std::ios::beg);
        std::string content(static_cast<size_t>(fileSize), '\0');
        if (!content.empty())
        {
            file.read(content.data(), static_cast<std::streamsize>(content.size()));
            if (!file)
            {
                return false;
            }
        }
        json data = json::parse(content);

        items_.clear();

        if (data.contains("Items") && data["Items"].is_array())
        {
            size_t loadedCount = 0;
            for (const auto& itemJson : data["Items"])
            {
                if (loadedCount++ >= kMaxLoadedTodoItems)
                {
                    break;
                }
                items_.push_back(ParseTodoFromJson(itemJson));
            }
        }

        if (data.contains("Statistics"))
        {
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
    catch (const std::exception& e)
    {
        return false;
    }
}

bool TodoManager::Save()
{
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
    for (const auto& todo : items_)
    {
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
    if (!file.is_open())
    {
        return false;
    }

    file << data.dump(2);
    return true;
}

bool TodoManager::Reload()
{
    return Load();
}

bool TodoManager::AddTodo(const std::string& text, const std::string& priority, const std::string& source)
{
    if (!CanAdd())
    {
        return false;
    }

    if (text.empty() || text.size() > kMaxTodoTextBytes || priority.size() > kMaxTodoFieldBytes ||
        source.size() > kMaxTodoFieldBytes)
    {
        return false;
    }

    TodoItem todo;
    todo.id = GetNextId();
    todo.text = clampField(text, kMaxTodoTextBytes);
    todo.priority = clampField(priority, kMaxTodoFieldBytes);
    todo.status = "pending";
    todo.category = "general";
    todo.source = clampField(source, kMaxTodoFieldBytes);

    auto now = std::time(nullptr);
    std::tm tm;
    localtime_s(&tm, &now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    todo.createdAt = oss.str();
    todo.updatedAt = oss.str();

    items_.push_back(todo);
    stats_.totalCreated++;

    if (source == "agentic")
        stats_.agenticCreated++;
    else if (source == "user")
        stats_.userCreated++;
    else if (source == "parsed")
        stats_.parsedCreated++;

    Save();
    SendUpdateNotification();

    return true;
}

bool TodoManager::CompleteTodo(int id)
{
    auto* todo = GetById(id);
    if (!todo)
    {
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

bool TodoManager::UpdateTodo(int id, const json& updates)
{
    auto* todo = GetById(id);
    if (!todo)
    {
        return false;
    }

    const std::string previousStatus = todo->status;

    if (updates.contains("text") && updates["text"].is_string())
    {
        todo->text = clampField(updates["text"].get<std::string>(), kMaxTodoTextBytes);
    }
    if (updates.contains("priority") && updates["priority"].is_string())
    {
        todo->priority = clampField(updates["priority"].get<std::string>(), kMaxTodoFieldBytes);
    }
    if (updates.contains("status") && updates["status"].is_string())
    {
        todo->status = clampField(updates["status"].get<std::string>(), kMaxTodoFieldBytes);
    }
    if (updates.contains("category") && updates["category"].is_string())
    {
        todo->category = clampField(updates["category"].get<std::string>(), kMaxTodoFieldBytes);
    }
    if (updates.contains("dependsOn") && updates["dependsOn"].is_array())
    {
        todo->dependsOnIds.clear();
        for (const auto& dep : updates["dependsOn"])
        {
            if (!dep.is_number_integer())
            {
                continue;
            }
            const int depId = clampNonNegativeInt(dep.get<int>(), 1000000000);
            if (depId > 0)
            {
                todo->dependsOnIds.push_back(depId);
            }
        }
    }
    if (updates.contains("blockedById") && updates["blockedById"].is_number_integer())
    {
        todo->blockedById = clampNonNegativeInt(updates["blockedById"].get<int>(), 1000000000);
    }
    if (updates.contains("blocker") && updates["blocker"].is_object())
    {
        const json& blocker = updates["blocker"];
        if (blocker.contains("type") && blocker["type"].is_string())
        {
            todo->blockerType = clampField(blocker["type"].get<std::string>(), kMaxTodoFieldBytes);
        }
        if (blocker.contains("reason") && blocker["reason"].is_string())
        {
            todo->blockerReason = clampField(blocker["reason"].get<std::string>(), kMaxTodoTextBytes);
        }
        if (blocker.contains("blockedById") && blocker["blockedById"].is_number_integer())
        {
            todo->blockedById = clampNonNegativeInt(blocker["blockedById"].get<int>(), 1000000000);
        }
    }
    if (updates.contains("statusTransition") && updates["statusTransition"].is_string())
    {
        todo->statusTransitions.push_back(
            clampField(updates["statusTransition"].get<std::string>(), kMaxTodoFieldBytes));
    }

    if (previousStatus != todo->status)
    {
        todo->statusTransitions.push_back(clampField(previousStatus + "->" + todo->status, kMaxTodoFieldBytes));
    }
    if (todo->statusTransitions.size() > 32)
    {
        const size_t eraseCount = todo->statusTransitions.size() - 32;
        todo->statusTransitions.erase(todo->statusTransitions.begin(), todo->statusTransitions.begin() + eraseCount);
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

bool TodoManager::DeleteTodo(int id)
{
    auto it = std::find_if(items_.begin(), items_.end(), [id](const TodoItem& todo) { return todo.id == id; });

    if (it == items_.end())
    {
        return false;
    }

    items_.erase(it);
    stats_.totalDeleted++;

    Save();
    SendUpdateNotification();

    return true;
}

bool TodoManager::ClearAll()
{
    items_.clear();
    Save();
    SendUpdateNotification();
    return true;
}

std::vector<TodoItem> TodoManager::ParseCommand(const std::string& command)
{
    std::vector<TodoItem> todos;

    std::vector<std::string> args = {"-Operation", "parse", "-Command", "\"" + command + "\"", "-Json"};

    std::string output;
    if (ExecutePowerShellCommandCapture("parse", args, output) && !output.empty())
    {
        try
        {
            const json parsed = json::parse(output);
            if (parsed.is_array())
            {
                for (const auto& item : parsed)
                {
                    TodoItem todo;
                    todo.text = clampField(item.value("Text", std::string()), kMaxTodoTextBytes);
                    todo.priority = clampField(item.value("Priority", std::string("Medium")), kMaxTodoFieldBytes);
                    todo.status = clampField(item.value("Status", std::string("pending")), kMaxTodoFieldBytes);
                    todo.source = clampField(item.value("Source", std::string("parsed")), kMaxTodoFieldBytes);
                    todo.category = clampField(item.value("Category", std::string("general")), kMaxTodoFieldBytes);
                    if (!todo.text.empty())
                    {
                        todos.push_back(std::move(todo));
                    }
                }
            }
        }
        catch (...)
        {
        }
    }

    if (!todos.empty())
    {
        return todos;
    }

    auto texts = TodoCommandParser::Parse(command);
    for (const auto& text : texts)
    {
        TodoItem todo;
        todo.text = text;
        todo.source = "parsed";
        todo.priority = "Medium";
        todo.status = "pending";
        todos.push_back(todo);
    }

    return todos;
}

std::vector<TodoItem> TodoManager::CreateAgenticTodos(const std::string& context)
{
    AgenticTodoCreator creator(context);
    return creator.Generate();
}

std::vector<TodoItem> TodoManager::GetByStatus(const std::string& status) const
{
    std::vector<TodoItem> result;
    std::copy_if(items_.begin(), items_.end(), std::back_inserter(result),
                 [&status](const TodoItem& todo) { return todo.status == status; });
    return result;
}

TodoItem* TodoManager::GetById(int id)
{
    auto it = std::find_if(items_.begin(), items_.end(), [id](const TodoItem& todo) { return todo.id == id; });
    return it != items_.end() ? &(*it) : nullptr;
}

const TodoItem* TodoManager::GetById(int id) const
{
    auto it = std::find_if(items_.begin(), items_.end(), [id](const TodoItem& todo) { return todo.id == id; });
    return it != items_.end() ? &(*it) : nullptr;
}

bool TodoManager::StartPipeServer()
{
    if (pipeHandle_ != INVALID_HANDLE_VALUE)
    {
        return true;
    }

    std::wstring pipeName = L"\\\\.\\pipe\\RawrXD_Todos";

    pipeHandle_ = CreateNamedPipeW(pipeName.c_str(), PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 4096, 4096, 0, NULL);

    if (pipeHandle_ == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    // Start server thread
    watchThread_ = CreateThread(NULL, 0, PipeServerThreadProc, this, 0, NULL);

    return true;
}

void TodoManager::StopPipeServer()
{
    stopWatch_ = true;

    if (pipeHandle_ != INVALID_HANDLE_VALUE)
    {
        CloseHandle(pipeHandle_);
        pipeHandle_ = INVALID_HANDLE_VALUE;
    }

    if (watchThread_ != NULL)
    {
        WaitForSingleObject(watchThread_, 1000);
        CloseHandle(watchThread_);
        watchThread_ = NULL;
    }
}

bool TodoManager::SendUpdateNotification()
{
    if (updateCallback_)
    {
        updateCallback_();
    }
    return true;
}

int TodoManager::GetNextId() const
{
    if (items_.empty())
    {
        return 1;
    }

    int maxId = 0;
    for (const auto& todo : items_)
    {
        if (todo.id > maxId)
        {
            maxId = todo.id;
        }
    }

    return maxId + 1;
}

TodoItem TodoManager::ParseTodoFromJson(const json& j)
{
    TodoItem todo;
    todo.id = clampNonNegativeInt(j.value("Id", 0), 1000000000);
    todo.text = clampField(j.value("Text", std::string("")), kMaxTodoTextBytes);
    todo.priority = clampField(j.value("Priority", std::string("Medium")), kMaxTodoFieldBytes);
    todo.status = clampField(j.value("Status", std::string("pending")), kMaxTodoFieldBytes);
    todo.category = clampField(j.value("Category", std::string("general")), kMaxTodoFieldBytes);
    todo.source = clampField(j.value("Source", std::string("user")), kMaxTodoFieldBytes);
    todo.createdAt = clampField(j.value("CreatedAt", std::string("")), kMaxTodoFieldBytes);
    todo.updatedAt = clampField(j.value("UpdatedAt", std::string("")), kMaxTodoFieldBytes);
    todo.estimatedMinutes = clampNonNegativeInt(j.value("EstimatedMinutes", 0), 1000000);
    todo.actualMinutes = clampNonNegativeInt(j.value("ActualMinutes", 0), 1000000);
    todo.blockedById = clampNonNegativeInt(j.value("BlockedById", 0), 1000000000);
    todo.blockerType = clampField(j.value("BlockerType", std::string("")), kMaxTodoFieldBytes);
    todo.blockerReason = clampField(j.value("BlockerReason", std::string("")), kMaxTodoTextBytes);

    if (j.contains("Tags") && j["Tags"].is_array())
    {
        size_t tagCount = 0;
        for (const auto& tag : j["Tags"])
        {
            if (!tag.is_string())
            {
                continue;
            }
            if (tagCount++ >= kMaxTodoTags)
            {
                break;
            }
            todo.tags.push_back(clampField(tag.get<std::string>(), kMaxTodoTagBytes));
        }
    }

    if (j.contains("DependsOn") && j["DependsOn"].is_array())
    {
        for (const auto& dep : j["DependsOn"])
        {
            if (!dep.is_number_integer())
            {
                continue;
            }
            const int depId = clampNonNegativeInt(dep.get<int>(), 1000000000);
            if (depId > 0)
            {
                todo.dependsOnIds.push_back(depId);
            }
        }
    }

    if (j.contains("StatusTransitions") && j["StatusTransitions"].is_array())
    {
        size_t count = 0;
        for (const auto& transition : j["StatusTransitions"])
        {
            if (!transition.is_string())
            {
                continue;
            }
            if (count++ >= 32)
            {
                break;
            }
            todo.statusTransitions.push_back(clampField(transition.get<std::string>(), kMaxTodoFieldBytes));
        }
    }

    return todo;
}

json TodoManager::TodoToJson(const TodoItem& todo) const
{
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
    j["DependsOn"] = todo.dependsOnIds;
    j["BlockedById"] = todo.blockedById;
    j["BlockerType"] = todo.blockerType;
    j["BlockerReason"] = todo.blockerReason;
    j["StatusTransitions"] = todo.statusTransitions;
    return j;
}

bool TodoManager::ExecutePowerShellCommand(const std::string& operation, const std::vector<std::string>& args)
{
    std::string output;
    return ExecutePowerShellCommandCapture(operation, args, output);
}

bool TodoManager::ExecutePowerShellCommandCapture(const std::string& operation, const std::vector<std::string>& args,
                                                  std::string& output)
{
    std::string scriptPath = getTodoScriptPath();
    if (scriptPath.empty() || GetFileAttributesA(scriptPath.c_str()) == INVALID_FILE_ATTRIBUTES)
        return false;
    std::ostringstream cmd;
    cmd << "powershell.exe -ExecutionPolicy Bypass -File \"" << scriptPath << "\" ";
    cmd << "-Operation " << operation;

    for (const auto& arg : args)
    {
        cmd << " " << arg;
    }

    SECURITY_ATTRIBUTES sa = {};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE readPipe = INVALID_HANDLE_VALUE;
    HANDLE writePipe = INVALID_HANDLE_VALUE;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
    {
        return false;
    }
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi = {0};

    std::string cmdStr = cmd.str();

    if (!CreateProcessA(NULL, cmdStr.data(), NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi))
    {
        CloseHandle(readPipe);
        CloseHandle(writePipe);
        return false;
    }

    CloseHandle(writePipe);

    char buffer[2048];
    DWORD bytesRead = 0;
    output.clear();
    while (ReadFile(readPipe, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0)
    {
        output.append(buffer, buffer + bytesRead);
    }
    CloseHandle(readPipe);

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    return exitCode == 0;
}

DWORD WINAPI TodoManager::PipeServerThreadProc(LPVOID param)
{
    auto* manager = static_cast<TodoManager*>(param);

    while (!manager->stopWatch_)
    {
        BOOL connected = ConnectNamedPipe(manager->pipeHandle_, NULL) ? TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);

        if (connected)
        {
            char buffer[4096];
            DWORD bytesRead;
            constexpr DWORD kMaxChunk = static_cast<DWORD>(sizeof(buffer) - 1);

            if (ReadFile(manager->pipeHandle_, buffer, kMaxChunk, &bytesRead, NULL) && bytesRead > 0)
            {
                const size_t safeBytes =
                    (bytesRead <= kMaxChunk) ? static_cast<size_t>(bytesRead) : static_cast<size_t>(kMaxChunk);
                buffer[safeBytes] = '\0';

                json reply;
                reply["ok"] = false;
                try
                {
                    json command = json::parse(buffer);
                    if (!command.is_object())
                    {
                        reply["error"] = "Command must be a JSON object";
                    }
                    else
                    {
                        const std::string opRaw = readOpString(command, "operation", "Operation");
                        const std::string op = toLowerCopy(opRaw);

                        if (op == "add")
                        {
                            const std::string text = readOpString(command, "text", "Text");
                            const std::string priority = readOpString(command, "priority", "Priority").empty()
                                                             ? "Medium"
                                                             : readOpString(command, "priority", "Priority");
                            const std::string source = readOpString(command, "source", "Source").empty()
                                                           ? "user"
                                                           : readOpString(command, "source", "Source");
                            const bool ok = manager->AddTodo(text, priority, source);
                            reply["ok"] = ok;
                            if (!ok)
                                reply["error"] = "AddTodo failed";
                        }
                        else if (op == "complete")
                        {
                            const int id = command.value("id", command.value("Id", 0));
                            const bool ok = manager->CompleteTodo(id);
                            reply["ok"] = ok;
                            if (!ok)
                                reply["error"] = "CompleteTodo failed";
                        }
                        else if (op == "update")
                        {
                            const int id = command.value("id", command.value("Id", 0));
                            json updates;
                            if (command.contains("updates") && command["updates"].is_object())
                            {
                                updates = command["updates"];
                            }
                            else if (command.contains("Updates") && command["Updates"].is_object())
                            {
                                updates = command["Updates"];
                            }
                            else
                            {
                                updates = command;
                            }
                            const bool ok = manager->UpdateTodo(id, updates);
                            reply["ok"] = ok;
                            if (!ok)
                                reply["error"] = "UpdateTodo failed";
                        }
                        else if (op == "delete")
                        {
                            const int id = command.value("id", command.value("Id", 0));
                            const bool ok = manager->DeleteTodo(id);
                            reply["ok"] = ok;
                            if (!ok)
                                reply["error"] = "DeleteTodo failed";
                        }
                        else if (op == "clear")
                        {
                            const bool ok = manager->ClearAll();
                            reply["ok"] = ok;
                            if (!ok)
                                reply["error"] = "ClearAll failed";
                        }
                        else if (op == "reload")
                        {
                            const bool ok = manager->Reload();
                            reply["ok"] = ok;
                            if (!ok)
                                reply["error"] = "Reload failed";
                        }
                        else if (op == "list" || op == "getall")
                        {
                            reply["ok"] = true;
                            reply["items"] = json::array();
                            for (const auto& todo : manager->GetAll())
                            {
                                reply["items"].push_back(manager->TodoToJson(todo));
                            }
                        }
                        else if (op == "stats")
                        {
                            const auto stats = manager->GetStatistics();
                            reply["ok"] = true;
                            reply["stats"] = {
                                {"totalCreated", stats.totalCreated}, {"totalCompleted", stats.totalCompleted},
                                {"totalDeleted", stats.totalDeleted}, {"agenticCreated", stats.agenticCreated},
                                {"userCreated", stats.userCreated},   {"parsedCreated", stats.parsedCreated}};
                            reply["count"] = manager->GetCount();
                        }
                        else if (op == "parse")
                        {
                            const std::string text = readOpString(command, "command", "Command");
                            auto parsed = TodoCommandParser::Parse(text);
                            reply["ok"] = true;
                            reply["parsed"] = parsed;
                        }
                        else if (op == "agentic_create")
                        {
                            const std::string context = readOpString(command, "context", "Context");
                            const auto todos = manager->CreateAgenticTodos(context);
                            reply["ok"] = true;
                            reply["items"] = json::array();
                            for (const auto& todo : todos)
                            {
                                reply["items"].push_back(manager->TodoToJson(todo));
                            }
                        }
                        else
                        {
                            reply["error"] = "Unsupported operation";
                            reply["operation"] = opRaw;
                        }
                    }
                }
                catch (...)
                {
                    reply["error"] = "Invalid JSON payload";
                }

                const std::string out = reply.dump();
                DWORD bytesWritten = 0;
                WriteFile(manager->pipeHandle_, out.c_str(), static_cast<DWORD>(out.size()), &bytesWritten, NULL);
            }

            DisconnectNamedPipe(manager->pipeHandle_);
        }

        Sleep(25);
    }

    return 0;
}

DWORD WINAPI TodoManager::FileWatchThreadProc(LPVOID param)
{
    auto* manager = static_cast<TodoManager*>(param);
    std::string watchDir = manager->storagePath_;
    size_t sep = watchDir.find_last_of("/\\");
    if (sep != std::string::npos)
        watchDir.resize(sep);
    else
        watchDir = ".";

    HANDLE hDir =
        CreateFileA(watchDir.c_str(), FILE_LIST_DIRECTORY, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);

    if (hDir == INVALID_HANDLE_VALUE)
    {
        return 1;
    }

    char buffer[4096];
    DWORD bytesReturned;

    while (!manager->stopWatch_)
    {
        if (ReadDirectoryChangesW(hDir, buffer, sizeof(buffer), FALSE, FILE_NOTIFY_CHANGE_LAST_WRITE, &bytesReturned,
                                  NULL, NULL))
        {

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

std::vector<std::string> TodoCommandParser::Parse(const std::string& command)
{
    // Try numbered list first
    auto todos = ParseNumberedList(command);
    if (!todos.empty())
    {
        return todos;
    }

    // Try bullet list
    todos = ParseBulletList(command);
    if (!todos.empty())
    {
        return todos;
    }

    // Try comma-separated
    return ParseCommaSeparated(command);
}

std::vector<std::string> TodoCommandParser::ParseNumberedList(const std::string& text)
{
    std::vector<std::string> todos;
    std::regex pattern(R"(\d+\.\s*([^0-9]+?)(?=\d+\.|$))");

    auto begin = std::sregex_iterator(text.begin(), text.end(), pattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it)
    {
        std::string todo = (*it)[1].str();
        // Trim whitespace
        todo.erase(0, todo.find_first_not_of(" \t\r\n"));
        todo.erase(todo.find_last_not_of(" \t\r\n") + 1);

        if (!todo.empty())
        {
            todos.push_back(todo);
        }
    }

    return todos;
}

std::vector<std::string> TodoCommandParser::ParseBulletList(const std::string& text)
{
    std::vector<std::string> todos;
    std::regex pattern(R"([-•]\s*([^-•]+))");

    auto begin = std::sregex_iterator(text.begin(), text.end(), pattern);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it)
    {
        std::string todo = (*it)[1].str();
        todo.erase(0, todo.find_first_not_of(" \t\r\n"));
        todo.erase(todo.find_last_not_of(" \t\r\n") + 1);

        if (!todo.empty())
        {
            todos.push_back(todo);
        }
    }

    return todos;
}

std::vector<std::string> TodoCommandParser::ParseCommaSeparated(const std::string& text)
{
    std::vector<std::string> todos;
    std::istringstream stream(text);
    std::string todo;

    while (std::getline(stream, todo, ','))
    {
        todo.erase(0, todo.find_first_not_of(" \t\r\n"));
        todo.erase(todo.find_last_not_of(" \t\r\n") + 1);

        if (!todo.empty())
        {
            todos.push_back(todo);
        }
    }

    return todos;
}

// ═══════════════════════════════════════════════════════════════════════════════
// AgenticTodoCreator Implementation
// ═══════════════════════════════════════════════════════════════════════════════

AgenticTodoCreator::AgenticTodoCreator(const std::string& context) : context_(context) {}

std::vector<TodoItem> AgenticTodoCreator::Generate()
{
    std::vector<TodoItem> todos;
    std::string type = DetectType();

    auto templates = GetTemplateForType(type);

    for (const auto& text : templates)
    {
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

std::string AgenticTodoCreator::DetectType() const
{
    std::string lower = context_;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("api") != std::string::npos || lower.find("rest") != std::string::npos)
        return "api";
    if (lower.find("bug") != std::string::npos || lower.find("fix") != std::string::npos)
        return "bug";
    if (lower.find("refactor") != std::string::npos)
        return "refactor";
    if (lower.find("model") != std::string::npos || lower.find("train") != std::string::npos)
        return "model";

    return "feature";
}

std::vector<std::string> AgenticTodoCreator::GetTemplateForType(const std::string& type) const
{
    if (type == "api")
    {
        return {"Design API endpoints and routes",      "Implement request/response models",
                "Add authentication and authorization", "Write API documentation",
                "Add error handling and validation",    "Create unit tests for API"};
    }
    else if (type == "bug")
    {
        return {"Reproduce and document bug", "Identify root cause", "Implement fix", "Add regression test",
                "Verify fix in all scenarios"};
    }
    else if (type == "model")
    {
        return {"Define model architecture",   "Prepare training dataset",   "Implement training pipeline",
                "Run training and validation", "Evaluate model performance", "Deploy model to production"};
    }
    else
    {
        return {"Analyze requirements", "Design architecture",  "Implement core functionality",
                "Write tests",          "Update documentation", "Perform code review"};
    }
}

std::vector<TodoItem> AgenticTodoCreator::GenerateContextSpecific() const
{
    std::vector<TodoItem> todos;

    // Extract specific items from context using regex
    std::regex implementRegex(R"(implement (\w+))", std::regex::icase);
    std::smatch match;

    if (std::regex_search(context_, match, implementRegex))
    {
        TodoItem todo;
        todo.text = "Implement " + match[1].str() + " functionality";
        todo.source = "agentic";
        todo.priority = "High";
        todo.status = "pending";
        todos.push_back(todo);
    }

    return todos;
}

// ═══════════════════════════════════════════════════════════════════════════════
// TodoUIRenderer Implementation
// VSU Effects: Uses Adobe RGBa color space for professional color accuracy
// ═══════════════════════════════════════════════════════════════════════════════

// Helper to convert AdobeRGBa to COLORREF for Win32 GDI
inline COLORREF AdobeRGBaToCOLORREF(const AdobeRGBa& color) {
    auto srgb = color.TosRGB();
    return RGB(static_cast<int>(srgb.r * 255), 
               static_cast<int>(srgb.g * 255), 
               static_cast<int>(srgb.b * 255));
}

void TodoUIRenderer::DrawText(HDC hdc, const std::wstring& text, RECT rect, AdobeRGBa color)
{
    if (!hdc || text.empty())
    {
        return;
    }

    const COLORREF oldColor = SetTextColor(hdc, AdobeRGBaToCOLORREF(color));
    const int oldBkMode = SetBkMode(hdc, TRANSPARENT);
    ::DrawTextW(hdc, text.c_str(), -1, &rect, DT_LEFT | DT_TOP | DT_NOPREFIX | DT_WORDBREAK);
    SetBkMode(hdc, oldBkMode);
    SetTextColor(hdc, oldColor);
}

void TodoUIRenderer::DrawIcon(HDC hdc, const std::wstring& icon, POINT pos)
{
    RECT rect = {pos.x, pos.y, pos.x + 24, pos.y + 24};
    DrawText(hdc, icon, rect, AdobeRGBa(0.90f, 0.90f, 0.90f, 1.00f));
}

void TodoUIRenderer::RenderTodoItem(HDC hdc, const TodoItem& todo, RECT itemRect, bool selected)
{
    if (!hdc)
    {
        return;
    }

    // VSU Acrylic background with selection state
    AdobeRGBa bgColor = selected ? VSU::Accents::BlueDark : VSU::Acrylic::DarkBase;
    HBRUSH bgBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(bgColor));
    FillRect(hdc, &itemRect, bgBrush);
    DeleteObject(bgBrush);

    RECT iconRect = itemRect;
    iconRect.left += 8;
    iconRect.top += 6;
    iconRect.right = iconRect.left + 24;
    iconRect.bottom = iconRect.top + 24;
    DrawText(hdc, todo.GetStatusIcon(), iconRect, todo.GetStatusColor());

    RECT priorityRect = iconRect;
    priorityRect.left += 28;
    priorityRect.right += 28;
    DrawText(hdc, todo.GetPriorityIcon(), priorityRect, AdobeRGBa(0.86f, 0.86f, 0.86f, 1.00f));

    RECT textRect = itemRect;
    textRect.left += 64;
    textRect.top += 6;
    textRect.right -= 12;
    textRect.bottom = textRect.top + 36;
    DrawText(hdc, std::wstring(todo.text.begin(), todo.text.end()), textRect, AdobeRGBa(0.94f, 0.94f, 0.94f, 1.00f));

    RECT metaRect = itemRect;
    metaRect.left += 64;
    metaRect.top += 38;
    metaRect.right -= 12;
    metaRect.bottom -= 6;
    std::wstring meta = std::wstring(todo.priority.begin(), todo.priority.end()) + L" | " +
                        std::wstring(todo.status.begin(), todo.status.end()) + L" | #" + std::to_wstring(todo.id);
    DrawText(hdc, meta, metaRect, AdobeRGBa(0.63f, 0.63f, 0.63f, 1.00f));
}

void TodoUIRenderer::RenderProgressBar(HDC hdc, int completed, int total, RECT barRect)
{
    if (!hdc)
    {
        return;
    }

    // VSU Acrylic background
    HBRUSH bgBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(VSU::Acrylic::DarkLuminosity));
    FillRect(hdc, &barRect, bgBrush);
    DeleteObject(bgBrush);

    RECT fillRect = barRect;
    if (total > 0 && completed > 0)
    {
        const int width = barRect.right - barRect.left;
        fillRect.right = barRect.left + (width * std::min(completed, total)) / total;
        // VSU Success accent for progress
        HBRUSH fillBrush = CreateSolidBrush(AdobeRGBaToCOLORREF(VSU::Accents::Success));
        FillRect(hdc, &fillRect, fillBrush);
        DeleteObject(fillBrush);
    }

    FrameRect(hdc, &barRect, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
}

void TodoUIRenderer::RenderStatistics(HDC hdc, const TodoManager::Statistics& stats, RECT statsRect)
{
    if (!hdc)
    {
        return;
    }

    const int totalActive = std::max(0, stats.totalCreated - stats.totalDeleted);
    const int completed = std::max(0, stats.totalCompleted);

    RECT titleRect = statsRect;
    titleRect.bottom = titleRect.top + 22;
    DrawText(hdc, L"Todo Statistics", titleRect, RGB(255, 255, 255));

    RECT bodyRect = statsRect;
    bodyRect.top += 24;
    std::wstring body = L"Created: " + std::to_wstring(stats.totalCreated) + L"  Completed: " +
                        std::to_wstring(stats.totalCompleted) + L"  Deleted: " + std::to_wstring(stats.totalDeleted) +
                        L"\nAgentic: " + std::to_wstring(stats.agenticCreated) + L"  User: " +
                        std::to_wstring(stats.userCreated) + L"  Parsed: " + std::to_wstring(stats.parsedCreated);
    DrawText(hdc, body, bodyRect, RGB(200, 200, 200));

    RECT progressRect = statsRect;
    progressRect.top = statsRect.bottom - 18;
    RenderProgressBar(hdc, completed, std::max(totalActive, completed), progressRect);
}

void TodoUIRenderer::RenderTodoList(HDC hdc, const std::vector<TodoItem>& todos, RECT clientRect)
{
    if (!hdc)
    {
        return;
    }

    HBRUSH bgBrush = CreateSolidBrush(RGB(24, 24, 24));
    FillRect(hdc, &clientRect, bgBrush);
    DeleteObject(bgBrush);

    RECT headerRect = clientRect;
    headerRect.left += 8;
    headerRect.top += 8;
    headerRect.bottom = headerRect.top + 24;
    DrawText(hdc, L"Todo List", headerRect, RGB(255, 255, 255));

    RECT statsRect = clientRect;
    statsRect.left += 8;
    statsRect.top = headerRect.bottom + 4;
    statsRect.right -= 8;
    statsRect.bottom = statsRect.top + 76;

    TodoManager::Statistics stats = {};
    stats.totalCreated = static_cast<int>(todos.size());
    stats.totalCompleted = static_cast<int>(
        std::count_if(todos.begin(), todos.end(), [](const TodoItem& todo) { return todo.status == "completed"; }));
    RenderStatistics(hdc, stats, statsRect);

    int y = statsRect.bottom + 10;
    constexpr int itemHeight = 62;
    for (size_t i = 0; i < todos.size(); ++i)
    {
        RECT itemRect = {clientRect.left + 8, y, clientRect.right - 8, y + itemHeight};
        if (itemRect.bottom > clientRect.bottom)
        {
            break;
        }
        RenderTodoItem(hdc, todos[i], itemRect, false);
        y += itemHeight + 6;
    }
}

}  // namespace Todos
}  // namespace RawrXD
