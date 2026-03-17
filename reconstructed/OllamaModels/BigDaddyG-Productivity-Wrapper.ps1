#!/usr/bin/env pwsh
<#
.SYNOPSIS
    BigDaddyG Productivity Wrapper - Optimizes large models for fast, focused work
    
.DESCRIPTION
    Wraps BigDaddyG models (Q2_K, Q4_K_M) for productivity:
    - Reduced context to 2048 tokens (vs 4096) = 50% faster
    - Smaller batch sizes = lower RAM usage
    - System prompts for task focus (coding, writing, analysis)
    - Temperature tuning for consistency
    - Fast inference mode enabled
    
.PARAMETER Model
    Model path or name (default: BigDaddyG-Custom-Q2_K.gguf)
    
.PARAMETER Task
    Productivity task: 'coding', 'writing', 'analysis', 'quick', 'focused'
    
.PARAMETER Prompt
    The user prompt/question
    
.PARAMETER MaxTokens
    Maximum output tokens (default: 256 for productivity)
    
.EXAMPLE
    .\BigDaddyG-Productivity-Wrapper.ps1 -Task coding -Prompt "Fix this Python error"
    
.EXAMPLE
    .\BigDaddyG-Productivity-Wrapper.ps1 -Task analysis -Prompt "Summarize this concept"
#>

param(
    [string]$Model = "BigDaddyG-Custom-Q2_K.gguf",
    [ValidateSet('coding', 'writing', 'analysis', 'quick', 'focused')]
    [string]$Task = 'quick',
    [string]$Prompt,
    [int]$MaxTokens = 256,
    [double]$Temperature = 0.7,
    [int]$ContextSize = 2048,
    [int]$Batch = 256
)

# Verify model exists
if (-not (Test-Path $Model)) {
    Write-Host "❌ Model not found: $Model" -ForegroundColor Red
    exit 1
}

# Define task-specific system prompts
$SystemPrompts = @{
    'coding' = @"
You are an expert programmer assistant. 
- Provide ONLY code solutions with minimal explanation
- Use concise variable names
- Include only necessary comments
- Prioritize speed and clarity over comprehensiveness
- Output code first, explanation second (if needed)
"@
    
    'writing' = @"
You are a focused writing assistant.
- Write concisely and directly
- Avoid flowery language
- Use active voice
- Prioritize clarity over depth
- Get to the point immediately
"@
    
    'analysis' = @"
You are a rapid analysis assistant.
- Provide structured bullet-point analysis
- Focus on key insights only
- Skip unnecessary context
- Lead with conclusions
- Be direct and actionable
"@
    
    'quick' = @"
You are a fast, no-nonsense assistant.
- Answer directly and briefly
- No preamble or filler
- Get straight to the answer
- Optimize for speed
- Respond in under 3 sentences when possible
"@
    
    'focused' = @"
You are a highly focused task assistant.
- Concentrate on the specific task
- Ignore tangential information
- Provide exactly what is asked for
- No elaboration beyond the scope
- Single-minded execution
"@
}

$systemPrompt = $SystemPrompts[$Task]

# Build llama-cli command
$llamaPath = ".\llama.cpp\llama-cli.exe"

if (-not (Test-Path $llamaPath)) {
    Write-Host "❌ llama-cli.exe not found at: $llamaPath" -ForegroundColor Red
    exit 1
}

Write-Host "🚀 BigDaddyG Productivity Mode" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "Model: $(Split-Path $Model -Leaf)" -ForegroundColor White
Write-Host "Task: $Task" -ForegroundColor Yellow
Write-Host "Context: $ContextSize tokens | Max Output: $MaxTokens tokens" -ForegroundColor Gray
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host ""

# Performance metrics
$startTime = Get-Date

# Run inference with productivity optimizations
& $llamaPath `
    -m $Model `
    -n $MaxTokens `
    -c $ContextSize `
    -b $Batch `
    --temp $Temperature `
    -t 8 `
    --no-display-prompt `
    -sys $systemPrompt `
    -p $Prompt

$elapsed = ((Get-Date) - $startTime).TotalSeconds
$tokensPerSecond = [math]::Round($MaxTokens / $elapsed, 2)

Write-Host ""
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
Write-Host "⏱️  Time: ${elapsed}s | Speed: ${tokensPerSecond} tok/s" -ForegroundColor Cyan
Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
