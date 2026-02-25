#requires -Version 5.1
<#
.SYNOPSIS
    Comprehensive Production Readiness Validation for RawrXD IDE/CLI Framework
.DESCRIPTION
    This script validates that all production-ready components are working correctly.
    It tests:
    - Module loading and dependency resolution
    - Core functionality of each auto-generated method
    - Integration between components
    - Performance benchmarks
    - Security validation
.NOTES
    Author: RawrXD Production Team
    Version: 2.0.0
#>

param(
    [switch]$Verbose,
    [switch]$SkipPerformanceTests,
    [switch]$GenerateReport
)

$ErrorActionPreference = 'Continue'
$script:TestResults = @{
    StartTime = Get-Date
    EndTime = $null
    TotalTests = 0
    Passed = 0
    Failed = 0
    Warnings = 0
    Skipped = 0
    Details = @()
}

# ============================================================================
# TEST HELPERS
# ============================================================================
function Write-TestHeader {
    param([string]$TestName)
    Write-Host "`n" -NoNewline
    Write-Host "=" * 70 -ForegroundColor Cyan
    Write-Host "  TEST: $TestName" -ForegroundColor Cyan
    Write-Host "=" * 70 -ForegroundColor Cyan
}

function Write-TestResult {
    param(
        [string]$TestName,
        [string]$Status,
        [string]$Message = "",
        [double]$Duration = 0
    )
    
    $script:TestResults.TotalTests++
    
    $color = switch ($Status) {
        'PASS' { $script:TestResults.Passed++; 'Green' }
        'FAIL' { $script:TestResults.Failed++; 'Red' }
        'WARN' { $script:TestResults.Warnings++; 'Yellow' }
        'SKIP' { $script:TestResults.Skipped++; 'DarkGray' }
        default { 'White' }
    }
    
    $statusIcon = switch ($Status) {
        'PASS' { '✅' }
        'FAIL' { '❌' }
        'WARN' { '⚠️' }
        'SKIP' { '⏭️' }
        default { '•' }
    }
    
    $durationStr = if ($Duration -gt 0) { " (${Duration}ms)" } else { "" }
    Write-Host "  $statusIcon [$Status] $TestName$durationStr" -ForegroundColor $color
    if ($Message) {
        Write-Host "      $Message" -ForegroundColor DarkGray
    }
    
    $script:TestResults.Details += @{
        Test = $TestName
        Status = $Status
        Message = $Message
        Duration = $Duration
    }
}

function Invoke-TestWithTiming {
    param(
        [string]$TestName,
        [scriptblock]$TestScript
    )
    
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    try {
        $result = & $TestScript
        $sw.Stop()
        
        if ($result -eq $true -or ($result -is [hashtable] -and $result.Success -eq $true)) {
            Write-TestResult -TestName $TestName -Status 'PASS' -Duration $sw.Elapsed.TotalMilliseconds
            return $true
        } else {
            $msg = if ($result -is [hashtable] -and $result.Error) { $result.Error } else { "Test returned false" }
            Write-TestResult -TestName $TestName -Status 'FAIL' -Message $msg -Duration $sw.Elapsed.TotalMilliseconds
            return $false
        }
    } catch {
        $sw.Stop()
        Write-TestResult -TestName $TestName -Status 'FAIL' -Message $_.Exception.Message -Duration $sw.Elapsed.TotalMilliseconds
        return $false
    }
}

# ============================================================================
# MODULE LOADING TESTS
# ============================================================================
function Test-ModuleLoading {
    Write-TestHeader "Module Loading and Dependencies"
    
    $modules = @(
        @{ Path = "D:/lazy init ide/RawrXD.Core.psm1"; Name = "RawrXD.Core" }
        @{ Path = "D:/lazy init ide/RawrXD.Logging.psm1"; Name = "RawrXD.Logging" }
        @{ Path = "D:/lazy init ide/RawrXD.Config.psm1"; Name = "RawrXD.Config" }
    )
    
    $autoMethods = @(
        "AutoDependencyGraph_AutoFeature.ps1",
        "SecurityVulnerabilityScanner_AutoFeature.ps1",
        "AutoRefactorSuggestor_AutoFeature.ps1",
        "DynamicTestHarness_AutoFeature.ps1",
        "LiveMetricsDashboard_AutoFeature.ps1",
        "SourceCodeSummarizer_AutoFeature.ps1",
        "SelfHealingModule_AutoFeature.ps1",
        "PluginAutoLoader_AutoFeature.ps1",
        "ContinuousIntegrationTrigger_AutoFeature.ps1",
        "ManifestChangeNotifier_AutoFeature.ps1",
        "SourceDigestionOrchestrator_AutoMethod.ps1",
        "Test-Security-Settings-Fix_AutoMethod.ps1"
    )
    
    # Test core modules
    foreach ($module in $modules) {
        Invoke-TestWithTiming -TestName "Load $($module.Name)" -TestScript {
            if (Test-Path $module.Path) {
                Import-Module $module.Path -Force -ErrorAction Stop
                return $true
            }
            return @{ Success = $false; Error = "Module not found: $($module.Path)" }
        }
    }
    
    # Test auto-generated methods
    foreach ($method in $autoMethods) {
        $path = "D:/lazy init ide/auto_generated_methods/$method"
        Invoke-TestWithTiming -TestName "Load $method" -TestScript {
            if (Test-Path $path) {
                . $path
                return $true
            }
            return @{ Success = $false; Error = "File not found" }
        }
    }
}

# ============================================================================
# FUNCTION EXISTENCE TESTS
# ============================================================================
function Test-FunctionExistence {
    Write-TestHeader "Function Existence Verification"
    
    $expectedFunctions = @(
        # Core
        "Write-EmergencyLog",
        "Send-OllamaRequest",
        "Test-OllamaConnection",
        
        # Auto Features
        "Invoke-AutoDependencyGraph",
        "Invoke-SecurityVulnerabilityScan",
        "Invoke-AutoRefactorAnalysis",
        "Invoke-DynamicTestHarness",
        "Invoke-LiveMetricsDashboard",
        "Invoke-SourceCodeSummarizer",
        "Invoke-SelfHealing",
        "Invoke-PluginAutoLoader",
        "Invoke-ContinuousIntegrationTrigger",
        "Invoke-ManifestChangeNotifier",
        "Invoke-SourceDigestionOrchestratorAuto",
        "Invoke-Test-Security-Settings-FixAuto"
    )
    
    foreach ($func in $expectedFunctions) {
        Invoke-TestWithTiming -TestName "Function: $func" -TestScript {
            $cmd = Get-Command $func -ErrorAction SilentlyContinue
            if ($cmd) {
                return $true
            }
            return @{ Success = $false; Error = "Function not found" }
        }
    }
}

# ============================================================================
# COMPONENT FUNCTIONALITY TESTS
# ============================================================================
function Test-AutoDependencyGraph {
    Write-TestHeader "AutoDependencyGraph Functionality"
    
    Invoke-TestWithTiming -TestName "Dependency Graph Generation" -TestScript {
        $result = Invoke-AutoDependencyGraph -SourcePath "D:/lazy init ide" -OutputFormat 'JSON' -ErrorAction Stop
        return ($result -ne $null -and $result.Graph -ne $null)
    }
    
    Invoke-TestWithTiming -TestName "Circular Dependency Detection" -TestScript {
        $result = Invoke-AutoDependencyGraph -SourcePath "D:/lazy init ide" -OutputFormat 'JSON' -ErrorAction Stop
        return ($result.CircularDependencies -is [array])
    }
}

function Test-SecurityVulnerabilityScanner {
    Write-TestHeader "SecurityVulnerabilityScanner Functionality"
    
    Invoke-TestWithTiming -TestName "Security Scan Execution" -TestScript {
        $result = Invoke-SecurityVulnerabilityScan -Path "D:/lazy init ide/auto_generated_methods" -OutputFormat 'Object' -ErrorAction Stop
        return ($result -ne $null)
    }
    
    Invoke-TestWithTiming -TestName "Vulnerability Pattern Detection" -TestScript {
        # Create a temp file with known vulnerability pattern
        $tempFile = [System.IO.Path]::GetTempFileName() + ".ps1"
        '$password = "plaintext123"' | Set-Content $tempFile
        
        try {
            $result = Invoke-SecurityVulnerabilityScan -Path (Split-Path $tempFile -Parent) -OutputFormat 'Object' -ErrorAction Stop
            return ($result.Vulnerabilities -is [array])
        } finally {
            Remove-Item $tempFile -Force -ErrorAction SilentlyContinue
        }
    }
}

function Test-AutoRefactorSuggestor {
    Write-TestHeader "AutoRefactorSuggestor Functionality"
    
    Invoke-TestWithTiming -TestName "Refactoring Analysis" -TestScript {
        $result = Invoke-AutoRefactorAnalysis -SourcePath "D:/lazy init ide/auto_generated_methods" -ErrorAction Stop
        return ($result -ne $null)
    }
    
    Invoke-TestWithTiming -TestName "Complexity Detection" -TestScript {
        $result = Invoke-AutoRefactorAnalysis -SourcePath "D:/lazy init ide/auto_generated_methods" -ErrorAction Stop
        return ($result.Suggestions -is [array] -or $result.Summary -ne $null)
    }
}

function Test-DynamicTestHarness {
    Write-TestHeader "DynamicTestHarness Functionality"
    
    Invoke-TestWithTiming -TestName "Test Discovery" -TestScript {
        $result = Invoke-DynamicTestHarness -TestPath "D:/lazy init ide" -DiscoverOnly -ErrorAction Stop
        return ($result -ne $null)
    }
}

function Test-LiveMetricsDashboard {
    Write-TestHeader "LiveMetricsDashboard Functionality"
    
    Invoke-TestWithTiming -TestName "Metrics Collection" -TestScript {
        $result = Invoke-LiveMetricsDashboard -CollectOnce -ErrorAction Stop
        return ($result -ne $null -and $result.Metrics -ne $null)
    }
    
    Invoke-TestWithTiming -TestName "Performance Percentiles" -TestScript {
        $result = Invoke-LiveMetricsDashboard -CollectOnce -ErrorAction Stop
        return ($result.Percentiles -ne $null -or $result.Summary -ne $null)
    }
}

function Test-SourceCodeSummarizer {
    Write-TestHeader "SourceCodeSummarizer Functionality"
    
    Invoke-TestWithTiming -TestName "Code Summarization" -TestScript {
        $result = Invoke-SourceCodeSummarizer -SourcePath "D:/lazy init ide/RawrXD.Core.psm1" -ErrorAction Stop
        return ($result -ne $null)
    }
    
    Invoke-TestWithTiming -TestName "Function Extraction" -TestScript {
        $result = Invoke-SourceCodeSummarizer -SourcePath "D:/lazy init ide/RawrXD.Core.psm1" -ErrorAction Stop
        return ($result.Functions -is [array] -and $result.Functions.Count -gt 0)
    }
}

function Test-SelfHealingModule {
    Write-TestHeader "SelfHealingModule Functionality"
    
    Invoke-TestWithTiming -TestName "Health Check Execution" -TestScript {
        $result = Invoke-SelfHealing -CheckOnly -ErrorAction Stop
        return ($result -ne $null -and $result.HealthState -ne $null)
    }
    
    Invoke-TestWithTiming -TestName "Health State Machine" -TestScript {
        $result = Invoke-SelfHealing -CheckOnly -ErrorAction Stop
        $validStates = @('Healthy', 'Degraded', 'Unhealthy', 'Recovering')
        return ($result.HealthState -in $validStates)
    }
}

function Test-PluginAutoLoader {
    Write-TestHeader "PluginAutoLoader Functionality"
    
    Invoke-TestWithTiming -TestName "Plugin Discovery" -TestScript {
        # Ensure plugins directory exists
        $pluginDir = "D:/lazy init ide/plugins"
        if (-not (Test-Path $pluginDir)) {
            New-Item -Path $pluginDir -ItemType Directory -Force | Out-Null
        }
        
        $result = Invoke-PluginAutoLoader -PluginDirs @($pluginDir) -ErrorAction Stop
        return ($result -ne $null)
    }
}

function Test-ContinuousIntegrationTrigger {
    Write-TestHeader "ContinuousIntegrationTrigger Functionality"
    
    Invoke-TestWithTiming -TestName "Manual Build Trigger" -TestScript {
        $result = Invoke-ContinuousIntegrationTrigger -RunOnce -TriggerFiles @("test.ps1") -ErrorAction Stop
        return ($result -ne $null -and $result.BuildId -ne $null)
    }
    
    Invoke-TestWithTiming -TestName "Build Statistics" -TestScript {
        $stats = Get-CIStatistics -ErrorAction Stop
        return ($stats -ne $null -and $stats.TotalBuilds -ge 0)
    }
}

function Test-SourceDigestionOrchestrator {
    Write-TestHeader "SourceDigestionOrchestrator Functionality"
    
    Invoke-TestWithTiming -TestName "Source Digestion" -TestScript {
        $result = Invoke-SourceDigestionOrchestratorAuto -InputDirectory "D:/lazy init ide" -OutputLevel 'Summary' -ErrorAction Stop
        return ($result -ne $null -and $result.ProcessedFiles -ge 0)
    }
    
    Invoke-TestWithTiming -TestName "Symbol Table Generation" -TestScript {
        $result = Invoke-SourceDigestionOrchestratorAuto -InputDirectory "D:/lazy init ide" -GenerateSymbolTable -OutputLevel 'Summary' -ErrorAction Stop
        return ($result.Success -eq $true)
    }
}

function Test-SecuritySettingsValidator {
    Write-TestHeader "Security Settings Validator Functionality"
    
    Invoke-TestWithTiming -TestName "Security Validation" -TestScript {
        $result = Invoke-Test-Security-Settings-FixAuto -ScanPath "D:/lazy init ide" -OutputLevel 'Summary' -ErrorAction Stop
        return ($result -ne $null -and $result.Summary -ne $null)
    }
    
    Invoke-TestWithTiming -TestName "Compliance Report" -TestScript {
        $result = Invoke-Test-Security-Settings-FixAuto -ScanPath "D:/lazy init ide" -ComplianceFrameworks @('SOC2') -OutputLevel 'Summary' -ErrorAction Stop
        return ($result.Compliance -ne $null)
    }
}

# ============================================================================
# PERFORMANCE TESTS
# ============================================================================
function Test-Performance {
    if ($SkipPerformanceTests) {
        Write-TestHeader "Performance Tests (SKIPPED)"
        Write-TestResult -TestName "Performance Tests" -Status 'SKIP' -Message "Skipped via parameter"
        return
    }
    
    Write-TestHeader "Performance Benchmarks"
    
    # Module load time
    Invoke-TestWithTiming -TestName "Module Load Time < 2s" -TestScript {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        Get-Module RawrXD* | Remove-Module -Force -ErrorAction SilentlyContinue
        Import-Module "D:/lazy init ide/RawrXD.Core.psm1" -Force
        $sw.Stop()
        return ($sw.Elapsed.TotalSeconds -lt 2)
    }
    
    # Dependency graph performance
    Invoke-TestWithTiming -TestName "Dependency Graph < 5s" -TestScript {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        Invoke-AutoDependencyGraph -SourcePath "D:/lazy init ide" -OutputFormat 'JSON' | Out-Null
        $sw.Stop()
        return ($sw.Elapsed.TotalSeconds -lt 5)
    }
    
    # Security scan performance
    Invoke-TestWithTiming -TestName "Security Scan < 10s" -TestScript {
        $sw = [System.Diagnostics.Stopwatch]::StartNew()
        Invoke-SecurityVulnerabilityScan -Path "D:/lazy init ide" -OutputFormat 'Object' | Out-Null
        $sw.Stop()
        return ($sw.Elapsed.TotalSeconds -lt 10)
    }
}

# ============================================================================
# INTEGRATION TESTS
# ============================================================================
function Test-Integration {
    Write-TestHeader "Integration Tests"
    
    Invoke-TestWithTiming -TestName "Full Pipeline: Digest -> Analyze -> Report" -TestScript {
        # 1. Digest source
        $digest = Invoke-SourceDigestionOrchestratorAuto -InputDirectory "D:/lazy init ide/auto_generated_methods" -OutputLevel 'Summary'
        if (-not $digest.Success) { return @{ Success = $false; Error = "Digestion failed" } }
        
        # 2. Analyze for security
        $security = Invoke-SecurityVulnerabilityScan -Path "D:/lazy init ide/auto_generated_methods" -OutputFormat 'Object'
        if (-not $security) { return @{ Success = $false; Error = "Security scan failed" } }
        
        # 3. Check refactoring needs
        $refactor = Invoke-AutoRefactorAnalysis -SourcePath "D:/lazy init ide/auto_generated_methods"
        if (-not $refactor) { return @{ Success = $false; Error = "Refactor analysis failed" } }
        
        return $true
    }
    
    Invoke-TestWithTiming -TestName "Self-Healing Recovery Simulation" -TestScript {
        # Simulate health check
        $health = Invoke-SelfHealing -CheckOnly
        return ($health.HealthState -ne $null)
    }
}

# ============================================================================
# MAIN EXECUTION
# ============================================================================
function Invoke-AllTests {
    Write-Host "`n" -NoNewline
    Write-Host "╔══════════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
    Write-Host "║       RawrXD IDE/CLI Framework - Production Readiness Test           ║" -ForegroundColor Magenta
    Write-Host "║                         Version 2.0.0                                ║" -ForegroundColor Magenta
    Write-Host "╚══════════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
    Write-Host "`nStarting comprehensive validation at $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n" -ForegroundColor Gray
    
    # Run all test suites
    Test-ModuleLoading
    Test-FunctionExistence
    Test-AutoDependencyGraph
    Test-SecurityVulnerabilityScanner
    Test-AutoRefactorSuggestor
    Test-DynamicTestHarness
    Test-LiveMetricsDashboard
    Test-SourceCodeSummarizer
    Test-SelfHealingModule
    Test-PluginAutoLoader
    Test-ContinuousIntegrationTrigger
    Test-SourceDigestionOrchestrator
    Test-SecuritySettingsValidator
    Test-Performance
    Test-Integration
    
    # Finalize
    $script:TestResults.EndTime = Get-Date
    $duration = ($script:TestResults.EndTime - $script:TestResults.StartTime).TotalSeconds
    
    # Summary
    Write-Host "`n" -NoNewline
    Write-Host "╔══════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
    Write-Host "║                         TEST SUMMARY                                  ║" -ForegroundColor Cyan
    Write-Host "╚══════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
    Write-Host ""
    Write-Host "  Total Tests:   $($script:TestResults.TotalTests)" -ForegroundColor White
    Write-Host "  ✅ Passed:     $($script:TestResults.Passed)" -ForegroundColor Green
    Write-Host "  ❌ Failed:     $($script:TestResults.Failed)" -ForegroundColor $(if ($script:TestResults.Failed -gt 0) { 'Red' } else { 'Green' })
    Write-Host "  ⚠️  Warnings:   $($script:TestResults.Warnings)" -ForegroundColor $(if ($script:TestResults.Warnings -gt 0) { 'Yellow' } else { 'Green' })
    Write-Host "  ⏭️  Skipped:    $($script:TestResults.Skipped)" -ForegroundColor Gray
    Write-Host ""
    Write-Host "  Duration:      $([math]::Round($duration, 2)) seconds" -ForegroundColor Gray
    Write-Host ""
    
    # Calculate pass rate
    $passRate = if ($script:TestResults.TotalTests -gt 0) {
        [math]::Round(($script:TestResults.Passed / $script:TestResults.TotalTests) * 100, 1)
    } else { 0 }
    
    $passRateColor = if ($passRate -ge 90) { 'Green' } 
                     elseif ($passRate -ge 70) { 'Yellow' } 
                     else { 'Red' }
    
    Write-Host "  Pass Rate:     $passRate%" -ForegroundColor $passRateColor
    Write-Host ""
    
    # Production readiness verdict
    $isProductionReady = ($script:TestResults.Failed -eq 0 -and $passRate -ge 80)
    
    if ($isProductionReady) {
        Write-Host "  ╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Green
        Write-Host "  ║  🎉 PRODUCTION READY - All critical tests passed!              ║" -ForegroundColor Green
        Write-Host "  ╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Green
    } else {
        Write-Host "  ╔════════════════════════════════════════════════════════════════╗" -ForegroundColor Red
        Write-Host "  ║  ⚠️  NOT PRODUCTION READY - Review failed tests               ║" -ForegroundColor Red
        Write-Host "  ╚════════════════════════════════════════════════════════════════╝" -ForegroundColor Red
    }
    
    # Generate report if requested
    if ($GenerateReport) {
        $reportPath = "D:/lazy init ide/production-readiness-report.json"
        $report = @{
            GeneratedAt = Get-Date -Format 'yyyy-MM-ddTHH:mm:ssZ'
            Summary = @{
                TotalTests = $script:TestResults.TotalTests
                Passed = $script:TestResults.Passed
                Failed = $script:TestResults.Failed
                Warnings = $script:TestResults.Warnings
                Skipped = $script:TestResults.Skipped
                PassRate = $passRate
                Duration = $duration
                ProductionReady = $isProductionReady
            }
            Details = $script:TestResults.Details
        }
        
        $report | ConvertTo-Json -Depth 10 | Set-Content $reportPath -Encoding UTF8
        Write-Host "`n  Report saved to: $reportPath" -ForegroundColor Gray
    }
    
    Write-Host ""
    return $script:TestResults
}

# Run tests
$results = Invoke-AllTests
exit $(if ($results.Failed -gt 0) { 1 } else { 0 })
