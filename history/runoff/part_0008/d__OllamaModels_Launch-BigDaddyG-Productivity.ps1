#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Quick launcher for BigDaddyG Productivity Mode
    
.DESCRIPTION
    Simple CLI for productive AI tasks with BigDaddyG models
    
.USAGE
    # Code task (fast)
    .\Launch-BigDaddyG-Productivity.ps1 code "Create a Python function"
    
    # Analysis (balanced)
    .\Launch-BigDaddyG-Productivity.ps1 analysis "Summarize this concept" -Speed balanced
    
    # Creative writing (quality)
    .\Launch-BigDaddyG-Productivity.ps1 writing "Write a blog intro" -Speed quality
#>

param(
    [Parameter(Position=0)]
    [ValidateSet('code', 'docs', 'analysis', 'creative', 'math', 'quick')]
    [string]$TaskType = 'quick',
    
    [Parameter(Position=1)]
    [string]$Prompt,
    
    [ValidateSet('ultra-fast', 'fast', 'balanced', 'quality')]
    [string]$Speed = 'fast',
    
    [switch]$NoCache,
    [switch]$Profile,
    [switch]$Ollama
)

$ModelsDir = "D:\OllamaModels"
cd $ModelsDir

# If Ollama mode, use ollama run
if ($Ollama) {
    Write-Host "🚀 Launching via Ollama..." -ForegroundColor Cyan
    ollama run bigdaddyg-productivity "$Prompt"
    return
}

# Otherwise use production wrapper
if ($TaskType -eq 'quick') {
    # Quick mode - use basic wrapper
    Write-Host "⚡ Quick Mode" -ForegroundColor Yellow
    .\BigDaddyG-Productivity-Wrapper.ps1 `
        -Task 'quick' `
        -Prompt $Prompt `
        -MaxTokens 256
} else {
    # Production mode with full features
    Write-Host "🎯 Production Mode ($TaskType - $Speed)" -ForegroundColor Cyan
    .\BigDaddyG-Production-Wrapper.ps1 `
        -Task $TaskType `
        -Input $Prompt `
        -Speed $Speed `
        -NoCache:$NoCache `
        -Profile:$Profile
}
