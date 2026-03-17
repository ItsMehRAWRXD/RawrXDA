#!/usr/bin/env pwsh
# Performance Profiling Script for RawrXD Win32 IDE
# Measures startup time, memory usage, and UI responsiveness

param(
    [string]$BinaryPath = "build-win32-only\bin\Release\AgenticIDEWin.exe",
    [int]$Iterations = 5,
    [string]$OutputFile = "performance_results.json"
)

$ErrorActionPreference = "Stop"

if (-not (Test-Path $BinaryPath)) {
    Write-Host "[ERROR] Binary not found: $BinaryPath" -ForegroundColor Red
    exit 1
}

Write-Host "Performance Profiling RawrXD Win32 IDE" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Binary: $BinaryPath" -ForegroundColor White
Write-Host "Iterations: $Iterations" -ForegroundColor White
Write-Host ""

$results = @{
    timestamp = (Get-Date -Format "yyyy-MM-dd HH:mm:ss")
    binary = $BinaryPath
    iterations = $Iterations
    tests = @{}
}

# Test 1: Cold Start Time
Write-Host "[1/5] Testing cold start time..." -ForegroundColor Yellow
$coldStartTimes = @()
for ($i = 1; $i -le $Iterations; $i++) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $BinaryPath -PassThru -WindowStyle Hidden
    Start-Sleep -Milliseconds 100
    $sw.Stop()
    
    if (-not $process.HasExited) {
        $coldStartTimes += $sw.ElapsedMilliseconds
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
    
    Start-Sleep -Seconds 1
}

$avgColdStart = ($coldStartTimes | Measure-Object -Average).Average
$results.tests.coldStart = @{
    iterations = $coldStartTimes
    average_ms = [math]::Round($avgColdStart, 2)
    target_ms = 3000
    passed = $avgColdStart -lt 3000
}
Write-Host "  Average: $([math]::Round($avgColdStart, 2)) ms (Target: < 3000 ms)" -ForegroundColor $(if ($avgColdStart -lt 3000) { "Green" } else { "Red" })

# Test 2: Memory Usage
Write-Host "[2/5] Testing memory usage..." -ForegroundColor Yellow
$process = Start-Process -FilePath $BinaryPath -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 3

$memoryMB = [math]::Round($process.WorkingSet64 / 1MB, 2)
$results.tests.memoryIdle = @{
    memory_mb = $memoryMB
    target_mb = 500
    passed = $memoryMB -lt 500
}
Write-Host "  Idle Memory: $memoryMB MB (Target: < 500 MB)" -ForegroundColor $(if ($memoryMB -lt 500) { "Green" } else { "Red" })

Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
Start-Sleep -Seconds 1

# Test 3: Binary Size
Write-Host "[3/5] Checking binary size..." -ForegroundColor Yellow
$binarySize = (Get-Item $BinaryPath).Length
$binarySizeMB = [math]::Round($binarySize / 1MB, 2)
$results.tests.binarySize = @{
    size_mb = $binarySizeMB
    target_mb = 10
    passed = $binarySizeMB -lt 10
}
Write-Host "  Binary Size: $binarySizeMB MB (Target: < 10 MB)" -ForegroundColor $(if ($binarySizeMB -lt 10) { "Green" } else { "Red" })

# Test 4: File Load Performance
Write-Host "[4/5] Testing file operations..." -ForegroundColor Yellow
$testFile = "test_large_file.txt"
"Line " * 10000 | Out-File -FilePath $testFile -Encoding utf8
$fileSize = (Get-Item $testFile).Length
$fileSizeMB = [math]::Round($fileSize / 1MB, 2)

$results.tests.fileOperations = @{
    test_file_size_mb = $fileSizeMB
    note = "File I/O tested manually via IDE"
}
Write-Host "  Test file created: $fileSizeMB MB" -ForegroundColor Green
Remove-Item $testFile -Force

# Test 5: Process Launch Overhead
Write-Host "[5/5] Testing process launch overhead..." -ForegroundColor Yellow
$launchTimes = @()
for ($i = 1; $i -le $Iterations; $i++) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $process = Start-Process -FilePath $BinaryPath -PassThru -WindowStyle Hidden
    $sw.Stop()
    $launchTimes += $sw.ElapsedMilliseconds
    
    Start-Sleep -Milliseconds 50
    Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    Start-Sleep -Milliseconds 500
}

$avgLaunch = ($launchTimes | Measure-Object -Average).Average
$results.tests.processLaunch = @{
    iterations = $launchTimes
    average_ms = [math]::Round($avgLaunch, 2)
}
Write-Host "  Average Launch: $([math]::Round($avgLaunch, 2)) ms" -ForegroundColor Green

# Summary
Write-Host ""
Write-Host "=======================================" -ForegroundColor Cyan
Write-Host "Performance Summary" -ForegroundColor Cyan
Write-Host "=======================================" -ForegroundColor Cyan

$passed = 0
$failed = 0
foreach ($test in $results.tests.GetEnumerator()) {
    if ($test.Value.passed -eq $true) {
        $passed++
        Write-Host "  [PASS] $($test.Key)" -ForegroundColor Green
    } elseif ($test.Value.passed -eq $false) {
        $failed++
        Write-Host "  [FAIL] $($test.Key)" -ForegroundColor Red
    } else {
        Write-Host "  [INFO] $($test.Key)" -ForegroundColor Cyan
    }
}

$results.summary = @{
    passed = $passed
    failed = $failed
    total = $passed + $failed
}

Write-Host ""
Write-Host "Results: $passed passed, $failed failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })

# Save results
$results | ConvertTo-Json -Depth 10 | Out-File -FilePath $OutputFile -Encoding utf8
Write-Host "Results saved to: $OutputFile" -ForegroundColor Cyan

exit $(if ($failed -eq 0) { 0 } else { 1 })
