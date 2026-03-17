#!/usr/bin/env pwsh
# RawrXD GUI Component Test Runner
# Runs comprehensive tests without requiring full application startup

param(
    [switch]$Build,
    [switch]$Verbose,
    [string]$Filter = "*"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = $PSScriptRoot

Write-Host "======================================" -ForegroundColor Cyan
Write-Host "RawrXD GUI Component Test Runner" -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# Step 1: Build test executable if requested
if ($Build) {
    Write-Host "[1/3] Building test executable..." -ForegroundColor Yellow
    
    # Configure with tests enabled
    Push-Location $ProjectRoot
    try {
        if (-not (Test-Path "build")) {
            cmake -B build -G "Visual Studio 17 2022" -A x64
        }
        
        # Build tests
        cmake --build build --config Release --target test_gui_components
        
        if ($LASTEXITCODE -ne 0) {
            Write-Host "✗ Build failed!" -ForegroundColor Red
            exit 1
        }
        Write-Host "✓ Build successful" -ForegroundColor Green
    }
    finally {
        Pop-Location
    }
} else {
    Write-Host "[1/3] Skipping build (use -Build to rebuild)" -ForegroundColor Gray
}

# Step 2: Deploy Qt DLLs for test executable
Write-Host "[2/3] Deploying Qt dependencies..." -ForegroundColor Yellow

$TestExe = Join-Path $ProjectRoot "build\bin\Release\test_gui_components.exe"
$QtDeployQt = "C:\Qt\6.7.3\msvc2022_64\bin\windeployqt.exe"

if (-not (Test-Path $TestExe)) {
    Write-Host "✗ Test executable not found: $TestExe" -ForegroundColor Red
    Write-Host "  Run with -Build flag to build tests" -ForegroundColor Yellow
    exit 1
}

if (Test-Path $QtDeployQt) {
    & $QtDeployQt $TestExe --release --no-translations --force | Out-Null
    Write-Host "✓ Qt DLLs deployed" -ForegroundColor Green
} else {
    Write-Host "⚠ windeployqt not found, skipping" -ForegroundColor Yellow
}

# Step 3: Run tests
Write-Host "[3/3] Running component tests..." -ForegroundColor Yellow
Write-Host ""

Push-Location (Split-Path $TestExe -Parent)
try {
    $StartTime = Get-Date
    
    if ($Verbose) {
        & $TestExe -v2
    } else {
        & $TestExe
    }
    
    $ExitCode = $LASTEXITCODE
    $Duration = (Get-Date) - $StartTime
    
    Write-Host ""
    Write-Host "======================================" -ForegroundColor Cyan
    Write-Host "Test Results" -ForegroundColor Cyan
    Write-Host "======================================" -ForegroundColor Cyan
    Write-Host "Duration: $($Duration.TotalSeconds.ToString('F2')) seconds"
    Write-Host "Exit Code: $ExitCode"
    
    if (Test-Path "gui_component_tests.log") {
        Write-Host ""
        Write-Host "Test Log Summary:" -ForegroundColor Cyan
        Get-Content "gui_component_tests.log" | Select-String "PASS|FAIL|SUMMARY" | ForEach-Object {
            $line = $_.Line
            if ($line -match "PASS") {
                Write-Host $line -ForegroundColor Green
            } elseif ($line -match "FAIL") {
                Write-Host $line -ForegroundColor Red
            } else {
                Write-Host $line -ForegroundColor Cyan
            }
        }
        
        Write-Host ""
        Write-Host "Full log available at: gui_component_tests.log" -ForegroundColor Gray
    }
    
    Write-Host ""
    if ($ExitCode -eq 0) {
        Write-Host "✓ ALL TESTS PASSED" -ForegroundColor Green
    } else {
        Write-Host "✗ SOME TESTS FAILED" -ForegroundColor Red
    }
    
    exit $ExitCode
}
finally {
    Pop-Location
}
