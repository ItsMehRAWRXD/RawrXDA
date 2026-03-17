#!/usr/bin/env pwsh
<#
.SYNOPSIS
    BigDaddyG Instant Query - Optimized for speed
    
.DESCRIPTION
    Ultra-fast wrapper:
    - Minimal context (1024 tokens)
    - Smallest batch size
    - Q2_K quantization
    - No system prompt overhead
    - Direct to answer
    
.USAGE
    .\BigDaddyG-Instant.ps1 "Your question here"
    .\BigDaddyG-Instant.ps1 "Code task" -Task code
#>

param(
    [Parameter(Position=0)]
    [string]$Prompt,
    
    [ValidateSet('code', 'writing', 'quick')]
    [string]$Task = 'quick',
    
    [int]$Tokens = 128
)

if (-not $Prompt) {
    Write-Host "Usage: .\BigDaddyG-Instant.ps1 'your question' [-Task code|writing|quick] [-Tokens 128]"
    exit
}

$Model = "D:\OllamaModels\BigDaddyG-Custom-Q2_K.gguf"
$Llama = "D:\OllamaModels\llama.cpp\llama-cli.exe"

if (-not (Test-Path $Model)) {
    Write-Host "❌ Model not found: $Model" -ForegroundColor Red
    exit 1
}

$start = Get-Date

# Ultra-minimal settings for speed
$output = & $Llama `
    -m $Model `
    -n $Tokens `
    -c 1024 `
    -b 128 `
    --temp 0.6 `
    -t 8 `
    -p $Prompt 2>&1

# Display output
$output | Write-Output

$elapsed = ([math]::Round(((Get-Date) - $start).TotalMilliseconds, 0))
Write-Host "" -ForegroundColor Gray
Write-Host "⚡ ${elapsed}ms" -ForegroundColor Cyan
