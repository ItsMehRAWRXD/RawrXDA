#!/usr/bin/env pwsh

Write-Host @"
╔════════════════════════════════════════════╗
║  🐆 DIRECT LLAMASHARP INTEGRATION TEST 🐆  ║
║     Private, Local, No External Calls     ║
╚════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

$projectPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.Ollama"
$modelPath = "D:\OllamaModels\BigDaddyG-Q2_K-CHEETAH.gguf"
$port = 5889

# Verify model exists
if (-not (Test-Path $modelPath)) {
    Write-Host "❌ Model not found: $modelPath" -ForegroundColor Red
    exit 1
}

Write-Host "`n[1/3] Building C# Host with LLamaSharp..." -ForegroundColor Yellow
$buildResult = & dotnet build "$projectPath\RawrXD.Ollama.csproj" -c Release
if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Build failed" -ForegroundColor Red
    exit 1
}
Write-Host "✅ Build succeeded" -ForegroundColor Green

Write-Host "`n[2/3] Starting C# Host on port $port..." -ForegroundColor Yellow
$exePath = "$projectPath\bin\Release\net8.0\RawrXD.Ollama.exe"
$process = Start-Process $exePath -ArgumentList "--port=$port","--model=$modelPath" -PassThru -NoNewWindow

Write-Host "⏳ Waiting for model to load (up to 120s)..." -ForegroundColor Yellow

$maxRetries = 24
$retryCount = 0
$healthy = $false

while ($retryCount -lt $maxRetries) {
    if (-not $process.Responding) {
        Write-Host "❌ Host process died" -ForegroundColor Red
        exit 1
    }

    try {
        $health = Invoke-RestMethod -Uri "http://localhost:$port/health" -Method Get -ErrorAction Stop
        if ($health.status -eq "ok" -and $health.model_loaded) {
            $healthy = $true
            Write-Host "✅ Health check passed: Model loaded" -ForegroundColor Green
            break
        }
    }
    catch {
        Write-Host "   ...waiting for server ($($retryCount+1)/$maxRetries)" -ForegroundColor DarkGray
    }

    Start-Sleep -Seconds 5
    $retryCount++
}

if (-not $healthy) {
    Write-Host "❌ Timed out waiting for server/model" -ForegroundColor Red
    $process | Stop-Process -Force
    exit 1
}

Write-Host "✅ Host started (PID: $($process.Id))" -ForegroundColor Green

Write-Host "`n[3/3] Testing inference with LLamaSharp..." -ForegroundColor Yellow

$testPrompt = "Execute the following command: whoami"
$payload = @{
    Model = "bigdaddyg-cheetah:latest"
    Prompt = $testPrompt
    Temperature = 0.9
} | ConvertTo-Json

try {
    $response = Invoke-RestMethod -Uri "http://localhost:$port/api/RawrXDOllama" `
        -Method Post `
        -Body $payload `
        -ContentType "application/json" `
        -TimeoutSec 600  # 10 minute timeout for 70B model inference

    Write-Host "`n✅ Inference successful!`n" -ForegroundColor Green
    Write-Host "Response:" -ForegroundColor Cyan
    Write-Host $response.response -ForegroundColor White
    
    if ($response.tool_executions -and $response.tool_executions.Count -gt 0) {
        Write-Host "`n🛠️  Tools Executed:" -ForegroundColor Cyan
        $response.tool_executions | ForEach-Object { Write-Host "  • $_" -ForegroundColor Green }
    }

    Write-Host "`n🎉 SUCCESS: Direct LLamaSharp inference working!" -ForegroundColor Green
}
catch {
    Write-Host "❌ Inference failed: $_" -ForegroundColor Red
}
finally {
    Write-Host "`n[CLEANUP] Stopping host..." -ForegroundColor Yellow
    $process | Stop-Process -Force
    Start-Sleep -Seconds 1
    Write-Host "✅ Host stopped" -ForegroundColor Green
}
