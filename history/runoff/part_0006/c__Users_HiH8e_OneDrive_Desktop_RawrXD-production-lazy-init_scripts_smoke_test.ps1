#!/usr/bin/env pwsh
# Smoke test script for staging deployment
# Validates basic functionality without full GUI interaction

param(
    [string]$StagingRoot = "D:\RawrXD-Staging",
    [int]$TimeoutSeconds = 10
)

$ErrorActionPreference = "Stop"

Write-Host "RawrXD Win32 IDE - Smoke Tests" -ForegroundColor Cyan
Write-Host "==============================" -ForegroundColor Cyan
Write-Host "Staging Root: $StagingRoot" -ForegroundColor White
Write-Host ""

$testResults = @()

function Test-Item {
    param([string]$Name, [scriptblock]$Test)
    
    try {
        $result = & $Test
        if ($result) {
            Write-Host "  [PASS] $Name" -ForegroundColor Green
            $script:testResults += @{ name = $Name; passed = $true }
            return $true
        } else {
            Write-Host "  [FAIL] $Name" -ForegroundColor Red
            $script:testResults += @{ name = $Name; passed = $false }
            return $false
        }
    } catch {
        Write-Host "  [ERROR] $Name - $_" -ForegroundColor Red
        $script:testResults += @{ name = $Name; passed = $false; error = $_.Exception.Message }
        return $false
    }
}

# Test 1: Binary exists
Write-Host "[1/8] Checking binary..." -ForegroundColor Yellow
Test-Item "Binary exists" {
    Test-Path (Join-Path $StagingRoot "bin\AgenticIDEWin.exe")
}

# Test 2: Config exists
Write-Host "[2/8] Checking config..." -ForegroundColor Yellow
Test-Item "Config exists" {
    Test-Path (Join-Path $StagingRoot "config\config.json")
}

# Test 3: Documentation exists
Write-Host "[3/8] Checking documentation..." -ForegroundColor Yellow
Test-Item "Docs directory exists" {
    Test-Path (Join-Path $StagingRoot "docs")
}

# Test 4: Launchers exist
Write-Host "[4/8] Checking launchers..." -ForegroundColor Yellow
Test-Item "PowerShell launcher exists" {
    Test-Path (Join-Path $StagingRoot "RawrXD.ps1")
}
Test-Item "CMD launcher exists" {
    Test-Path (Join-Path $StagingRoot "RawrXD.bat")
}

# Test 5: Binary is valid PE
Write-Host "[5/8] Validating binary..." -ForegroundColor Yellow
Test-Item "Binary is valid PE" {
    $binary = Join-Path $StagingRoot "bin\AgenticIDEWin.exe"
    $bytes = [System.IO.File]::ReadAllBytes($binary)
    ($bytes[0] -eq 0x4D) -and ($bytes[1] -eq 0x5A)  # MZ header
}

# Test 6: Config is valid JSON
Write-Host "[6/8] Validating config..." -ForegroundColor Yellow
Test-Item "Config is valid JSON" {
    $configPath = Join-Path $StagingRoot "config\config.json"
    $config = Get-Content $configPath -Raw | ConvertFrom-Json
    $config.version -ne $null
}

# Test 7: Launch test (spawn and kill)
Write-Host "[7/8] Testing process launch..." -ForegroundColor Yellow
Test-Item "IDE launches without crash" {
    $binary = Join-Path $StagingRoot "bin\AgenticIDEWin.exe"
    $process = Start-Process -FilePath $binary -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 2
    
    $running = -not $process.HasExited
    if ($running) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    $running
}

# Test 8: Check logs directory creation capability
Write-Host "[8/8] Testing log directory..." -ForegroundColor Yellow
Test-Item "Can create logs directory" {
    $logsDir = "$env:LOCALAPPDATA\RawrXD\logs"
    if (-not (Test-Path $logsDir)) {
        New-Item -ItemType Directory -Force -Path $logsDir | Out-Null
    }
    Test-Path $logsDir
}

# Summary
Write-Host ""
Write-Host "==============================" -ForegroundColor Cyan
Write-Host "Smoke Test Summary" -ForegroundColor Cyan
Write-Host "==============================" -ForegroundColor Cyan

$passed = ($testResults | Where-Object { $_.passed }).Count
$total = $testResults.Count
$failed = $total - $passed

Write-Host "Results: $passed/$total passed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })

if ($failed -gt 0) {
    Write-Host ""
    Write-Host "Failed tests:" -ForegroundColor Red
    $testResults | Where-Object { -not $_.passed } | ForEach-Object {
        Write-Host "  - $($_.name)" -ForegroundColor Red
        if ($_.error) {
            Write-Host "    Error: $($_.error)" -ForegroundColor Red
        }
    }
}

exit $(if ($failed -eq 0) { 0 } else { 1 })
