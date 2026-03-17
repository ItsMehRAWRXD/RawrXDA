#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Todo Manager Module - Exportable Functions and Classes

.DESCRIPTION
    PowerShell module that exports all todo management functionality:
    - Classes: TodoItem, TodoList, TodoCommandParser, AgenticTodoCreator, Win32TodoBridge
    - Functions: All todo operations for programmatic access
    - Constants: MAX_TODOS, VERSION

.EXAMPLE
    Import-Module .\TodoManager.psm1
    $todoList = New-TodoList -StoragePath "D:\todos.json"
    Add-Todo -TodoList $todoList -Text "My task" -Priority High
    
.EXAMPLE
    Import-Module .\TodoManager.psm1
    $todos = Parse-TodoCommand "!todos 1. Task one 2. Task two"
    
.EXAMPLE
    Import-Module .\TodoManager.psm1
    $agenticTodos = New-AgenticTodos -Context "Build REST API"
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

# ═══════════════════════════════════════════════════════════════════════════════
# MODULE CONSTANTS
# ═══════════════════════════════════════════════════════════════════════════════

$script:MAX_TODOS = 25
$script:VERSION = "1.0.0"

# ═══════════════════════════════════════════════════════════════════════════════
# CLASSES
# ═══════════════════════════════════════════════════════════════════════════════

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
            Write-Host "`n  No todos yet. Add one with: Add-Todo -TodoList `$todoList -Text 'Your task'" -ForegroundColor Yellow
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
# EXPORTED FUNCTIONS
# ═══════════════════════════════════════════════════════════════════════════════

function New-TodoList {
    <#
    .SYNOPSIS
        Create a new TodoList instance
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$false)]
        [string]$StoragePath = "D:\lazy init ide\data\todos.json"
    )
    
    return [TodoList]::new($StoragePath)
}

function Add-Todo {
    <#
    .SYNOPSIS
        Add a new todo to the list
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList,
        
        [Parameter(Mandatory=$true)]
        [string]$Text,
        
        [Parameter(Mandatory=$false)]
        [ValidateSet('Low', 'Medium', 'High', 'Critical')]
        [string]$Priority = "Medium",
        
        [Parameter(Mandatory=$false)]
        [ValidateSet('user', 'agentic', 'parsed')]
        [string]$Source = "user"
    )
    
    return $TodoList.Add($Text, $Priority, $Source)
}

function Get-Todo {
    <#
    .SYNOPSIS
        Get a specific todo by ID
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList,
        
        [Parameter(Mandatory=$true)]
        [int]$Id
    )
    
    return $TodoList.Get($Id)
}

function Get-AllTodos {
    <#
    .SYNOPSIS
        Get all todos, optionally filtered by status
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList,
        
        [Parameter(Mandatory=$false)]
        [ValidateSet('pending', 'in-progress', 'completed', 'blocked', 'cancelled')]
        [string]$Status = ""
    )
    
    return $TodoList.GetAll($Status)
}

function Complete-Todo {
    <#
    .SYNOPSIS
        Mark a todo as completed
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList,
        
        [Parameter(Mandatory=$true)]
        [int]$Id
    )
    
    return $TodoList.Complete($Id)
}

function Update-Todo {
    <#
    .SYNOPSIS
        Update a todo's properties
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList,
        
        [Parameter(Mandatory=$true)]
        [int]$Id,
        
        [Parameter(Mandatory=$true)]
        [hashtable]$Updates
    )
    
    return $TodoList.Update($Id, $Updates)
}

function Remove-Todo {
    <#
    .SYNOPSIS
        Delete a todo
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList,
        
        [Parameter(Mandatory=$true)]
        [int]$Id
    )
    
    return $TodoList.Delete($Id)
}

function Clear-TodoList {
    <#
    .SYNOPSIS
        Clear all todos from the list
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList
    )
    
    $TodoList.Clear()
}

function Show-TodoList {
    <#
    .SYNOPSIS
        Display the todo list
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList,
        
        [Parameter(Mandatory=$false)]
        [switch]$Verbose
    )
    
    $TodoList.Display($Verbose)
}

function Save-TodoList {
    <#
    .SYNOPSIS
        Save the todo list to storage
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList
    )
    
    $TodoList.Save()
}

function Import-TodoList {
    <#
    .SYNOPSIS
        Load the todo list from storage
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList
    )
    
    $TodoList.Load()
}

function Parse-TodoCommand {
    <#
    .SYNOPSIS
        Parse a !todos command into TodoItem array
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Command
    )
    
    return [TodoCommandParser]::Parse($Command)
}

function New-AgenticTodos {
    <#
    .SYNOPSIS
        Generate todos agentically from context
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Context
    )
    
    $creator = [AgenticTodoCreator]::new($Context)
    return $creator.Generate()
}

function Export-TodosForWin32 {
    <#
    .SYNOPSIS
        Export todos in Win32IDE format
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList
    )
    
    return [Win32TodoBridge]::ExportForWin32($TodoList)
}

function Send-TodosToWin32Pipe {
    <#
    .SYNOPSIS
        Send todo data to Win32 named pipe
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Data
    )
    
    [Win32TodoBridge]::WriteToWin32Pipe($Data)
}

function Get-TodoStatistics {
    <#
    .SYNOPSIS
        Get statistics about the todo list
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList
    )
    
    return $TodoList.Statistics
}

function Test-CanAddTodo {
    <#
    .SYNOPSIS
        Check if more todos can be added
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoList]$TodoList
    )
    
    return $TodoList.CanAdd()
}

function New-TodoItem {
    <#
    .SYNOPSIS
        Create a new TodoItem instance
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Text
    )
    
    return [TodoItem]::new($Text)
}

function ConvertTo-TodoHashtable {
    <#
    .SYNOPSIS
        Convert a TodoItem to hashtable
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [TodoItem]$TodoItem
    )
    
    return $TodoItem.ToHashtable()
}

function ConvertFrom-TodoHashtable {
    <#
    .SYNOPSIS
        Create TodoItem from hashtable
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [hashtable]$Data
    )
    
    return [TodoItem]::FromHashtable($Data)
}

function Get-TodoMaxLimit {
    <#
    .SYNOPSIS
        Get the maximum number of todos allowed
    #>
    [CmdletBinding()]
    param()
    
    return $script:MAX_TODOS
}

function Get-TodoModuleVersion {
    <#
    .SYNOPSIS
        Get the module version
    #>
    [CmdletBinding()]
    param()
    
    return $script:VERSION
}

# ═══════════════════════════════════════════════════════════════════════════════
# MODULE EXPORTS
# ═══════════════════════════════════════════════════════════════════════════════

Export-ModuleMember -Function @(
    'New-TodoList',
    'Add-Todo',
    'Get-Todo',
    'Get-AllTodos',
    'Complete-Todo',
    'Update-Todo',
    'Remove-Todo',
    'Clear-TodoList',
    'Show-TodoList',
    'Save-TodoList',
    'Import-TodoList',
    'Parse-TodoCommand',
    'New-AgenticTodos',
    'Export-TodosForWin32',
    'Send-TodosToWin32Pipe',
    'Get-TodoStatistics',
    'Test-CanAddTodo',
    'New-TodoItem',
    'ConvertTo-TodoHashtable',
    'ConvertFrom-TodoHashtable',
    'Get-TodoMaxLimit',
    'Get-TodoModuleVersion'
)

Export-ModuleMember -Variable @(
    'MAX_TODOS',
    'VERSION'
)
