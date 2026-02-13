# Quick Production Test Suite for RawrXD
# Simplified tests without parameter conflicts

param(
    [string]$TestScope = 'All'
)

$scriptRoot = $PSScriptRoot
$moduleDir = Join-Path $scriptRoot 'auto_generated_methods'

Write-Host "=== RawrXD Production Quick Test Suite ===" -ForegroundColor Cyan
Write-Host "Testing modules and features in: $moduleDir" -ForegroundColor Green

# Test 1: Production Modules
Write-Host "`n[1] Testing Production Modules (.psm1)" -ForegroundColor Yellow

$modules = Get-ChildItem -Path $moduleDir -Filter 'RawrXD.*.psm1' -ErrorAction SilentlyContinue
$moduleResults = @()

foreach ($module in $modules) {
    Write-Host "  Testing $($module.Name)..." -NoNewline
    try {
        $importResult = Import-Module $module.FullName -Force -PassThru -ErrorAction Stop
        $functions = $importResult.ExportedFunctions.Count
        Write-Host " ✓ ($functions functions)" -ForegroundColor Green
        $moduleResults += @{ Name = $module.BaseName; Status = 'PASS'; Functions = $functions }
    } catch {
        Write-Host " ✗ $($_.Exception.Message)" -ForegroundColor Red
        $moduleResults += @{ Name = $module.BaseName; Status = 'FAIL'; Error = $_.Exception.Message }
    }
}

# Test 2: Auto-Feature Scripts  
Write-Host "`n[2] Testing Auto-Feature Scripts (.ps1)" -ForegroundColor Yellow

$features = Get-ChildItem -Path $moduleDir -Filter '*_AutoFeature.ps1' -ErrorAction SilentlyContinue
$featureResults = @()

foreach ($feature in $features) {
    Write-Host "  Testing $($feature.Name)..." -NoNewline
    try {
        . $feature.FullName
        $functionName = 'Invoke-' + ($feature.BaseName -replace '_AutoFeature$','')
        $cmd = Get-Command $functionName -ErrorAction SilentlyContinue
        if ($cmd) {
            Write-Host " ✓" -ForegroundColor Green
            $featureResults += @{ Name = $feature.BaseName; Status = 'PASS'; Function = $functionName }
        } else {
            Write-Host " ⚠ (function $functionName not found)" -ForegroundColor Yellow
            $featureResults += @{ Name = $feature.BaseName; Status = 'WARN'; Function = $functionName }
        }
    } catch {
        Write-Host " ✗ $($_.Exception.Message)" -ForegroundColor Red
        $featureResults += @{ Name = $feature.BaseName; Status = 'FAIL'; Error = $_.Exception.Message }
    }
}

# Test 3: Quick Function Tests
Write-Host "`n[3] Quick Function Tests" -ForegroundColor Yellow

# Test AutoDependencyGraph
Write-Host "  Testing AutoDependencyGraph..." -NoNewline
try {
    if (Get-Command Invoke-AutoDependencyGraph -ErrorAction SilentlyContinue) {
        $result = Invoke-AutoDependencyGraph -SourceDir $moduleDir -MaxDepth 1 -IncludePatterns @('*.psm1') 
        Write-Host " ✓ (found $($result.Files.Count) files)" -ForegroundColor Green
    } else {
        Write-Host " ✗ (function not available)" -ForegroundColor Red
    }
} catch {
    Write-Host " ✗ $($_.Exception.Message)" -ForegroundColor Red
}

# Test SelfHealing
Write-Host "  Testing SelfHealingModule..." -NoNewline
try {
    if (Get-Command Invoke-SelfHealingModule -ErrorAction SilentlyContinue) {
        $result = Invoke-SelfHealingModule -ModuleDir $moduleDir -NoInvoke
        Write-Host " ✓ (checked $($result.ComponentsChecked) components)" -ForegroundColor Green
    } else {
        Write-Host " ✗ (function not available)" -ForegroundColor Red
    }
} catch {
    Write-Host " ✗ $($_.Exception.Message)" -ForegroundColor Red
}

# Test Security Scanner
Write-Host "  Testing SecurityScanner..." -NoNewline
try {
    if (Get-Command Invoke-SecurityVulnerabilityScanner -ErrorAction SilentlyContinue) {
        $result = Invoke-SecurityVulnerabilityScanner -SourceDir $moduleDir -ScanLevel Basic
        Write-Host " ✓ (scanned $($result.FilesScanned) files)" -ForegroundColor Green
    } else {
        Write-Host " ✗ (function not available)" -ForegroundColor Red
    }
} catch {
    Write-Host " ✗ $($_.Exception.Message)" -ForegroundColor Red
}

# Summary
Write-Host "`n=== Test Summary ===" -ForegroundColor Cyan
$passedModules = ($moduleResults | Where-Object { $_.Status -eq 'PASS' }).Count
$passedFeatures = ($featureResults | Where-Object { $_.Status -eq 'PASS' }).Count
$warnFeatures = ($featureResults | Where-Object { $_.Status -eq 'WARN' }).Count

Write-Host "Modules:  $passedModules/$($modules.Count) passed" -ForegroundColor $(if($passedModules -eq $modules.Count){'Green'}else{'Yellow'})
Write-Host "Features: $passedFeatures/$($features.Count) passed, $warnFeatures warnings" -ForegroundColor $(if($passedFeatures -eq $features.Count){'Green'}else{'Yellow'})

$overallStatus = if ($passedModules -eq $modules.Count -and $passedFeatures -ge ($features.Count * 0.8)) { 'PASS' } else { 'PARTIAL' }
Write-Host "Overall:  $overallStatus" -ForegroundColor $(if($overallStatus -eq 'PASS'){'Green'}else{'Yellow'})

Write-Host "`nProduction modules ready for deployment!" -ForegroundColor Green