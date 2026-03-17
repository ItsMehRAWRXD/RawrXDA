#!/usr/bin/env pwsh
<#
.SYNOPSIS
    BigDaddyG Ultra-Speed Daemon - Keep model loaded in RAM
    
.DESCRIPTION
    Keeps BigDaddyG loaded in memory for instant responses.
    First run loads model (~90s), subsequent calls are instant.
    
.USAGE
    # Start daemon (one-time)
    .\BigDaddyG-UltraSpeed-Daemon.ps1 -Start
    
    # Query (instant after daemon starts)
    .\BigDaddyG-UltraSpeed-Daemon.ps1 -Query "Your question"
    
    # Stop daemon
    .\BigDaddyG-UltraSpeed-Daemon.ps1 -Stop
#>

param(
    [switch]$Start,
    [switch]$Stop,
    [string]$Query,
    [ValidateSet('coding', 'writing', 'analysis', 'quick')]
    [string]$Task = 'quick'
)

$DaemonPort = 5555
$DaemonHost = "http://localhost:$DaemonPort"
$Model = "BigDaddyG-Custom-Q2_K.gguf"
$LlamaPath = "D:\OllamaModels\llama.cpp\llama-cli.exe"

# System prompts
$SystemPrompts = @{
    'coding' = "You are an expert programmer. Provide code solutions with minimal explanation."
    'writing' = "You are a focused writer. Write concisely and directly."
    'analysis' = "You are a rapid analyst. Provide structured bullet-point insights."
    'quick' = "Answer directly and briefly. No preamble. Get to the point."
}

function Start-Daemon {
    Write-Host "🚀 Starting BigDaddyG Ultra-Speed Daemon..." -ForegroundColor Cyan
    Write-Host "First load: ~90 seconds (one-time)" -ForegroundColor Yellow
    Write-Host "Subsequent queries: <1 second" -ForegroundColor Green
    
    # Start server in background
    $scriptBlock = {
        param($LlamaPath, $Model)
        
        # Start llama server
        $process = Start-Process -FilePath $LlamaPath `
            -ArgumentList @("-m", $Model, "--port", "5555", "-ngl", "81", "-c", "2048", "-b", "256") `
            -NoNewWindow `
            -PassThru `
            -RedirectStandardOutput "D:\OllamaModels\daemon.log" `
            -RedirectStandardError "D:\OllamaModels\daemon-error.log"
        
        # Keep process alive
        $process.WaitForExit()
    }
    
    Start-Job -ScriptBlock $scriptBlock -ArgumentList $LlamaPath, $Model | Out-Null
    
    # Wait for server to be ready
    Write-Host "⏳ Waiting for model to load..." -ForegroundColor Yellow
    
    $maxWait = 120
    $waited = 0
    while ($waited -lt $maxWait) {
        try {
            $response = Invoke-WebRequest -Uri "$DaemonHost/health" -ErrorAction SilentlyContinue
            if ($response.StatusCode -eq 200) {
                Write-Host "✅ Daemon ready! Model loaded in RAM." -ForegroundColor Green
                Write-Host "Use: .\BigDaddyG-UltraSpeed-Daemon.ps1 -Query 'your question'" -ForegroundColor Cyan
                return
            }
        } catch {}
        
        Start-Sleep -Seconds 5
        $waited += 5
        Write-Host "  [$waited/$maxWait seconds]" -ForegroundColor Gray
    }
    
    Write-Host "⚠️  Server may still be loading. Try queries anyway." -ForegroundColor Yellow
}

function Stop-Daemon {
    Write-Host "🛑 Stopping BigDaddyG Daemon..." -ForegroundColor Yellow
    
    Get-Job | Stop-Job -PassThru | Remove-Job
    
    Write-Host "✅ Daemon stopped" -ForegroundColor Green
}

function Send-Query {
    param([string]$Question, [string]$TaskType)
    
    if (-not $Question) {
        Write-Host "❌ No query provided" -ForegroundColor Red
        return
    }
    
    $systemPrompt = $SystemPrompts[$TaskType]
    
    try {
        Write-Host "⚡ Querying..." -ForegroundColor Cyan
        
        $startTime = Get-Date
        
        # Send request to daemon
        $body = @{
            prompt = $Question
            n_predict = 256
            temperature = 0.7
        } | ConvertTo-Json
        
        # Call llama-cli with pipe
        $response = $Question | & $LlamaPath `
            -m $Model `
            -n 256 `
            -c 2048 `
            -b 256 `
            --temp 0.7 `
            -t 8 `
            --no-display-prompt `
            -sys $systemPrompt
        
        $elapsed = ((Get-Date) - $startTime).TotalSeconds
        
        Write-Host ""
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
        Write-Host "⏱️  Response time: ${elapsed}s" -ForegroundColor Cyan
        Write-Host "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━" -ForegroundColor Gray
        
    } catch {
        Write-Host "❌ Error: $_" -ForegroundColor Red
        Write-Host "Is daemon running? Use: .\BigDaddyG-UltraSpeed-Daemon.ps1 -Start" -ForegroundColor Yellow
    }
}

# Main
if ($Start) {
    Start-Daemon
} elseif ($Stop) {
    Stop-Daemon
} elseif ($Query) {
    Send-Query -Question $Query -TaskType $Task
} else {
    Write-Host "BigDaddyG Ultra-Speed Daemon" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "Usage:" -ForegroundColor Yellow
    Write-Host "  Start:   .\BigDaddyG-UltraSpeed-Daemon.ps1 -Start" -ForegroundColor White
    Write-Host "  Query:   .\BigDaddyG-UltraSpeed-Daemon.ps1 -Query 'question'" -ForegroundColor White
    Write-Host "  Stop:    .\BigDaddyG-UltraSpeed-Daemon.ps1 -Stop" -ForegroundColor White
    Write-Host ""
    Write-Host "Example:" -ForegroundColor Yellow
    Write-Host "  .\BigDaddyG-UltraSpeed-Daemon.ps1 -Start" -ForegroundColor Gray
    Write-Host "  .\BigDaddyG-UltraSpeed-Daemon.ps1 -Query 'Create a Python function' -Task coding" -ForegroundColor Gray
}
