<#
.SYNOPSIS
    RawrXD Scheduling Module - Production-Ready Task Automation
    
.DESCRIPTION
    Provides enterprise-grade task scheduling, reminders, and automation capabilities
    with robust error handling, logging, security, monitoring, and performance optimization.
    
.NOTES
    Module: RawrXD.Scheduling
    Version: 1.0.0
    Author: RawrXD Development Team
    Generated: 2025-11-25
    
    Features:
    - Task scheduling with recurrence patterns
    - Reminder notifications
    - Error handling and logging
    - Security and access control
    - Performance monitoring
    - Task persistence
#>

# Module metadata
$script:ModuleName = 'RawrXD.Scheduling'
$script:ModuleVersion = '1.0.0'
$script:ModulePath = $PSScriptRoot

# Module configuration
$script:SchedulingConfig = @{
    StoragePath = Join-Path $PSScriptRoot "..\..\config\scheduled-tasks.json"
    MaxConcurrentTasks = 10
    TaskTimeoutMinutes = 30
    EnablePersistence = $true
    EnableAuditing = $true
    EnableMonitoring = $true
    RetryAttempts = 3
    RetryBackoffSeconds = 5
}

# Task storage (in-memory cache)
$script:TaskStorage = @{
    Tasks = @{}
    Reminders = @{}
    LastUpdate = Get-Date
}

# Performance metrics
$script:PerformanceMetrics = @{
    TasksCreated = 0
    TasksExecuted = 0
    TasksFailed = 0
    AverageExecutionTime = 0
    LastExecutionTime = $null
}

# Security context
$script:SecurityContext = @{
    RequireAuthentication = $true
    AllowedUsers = @()
    AuditLogPath = Join-Path $PSScriptRoot "..\..\logs\scheduling-audit.log"
}

#region Helper Functions

function Write-SchedulingLog {
    <#
    .SYNOPSIS
        Internal logging function for scheduling module
    #>
    param(
        [Parameter(Mandatory)]
        [string]$Message,
        
        [ValidateSet('INFO', 'WARNING', 'ERROR', 'SUCCESS', 'DEBUG')]
        [string]$Level = 'INFO',
        
        [hashtable]$Metadata = @{}
    )
    
    $timestamp = Get-Date -Format 'yyyy-MM-dd HH:mm:ss'
    $logEntry = "[$timestamp] [$Level] $Message"
    
    if ($Metadata.Count -gt 0) {
        $logEntry += " | Metadata: $($Metadata | ConvertTo-Json -Compress)"
    }
    
    # Write to console
    $color = switch ($Level) {
        'ERROR' { 'Red' }
        'WARNING' { 'Yellow' }
        'SUCCESS' { 'Green' }
        'DEBUG' { 'Gray' }
        default { 'Cyan' }
    }
    Write-Host $logEntry -ForegroundColor $color
    
    # Write to audit log if enabled
    if ($script:SchedulingConfig.EnableAuditing) {
        try {
            $auditDir = Split-Path -Path $script:SecurityContext.AuditLogPath -Parent
            if (-not (Test-Path $auditDir)) {
                New-Item -ItemType Directory -Path $auditDir -Force | Out-Null
            }
            Add-Content -Path $script:SecurityContext.AuditLogPath -Value $logEntry -Encoding UTF8 -ErrorAction SilentlyContinue
        }
        catch {
            # Silently fail if audit logging fails
        }
    }
    
    # Use RawrXD logging if available
    if (Get-Command Write-StartupLog -ErrorAction SilentlyContinue) {
        Write-StartupLog $Message $Level
    }
}

function Test-SecurityAccess {
    <#
    .SYNOPSIS
        Validates user has permission to perform scheduling operations
    #>
    param([string]$Operation)
    
    if (-not $script:SecurityContext.RequireAuthentication) {
        return $true
    }
    
    $currentUser = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
    
    if ($script:SecurityContext.AllowedUsers.Count -eq 0) {
        # No restrictions
        return $true
    }
    
    if ($script:SecurityContext.AllowedUsers -contains $currentUser) {
        return $true
    }
    
    Write-SchedulingLog "Access denied for user $currentUser attempting operation: $Operation" "ERROR" @{
        User = $currentUser
        Operation = $Operation
    }
    
    return $false
}

function Save-TaskStorage {
    <#
    .SYNOPSIS
        Persists task storage to disk
    #>
    if (-not $script:SchedulingConfig.EnablePersistence) {
        return
    }
    
    try {
        $storageDir = Split-Path -Path $script:SchedulingConfig.StoragePath -Parent
        if (-not (Test-Path $storageDir)) {
            New-Item -ItemType Directory -Path $storageDir -Force | Out-Null
        }
        
        $storageData = @{
            Tasks = $script:TaskStorage.Tasks
            Reminders = $script:TaskStorage.Reminders
            LastUpdate = (Get-Date).ToString('o')
            Version = $script:ModuleVersion
        }
        
        $storageData | ConvertTo-Json -Depth 10 | Set-Content -Path $script:SchedulingConfig.StoragePath -Encoding UTF8 -Force
        
        Write-SchedulingLog "Task storage persisted successfully" "DEBUG"
    }
    catch {
        Write-SchedulingLog "Failed to persist task storage: $($_.Exception.Message)" "ERROR" @{
            Error = $_.Exception.Message
            StackTrace = $_.ScriptStackTrace
        }
    }
}

function Load-TaskStorage {
    <#
    .SYNOPSIS
        Loads task storage from disk
    #>
    if (-not $script:SchedulingConfig.EnablePersistence) {
        return
    }
    
    if (-not (Test-Path $script:SchedulingConfig.StoragePath)) {
        Write-SchedulingLog "Task storage file not found, starting with empty storage" "INFO"
        return
    }
    
    try {
        $storageData = Get-Content -Path $script:SchedulingConfig.StoragePath -Raw -Encoding UTF8 | ConvertFrom-Json
        
        if ($storageData.Tasks) {
            $script:TaskStorage.Tasks = @{}
            foreach ($task in $storageData.Tasks.PSObject.Properties) {
                $script:TaskStorage.Tasks[$task.Name] = $task.Value
            }
        }
        
        if ($storageData.Reminders) {
            $script:TaskStorage.Reminders = @{}
            foreach ($reminder in $storageData.Reminders.PSObject.Properties) {
                $script:TaskStorage.Reminders[$reminder.Name] = $reminder.Value
            }
        }
        
        Write-SchedulingLog "Task storage loaded: $($script:TaskStorage.Tasks.Count) tasks, $($script:TaskStorage.Reminders.Count) reminders" "INFO"
    }
    catch {
        Write-SchedulingLog "Failed to load task storage: $($_.Exception.Message)" "ERROR"
    }
}

function Update-PerformanceMetrics {
    <#
    .SYNOPSIS
        Updates performance metrics for monitoring
    #>
    param(
        [string]$Operation,
        [double]$ExecutionTimeMs = 0,
        [bool]$Success = $true
    )
    
    $script:PerformanceMetrics.LastExecutionTime = Get-Date
    
    switch ($Operation) {
        'Created' {
            $script:PerformanceMetrics.TasksCreated++
        }
        'Executed' {
            $script:PerformanceMetrics.TasksExecuted++
            if ($ExecutionTimeMs -gt 0) {
                $currentAvg = $script:PerformanceMetrics.AverageExecutionTime
                $count = $script:PerformanceMetrics.TasksExecuted
                $script:PerformanceMetrics.AverageExecutionTime = (($currentAvg * ($count - 1)) + $ExecutionTimeMs) / $count
            }
        }
        'Failed' {
            $script:PerformanceMetrics.TasksFailed++
        }
    }
    
    if ($script:SchedulingConfig.EnableMonitoring) {
        Write-SchedulingLog "Performance metrics updated: $Operation" "DEBUG" @{
            Operation = $Operation
            ExecutionTimeMs = $ExecutionTimeMs
            Success = $Success
        }
    }
}

#endregion

#region Public Functions

function New-ScheduledTask {
    <#
    .SYNOPSIS
        Creates a new scheduled task with the specified name, description, and start time.
    
    .DESCRIPTION
        Creates a production-ready scheduled task with validation, error handling, and persistence.
        Supports recurrence patterns and integrates with RawrXD's task management system.
    
    .PARAMETER Name
        The name of the scheduled task. Must be unique.
    
    .PARAMETER Description
        A brief description of the scheduled task.
    
    .PARAMETER StartTime
        The start time for the scheduled task. Must be in the future.
    
    .PARAMETER RecurrencePattern
        The recurrence pattern for the scheduled task. Options: 'Once', 'Daily', 'Weekly', 'Monthly', 'Yearly'.
    
    .PARAMETER RecurrenceInterval
        Interval for recurrence (e.g., every 2 days, every 3 weeks).
    
    .PARAMETER EndTime
        Optional end time for recurring tasks.
    
    .PARAMETER ScriptBlock
        Optional script block to execute when task runs.
    
    .PARAMETER Priority
        Task priority: 'Low', 'Normal', 'High', 'Critical'.
    
    .PARAMETER Enabled
        Whether the task is enabled immediately.
    
    .EXAMPLE
        $task = New-ScheduledTask -Name "Daily Backup" -Description "Daily system backup" `
            -StartTime (Get-Date).AddDays(1) -RecurrencePattern "Daily"
    
    .EXAMPLE
        $task = New-ScheduledTask -Name "Weekly Report" -Description "Generate weekly report" `
            -StartTime (Get-Date).AddDays(7) -RecurrencePattern "Weekly" -RecurrenceInterval 1 `
            -ScriptBlock { Write-Host "Generating report..." }
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [ValidateNotNullOrEmpty()]
        [string]$Name,
        
        [Parameter(Mandatory = $false)]
        [string]$Description = "",
        
        [Parameter(Mandatory)]
        [ValidateScript({
            if ($_ -lt (Get-Date)) {
                throw "StartTime must be in the future"
            }
            return $true
        })]
        [DateTime]$StartTime,
        
        [Parameter(Mandatory = $false)]
        [ValidateSet('Once', 'Daily', 'Weekly', 'Monthly', 'Yearly')]
        [string]$RecurrencePattern = 'Once',
        
        [Parameter(Mandatory = $false)]
        [ValidateRange(1, [int]::MaxValue)]
        [int]$RecurrenceInterval = 1,
        
        [Parameter(Mandatory = $false)]
        [DateTime]$EndTime,
        
        [Parameter(Mandatory = $false)]
        [scriptblock]$ScriptBlock,
        
        [Parameter(Mandatory = $false)]
        [ValidateSet('Low', 'Normal', 'High', 'Critical')]
        [string]$Priority = 'Normal',
        
        [Parameter(Mandatory = $false)]
        [bool]$Enabled = $true
    )
    
    begin {
        if (-not (Test-SecurityAccess -Operation "New-ScheduledTask")) {
            throw "Access denied: Insufficient permissions to create scheduled tasks"
        }
    }
    
    process {
        try {
            # Validate task name uniqueness
            if ($script:TaskStorage.Tasks.ContainsKey($Name)) {
                throw "A task with the name '$Name' already exists"
            }
            
            # Validate end time if provided
            if ($EndTime -and $EndTime -le $StartTime) {
                throw "EndTime must be after StartTime"
            }
            
            # Create task object
            $taskId = [guid]::NewGuid().ToString()
            $task = @{
                Id = $taskId
                Name = $Name
                Description = $Description
                StartTime = $StartTime
                RecurrencePattern = $RecurrencePattern
                RecurrenceInterval = $RecurrenceInterval
                EndTime = $EndTime
                ScriptBlock = $ScriptBlock
                Priority = $Priority
                Enabled = $Enabled
                Status = if ($Enabled) { 'Scheduled' } else { 'Disabled' }
                CreatedAt = Get-Date
                LastRun = $null
                NextRun = $StartTime
                RunCount = 0
                FailureCount = 0
                LastError = $null
                Metadata = @{
                    CreatedBy = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
                    ModuleVersion = $script:ModuleVersion
                }
            }
            
            # Store task
            $script:TaskStorage.Tasks[$Name] = $task
            $script:TaskStorage.LastUpdate = Get-Date
            
            # Persist to disk
            Save-TaskStorage
            
            # Update metrics
            Update-PerformanceMetrics -Operation 'Created'
            
            # Log creation
            Write-SchedulingLog "Scheduled task created: $Name" "SUCCESS" @{
                TaskId = $taskId
                StartTime = $StartTime.ToString('o')
                RecurrencePattern = $RecurrencePattern
                Priority = $Priority
            }
            
            # Integrate with RawrXD task registry if available
            if (Get-Variable -Name 'script:TaskRegistry' -ErrorAction SilentlyContinue) {
                $script:TaskRegistry.ScheduledTasks[$taskId] = $task
            }
            
            return $task
        }
        catch {
            # Don't log expected validation errors as ERROR level
            $isValidationError = $_.Exception.Message -match "already exists|must be in the future|must be after"
            
            if ($isValidationError) {
                Write-SchedulingLog "Task creation validation failed: $($_.Exception.Message)" "WARNING" @{
                    TaskName = $Name
                }
            }
            else {
                Write-SchedulingLog "Failed to create scheduled task '$Name': $($_.Exception.Message)" "ERROR" @{
                    Error = $_.Exception.Message
                    StackTrace = $_.ScriptStackTrace
                    TaskName = $Name
                }
            }
            throw
        }
    }
}

function Get-ScheduledTask {
    <#
    .SYNOPSIS
        Retrieves the details of an existing scheduled task by its ID or name.
    
    .DESCRIPTION
        Retrieves task information with optional filtering and detailed output.
    
    .PARAMETER TaskId
        The ID of the scheduled task.
    
    .PARAMETER TaskName
        The name of the scheduled task.
    
    .PARAMETER All
        Retrieve all scheduled tasks.
    
    .PARAMETER Status
        Filter tasks by status: 'Scheduled', 'Running', 'Completed', 'Failed', 'Disabled'.
    
    .EXAMPLE
        $task = Get-ScheduledTask -TaskName "Daily Backup"
    
    .EXAMPLE
        $allTasks = Get-ScheduledTask -All
    
    .EXAMPLE
        $activeTasks = Get-ScheduledTask -All -Status "Scheduled"
    #>
    [CmdletBinding(DefaultParameterSetName = 'ByName')]
    param(
        [Parameter(ParameterSetName = 'ById', Mandatory)]
        [string]$TaskId,
        
        [Parameter(ParameterSetName = 'ByName', Mandatory)]
        [string]$TaskName,
        
        [Parameter(ParameterSetName = 'All')]
        [switch]$All,
        
        [Parameter(ParameterSetName = 'All')]
        [ValidateSet('Scheduled', 'Running', 'Completed', 'Failed', 'Disabled')]
        [string]$Status
    )
    
    process {
        try {
            if ($All) {
                $tasks = $script:TaskStorage.Tasks.Values
                
                if ($Status) {
                    $tasks = $tasks | Where-Object { $_.Status -eq $Status }
                }
                
                return $tasks
            }
            elseif ($TaskId) {
                $task = $script:TaskStorage.Tasks.Values | Where-Object { $_.Id -eq $TaskId } | Select-Object -First 1
                if (-not $task) {
                    throw "Scheduled task with ID '$TaskId' not found"
                }
                return $task
            }
            else {
                if (-not $script:TaskStorage.Tasks.ContainsKey($TaskName)) {
                    throw "Scheduled task '$TaskName' not found"
                }
                return $script:TaskStorage.Tasks[$TaskName]
            }
        }
        catch {
            Write-SchedulingLog "Failed to retrieve scheduled task: $($_.Exception.Message)" "ERROR"
            throw
        }
    }
}

function Update-ScheduledTask {
    <#
    .SYNOPSIS
        Updates the details of an existing scheduled task.
    
    .DESCRIPTION
        Updates task properties with validation and error handling.
    
    .PARAMETER TaskId
        The ID of the scheduled task to update.
    
    .PARAMETER TaskName
        The name of the scheduled task to update.
    
    .PARAMETER Name
        New name for the task.
    
    .PARAMETER Description
        New description for the task.
    
    .PARAMETER StartTime
        New start time for the task.
    
    .PARAMETER Enabled
        Enable or disable the task.
    
    .PARAMETER Priority
        New priority level.
    
    .EXAMPLE
        Update-ScheduledTask -TaskName "Daily Backup" -Description "Updated description"
    
    .EXAMPLE
        Update-ScheduledTask -TaskName "Daily Backup" -Enabled $false
    #>
    [CmdletBinding()]
    param(
        [Parameter(ParameterSetName = 'ById', Mandatory)]
        [string]$TaskId,
        
        [Parameter(ParameterSetName = 'ByName', Mandatory)]
        [string]$TaskName,
        
        [Parameter(Mandatory = $false)]
        [string]$Name,
        
        [Parameter(Mandatory = $false)]
        [string]$Description,
        
        [Parameter(Mandatory = $false)]
        [DateTime]$StartTime,
        
        [Parameter(Mandatory = $false)]
        [bool]$Enabled,
        
        [Parameter(Mandatory = $false)]
        [ValidateSet('Low', 'Normal', 'High', 'Critical')]
        [string]$Priority
    )
    
    begin {
        if (-not (Test-SecurityAccess -Operation "Update-ScheduledTask")) {
            throw "Access denied: Insufficient permissions to update scheduled tasks"
        }
    }
    
    process {
        try {
            # Find task
            if ($TaskId) {
                $task = $script:TaskStorage.Tasks.Values | Where-Object { $_.Id -eq $TaskId } | Select-Object -First 1
                if (-not $task) {
                    throw "Scheduled task with ID '$TaskId' not found"
                }
                $taskName = $task.Name
            }
            else {
                if (-not $script:TaskStorage.Tasks.ContainsKey($TaskName)) {
                    throw "Scheduled task '$TaskName' not found"
                }
                $task = $script:TaskStorage.Tasks[$TaskName]
                $taskName = $TaskName
            }
            
            # Update properties
            if ($PSBoundParameters.ContainsKey('Name')) {
                if ($Name -ne $taskName -and $script:TaskStorage.Tasks.ContainsKey($Name)) {
                    throw "A task with the name '$Name' already exists"
                }
                # Rename task in storage
                $script:TaskStorage.Tasks.Remove($taskName)
                $task.Name = $Name
                $script:TaskStorage.Tasks[$Name] = $task
                $taskName = $Name
            }
            
            if ($PSBoundParameters.ContainsKey('Description')) {
                $task.Description = $Description
            }
            
            if ($PSBoundParameters.ContainsKey('StartTime')) {
                if ($StartTime -lt (Get-Date)) {
                    throw "StartTime must be in the future"
                }
                $task.StartTime = $StartTime
                $task.NextRun = $StartTime
            }
            
            if ($PSBoundParameters.ContainsKey('Enabled')) {
                $task.Enabled = $Enabled
                $task.Status = if ($Enabled) { 'Scheduled' } else { 'Disabled' }
            }
            
            if ($PSBoundParameters.ContainsKey('Priority')) {
                $task.Priority = $Priority
            }
            
            # Update metadata
            $task.Metadata.LastModified = Get-Date
            $task.Metadata.LastModifiedBy = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
            
            $script:TaskStorage.LastUpdate = Get-Date
            
            # Persist changes
            Save-TaskStorage
            
            Write-SchedulingLog "Scheduled task updated: $taskName" "SUCCESS" @{
                TaskId = $task.Id
                UpdatedFields = $PSBoundParameters.Keys
            }
            
            return $task
        }
        catch {
            Write-SchedulingLog "Failed to update scheduled task: $($_.Exception.Message)" "ERROR"
            throw
        }
    }
}

function Remove-ScheduledTask {
    <#
    .SYNOPSIS
        Deletes a scheduled task by its ID or name.
    
    .DESCRIPTION
        Safely removes a task with validation and cleanup.
    
    .PARAMETER TaskId
        The ID of the scheduled task to delete.
    
    .PARAMETER TaskName
        The name of the scheduled task to delete.
    
    .PARAMETER Force
        Force deletion without confirmation.
    
    .EXAMPLE
        Remove-ScheduledTask -TaskName "Daily Backup"
    
    .EXAMPLE
        Remove-ScheduledTask -TaskId "guid-here" -Force
    #>
    [CmdletBinding(SupportsShouldProcess)]
    param(
        [Parameter(ParameterSetName = 'ById', Mandatory)]
        [string]$TaskId,
        
        [Parameter(ParameterSetName = 'ByName', Mandatory)]
        [string]$TaskName,
        
        [switch]$Force
    )
    
    begin {
        if (-not (Test-SecurityAccess -Operation "Remove-ScheduledTask")) {
            throw "Access denied: Insufficient permissions to delete scheduled tasks"
        }
    }
    
    process {
        try {
            # Find task
            if ($TaskId) {
                $task = $script:TaskStorage.Tasks.Values | Where-Object { $_.Id -eq $TaskId } | Select-Object -First 1
                if (-not $task) {
                    throw "Scheduled task with ID '$TaskId' not found"
                }
                $taskName = $task.Name
            }
            else {
                if (-not $script:TaskStorage.Tasks.ContainsKey($TaskName)) {
                    throw "Scheduled task '$TaskName' not found"
                }
                $task = $script:TaskStorage.Tasks[$TaskName]
            }
            
            if ($PSCmdlet.ShouldProcess($taskName, "Delete scheduled task")) {
                # Remove from storage
                $script:TaskStorage.Tasks.Remove($taskName)
                $script:TaskStorage.LastUpdate = Get-Date
                
                # Persist changes
                Save-TaskStorage
                
                # Remove from RawrXD task registry if available
                if (Get-Variable -Name 'script:TaskRegistry' -ErrorAction SilentlyContinue) {
                    $script:TaskRegistry.ScheduledTasks.Remove($task.Id)
                }
                
                Write-SchedulingLog "Scheduled task deleted: $taskName" "SUCCESS" @{
                    TaskId = $task.Id
                }
                
                return $true
            }
            
            return $false
        }
        catch {
            Write-SchedulingLog "Failed to delete scheduled task: $($_.Exception.Message)" "ERROR"
            throw
        }
    }
}

function Schedule-Reminder {
    <#
    .SYNOPSIS
        Schedules a reminder notification for a specified date and time.
    
    .DESCRIPTION
        Creates a reminder that will trigger a notification at the specified time.
        Integrates with RawrXD's notification system.
    
    .PARAMETER Message
        The reminder message to display.
    
    .PARAMETER DateTime
        The date and time for the reminder.
    
    .PARAMETER Title
        Optional title for the reminder.
    
    .PARAMETER Priority
        Reminder priority: 'Low', 'Normal', 'High', 'Critical'.
    
    .EXAMPLE
        Schedule-Reminder -Message "Team meeting in 15 minutes" -DateTime (Get-Date).AddMinutes(15)
    
    .EXAMPLE
        Schedule-Reminder -Message "Project deadline" -DateTime (Get-Date).AddDays(7) -Title "Important" -Priority "High"
    #>
    [CmdletBinding()]
    param(
        [Parameter(Mandatory)]
        [ValidateNotNullOrEmpty()]
        [string]$Message,
        
        [Parameter(Mandatory)]
        [ValidateScript({
            if ($_ -lt (Get-Date)) {
                throw "DateTime must be in the future"
            }
            return $true
        })]
        [DateTime]$DateTime,
        
        [Parameter(Mandatory = $false)]
        [string]$Title = "Reminder",
        
        [Parameter(Mandatory = $false)]
        [ValidateSet('Low', 'Normal', 'High', 'Critical')]
        [string]$Priority = 'Normal'
    )
    
    process {
        try {
            $reminderId = [guid]::NewGuid().ToString()
            
            $reminder = @{
                Id = $reminderId
                Message = $Message
                Title = $Title
                DateTime = $DateTime
                Priority = $Priority
                CreatedAt = Get-Date
                Triggered = $false
                Metadata = @{
                    CreatedBy = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
                }
            }
            
            $script:TaskStorage.Reminders[$reminderId] = $reminder
            $script:TaskStorage.LastUpdate = Get-Date
            
            # Persist
            Save-TaskStorage
            
            Write-SchedulingLog "Reminder scheduled: $Title" "SUCCESS" @{
                ReminderId = $reminderId
                DateTime = $DateTime.ToString('o')
                Priority = $Priority
            }
            
            # Schedule notification job if available
            if (Get-Command Show-DesktopNotification -ErrorAction SilentlyContinue) {
                $delay = ($DateTime - (Get-Date)).TotalSeconds
                if ($delay -gt 0) {
                    Start-Job -ScriptBlock {
                        param($ReminderId, $Title, $Message, $Priority, $Delay)
                        Start-Sleep -Seconds $Delay
                        Show-DesktopNotification -Title $Title -Message $Message -Priority $Priority
                    } -ArgumentList $reminderId, $Title, $Message, $Priority, $delay | Out-Null
                }
            }
            
            return $reminder
        }
        catch {
            Write-SchedulingLog "Failed to schedule reminder: $($_.Exception.Message)" "ERROR"
            throw
        }
    }
}

function Get-SchedulingMetrics {
    <#
    .SYNOPSIS
        Retrieves performance and operational metrics for the scheduling module.
    
    .DESCRIPTION
        Returns comprehensive metrics for monitoring and analysis.
    
    .EXAMPLE
        $metrics = Get-SchedulingMetrics
    #>
    [CmdletBinding()]
    param()
    
    process {
        return @{
            ModuleVersion = $script:ModuleVersion
            TotalTasks = $script:TaskStorage.Tasks.Count
            TotalReminders = $script:TaskStorage.Reminders.Count
            TasksCreated = $script:PerformanceMetrics.TasksCreated
            TasksExecuted = $script:PerformanceMetrics.TasksExecuted
            TasksFailed = $script:PerformanceMetrics.TasksFailed
            AverageExecutionTime = $script:PerformanceMetrics.AverageExecutionTime
            LastExecutionTime = $script:PerformanceMetrics.LastExecutionTime
            LastUpdate = $script:TaskStorage.LastUpdate
            Configuration = $script:SchedulingConfig
        }
    }
}

#endregion

#region Module Initialization

# Load persisted tasks on module import
Load-TaskStorage

# Log module initialization
Write-SchedulingLog "RawrXD.Scheduling module initialized" "INFO" @{
    Version = $script:ModuleVersion
    TasksLoaded = $script:TaskStorage.Tasks.Count
    RemindersLoaded = $script:TaskStorage.Reminders.Count
}

#endregion

#region Module Export

$ExportedFunctions = @(
    'New-ScheduledTask',
    'Get-ScheduledTask',
    'Update-ScheduledTask',
    'Remove-ScheduledTask',
    'Schedule-Reminder',
    'Get-SchedulingMetrics'
)

Export-ModuleMember -Function $ExportedFunctions

#endregion

