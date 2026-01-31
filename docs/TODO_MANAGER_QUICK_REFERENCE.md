# Todo Manager Quick Reference

## Overview
Dynamic, persistent, and static todo management system with:
- **Max 25 todos** per list
- **3 Creation Methods**: User (manual), Parsed (!todos), Agentic (AI-generated)
- **5 Status Types**: pending, in-progress, completed, blocked, cancelled
- **4 Priority Levels**: Low, Medium, High, Critical
- **Persistent Storage**: JSON-based with auto-save/load
- **Win32 Integration**: Native C++ bridge for UI display

## PowerShell Commands

### Add Todo (Manual)
```powershell
.\todo_manager.ps1 -Operation add -TodoText "Implement authentication" -Priority High
.\todo_manager.ps1 -Operation add -TodoText "Fix memory leak" -Priority Critical
.\todo_manager.ps1 -Operation add -TodoText "Update docs" -Priority Low
```

### Parse !todos Command
```powershell
# Numbered format
.\todo_manager.ps1 -Operation parse -Command "!todos 1. Fix bug 2. Add feature 3. Test code"

# Bullet format
.\todo_manager.ps1 -Operation parse -Command "!todos - Task one - Task two - Task three"

# Comma-separated
.\todo_manager.ps1 -Operation parse -Command "!todos Task one, Task two, Task three"
```

### Agentic Creation (AI-Generated)
```powershell
.\todo_manager.ps1 -Operation agentic-create -Context "Build REST API server"
.\todo_manager.ps1 -Operation agentic-create -Context "Fix authentication bug"
.\todo_manager.ps1 -Operation agentic-create -Context "Train ML model for classification"
```

### List Todos
```powershell
# List all
.\todo_manager.ps1 -Operation list

# List with details
.\todo_manager.ps1 -Operation list -Verbose

# Filter by status
.\todo_manager.ps1 -Operation list -Status pending
.\todo_manager.ps1 -Operation list -Status completed
```

### Update Todo
```powershell
# Complete a todo
.\todo_manager.ps1 -Operation complete -TodoId 5

# Update text/priority/status
.\todo_manager.ps1 -Operation update -TodoId 3 -TodoText "Updated task" -Priority High
.\todo_manager.ps1 -Operation update -TodoId 7 -Status in-progress
```

### Delete/Clear
```powershell
# Delete single todo
.\todo_manager.ps1 -Operation delete -TodoId 4

# Clear all todos (with confirmation)
.\todo_manager.ps1 -Operation clear
```

### Export/Import
```powershell
# Export to file
.\todo_manager.ps1 -Operation export -ExportPath "D:\backup\todos.json"

# Import from file
.\todo_manager.ps1 -Operation import -ExportPath "D:\backup\todos.json"
```

### Watch Mode (Continuous Monitoring)
```powershell
.\todo_manager.ps1 -Operation list -Watch -Verbose
```

## C++ Integration Examples

### Basic Usage
```cpp
#include "TodoManager.h"

using namespace RawrXD::Todos;

// Initialize
TodoManager todoMgr("D:\\lazy init ide\\data\\todos.json");

// Add todo
todoMgr.AddTodo("Implement feature X", "High", "user");

// Parse command
auto todos = todoMgr.ParseCommand("!todos 1. Task one 2. Task two");
for (const auto& todo : todos) {
    todoMgr.AddTodo(todo.text, todo.priority, "parsed");
}

// Agentic creation
auto agenticTodos = todoMgr.CreateAgenticTodos("Build REST API");
for (const auto& todo : agenticTodos) {
    todoMgr.AddTodo(todo.text, todo.priority, "agentic");
}

// Query todos
auto allTodos = todoMgr.GetAll();
auto pending = todoMgr.GetByStatus("pending");

// Update operations
todoMgr.CompleteTodo(5);
todoMgr.DeleteTodo(3);

// Statistics
auto stats = todoMgr.GetStatistics();
std::cout << "Total: " << stats.totalCreated << "\n";
std::cout << "Completed: " << stats.totalCompleted << "\n";
```

### Win32 UI Integration
```cpp
// Start pipe server for PowerShell communication
todoMgr.StartPipeServer();

// Set callback for updates
todoMgr.SetUpdateCallback([]() {
    InvalidateRect(hwnd, NULL, TRUE);
});

// Render in WM_PAINT
case WM_PAINT: {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    
    auto todos = todoMgr.GetAll();
    TodoUIRenderer::RenderTodoList(hdc, todos, clientRect);
    
    EndPaint(hwnd, &ps);
    break;
}
```

## File Locations

- **PowerShell Script**: `D:\lazy init ide\scripts\todo_manager.ps1`
- **Storage**: `D:\lazy init ide\data\todos.json`
- **C++ Header**: `D:\lazy init ide\src\win32app\TodoManager.h`
- **C++ Implementation**: `D:\lazy init ide\src\win32app\TodoManager.cpp`

## Data Structure

### JSON Format
```json
{
  "Version": "1.0.0",
  "SavedAt": "2026-01-25T15:30:00",
  "MaxItems": 25,
  "Statistics": {
    "TotalCreated": 15,
    "TotalCompleted": 8,
    "TotalDeleted": 2,
    "AgenticCreated": 5,
    "UserCreated": 7,
    "ParsedCreated": 3
  },
  "Items": [
    {
      "Id": 1,
      "Text": "Implement authentication",
      "Priority": "High",
      "Status": "in-progress",
      "Category": "api",
      "Source": "agentic",
      "CreatedAt": "2026-01-25T14:00:00",
      "UpdatedAt": "2026-01-25T15:00:00",
      "Tags": ["auth", "security"],
      "EstimatedMinutes": 120,
      "ActualMinutes": 0
    }
  ]
}
```

## Status Icons

- ⏳ Pending
- 🔄 In Progress
- ✅ Completed
- 🚫 Blocked
- ❌ Cancelled

## Priority Icons

- 🔴 Critical
- 🟠 High
- 🟡 Medium
- 🟢 Low

## Tips

1. **Agentic Detection**: System auto-detects context type (api, bug, feature, model, refactor) and generates appropriate tasks
2. **25 Item Limit**: Prevents overflow; complete or delete old todos to add new ones
3. **Persistent Storage**: All changes auto-save to JSON file
4. **Win32 Real-time**: File watcher detects external changes and updates UI
5. **Named Pipe**: PowerShell ↔ C++ communication via `\\.\pipe\RawrXD_Todos`

## Advanced Usage

### Batch Operations
```powershell
# Add multiple via parsing
.\todo_manager.ps1 -Operation parse -Command "!todos 1. Setup database 2. Create models 3. Build API 4. Write tests 5. Deploy"

# Generate full project todo list
.\todo_manager.ps1 -Operation agentic-create -Context "Create full-stack web application with authentication and REST API"
```

### Integration with Autonomous Training
```powershell
# Create todos for training session
.\todo_manager.ps1 -Operation agentic-create -Context "Train 7B model on custom dataset for 24 hours"

# Then use with autonomous trainer
.\autonomous_finetune_bench.ps1 -Operation autonomous-train -TrainingFiles "data/*.txt" -Duration 24 -Win32Integration
```

### Programmatic Access
```cpp
// Auto-generate todos from build errors
auto errors = GetCompileErrors();
for (const auto& error : errors) {
    std::string todoText = "Fix: " + error.message;
    todoMgr.AddTodo(todoText, "High", "agentic");
}

// Generate todos from code analysis
auto issues = AnalyzeCode();
for (const auto& issue : issues) {
    auto todos = todoMgr.CreateAgenticTodos("Fix: " + issue.description);
    for (const auto& todo : todos) {
        todoMgr.AddTodo(todo.text, "Medium", "agentic");
    }
}
```
