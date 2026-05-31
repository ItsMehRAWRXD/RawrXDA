#Requires -Version 5.1
<#
.SYNOPSIS
    Smoke test for RawrXD sovereign inference with timeout fixes.
.DESCRIPTION
    Tests the sovereign inference engine endpoints with extended timeouts
    for complex chat/completion routes. Validates that the timeout fixes
    and pre-warming are working correctly.
#>
param(
    [string]$ExePath = "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe",
    [int]$Port = 39291,
    [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = "Stop"
$script:allPassed = $true

function Test-Endpoint {
    param(
        [string]$Name,
        [string]$Method,
        [string]$Path,
        [string]$Body = "",
        [int]$ExpectedStatus = 200,
        [int]$TimeoutMs = 30000
    )
    
    try {
        $uri = "http://127.0.0.1:$Port$Path"
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        
        if ($Method -eq "GET") {
            $response = Invoke-WebRequest -Uri $uri -Method GET -TimeoutSec ($TimeoutMs / 1000) -ErrorAction Stop
        } else {
            $response = Invoke-WebRequest -Uri $uri -Method POST -Body $Body -ContentType "application/json" -TimeoutSec ($TimeoutMs / 1000) -ErrorAction Stop
        }
        
        $sw.Stop()
        $elapsed = $sw.ElapsedMilliseconds
        
        if ($response.StatusCode -eq $ExpectedStatus) {
            Write-Host "  PASS $Name (${elapsed}ms)" -ForegroundColor Green
            return $true
        } else {
            Write-Host "  FAIL $Name - Status $($response.StatusCode) (expected $ExpectedStatus)" -ForegroundColor Red
            $script:allPassed = $false
            return $false
        }
    } catch {
        Write-Host "  FAIL $Name - $($_.Exception.Message)" -ForegroundColor Red
        $script:allPassed = $false
        return $false
    }
}

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrXD Sovereign Inference Smoke Test" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Exe: $ExePath" -ForegroundColor Gray
Write-Host "Port: $Port" -ForegroundColor Gray
Write-Host "Timeout: ${TimeoutSeconds}s" -ForegroundColor Gray
Write-Host ""

# Verify executable exists
if (-not (Test-Path $ExePath)) {
    Write-Host "ERROR: Executable not found at $ExePath" -ForegroundColor Red
    exit 1
}

# Start the server
Write-Host "Starting RawrXD server..." -ForegroundColor Yellow
$proc = Start-Process -FilePath $ExePath -ArgumentList @("--headless", "--port", $Port) -PassThru -WindowStyle Hidden

# Wait for server to be ready
Write-Host "Waiting for server to start (10s)..." -ForegroundColor Yellow
Start-Sleep -Seconds 10

# Check if process is still running
if ($proc.HasExited) {
    Write-Host "ERROR: Server process exited early (code $($proc.ExitCode))" -ForegroundColor Red
    exit 1
}

Write-Host "Server PID $($proc.Id) running. Running tests..." -ForegroundColor Green
Write-Host ""

# Test 1: Health check (fast)
Test-Endpoint -Name "Health Check" -Method "GET" -Path "/api/health" -TimeoutMs 5000

# Test 2: Status endpoint
Test-Endpoint -Name "Status" -Method "GET" -Path "/api/status" -TimeoutMs 5000

# Test 3: Tags endpoint
Test-Endpoint -Name "Tags" -Method "GET" -Path "/api/tags" -TimeoutMs 5000

# Test 4: Simple generation (short prompt)
$simpleBody = '{"model":"Sovereign-Small","prompt":"Hello"}'
Test-Endpoint -Name "Simple Generation" -Method "POST" -Path "/api/generate" -Body $simpleBody -TimeoutMs 60000

# Test 5: Chat completion (complex route with extended timeout)
$chatBody = '{"model":"Sovereign-Small","messages":[{"role":"user","content":"Say hello"}]}'
Test-Endpoint -Name "Chat Completion" -Method "POST" -Path "/v1/chat/completions" -Body $chatBody -TimeoutMs ($TimeoutSeconds * 1000)

# Test 6: CoT health endpoint
Test-Endpoint -Name "CoT Health" -Method "GET" -Path "/api/cot/health" -TimeoutMs 5000

# Cleanup
Write-Host ""
Write-Host "Stopping server..." -ForegroundColor Yellow
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

# Summary
Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
if ($script:allPassed) {
    Write-Host "ALL TESTS PASSED" -ForegroundColor Green
    exit 0
} else {
    Write-Host "SOME TESTS FAILED" -ForegroundColor Red
    exit 1
}
