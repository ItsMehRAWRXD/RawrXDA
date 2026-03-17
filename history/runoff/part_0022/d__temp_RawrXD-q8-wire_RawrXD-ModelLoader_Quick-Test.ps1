#!/usr/bin/env pwsh
# Quick Test Runner - No CMake required
# Runs test harness directly after building

$ErrorActionPreference = "Stop"

Write-Host "======================================" -ForegroundColor Cyan
Write-Host "RawrXD Quick Test Runner"  -ForegroundColor Cyan
Write-Host "======================================" -ForegroundColor Cyan
Write-Host ""

# Build main executable first
Write-Host "[1/2] Building RawrXD-QtShell..." -ForegroundColor Yellow
cd d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader
cmake --build build --config Release --target RawrXD-QtShell 2>&1 | Out-Null

if ($LASTEXITCODE -ne 0) {
    Write-Host "✗ Build failed" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Build successful" -ForegroundColor Green

# Run comprehensive validation
Write-Host "[2/2] Running validation..." -ForegroundColor Yellow
Write-Host ""

.\Run-Full-Validation.ps1 -Verbose:$Verbose

exit $LASTEXITCODE
