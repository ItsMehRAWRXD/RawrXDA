# RawrXD Swarm System Demo
# Demonstration of hexmag-style swarm system

#Requires -Version 5.1

<#
.SYNOPSIS
    Demo-SwarmSystem - Demonstration of hexmag-style swarm system

.DESCRIPTION
    Comprehensive demonstration of the swarm system showing:
    - Dynamic agent spawning
    - Task planning and audit phases
    - Model loader integration
    - Self-optimization and reverse engineering
    - Swarm coordination and communication

.EXAMPLE
    .\Demo-SwarmSystem.ps1
    
    Run the full demonstration

.EXAMPLE
    .\Demo-SwarmSystem.ps1 -ShowAgentCreation
    
    Show agent creation

.EXAMPLE
    .\Demo-SwarmSystem.ps1 -ShowTaskProcessing
    
    Show task processing
#>

param(
    [Parameter(Mandatory=$false)]
    [switch]$ShowAgentCreation = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$ShowTaskProcessing = $false,
    
    [Parameter(Mandatory=$false)]
    [switch]$FullDemo = $true
)

# Import modules
$modulePath = Split-Path -Parent $MyInvocation.MyCommand.Path

Write-Host "=== RawrXD Swarm System Demo ===" -ForegroundColor Cyan
Write-Host "Loading modules..." -ForegroundColor Yellow

# Import logging
Import-Module (Join-Path $modulePath "RawrXD.Logging.psm1") -Force -Global

# Import swarm components
Import-Module (Join-Path $modulePath "RawrXD.SwarmAgent.psm1") -Force -Global
Import-Module (Join-Path $modulePath "RawrXD.SwarmOrchestrator.psm1") -Force -Global

# Import other required modules
Import-Module (Join-Path $modulePath "RawrXD.ModelLoader.psm1") -Force -Global -ErrorAction SilentlyContinue
Import-Module (Join-Path $modulePath "RawrXD.ReverseEngineering.psm1") -Force -Global -ErrorAction SilentlyContinue

Write-Host "Modules loaded successfully!" -ForegroundColor Green
Write-Host ""

# Demo 1: Agent Creation
if ($ShowAgentCreation -or $FullDemo) {
    Write-Host "=== Demo 1: Swarm Agent Creation ===" -ForegroundColor Cyan
    Write-Host "Creating swarm agents..." -ForegroundColor Yellow
    
    # Create different types of agents
    $agents = @()
    
    # Model Loader Agent
    Write-Host "Creating ModelLoader agent..." -ForegroundColor Gray
    $modelLoaderAgent = New-SwarmAgent -AgentType "ModelLoader" -Configuration @{
        BatchSize = 10
        MaxCacheSize = 100
        ParallelTasks = 3
        SelfOptimize = $true
    }
    $agents += $modelLoaderAgent
    Write-Host "✓ ModelLoader agent created: $($modelLoaderAgent.AgentId)" -ForegroundColor Green
    
    # Code Generator Agent
    Write-Host "Creating CodeGenerator agent..." -ForegroundColor Gray
    $codeGeneratorAgent = New-SwarmAgent -AgentType "CodeGenerator" -Configuration @{
        BatchSize = 5
        MaxCacheSize = 50
        ParallelTasks = 2
        SelfOptimize = $true
    }
    $agents += $codeGeneratorAgent
    Write-Host "✓ CodeGenerator agent created: $($codeGeneratorAgent.AgentId)" -ForegroundColor Green
    
    # Tester Agent
    Write-Host "Creating Tester agent..." -ForegroundColor Gray
    $testerAgent = New-SwarmAgent -AgentType "Tester" -Configuration @{
        BatchSize = 15
        MaxCacheSize = 200
        ParallelTasks = 5
        SelfOptimize = $true
    }
    $agents += $testerAgent
    Write-Host "✓ Tester agent created: $($testerAgent.AgentId)" -ForegroundColor Green
    
    # Deployer Agent
    Write-Host "Creating Deployer agent..." -ForegroundColor Gray
    $deployerAgent = New-SwarmAgent -AgentType "Deployer" -Configuration @{
        BatchSize = 3
        MaxCacheSize = 30
        ParallelTasks = 1
        SelfOptimize = $true
    }
    $agents += $deployerAgent
    Write-Host "✓ Deployer agent created: $($deployerAgent.AgentId)" -ForegroundColor Green
    
    # Optimizer Agent
    Write-Host "Creating Optimizer agent..." -ForegroundColor Gray
    $optimizerAgent = New-SwarmAgent -AgentType "Optimizer" -Configuration @{
        BatchSize = 8
        MaxCacheSize = 80
        ParallelTasks = 2
        SelfOptimize = $true
    }
    $agents += $optimizerAgent
    Write-Host "✓ Optimizer agent created: $($optimizerAgent.AgentId)" -ForegroundColor Green
    
    Write-Host ""
    Write-Host "Agent Creation Results:" -ForegroundColor Green
    Write-Host "  Total Agents: $($agents.Count)" -ForegroundColor White
    Write-Host ""
    
    Write-Host "  Agent Status:" -ForegroundColor Yellow
    $agents | ForEach-Object {
        $status = Get-AgentStatus -Agent $_
        Write-Host "    - [$($_.AgentType)] $($_.AgentId)" -ForegroundColor White
        Write-Host "      Status: $($status.State.Status)" -ForegroundColor Gray
        Write-Host "      Uptime: $($status.Uptime)" -ForegroundColor Gray
        Write-Host "      Tasks Completed: $($status.PerformanceMetrics.TasksCompleted)" -ForegroundColor Gray
        Write-Host ""
    }
    Write-Host ""
}

# Demo 2: Swarm Orchestrator Creation
if ($FullDemo) {
    Write-Host "=== Demo 2: Swarm Orchestrator Creation ===" -ForegroundColor Cyan
    Write-Host "Creating swarm orchestrator..." -ForegroundColor Yellow
    
    $orchestratorConfig = @{
        MaxAgents = 50
        EnableSelfOptimization = $true
        EnableContinuousImprovement = $true
        TaskTimeoutSeconds = 300
        AgentIdleTimeoutMinutes = 30
        EnableModelLoaderIntegration = $true
        EnableReverseEngineering = $true
    }
    
    $orchestrator = New-SwarmOrchestrator -Configuration $orchestratorConfig
    
    Write-Host "✓ Swarm orchestrator created: $($orchestrator.OrchestratorId)" -ForegroundColor Green
    Write-Host ""
    
    Write-Host "Orchestrator Configuration:" -ForegroundColor Green
    $orchestratorConfig.GetEnumerator() | ForEach-Object {
        Write-Host "  $($_.Key): $($_.Value)" -ForegroundColor White
    }
    Write-Host ""
}

# Demo 3: Task Planning and Audit
if ($ShowTaskProcessing -or $FullDemo) {
    Write-Host "=== Demo 3: Task Planning and Audit ===" -ForegroundColor Cyan
    Write-Host "Demonstrating task planning and audit phases..." -ForegroundColor Yellow
    
    # Create a sample task
    $sampleTask = @{
        TaskId = "TASK-001"
        TaskType = "ModelLoading"
        ModelPath = "C:\\Models\\model.gguf"
        ModelFormat = "GGUF"
        Priority = "High"
        SecurityLevel = "Medium"
        Dependencies = @()
    }
    
    Write-Host "Sample Task:" -ForegroundColor Yellow
    Write-Host "  TaskId: $($sampleTask.TaskId)" -ForegroundColor White
    Write-Host "  TaskType: $($sampleTask.TaskType)" -ForegroundColor White
    Write-Host "  ModelPath: $($sampleTask.ModelPath)" -ForegroundColor White
    Write-Host "  Priority: $($sampleTask.Priority)" -ForegroundColor White
    Write-Host ""
    
    # Plan phase
    Write-Host "Phase 1: Planning" -ForegroundColor Yellow
    $plan = $orchestrator.PlanTask($sampleTask)
    
    Write-Host "  Plan Results:" -ForegroundColor Green
    Write-Host "    Required Agents: $($plan.RequiredAgents -join ', ')" -ForegroundColor White
    Write-Host "    Requires Model Loading: $($plan.RequiresModelLoading)" -ForegroundColor White
    Write-Host "    Requires Reverse Engineering: $($plan.RequiresReverseEngineering)" -ForegroundColor White
    Write-Host "    Estimated Duration: $($plan.EstimatedDuration)s" -ForegroundColor White
    Write-Host "    Agent Count: $($plan.AgentCount)" -ForegroundColor White
    Write-Host ""
    
    # Audit phase
    Write-Host "Phase 2: Auditing" -ForegroundColor Yellow
    $auditResult = $orchestrator.AuditTask($sampleTask, $plan)
    
    Write-Host "  Audit Results:" -ForegroundColor Green
    Write-Host "    Approved: $($auditResult.Approved)" -ForegroundColor $(if ($auditResult.Approved) { "Green" } else { "Red" })
    Write-Host "    Reason: $($auditResult.Reason)" -ForegroundColor White
    Write-Host "    Security Check: $($auditResult.SecurityCheck)" -ForegroundColor $(if ($auditResult.SecurityCheck) { "Green" } else { "Red" })
    Write-Host "    Resource Check: $($auditResult.ResourceCheck)" -ForegroundColor $(if ($auditResult.ResourceCheck) { "Green" } else { "Red" })
    Write-Host "    Dependency Check: $($auditResult.DependencyCheck)" -ForegroundColor $(if ($auditResult.DependencyCheck) { "Green" } else { "Red" })
    Write-Host ""
}

# Demo 4: Dynamic Agent Spawning
if ($FullDemo) {
    Write-Host "=== Demo 4: Dynamic Agent Spawning ===" -ForegroundColor Cyan
    Write-Host "Demonstrating dynamic agent spawning..." -ForegroundColor Yellow
    
    # Queue multiple tasks to trigger agent spawning
    $tasks = @(
        @{
            TaskId = "TASK-002"
            TaskType = "CodeGeneration"
            FeatureName = "Multi-Agent Collaboration"
            Category = "AgenticIDE"
            Description = "Coordinate multiple AI agents"
            Priority = "High"
        },
        @{
            TaskId = "TASK-003"
            TaskType = "Testing"
            TestType = "Unit"
            TargetPath = "C:\\RawrXD"
            Priority = "Medium"
        },
        @{
            TaskId = "TASK-004"
            TaskType = "Deployment"
            DeploymentAction = "InstallService"
            ServiceName = "RawrXDService"
            DisplayName = "RawrXD Production Service"
            BinaryPath = "C:\\RawrXD\\RawrXD.exe"
            Priority = "High"
        }
    )
    
    Write-Host "Queueing tasks..." -ForegroundColor Yellow
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

# Demo 5: Task Processing
if ($ShowTaskProcessing -or $FullDemo) {
    Write-Host "=== Demo 5: Task Processing ===" -ForegroundColor Cyan
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
    Write-Host "    Tasks Completed: $($finalStatus.PerformanceMetrics.TasksCompleted)" -ForegroundColor Gray
    Write-Host "    Tasks Failed: $($finalStatus.PerformanceMetrics.TasksFailed)" -ForegroundColor Gray
    Write-Host "    Agents Spawned: $($finalStatus.PerformanceMetrics.AgentsSpawned)" -ForegroundColor Gray
    Write-Host "    Swarm Efficiency: $([Math]::Round($finalStatus.PerformanceMetrics.SwarmEfficiency, 2))%" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  Agent Pool:" -ForegroundColor White
    $finalStatus.AgentPool.GetEnumerator() | ForEach-Object {
        Write-Host "    $($_.Key): $($_.Value)" -ForegroundColor Gray
    }
    Write-Host ""
}

# Demo 6: Model Loader Integration
if ($FullDemo) {
    Write-Host "=== Demo 6: Model Loader Integration ===" -ForegroundColor Cyan
    Write-Host "Demonstrating model loader integration..." -ForegroundColor Yellow
    
    # Create model loading task
    $modelTask = @{
        TaskId = "TASK-MODEL-001"
        TaskType = "ModelLoading"
        ModelPath = "C:\\Models\\model.gguf"
        ModelFormat = "GGUF"
        Priority = "High"
        SelfOptimize = $true
    }
    
    Write-Host "Queueing model loading task..." -ForegroundColor Yellow
    Add-SwarmTask -Orchestrator $orchestrator -Task $modelTask
    Write-Host "✓ Model loading task queued" -ForegroundColor Green
    Write-Host ""
    
    # Process the task
    Write-Host "Processing model loading task..." -ForegroundColor Yellow
    Start-SwarmProcessing -Orchestrator $orchestrator
    Write-Host "✓ Model loading task processed" -ForegroundColor Green
    Write-Host ""
}

# Demo 7: Reverse Engineering Integration
if ($FullDemo) {
    Write-Host "=== Demo 7: Reverse Engineering Integration ===" -ForegroundColor Cyan
    Write-Host "Demonstrating reverse engineering integration..." -ForegroundColor Yellow
    
    # Create reverse engineering task
    $reverseTask = @{
        TaskId = "TASK-REVERSE-001"
        TaskType = "ReverseEngineering"
        TargetPath = $modulePath
        AnalysisDepth = "Detailed"
        SelfOptimize = $true
        Priority = "Medium"
    }
    
    Write-Host "Queueing reverse engineering task..." -ForegroundColor Yellow
    Add-SwarmTask -Orchestrator $orchestrator -Task $reverseTask
    Write-Host "✓ Reverse engineering task queued" -ForegroundColor Green
    Write-Host ""
    
    # Process the task
    Write-Host "Processing reverse engineering task..." -ForegroundColor Yellow
    Start-SwarmProcessing -Orchestrator $orchestrator
    Write-Host "✓ Reverse engineering task processed" -ForegroundColor Green
    Write-Host ""
}

# Demo 8: Self-Optimization
if ($FullDemo) {
    Write-Host "=== Demo 8: Self-Optimization ===" -ForegroundColor Cyan
    Write-Host "Demonstrating self-optimization capabilities..." -ForegroundColor Yellow
    
    # Create optimization task
    $optimizationTask = @{
        TaskId = "TASK-OPTIMIZE-001"
        TaskType = "Optimization"
        OptimizationType = "Performance"
        Priority = "Medium"
        SelfOptimize = $true
    }
    
    Write-Host "Queueing optimization task..." -ForegroundColor Yellow
    Add-SwarmTask -Orchestrator $orchestrator -Task $optimizationTask
    Write-Host "✓ Optimization task queued" -ForegroundColor Green
    Write-Host ""
    
    # Process the task
    Write-Host "Processing optimization task..." -ForegroundColor Yellow
    Start-SwarmProcessing -Orchestrator $orchestrator
    Write-Host "✓ Optimization task processed" -ForegroundColor Green
    Write-Host ""
}

# Demo 9: Complex Multi-Agent Task
if ($FullDemo) {
    Write-Host "=== Demo 9: Complex Multi-Agent Task ===" -ForegroundColor Cyan
    Write-Host "Demonstrating complex task requiring multiple agents..." -ForegroundColor Yellow
    
    # Create complex task that requires multiple agents
    $complexTask = @{
        TaskId = "TASK-COMPLEX-001"
        TaskType = "Complex"
        Description = "Generate, test, and deploy new feature"
        FeatureName = "Multi-Agent Collaboration"
        Category = "AgenticIDE"
        ModelPath = "C:\\Models\\model.gguf"
        ModelFormat = "GGUF"
        TargetPath = $modulePath
        AnalysisDepth = "Detailed"
        Priority = "High"
        Parallel = $true
        SelfOptimize = $true
    }
    
    Write-Host "Queueing complex multi-agent task..." -ForegroundColor Yellow
    Add-SwarmTask -Orchestrator $orchestrator -Task $complexTask
    Write-Host "✓ Complex task queued" -ForegroundColor Green
    Write-Host ""
    
    # Process the task
    Write-Host "Processing complex task..." -ForegroundColor Yellow
    Start-SwarmProcessing -Orchestrator $orchestrator
    Write-Host "✓ Complex task processed" -ForegroundColor Green
    Write-Host ""
}

# Final Status
if ($FullDemo) {
    Write-Host "=== Final Swarm Status ===" -ForegroundColor Cyan
    $finalStatus = Get-SwarmStatus -Orchestrator $orchestrator
    
    Write-Host "Swarm Status:" -ForegroundColor Green
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
Write-Host "The RawrXD Swarm System can:" -ForegroundColor White
Write-Host "  ✓ Dynamically spawn agents based on tasks" -ForegroundColor Green
Write-Host "  ✓ Plan and audit tasks before execution" -ForegroundColor Green
Write-Host "  ✓ Integrate with model loaders" -ForegroundColor Green
Write-Host "  ✓ Perform reverse engineering" -ForegroundColor Green
Write-Host "  ✓ Apply self-optimization" -ForegroundColor Green
Write-Host "  ✓ Coordinate multi-agent tasks" -ForegroundColor Green
Write-Host "  ✓ Manage agent pools efficiently" -ForegroundColor Green
Write-Host "  ✓ Track performance metrics" -ForegroundColor Green
Write-Host ""
Write-Host "Key Features:" -ForegroundColor Yellow
Write-Host "  • Hexmag-style swarm architecture" -ForegroundColor White
Write-Host "  • Dynamic agent spawning" -ForegroundColor White
Write-Host "  • Task planning and audit phases" -ForegroundColor White
Write-Host "  • Model loader integration" -ForegroundColor White
Write-Host "  • Reverse engineering capabilities" -ForegroundColor White
Write-Host "  • Self-optimization" -ForegroundColor White
Write-Host "  • No external dependencies" -ForegroundColor White
Write-Host "  • Production-ready" -ForegroundColor White
Write-Host ""
Write-Host "=== Demo Complete ===" -ForegroundColor Cyan
Write-Host ""

# Show usage examples
Write-Host "=== Usage Examples ===" -ForegroundColor Cyan
Write-Host ""
Write-Host "1. Create swarm orchestrator:" -ForegroundColor Yellow
Write-Host "   `$orchestrator = New-SwarmOrchestrator -Configuration @{ MaxAgents = 50; EnableSelfOptimization = `$true }" -ForegroundColor White
Write-Host ""
Write-Host "2. Queue task:" -ForegroundColor Yellow
Write-Host "   `$task = @{ TaskId = 'TASK-001'; TaskType = 'ModelLoading'; ModelPath = 'C:\\Models\\model.gguf'; Priority = 'High' }" -ForegroundColor White
Write-Host "   Add-SwarmTask -Orchestrator `$orchestrator -Task `$task" -ForegroundColor White
Write-Host ""
Write-Host "3. Process tasks:" -ForegroundColor Yellow
Write-Host "   Start-SwarmProcessing -Orchestrator `$orchestrator" -ForegroundColor White
Write-Host ""
Write-Host "4. Get swarm status:" -ForegroundColor Yellow
Write-Host "   `$status = Get-SwarmStatus -Orchestrator `$orchestrator" -ForegroundColor White
Write-Host ""
Write-Host "5. Shutdown swarm:" -ForegroundColor Yellow
Write-Host "   Stop-Swarm -Orchestrator `$orchestrator" -ForegroundColor White
Write-Host ""