#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Compare speed of instant wrapper vs daemon approach
#>

Write-Host "=== BigDaddyG Speed Comparison ===" -ForegroundColor Cyan
Write-Host ""

$Model = "D:\OllamaModels\BigDaddyG-Custom-Q2_K.gguf"
$Llama = "D:\OllamaModels\llama.cpp\llama-cli.exe"

# Test 1: Instant wrapper approach (first query)
Write-Host "Test 1: Instant Wrapper (First Load)" -ForegroundColor Yellow
$start = Get-Date
$q1_output = & $Llama -m $Model -n 50 -c 1024 -b 128 --temp 0.6 -k 20 -t 8 -p "What is 2+2?" 2>$null
$q1_time = [math]::Round(((Get-Date) - $start).TotalMilliseconds, 0)
Write-Host "Response: $q1_output"
Write-Host "Time: ${q1_time}ms" -ForegroundColor Green
Write-Host ""

# Test 2: Second query (model already loaded in memory cache)
Write-Host "Test 2: Instant Wrapper (Second Query - Model Cached)" -ForegroundColor Yellow
$start = Get-Date
$q2_output = & $Llama -m $Model -n 50 -c 1024 -b 128 --temp 0.6 -k 20 -t 8 -p "What is 3+3?" 2>$null
$q2_time = [math]::Round(((Get-Date) - $start).TotalMilliseconds, 0)
Write-Host "Response: $q2_output"
Write-Host "Time: ${q2_time}ms" -ForegroundColor Green
Write-Host ""

Write-Host "Speed Improvement: $([math]::Round($q1_time / $q2_time, 1))x faster on second query" -ForegroundColor Cyan
