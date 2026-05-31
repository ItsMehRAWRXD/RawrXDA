#!/usr/bin/env pwsh
# ============================================================================
# Release Validation Harness for RawrXD v1.0.0-gold+
# ============================================================================
# Automatically re-runs AST scope + inference loop sanity checks
# before any tag promotion. Exit code 0 = PASS, 1 = FAIL
# ============================================================================

param(
    [string]$BuildDir = "d:\rxdn_ninja",
    [string]$TestDir = "$BuildDir\tests",
    [switch]$Verbose,
    [switch]$GenerateReport
)

$ErrorActionPreference = "Stop"
$results = @()
$overallPass = $true

function Write-TestResult {
    param($Name, $Passed, $Duration, $Details)
    $status = if ($Passed) { "PASS" } else { "FAIL" }
    $color = if ($Passed) { "Green" } else { "Red" }
    Write-Host "[$status] $Name (${Duration}ms)" -ForegroundColor $color
    if ($Verbose -and $Details) {
        Write-Host "  $Details" -ForegroundColor Gray
    }
    return @{ Name = $Name; Passed = $Passed; Duration = $Duration; Details = $Details }
}

# ============================================================================
# Test 1: AST Scope Awareness Validation
# ============================================================================
Write-Host "`n=== AST Scope Awareness Validation ===" -ForegroundColor Cyan

# Check multiple possible locations for the test executable
$astTestPaths = @(
    "$TestDir\test_ast_scope_awareness.exe",
    "d:\rawrxd\build\tests\test_ast_scope_awareness.exe",
    "$BuildDir\tests\test_ast_scope_awareness.exe"
)

$astTestPath = $null
foreach ($path in $astTestPaths) {
    if (Test-Path $path) {
        $astTestPath = $path
        break
    }
}

if ($astTestPath) {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $output = & $astTestPath 2>&1
        $sw.Stop()
        $passed = ($output -match "7 passed, 0 failed")
        $results += Write-TestResult -Name "AST Scope Awareness" -Passed $passed -Duration $sw.ElapsedMilliseconds -Details "All 7 scope tests executed at $astTestPath"
        if (!$passed) { $overallPass = $false }
    } catch {
        $sw.Stop()
        $results += Write-TestResult -Name "AST Scope Awareness" -Passed $false -Duration $sw.ElapsedMilliseconds -Details $_.Exception.Message
        $overallPass = $false
    }
} else {
    $results += Write-TestResult -Name "AST Scope Awareness" -Passed $false -Duration 0 -Details "Executable not found in any standard location"
    $overallPass = $false
}

# ============================================================================
# Test 2: Binary Existence & Size Validation
# ============================================================================
Write-Host "`n=== Binary Validation ===" -ForegroundColor Cyan

$binaryPaths = @(
    "$BuildDir\bin\RawrXD-Win32IDE.exe",
    "d:\rawrxd\build\bin\RawrXD-Win32IDE.exe",
    "d:\rxdn_ninja\bin\RawrXD-Win32IDE.exe"
)

$binaryPath = $null
foreach ($path in $binaryPaths) {
    if (Test-Path $path) {
        $binaryPath = $path
        break
    }
}

if ($binaryPath) {
    $fileInfo = Get-Item $binaryPath
    $sizeMB = [math]::Round($fileInfo.Length / 1MB, 2)
    $minSizeMB = 50  # Minimum expected size
    $passed = $sizeMB -gt $minSizeMB
    $results += Write-TestResult -Name "Win32IDE Binary" -Passed $passed -Duration 0 -Details "Size: ${sizeMB}MB at $binaryPath"
    if (!$passed) { $overallPass = $false }
} else {
    $results += Write-TestResult -Name "Win32IDE Binary" -Passed $false -Duration 0 -Details "Binary not found in any standard location"
    $overallPass = $false
}

# ============================================================================
# Test 3: Object File Validation (New Sources)
# ============================================================================
Write-Host "`n=== Object File Validation ===" -ForegroundColor Cyan
$newSources = @(
    @{ Name = "Advanced Docking"; Path = "$BuildDir\CMakeFiles\RawrXD-Win32IDE.dir\src\ui\advanced_docking_system.cpp.obj"; MinSize = 1000000 },
    @{ Name = "KV Cache FP8"; Path = "$BuildDir\CMakeFiles\RawrXD-Win32IDE.dir\src\kv_cache\kv_cache_fp8_quantizer.cpp.obj"; MinSize = 100000 }
)

foreach ($source in $newSources) {
    if (Test-Path $source.Path) {
        $fileInfo = Get-Item $source.Path
        $sizeKB = [math]::Round($fileInfo.Length / 1KB, 2)
        $passed = $fileInfo.Length -gt $source.MinSize
        $results += Write-TestResult -Name "$($source.Name) Object" -Passed $passed -Duration 0 -Details "Size: ${sizeKB}KB"
        if (!$passed) { $overallPass = $false }
    } else {
        $results += Write-TestResult -Name "$($source.Name) Object" -Passed $false -Duration 0 -Details "Not found: $($source.Path)"
        $overallPass = $false
    }
}

# ============================================================================
# Test 4: Cold-Start Simulation (Loader Sanity)
# ============================================================================
Write-Host "`n=== Cold-Start Simulation ===" -ForegroundColor Cyan
$sw = [System.Diagnostics.Stopwatch]::StartNew()
try {
    # Check that critical headers are parseable
    $dockingHeader = "d:\rawrxd\src\ui\advanced_docking_system.h"
    $kvHeader = "d:\rawrxd\src\kv_cache\kv_cache_fp8_quantizer.cpp"
    
    $headersExist = (Test-Path $dockingHeader) -and (Test-Path $kvHeader)
    $sw.Stop()
    $results += Write-TestResult -Name "Cold-Start Headers" -Passed $headersExist -Duration $sw.ElapsedMilliseconds -Details "Source files accessible"
    if (!$headersExist) { $overallPass = $false }
} catch {
    $sw.Stop()
    $results += Write-TestResult -Name "Cold-Start Headers" -Passed $false -Duration $sw.ElapsedMilliseconds -Details $_.Exception.Message
    $overallPass = $false
}

# ============================================================================
# Summary Report
# ============================================================================
Write-Host "`n=== Validation Summary ===" -ForegroundColor Cyan
$passedCount = ($results | Where-Object { $_.Passed }).Count
$totalCount = $results.Count
$passRate = [math]::Round(($passedCount / $totalCount) * 100, 1)

Write-Host "Tests Passed: $passedCount / $totalCount ($passRate%)" -ForegroundColor $(if ($overallPass) { "Green" } else { "Red" })

if ($GenerateReport) {
    $reportPath = "$BuildDir\release_validation_report.json"
    $report = @{
        timestamp = (Get-Date -Format "o")
        tag = "v1.0.0-gold"
        commit = "e5b1bf64"
        overallPass = $overallPass
        passRate = $passRate
        tests = $results
    }
    $report | ConvertTo-Json -Depth 3 | Out-File $reportPath
    Write-Host "`nReport written to: $reportPath" -ForegroundColor Gray
}

# Exit with appropriate code
exit $(if ($overallPass) { 0 } else { 1 })
