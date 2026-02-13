<#
.SYNOPSIS
    Quick verification test for RawrXD agentic module integration
.DESCRIPTION
    Tests that the agentic module loads properly and all core functions are available.
.EXAMPLE
    .\Test-RawrXD-Agentic-Integration.ps1
#>

Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║      RawrXD Agentic Module Integration Test Suite          ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$testResults = @()
$testsPassed = 0
$testsFailed = 0

# Test 1: Module file exists
Write-Host "[TEST 1] Checking if RawrXD-Agentic-Module.psm1 exists..." -ForegroundColor Yellow
$modulePath = Join-Path $PSScriptRoot "RawrXD-Agentic-Module.psm1"
if (Test-Path $modulePath) {
    Write-Host "✅ PASS: Module file found at $modulePath" -ForegroundColor Green
    $testsPassed++
    $testResults += "Module File: PASS"
}
else {
    Write-Host "❌ FAIL: Module file not found at $modulePath" -ForegroundColor Red
    $testsFailed++
    $testResults += "Module File: FAIL"
}
Write-Host ""

# Test 2: Module imports successfully
Write-Host "[TEST 2] Attempting to import module..." -ForegroundColor Yellow
try {
    Import-Module $modulePath -Force -ErrorAction Stop
    Write-Host "✅ PASS: Module imported successfully" -ForegroundColor Green
    $testsPassed++
    $testResults += "Module Import: PASS"
}
catch {
    Write-Host "❌ FAIL: Module import failed: $_" -ForegroundColor Red
    $testsFailed++
    $testResults += "Module Import: FAIL ($_)"
}
Write-Host ""

# Test 3: Check if Ollama is running
Write-Host "[TEST 3] Checking Ollama connectivity..." -ForegroundColor Yellow
try {
    $ollamaResponse = Invoke-WebRequest -Uri "http://localhost:11434/api/tags" -ErrorAction Stop
    Write-Host "✅ PASS: Ollama is running and accessible" -ForegroundColor Green
    $testsPassed++
    $testResults += "Ollama Connection: PASS"
    
    # Check if agentic model is available
    $models = $ollamaResponse.Content | ConvertFrom-Json
    $agenticModel = $models.models | Where-Object { $_.name -like "*cheetah-stealth*" }
    if ($agenticModel) {
        Write-Host "   Found agentic model: $($agenticModel.name)" -ForegroundColor Cyan
    }
    else {
        Write-Host "   ⚠️  Agentic model not found. Available models:" -ForegroundColor Yellow
        $models.models | ForEach-Object { Write-Host "      - $($_.name)" -ForegroundColor Gray }
    }
}
catch {
    Write-Host "❌ FAIL: Cannot connect to Ollama at http://localhost:11434" -ForegroundColor Red
    Write-Host "   Make sure Ollama is running: ollama serve" -ForegroundColor Gray
    $testsFailed++
    $testResults += "Ollama Connection: FAIL (offline)"
}
Write-Host ""

# Test 4: Check if Enable-RawrXDAgentic function exists
Write-Host "[TEST 4] Checking if Enable-RawrXDAgentic function exists..." -ForegroundColor Yellow
if (Get-Command "Enable-RawrXDAgentic" -ErrorAction SilentlyContinue) {
    Write-Host "✅ PASS: Enable-RawrXDAgentic function found" -ForegroundColor Green
    $testsPassed++
    $testResults += "Enable-RawrXDAgentic: PASS"
}
else {
    Write-Host "❌ FAIL: Enable-RawrXDAgentic function not found" -ForegroundColor Red
    $testsFailed++
    $testResults += "Enable-RawrXDAgentic: FAIL"
}
Write-Host ""

# Test 5: Check if Invoke-RawrXDAgenticCodeGen function exists
Write-Host "[TEST 5] Checking if Invoke-RawrXDAgenticCodeGen function exists..." -ForegroundColor Yellow
if (Get-Command "Invoke-RawrXDAgenticCodeGen" -ErrorAction SilentlyContinue) {
    Write-Host "✅ PASS: Invoke-RawrXDAgenticCodeGen function found" -ForegroundColor Green
    $testsPassed++
    $testResults += "Invoke-RawrXDAgenticCodeGen: PASS"
}
else {
    Write-Host "❌ FAIL: Invoke-RawrXDAgenticCodeGen function not found" -ForegroundColor Red
    $testsFailed++
    $testResults += "Invoke-RawrXDAgenticCodeGen: FAIL"
}
Write-Host ""

# Test 6: Check if Get-RawrXDAgenticStatus function exists
Write-Host "[TEST 6] Checking if Get-RawrXDAgenticStatus function exists..." -ForegroundColor Yellow
if (Get-Command "Get-RawrXDAgenticStatus" -ErrorAction SilentlyContinue) {
    Write-Host "✅ PASS: Get-RawrXDAgenticStatus function found" -ForegroundColor Green
    $testsPassed++
    $testResults += "Get-RawrXDAgenticStatus: PASS"
}
else {
    Write-Host "❌ FAIL: Get-RawrXDAgenticStatus function not found" -ForegroundColor Red
    $testsFailed++
    $testResults += "Get-RawrXDAgenticStatus: FAIL"
}
Write-Host ""

# Test 7: Try to enable agentic mode
Write-Host "[TEST 7] Attempting to enable agentic mode..." -ForegroundColor Yellow
try {
    Enable-RawrXDAgentic -Model "cheetah-stealth-agentic:latest" -Temperature 0.9 -ErrorAction Stop
    Write-Host "✅ PASS: Agentic mode enabled successfully" -ForegroundColor Green
    $testsPassed++
    $testResults += "Enable Agentic Mode: PASS"
}
catch {
    Write-Host "❌ FAIL: Could not enable agentic mode: $_" -ForegroundColor Red
    $testsFailed++
    $testResults += "Enable Agentic Mode: FAIL ($_)"
}
Write-Host ""

# Test 8: Check current status
Write-Host "[TEST 8] Checking current agentic status..." -ForegroundColor Yellow
try {
    $status = Get-RawrXDAgenticStatus -ErrorAction Stop
    Write-Host "✅ PASS: Status retrieved successfully" -ForegroundColor Green
    Write-Host "   Status Output:" -ForegroundColor Cyan
    $status | Write-Host -ForegroundColor Gray
    $testsPassed++
    $testResults += "Get Status: PASS"
}
catch {
    Write-Host "❌ FAIL: Could not retrieve status: $_" -ForegroundColor Red
    $testsFailed++
    $testResults += "Get Status: FAIL"
}
Write-Host ""

# Summary
Write-Host "╔════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                    TEST RESULTS SUMMARY                    ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""
Write-Host "Tests Passed: $testsPassed" -ForegroundColor Green
Write-Host "Tests Failed: $testsFailed" -ForegroundColor $(if ($testsFailed -gt 0) { "Red" } else { "Green" })
Write-Host ""

foreach ($result in $testResults) {
    if ($result -match "PASS") {
        Write-Host "✅ $result" -ForegroundColor Green
    }
    else {
        Write-Host "❌ $result" -ForegroundColor Red
    }
}
Write-Host ""

if ($testsFailed -eq 0) {
    Write-Host "🎉 ALL TESTS PASSED! RawrXD agentic integration is working correctly!" -ForegroundColor Green
    exit 0
}
else {
    Write-Host "⚠️  SOME TESTS FAILED. Check the output above for details." -ForegroundColor Yellow
    exit 1
}
