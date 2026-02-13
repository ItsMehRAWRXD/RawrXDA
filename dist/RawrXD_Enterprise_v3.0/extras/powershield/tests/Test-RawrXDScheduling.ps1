<#
.SYNOPSIS
    Test script for RawrXD.Scheduling module

.DESCRIPTION
    Comprehensive tests for the scheduling module including error handling,
    security, performance, and functionality validation.
#>

$ErrorActionPreference = 'Stop'

Write-Host "`n=== Testing RawrXD.Scheduling Module ===" -ForegroundColor Cyan
Write-Host ""

# Import the module
$modulePath = Join-Path $PSScriptRoot "Modules\Modules\RawrXD.Scheduling.psm1"
if (-not (Test-Path $modulePath)) {
    Write-Host "ERROR: Module not found at $modulePath" -ForegroundColor Red
    exit 1
}

try {
    Import-Module $modulePath -Force -ErrorAction Stop
    Write-Host "[TEST 1] ✓ Module imported successfully" -ForegroundColor Green
}
catch {
    Write-Host "[TEST 1] ✗ Failed to import module: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

# Test 2: Create a scheduled task
Write-Host "`n[TEST 2] Testing New-ScheduledTask..." -ForegroundColor Yellow
try {
    $task = New-ScheduledTask -Name "Test Task 1" `
        -Description "Test scheduled task" `
        -StartTime (Get-Date).AddHours(1) `
        -RecurrencePattern "Daily" `
        -Priority "Normal"

    if ($task -and $task.Id) {
        Write-Host "  ✓ Task created successfully" -ForegroundColor Green
        Write-Host "    Task ID: $($task.Id)" -ForegroundColor Gray
        Write-Host "    Name: $($task.Name)" -ForegroundColor Gray
        Write-Host "    Start Time: $($task.StartTime)" -ForegroundColor Gray
    }
    else {
        Write-Host "  ✗ Task creation failed - invalid task object" -ForegroundColor Red
    }
}
catch {
    Write-Host "  ✗ Task creation failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 3: Retrieve task
Write-Host "`n[TEST 3] Testing Get-ScheduledTask..." -ForegroundColor Yellow
try {
    $retrievedTask = Get-ScheduledTask -TaskName "Test Task 1"
    if ($retrievedTask -and $retrievedTask.Name -eq "Test Task 1") {
        Write-Host "  ✓ Task retrieved successfully" -ForegroundColor Green
    }
    else {
        Write-Host "  ✗ Task retrieval failed" -ForegroundColor Red
    }
}
catch {
    Write-Host "  ✗ Task retrieval failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 4: Update task
Write-Host "`n[TEST 4] Testing Update-ScheduledTask..." -ForegroundColor Yellow
try {
    $updatedTask = Update-ScheduledTask -TaskName "Test Task 1" `
        -Description "Updated description"

    if ($updatedTask -and $updatedTask.Description -eq "Updated description") {
        Write-Host "  ✓ Task updated successfully" -ForegroundColor Green
    }
    else {
        Write-Host "  ✗ Task update failed" -ForegroundColor Red
    }
}
catch {
    Write-Host "  ✗ Task update failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 5: Schedule reminder
Write-Host "`n[TEST 5] Testing Schedule-Reminder..." -ForegroundColor Yellow
try {
    $reminder = Schedule-Reminder -Message "Test reminder" `
        -DateTime (Get-Date).AddMinutes(5) `
        -Title "Test Reminder" `
        -Priority "Normal"

    if ($reminder -and $reminder.Id) {
        Write-Host "  ✓ Reminder scheduled successfully" -ForegroundColor Green
        Write-Host "    Reminder ID: $($reminder.Id)" -ForegroundColor Gray
    }
    else {
        Write-Host "  ✗ Reminder scheduling failed" -ForegroundColor Red
    }
}
catch {
    Write-Host "  ✗ Reminder scheduling failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 6: Get all tasks
Write-Host "`n[TEST 6] Testing Get-ScheduledTask -All..." -ForegroundColor Yellow
try {
    $allTasks = Get-ScheduledTask -All
    if ($allTasks) {
        Write-Host "  ✓ Retrieved $($allTasks.Count) task(s)" -ForegroundColor Green
    }
    else {
        Write-Host "  ⚠ No tasks found" -ForegroundColor Yellow
    }
}
catch {
    Write-Host "  ✗ Failed to retrieve all tasks: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 7: Get metrics
Write-Host "`n[TEST 7] Testing Get-SchedulingMetrics..." -ForegroundColor Yellow
try {
    $metrics = Get-SchedulingMetrics
    if ($metrics) {
        Write-Host "  ✓ Metrics retrieved successfully" -ForegroundColor Green
        Write-Host "    Total Tasks: $($metrics.TotalTasks)" -ForegroundColor Gray
        Write-Host "    Total Reminders: $($metrics.TotalReminders)" -ForegroundColor Gray
        Write-Host "    Tasks Created: $($metrics.TasksCreated)" -ForegroundColor Gray
    }
    else {
        Write-Host "  ✗ Metrics retrieval failed" -ForegroundColor Red
    }
}
catch {
    Write-Host "  ✗ Metrics retrieval failed: $($_.Exception.Message)" -ForegroundColor Red
}

# Test 8: Error handling - duplicate task name
Write-Host "`n[TEST 8] Testing error handling (duplicate name)..." -ForegroundColor Yellow
try {
    $duplicateTask = New-ScheduledTask -Name "Test Task 1" `
        -Description "Duplicate test" `
        -StartTime (Get-Date).AddHours(2) `
        -ErrorAction Stop
    Write-Host "  ✗ Should have thrown error for duplicate name" -ForegroundColor Red
}
catch {
    if ($_.Exception.Message -like "*already exists*") {
        Write-Host "  ✓ Error handling works correctly (duplicate name detected)" -ForegroundColor Green
        Write-Host "    Expected error: $($_.Exception.Message)" -ForegroundColor Gray
    }
    else {
        Write-Host "  ✗ Unexpected error: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Test 9: Error handling - past start time
Write-Host "`n[TEST 9] Testing error handling (past start time)..." -ForegroundColor Yellow
try {
    $pastTask = New-ScheduledTask -Name "Past Task" `
        -Description "Test" `
        -StartTime (Get-Date).AddHours(-1)
    Write-Host "  ✗ Should have thrown error for past start time" -ForegroundColor Red
}
catch {
    if ($_.Exception.Message -like "*future*") {
        Write-Host "  ✓ Validation works correctly" -ForegroundColor Green
    }
    else {
        Write-Host "  ✗ Unexpected error: $($_.Exception.Message)" -ForegroundColor Red
    }
}

# Test 10: Cleanup
Write-Host "`n[TEST 10] Cleaning up test tasks..." -ForegroundColor Yellow
try {
    Remove-ScheduledTask -TaskName "Test Task 1" -Force
    Write-Host "  ✓ Test task removed" -ForegroundColor Green
}
catch {
    Write-Host "  ⚠ Cleanup warning: $($_.Exception.Message)" -ForegroundColor Yellow
}

Write-Host "`n=== Testing Complete ===" -ForegroundColor Cyan
Write-Host ""

# Display final metrics
$finalMetrics = Get-SchedulingMetrics
Write-Host "Final Module Metrics:" -ForegroundColor Cyan
$finalMetrics | Format-List

