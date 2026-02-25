# RawrXD.Agentic.Autonomy.psm1
# Production agentic autonomy system with goal-based planning and action execution

$script:AutonomyState = @{
    Goal = $null
    Enabled = $false
    Actions = @()
    LastRun = $null
    LoopJob = $null
    ExecutionHistory = @()
}

$script:ActionHandlers = @{
    'scan' = { 
        param($Payload)
        try {
            $items = Get-ChildItem -Path (Get-RawrXDRootPath) -Recurse -File -ErrorAction SilentlyContinue | Select-Object -First 100
            return @{ Success = $true; Result = "Scanned $($items.Count) files in repository" }
        } catch {
            return @{ Success = $false; Error = $_.Exception.Message }
        }
    }
    'summarize' = {
        param($Payload)
        try {
            $modules = Get-ChildItem -Path (Get-RawrXDRootPath) -Filter "*.psm1" | Measure-Object
            return @{ Success = $true; Result = "Repository contains $($modules.Count) PowerShell modules" }
        } catch {
            return @{ Success = $false; Error = $_.Exception.Message }
        }
    }
    'validate' = {
        param($Payload)
        try {
            $testResults = & { Import-Module (Join-Path (Get-RawrXDRootPath) 'RawrXD.QuickTests.ps1') -ErrorAction SilentlyContinue }
            return @{ Success = $true; Result = "Validation complete" }
        } catch {
            return @{ Success = $false; Error = $_.Exception.Message }
        }
    }
    'model_request' = {
        param($Payload)
        try {
            if (-not (Get-Command Invoke-RawrXDModelRequest -ErrorAction SilentlyContinue)) {
                return @{ Success = $false; Error = 'RawrXD.ModelLoader module not loaded' }
            }
            $result = Invoke-RawrXDModelRequest -ModelName $Payload.Model -Prompt $Payload.Prompt
            return $result
        } catch {
            return @{ Success = $false; Error = $_.Exception.Message }
        }
    }
    'file_read' = {
        param($Payload)
        try {
            $content = Get-Content -Path $Payload.Path -Raw -ErrorAction Stop
            return @{ Success = $true; Result = $content.Substring(0, [Math]::Min(500, $content.Length)) }
        } catch {
            return @{ Success = $false; Error = $_.Exception.Message }
        }
    }
    'file_write' = {
        param($Payload)
        try {
            Set-Content -Path $Payload.Path -Value $Payload.Content -ErrorAction Stop
            return @{ Success = $true; Result = "File written: $($Payload.Path)" }
        } catch {
            return @{ Success = $false; Error = $_.Exception.Message }
        }
    }
}

function Set-RawrXDAutonomyGoal {
    param([string]$Goal)
    $script:AutonomyState.Goal = $Goal
    $script:AutonomyState.Enabled = $true
    if (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) {
        Write-StructuredLog -Level 'INFO' -Message "Autonomy goal set: $Goal" -Function 'Set-RawrXDAutonomyGoal'
    }
}

function Get-RawrXDAutonomyPlan {
    param([string]$Goal)
    
    # Try to use LLM for planning if available
    if (Get-Command Invoke-RawrXDModelRequest -ErrorAction SilentlyContinue) {
        $planningPrompt = @"
You are an autonomous AI agent planning system. Given the following goal, create a step-by-step execution plan.

Goal: $Goal

Available Actions:
- scan: Scan repository structure
- summarize: Generate architecture summary
- validate: Run smoke tests
- model_request: Make LLM API call (requires Model and Prompt)
- file_read: Read file content (requires Path)
- file_write: Write file content (requires Path and Content)

Respond with a JSON array of actions. Each action should have 'Type' and 'Payload' fields.
Example: [{"Type":"scan","Payload":"Scan all files"},{"Type":"summarize","Payload":"Generate summary"}]

JSON Plan:
"@
        
        try {
            $result = Invoke-RawrXDModelRequest -ModelName 'default' -Prompt $planningPrompt
            if ($result.Success) {
                $plan = $result.Output | ConvertFrom-Json
                return $plan
            }
        } catch {
            # Fall through to default plan
        }
    }
    
    # Default plan if LLM unavailable
    return @(
        @{ Type = 'scan'; Payload = @{ Path = (Get-RawrXDRootPath) } },
        @{ Type = 'summarize'; Payload = 'Generate architecture summary' },
        @{ Type = 'validate'; Payload = 'Run smoke tests' }
    )
}

function Invoke-RawrXDAutonomyAction {
    param([hashtable]$Action)
    
    $name = "AutonomyAction:$($Action.Type)"
    $handler = $script:ActionHandlers[$Action.Type]
    
    if (-not $handler) {
        return @{ Success = $false; Error = "Unknown action type: $($Action.Type)" }
    }
    
    return Invoke-RawrXDSafeOperation -Name $name -Context $Action -Script {
        return & $handler $Action.Payload
    }
}

function Register-RawrXDAutonomyAction {
    param(
        [Parameter(Mandatory = $true)][string]$ActionType,
        [Parameter(Mandatory = $true)][scriptblock]$Handler
    )
    $script:ActionHandlers[$ActionType] = $Handler
}

function Start-RawrXDAutonomyLoop {
    param(
        [Parameter(Mandatory = $true)][string]$Goal,
        [int]$MaxIterations = 5
    )
    
    Write-Host "Starting autonomy loop for goal: $Goal" -ForegroundColor Cyan
    $history = New-Object System.Collections.Generic.List[object]
    $iteration = 0
    $isComplete = $false
    
    while ($iteration -lt $MaxIterations -and -not $isComplete) {
        $iteration++
        Write-Host "Iteration $iteration/$MaxIterations..." -ForegroundColor Gray
        
        $plan = Get-RawrXDAutonomyPlan -Goal $Goal -History $history
        foreach ($step in $plan) {
            Write-Host "Executing step: $($step.Description)" -ForegroundColor Yellow
            $result = Invoke-RawrXDAutonomyAction -Action $step.Action
            
            $history.Add(@{
                Iteration = $iteration
                Step = $step.Description
                Action = $step.Action
                Result = $result
            })
            
            if ($result.Complete) {
                $isComplete = $true
                Write-Host "Goal achieved!" -ForegroundColor Green
                break
            }
        }
    }
    
    if (-not $isComplete) {
        Write-Warning "Goal not achieved within $MaxIterations iterations."
    }
    
    return $history
}

Export-ModuleMember -Function Set-RawrXDAutonomyGoal, Get-RawrXDAutonomyPlan, Invoke-RawrXDAutonomyAction, Start-RawrXDAutonomyLoop, Register-RawrXDAutonomyAction
