# RawrXD.Scheduling Module - Production-Ready Task Automation

## Overview

The `RawrXD.Scheduling` module provides enterprise-grade task scheduling, reminders, and automation capabilities with robust error handling, logging, security, monitoring, and performance optimization.

## Features

✅ **Production-Ready**
- Comprehensive error handling and validation
- Security and access control
- Performance monitoring and metrics
- Task persistence to disk
- Audit logging

✅ **Enterprise Features**
- Recurrence patterns (Once, Daily, Weekly, Monthly, Yearly)
- Task prioritization
- Integration with RawrXD task registry
- Reminder notifications
- Performance metrics tracking

✅ **Reliability**
- Automatic persistence
- Task validation
- Error recovery
- Resource management

## Installation

The module is automatically loaded when you run `Modules\main.ps1`. To use it standalone:

```powershell
Import-Module .\Modules\Modules\RawrXD.Scheduling.psm1 -Force
```

## Functions

### New-ScheduledTask

Creates a new scheduled task with validation and persistence.

```powershell
$task = New-ScheduledTask -Name "Daily Backup" `
    -Description "Daily system backup" `
    -StartTime (Get-Date).AddDays(1) `
    -RecurrencePattern "Daily" `
    -Priority "High"
```

**Parameters:**
- `Name` (Required): Unique task name
- `Description`: Task description
- `StartTime` (Required): Future date/time
- `RecurrencePattern`: Once, Daily, Weekly, Monthly, Yearly
- `RecurrenceInterval`: Interval for recurrence
- `EndTime`: Optional end time for recurring tasks
- `ScriptBlock`: Script to execute
- `Priority`: Low, Normal, High, Critical
- `Enabled`: Enable/disable task

### Get-ScheduledTask

Retrieves task information.

```powershell
# Get by name
$task = Get-ScheduledTask -TaskName "Daily Backup"

# Get by ID
$task = Get-ScheduledTask -TaskId "guid-here"

# Get all tasks
$allTasks = Get-ScheduledTask -All

# Filter by status
$activeTasks = Get-ScheduledTask -All -Status "Scheduled"
```

### Update-ScheduledTask

Updates existing task properties.

```powershell
Update-ScheduledTask -TaskName "Daily Backup" `
    -Description "Updated description" `
    -Enabled $false
```

### Remove-ScheduledTask

Deletes a scheduled task.

```powershell
Remove-ScheduledTask -TaskName "Daily Backup" -Force
```

### Schedule-Reminder

Creates a reminder notification.

```powershell
$reminder = Schedule-Reminder -Message "Team meeting in 15 minutes" `
    -DateTime (Get-Date).AddMinutes(15) `
    -Title "Important" `
    -Priority "High"
```

### Get-SchedulingMetrics

Retrieves performance and operational metrics.

```powershell
$metrics = Get-SchedulingMetrics
$metrics | Format-List
```

## Configuration

Module configuration is stored in `$script:SchedulingConfig`:

```powershell
$SchedulingConfig = @{
    StoragePath = "config\scheduled-tasks.json"
    MaxConcurrentTasks = 10
    TaskTimeoutMinutes = 30
    EnablePersistence = $true
    EnableAuditing = $true
    EnableMonitoring = $true
    RetryAttempts = 3
    RetryBackoffSeconds = 5
}
```

## Security

The module includes security features:

- **Access Control**: Configurable user permissions
- **Audit Logging**: All operations logged to audit log
- **Validation**: Input validation and sanitization
- **Authentication**: Integration with Windows authentication

Configure security in `$script:SecurityContext`:

```powershell
$SecurityContext = @{
    RequireAuthentication = $true
    AllowedUsers = @("DOMAIN\User1", "DOMAIN\User2")
    AuditLogPath = "logs\scheduling-audit.log"
}
```

## Persistence

Tasks are automatically persisted to:
- **Location**: `config\scheduled-tasks.json`
- **Format**: JSON
- **Auto-save**: On create, update, delete
- **Auto-load**: On module import

## Integration with RawrXD

The module integrates with RawrXD's existing task management:

- Uses `$script:TaskRegistry` if available
- Integrates with RawrXD logging (`Write-StartupLog`)
- Uses RawrXD notifications (`Show-DesktopNotification`)
- Follows RawrXD module patterns

## Performance Monitoring

Track performance with metrics:

```powershell
$metrics = Get-SchedulingMetrics
Write-Host "Tasks Created: $($metrics.TasksCreated)"
Write-Host "Tasks Executed: $($metrics.TasksExecuted)"
Write-Host "Average Execution Time: $($metrics.AverageExecutionTime) ms"
```

## Error Handling

All functions include comprehensive error handling:

- Input validation
- Try-catch blocks
- Detailed error logging
- Graceful failure handling

## Testing

Run the test suite:

```powershell
.\Test-RawrXDScheduling.ps1
```

Tests include:
- Module import
- Task creation
- Task retrieval
- Task updates
- Reminder scheduling
- Error handling
- Metrics retrieval

## Examples

### Daily Backup Task

```powershell
$backupTask = New-ScheduledTask -Name "Daily Backup" `
    -Description "Backup system files" `
    -StartTime (Get-Date).AddDays(1).Date.AddHours(2) `
    -RecurrencePattern "Daily" `
    -ScriptBlock {
        Write-Host "Running backup..."
        # Backup logic here
    } `
    -Priority "High"
```

### Weekly Report

```powershell
$reportTask = New-ScheduledTask -Name "Weekly Report" `
    -Description "Generate weekly report" `
    -StartTime (Get-Date).AddDays(7).Date.AddHours(9) `
    -RecurrencePattern "Weekly" `
    -RecurrenceInterval 1 `
    -EndTime (Get-Date).AddMonths(6)
```

### Reminder

```powershell
$reminder = Schedule-Reminder `
    -Message "Project deadline approaching" `
    -DateTime (Get-Date).AddDays(7) `
    -Title "Project Deadline" `
    -Priority "Critical"
```

## Best Practices

1. **Naming**: Use descriptive, unique task names
2. **Validation**: Always validate start times are in the future
3. **Error Handling**: Wrap calls in try-catch blocks
4. **Monitoring**: Regularly check metrics
5. **Security**: Configure access control appropriately
6. **Persistence**: Ensure config directory exists

## Troubleshooting

### Tasks Not Persisting

- Check `EnablePersistence` is `$true`
- Verify write permissions to config directory
- Check disk space

### Access Denied Errors

- Verify user is in `AllowedUsers` list
- Check `RequireAuthentication` setting
- Review audit log for details

### Performance Issues

- Check `MaxConcurrentTasks` setting
- Review metrics for bottlenecks
- Monitor execution times

## Module Structure

```
RawrXD.Scheduling.psm1
├── Module Metadata
├── Configuration
├── Storage (In-Memory Cache)
├── Performance Metrics
├── Security Context
├── Helper Functions
│   ├── Write-SchedulingLog
│   ├── Test-SecurityAccess
│   ├── Save-TaskStorage
│   ├── Load-TaskStorage
│   └── Update-PerformanceMetrics
├── Public Functions
│   ├── New-ScheduledTask
│   ├── Get-ScheduledTask
│   ├── Update-ScheduledTask
│   ├── Remove-ScheduledTask
│   ├── Schedule-Reminder
│   └── Get-SchedulingMetrics
└── Module Initialization
```

## Version History

- **1.0.0** (2025-11-25): Initial production release
  - Core scheduling functionality
  - Reminder system
  - Security and auditing
  - Performance monitoring
  - Persistence

## Support

For issues or questions:
1. Check the audit log: `logs\scheduling-audit.log`
2. Review metrics: `Get-SchedulingMetrics`
3. Check module logs in console output

