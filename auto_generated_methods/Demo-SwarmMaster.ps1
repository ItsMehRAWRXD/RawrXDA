# RawrXD Swarm Master Demo
# Complete demonstration of hexmag-style swarm system

#Requires -Version 5.1

<#
.SYNOPSIS
    Demo-SwarmMaster - Complete demonstration of swarm master system

.DESCRIPTION
    Comprehensive demonstration of the swarm master system showing:
    - Complete swarm architecture
    - Dynamic agent spawning and management
    - Task planning, audit, and execution phases
    - Model loader integration
    - Reverse engineering capabilities
    - Self-optimization and continuous improvement
    - Multi-agent coordination
    - Production-ready deployment

.EXAMPLE
    .\Demo-SwarmMaster.ps1
    
    Run the full demonstration

.EXAMPLE
    .\Demo-SwarmMaster.ps1 -ShowSwarmCreation
    
    Show swarm creation

.EXAMPLE
    .\Demo-SwarmMaster.ps1 -ShowTaskExecution
    
    Show task execution
#>

param(
    [Parameter(Mandatory=$false)]
    [switch]$ShowSwarmCreation = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$ShowTaskExecution = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$FullDemo = $true
)

# Import modules
$modulePath = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "=== RawrXD Swarm Master Demo ===" -ForegroundColor Cyan
Write-Host "Loading modules..." -ForegroundColor Yellow

# Import swarm master
Import-Module (Join-Path $modulePath "RawrXD.SwarmMaster.psm1") -Force -Global

Write-Host "Modules loaded successfully!" -ForegroundColor Green
Write-Host ""

# Demo 1: Swarm Master System Status
if ($FullDemo) {
    Write-Host "=== Demo 1: Swarm Master System Status ===" -ForegroundColor Cyan
    Write-Host "Getting swarm master system status..." -ForegroundColor Yellow
    
    $status = Get-SwarmMasterStatus
    
    Write-Host "Swarm Master Status:" -ForegroundColor Green
    Write-Host "  Version: $($status.Version)" -ForegroundColor White
    Write-Host "  Build Date: $($status.BuildDate)" -ForegroundColor White
    Write-Host "  Current Time: $($status.CurrentTime)" -ForegroundColor White
    Write-Host "  PowerShell Version: $($status.PowerShellVersion)" -ForegroundColor White
    Write-Host "  OS Version: $($status.OSVersion)" -ForegroundColor White
    Write-Host "  Is Admin: $($status.IsAdmin)" -ForegroundColor White
    Write-Host "  Loaded Modules: $($status.LoadedModules.Count)" -ForegroundColor White
    Write-Host ""
    
    Write-Host "  Modules:" -ForegroundColor Yellow
    $status.Modules.GetEnumerator() | ForEach-Object {
        $statusColor = if ($_.Value) { "Green" } else { "Red" }
        Write-Host "    ✓ $($_.Key): $($_.Value)" -ForegroundColor $statusColor
    }
    Write-Host ""
    
    Write-Host "  Capabilities: $($status.Capabilities.Count)" -ForegroundColor Yellow
    $status.Capabilities | ForEach-Object {
        Write-Host "    • $_" -ForegroundColor White
    }
    Write-Host ""
    
    Write-Host "  Swarm Configuration:" -ForegroundColor Yellow
    $status.SwarmConfiguration.GetEnumerator() | ForEach-Object {
        Write-Host "    $($_.Key): $($_.Value)" -ForegroundColor White
    }
    Write-Host ""
}

# Demo 2: Initialize Swarm Master System
if ($ShowSwarmCreation -or $FullDemo) {
    Write-Host "=== Demo 2: Initialize Swarm Master System ===" -ForegroundColor Cyan
    Write-Host "Initializing swarm master system..." -ForegroundColor Yellow
    
    $swarmConfig = @{
        MaxAgents = 100
        EnableSelfOptimization = $true
        EnableContinuousImprovement = $true
        TaskTimeoutSeconds = 300
        AgentIdleTimeoutMinutes = 30
        EnableModelLoaderIntegration = $true
        EnableReverseEngineering = $true
        EnableAutonomousEnhancement = $true
    }
    
    $initResult = Initialize-SwarmMasterSystem -LogPath "C:\RawrXD\Logs" -LogLevel "Info" -SwarmConfiguration $swarmConfig
    
    Write-Host "✓ Swarm master system initialized" -ForegroundColor Green
    Write-Host "  Duration: $($initResult.Duration)s" -ForegroundColor White
    Write-Host "  Modules: $($initResult.Modules.Count)" -ForegroundColor White
    Write-Host "  Capabilities: $($initResult.Capabilities.Count)" -ForegroundColor White
    Write-Host ""
}

# Demo 3: Create Swarm Master Orchestrator
if ($ShowSwarmCreation -or $FullDemo) {
    Write-Host "=== Demo 3: Create Swarm Master Orchestrator ===" -ForegroundColor Cyan
    Write-Host "Creating swarm master orchestrator..." -ForegroundColor Yellow
    
    $orchestratorConfig = @{
        MaxAgents = 100
        EnableSelfOptimization = $true
        EnableContinuousImprovement = $true
        TaskTimeoutSeconds = 300
        AgentIdleTimeoutMinutes = 30
        EnableModelLoaderIntegration = $true
        EnableReverseEngineering = $true
        EnableAutonomousEnhancement = $true
    }
    
    $orchestrator = New-SwarmMasterOrchestrator -Configuration $orchestratorConfig
    
    Write-Host "✓ Swarm master orchestrator created: $($orchestrator.OrchestratorId)" -ForegroundColor Green
    Write-Host ""
    
    Write-Host "Orchestrator Configuration:" -ForegroundColor Green
    $orchestratorConfig.GetEnumerator() | ForEach-Object {
        Write-Host "  $($_.Key): $($_.Value)" -ForegroundColor White
    }
    Write-Host ""
}

# Demo 4: Queue Tasks
if ($FullDemo) {
    Write-Host "=== Demo 4: Queue Tasks ===" -ForegroundColor Cyan
    Write-Host "Queueing various tasks..." -ForegroundColor Yellow
    
    # Create different types of tasks
    $tasks = @(
        @{
            TaskId = "TASK-MODEL-001"
            TaskType = "ModelLoading"
            ModelPath = "C:\Models\model.gguf"
            ModelFormat = "GGUF"
            Priority = "High"
            SecurityLevel = "Medium"
        },
        @{
            TaskId = "TASK-CODE-001"
            TaskType = "CodeGeneration"
            FeatureName = "Multi-Agent Collaboration"
            Category = "AgenticIDE"
            Description = "Coordinate multiple AI agents"
            Priority = "High"
        },
        @{
            TaskId = "TASK-TEST-001"
            TaskType = "Testing"
            TestType = "Unit"
            TargetPath = $modulePath
            Priority = "Medium"
        },
        @{
            TaskId = "TASK-DEPLOY-001"
            TaskType = "Deployment"
            DeploymentAction = "InstallService"
            ServiceName = "RawrXDService"
            DisplayName = "RawrXD Production Service"
            BinaryPath = "C:\RawrXD\RawrXD.exe"
            Priority = "High"
        },
        @{
            TaskId = "TASK-OPTIMIZE-001"
            TaskType = "Optimization"
            OptimizationType = "Performance"
            Priority = "Medium"
        },
        @{
            TaskId = "TASK-REVERSE-001"
            TaskType = "ReverseEngineering"
            TargetPath = $modulePath
            AnalysisDepth = "Detailed"
            Priority = "Medium"
        },
        @{
            TaskId = "TASK-COMPLEX-001"
            TaskType = "Complex"
            Description = "Generate, test, and deploy new feature"
            FeatureName = "Predictive Code Completion"
            Category = "AgenticIDE"
            ModelPath = "C:\Models\model.gguf"
            ModelFormat = "GGUF"
            TargetPath = $modulePath
            AnalysisDepth = "Detailed"
            Priority = "High"
            Parallel = $true
        }
    )
    
    Write-Host "Queueing $($tasks.Count) tasks..." -ForegroundColor Yellow
    $tasks | ForEach-Object {
        Add-SwarmTask -Orchestrator $orchestrator -Task $_
        Write-Host "  ✓ Queued: $($_.TaskId) ($($_.TaskType))" -ForegroundColor Green
    }
    Write-Host ""
    
    # Show initial swarm status
    $initialStatus = Get-SwarmStatus -Orchestrator $orchestrator
    Write-Host "Initial Swarm Status:" -ForegroundColor Green
    Write-Host "  Task Queue: $($initialStatus.TaskQueueLength)" -ForegroundColor White
    Write-Host "  Active Agents: $($initialStatus.ActiveAgents)" -ForegroundColor White
    Write-Host "  Agent Pool:" -ForegroundColor White
    $initialStatus.AgentPool.GetEnumerator() | ForEach-Object {
        Write-Host "    $($_.Key): $($_.Value)" -ForegroundColor Gray
    }
    Write-Host ""
}

# Demo 5: Process Tasks
if ($ShowTaskExecution -or $FullDemo) {
    Write-Host "=== Demo 5: Process Tasks ===" -ForegroundColor Cyan
    Write-Host "Processing swarm task queue..." -ForegroundColor Yellow
    
    Start-SwarmProcessing -Orchestrator $orchestrator
    
    Write-Host "✓ Task processing completed" -ForegroundColor Green
    Write-Host ""
    
    # Show final swarm status
    $finalStatus = Get-SwarmStatus -Orchestrator $orchestrator
    Write-Host "Final Swarm Status:" -ForegroundColor Green
    Write-Host "  Task Queue: $($finalStatus.TaskQueueLength)" -ForegroundColor White
    Write-Host "  Completed Tasks: $($finalStatus.CompletedTasks)" -ForegroundColor White
    Write-Host "  Active Agents: $($finalStatus.ActiveAgents)" -ForegroundColor White
    Write-Host "  Performance Metrics:" -ForegroundColor White
    Write-Host "    Tasks Queued: $($finalStatus.PerformanceMetrics.TasksQueued)" -ForegroundColor Gray
    Write-Host "    Tasks Completed: $($finalStatus.PerformanceMetrics.TasksCompleted)" -ForegroundColor Green
    Write-Host "    Tasks Failed: $($finalStatus.PerformanceMetrics.TasksFailed)" -ForegroundColor Red
    Write-Host "    Agents Spawned: $($finalStatus.PerformanceMetrics.AgentsSpawned)" -ForegroundColor White
    Write-Host "    Swarm Efficiency: $([Math]::Round($finalStatus.PerformanceMetrics.SwarmEfficiency, 2))%" -ForegroundColor $(if ($finalStatus.PerformanceMetrics.SwarmEfficiency -gt 80) { "Green" } elseif ($finalStatus.PerformanceMetrics.SwarmEfficiency -gt 50) { "Yellow" } else { "Red" })
    Write-Host ""
    Write-Host "  Agent Pool:" -ForegroundColor White
    $finalStatus.AgentPool.GetEnumerator() | ForEach-Object {
        Write-Host "    $($_.Key): $($_.Value) agents" -ForegroundColor Gray
    }
    Write-Host ""
}

# Demo 6: Run Individual Tasks
if ($FullDemo) {
    Write-Host "=== Demo 6: Run Individual Tasks ===" -ForegroundColor Cyan
    Write-Host "Running individual tasks with swarm master..." -ForegroundColor Yellow
    
    # Model loading task
    Write-Host "Running model loading task..." -ForegroundColor Yellow
    $modelTask = @{
        TaskId = "TASK-MODEL-002"
        TaskType = "ModelLoading"
        ModelPath = "C:\Models\model.gguf"
        ModelFormat = "GGUF"
        Priority = "High"
    }
    
    $modelResult = Invoke-SwarmMasterTask -Orchestrator $orchestrator -Task $modelTask
    Write-Host "✓ Model loading task completed: $($modelResult.Success)" -ForegroundColor $(if ($modelResult.Success) { "Green" } else { "Red" })
    Write-Host ""
    
    # Code generation task
    Write-Host "Running code generation task..." -ForegroundColor Yellow
    $codeTask = @{
        TaskId = "TASK-CODE-002"
        TaskType = "CodeGeneration"
        FeatureName = "Predictive Code Completion"
        Category = "AgenticIDE"
        Description = "AI-powered code completion based on context and patterns"
        Priority = "High"
    }
    
    $codeResult = Invoke-SwarmMasterTask -Orchestrator $orchestrator -Task $codeTask
    Write-Host "✓ Code generation task completed: $($codeResult.Success)" -ForegroundColor $(if ($codeResult.Success) { "Green" } else { "Red" })
    Write-Host "  Code Lines: $($codeResult.CodeLines)" -ForegroundColor White
    Write-Host ""
    
    # Reverse engineering task
    Write-Host "Running reverse engineering task..." -ForegroundColor Yellow
    $reverseTask = @{
        TaskId = "TASK-REVERSE-002"
        TaskType = "ReverseEngineering"
        TargetPath = $modulePath
        AnalysisDepth = "Detailed"
        SelfOptimize = $true
        Priority = "Medium"
    }
    
    $reverseResult = Invoke-SwarmMasterTask -Orchestrator $orchestrator -Task $reverseTask
    Write-Host "✓ Reverse engineering task completed: $($reverseResult.Success)" -ForegroundColor $(if ($reverseResult.Success) { "Green" } else { "Red" })
    Write-Host "  Optimizations Applied: $($reverseResult.OptimizationsApplied)" -ForegroundColor White
    Write-Host ""
}

# Demo 7: Get Enhancement Roadmap
if ($FullDemo) {
    Write-Host "=== Demo 7: Get Enhancement Roadmap ===" -ForegroundColor Cyan
    Write-Host "Getting swarm master enhancement roadmap..." -ForegroundColor Yellow
    
    $roadmap = Get-SwarmMasterRoadmap -Category "SwarmSystem" -Count 10
    
    Write-Host "Enhancement Roadmap:" -ForegroundColor Green
    Write-Host "  Category: $($roadmap.Category)" -ForegroundColor White
    Write-Host "  Total Enhancements: $($roadmap.TotalEnhancements)" -ForegroundColor White
    Write-Host "  High Priority: $($roadmap.HighPriority.Count)" -ForegroundColor Red
    Write-Host "  Medium Priority: $($roadmap.MediumPriority.Count)" -ForegroundColor Yellow
    Write-Host "  Low Priority: $($roadmap.LowPriority.Count)" -ForegroundColor Green
    Write-Host ""
    
    Write-Host "  High Priority Enhancements:" -ForegroundColor Red
    $roadmap.HighPriority | ForEach-Object {
        Write-Host "    - $($_.Feature)" -ForegroundColor White
        Write-Host "      Description: $($_.Description)" -ForegroundColor Gray
        Write-Host "      Priority: $($_.Priority)" -ForegroundColor Red
        Write-Host ""
    }
    
    Write-Host "  Swarm-Specific Enhancements:" -ForegroundColor Yellow
    $roadmap.SwarmEnhancements | ForEach-Object {
        Write-Host "    - $($_.Feature)" -ForegroundColor White
        Write-Host "      Description: $($_.Description)" -ForegroundColor Gray
        Write-Host "      Priority: $($_.Priority)" -ForegroundColor $(if ($_.Priority -eq "High") { "Red" } elseif ($_.Priority -eq "Medium") { "Yellow" } else { "Green" })
        Write-Host ""
    }
    Write-Host ""
}

# Demo 8: Final Swarm Status
if ($FullDemo) {
    Write-Host "=== Demo 8: Final Swarm Status ===" -ForegroundColor Cyan
    $finalStatus = Get-SwarmStatus -Orchestrator $orchestrator
    
    Write-Host "Final Swarm Status:" -ForegroundColor Green
    Write-Host "  Orchestrator ID: $($finalStatus.OrchestratorId)" -ForegroundColor White
    Write-Host "  Is Active: $($finalStatus.IsActive)" -ForegroundColor $(if ($finalStatus.IsActive) { "Green" } else { "Red" })
    Write-Host "  Uptime: $($finalStatus.Uptime)" -ForegroundColor White
    Write-Host ""
    
    Write-Host "  Swarm State:" -ForegroundColor Yellow
    Write-Host "    Status: $($finalStatus.SwarmState.Status)" -ForegroundColor White
    Write-Host "    Current Phase: $($finalStatus.SwarmState.CurrentPhase)" -ForegroundColor White
    Write-Host "    Active Tasks: $($finalStatus.SwarmState.ActiveTasks)" -ForegroundColor White
    Write-Host "    Active Agents: $($finalStatus.SwarmState.ActiveAgents)" -ForegroundColor White
    Write-Host ""
    
    Write-Host "  Performance Metrics:" -ForegroundColor Yellow
    Write-Host "    Tasks Queued: $($finalStatus.PerformanceMetrics.TasksQueued)" -ForegroundColor White
    Write-Host "    Tasks Completed: $($finalStatus.PerformanceMetrics.TasksCompleted)" -ForegroundColor Green
    Write-Host "    Tasks Failed: $($finalStatus.PerformanceMetrics.TasksFailed)" -ForegroundColor Red
    Write-Host "    Agents Spawned: $($finalStatus.PerformanceMetrics.AgentsSpawned)" -ForegroundColor White
    Write-Host "    Agents Shutdown: $($finalStatus.PerformanceMetrics.AgentsShutdown)" -ForegroundColor White
    Write-Host "    Average Task Duration: $([Math]::Round($finalStatus.PerformanceMetrics.AverageTaskDuration, 2))s" -ForegroundColor White
    Write-Host "    Swarm Efficiency: $([Math]::Round($finalStatus.PerformanceMetrics.SwarmEfficiency, 2))%" -ForegroundColor $(if ($finalStatus.PerformanceMetrics.SwarmEfficiency -gt 80) { "Green" } elseif ($finalStatus.PerformanceMetrics.SwarmEfficiency -gt 50) { "Yellow" } else { "Red" })
    Write-Host ""
    
    Write-Host "  Agent Pool:" -ForegroundColor Yellow
    $finalStatus.AgentPool.GetEnumerator() | ForEach-Object {
        Write-Host "    $($_.Key): $($_.Value) agents" -ForegroundColor White
    }
    Write-Host ""
}

# Shutdown swarm
Write-Host "=== Shutdown ===" -ForegroundColor Cyan
Write-Host "Shutting down swarm..." -ForegroundColor Yellow
Stop-Swarm -Orchestrator $orchestrator
Write-Host "✓ Swarm shutdown completed" -ForegroundColor Green
Write-Host ""

# Summary
Write-Host "=== Demo Summary ===" -ForegroundColor Cyan
Write-Host "The RawrXD Swarm Master System can:" -ForegroundColor White
Write-Host "  ✓ Initialize complete swarm master system" -ForegroundColor Green
Write-Host "  ✓ Create swarm master orchestrator" -ForegroundColor Green
Write-Host "  ✓ Queue and process various task types" -ForegroundColor Green
Write-Host "  ✓ Dynamically spawn agents based on tasks" -ForegroundColor Green
Write-Host "  ✓ Plan and audit tasks before execution" -ForegroundColor Green
Write-Host "  ✓ Integrate with model loaders" -ForegroundColor Green
Write-Host "  ✓ Perform reverse engineering" -ForegroundColor Green
Write-Host "  ✓ Apply self-optimization" -ForegroundColor Green
Write-Host "  ✓ Coordinate multi-agent tasks" -ForegroundColor Green
Write-Host "  ✓ Track performance metrics" -ForegroundColor Green
Write-Host "  ✓ Generate enhancement roadmaps" -ForegroundColor Green
Write-Host "  ✓ Manage agent pools efficiently" -ForegroundColor Green
Write-Host "  ✓ Shutdown gracefully" -ForegroundColor Green
Write-Host ""
Write-Host "Key Features:" -ForegroundColor Yellow
Write-Host "  • Hexmag-style swarm architecture" -ForegroundColor White
Write-Host "  • Dynamic agent spawning based on tasks" -ForegroundColor White
Write-Host "  • Task planning and audit phases" -ForegroundColor White
Write-Host "  • Model loader integration" -ForegroundColor White
Write-Host "  • Reverse engineering capabilities" -ForegroundColor White
Write-Host "  • Self-optimization and continuous improvement" -ForegroundColor White
Write-Host "  • Swarm coordination and communication" -ForegroundColor White
Write-Host "  • Custom model performance monitoring" -ForegroundColor White
Write-Host "  • Agentic command execution" -ForegroundColor White
Write-Host "  • Win32 deployment and system integration" -ForegroundColor White
Write-Host "  • Custom model loading and optimization" -ForegroundColor White
Write-Host "  • Autonomous enhancement and self-improvement" -ForegroundColor White
Write-Host "  • Research-driven development" -ForegroundColor White
Write-Host "  • No external dependencies" -ForegroundColor White
Write-Host "  • Production-ready" -ForegroundColor White
Write-Host ""
Write-Host "=== Demo Complete ===" -ForegroundColor Cyan
Write-Host ""

# Show usage examples
Write-Host "=== Usage Examples ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Initialize swarm master system:" -ForegroundColor Yellow
Write-Host "   Initialize-SwarmMasterSystem -LogPath 'C:\RawrXD\Logs' -SwarmConfiguration @{ MaxAgents = 100; EnableSelfOptimization = $true }" -ForegroundColor White
Write-Host ""
Write-Host "2. Create swarm master orchestrator:" -ForegroundColor Yellow
Write-Host "   $orchestrator = New-SwarmMasterOrchestrator -Configuration @{ MaxAgents = 100; EnableSelfOptimization = $true }" -ForegroundColor White
Write-Host ""
Write-Host "3. Queue task:" -ForegroundColor Yellow
Write-Host "   $task = @{ TaskId = 'TASK-001'; TaskType = 'ModelLoading'; ModelPath = 'C:\Models\model.gguf'; Priority = 'High' }" -ForegroundColor White
Write-Host "   Add-SwarmTask -Orchestrator $orchestrator -Task $task" -ForegroundColor White
Write-Host ""
Write-Host "4. Process tasks:" -ForegroundColor Yellow
Write-Host "   Start-SwarmProcessing -Orchestrator $orchestrator" -ForegroundColor White
Write-Host ""
Write-Host "5. Run individual task:" -ForegroundColor Yellow
Write-Host "   $result = Invoke-SwarmMasterTask -Orchestrator $orchestrator -Task $task" -ForegroundColor White
Write-Host ""
Write-Host "6. Get swarm status:" -ForegroundColor Yellow
Write-Host "   $status = Get-SwarmStatus -Orchestrator $orchestrator" -ForegroundColor White
Write-Host ""
Write-Host "7. Get enhancement roadmap:" -ForegroundColor Yellow
Write-Host "   $roadmap = Get-SwarmMasterRoadmap -Category 'SwarmSystem' -Count 10" -ForegroundColor White
Write-Host ""
Write-Host "8. Shutdown swarm:" -ForegroundColor Yellow
Write-Host "   Stop-Swarm -Orchestrator $orchestrator" -ForegroundColor White
Write-Host ""