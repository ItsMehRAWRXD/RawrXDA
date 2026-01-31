# RawrXD Production Integration Test Suite
# Comprehensive testing for all production components

#Requires -Version 5.1

param(
    [Parameter(Mandatory = $false)]
    [switch]$VerboseOutput = $false,
    
    [Parameter(Mandatory = $false)]
    [switch]$FuzzTest = $false,
    
    [Parameter(Mandatory = $false)]
    [int]$FuzzIterations = 1000
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║     RawrXD Production Integration Test Suite            ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

$testResults = @{
    Passed = 0
    Failed = 0
    Skipped = 0
    Tests = @()
    StartTime = Get-Date
}

function Test-Component {
    param(
        [string]$Name,
        [scriptblock]$TestScript
    )
    
    Write-Host "[TEST] $Name" -ForegroundColor Yellow
    
    try {
        $result = & $TestScript
        if ($result) {
            Write-Host "  ✓ PASSED" -ForegroundColor Green
            $testResults.Passed++
            $testResults.Tests += @{ Name = $Name; Result = "PASSED"; Error = $null }
            return $true
        } else {
            Write-Host "  ✗ FAILED" -ForegroundColor Red
            $testResults.Failed++
            $testResults.Tests += @{ Name = $Name; Result = "FAILED"; Error = "Test returned false" }
            return $false
        }
    } catch {
        Write-Host "  ✗ FAILED: $_" -ForegroundColor Red
        $testResults.Failed++
        $testResults.Tests += @{ Name = $Name; Result = "FAILED"; Error = $_.Exception.Message }
        return $false
    }
}

# Test Suite 1: Module Import Tests
Write-Host "`n═══ MODULE IMPORT TESTS ═══" -ForegroundColor Cyan

Test-Component "RawrXD.Config Module Import" {
    Import-Module (Join-Path $root 'RawrXD.Config.psm1') -Force
    return (Get-Command Get-RawrXDRootPath -ErrorAction SilentlyContinue) -ne $null
}

Test-Component "RawrXD.Logging Module Import" {
    Import-Module (Join-Path $root 'RawrXD.Logging.psm1') -Force
    return (Get-Command Write-StructuredLog -ErrorAction SilentlyContinue) -ne $null
}

Test-Component "RawrXD.ErrorHandling Module Import" {
    Import-Module (Join-Path $root 'RawrXD.ErrorHandling.psm1') -Force
    return (Get-Command Invoke-RawrXDSafeOperation -ErrorAction SilentlyContinue) -ne $null
}

Test-Component "RawrXD.Metrics Module Import" {
    Import-Module (Join-Path $root 'RawrXD.Metrics.psm1') -Force
    return (Get-Command Initialize-RawrXDMetrics -ErrorAction SilentlyContinue) -ne $null
}

Test-Component "RawrXD.Tracing Module Import" {
    Import-Module (Join-Path $root 'RawrXD.Tracing.psm1') -Force
    return (Get-Command New-RawrXDTraceId -ErrorAction SilentlyContinue) -ne $null
}

Test-Component "RawrXD.ModelLoader Module Import" {
    Import-Module (Join-Path $root 'RawrXD.ModelLoader.psm1') -Force
    return (Get-Command Invoke-RawrXDModelRequest -ErrorAction SilentlyContinue) -ne $null
}

Test-Component "RawrXD.Agentic.Autonomy Module Import" {
    Import-Module (Join-Path $root 'RawrXD.Agentic.Autonomy.psm1') -Force
    return (Get-Command Start-RawrXDAutonomyLoop -ErrorAction SilentlyContinue) -ne $null
}

Test-Component "RawrXD.Win32Deployment Module Import" {
    Import-Module (Join-Path $root 'RawrXD.Win32Deployment.psm1') -Force
    return (Get-Command Test-RawrXDWin32Prereqs -ErrorAction SilentlyContinue) -ne $null
}

# Test Suite 2: Functional Tests
Write-Host "`n═══ FUNCTIONAL TESTS ═══" -ForegroundColor Cyan

Test-Component "Metrics Collection" {
    Initialize-RawrXDMetrics
    Increment-RawrXDCounter -Name 'test_counter'
    Set-RawrXDGauge -Name 'test_gauge' -Value 42
    Observe-RawrXDHistogram -Name 'test_histogram' -Value 0.123
    $path = Export-RawrXDPrometheusText
    $text = Get-Content $path -Raw
    return ($text -match 'test_counter') -and ($text -match 'test_gauge') -and ($text -match 'test_histogram')
}

Test-Component "Tracing Span Creation" {
    $traceId = New-RawrXDTraceId
    $span = Start-RawrXDSpan -TraceId $traceId -Name 'test_operation'
    Stop-RawrXDSpan -Span $span
    return ($traceId.Length -gt 0) -and ($span.TraceId -eq $traceId)
}

Test-Component "Error Handling Safe Operation" {
    $result = Invoke-RawrXDSafeOperation -Name 'test_op' -Context @{} -Script {
        return @{ Success = $true }
    }
    return $result.Success -eq $true
}

Test-Component "Model Config Loading" {
    $config = Get-RawrXDModelConfig
    return $config -is [PSCustomObject] -or $config -is [hashtable]
}

Test-Component "Autonomy Goal Setting" {
    Set-RawrXDAutonomyGoal -Goal "Test autonomous workflow"
    $plan = Get-RawrXDAutonomyPlan -Goal "Test goal"
    return $plan.Count -gt 0
}

Test-Component "Win32 Prerequisites Check" {
    $prereqs = Test-RawrXDWin32Prereqs
    return $prereqs -is [array] -and $prereqs.Count -gt 0
}

# Test Suite 3: Regression Tests
Write-Host "`n═══ REGRESSION TESTS ═══" -ForegroundColor Cyan

Test-Component "Config Path Resolution" {
    $root = Get-RawrXDRootPath
    return (Test-Path $root) -and $root.Length -gt 0
}

Test-Component "Structured Logging" {
    Write-StructuredLog -Level 'INFO' -Message 'Test log message' -Function 'TestFunction'
    $logPath = Get-RawrXDLogPath
    return Test-Path $logPath
}

Test-Component "Metrics State Persistence" {
    Increment-RawrXDCounter -Name 'persistent_counter'
    Increment-RawrXDCounter -Name 'persistent_counter'
    $path = Export-RawrXDPrometheusText
    $text = Get-Content $path -Raw
    return $text -match 'persistent_counter\s+2'
}

# Test Suite 4: Fuzz Testing (optional)
if ($FuzzTest) {
    Write-Host "`n═══ FUZZ TESTS ═══" -ForegroundColor Cyan
    
    Test-Component "Fuzz Test: Model Request with Random Inputs" {
        $passed = 0
        for ($i = 0; $i -lt $FuzzIterations; $i++) {
            try {
                $randomPrompt = -join ((65..90) + (97..122) | Get-Random -Count 50 | ForEach-Object { [char]$_ })
                $result = Invoke-RawrXDModelRequest -ModelName 'invalid_model' -Prompt $randomPrompt
                if ($result.Success -eq $false) { $passed++ }
            } catch {
                # Expected to fail gracefully
                $passed++
            }
        }
        return $passed -eq $FuzzIterations
    }
    
    Test-Component "Fuzz Test: Metrics with Edge Cases" {
        $passed = 0
        $edgeCases = @(0, -1, 1e308, [double]::NaN, [double]::PositiveInfinity)
        foreach ($value in $edgeCases) {
            try {
                Set-RawrXDGauge -Name 'fuzz_gauge' -Value $value
                $passed++
            } catch {
                # Some edge cases should throw
            }
        }
        return $passed -ge 3
    }
}

# Test Suite 5: Integration Tests
Write-Host "`n═══ INTEGRATION TESTS ═══" -ForegroundColor Cyan

Test-Component "End-to-End: Autonomy Action Execution" {
    $action = @{ Type = 'scan'; Payload = @{ Path = $root } }
    $result = Invoke-RawrXDAutonomyAction -Action $action
    return $result.Success -eq $true
}

Test-Component "End-to-End: Metrics HTTP Server" {
    Start-RawrXDMetricServer -Port 19090
    Start-Sleep -Seconds 2
    try {
        $response = Invoke-WebRequest -Uri 'http://localhost:19090/metrics' -UseBasicParsing -TimeoutSec 5
        $success = $response.StatusCode -eq 200
        Stop-RawrXDMetricServer
        return $success
    } catch {
        Stop-RawrXDMetricServer
        return $false
    }
}

Test-Component "End-to-End: Win32 Build Dry Run" {
    $result = Invoke-RawrXDWin32Build -DryRun
    return $result.Success -eq $true
}

# Final Summary
$testResults.EndTime = Get-Date
$testResults.Duration = ($testResults.EndTime - $testResults.StartTime).TotalSeconds

Write-Host ""
Write-Host "╔═══════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║              TEST SUITE COMPLETED                        ║" -ForegroundColor Cyan
Write-Host "╚═══════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
Write-Host ""

Write-Host "Test Results Summary:" -ForegroundColor Yellow
Write-Host "  Total Tests: $($testResults.Passed + $testResults.Failed + $testResults.Skipped)" -ForegroundColor White
Write-Host "  Passed: $($testResults.Passed)" -ForegroundColor Green
Write-Host "  Failed: $($testResults.Failed)" -ForegroundColor $(if ($testResults.Failed -eq 0) { "Green" } else { "Red" })
Write-Host "  Skipped: $($testResults.Skipped)" -ForegroundColor Gray
Write-Host "  Duration: $([Math]::Round($testResults.Duration, 2))s" -ForegroundColor White
Write-Host ""

if ($testResults.Failed -gt 0) {
    Write-Host "Failed Tests:" -ForegroundColor Red
    foreach ($test in $testResults.Tests | Where-Object { $_.Result -eq 'FAILED' }) {
        Write-Host "  ✗ $($test.Name)" -ForegroundColor Red
        Write-Host "    Error: $($test.Error)" -ForegroundColor Gray
    }
    Write-Host ""
    exit 1
} else {
    Write-Host "✓ All tests passed!" -ForegroundColor Green
    Write-Host ""
    exit 0
}
