<#
.SYNOPSIS
    Task Orchestrator - Translates goals into actionable steps
.DESCRIPTION
    Takes a high-level goal and decomposes it into a sequence of actionable steps
    using AI-powered task decomposition.
.PARAMETER Goal
    The high-level goal to decompose into steps
.PARAMETER Model
    The Ollama model to use for decomposition (default: bigdaddyg-personalized-agentic:latest)
.PARAMETER MaxSteps
    Maximum number of steps to generate (default: 10)
.EXAMPLE
    .\TaskOrchestrator.ps1 -Goal "Build a personal website"
.EXAMPLE
    .\TaskOrchestrator.ps1 -Goal "Learn Python programming" -Model "codellama:7b" -MaxSteps 15
#>

param(
    [Parameter(Mandatory = $true)]
    [string]$Goal,

    [Parameter(Mandatory = $false)]
    [string]$Model = "bigdaddyg-personalized-agentic:latest",

    [Parameter(Mandatory = $false)]
    [int]$MaxSteps = 10,

    [Parameter(Mandatory = $false)]
    [string]$OllamaServer = "http://localhost:11434"
)

$ErrorActionPreference = "Stop"

# Ensure TLS 1.2+ for web requests
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 -bor [Net.SecurityProtocolType]::Tls13

function Invoke-OllamaRequest {
    param(
        [string]$Prompt,
        [string]$ModelName = $Model,
        [int]$MaxTokens = 1000
    )

    $body = @{
        model = $ModelName
        prompt = $Prompt
        stream = $false
        options = @{
            num_predict = $MaxTokens
            temperature = 0.7
        }
    } | ConvertTo-Json

    try {
        $response = Invoke-RestMethod -Uri "$OllamaServer/api/generate" -Method Post -Body $body -ContentType "application/json" -TimeoutSec 60
        return $response.response
    }
    catch {
        throw "Failed to call Ollama API: $_"
    }
}

function Get-TaskSteps {
    param([string]$Goal, [int]$MaxSteps)

    $decompositionPrompt = @"
You are a task decomposition expert. Break down the following goal into a clear, actionable sequence of steps.

Goal: $Goal

Provide the steps in a numbered list format, with each step being specific and executable. Keep it to $MaxSteps steps or fewer if the task is simple.

Format your response as:
1. Step one description
2. Step two description
etc.

Only include the numbered list, no additional text.
"@

    $response = Invoke-OllamaRequest -Prompt $decompositionPrompt -MaxTokens 1500

    # Parse the response into steps
    $lines = $response -split "`n" | Where-Object { $_ -match '^\d+\.\s+' }
    $steps = @()

    foreach ($line in $lines) {
        if ($line -match '^\d+\.\s+(.+)$') {
            $steps += $matches[1].Trim()
        }
    }

    # Limit to MaxSteps
    if ($steps.Count -gt $MaxSteps) {
        $steps = $steps[0..($MaxSteps-1)]
    }

    return $steps
}

# Main execution
Write-Host "🔄 Task Orchestrator" -ForegroundColor Cyan
Write-Host "Goal: $Goal" -ForegroundColor Yellow
Write-Host "Model: $Model" -ForegroundColor Yellow
Write-Host "Max Steps: $MaxSteps" -ForegroundColor Yellow
Write-Host ""

try {
    $steps = Get-TaskSteps -Goal $Goal -MaxSteps $MaxSteps

    if ($steps.Count -eq 0) {
        Write-Host "❌ No steps could be generated for the goal." -ForegroundColor Red
        exit 1
    }

    Write-Host "✅ Generated $($steps.Count) steps:" -ForegroundColor Green
    Write-Host ""

    for ($i = 0; $i -lt $steps.Count; $i++) {
        Write-Host "$($i+1). $($steps[$i])" -ForegroundColor White
    }

    Write-Host ""
    Write-Host "Tip: Execute these steps in order, or use them as a checklist for your project." -ForegroundColor Cyan

} catch {
    Write-Host "❌ Error: $_" -ForegroundColor Red
    exit 1
}

exit 0