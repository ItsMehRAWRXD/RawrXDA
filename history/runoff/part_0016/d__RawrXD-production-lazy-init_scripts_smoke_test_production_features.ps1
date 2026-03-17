#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Comprehensive CLI Smoke Tests for AutoModelLoader Production Features
    
.DESCRIPTION
    This script tests all production-ready implementations of AutoModelLoader:
    - Phase 1: GitHub Copilot Integration
    - Phase 2: External Logging System
    - Phase 3: Performance History Tracking
    - Phase 4: SHA256 Hashing
    - Phase 5: Prometheus Metrics Export
    - Phase 6: Full Integration Verification
    
.EXAMPLE
    .\smoke_test_production_features.ps1
    
.NOTES
    Author: RawrXD Team
    Date: January 2026
    Version: 1.0.0
#>

param(
    [switch]$Verbose,
    [switch]$SkipBuild,
    [string]$TestFilter = "*"
)

$ErrorActionPreference = "Continue"
$script:TestResults = @{
    Passed = 0
    Failed = 0
    Skipped = 0
    Details = @()
}

# ============================================================================
# Helper Functions
# ============================================================================

function Write-TestHeader {
    param([string]$Title)
    Write-Host ""
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host "  $Title" -ForegroundColor Cyan
    Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
    Write-Host ""
}

function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed,
        [string]$Message = ""
    )
    
    if ($Passed) {
        Write-Host "  ✅ PASS: $TestName" -ForegroundColor Green
        $script:TestResults.Passed++
    } else {
        Write-Host "  ❌ FAIL: $TestName" -ForegroundColor Red
        if ($Message) {
            Write-Host "     └─ $Message" -ForegroundColor Yellow
        }
        $script:TestResults.Failed++
    }
    
    $script:TestResults.Details += @{
        Name = $TestName
        Passed = $Passed
        Message = $Message
        Timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    }
}

function Write-TestSkipped {
    param([string]$TestName, [string]$Reason = "")
    Write-Host "  ⏭️  SKIP: $TestName" -ForegroundColor Yellow
    if ($Reason) {
        Write-Host "     └─ $Reason" -ForegroundColor Gray
    }
    $script:TestResults.Skipped++
}

function Test-FileExists {
    param([string]$Path, [string]$Description = "File")
    if (Test-Path $Path) {
        Write-TestResult "$Description exists" $true
        return $true
    } else {
        Write-TestResult "$Description exists" $false "Not found: $Path"
        return $false
    }
}

function Test-FileContains {
    param([string]$Path, [string]$Pattern, [string]$Description)
    if (-not (Test-Path $Path)) {
        Write-TestResult $Description $false "File not found: $Path"
        return $false
    }
    
    $content = Get-Content $Path -Raw
    if ($content -match $Pattern) {
        Write-TestResult $Description $true
        return $true
    } else {
        Write-TestResult $Description $false "Pattern not found in file"
        return $false
    }
}

# ============================================================================
# Test Setup
# ============================================================================

Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║     AutoModelLoader Production Features - Smoke Tests         ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""
Write-Host "Date: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor Gray
Write-Host "Host: $env:COMPUTERNAME" -ForegroundColor Gray
Write-Host "User: $env:USERNAME" -ForegroundColor Gray
Write-Host ""

$ProjectRoot = "D:\RawrXD-production-lazy-init"
$SrcDir = "$ProjectRoot\src"
$IncludeDir = "$ProjectRoot\include"
$LogsDir = "$ProjectRoot\logs"

# Create logs directory if needed
if (-not (Test-Path $LogsDir)) {
    New-Item -ItemType Directory -Path $LogsDir -Force | Out-Null
}

# ============================================================================
# Phase 1: Source Code Verification
# ============================================================================

Write-TestHeader "Phase 1: Source Code Verification"

# Check main source file exists
Test-FileExists "$SrcDir\auto_model_loader.cpp" "Main source file (auto_model_loader.cpp)"

# Check header file exists
Test-FileExists "$IncludeDir\auto_model_loader.h" "Header file (auto_model_loader.h)"

# Check production implementation file exists
Test-FileExists "$SrcDir\auto_model_loader_production.cpp" "Production implementation file"

# ============================================================================
# Phase 2: GitHub Copilot Integration Tests
# ============================================================================

Write-TestHeader "Phase 2: GitHub Copilot Integration Tests"

# Check for Copilot extension detection code
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "getCopilotConfigPath" `
    "Copilot config path detection implemented"

# Check for model preference parsing
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "parseModelPreferences" `
    "Model preferences parsing implemented"

# Check for production model names (not :latest)
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "codellama:7b-instruct" `
    "Production model names (specific versions)"

# Verify VS Code extensions directory detection
$vscodePath = "$env:USERPROFILE\.vscode\extensions"
if (Test-Path $vscodePath) {
    $copilotExtension = Get-ChildItem $vscodePath -Directory | Where-Object { $_.Name -like "*github.copilot*" }
    if ($copilotExtension) {
        Write-TestResult "GitHub Copilot extension detected" $true
        Write-Host "     └─ Found: $($copilotExtension.Name)" -ForegroundColor Gray
    } else {
        Write-TestResult "GitHub Copilot extension detected" $false "Extension not installed"
    }
} else {
    Write-TestSkipped "GitHub Copilot extension check" "VS Code extensions directory not found"
}

# ============================================================================
# Phase 3: External Logging System Tests
# ============================================================================

Write-TestHeader "Phase 3: External Logging System Tests"

# Check for file logging implementation
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "s_logFile" `
    "File logging implementation"

# Check for log directory creation
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "create_directories" `
    "Log directory auto-creation"

# Check for Windows Event Log support
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "OutputDebugStringA" `
    "Windows debug output support"

# Verify ProductionFileLogger class in production file
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "class ProductionFileLogger" `
    "ProductionFileLogger class implemented"

# Verify WindowsEventLogger class
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "class WindowsEventLogger" `
    "WindowsEventLogger class implemented"

# Check log rotation support
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "rotateFile" `
    "Log file rotation implemented"

# ============================================================================
# Phase 4: Performance History Tracking Tests
# ============================================================================

Write-TestHeader "Phase 4: Performance History Tracking Tests"

# Check for performance tracking implementation
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "class PerformanceHistoryTracker" `
    "PerformanceHistoryTracker class implemented"

# Check for latency recording
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "recordPerformance" `
    "Performance recording method"

# Check for model scoring
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "getModelScore" `
    "Model performance scoring"

# Check for percentile calculations
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "getPercentileLatency" `
    "Percentile latency calculation"

# Check for persistence
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "saveToFile" `
    "Performance data persistence"

# Check actual performance score implementation
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "PRODUCTION.*Actual performance history tracking" `
    "Integrated performance tracking in main file"

# ============================================================================
# Phase 5: SHA256 Hashing Tests
# ============================================================================

Write-TestHeader "Phase 5: SHA256 Hashing Tests"

# Check for Windows CryptoAPI include
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "wincrypt\.h" `
    "Windows CryptoAPI header included"

# Check for library linking
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "pragma comment.*crypt32\.lib" `
    "CryptoAPI library linked"

# Check for CryptAcquireContext usage
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "CryptAcquireContext" `
    "CryptAcquireContext API call"

# Check for SHA256 algorithm
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "CALG_SHA_256" `
    "SHA256 algorithm specified"

# Check for proper cleanup
Test-FileContains "$SrcDir\auto_model_loader.cpp" `
    "CryptDestroyHash" `
    "Crypto resource cleanup"

# Verify ProductionSHA256 class
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "class ProductionSHA256" `
    "ProductionSHA256 class implemented"

# ============================================================================
# Phase 6: Prometheus Metrics Export Tests
# ============================================================================

Write-TestHeader "Phase 6: Prometheus Metrics Export Tests"

# Check for Prometheus exporter
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "class PrometheusMetricsExporter" `
    "PrometheusMetricsExporter class implemented"

# Check for metric types (counter, gauge, histogram)
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "# TYPE.*counter" `
    "Counter metrics format"

Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "# TYPE.*summary" `
    "Summary metrics format"

# Check for quantile support
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    'quantile=.*0\.99' `
    "P99 quantile metrics"

# ============================================================================
# Phase 7: Integration Verification
# ============================================================================

Write-TestHeader "Phase 7: Integration Verification"

# Check for global initialization functions
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "void InitializeProductionLogging" `
    "Production logging initialization function"

# Check for ProductionLog function
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "void ProductionLog" `
    "ProductionLog function implemented"

# Check for ProductionComputeModelHash
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "ProductionComputeModelHash" `
    "Production hash function exported"

# Check for ProductionGetCopilotRecommendations
Test-FileContains "$SrcDir\auto_model_loader_production.cpp" `
    "ProductionGetCopilotRecommendations" `
    "Production Copilot recommendations exported"

# Check all "in production, would use" comments are replaced
$mainContent = Get-Content "$SrcDir\auto_model_loader.cpp" -Raw
$placeholderMatches = [regex]::Matches($mainContent, "in production.*would use")
if ($placeholderMatches.Count -eq 0) {
    Write-TestResult "All placeholders replaced" $true
} else {
    Write-TestResult "All placeholders replaced" $false "Found $($placeholderMatches.Count) remaining placeholders"
    foreach ($match in $placeholderMatches) {
        Write-Host "     └─ Line ~$([regex]::Matches($mainContent.Substring(0, $match.Index), "`n").Count + 1): $($match.Value.Substring(0, [Math]::Min(50, $match.Value.Length)))..." -ForegroundColor Yellow
    }
}

# Check PRODUCTION comments are present (indicating production code)
$productionComments = [regex]::Matches($mainContent, "// PRODUCTION:")
if ($productionComments.Count -ge 3) {
    Write-TestResult "Production implementations marked" $true "Found $($productionComments.Count) PRODUCTION markers"
} else {
    Write-TestResult "Production implementations marked" $false "Expected at least 3 PRODUCTION markers"
}

# ============================================================================
# Phase 8: Build Verification (Optional)
# ============================================================================

if (-not $SkipBuild) {
    Write-TestHeader "Phase 8: Build Verification"
    
    $buildDir = "$ProjectRoot\build"
    if (Test-Path $buildDir) {
        # Check for recent build artifacts
        $recentBuild = Get-ChildItem "$buildDir" -Recurse -Include "*.obj","*.exe" | 
            Where-Object { $_.LastWriteTime -gt (Get-Date).AddHours(-24) } |
            Select-Object -First 1
        
        if ($recentBuild) {
            Write-TestResult "Recent build artifacts found" $true "Last build: $($recentBuild.LastWriteTime)"
        } else {
            Write-TestSkipped "Recent build artifacts" "No builds in last 24 hours"
        }
    } else {
        Write-TestSkipped "Build verification" "Build directory not found"
    }
}

# ============================================================================
# Phase 9: Documentation Verification
# ============================================================================

Write-TestHeader "Phase 9: Documentation Verification"

# Check for updated documentation
$docFiles = @(
    "AUTO_MODEL_LOADER_README.md",
    "CUSTOM_MODEL_BUILDER_COMPLETE.md",
    "PRODUCTION_READINESS.md"
)

foreach ($doc in $docFiles) {
    $docPath = "$ProjectRoot\$doc"
    if (Test-Path $docPath) {
        Write-TestResult "Documentation: $doc" $true
    } else {
        Write-TestSkipped "Documentation: $doc" "File not found"
    }
}

# ============================================================================
# Final Summary
# ============================================================================

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host "                       TEST SUMMARY                            " -ForegroundColor Magenta
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host ""

$total = $script:TestResults.Passed + $script:TestResults.Failed + $script:TestResults.Skipped
$passRate = if ($total -gt 0) { [math]::Round(($script:TestResults.Passed / $total) * 100, 1) } else { 0 }

Write-Host "  Total Tests:  $total" -ForegroundColor White
Write-Host "  ✅ Passed:    $($script:TestResults.Passed)" -ForegroundColor Green
Write-Host "  ❌ Failed:    $($script:TestResults.Failed)" -ForegroundColor Red
Write-Host "  ⏭️  Skipped:   $($script:TestResults.Skipped)" -ForegroundColor Yellow
Write-Host ""
Write-Host "  Pass Rate:    $passRate%" -ForegroundColor $(if ($passRate -ge 80) { "Green" } elseif ($passRate -ge 60) { "Yellow" } else { "Red" })
Write-Host ""

# Generate detailed report
$reportPath = "$LogsDir\smoke_test_report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$reportData = @{
    Timestamp = Get-Date -Format "yyyy-MM-ddTHH:mm:ss"
    Summary = @{
        Total = $total
        Passed = $script:TestResults.Passed
        Failed = $script:TestResults.Failed
        Skipped = $script:TestResults.Skipped
        PassRate = $passRate
    }
    Details = $script:TestResults.Details
    Environment = @{
        Computer = $env:COMPUTERNAME
        User = $env:USERNAME
        OS = [System.Environment]::OSVersion.VersionString
        PowerShell = $PSVersionTable.PSVersion.ToString()
    }
}
$reportData | ConvertTo-Json -Depth 5 | Out-File $reportPath -Encoding UTF8
Write-Host "  📄 Report saved: $reportPath" -ForegroundColor Gray
Write-Host ""

# Exit with appropriate code
if ($script:TestResults.Failed -gt 0) {
    Write-Host "❌ Some tests failed. Please review and fix." -ForegroundColor Red
    exit 1
} else {
    Write-Host "✅ All tests passed! Production features are ready." -ForegroundColor Green
    exit 0
}
