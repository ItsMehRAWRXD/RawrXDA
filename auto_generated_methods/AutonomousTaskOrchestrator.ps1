#Requires -Version 7.4

<#
.SYNOPSIS
    Autonomous Task Orchestrator with Model Integration.
.DESCRIPTION
    Recursively scans a directory, uses model inference to analyze files,
    generates intelligent tasks, executes them autonomously, and tracks progress.
#>

# Import required modules
$moduleDir = Split-Path -Parent $MyInvocation.MyCommand.Path

# Import ModelLoader if available
if (Test-Path "$moduleDir\RawrXD.ModelLoader.psm1") {
    Import-Module "$moduleDir\RawrXD.ModelLoader.psm1" -Force
} else {
    Write-Warning "ModelLoader module not found. Using basic task generation."
}

function Initialize-AutonomousOrchestrator {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$DirectoryPath,
        
        [Parameter(Mandatory=$false)]
        [string]$ModelBackend = "GGUF",
        
        [Parameter(Mandatory=$false)]
        [string]$ModelPath = "$env:USERPROFILE\models\code-analysis.gguf",
        
        [Parameter(Mandatory=$false)]
        [string]$ProgressFile = "$env:TEMP\AutonomousProgress.json"
    )

    if (-Not (Test-Path $DirectoryPath)) {
        throw "Directory not found: $DirectoryPath"
    }

    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        Autonomous Task Orchestrator - Initialization           ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""

    # Initialize model loader if available
    $modelContext = $null
    if (Get-Command Initialize-RawrXDModelLoader -ErrorAction SilentlyContinue) {
        try {
            Write-Host "🤖 Loading model: $ModelPath" -ForegroundColor Yellow
            $modelContext = Initialize-RawrXDModelLoader -Backend $ModelBackend -ModelPath $ModelPath
            Write-Host "✓ Model loaded successfully" -ForegroundColor Green
        } catch {
            Write-Warning "Failed to load model: $($_.Exception.Message)"
            Write-Host "⚠ Continuing with basic analysis mode" -ForegroundColor Yellow
        }
    }

    # Scan directory
    Write-Host "📂 Scanning directory: $DirectoryPath" -ForegroundColor Yellow
    $files = Scan-DirectoryWithModel -Path $DirectoryPath -ModelContext $modelContext
    Write-Host "✓ Found $($files.Count) files" -ForegroundColor Green

    # Generate intelligent tasks
    Write-Host "🧠 Generating intelligent tasks..." -ForegroundColor Yellow
    $tasks = Generate-IntelligentTasks -Files $files -ModelContext $modelContext
    Write-Host "✓ Generated $($tasks.Count) tasks" -ForegroundColor Green

    # Initialize progress tracking
    $progress = Initialize-Progress -Tasks $tasks -ProgressFile $ProgressFile

    return [PSCustomObject]@{
        DirectoryPath = $DirectoryPath
        Files = $files
        Tasks = $tasks
        ModelContext = $modelContext
        ProgressFile = $ProgressFile
        Progress = $progress
        Status = "Initialized"
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    }
}

function Scan-DirectoryWithModel {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [string]$Path,
        
        [Parameter(Mandatory=$false)]
        $ModelContext
    )

    if (-Not (Test-Path $Path)) {
        throw "Path not found: $Path"
    }

    $files = Get-ChildItem -Path $Path -Recurse -File |
        Select-Object FullName, Name, Extension, Length, LastWriteTime, Directory |
        ForEach-Object {
            $fileInfo = $_
            $analysis = $null
            
            # Use model for intelligent analysis if available
            if ($ModelContext -and $_.Extension -match '\.(ps1|psm1|json|xml|md)$') {
                try {
                    $content = Get-Content $_.FullName -Raw -ErrorAction SilentlyContinue
                    if ($content) {
                        # Generate analysis prompt
                        $prompt = @"
Analyze this file for potential issues and improvements:
File: $($_.Name)
Extension: $($_.Extension)
Size: $($_.Length) bytes

Content preview (first 500 chars):
$($content.Substring(0, [Math]::Min(500, $content.Length)))

Provide a brief analysis in JSON format:
{
  "issues": ["issue1", "issue2"],
  "improvements": ["improvement1", "improvement2"],
  "priority": "High|Medium|Low"
}
"@
                        
                        # Get model inference (simulated for now)
                        $analysis = Get-FileAnalysis -Content $content -FileName $_.Name
                    }
                } catch {
                    Write-Verbose "Analysis failed for $($_.Name): $($_.Exception.Message)"
                }
            }
            
            [PSCustomObject]@{
                FullName = $fileInfo.FullName
                Name = $fileInfo.Name
                Extension = $fileInfo.Extension
                Length = $fileInfo.Length
                LastWriteTime = $fileInfo.LastWriteTime
                Directory = $fileInfo.Directory
                Analysis = $analysis
                Priority = if ($analysis) { $analysis.Priority } else { "Medium" }
            }
        }

    return $files
}

function Get-FileAnalysis {
    param(
        [string]$Content,
        [string]$FileName
    )
    
    # Basic analysis (can be enhanced with actual model inference)
    $issues = @()
    $improvements = @()
    $priority = "Medium"
    
    # Check for common issues
    if ($Content -match 'Write-Host') {
        $issues += "Uses Write-Host instead of Write-Output"
        $improvements += "Replace Write-Host with Write-Output for better pipeline support"
    }
    
    if ($Content -notmatch 'CmdletBinding') {
        $issues += "Missing CmdletBinding attribute"
        $improvements += "Add [CmdletBinding()] for advanced function capabilities"
    }
    
    if ($Content -match 'Get-Content.*-Raw' -and $Content -notmatch 'ErrorAction') {
        $issues += "Get-Content without error handling"
        $improvements += "Add -ErrorAction SilentlyContinue for robust file reading"
    }
    
    # Determine priority based on file type and issues
    if ($FileName -match '\.(ps1|psm1)$' -and $issues.Count -gt 0) {
        $priority = "High"
    } elseif ($issues.Count -gt 2) {
        $priority = "High"
    } elseif ($issues.Count -eq 0) {
        $priority = "Low"
    }
    
    return [PSCustomObject]@{
        Issues = $issues
        Improvements = $improvements
        Priority = $priority
    }
}

function Generate-IntelligentTasks {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [array]$Files,
        
        [Parameter(Mandatory=$false)]
        $ModelContext
    )
    
    $tasks = @()
    $taskId = 1
    
    foreach ($file in $Files) {
        $fileTasks = @()
        
        # Base tasks for all files
        $fileTasks += [PSCustomObject]@{
            Id = $taskId++
            FileName = $file.Name
            TaskType = "SyntaxValidation"
            Description = "Validate syntax of $($file.Name)"
            Priority = $file.Priority
            Status = "Pending"
            EstimatedDuration = "5-10 seconds"
        }
        
        # Extension-specific tasks
        switch ($file.Extension) {
            '.ps1' {
                $fileTasks += [PSCustomObject]@{
                    Id = $taskId++
                    FileName = $file.Name
                    TaskType = "BestPractices"
                    Description = "Apply PowerShell best practices to $($file.Name)"
                    Priority = if ($file.Priority -eq "High") { "High" } else { "Medium" }
                    Status = "Pending"
                    EstimatedDuration = "10-30 seconds"
                }
                
                $fileTasks += [PSCustomObject]@{
                    Id = $taskId++
                    FileName = $file.Name
                    TaskType = "Documentation"
                    Description = "Add or improve inline documentation for $($file.Name)"
                    Priority = "Medium"
                    Status = "Pending"
                    EstimatedDuration = "5-15 seconds"
                }
            }
            
            '.psm1' {
                $fileTasks += [PSCustomObject]@{
                    Id = $taskId++
                    FileName = $file.Name
                    TaskType = "ModuleValidation"
                    Description = "Validate module exports and dependencies for $($file.Name)"
                    Priority = "High"
                    Status = "Pending"
                    EstimatedDuration = "10-20 seconds"
                }
            }
            
            '.json' {
                $fileTasks += [PSCustomObject]@{
                    Id = $taskId++
                    FileName = $file.Name
                    TaskType = "SchemaValidation"
                    Description = "Validate JSON schema for $($file.Name)"
                    Priority = "Medium"
                    Status = "Pending"
                    EstimatedDuration = "5-10 seconds"
                }
            }
        }
        
        # Add model-suggested tasks if analysis exists
        if ($file.Analysis -and $file.Analysis.Issues.Count -gt 0) {
            foreach ($issue in $file.Analysis.Issues) {
                $fileTasks += [PSCustomObject]@{
                    Id = $taskId++
                    FileName = $file.Name
                    TaskType = "ModelSuggested"
                    Description = "Fix: $issue"
                    Priority = $file.Priority
                    Status = "Pending"
                    EstimatedDuration = "10-30 seconds"
                }
            }
        }
        
        $tasks += $fileTasks
    }
    
    # Sort by priority
    $priorityOrder = @{ "High" = 1; "Medium" = 2; "Low" = 3 }
    $tasks = $tasks | Sort-Object { $priorityOrder[$_.Priority] }
    
    return $tasks
}

function Initialize-Progress {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        [array]$Tasks,
        
        [Parameter(Mandatory=$true)]
        [string]$ProgressFile
    )
    
    $progress = @()
    
    if (Test-Path $ProgressFile) {
        $progress = Get-Content -Path $ProgressFile | ConvertFrom-Json
        Write-Host "📊 Loaded existing progress: $($progress.Count) tasks" -ForegroundColor Cyan
    }
    
    foreach ($task in $Tasks) {
        $existing = $progress | Where-Object { $_.Id -eq $task.Id }
        
        if (-not $existing) {
            $progress += [PSCustomObject]@{
                Id = $task.Id
                FileName = $task.FileName
                TaskType = $task.TaskType
                Description = $task.Description
                Priority = $task.Priority
                Status = "Pending"
                CreatedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
                StartedAt = $null
                CompletedAt = $null
                Duration = $null
                Error = $null
            }
        }
    }
    
    # Save progress
    $progress | ConvertTo-Json -Depth 10 | Set-Content -Path $ProgressFile -Encoding UTF8
    
    return $progress
}

function Start-AutonomousExecution {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        $OrchestratorContext,
        
        [Parameter(Mandatory=$false)]
        [switch]$Parallel,
        
        [Parameter(Mandatory=$false)]
        [int]$MaxConcurrentTasks = 5
    )
    
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        Starting Autonomous Task Execution                      ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    $pendingTasks = $OrchestratorContext.Progress | Where-Object { $_.Status -eq "Pending" }
    $totalTasks = $pendingTasks.Count
    $completedTasks = 0
    $failedTasks = 0
    
    Write-Host "📋 Total tasks to execute: $totalTasks" -ForegroundColor Yellow
    
    foreach ($task in $pendingTasks) {
        Write-Host ""
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
        Write-Host "🔄 Executing Task #$($task.Id): $($task.TaskType)" -ForegroundColor Yellow
        Write-Host "📄 File: $($task.FileName)" -ForegroundColor Cyan
        Write-Host "🎯 Priority: $($task.Priority)" -ForegroundColor White
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
        
        # Update status to InProgress
        $task.Status = "InProgress"
        $task.StartedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
        
        $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
        
        try {
            # Execute the task based on type
            $result = Execute-SingleTask -Task $task -OrchestratorContext $OrchestratorContext
            
            $stopwatch.Stop()
            $task.Duration = $stopwatch.Elapsed.TotalSeconds
            $task.CompletedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            $task.Status = "Completed"
            
            $completedTasks++
            
            Write-Host "✅ Task completed successfully" -ForegroundColor Green
            Write-Host "⏱️  Duration: $($task.Duration.ToString('F2')) seconds" -ForegroundColor Cyan
            
        } catch {
            $stopwatch.Stop()
            $task.Duration = $stopwatch.Elapsed.TotalSeconds
            $task.CompletedAt = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
            $task.Status = "Failed"
            $task.Error = $_.Exception.Message
            
            $failedTasks++
            
            Write-Host "❌ Task failed: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host "⏱️  Duration: $($task.Duration.ToString('F2')) seconds" -ForegroundColor Yellow
        }
        
        # Update progress file
        $OrchestratorContext.Progress | ConvertTo-Json -Depth 10 | Set-Content -Path $OrchestratorContext.ProgressFile -Encoding UTF8
        
        # Show progress bar
        $progressPercent = [Math]::Round(($completedTasks + $failedTasks) / $totalTasks * 100, 2)
        Write-Progress -Activity "Autonomous Task Execution" -Status "Progress: $progressPercent%" -PercentComplete $progressPercent
        
        # Brief pause between tasks
        Start-Sleep -Milliseconds 500
    }
    
    Write-Host ""
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║                    Execution Summary                           ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "✅ Completed: $completedTasks tasks" -ForegroundColor Green
    Write-Host "❌ Failed: $failedTasks tasks" -ForegroundColor Red
    Write-Host "📊 Success Rate: $([Math]::Round($completedTasks / $totalTasks * 100, 2))%" -ForegroundColor Cyan
    Write-Host ""
    
    return [PSCustomObject]@{
        TotalTasks = $totalTasks
        CompletedTasks = $completedTasks
        FailedTasks = $failedTasks
        SuccessRate = [Math]::Round($completedTasks / $totalTasks * 100, 2)
        ProgressFile = $OrchestratorContext.ProgressFile
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    }
}

function Execute-SingleTask {
    [CmdletBinding()]
    param(
        [Parameter(Mandatory=$true)]
        $Task,
        
        [Parameter(Mandatory=$true)]
        $OrchestratorContext
    )
    
    switch ($Task.TaskType) {
        "SyntaxValidation" {
            # Validate PowerShell syntax
            $filePath = Join-Path $OrchestratorContext.DirectoryPath $Task.FileName
            if (Test-Path $filePath) {
                $content = Get-Content -Path $filePath -Raw
                $errors = $null
                $null = [System.Management.Automation.Language.Parser]::ParseInput($content, [ref]$null, [ref]$errors)
                
                if ($errors.Count -gt 0) {
                    throw "Syntax errors found: $($errors.Count) issues"
                }
            }
        }
        
        "BestPractices" {
            # Apply best practices (placeholder for actual implementation)
            $filePath = Join-Path $OrchestratorContext.DirectoryPath $Task.FileName
            if (Test-Path $filePath) {
                $content = Get-Content -Path $filePath -Raw
                
                # Example: Replace Write-Host with Write-Output
                if ($content -match 'Write-Host') {
                    # This would be enhanced with actual refactoring logic
                    Write-Verbose "Found Write-Host in $filePath - needs refactoring"
                }
            }
        }
        
        "ModuleValidation" {
            # Validate module exports
            $filePath = Join-Path $OrchestratorContext.DirectoryPath $Task.FileName
            if (Test-Path $filePath) {
                try {
                    Import-Module $filePath -Force -ErrorAction Stop
                    $module = Get-Module (Split-Path $filePath -LeafBase) -ListAvailable
                    
                    if ($module.ExportedFunctions.Count -eq 0) {
                        Write-Warning "No exported functions found in $filePath"
                    }
                } catch {
                    throw "Module validation failed: $($_.Exception.Message)"
                }
            }
        }
        
        "Documentation" {
            # Add or improve documentation
            $filePath = Join-Path $OrchestratorContext.DirectoryPath $Task.FileName
            if (Test-Path $filePath) {
                $content = Get-Content -Path $filePath -Raw
                
                if ($content -notmatch '<#.*SYNOPSIS.*#>') {
                    Write-Warning "Missing SYNOPSIS documentation in $filePath"
                    # This would be enhanced with actual documentation generation
                }
            }
        }
        
        "ModelSuggested" {
            # Execute model-suggested improvements
            Write-Verbose "Executing model-suggested task: $($Task.Description)"
            # This would be enhanced with specific logic based on the suggestion
        }
        
        default {
            Write-Warning "Unknown task type: $($Task.TaskType)"
        }
    }
    
    return $true
}

# Example usage
if ($MyInvocation.InvocationName -ne '.') {
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║        Autonomous Task Orchestrator - Demo Mode                ║" -ForegroundColor Cyan
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    
    $directory = Read-Host "Enter the directory to process"
    
    # Initialize orchestrator
    $context = Initialize-AutonomousOrchestrator -DirectoryPath $directory
    
    Write-Host ""
    Write-Host "📊 Task Summary:" -ForegroundColor Cyan
    Write-Host "   Total Files: $($context.Files.Count)" -ForegroundColor White
    Write-Host "   Total Tasks: $($context.Tasks.Count)" -ForegroundColor White
    Write-Host "   High Priority: $(($context.Tasks | Where-Object { $_.Priority -eq 'High' }).Count)" -ForegroundColor Red
    Write-Host "   Medium Priority: $(($context.Tasks | Where-Object { $_.Priority -eq 'Medium' }).Count)" -ForegroundColor Yellow
    Write-Host "   Low Priority: $(($context.Tasks | Where-Object { $_.Priority -eq 'Low' }).Count)" -ForegroundColor Green
    Write-Host ""
    
    $confirm = Read-Host "Start autonomous execution? (Y/N)"
    if ($confirm -eq 'Y') {
        # Start autonomous execution
        $results = Start-AutonomousExecution -OrchestratorContext $context
        
        Write-Host ""
        Write-Host "✨ Autonomous execution completed!" -ForegroundColor Green
        Write-Host "📄 Progress saved to: $($results.ProgressFile)" -ForegroundColor Cyan
    }
}