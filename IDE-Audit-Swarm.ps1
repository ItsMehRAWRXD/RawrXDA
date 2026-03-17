# IDE-Audit-Swarm.ps1 — Recursive IDE audit system that spawns subagents
# Scans codebase for issues, delegates fixes to subagents, repeats until capacity filled

param(
    [int]$MaxSubAgents = 8,
    [switch]$DryRun,
    [switch]$Verbose
)

$ErrorActionPreference = "Continue"

Write-Host @"
╔══════════════════════════════════════════════════════════════╗
║           RawrXD IDE AUDIT SWARM v1.0                        ║
║     Recursive Codebase Analysis + Subagent Orchestration     ║
╚══════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

# ==============================================================================
# STATE TRACKING
# ==============================================================================

$script:SubAgentCount = 0
$script:TaskQueue = [System.Collections.Generic.Queue[object]]::new()
$script:CompletedTasks = @()
$script:ActiveSubAgents = @()

class AuditTask {
    [string]$Category
    [string]$Description
    [string]$FilePath
    [int]$Priority
    [string]$SubAgentPrompt
    [datetime]$CreatedAt
    [string]$Status
}

# ==============================================================================
# AUDIT SCANNERS
# ==============================================================================

function Scan-CompileErrors {
    Write-Host "`n[AUDIT] Scanning for compile errors..." -ForegroundColor Yellow
    
    # Run a quick build check
    $buildOutput = & .\RawrXD-Build.ps1 -Config Debug 2>&1 | Out-String
    
    $errors = @()
    
    # Parse error patterns
    if ($buildOutput -match "error C\d+:") {
        $errorLines = $buildOutput -split "`n" | Where-Object { $_ -match "error C\d+:" }
        foreach ($line in $errorLines) {
            if ($line -match "(.+?)\((\d+)\).*error C(\d+): (.+)") {
                $errors += [PSCustomObject]@{
                    File = $matches[1]
                    Line = $matches[2]
                    Code = "C$($matches[3])"
                    Message = $matches[4]
                }
            }
        }
    }
    
    Write-Host "  Found $($errors.Count) compile errors" -ForegroundColor $(if ($errors.Count -gt 0) { "Red" } else { "Green" })
    
    # Group by file
    $groupedErrors = $errors | Group-Object -Property File
    foreach ($group in $groupedErrors) {
        $task = [AuditTask]@{
            Category = "CompileError"
            Description = "Fix $($group.Count) compile error(s) in $($group.Name)"
            FilePath = $group.Name
            Priority = 100
            SubAgentPrompt = @"
Fix the following compile errors in $($group.Name):

$($group.Group | ForEach-Object { "Line $($_.Line): $($_.Code) - $($_.Message)" } | Out-String)

Requirements:
1. Read the file context around each error
2. Fix the errors while maintaining code structure
3. Ensure no new errors are introduced
4. Test compilation after fix
"@
            CreatedAt = Get-Date
            Status = "Pending"
        }
        $script:TaskQueue.Enqueue($task)
    }
}

function Scan-LinkErrors {
    Write-Host "`n[AUDIT] Scanning for link errors..." -ForegroundColor Yellow
    
    # Look for unresolved externals in recent build output
    $linkLog = Get-Content "build_log.txt" -ErrorAction SilentlyContinue -Raw
    
    $unresolvedSymbols = @()
    
    if ($linkLog -match "unresolved external symbol") {
        $symbolLines = $linkLog -split "`n" | Where-Object { $_ -match "unresolved external symbol" }
        foreach ($line in $symbolLines) {
            if ($line -match 'unresolved external symbol\s+"?([^"]+)"?\s+referenced in (.+)') {
                $unresolvedSymbols += [PSCustomObject]@{
                    Symbol = $matches[1]
                    ReferencedIn = $matches[2]
                }
            }
        }
    }
    
    Write-Host "  Found $($unresolvedSymbols.Count) unresolved symbols" -ForegroundColor $(if ($unresolvedSymbols.Count -gt 0) { "Red" } else { "Green" })
    
    # Group by category
    $diskRecoverySymbols = $unresolvedSymbols | Where-Object { $_.Symbol -match "DiskRecovery" }
    $swarmSymbols = $unresolvedSymbols | Where-Object { $_.Symbol -match "Swarm" }
    $lspSymbols = $unresolvedSymbols | Where-Object { $_.Symbol -match "LSP_" }
    
    if ($diskRecoverySymbols) {
        $task = [AuditTask]@{
            Category = "LinkError"
            Description = "Resolve DiskRecovery link errors ($($diskRecoverySymbols.Count) symbols)"
            FilePath = "src/core/DiskRecoveryAgent.cpp"
            Priority = 90
            SubAgentPrompt = @"
Fix DiskRecovery subsystem link errors. The following symbols are unresolved:

$($diskRecoverySymbols | ForEach-Object { "- $($_.Symbol) (referenced in $($_.ReferencedIn))" } | Out-String)

Tasks:
1. Ensure DiskRecoveryAgent.cpp implementation exists in src/core/
2. Verify extern "C" declarations match ASM exports in RawrXD_DiskRecoveryAgent.asm
3. Add RAWR_HAS_MASM guard if ASM module is optional
4. Create minimal stubs for any ASM functions not yet implemented
5. Test linking

The ASM file is at src/asm/RawrXD_DiskRecoveryAgent.asm
The C++ wrapper is at src/core/DiskRecoveryAgent.h
"@
            CreatedAt = Get-Date
            Status = "Pending"
        }
        $script:TaskQueue.Enqueue($task)
    }
    
    if ($swarmSymbols) {
        $task = [AuditTask]@{
            Category = "LinkError"
            Description = "Resolve Swarm subsystem link errors ($($swarmSymbols.Count) symbols)"
            FilePath = "src/core/swarm_coordinator.cpp"
            Priority = 90
            SubAgentPrompt = @"
Fix Swarm subsystem link errors. The following symbols are unresolved:

$($swarmSymbols | ForEach-Object { "- $($_.Symbol) (referenced in $($_.ReferencedIn))" } | Out-String)

Tasks:
1. Ensure RawrXD_Swarm_Network.asm and RawrXD_Swarm_Orchestrator.asm are in src/asm/
2. Verify extern "C" declarations in swarm_coordinator.cpp match ASM exports
3. Check that swarm_network_stubs.cpp provides fallback implementations
4. Add conditional compilation guards for ASM-dependent code
5. Test linking

The ASM files are at:
- src/asm/RawrXD_Swarm_Network.asm
- src/asm/RawrXD_Swarm_Orchestrator.asm
The C++ files are at:
- src/core/swarm_coordinator.cpp
- src/core/swarm_worker.cpp
- src/core/swarm_reconciliation.cpp
"@
            CreatedAt = Get-Date
            Status = "Pending"
        }
        $script:TaskQueue.Enqueue($task)
    }
}

function Scan-MissingImplementations {
    Write-Host "`n[AUDIT] Scanning for stub implementations..." -ForegroundColor Yellow
    
    $stubFiles = Get-ChildItem -Path "src" -Recurse -Filter "*stub*.cpp"
    
    Write-Host "  Found $($stubFiles.Count) stub files" -ForegroundColor Yellow
    
    foreach ($file in $stubFiles) {
        # Count stub functions
        $content = Get-Content $file.FullName -Raw
        $stubCount = ($content -split "\{\s*\}").Count - 1
        
        if ($stubCount -gt 5) {
            $task = [AuditTask]@{
                Category = "MissingImplementation"
                Description = "Implement real logic for ~$stubCount stubs in $($file.Name)"
                FilePath = $file.FullName
                Priority = 50
                SubAgentPrompt = @"
Replace stub implementations with real logic in $($file.Name).

Current file has $stubCount empty stub functions. 

Tasks:
1. Read the header files to understand expected behavior
2. Find real implementations in the 'Full Source' directory if they exist
3. Implement or copy real logic for each stub
4. Test that callers work correctly
5. Ensure no exceptions are thrown (use PatchResult pattern)

File location: $($file.FullName)
"@
                CreatedAt = Get-Date
                Status = "Pending"
            }
            $script:TaskQueue.Enqueue($task)
        }
    }
}

function Scan-IncompleteFeatures {
    Write-Host "`n[AUDIT] Scanning for incomplete feature implementations..." -ForegroundColor Yellow
    
    # Check Win32IDE methods that might be stubbed
    $win32ideFile = "src\win32app\Win32IDE.cpp"
    if (Test-Path $win32ideFile) {
        $content = Get-Content $win32ideFile -Raw
        
        # Look for TODO comments
        $todos = $content -split "`n" | Where-Object { $_ -match "TODO|FIXME|HACK|XXX" }
        
        if ($todos.Count -gt 0) {
            $task = [AuditTask]@{
                Category = "IncompleteFeature"
                Description = "Address $($todos.Count) TODO/FIXME comments in Win32IDE.cpp"
                FilePath = $win32ideFile
                Priority = 40
                SubAgentPrompt = @"
Address TODO/FIXME comments in Win32IDE.cpp:

$($todos | Select-Object -First 10 | Out-String)

Tasks:
1. Review each TODO comment
2. Implement the missing functionality or remove obsolete TODOs
3. Test affected features
4. Update documentation if needed
"@
                CreatedAt = Get-Date
                Status = "Pending"
            }
            $script:TaskQueue.Enqueue($task)
        }
    }
}

function Scan-MemoryLeaks {
    Write-Host "`n[AUDIT] Scanning for potential memory leaks..." -ForegroundColor Yellow
    
    # Simple heuristic: new/malloc without corresponding delete/free
    $cppFiles = Get-ChildItem -Path "src" -Recurse -Include "*.cpp","*.h"
    
    $suspectFiles = @()
    
    foreach ($file in $cppFiles) {
        $content = Get-Content $file.FullName -Raw
        
        $newCount = ($content -split "\bnew\b").Count - 1
        $deleteCount = ($content -split "\bdelete\b").Count - 1
        $mallocCount = ($content -split "\bmalloc\b").Count - 1
        $freeCount = ($content -split "\bfree\b").Count - 1
        
        $imbalance = ($newCount - $deleteCount) + ($mallocCount - $freeCount)
        
        if ($imbalance -gt 3) {
            $suspectFiles += [PSCustomObject]@{
                File = $file.FullName
                NewCount = $newCount
                DeleteCount = $deleteCount
                MallocCount = $mallocCount
                FreeCount = $freeCount
                Imbalance = $imbalance
            }
        }
    }
    
    Write-Host "  Found $($suspectFiles.Count) files with potential memory leaks" -ForegroundColor $(if ($suspectFiles.Count -gt 0) { "Yellow" } else { "Green" })
    
    foreach ($suspect in $suspectFiles | Select-Object -First 3) {
        $task = [AuditTask]@{
            Category = "MemoryLeak"
            Description = "Review memory management in $([System.IO.Path]::GetFileName($suspect.File))"
            FilePath = $suspect.File
            Priority = 60
            SubAgentPrompt = @"
Review and fix potential memory leaks in $($suspect.File):

Statistics:
- new: $($suspect.NewCount)
- delete: $($suspect.DeleteCount)
- malloc: $($suspect.MallocCount)
- free: $($suspect.FreeCount)
- Imbalance: $($suspect.Imbalance)

Tasks:
1. Audit all heap allocations
2. Ensure each allocation has a corresponding deallocation
3. Consider using RAII (unique_ptr, vector) instead of raw pointers
4. Check for exception paths that might skip cleanup
5. Add cleanup code in destructors if needed
"@
            CreatedAt = Get-Date
            Status = "Pending"
        }
        $script:TaskQueue.Enqueue($task)
    }
}

# ==============================================================================
# SUBAGENT ORCHESTRATION
# ==============================================================================

function Spawn-SubAgent {
    param([AuditTask]$Task)
    
    if ($script:SubAgentCount -ge $MaxSubAgents) {
        Write-Host "  [SWARM] Max subagents reached ($MaxSubAgents), queuing task" -ForegroundColor Yellow
        return $false
    }
    
    if ($DryRun) {
        Write-Host "`n[DRY-RUN] Would spawn subagent for: $($Task.Description)" -ForegroundColor Magenta
        Write-Host "Priority: $($Task.Priority)" -ForegroundColor Gray
        Write-Host "Prompt preview:" -ForegroundColor Gray
        Write-Host ($Task.SubAgentPrompt.Substring(0, [Math]::Min(200, $Task.SubAgentPrompt.Length))) -ForegroundColor DarkGray
        return $true
    }
    
    $script:SubAgentCount++
    $Task.Status = "Running"
    $script:ActiveSubAgents += $Task
    
    Write-Host "`n[SPAWN] Subagent #$script:SubAgentCount: $($Task.Description)" -ForegroundColor Green
    Write-Host "  Category: $($Task.Category) | Priority: $($Task.Priority)" -ForegroundColor Gray
    
    # In real implementation, this would call runSubagent tool
    # For now, we'll simulate by logging the task
    
    $logEntry = @{
        SubAgentId = $script:SubAgentCount
        Task = $Task
        SpawnedAt = Get-Date
    }
    
    # Write task to file for later subagent execution
    $taskFile = "audit_tasks\task_$script:SubAgentCount.json"
    New-Item -ItemType Directory -Path "audit_tasks" -Force | Out-Null
    $logEntry | ConvertTo-Json -Depth 10 | Set-Content $taskFile
    
    Write-Host "  Task file: $taskFile" -ForegroundColor DarkGray
    
    return $true
}

function Process-TaskQueue {
    Write-Host "`n[ORCHESTRATOR] Processing task queue ($($script:TaskQueue.Count) tasks pending)..." -ForegroundColor Cyan
    
    # Sort by priority (highest first)
    $sortedTasks = $script:TaskQueue.ToArray() | Sort-Object -Property Priority -Descending
    
    foreach ($task in $sortedTasks) {
        if ($script:SubAgentCount -ge $MaxSubAgents) {
            Write-Host "`n[ORCHESTRATOR] Subagent capacity reached. Stopping spawn loop." -ForegroundColor Yellow
            break
        }
        
        $spawned = Spawn-SubAgent -Task $task
        if ($spawned) {
            $script:CompletedTasks += $task
            $null = $script:TaskQueue.Dequeue()
        }
    }
}

# ==============================================================================
# RECURSIVE AUDIT LOOP
# ==============================================================================

function Start-RecursiveAudit {
    $iteration = 1
    
    while ($script:SubAgentCount -lt $MaxSubAgents) {
        Write-Host "`n╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
        Write-Host "║  AUDIT ITERATION #$iteration" -ForegroundColor Cyan
        Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
        
        # Run all audit scanners
        Scan-CompileErrors
        Scan-LinkErrors
        Scan-MissingImplementations
        Scan-IncompleteFeatures
        Scan-MemoryLeaks
        
        # Process queue and spawn subagents
        Process-TaskQueue
        
        # Check if we have capacity for more
        if ($script:TaskQueue.Count -eq 0) {
            Write-Host "`n[AUDIT] No more tasks found. Audit complete." -ForegroundColor Green
            break
        }
        
        if ($script:SubAgentCount -ge $MaxSubAgents) {
            Write-Host "`n[AUDIT] Subagent capacity filled. Remaining tasks queued for later." -ForegroundColor Yellow
            break
        }
        
        $iteration++
        
        # Safety limit
        if ($iteration -gt 10) {
            Write-Host "`n[AUDIT] Max iterations reached. Stopping." -ForegroundColor Red
            break
        }
    }
}

# ==============================================================================
# SUMMARY REPORT
# ==============================================================================

function Show-AuditReport {
    Write-Host "`n`n╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Green
    Write-Host "║                  AUDIT SWARM REPORT                          ║" -ForegroundColor Green
    Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    
    Write-Host "`nSubagents Spawned: $script:SubAgentCount / $MaxSubAgents" -ForegroundColor Cyan
    Write-Host "Tasks Completed:   $($script:CompletedTasks.Count)" -ForegroundColor Green
    Write-Host "Tasks Queued:      $($script:TaskQueue.Count)" -ForegroundColor Yellow
    
    Write-Host "`nTasks by Category:" -ForegroundColor White
    $categoryStats = $script:CompletedTasks | Group-Object -Property Category
    foreach ($cat in $categoryStats) {
        Write-Host "  $($cat.Name): $($cat.Count)" -ForegroundColor Gray
    }
    
    if ($script:TaskQueue.Count -gt 0) {
        Write-Host "`nQueued Tasks (not processed due to capacity):" -ForegroundColor Yellow
        foreach ($task in $script:TaskQueue) {
            Write-Host "  [$($task.Category)] $($task.Description)" -ForegroundColor DarkYellow
        }
    }
    
    if (-not $DryRun) {
        Write-Host "`nTask files written to: audit_tasks\" -ForegroundColor Cyan
        Write-Host "To execute subagents, run: Get-ChildItem audit_tasks\*.json | ForEach-Object { /* runSubagent */ }" -ForegroundColor Gray
    }
}

# ==============================================================================
# MAIN EXECUTION
# ==============================================================================

try {
    Start-RecursiveAudit
    Show-AuditReport
    
    Write-Host "`n[SUCCESS] Audit swarm completed!" -ForegroundColor Green
    exit 0
}
catch {
    Write-Host "`n[ERROR] Audit failed: $_" -ForegroundColor Red
    Write-Host $_.ScriptStackTrace -ForegroundColor DarkGray
    exit 1
}
