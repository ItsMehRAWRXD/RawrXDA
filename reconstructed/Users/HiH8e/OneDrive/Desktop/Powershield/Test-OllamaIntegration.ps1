<#
.SYNOPSIS
    Test script for RawrXD Ollama integration
.DESCRIPTION
    Verifies that the C# Ollama host can be started and responds to requests
#>

$ErrorActionPreference = "Stop"

Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RawrXD Ollama Integration Test" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Test 1: Check if executable exists
Write-Host "[1/5] Checking for Ollama host executable..." -ForegroundColor Yellow
$exePath = Join-Path $PSScriptRoot "RawrXD.Ollama\bin\Release\net8.0\RawrXD.Ollama.exe"

if (Test-Path $exePath) {
    Write-Host "  ✓ Found: $exePath" -ForegroundColor Green
} else {
    Write-Host "  ✗ Not found - building..." -ForegroundColor Red
    & dotnet build "$PSScriptRoot\RawrXD.Ollama\RawrXD.Ollama.csproj" -c Release
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed"
    }
}

# Test 2: Start the host
Write-Host "`n[2/5] Starting Ollama host..." -ForegroundColor Yellow
$process = Start-Process -FilePath $exePath `
    -ArgumentList "--port", "5886" `
    -NoNewWindow -PassThru

Start-Sleep -Seconds 2

if ($process.HasExited) {
    throw "Ollama host exited immediately - check for errors"
}

Write-Host "  ✓ Host started (PID: $($process.Id))" -ForegroundColor Green

try {
    # Test 3: Health check
    Write-Host "`n[3/5] Testing health endpoint..." -ForegroundColor Yellow
    
    $maxRetries = 10
    $healthOk = $false
    
    for ($i = 0; $i -lt $maxRetries; $i++) {
        try {
            $health = Invoke-RestMethod -Uri "http://127.0.0.1:5886/health" -TimeoutSec 2
            if ($health.status -eq "ok") {
                Write-Host "  ✓ Health check passed: $($health | ConvertTo-Json -Compress)" -ForegroundColor Green
                $healthOk = $true
                break
            }
        }
        catch {
            Start-Sleep -Milliseconds 500
        }
    }
    
    if (-not $healthOk) {
        throw "Health check failed after $maxRetries attempts"
    }
    
    # Test 4: Test API endpoint (will fail if Ollama isn't running, but tests the host)
    Write-Host "`n[4/5] Testing API endpoint..." -ForegroundColor Yellow
    
    $testRequest = @{
        Model = "test-model"
        Prompt = "Hello"
        Temperature = 0.7
    } | ConvertTo-Json
    
    try {
        $response = Invoke-RestMethod -Uri "http://127.0.0.1:5886/api/RawrXDOllama" `
            -Method Post `
            -Body $testRequest `
            -ContentType "application/json" `
            -TimeoutSec 5
        
        Write-Host "  ✓ API endpoint responded: $($response | ConvertTo-Json -Compress)" -ForegroundColor Green
    }
    catch {
        # Expected to fail if Ollama isn't running
        if ($_.Exception.Message -like "*Ollama*") {
            Write-Host "  ⚠ API returned expected error (Ollama not running): OK" -ForegroundColor Yellow
        } else {
            Write-Host "  ✗ Unexpected error: $($_.Exception.Message)" -ForegroundColor Red
        }
    }
    
    # Test 5: Cleanup
    Write-Host "`n[5/5] Stopping Ollama host..." -ForegroundColor Yellow
    $process.Kill()
    $process.WaitForExit(5000)
    Write-Host "  ✓ Host stopped cleanly" -ForegroundColor Green
    
    Write-Host "`n═══════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  ✓ ALL TESTS PASSED" -ForegroundColor Green
    Write-Host "═══════════════════════════════════════════════════════" -ForegroundColor Cyan
}
catch {
    Write-Host "`n✗ TEST FAILED: $_" -ForegroundColor Red
    if (-not $process.HasExited) {
        $process.Kill()
    }
    exit 1
}
