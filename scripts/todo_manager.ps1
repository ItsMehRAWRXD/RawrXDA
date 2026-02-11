#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Dynamic, Persistent, and Static Todo Management System

.DESCRIPTION
    Advanced todo management system with:
    - Dynamic: Agentically created by AI/models
    - Persistent: JSON storage with auto-save/load
    - Static: User-defined via !todos command
    - Maximum 25 todos per list
    - Priority levels, status tracking, timestamps
    - Win32IDE integration ready
    - Agent-driven todo generation and execution

.PARAMETER Operation
    add, list, complete, delete, clear, parse, agentic-create, export, import

.EXAMPLE
    .\todo_manager.ps1 -Operation parse -Command "!todos 1. Fix bug 2. Add feature 3. Test code"
    
.EXAMPLE
    .\todo_manager.ps1 -Operation agentic-create -Context "Build REST API server"
    
.EXAMPLE
    .\todo_manager.ps1 -Operation add -TodoText "Implement authentication" -Priority High
    
.EXAMPLE
    .\todo_manager.ps1 -Operation list -Status pending
#>

param(
    [Parameter(Mandatory=$false)]
    [ValidateSet('add', 'list', 'complete', 'delete', 'clear', 'parse', 'agentic-create', 'export', 'import', 'update', 'reorder')]
    [string]$Operation = "list",
    
    [string]$TodoText = "",
    [string]$Command = "",
    [string]$Context = "",
    [int]$TodoId = 0,
    [ValidateSet('Low', 'Medium', 'High', 'Critical')]
    [string]$Priority = "Medium",
    [ValidateSet('pending', 'in-progress', 'completed', 'blocked', 'cancelled')]
    [string]$Status = "pending",
    [string]$StoragePath = "D:\lazy init ide\data\todos.json",
    [string]$ExportPath = "",
    [switch]$AgenticMode,
    [switch]$Watch,
    [switch]$Verbose
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# TODO DATA STRUCTURES
# ═══════════════════════════════════════════════════════════════════════════════

$script:MAX_TODOS = 25
$script:VERSION = "1.0.0"

class TodoItem {
    [int]$Id
    [string]$Text
    [string]$Priority
    [string]$Status
    [string]$Category
    [string]$Source  # "user", "agentic", "parsed"
    [datetime]$CreatedAt
    [datetime]$UpdatedAt
    [datetime]$CompletedAt
    [string[]]$Tags
    [hashtable]$Metadata
    [int]$EstimatedMinutes
    [int]$ActualMinutes
    
    TodoItem([string]$text) {
        $this.Text = $text
        $this.Priority = "Medium"
        $this.Status = "pending"
        $this.Category = "general"
        $this.Source = "user"
        $this.CreatedAt = Get-Date
        $this.UpdatedAt = Get-Date
        $this.Tags = @()
        $this.Metadata = @{}
        $this.EstimatedMinutes = 0
        $this.ActualMinutes = 0
    }
    
    [string] ToString() {
        $statusIcon = switch ($this.Status) {
            "pending" { "⏳" }
            "in-progress" { "🔄" }
            "completed" { "✅" }
            "blocked" { "🚫" }
            "cancelled" { "❌" }
            default { "❓" }
        }
        
        $priorityIcon = switch ($this.Priority) {
            "Critical" { "🔴" }
            "High" { "🟠" }
            "Medium" { "🟡" }
            "Low" { "🟢" }
            default { "⚪" }
        }
        
        return "$statusIcon $priorityIcon [$($this.Id)] $($this.Text)"
    }
    
    [hashtable] ToHashtable() {
        return @{
            Id = $this.Id
            Text = $this.Text
            Priority = $this.Priority
            Status = $this.Status
            Category = $this.Category
            Source = $this.Source
            CreatedAt = $this.CreatedAt.ToString("o")
            UpdatedAt = $this.UpdatedAt.ToString("o")
            CompletedAt = if ($this.CompletedAt) { $this.CompletedAt.ToString("o") } else { $null }
            Tags = $this.Tags
            Metadata = $this.Metadata
            EstimatedMinutes = $this.EstimatedMinutes
            ActualMinutes = $this.ActualMinutes
        }
    }
    
    static [TodoItem] FromHashtable([hashtable]$data) {
        $todo = [TodoItem]::new($data.Text)
        $todo.Id = $data.Id
        $todo.Priority = $data.Priority
        $todo.Status = $data.Status
        $todo.Category = $data.Category
        $todo.Source = $data.Source
        $todo.CreatedAt = [datetime]::Parse($data.CreatedAt)
        $todo.UpdatedAt = [datetime]::Parse($data.UpdatedAt)
        if ($data.CompletedAt) {
            $todo.CompletedAt = [datetime]::Parse($data.CompletedAt)
        }
        $todo.Tags = $data.Tags
        $todo.Metadata = $data.Metadata
        $todo.EstimatedMinutes = $data.EstimatedMinutes
        $todo.ActualMinutes = $data.ActualMinutes
        return $todo
    }
}

class TodoList {
    [TodoItem[]]$Items
    [int]$MaxItems
    [string]$StoragePath
    [datetime]$LastSaved
    [datetime]$LastLoaded
    [hashtable]$Statistics
    
    TodoList([string]$storagePath) {
        $this.Items = @()
        $this.MaxItems = $script:MAX_TODOS
        $this.StoragePath = $storagePath
        $this.Statistics = @{
            TotalCreated = 0
            TotalCompleted = 0
            TotalDeleted = 0
            AgenticCreated = 0
            UserCreated = 0
            ParsedCreated = 0
        }
        $this.Load()
    }
    
    [int] GetNextId() {
        if ($this.Items.Count -eq 0) {
            return 1
        }
        return ($this.Items | Measure-Object -Property Id -Maximum).Maximum + 1
    }
    
    [bool] CanAdd() {
        return $this.Items.Count -lt $this.MaxItems
    }
    
    [TodoItem] Add([string]$text, [string]$priority, [string]$source) {
        if (-not $this.CanAdd()) {
            throw "Cannot add todo: Maximum of $($this.MaxItems) todos reached"
        }
        
        $todo = [TodoItem]::new($text)
        $todo.Id = $this.GetNextId()
        $todo.Priority = $priority
        $todo.Source = $source
        
        $this.Items += $todo
        $this.Statistics.TotalCreated++
        
        switch ($source) {
            "agentic" { $this.Statistics.AgenticCreated++ }
            "user" { $this.Statistics.UserCreated++ }
            "parsed" { $this.Statistics.ParsedCreated++ }
        }
        
        $this.Save()
        return $todo
    }
    
    [TodoItem] Get([int]$id) {
        return $this.Items | Where-Object { $_.Id -eq $id } | Select-Object -First 1
    }
    
    [TodoItem[]] GetAll([string]$filterStatus) {
        if ($filterStatus) {
            return $this.Items | Where-Object { $_.Status -eq $filterStatus }
        }
        return $this.Items
    }
    
    [bool] Complete([int]$id) {
        $todo = $this.Get($id)
        if (-not $todo) {
            return $false
        }
        
        $todo.Status = "completed"
        $todo.CompletedAt = Get-Date
        $todo.UpdatedAt = Get-Date
        $this.Statistics.TotalCompleted++
        
        $this.Save()
        return $true
    }
    
    [bool] Update([int]$id, [hashtable]$updates) {
        $todo = $this.Get($id)
        if (-not $todo) {
            return $false
        }
        
        if ($updates.ContainsKey("Text")) { $todo.Text = $updates.Text }
        if ($updates.ContainsKey("Priority")) { $todo.Priority = $updates.Priority }
        if ($updates.ContainsKey("Status")) { $todo.Status = $updates.Status }
        if ($updates.ContainsKey("Category")) { $todo.Category = $updates.Category }
        if ($updates.ContainsKey("Tags")) { $todo.Tags = $updates.Tags }
        if ($updates.ContainsKey("EstimatedMinutes")) { $todo.EstimatedMinutes = $updates.EstimatedMinutes }
        
        $todo.UpdatedAt = Get-Date
        
        $this.Save()
        return $true
    }
    
    [bool] Delete([int]$id) {
        $todo = $this.Get($id)
        if (-not $todo) {
            return $false
        }
        
        $this.Items = $this.Items | Where-Object { $_.Id -ne $id }
        $this.Statistics.TotalDeleted++
        
        $this.Save()
        return $true
    }
    
    [void] Clear() {
        $this.Items = @()
        $this.Save()
    }
    
    [void] Save() {
        $storageDir = Split-Path $this.StoragePath -Parent
        if (-not (Test-Path $storageDir)) {
            New-Item -ItemType Directory -Path $storageDir -Force | Out-Null
        }
        
        $data = @{
            Version = $script:VERSION
            SavedAt = (Get-Date).ToString("o")
            MaxItems = $this.MaxItems
            Statistics = $this.Statistics
            Items = @($this.Items | ForEach-Object { $_.ToHashtable() })
        }
        
        $json = $data | ConvertTo-Json -Depth 10
        $json | Set-Content $this.StoragePath -Encoding UTF8
        
        $this.LastSaved = Get-Date
    }
    
    [void] Load() {
        if (-not (Test-Path $this.StoragePath)) {
            return
        }
        
        try {
            $json = Get-Content $this.StoragePath -Raw -Encoding UTF8
            $data = $json | ConvertFrom-Json -AsHashtable
            
            $this.Statistics = $data.Statistics
            $this.Items = @()
            
            foreach ($itemData in $data.Items) {
                $this.Items += [TodoItem]::FromHashtable($itemData)
            }
            
            $this.LastLoaded = Get-Date
        }
        catch {
            Write-Warning "Failed to load todos: $_"
        }
    }
    
    [void] Display([bool]$verbose) {
        Write-Host "`n╔════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║                         TODO LIST                                  ║" -ForegroundColor Cyan
        Write-Host "╚════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
        
        Write-Host "`n  Total: $($this.Items.Count)/$($this.MaxItems)" -ForegroundColor Gray
        
        $pending = ($this.Items | Where-Object { $_.Status -eq "pending" }).Count
        $inProgress = ($this.Items | Where-Object { $_.Status -eq "in-progress" }).Count
        $completed = ($this.Items | Where-Object { $_.Status -eq "completed" }).Count
        $blocked = ($this.Items | Where-Object { $_.Status -eq "blocked" }).Count
        
        Write-Host "  Pending: $pending | In Progress: $inProgress | Completed: $completed | Blocked: $blocked" -ForegroundColor Gray
        
        if ($this.Items.Count -eq 0) {
            Write-Host "`n  No todos yet. Add one with: todo_manager.ps1 -Operation add -TodoText 'Your task'" -ForegroundColor Yellow
            return
        }
        
        Write-Host ""
        
        # Group by status
        $groups = $this.Items | Group-Object -Property Status
        
        foreach ($group in $groups) {
            $statusColor = switch ($group.Name) {
                "pending" { "Yellow" }
                "in-progress" { "Cyan" }
                "completed" { "Green" }
                "blocked" { "Red" }
                "cancelled" { "DarkGray" }
                default { "White" }
            }
            
            Write-Host "  $($group.Name.ToUpper())" -ForegroundColor $statusColor
            Write-Host "  " + ("─" * 70) -ForegroundColor DarkGray
            
            foreach ($todo in $group.Group | Sort-Object Priority, Id) {
                Write-Host "  $($todo.ToString())" -ForegroundColor $statusColor
                
                if ($verbose) {
                    Write-Host "     Category: $($todo.Category) | Source: $($todo.Source)" -ForegroundColor DarkGray
                    Write-Host "     Created: $($todo.CreatedAt.ToString('yyyy-MM-dd HH:mm'))" -ForegroundColor DarkGray
                    if ($todo.Tags.Count -gt 0) {
                        Write-Host "     Tags: $($todo.Tags -join ', ')" -ForegroundColor DarkGray
                    }
                    if ($todo.EstimatedMinutes -gt 0) {
                        Write-Host "     Estimated: $($todo.EstimatedMinutes) minutes" -ForegroundColor DarkGray
                    }
                }
            }
            Write-Host ""
        }
        
        if ($verbose) {
            Write-Host "  STATISTICS" -ForegroundColor Magenta
            Write-Host "  " + ("─" * 70) -ForegroundColor DarkGray
            Write-Host "  Total Created: $($this.Statistics.TotalCreated)" -ForegroundColor Gray
            Write-Host "  Total Completed: $($this.Statistics.TotalCompleted)" -ForegroundColor Gray
            Write-Host "  Total Deleted: $($this.Statistics.TotalDeleted)" -ForegroundColor Gray
            Write-Host "  Agentic: $($this.Statistics.AgenticCreated) | User: $($this.Statistics.UserCreated) | Parsed: $($this.Statistics.ParsedCreated)" -ForegroundColor Gray
            Write-Host ""
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# COMMAND PARSER FOR !todos SYNTAX
# ═══════════════════════════════════════════════════════════════════════════════

class TodoCommandParser {
    static [TodoItem[]] Parse([string]$command) {
        $todos = @()
        
        # Remove !todos prefix if present
        $command = $command -replace '^!todos\s*', ''
        
        # Parse numbered list: "1. Task one 2. Task two 3. Task three"
        $pattern = '\d+\.\s*([^0-9]+?)(?=\d+\.|$)'
        $matches = [regex]::Matches($command, $pattern)
        
        foreach ($match in $matches) {
            $text = $match.Groups[1].Value.Trim()
            if ($text) {
                $todo = [TodoItem]::new($text)
                $todo.Source = "parsed"
                $todos += $todo
            }
        }
        
        # If no numbered format, try bullet points or comma-separated
        if ($todos.Count -eq 0) {
            # Try bullet points: "- Task one - Task two"
            $bulletPattern = '[-•]\s*([^-•]+)'
            $bulletMatches = [regex]::Matches($command, $bulletPattern)
            
            if ($bulletMatches.Count -gt 0) {
                foreach ($match in $bulletMatches) {
                    $text = $match.Groups[1].Value.Trim()
                    if ($text) {
                        $todo = [TodoItem]::new($text)
                        $todo.Source = "parsed"
                        $todos += $todo
                    }
                }
            }
            else {
                # Try comma-separated: "Task one, Task two, Task three"
                $parts = $command -split '[,;]'
                foreach ($part in $parts) {
                    $text = $part.Trim()
                    if ($text) {
                        $todo = [TodoItem]::new($text)
                        $todo.Source = "parsed"
                        $todos += $todo
                    }
                }
            }
        }
        
        return $todos
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# AGENTIC TODO CREATOR
# ═══════════════════════════════════════════════════════════════════════════════

class AgenticTodoCreator {
    [string]$Context
    [hashtable]$Templates
    
    AgenticTodoCreator([string]$context) {
        $this.Context = $context
        $this.InitializeTemplates()
    }
    
    [void] InitializeTemplates() {
        $this.Templates = @{
            "api" = @(
                "Design API endpoints and routes"
                "Implement request/response models"
                "Add authentication and authorization"
                "Write API documentation"
                "Add error handling and validation"
                "Create unit tests for API"
                "Add rate limiting and security"
            )
            "feature" = @(
                "Analyze feature requirements"
                "Design architecture and components"
                "Implement core functionality"
                "Add user interface elements"
                "Write unit and integration tests"
                "Update documentation"
                "Perform code review"
            )
            "bug" = @(
                "Reproduce and document bug"
                "Identify root cause"
                "Implement fix"
                "Add regression test"
                "Verify fix in all scenarios"
                "Update changelog"
            )
            "refactor" = @(
                "Analyze code for improvements"
                "Extract reusable components"
                "Improve naming and structure"
                "Add missing documentation"
                "Run full test suite"
                "Measure performance impact"
            )
            "model" = @(
                "Define model architecture"
                "Prepare training dataset"
                "Implement training pipeline"
                "Run training and validation"
                "Evaluate model performance"
                "Optimize hyperparameters"
                "Deploy model to production"
            )
        }
    }
    
    [TodoItem[]] Generate() {
        $todos = @()
        
        # Analyze context to determine type
        $type = $this.DetectType()
        
        Write-Host "`n  🤖 Agentic Todo Creator" -ForegroundColor Cyan
        Write-Host "  Context: $($this.Context)" -ForegroundColor Gray
        Write-Host "  Detected Type: $type" -ForegroundColor Gray
        
        # Get appropriate template
        $template = $this.Templates[$type]
        if (-not $template) {
            $template = $this.Templates["feature"]
        }
        
        # Generate todos from template
        foreach ($taskText in $template) {
            $todo = [TodoItem]::new($taskText)
            $todo.Source = "agentic"
            $todo.Category = $type
            $todo.Metadata["GeneratedFrom"] = $this.Context
            $todo.Metadata["GeneratedAt"] = (Get-Date).ToString("o")
            $todos += $todo
        }
        
        # Add context-specific todos
        $contextTodos = $this.GenerateContextSpecific()
        $todos += $contextTodos
        
        Write-Host "  Generated: $($todos.Count) todos" -ForegroundColor Green
        
        return $todos
    }
    
    [string] DetectType() {
        $lower = $this.Context.ToLower()
        
        if ($lower -match 'api|rest|endpoint|route') { return "api" }
        if ($lower -match 'bug|fix|error|issue') { return "bug" }
        if ($lower -match 'refactor|clean|improve|optimize') { return "refactor" }
        if ($lower -match 'model|train|ml|ai|neural') { return "model" }
        
        return "feature"
    }
    
    [TodoItem[]] GenerateContextSpecific() {
        $todos = @()
        
        # Extract specific items from context
        if ($this.Context -match 'implement (\w+)') {
            $feature = $matches[1]
            $todo = [TodoItem]::new("Implement $feature functionality")
            $todo.Source = "agentic"
            $todo.Priority = "High"
            $todos += $todo
        }
        
        if ($this.Context -match 'test') {
            $todo = [TodoItem]::new("Create comprehensive test suite")
            $todo.Source = "agentic"
            $todos += $todo
        }
        
        return $todos
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# WIN32 INTEGRATION HELPERS
# ═══════════════════════════════════════════════════════════════════════════════

class Win32TodoBridge {
    static [string] ExportForWin32([TodoList]$todoList) {
        # Export in format Win32IDE can consume
        $export = @{
            Version = $script:VERSION
            Timestamp = (Get-Date).ToString("o")
            Count = $todoList.Items.Count
            MaxCount = $todoList.MaxItems
            Items = @()
        }
        
        foreach ($todo in $todoList.Items) {
            $export.Items += @{
                Id = $todo.Id
                Text = $todo.Text
                Status = $todo.Status
                Priority = $todo.Priority
                Icon = switch ($todo.Status) {
                    "pending" { "PENDING" }
                    "in-progress" { "INPROGRESS" }
                    "completed" { "COMPLETED" }
                    "blocked" { "BLOCKED" }
                    default { "UNKNOWN" }
                }
            }
        }
        
        return $export | ConvertTo-Json -Depth 10 -Compress
    }
    
    static [void] WriteToWin32Pipe([string]$data) {
        # In real implementation, write to named pipe for Win32IDE
        $pipePath = "\\.\pipe\RawrXD_Todos"
        
        try {
            # Placeholder for actual Win32 pipe communication
            Write-Verbose "Would write to Win32 pipe: $pipePath"
        }
        catch {
            Write-Warning "Failed to communicate with Win32IDE: $_"
        }
    }
}

# ═══════════════════════════════════════════════════════════════════════════════
# MAIN EXECUTION
# ═══════════════════════════════════════════════════════════════════════════════

$todoList = [TodoList]::new($StoragePath)

switch ($Operation) {
    "add" {
        if (-not $TodoText) {
            throw "TodoText parameter required for add operation"
        }
        
        $todo = $todoList.Add($TodoText, $Priority, "user")
        Write-Host "`n✅ Added todo #$($todo.Id): $($todo.Text)" -ForegroundColor Green
        Write-Host "   Priority: $Priority | Status: pending" -ForegroundColor Gray
    }
    
    "list" {
        $filter = if ($Status -ne "pending") { $Status } else { "" }
        $todoList.Display($Verbose)
    }
    
    "complete" {
        if ($TodoId -eq 0) {
            throw "TodoId parameter required for complete operation"
        }
        
        if ($todoList.Complete($TodoId)) {
            Write-Host "`n✅ Completed todo #$TodoId" -ForegroundColor Green
        }
        else {
            Write-Host "`n❌ Todo #$TodoId not found" -ForegroundColor Red
        }
    }
    
    "update" {
        if ($TodoId -eq 0) {
            throw "TodoId parameter required for update operation"
        }
        
        $updates = @{}
        if ($TodoText) { $updates.Text = $TodoText }
        if ($Priority) { $updates.Priority = $Priority }
        if ($Status) { $updates.Status = $Status }
        
        if ($todoList.Update($TodoId, $updates)) {
            Write-Host "`n✅ Updated todo #$TodoId" -ForegroundColor Green
        }
        else {
            Write-Host "`n❌ Todo #$TodoId not found" -ForegroundColor Red
        }
    }
    
    "delete" {
        if ($TodoId -eq 0) {
            throw "TodoId parameter required for delete operation"
        }
        
        if ($todoList.Delete($TodoId)) {
            Write-Host "`n✅ Deleted todo #$TodoId" -ForegroundColor Green
        }
        else {
            Write-Host "`n❌ Todo #$TodoId not found" -ForegroundColor Red
        }
    }
    
    "clear" {
        Write-Host "`n⚠️  Are you sure you want to clear all todos? (y/N): " -NoNewline -ForegroundColor Yellow
        $confirm = Read-Host
        if ($confirm -eq 'y' -or $confirm -eq 'Y') {
            $todoList.Clear()
            Write-Host "✅ All todos cleared" -ForegroundColor Green
        }
        else {
            Write-Host "❌ Cancelled" -ForegroundColor Gray
        }
    }
    
    "parse" {
        if (-not $Command) {
            throw "Command parameter required for parse operation. Use: -Command '!todos 1. Task one 2. Task two'"
        }
        
        Write-Host "`n📝 Parsing command..." -ForegroundColor Cyan
        $parsedTodos = [TodoCommandParser]::Parse($Command)
        
        if ($parsedTodos.Count -eq 0) {
            Write-Host "❌ No todos found in command" -ForegroundColor Red
            Write-Host "   Expected format: !todos 1. First task 2. Second task 3. Third task" -ForegroundColor Yellow
            return
        }
        
        Write-Host "✅ Parsed $($parsedTodos.Count) todos:" -ForegroundColor Green
        
        $added = 0
        foreach ($todo in $parsedTodos) {
            if ($todoList.CanAdd()) {
                $addedTodo = $todoList.Add($todo.Text, $Priority, "parsed")
                Write-Host "   [$($addedTodo.Id)] $($addedTodo.Text)" -ForegroundColor Gray
                $added++
            }
            else {
                Write-Host "   ⚠️  Cannot add more todos (limit: $($todoList.MaxItems))" -ForegroundColor Yellow
                break
            }
        }
        
        Write-Host "`n✅ Added $added todos to list" -ForegroundColor Green
    }
    
    "agentic-create" {
        if (-not $Context) {
            throw "Context parameter required for agentic-create operation"
        }
        
        $creator = [AgenticTodoCreator]::new($Context)
        $agenticTodos = $creator.Generate()
        
        Write-Host "`n📝 Adding agentic todos..." -ForegroundColor Cyan
        
        $added = 0
        foreach ($todo in $agenticTodos) {
            if ($todoList.CanAdd()) {
                $addedTodo = $todoList.Add($todo.Text, $todo.Priority, "agentic")
                $addedTodo.Category = $todo.Category
                $addedTodo.Metadata = $todo.Metadata
                Write-Host "   [$($addedTodo.Id)] $($addedTodo.Text)" -ForegroundColor Gray
                $added++
            }
            else {
                Write-Host "   ⚠️  Cannot add more todos (limit: $($todoList.MaxItems))" -ForegroundColor Yellow
                break
            }
        }
        
        Write-Host "`n✅ Added $added agentic todos" -ForegroundColor Green
    }
    
    "export" {
        if (-not $ExportPath) {
            $ExportPath = "D:\lazy init ide\data\todos_export_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
        }
        
        $todoList.Save()
        Copy-Item $StoragePath $ExportPath -Force
        
        Write-Host "`n✅ Exported todos to: $ExportPath" -ForegroundColor Green
        
        # Also export Win32 format
        $win32Data = [Win32TodoBridge]::ExportForWin32($todoList)
        $win32Path = $ExportPath -replace '\.json$', '_win32.json'
        $win32Data | Set-Content $win32Path -Encoding UTF8
        
        Write-Host "✅ Win32 format exported to: $win32Path" -ForegroundColor Green
    }
    
    "import" {
        if (-not $ExportPath -or -not (Test-Path $ExportPath)) {
            throw "Valid ExportPath required for import operation"
        }
        
        Copy-Item $ExportPath $StoragePath -Force
        $todoList.Load()
        
        Write-Host "`n✅ Imported todos from: $ExportPath" -ForegroundColor Green
        $todoList.Display($false)
    }
}

# Watch mode for continuous monitoring
if ($Watch) {
    Write-Host "`n👁️  Watch mode enabled. Press Ctrl+C to exit." -ForegroundColor Yellow
    
    while ($true) {
        Clear-Host
        $todoList.Load()
        $todoList.Display($Verbose)
        Start-Sleep -Seconds 5
    }
}

Write-Host ""
