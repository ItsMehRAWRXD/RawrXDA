# TodoManager Module Usage Examples

## Import the Module

```powershell
Import-Module "D:\lazy init ide\scripts\TodoManager.psm1" -Force
```

## Basic Usage

### Create and Initialize Todo List
```powershell
# Create new todo list
$todoList = New-TodoList -StoragePath "D:\lazy init ide\data\todos.json"

# Check if we can add more todos
if (Test-CanAddTodo -TodoList $todoList) {
    Write-Host "Can add more todos!"
}

# Get module info
$version = Get-TodoModuleVersion
$maxTodos = Get-TodoMaxLimit
Write-Host "TodoManager v$version - Max: $maxTodos todos"
```

### Add Todos (Manual)
```powershell
# Add simple todo
$todo1 = Add-Todo -TodoList $todoList -Text "Implement authentication" -Priority High

# Add with source specification
$todo2 = Add-Todo -TodoList $todoList -Text "Fix memory leak" -Priority Critical -Source user

# Add low priority todo
$todo3 = Add-Todo -TodoList $todoList -Text "Update documentation" -Priority Low
```

### Parse !todos Commands
```powershell
# Parse numbered list
$parsedTodos = Parse-TodoCommand -Command "!todos 1. Setup database 2. Create API 3. Write tests"

foreach ($todo in $parsedTodos) {
    Add-Todo -TodoList $todoList -Text $todo.Text -Priority Medium -Source parsed
}

# Parse bullet list
$bullets = Parse-TodoCommand -Command "!todos - Task one - Task two - Task three"

# Parse comma-separated
$commas = Parse-TodoCommand -Command "!todos Task one, Task two, Task three"
```

### Agentic Todo Creation
```powershell
# Generate todos for API development
$apiTodos = New-AgenticTodos -Context "Build REST API server"

foreach ($todo in $apiTodos) {
    Add-Todo -TodoList $todoList -Text $todo.Text -Priority $todo.Priority -Source agentic
}

# Generate todos for bug fixing
$bugTodos = New-AgenticTodos -Context "Fix authentication bug"

# Generate todos for ML model
$modelTodos = New-AgenticTodos -Context "Train classification model"
```

### Query Todos
```powershell
# Get all todos
$allTodos = Get-AllTodos -TodoList $todoList

# Get pending todos only
$pending = Get-AllTodos -TodoList $todoList -Status pending

# Get in-progress todos
$inProgress = Get-AllTodos -TodoList $todoList -Status in-progress

# Get completed todos
$completed = Get-AllTodos -TodoList $todoList -Status completed

# Get specific todo by ID
$specificTodo = Get-Todo -TodoList $todoList -Id 5
```

### Update Todos
```powershell
# Complete a todo
Complete-Todo -TodoList $todoList -Id 3

# Update todo text
Update-Todo -TodoList $todoList -Id 5 -Updates @{ Text = "Updated task description" }

# Update priority
Update-Todo -TodoList $todoList -Id 7 -Updates @{ Priority = "High" }

# Update status
Update-Todo -TodoList $todoList -Id 9 -Updates @{ Status = "in-progress" }

# Update multiple properties
Update-Todo -TodoList $todoList -Id 11 -Updates @{
    Text = "New text"
    Priority = "Critical"
    Status = "blocked"
    Category = "urgent"
}
```

### Delete and Clear
```powershell
# Delete single todo
Remove-Todo -TodoList $todoList -Id 4

# Clear all todos
Clear-TodoList -TodoList $todoList
```

### Display and Save
```powershell
# Display todo list
Show-TodoList -TodoList $todoList

# Display with verbose details
Show-TodoList -TodoList $todoList -Verbose

# Manually save
Save-TodoList -TodoList $todoList

# Reload from disk
Import-TodoList -TodoList $todoList
```

### Statistics
```powershell
# Get statistics
$stats = Get-TodoStatistics -TodoList $todoList

Write-Host "Total Created: $($stats.TotalCreated)"
Write-Host "Total Completed: $($stats.TotalCompleted)"
Write-Host "Total Deleted: $($stats.TotalDeleted)"
Write-Host "Agentic: $($stats.AgenticCreated)"
Write-Host "User: $($stats.UserCreated)"
Write-Host "Parsed: $($stats.ParsedCreated)"
```

### Win32 Integration
```powershell
# Export for Win32IDE
$win32Data = Export-TodosForWin32 -TodoList $todoList

# Save to file
$win32Data | Set-Content "D:\lazy init ide\data\todos_win32.json"

# Send to Win32 pipe
Send-TodosToWin32Pipe -Data $win32Data
```

## Advanced Examples

### Bulk Operations
```powershell
# Add multiple todos from array
$tasks = @(
    "Setup development environment",
    "Install dependencies",
    "Configure database",
    "Create project structure",
    "Implement core features"
)

foreach ($task in $tasks) {
    Add-Todo -TodoList $todoList -Text $task -Priority Medium
}
```

### Conditional Todo Management
```powershell
# Complete all pending high-priority todos
$highPriority = Get-AllTodos -TodoList $todoList -Status pending | 
    Where-Object { $_.Priority -eq "High" }

foreach ($todo in $highPriority) {
    Complete-Todo -TodoList $todoList -Id $todo.Id
}
```

### Custom TodoItem Creation
```powershell
# Create custom todo item
$customTodo = New-TodoItem -Text "Custom task"
$customTodo.Priority = "Critical"
$customTodo.Category = "security"
$customTodo.Tags = @("urgent", "security", "hotfix")
$customTodo.EstimatedMinutes = 120

# Convert to hashtable
$todoHash = ConvertTo-TodoHashtable -TodoItem $customTodo

# Convert back from hashtable
$restoredTodo = ConvertFrom-TodoHashtable -Data $todoHash
```

### Workflow Automation
```powershell
# Automated workflow: Parse, filter, execute
$command = "!todos 1. Review code 2. Fix bugs 3. Write tests 4. Deploy"
$todos = Parse-TodoCommand -Command $command

# Add only critical tasks
foreach ($todo in $todos | Select-Object -First 2) {
    Add-Todo -TodoList $todoList -Text $todo.Text -Priority High -Source parsed
}

# Generate comprehensive task list for project
$projectContext = "Build full-stack web application with authentication and REST API"
$projectTodos = New-AgenticTodos -Context $projectContext

foreach ($todo in $projectTodos) {
    if (Test-CanAddTodo -TodoList $todoList) {
        Add-Todo -TodoList $todoList -Text $todo.Text -Priority $todo.Priority -Source agentic
    } else {
        Write-Warning "Todo limit reached!"
        break
    }
}
```

### Integration with Other Scripts
```powershell
# Integrate with autonomous training
$trainingTodos = New-AgenticTodos -Context "Train 7B model for 24 hours"

foreach ($todo in $trainingTodos) {
    Add-Todo -TodoList $todoList -Text $todo.Text -Source agentic
}

# Start training and update todos
# .\autonomous_finetune_bench.ps1 -Operation autonomous-train -Duration 24

# Mark first todo as in progress
$firstTodo = (Get-AllTodos -TodoList $todoList -Status pending)[0]
Update-Todo -TodoList $todoList -Id $firstTodo.Id -Updates @{ Status = "in-progress" }
```

### Real-time Monitoring
```powershell
# Monitor todo progress
while ($true) {
    Clear-Host
    Import-TodoList -TodoList $todoList
    Show-TodoList -TodoList $todoList -Verbose
    
    $stats = Get-TodoStatistics -TodoList $todoList
    $completionRate = if ($stats.TotalCreated -gt 0) {
        [Math]::Round(($stats.TotalCompleted / $stats.TotalCreated) * 100, 2)
    } else { 0 }
    
    Write-Host "`nCompletion Rate: $completionRate%" -ForegroundColor Cyan
    
    Start-Sleep -Seconds 5
}
```

### Error Handling
```powershell
try {
    # Attempt to add todo
    $todo = Add-Todo -TodoList $todoList -Text "New task" -Priority High
    Write-Host "✅ Todo added: #$($todo.Id)" -ForegroundColor Green
}
catch {
    if ($_.Exception.Message -match "Maximum") {
        Write-Warning "Todo limit reached. Complete or delete some todos first."
        
        # Auto-complete oldest completed todos
        $oldCompleted = Get-AllTodos -TodoList $todoList -Status completed | 
            Sort-Object CompletedAt | 
            Select-Object -First 5
        
        foreach ($todo in $oldCompleted) {
            Remove-Todo -TodoList $todoList -Id $todo.Id
            Write-Host "Removed old completed todo: #$($todo.Id)"
        }
    }
    else {
        Write-Error "Failed to add todo: $_"
    }
}
```

## Exported Functions Reference

### TodoList Management
- `New-TodoList` - Create new todo list instance
- `Add-Todo` - Add todo to list
- `Get-Todo` - Get todo by ID
- `Get-AllTodos` - Get all or filtered todos
- `Complete-Todo` - Mark todo as completed
- `Update-Todo` - Update todo properties
- `Remove-Todo` - Delete todo
- `Clear-TodoList` - Clear all todos
- `Show-TodoList` - Display todo list
- `Save-TodoList` - Save to storage
- `Import-TodoList` - Load from storage

### Parsing and Generation
- `Parse-TodoCommand` - Parse !todos syntax
- `New-AgenticTodos` - Generate todos from context

### Win32 Integration
- `Export-TodosForWin32` - Export Win32 format
- `Send-TodosToWin32Pipe` - Send to named pipe

### Utilities
- `Get-TodoStatistics` - Get statistics
- `Test-CanAddTodo` - Check if can add more
- `New-TodoItem` - Create TodoItem instance
- `ConvertTo-TodoHashtable` - Convert to hashtable
- `ConvertFrom-TodoHashtable` - Convert from hashtable
- `Get-TodoMaxLimit` - Get max todos limit
- `Get-TodoModuleVersion` - Get module version

## Classes Available
- `TodoItem` - Individual todo item
- `TodoList` - Todo list container
- `TodoCommandParser` - Command parser
- `AgenticTodoCreator` - AI todo generator
- `Win32TodoBridge` - Win32 integration bridge
