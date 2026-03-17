#!/usr/bin/env pwsh
<#
.SYNOPSIS
    BigDaddyG Production Wrapper - Enterprise productivity for large models
    
.DESCRIPTION
    Advanced wrapper for BigDaddyG models with:
    - Model auto-detection and selection
    - Response caching to avoid re-computation
    - Multi-model fallback (fast→quality)
    - Output formatting (code, markdown, json)
    - Streaming mode for long outputs
    - Performance profiling
    - Batch processing
    
.PARAMETER Task
    Task type: 'code', 'docs', 'analysis', 'creative', 'math'
    
.PARAMETER Input
    Input file or direct text prompt
    
.PARAMETER Format
    Output format: 'raw', 'markdown', 'code', 'json'
    
.PARAMETER Speed
    Speed vs Quality: 'ultra-fast', 'fast', 'balanced', 'quality'
    
.EXAMPLE
    .\BigDaddyG-Production-Wrapper.ps1 -Task code -Input "Generate a sorting function" -Format code -Speed fast
#>

param(
    [ValidateSet('code', 'docs', 'analysis', 'creative', 'math')]
    [string]$Task = 'code',
    
    [string]$Input,
    
    [ValidateSet('raw', 'markdown', 'code', 'json')]
    [string]$Format = 'raw',
    
    [ValidateSet('ultra-fast', 'fast', 'balanced', 'quality')]
    [string]$Speed = 'balanced',
    
    [switch]$NoCache,
    [switch]$Profile,
    [switch]$Stream
)

# Configuration
$CacheDir = "D:\OllamaModels\cache"
$LogDir = "D:\OllamaModels\logs"
$ModelsDir = "D:\OllamaModels"

# Create directories if needed
@($CacheDir, $LogDir) | ForEach-Object {
    if (-not (Test-Path $_)) { New-Item -ItemType Directory -Path $_ -Force | Out-Null }
}

# Model selection based on speed preference
$ModelMap = @{
    'ultra-fast' = @{
        models = @('BigDaddyG-Custom-Q2_K.gguf', 'BigDaddyG-Q2_K-ULTRA.gguf')
        ctx = 1024
        batch = 128
        temp = 0.5
        topk = 20
    }
    'fast' = @{
        models = @('BigDaddyG-Q2_K-ULTRA.gguf', 'BigDaddyG-Custom-Q2_K.gguf')
        ctx = 2048
        batch = 256
        temp = 0.6
        topk = 30
    }
    'balanced' = @{
        models = @('BigDaddyG-Q4_K_M-UNLEASHED.gguf', 'BigDaddyG-Custom-Q2_K.gguf')
        ctx = 2048
        batch = 512
        temp = 0.7
        topk = 40
    }
    'quality' = @{
        models = @('bigdaddyg_q5_k_m.gguf', 'BigDaddyG-Q4_K_M-UNLEASHED.gguf')
        ctx = 4096
        batch = 1024
        temp = 0.8
        topk = 50
    }
}

$config = $ModelMap[$Speed]

# Task-specific prompting
$TaskPrompts = @{
    'code' = "You are an expert programmer. Provide clean, efficient code with minimal explanation."
    'docs' = "You are a technical writer. Write clear, concise documentation."
    'analysis' = "You are a data analyst. Provide structured analysis with key insights."
    'creative' = "You are a creative writer. Produce engaging, original content."
    'math' = "You are a mathematician. Provide rigorous mathematical explanations."
}

$systemPrompt = $TaskPrompts[$Task]

# Cache function
function Get-CachedResponse {
    param([string]$InputHash)
    
    $cachePath = Join-Path $CacheDir "$InputHash.cache"
    if (Test-Path $cachePath) {
        return Get-Content $cachePath -Raw
    }
    return $null
}

function Save-CachedResponse {
    param([string]$InputHash, [string]$Response)
    
    $cachePath = Join-Path $CacheDir "$InputHash.cache"
    Set-Content -Path $cachePath -Value $Response -Force
}

# Find best available model
$selectedModel = $null
foreach ($model in $config.models) {
    $modelPath = Join-Path $ModelsDir $model
    if (Test-Path $modelPath) {
        $selectedModel = $modelPath
        Write-Host "✅ Selected: $(Split-Path $model -Leaf)" -ForegroundColor Green
        break
    }
}

if (-not $selectedModel) {
    Write-Host "❌ No suitable models found!" -ForegroundColor Red
    exit 1
}

# Load input
if (Test-Path $Input) {
    $inputText = Get-Content $Input -Raw
} else {
    $inputText = $Input
}

# Hashing for cache
$inputHash = (
    [System.Security.Cryptography.MD5]::Create().ComputeHash(
        [System.Text.Encoding]::UTF8.GetBytes($inputText)
    ) | ForEach-Object { $_.ToString("x2") }
) -join ""

# Check cache
if (-not $NoCache) {
    $cached = Get-CachedResponse $inputHash
    if ($cached) {
        Write-Host "⚡ Cache HIT - returning cached response" -ForegroundColor Yellow
        return $cached
    }
}

# Run inference
Write-Host "🔄 Running inference ($Speed mode)..." -ForegroundColor Cyan

$startTime = Get-Date
$llamaPath = "D:\OllamaModels\llama.cpp\llama-cli.exe"

$args = @(
    "-m", $selectedModel,
    "-n", 512,
    "-c", $config.ctx,
    "-b", $config.batch,
    "--temp", $config.temp,
    "-k", $config.topk,
    "-t", 8,
    "--no-display-prompt",
    "-sys", $systemPrompt,
    "-p", $inputText
)

if ($Stream) { $args += "--stream" }

$response = & $llamaPath @args

$elapsed = ((Get-Date) - $startTime).TotalSeconds

# Cache response
if (-not $NoCache) {
    Save-CachedResponse $inputHash $response
}

# Format output
switch ($Format) {
    'markdown' { $response = "```pwsh`n$response`n```" }
    'code' { $response = "```pwsh`n$response`n```" }
    'json' { $response = $response | ConvertTo-Json }
}

# Profiling
if ($Profile) {
    Write-Host ""
    Write-Host "📊 Performance Profile" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
    Write-Host "Model: $(Split-Path $selectedModel -Leaf)" -ForegroundColor White
    Write-Host "Speed Setting: $Speed" -ForegroundColor Yellow
    Write-Host "Context: $($config.ctx) tokens" -ForegroundColor White
    Write-Host "Batch: $($config.batch)" -ForegroundColor White
    Write-Host "Elapsed: ${elapsed}s" -ForegroundColor Cyan
    Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
}

# Log execution
$logEntry = @"
[$(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')] 
Task: $Task | Speed: $Speed | Elapsed: ${elapsed}s
Input Hash: $inputHash
Model: $(Split-Path $selectedModel -Leaf)
---
$response
---
"@

Add-Content -Path (Join-Path $LogDir "productivity.log") -Value $logEntry

# Output
Write-Output $response
