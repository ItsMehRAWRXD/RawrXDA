// Win32 Todo Integration Header
// Provides Win32IDE C++ integration for PowerShell todo system
// VSU Effects: Uses Adobe RGBa color space for professional color accuracy

#pragma once

#include <windows.h>
#include <algorithm>
#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include "../../include/RawrXD_ColorSpace.h"

using json = nlohmann::json;
using namespace RawrXD::ColorSpace;

namespace RawrXD {
namespace Todos {

// Todo item structure matching PowerShell schema
struct TodoItem {
    int id;
    std::string text;
    std::string priority;  // Low, Medium, High, Critical
    std::string status;    // pending, in-progress, completed, blocked, cancelled
    std::string category;
    std::string source;    // user, agentic, parsed
    std::string createdAt;
    std::string updatedAt;
    std::vector<std::string> tags;
    int estimatedMinutes = 0;
    int actualMinutes = 0;
    std::vector<int> dependsOnIds;
    int blockedById = 0;
    std::string blockerType;
    std::string blockerReason;
    std::vector<std::string> statusTransitions;
    
    std::wstring GetStatusIcon() const {
        if (status == "pending") return L"⏳";
        if (status == "in-progress") return L"🔄";
        if (status == "completed") return L"✅";
        if (status == "blocked") return L"🚫";
        if (status == "cancelled") return L"❌";
        return L"❓";
    }
    
    std::wstring GetPriorityIcon() const {
        if (priority == "Critical") return L"🔴";
        if (priority == "High") return L"🟠";
        if (priority == "Medium") return L"🟡";
        if (priority == "Low") return L"🟢";
        return L"⚪";
    }
    
    AdobeRGBa GetStatusColor() const {
        if (status == "pending") return VSU::Accents::Warning;      // Yellow
        if (status == "in-progress") return VSU::Accents::Blue;   // Cyan
        if (status == "completed") return VSU::Accents::Success;  // Green
        if (status == "blocked") return VSU::Accents::Error;      // Red
        if (status == "cancelled") return AdobeRGBa(0.50f, 0.50f, 0.50f, 1.00f); // Gray
        return AdobeRGBa(1.00f, 1.00f, 1.00f, 1.00f);             // White
    }
};

// Todo list manager
class TodoManager {
public:
    TodoManager(const std::string& storagePath = "");  // default: %APPDATA%\RawrXD\todos.json
    ~TodoManager();
    
    // Core operations
    bool Load();
    bool Save();
    bool Reload();
    
    // Todo operations
    bool AddTodo(const std::string& text, const std::string& priority = "Medium", const std::string& source = "user");
    bool CompleteTodo(int id);
    bool UpdateTodo(int id, const json& updates);
    bool DeleteTodo(int id);
    bool ClearAll();
    
    // Parsing and agentic creation
    std::vector<TodoItem> ParseCommand(const std::string& command);
    std::vector<TodoItem> CreateAgenticTodos(const std::string& context);
    
    // Query operations
    std::vector<TodoItem> GetAll() const { return items_; }
    std::vector<TodoItem> GetByStatus(const std::string& status) const;
    TodoItem* GetById(int id);
    const TodoItem* GetById(int id) const;
    int GetCount() const { return static_cast<int>(items_.size()); }
    int GetMaxCount() const { return maxItems_; }
    bool CanAdd() const { return items_.size() < maxItems_; }

    // Configurable limit (1–99): set from IDEConfig or audit estimate
    void SetMaxItems(int max) { maxItems_ = std::clamp(static_cast<size_t>(max), size_t(1), size_t(99)); }
    
    // Statistics
    struct Statistics {
        int totalCreated;
        int totalCompleted;
        int totalDeleted;
        int agenticCreated;
        int userCreated;
        int parsedCreated;
    };
    Statistics GetStatistics() const { return stats_; }
    
    // Win32 integration
    bool StartPipeServer();
    void StopPipeServer();
    bool SendUpdateNotification();
    
    // Watch mode for UI
    void SetUpdateCallback(std::function<void()> callback) { updateCallback_ = callback; }
    
private:
    std::string storagePath_;
    std::vector<TodoItem> items_;
    size_t maxItems_;
    Statistics stats_;
    HANDLE pipeHandle_;
    HANDLE watchThread_;
    bool stopWatch_;
    std::function<void()> updateCallback_;
    
    // Helper methods
    int GetNextId() const;
    TodoItem ParseTodoFromJson(const json& j);
    json TodoToJson(const TodoItem& todo) const;
    bool ExecutePowerShellCommand(const std::string& operation, const std::vector<std::string>& args);
    bool ExecutePowerShellCommandCapture(const std::string& operation, const std::vector<std::string>& args, std::string& output);
    static DWORD WINAPI PipeServerThreadProc(LPVOID param);
    static DWORD WINAPI FileWatchThreadProc(LPVOID param);
};

// Win32 UI helpers
class TodoUIRenderer {
public:
    static void RenderTodoList(HDC hdc, const std::vector<TodoItem>& todos, RECT clientRect);
    static void RenderTodoItem(HDC hdc, const TodoItem& todo, RECT itemRect, bool selected);
    static void RenderProgressBar(HDC hdc, int completed, int total, RECT barRect);
    static void RenderStatistics(HDC hdc, const TodoManager::Statistics& stats, RECT statsRect);
    
private:
    static void DrawText(HDC hdc, const std::wstring& text, RECT rect, AdobeRGBa color);
    static void DrawIcon(HDC hdc, const std::wstring& icon, POINT pos);
};

// Command parser for !todos syntax
class TodoCommandParser {
public:
    static std::vector<std::string> Parse(const std::string& command);
    
private:
    static std::vector<std::string> ParseNumberedList(const std::string& text);
    static std::vector<std::string> ParseBulletList(const std::string& text);
    static std::vector<std::string> ParseCommaSeparated(const std::string& text);
};

// Agentic todo creator
class AgenticTodoCreator {
public:
    AgenticTodoCreator(const std::string& context);
    std::vector<TodoItem> Generate();
    
private:
    std::string context_;
    std::string DetectType() const;
    std::vector<std::string> GetTemplateForType(const std::string& type) const;
    std::vector<TodoItem> GenerateContextSpecific() const;
};

} // namespace Todos
} // namespace RawrXD
