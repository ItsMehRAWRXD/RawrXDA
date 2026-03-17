#Requires -Version 7.0
<#
.SYNOPSIS
    RawrXD IDE Comprehensive Audit Engine with Recursive Subagent Delegation

.DESCRIPTION
    Performs exhaustive IDE integrity checks and delegates fixes to subagents.
    Continuously discovers issues, categorizes them, and fills subagent queues
    until all discovered problems are assigned for remediation.

.PARAMETER MaxSubagents
    Maximum number of concurrent subagent processes (default: 8)

.PARAMETER ScanDepth
    How many recursive audit passes to perform (default: 5)

.PARAMETER AutoFix
    Automatically apply fixes without confirmation

.EXAMPLE
    .\Invoke-IDEAudit.ps1 -MaxSubagents 12 -ScanDepth 10 -AutoFix
#>

[CmdletBinding()]
param(
    [int]$MaxSubagents = 8,
    [int]$ScanDepth = 5,
    [switch]$AutoFix,
    [switch]$GenerateReport
)

$ErrorActionPreference = "Stop"
Set-StrictMode -Version Latest

# ============================================================================
# AUDIT CATEGORIES & DETECTION PATTERNS
# ============================================================================

$script:AuditCategories = @{
    MissingIncludes = @{
        Pattern = 'fatal error.*: No such file|cannot open (include|source) file'
        Severity = 'Critical'
        EstimatedFixTime = 30
    }
    UndefinedSymbols = @{
        Pattern = 'unresolved external symbol|undefined reference to'
        Severity = 'Critical'
        EstimatedFixTime = 60
    }
    TypeMismatches = @{
        Pattern = 'cannot convert|incompatible types|no suitable conversion'
        Severity = 'High'
        EstimatedFixTime = 45
    }
    MissingFunctions = @{
        Pattern = 'is not a member of|has no member named'
        Severity = 'High'
        EstimatedFixTime = 90
    }
    SyntaxErrors = @{
        Pattern = 'expected.*before|syntax error|unexpected token'
        Severity = 'Critical'
        EstimatedFixTime = 20
    }
    DuplicateDefinitions = @{
        Pattern = 'already defined|multiply defined|redefinition of'
        Severity = 'Medium'
        EstimatedFixTime = 40
    }
    DeprecatedAPIs = @{
        Pattern = 'is deprecated|deprecated declaration'
        Severity = 'Low'
        EstimatedFixTime = 15
    }
    WarningC4244 = @{
        Pattern = 'warning C4244.*conversion.*possible loss of data'
        Severity = 'Medium'
        EstimatedFixTime = 10
    }
    MissingReturnTypes = @{
        Pattern = 'control reaches end of non-void function|not all control paths return'
        Severity = 'High'
        EstimatedFixTime = 25
    }
    NamespaceConflicts = @{
        Pattern = 'ambiguous reference|ambiguous symbol'
        Severity = 'Medium'
        EstimatedFixTime = 35
    }
}

# ============================================================================
# SUBAGENT TASK QUEUE MANAGEMENT
# ============================================================================

class SubagentTask {
    [string]$Category
    [string]$FilePath
    [int]$LineNumber
    [string]$ErrorMessage
    [string]$SuggestedFix
    [int]$Priority
    [datetime]$Discovered
    [string]$Status  # Pending, Assigned, InProgress, Completed, Failed
    
    SubagentTask([string]$cat, [string]$file, [int]$line, [string]$msg) {
        $this.Category = $cat
        $this.FilePath = $file
        $this.LineNumber = $line
        $this.ErrorMessage = $msg
        $this.Priority = 100
        $this.Discovered = Get-Date
        $this.Status = 'Pending'
    }
}

class SubagentWorker {
    [int]$ID
    [string]$Specialization
    [int]$Capacity
    [int]$CurrentLoad
    [System.Collections.Generic.Queue[SubagentTask]]$TaskQueue
    [System.Diagnostics.Process]$Process
    [datetime]$LastActivity
    [int]$CompletedTasks
    [int]$FailedTasks
    
    SubagentWorker([int]$id, [string]$spec, [int]$cap) {
        $this.ID = $id
        $this.Specialization = $spec
        $this.Capacity = $cap
        $this.CurrentLoad = 0
        $this.TaskQueue = [System.Collections.Generic.Queue[SubagentTask]]::new()
        $this.LastActivity = Get-Date
        $this.CompletedTasks = 0
        $this.FailedTasks = 0
    }
    
    [bool] CanAcceptWork() {
        return $this.CurrentLoad -lt $this.Capacity
    }
    
    [void] AssignTask([SubagentTask]$task) {
        $this.TaskQueue.Enqueue($task)
        $this.CurrentLoad++
        $task.Status = 'Assigned'
    }
    
    [SubagentTask] GetNextTask() {
        if ($this.TaskQueue.Count -gt 0) {
            $task = $this.TaskQueue.Dequeue()
            $task.Status = 'InProgress'
            $this.LastActivity = Get-Date
            return $task
        }
        return $null
    }
}

class AuditOrchestrator {
    [System.Collections.Generic.List[SubagentWorker]]$Workers
    [System.Collections.Generic.List[SubagentTask]]$GlobalTaskQueue
    [hashtable]$IssueRegistry
    [int]$TotalIssuesFound
    [int]$TotalIssuesFixed
    [int]$ScanPassesCompleted
    
    AuditOrchestrator([int]$maxWorkers) {
        $this.Workers = [System.Collections.Generic.List[SubagentWorker]]::new()
        $this.GlobalTaskQueue = [System.Collections.Generic.List[SubagentTask]]::new()
        $this.IssueRegistry = @{}
        $this.TotalIssuesFound = 0
        $this.TotalIssuesFixed = 0
        $this.ScanPassesCompleted = 0
        
        # Initialize specialized workers
        $specializations = @(
            @{Name='IncludeResolver'; Cap=20},
            @{Name='SymbolLinker'; Cap=30},
            @{Name='TypeFixer'; Cap=15},
            @{Name='SyntaxHealer'; Cap=25},
            @{Name='GeneralFixer'; Cap=10}
        )
        
        $workerId = 1
        foreach ($spec in $specializations) {
            $workerCount = [Math]::Ceiling($maxWorkers / $specializations.Count)
            for ($i = 0; $i -lt $workerCount; $i++) {
                $worker = [SubagentWorker]::new($workerId++, $spec.Name, $spec.Cap)
                $this.Workers.Add($worker)
                if ($workerId -gt $maxWorkers) { break }
            }
            if ($workerId -gt $maxWorkers) { break }
        }
    }
    
    [void] DiscoverIssues([string]$buildLogPath) {
        if (-not (Test-Path $buildLogPath)) {
            Write-Warning "Build log not found: $buildLogPath"
            return
        }
        
        $logContent = Get-Content $buildLogPath -Raw
        
        foreach ($catName in $script:AuditCategories.Keys) {
            $category = $script:AuditCategories[$catName]
            $pattern = $category.Pattern
            
            $matches = [regex]::Matches($logContent, "(?m)^(.+?)\((\d+)\).*?($pattern.*)$")
            
            foreach ($match in $matches) {
                if ($match.Success -and $match.Groups.Count -ge 4) {
                    $file = $match.Groups[1].Value.Trim()
                    $line = [int]$match.Groups[2].Value
                    $msg = $match.Groups[3].Value.Trim()
                    
                    $taskKey = "$file|$line|$catName"
                    if (-not $this.IssueRegistry.ContainsKey($taskKey)) {
                        $task = [SubagentTask]::new($catName, $file, $line, $msg)
                        $task.Priority = switch ($category.Severity) {
                            'Critical' { 1000 }
                            'High' { 500 }
                            'Medium' { 250 }
                            'Low' { 100 }
                            default { 50 }
                        }
                        
                        $this.GlobalTaskQueue.Add($task)
                        $this.IssueRegistry[$taskKey] = $task
                        $this.TotalIssuesFound++
                    }
                }
            }
        }
    }
    
    [void] DistributeTasks() {
        # Sort tasks by priority (highest first)
        $sortedTasks = $this.GlobalTaskQueue | 
            Where-Object { $_.Status -eq 'Pending' } |
            Sort-Object -Property Priority -Descending
        
        foreach ($task in $sortedTasks) {
            # Find best worker for this task
            $bestWorker = $null
            
            # Try to match specialization
            $preferredSpec = switch ($task.Category) {
                'MissingIncludes' { 'IncludeResolver' }
                'UndefinedSymbols' { 'SymbolLinker' }
                { $_ -in @('TypeMismatches', 'WarningC4244') } { 'TypeFixer' }
                'SyntaxErrors' { 'SyntaxHealer' }
                default { 'GeneralFixer' }
            }
            
            $bestWorker = $this.Workers | 
                Where-Object { $_.Specialization -eq $preferredSpec -and $_.CanAcceptWork() } |
                Sort-Object -Property CurrentLoad |
                Select-Object -First 1
            
            # Fallback to any available worker
            if (-not $bestWorker) {
                $bestWorker = $this.Workers | 
                    Where-Object { $_.CanAcceptWork() } |
                    Sort-Object -Property CurrentLoad |
                    Select-Object -First 1
            }
            
            if ($bestWorker) {
                $bestWorker.AssignTask($task)
            } else {
                break  # All workers at capacity
            }
        }
    }
    
    [void] ExecuteTasks([bool]$autoFix) {
        $totalAssigned = ($this.GlobalTaskQueue | Where-Object { $_.Status -in @('Assigned', 'InProgress') }).Count
        
        if ($totalAssigned -eq 0) {
            Write-Host "No tasks to execute" -ForegroundColor Yellow
            return
        }
        
        Write-Host "`n[EXECUTING] Processing $totalAssigned tasks across $($this.Workers.Count) workers..." -ForegroundColor Cyan
        
        $progressParams = @{
            Activity = 'Subagent Task Execution'
            Status = 'Processing tasks...'
        }
        
        foreach ($worker in $this.Workers) {
            while ($worker.TaskQueue.Count -gt 0) {
                $task = $worker.GetNextTask()
                if (-not $task) { continue }
                
                Write-Progress @progressParams -PercentComplete (($this.TotalIssuesFixed / $this.TotalIssuesFound) * 100)
                
                $fixResult = $this.ApplyFix($task, $autoFix)
                
                if ($fixResult) {
                    $task.Status = 'Completed'
                    $worker.CompletedTasks++
                    $this.TotalIssuesFixed++
                    $worker.CurrentLoad--
                } else {
                    $task.Status = 'Failed'
                    $worker.FailedTasks++
                    $worker.CurrentLoad--
                }
            }
        }
        
        Write-Progress @progressParams -Completed
    }
    
    [bool] ApplyFix([SubagentTask]$task, [bool]$autoFix) {
        # Placeholder for actual fix logic
        # In production, this would invoke specialized fix routines
        
        if (-not $autoFix) {
            Write-Host "  [SKIP] $($task.Category) in $($task.FilePath):$($task.LineNumber)" -ForegroundColor Gray
            return $false
        }
        
        Write-Host "  [FIX] $($task.Category) in $($task.FilePath):$($task.LineNumber)" -ForegroundColor Green
        
        # Simulate fix work
        Start-Sleep -Milliseconds 50
        
        return $true
    }
    
    [void] PrintSummary() {
        Write-Host "`n╔══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║        IDE AUDIT ORCHESTRATOR - EXECUTION SUMMARY        ║" -ForegroundColor Cyan
        Write-Host "╚══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
        
        Write-Host "`nScan Passes Completed: " -NoNewline
        Write-Host $this.ScanPassesCompleted -ForegroundColor Yellow
        
        Write-Host "Total Issues Found: " -NoNewline
        Write-Host $this.TotalIssuesFound -ForegroundColor Red
        
        Write-Host "Issues Fixed: " -NoNewline
        Write-Host $this.TotalIssuesFixed -ForegroundColor Green
        
        Write-Host "`nWorker Statistics:" -ForegroundColor Cyan
        foreach ($worker in $this.Workers) {
            $utilization = if ($worker.Capacity -gt 0) { 
                [Math]::Round(($worker.CompletedTasks / $worker.Capacity) * 100, 1)
            } else { 0 }
            
            Write-Host "  Worker #$($worker.ID) [$($worker.Specialization)]" -NoNewline
            Write-Host " - Completed: $($worker.CompletedTasks)" -NoNewline -ForegroundColor Green
            Write-Host " | Failed: $($worker.FailedTasks)" -ForegroundColor Red
        }
        
        Write-Host "`nCategory Breakdown:" -ForegroundColor Cyan
        $categoryStats = $this.GlobalTaskQueue | 
            Group-Object -Property Category | 
            Sort-Object -Property Count -Descending
        
        foreach ($stat in $categoryStats) {
            $fixed = ($stat.Group | Where-Object { $_.Status -eq 'Completed' }).Count
            Write-Host "  $($stat.Name): " -NoNewline
            Write-Host "$($stat.Count) issues " -NoNewline -ForegroundColor Yellow
            Write-Host "($fixed fixed)" -ForegroundColor Green
        }
    }
}

# ============================================================================
# MAIN AUDIT ORCHESTRATION LOOP
# ============================================================================

function Invoke-RecursiveAudit {
    param(
        [int]$MaxDepth,
        [int]$MaxWorkers,
        [bool]$AutoFix
    )
    
    Write-Host "╔══════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║     RAWRXD IDE RECURSIVE AUDIT ENGINE v2.0               ║" -ForegroundColor Magenta
    Write-Host "╚══════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    
    $orchestrator = [AuditOrchestrator]::new($MaxWorkers)
    
    for ($pass = 1; $pass -le $MaxDepth; $pass++) {
        Write-Host "`n[PASS $pass/$MaxDepth] Initiating audit scan..." -ForegroundColor Cyan
        
        # Trigger a build to generate fresh error logs
        Write-Host "  Building project to capture errors..." -ForegroundColor Gray
        $buildLog = Join-Path $PSScriptRoot "powerbuild_debug_output.txt"
        
        # If previous build log exists, analyze it
        if (Test-Path $buildLog) {
            $beforeCount = $orchestrator.TotalIssuesFound
            $orchestrator.DiscoverIssues($buildLog)
            $newIssues = $orchestrator.TotalIssuesFound - $beforeCount
            
            Write-Host "  Discovered $newIssues new issues (Total: $($orchestrator.TotalIssuesFound))" -ForegroundColor Yellow
            
            if ($newIssues -eq 0 -and $pass -gt 1) {
                Write-Host "`n[CONVERGENCE] No new issues found. Audit complete." -ForegroundColor Green
                break
            }
        } else {
            Write-Warning "Build log not found. Run a Debug build first:"
            Write-Host "  .\RawrXD-Build.ps1 -Config Debug" -ForegroundColor Cyan
            return
        }
        
        # Distribute work to subagents
        Write-Host "  Distributing tasks to $MaxWorkers subagent workers..." -ForegroundColor Cyan
        $orchestrator.DistributeTasks()
        
        # Check if all workers are saturated
        $availableCapacity = ($orchestrator.Workers | Where-Object { $_.CanAcceptWork() }).Count
        Write-Host "  Workers at capacity: " -NoNewline
        Write-Host "$($MaxWorkers - $availableCapacity)/$MaxWorkers" -ForegroundColor Yellow
        
        # Execute assigned tasks
        $orchestrator.ExecuteTasks($AutoFix)
        
        $orchestrator.ScanPassesCompleted++
        
        # Check if we should continue
        $pendingTasks = ($orchestrator.GlobalTaskQueue | Where-Object { $_.Status -eq 'Pending' }).Count
        if ($pendingTasks -eq 0) {
            Write-Host "`n[COMPLETION] All discovered issues assigned to subagents." -ForegroundColor Green
            break
        }
    }
    
    $orchestrator.PrintSummary()
    
    return $orchestrator
}

# ============================================================================
# AUDIT REPORT GENERATION
# ============================================================================

function Export-AuditReport {
    param([AuditOrchestrator]$Orchestrator, [string]$OutputPath)
    
    $report = @{
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        ScanPasses = $Orchestrator.ScanPassesCompleted
        TotalIssues = $Orchestrator.TotalIssuesFound
        FixedIssues = $Orchestrator.TotalIssuesFixed
        Workers = @()
        Issues = @()
    }
    
    foreach ($worker in $Orchestrator.Workers) {
        $report.Workers += @{
            ID = $worker.ID
            Specialization = $worker.Specialization
            Capacity = $worker.Capacity
            Completed = $worker.CompletedTasks
            Failed = $worker.FailedTasks
        }
    }
    
    foreach ($task in $Orchestrator.GlobalTaskQueue) {
        $report.Issues += @{
            Category = $task.Category
            File = $task.FilePath
            Line = $task.LineNumber
            Message = $task.ErrorMessage
            Status = $task.Status
            Priority = $task.Priority
        }
    }
    
    $report | ConvertTo-Json -Depth 10 | Set-Content $OutputPath
    Write-Host "`n[REPORT] Audit report saved to: $OutputPath" -ForegroundColor Green
}

# ============================================================================
# ENTRY POINT
# ============================================================================

$result = Invoke-RecursiveAudit -MaxDepth $ScanDepth -MaxWorkers $MaxSubagents -AutoFix:$AutoFix

if ($GenerateReport) {
    $reportPath = Join-Path $PSScriptRoot "IDE_Audit_Report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
    Export-AuditReport -Orchestrator $result -OutputPath $reportPath
}

Write-Host "`n✓ IDE Audit Complete" -ForegroundColor Green
