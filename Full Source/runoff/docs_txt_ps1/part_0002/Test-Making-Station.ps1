#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Test the Model/Agent Making Station

.DESCRIPTION
    Validates that all Making Station components are properly configured
#>

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║         Testing Model/Agent Making Station                       ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$root = "D:\lazy init ide"
$passed = 0
$failed = 0

# Test 1: Check main script exists
Write-Host "Test 1: Main Making Station script..." -NoNewline -ForegroundColor Yellow
$mainScript = Join-Path $root "scripts\model_agent_making_station.ps1"
if (Test-Path $mainScript) {
    Write-Host " ✓ PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " ✗ FAIL" -ForegroundColor Red
    $failed++
}

# Test 2: Check launcher exists
Write-Host "Test 2: Launcher script..." -NoNewline -ForegroundColor Yellow
$launcher = Join-Path $root "Launch-Making-Station.ps1"
if (Test-Path $launcher) {
    Write-Host " ✓ PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " ✗ FAIL" -ForegroundColor Red
    $failed++
}

# Test 3: Check config directory
Write-Host "Test 3: Configuration directory..." -NoNewline -ForegroundColor Yellow
$configDir = Join-Path $root "logs\swarm_config\making_station"
if (Test-Path $configDir) {
    Write-Host " ✓ PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " ✗ FAIL (will be created on first run)" -ForegroundColor Yellow
    $failed++
}

# Test 4: Check model_sources.ps1
Write-Host "Test 4: Model sources module..." -NoNewline -ForegroundColor Yellow
$modelSources = Join-Path $root "scripts\model_sources.ps1"
if (Test-Path $modelSources) {
    Write-Host " ✓ PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " ✗ FAIL" -ForegroundColor Red
    $failed++
}

# Test 5: Check swarm_control_center.ps1
Write-Host "Test 5: Swarm Control Center..." -NoNewline -ForegroundColor Yellow
$swarmControl = Join-Path $root "scripts\swarm_control_center.ps1"
if (Test-Path $swarmControl) {
    Write-Host " ✓ PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " ✗ FAIL" -ForegroundColor Red
    $failed++
}

# Test 6: Validate script syntax
Write-Host "Test 6: Script syntax validation..." -NoNewline -ForegroundColor Yellow
try {
    $null = [System.Management.Automation.PSParser]::Tokenize((Get-Content $mainScript -Raw), [ref]$null)
    Write-Host " ✓ PASS" -ForegroundColor Green
    $passed++
} catch {
    Write-Host " ✗ FAIL" -ForegroundColor Red
    $failed++
}

# Test 7: Check README
Write-Host "Test 7: Documentation..." -NoNewline -ForegroundColor Yellow
$readme = Join-Path $root "MAKING_STATION_README.md"
if (Test-Path $readme) {
    Write-Host " ✓ PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " ✗ FAIL" -ForegroundColor Red
    $failed++
}

# Test 8: Check quick reference
Write-Host "Test 8: Quick reference card..." -NoNewline -ForegroundColor Yellow
$quickRef = Join-Path $root "MAKING_STATION_QUICK_REFERENCE.txt"
if (Test-Path $quickRef) {
    Write-Host " ✓ PASS" -ForegroundColor Green
    $passed++
} else {
    Write-Host " ✗ FAIL" -ForegroundColor Red
    $failed++
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Results: $passed passed, $failed failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })
Write-Host "═══════════════════════════════════════════════════════════════════" -ForegroundColor Cyan

if ($failed -eq 0) {
    Write-Host ""
    Write-Host "✓ All tests passed! Making Station is ready." -ForegroundColor Green
    Write-Host ""
    Write-Host "To launch:" -ForegroundColor Yellow
    Write-Host "  .\Launch-Making-Station.ps1" -ForegroundColor White
    Write-Host ""
} else {
    Write-Host ""
    Write-Host "⚠ Some tests failed. Please check the errors above." -ForegroundColor Yellow
    Write-Host ""
}
