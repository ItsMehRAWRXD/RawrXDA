# ============================================================================
# autonomous_orchestrator_cli.ps1 — Ultimate Autonomous Orchestrator CLI
# ============================================================================
# PowerShell interface for the RawrXD Autonomous Orchestrator
# Supports:
# - Automatic codebase auditing
# - Todo generation and execution
# - Multi-agent cycling (1x-99x)
# - Quality modes (Auto/Balance/Max)
# - Self-adjusting time limits
# - Complete production readiness analysis
# ============================================================================

param(
    [Parameter(Mandatory=$false)]
    [string]$Command = "help",
    
    [Parameter(Mandatory=$false)]
    [string]$Target = ".",
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("Auto", "Balance", "Max")]
    [string]$QualityMode = "Auto",
    
    [Parameter(Mandatory=$false)]
    [ValidateRange(1, 99)]
    [int]$AgentMultiplier = 1,
    
    [Parameter(Mandatory=$false)]
    [ValidateRange(1, 99)]
    [int]$AgentCount = 1,
    
    [Parameter(Mandatory=$false)]
    [int]$TopN = 20,
    
    [Parameter(Mandatory=$false)]
    [int]$TimeoutMs = 30000,
    
    [Parameter(Mandatory=$false)]
    [switch]$AutoAdjustTimeout,
    
    [Parameter(Mandatory=$false)]
    [switch]$RandomizeTimeout,
    
    [Parameter(Mandatory=$false)]
    [switch]$DeepAudit,
    
    [Parameter(Mandatory=$false)]
    [switch]$IgnoreConstraints
)

$ErrorActionPreference = "Stop"
$ProgressPreference = "SilentlyContinue"

# Color functions
function Write-ColorOutput {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Message,
        [Parameter(Mandatory=$false)]
        [ConsoleColor]$ForegroundColor = "White"
    )
    Write-Host $Message -ForegroundColor $ForegroundColor
}

function Write-Header {
    param([string]$Text)
    Write-Host ""
    Write-ColorOutput "╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-ColorOutput "║ $($Text.PadRight(60)) ║" -ForegroundColor Cyan
    Write-ColorOutput "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
}

function Write-Success {
    param([string]$Message)
    Write-ColorOutput "✓ $Message" -ForegroundColor Green
}

function Write-Error {
    param([string]$Message)
    Write-ColorOutput "✗ $Message" -ForegroundColor Red
}

function Write-Warning {
    param([string]$Message)
    Write-ColorOutput "⚠ $Message" -ForegroundColor Yellow
}

function Write-Info {
    param([string]$Message)
    Write-ColorOutput "ℹ $Message" -ForegroundColor Cyan
}

# Banner
function Show-Banner {
    Write-Host ""
    Write-ColorOutput "    ___        __                                               " -ForegroundColor Cyan
    Write-ColorOutput "   /   | __  _/ /_____  ____  ____  ____ ___  ____  __  _______" -ForegroundColor Cyan
    Write-ColorOutput "  / /| |/ / / / __/ __ \/ __ \/ __ \/ __ \`__ \/ __ \/ / / / ___/" -ForegroundColor Cyan
    Write-ColorOutput " / ___ / /_/ / /_/ /_/ / / / / /_/ / / / / / / /_/ / /_/ (__  ) " -ForegroundColor Cyan
    Write-ColorOutput "/_/  |_\__,_/\__/\____/_/ /_/\____/_/ /_/ /_/\____/\__,_/____/  " -ForegroundColor Cyan
    Write-ColorOutput "                                                                  " -ForegroundColor Cyan
    Write-ColorOutput "   ___           __              __            __                " -ForegroundColor Magenta
    Write-ColorOutput "  / _ \________/ /  ___ ___ ___/ /________ __/ /____ ____       " -ForegroundColor Magenta
    Write-ColorOutput " / // / __/ __/ _ \/ -_|_-<_-< _ / __/ _ \`/ __/ __ \/ __/       " -ForegroundColor Magenta
    Write-ColorOutput "/____/_/  \__/_//_/\__/___/___/_/ \__/\_,_/\__/\___/_/          " -ForegroundColor Magenta
    Write-Host ""
    Write-ColorOutput "           RawrXD Ultimate Autonomous Orchestrator v1.0" -ForegroundColor White
    Write-ColorOutput "           Multi-Agent | Self-Adjusting | Production-Ready" -ForegroundColor Gray
    Write-Host ""
}

# Configuration builder
function Build-Config {
    $config = @{
        qualityMode = switch ($QualityMode) {
            "Auto" { 0 }
            "Balance" { 1 }
            "Max" { 2 }
        }
        executionMode = 2  # Adaptive
        agentConfig = @{
            cycleMultiplier = $AgentMultiplier
            agentCount = $AgentCount
            enableDebate = $true
            enableVoting = $true
            consensusThreshold = 0.7
            strategy = 3  # Collaborative
        }
        terminalLimits = @{
            minTimeoutMs = 1000
            maxTimeoutMs = 3600000
            currentTimeoutMs = $TimeoutMs
            autoAdjust = $AutoAdjustTimeout.IsPresent
            randomize = $RandomizeTimeout.IsPresent
            randomVariancePercent = 10
            strategy = if ($AutoAdjustTimeout) { 3 } else { 0 }  # Adaptive or Fixed
        }
        maxConcurrentTasks = 4
        maxRetries = 3
        failFast = $false
        autoSave = $true
        autoSaveIntervalSeconds = 60
        enableDetailedLogging = $true
        enableTelemetry = $true
        enableSelfHealing = $true
        ignoreTokenLimits = $IgnoreConstraints.IsPresent
        ignoreTimeLimits = $IgnoreConstraints.IsPresent
        ignoreComplexityLimits = $IgnoreConstraints.IsPresent
        workspaceRoot = (Get-Location).Path
        progressFile = "orchestrator_progress.json"
    }
    
    return $config | ConvertTo-Json -Depth 10
}

# Execute RawrXD with orchestrator
function Invoke-Orchestrator {
    param(
        [string]$OrchestratorCommand,
        [string]$ConfigJson
    )
    
    $configFile = [System.IO.Path]::GetTempFileName() + ".json"
    $ConfigJson | Out-File -FilePath $configFile -Encoding UTF8
    
    try {
        $exe = ".\RawrXD-Shell.exe"
        if (-not (Test-Path $exe)) {
            $exe = ".\build\Release\RawrXD-Shell.exe"
        }
        if (-not (Test-Path $exe)) {
            Write-Error "RawrXD-Shell.exe not found!"
            return $null
        }
        
        Write-Info "Executing: $exe --orchestrator $OrchestratorCommand --config $configFile"
        
        $result = & $exe --orchestrator $OrchestratorCommand --config $configFile 2>&1
        
        return $result
    }
    finally {
        if (Test-Path $configFile) {
            Remove-Item $configFile -Force
        }
    }
}

# Command: Audit
function Invoke-Audit {
    Write-Header "CODEBASE AUDIT"
    
    Write-Info "Target: $Target"
    Write-Info "Deep Audit: $($DeepAudit.IsPresent)"
    Write-Info "Quality Mode: $QualityMode"
    
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    
    $config = Build-Config
    $result = Invoke-Orchestrator "audit:$Target:$($DeepAudit.IsPresent)" $config
    
    $stopwatch.Stop()
    
    if ($result) {
        $resultJson = $result | ConvertFrom-Json
        
        Write-Host ""
        Write-Success "Audit completed in $($stopwatch.Elapsed.TotalSeconds) seconds"
        Write-Host ""
        Write-ColorOutput "📊 AUDIT SUMMARY" -ForegroundColor Yellow
        Write-ColorOutput "─────────────────────────────────────────────────────────────" -ForegroundColor Gray
        Write-Host "  Total Files:        $($resultJson.totalFiles)"
        Write-Host "  Files with Issues:  $($resultJson.filesWithIssues)"
        Write-Host "  Total Lines:        $($resultJson.totalLines)"
        Write-Host "  Critical Issues:    $($resultJson.criticalIssues)" -ForegroundColor Red
        Write-Host "  Warnings:           $($resultJson.warnings)" -ForegroundColor Yellow
        Write-Host "  Suggestions:        $($resultJson.suggestions)" -ForegroundColor Cyan
        Write-Host "  Total Todos:        $($resultJson.todos.Count)"
        Write-Host ""
        
        if ($resultJson.todos.Count -gt 0) {
            Write-ColorOutput "🎯 TOP $TopN PRIORITY TASKS" -ForegroundColor Yellow
            Write-ColorOutput "─────────────────────────────────────────────────────────────" -ForegroundColor Gray
            
            $topTodos = $resultJson.todos | Sort-Object { $_.priority } | Select-Object -First $TopN
            
            foreach ($todo in $topTodos) {
                $priorityColor = switch ($todo.priority) {
                    {$_ -le 3} { "Red" }
                    {$_ -le 6} { "Yellow" }
                    default { "Cyan" }
                }
                
                Write-Host ""
                Write-ColorOutput "  [$($todo.id)] " -ForegroundColor Gray -NoNewline
                Write-ColorOutput "$($todo.title)" -ForegroundColor White
                Write-Host "      Priority: $($todo.priority)/10 | Complexity: $($todo.complexity)/10 | Category: $($todo.category)"
                if ($todo.targetFile) {
                    Write-Host "      File: $($todo.targetFile)" -ForegroundColor Gray
                }
            }
        }
        
        # Save full report
        $reportFile = "audit_report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
        $result | Out-File -FilePath $reportFile -Encoding UTF8
        Write-Host ""
        Write-Success "Full report saved to: $reportFile"
    }
}

# Command: Execute
function Invoke-Execute {
    param([string]$ExecuteMode = "all")
    
    Write-Header "AUTONOMOUS EXECUTION"
    
    Write-Info "Execution Mode: $ExecuteMode"
    Write-Info "Quality Mode: $QualityMode"
    Write-Info "Agent Multiplier: ${AgentMultiplier}x"
    Write-Info "Agent Count: $AgentCount"
    Write-Info "Timeout: $TimeoutMs ms"
    Write-Info "Auto-Adjust: $($AutoAdjustTimeout.IsPresent)"
    
    $config = Build-Config
    $result = Invoke-Orchestrator "execute:$ExecuteMode" $config
    
    if ($result) {
        $resultJson = $result | ConvertFrom-Json
        
        Write-Host ""
        Write-Success "Execution completed!"
        Write-Host ""
        Write-ColorOutput "📈 EXECUTION STATS" -ForegroundColor Yellow
        Write-ColorOutput "─────────────────────────────────────────────────────────────" -ForegroundColor Gray
        Write-Host "  Total Todos:         $($resultJson.totalTodos)"
        Write-Host "  Completed:           $($resultJson.completed)" -ForegroundColor Green
        Write-Host "  Failed:              $($resultJson.failed)" -ForegroundColor Red
        Write-Host "  Skipped:             $($resultJson.skipped)" -ForegroundColor Yellow
        Write-Host "  Total Iterations:    $($resultJson.totalIterations)"
        Write-Host "  Agents Spawned:      $($resultJson.totalAgentsSpawned)"
        Write-Host "  Avg Confidence:      $([math]::Round($resultJson.avgConfidence * 100, 2))%"
        Write-Host "  Avg Task Time:       $([math]::Round($resultJson.avgTaskTimeMs / 1000, 2))s"
        Write-Host "  Success Rate:        $([math]::Round(($resultJson.completed / ($resultJson.completed + $resultJson.failed)) * 100, 2))%"
        Write-Host ""
    }
}

# Command: Execute Top Difficult
function Invoke-ExecuteTopDifficult {
    Write-Header "EXECUTE TOP $TopN MOST DIFFICULT TASKS"
    
    Write-Info "Analyzing complexity and selecting top $TopN tasks..."
    
    $config = Build-Config
    $result = Invoke-Orchestrator "execute-top-difficult:$TopN" $config
    
    if ($result) {
        Write-Success "Top difficult tasks execution completed!"
    }
}

# Command: Status
function Invoke-Status {
    Write-Header "ORCHESTRATOR STATUS"
    
    if (Test-Path "orchestrator_progress.json") {
        $progress = Get-Content "orchestrator_progress.json" | ConvertFrom-Json
        
        Write-ColorOutput "📊 CURRENT PROGRESS" -ForegroundColor Yellow
        Write-ColorOutput "─────────────────────────────────────────────────────────────" -ForegroundColor Gray
        Write-Host "  Total Todos:      $($progress.stats.totalTodos)"
        Write-Host "  Completed:        $($progress.stats.completed)" -ForegroundColor Green
        Write-Host "  Failed:           $($progress.stats.failed)" -ForegroundColor Red
        Write-Host "  In Progress:      $($progress.stats.inProgress)" -ForegroundColor Cyan
        Write-Host "  Progress:         $([math]::Round($progress.stats.completed / $progress.stats.totalTodos * 100, 1))%"
        Write-Host ""
        
        Write-ColorOutput "⚙️  CONFIGURATION" -ForegroundColor Yellow
        Write-ColorOutput "─────────────────────────────────────────────────────────────" -ForegroundColor Gray
        Write-Host "  Quality Mode:     $(('Auto', 'Balance', 'Max')[$progress.config.qualityMode])"
        Write-Host "  Agent Multiplier: $($progress.config.agentConfig.cycleMultiplier)x"
        Write-Host "  Agent Count:      $($progress.config.agentConfig.agentCount)"
        Write-Host "  Timeout:          $($progress.config.terminalLimits.currentTimeoutMs)ms"
        Write-Host ""
    } else {
        Write-Warning "No progress file found. Run an audit or execution first."
    }
}

# Command: Auto-Optimize
function Invoke-AutoOptimize {
    Write-Header "AUTO-OPTIMIZATION"
    
    Write-Info "Analyzing execution patterns and optimizing settings..."
    
    $config = Build-Config
    $result = Invoke-Orchestrator "auto-optimize" $config
    
    if ($result) {
        $recommendations = $result | ConvertFrom-Json
        
        Write-Host ""
        Write-ColorOutput "💡 OPTIMIZATION RECOMMENDATIONS" -ForegroundColor Yellow
        Write-ColorOutput "─────────────────────────────────────────────────────────────" -ForegroundColor Gray
        
        foreach ($rec in $recommendations) {
            Write-Host ""
            Write-ColorOutput "  $($rec.type.ToUpper())" -ForegroundColor Cyan
            Write-Host "  $($rec.message)"
            Write-Host "  Action: $($rec.action)" -ForegroundColor Gray
        }
        Write-Host ""
    }
}

# Command: Help
function Show-Help {
    Show-Banner
    
    Write-ColorOutput "USAGE:" -ForegroundColor Yellow
    Write-Host "  ./autonomous_orchestrator_cli.ps1 -Command <command> [options]"
    Write-Host ""
    
    Write-ColorOutput "COMMANDS:" -ForegroundColor Yellow
    Write-Host "  audit                    Perform comprehensive codebase audit"
    Write-Host "  execute                  Execute all pending todos"
    Write-Host "  execute-top-difficult    Execute top N most difficult tasks"
    Write-Host "  execute-top-priority     Execute top N highest priority tasks"
    Write-Host "  status                   Show current orchestrator status"
    Write-Host "  auto-optimize            Analyze and suggest optimizations"
    Write-Host "  help                     Show this help message"
    Write-Host ""
    
    Write-ColorOutput "OPTIONS:" -ForegroundColor Yellow
    Write-Host "  -Target <path>           Target directory or file (default: .)"
    Write-Host "  -QualityMode <mode>      Auto | Balance | Max (default: Auto)"
    Write-Host "  -AgentMultiplier <1-99>  Agent cycle multiplier (default: 1)"
    Write-Host "  -AgentCount <1-99>       Number of parallel agents (default: 1)"
    Write-Host "  -TopN <number>           Number of top tasks (default: 20)"
    Write-Host "  -TimeoutMs <ms>          Terminal timeout in ms (default: 30000)"
    Write-Host "  -AutoAdjustTimeout       Enable auto-adjusting timeouts"
    Write-Host "  -RandomizeTimeout        Add random variance to timeouts"
    Write-Host "  -DeepAudit               Enable deep analysis during audit"
    Write-Host "  -IgnoreConstraints       Ignore token/time/complexity limits (Max mode)"
    Write-Host ""
    
    Write-ColorOutput "EXAMPLES:" -ForegroundColor Yellow
    Write-Host "  # Perform deep audit of current directory"
    Write-Host "  ./autonomous_orchestrator_cli.ps1 -Command audit -DeepAudit"
    Write-Host ""
    Write-Host "  # Execute top 20 most difficult tasks with Max quality and 8x agents"
    Write-Host "  ./autonomous_orchestrator_cli.ps1 -Command execute-top-difficult -TopN 20 -QualityMode Max -AgentMultiplier 8 -AgentCount 8"
    Write-Host ""
    Write-Host "  # Execute with auto-adjusting timeouts and no constraints"
    Write-Host "  ./autonomous_orchestrator_cli.ps1 -Command execute -AutoAdjustTimeout -IgnoreConstraints"
    Write-Host ""
    Write-Host "  # Check current status"
    Write-Host "  ./autonomous_orchestrator_cli.ps1 -Command status"
    Write-Host ""
}

# Main execution
try {
    switch ($Command.ToLower()) {
        "audit" {
            Invoke-Audit
        }
        "execute" {
            Invoke-Execute
        }
        "execute-top-difficult" {
            Invoke-ExecuteTopDifficult
        }
        "execute-top-priority" {
            Invoke-Execute "top-priority"
        }
        "status" {
            Invoke-Status
        }
        "auto-optimize" {
            Invoke-AutoOptimize
        }
        "help" {
            Show-Help
        }
        default {
            Show-Help
        }
    }
}
catch {
    Write-Host ""
    Write-Error "Error: $($_.Exception.Message)"
    Write-Host ""
    Write-Host "Stack Trace:" -ForegroundColor Gray
    Write-Host $_.ScriptStackTrace -ForegroundColor Gray
    exit 1
}
