<#
.SYNOPSIS
    Master test runner for all RawrXD IDE extension tests
.DESCRIPTION
    Executes complete test suite for Amazon Q and GitHub Copilot extensions
#>

param(
    [switch]$Detailed,
    [switch]$ExportHTML,
    [switch]$StopOnError
)

$ErrorActionPreference = if ($StopOnError) { "Stop" } else { "Continue" }

# Banner
Clear-Host
Write-Host @"

    ____                   _  ________  
   / __ \____ __      ____| |/ / __ \ 
  / /_/ / __ \\ \ /\ / / __   / / / / 
 / _, _/ /_/ / \ V  V / / /  / /_/ /  
/_/ |_|\__,_/   \_/\_/ /_/   \____/   
                                       
   Extension Test Suite — Complete
   Amazon Q + GitHub Copilot Validation
                                       
"@ -ForegroundColor Cyan

Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Gray

$startTime = Get-Date
$testResults = @{}

# Test 1: Smoke Tests
Write-Host "[1/3] Running Smoke Tests (Infrastructure & Integration)..." -ForegroundColor White
Write-Host "      This validates VSIX loading, plugin system, and CLI/GUI modes`n" -ForegroundColor Gray

try {
    $smokeTest = & "d:\rawrxd\Test-Extensions-Smoke.ps1" -Mode both
    $testResults["SmokeTest"] = @{
        Status = "Success"
        Output = $smokeTest
        Duration = (Get-Date) - $startTime
    }
    Write-Host "✓ Smoke Tests completed successfully`n" -ForegroundColor Green
} catch {
    $testResults["SmokeTest"] = @{
        Status = "Failed"
        Error = $_.Exception.Message
        Duration = (Get-Date) - $startTime
    }
    Write-Host "✗ Smoke Tests failed: $($_.Exception.Message)`n" -ForegroundColor Red
    if ($StopOnError) { exit 1 }
}

Start-Sleep -Seconds 2

# Test 2: Feature Tests - Copilot
Write-Host "[2/3] Running Feature Tests — GitHub Copilot..." -ForegroundColor White
Write-Host "      This tests code completion, chat, and explanation features`n" -ForegroundColor Gray

$featureStartTime = Get-Date
try {
    $copilotTest = & "d:\rawrxd\Test-Extensions-Features.ps1" -Extension copilot
    $testResults["CopilotFeatures"] = @{
        Status = "Success"
        Output = $copilotTest
        Duration = (Get-Date) - $featureStartTime
    }
    Write-Host "✓ Copilot Feature Tests completed`n" -ForegroundColor Green
} catch {
    $testResults["CopilotFeatures"] = @{
        Status = "Failed"
        Error = $_.Exception.Message
        Duration = (Get-Date) - $featureStartTime
    }
    Write-Host "✗ Copilot Feature Tests failed: $($_.Exception.Message)`n" -ForegroundColor Red
    if ($StopOnError) { exit 1 }
}

Start-Sleep -Seconds 2

# Test 3: Feature Tests - Amazon Q
Write-Host "[3/3] Running Feature Tests — Amazon Q..." -ForegroundColor White
Write-Host "      This tests AWS chat, security scanning, and code generation`n" -ForegroundColor Gray

$amazonQStartTime = Get-Date
try {
    $amazonQTest = & "d:\rawrxd\Test-Extensions-Features.ps1" -Extension amazonq
    $testResults["AmazonQFeatures"] = @{
        Status = "Success"
        Output = $amazonQTest
        Duration = (Get-Date) - $amazonQStartTime
    }
    Write-Host "✓ Amazon Q Feature Tests completed`n" -ForegroundColor Green
} catch {
    $testResults["AmazonQFeatures"] = @{
        Status = "Failed"
        Error = $_.Exception.Message
        Duration = (Get-Date) - $amazonQStartTime
    }
    Write-Host "✗ Amazon Q Feature Tests failed: $($_.Exception.Message)`n" -ForegroundColor Red
    if ($StopOnError) { exit 1 }
}

$totalDuration = (Get-Date) - $startTime

# Summary
Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
Write-Host "                 COMPLETE TEST SUITE SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Gray

foreach ($test in $testResults.Keys) {
    $result = $testResults[$test]
    $status = if ($result.Status -eq "Success") { "✓" } else { "✗" }
    $color = if ($result.Status -eq "Success") { "Green" } else { "Red" }
    $duration = [math]::Round($result.Duration.TotalSeconds, 2)
    
    Write-Host "  $status " -ForegroundColor $color -NoNewline
    Write-Host "$test".PadRight(30) -NoNewline
    Write-Host "$duration`s" -ForegroundColor Gray
    
    if ($result.Status -eq "Failed" -and $Detailed) {
        Write-Host "    Error: $($result.Error)" -ForegroundColor Red
    }
}

Write-Host "`nTotal Duration: $([math]::Round($totalDuration.TotalSeconds, 2))s" -ForegroundColor White

# Pass/Fail Count
$passed = ($testResults.Values | Where-Object { $_.Status -eq "Success" }).Count
$failed = ($testResults.Values | Where-Object { $_.Status -eq "Failed" }).Count

Write-Host "`n┌───────────────────────────────────────────────────────────────┐" -ForegroundColor Gray
if ($failed -eq 0) {
    Write-Host "│  " -ForegroundColor Gray -NoNewline
    Write-Host "ALL TESTS PASSED ($passed/$($testResults.Count))".PadRight(58) -ForegroundColor Green -NoNewline
    Write-Host "  │" -ForegroundColor Gray
} else {
    Write-Host "│  " -ForegroundColor Gray -NoNewline
    Write-Host "SOME TESTS FAILED ($passed passed, $failed failed)".PadRight(58) -ForegroundColor Yellow -NoNewline
    Write-Host "  │" -ForegroundColor Gray
}
Write-Host "└───────────────────────────────────────────────────────────────┘`n" -ForegroundColor Gray

# Collect all test output files
Write-Host "Test Artifacts:" -ForegroundColor White
$testFiles = Get-ChildItem "d:\rawrxd\test_*.txt", "d:\rawrxd\*_out.txt", "d:\rawrxd\*_err.txt" -ErrorAction SilentlyContinue
if ($testFiles) {
    foreach ($file in $testFiles | Select-Object -First 10) {
        Write-Host "  → $($file.Name) ($([math]::Round($file.Length/1KB, 2))KB)" -ForegroundColor Gray
    }
    if ($testFiles.Count -gt 10) {
        Write-Host "  → ... and $($testFiles.Count - 10) more files" -ForegroundColor Gray
    }
} else {
    Write-Host "  (No test artifacts found)" -ForegroundColor DarkGray
}

# JSON Export
$reportPath = "d:\rawrxd\master_test_report_$(Get-Date -Format 'yyyyMMdd_HHmmss').json"
$testResults | ConvertTo-Json -Depth 10 | Out-File $reportPath
Write-Host "`nJSON Report: $reportPath" -ForegroundColor Gray

# HTML Export (if requested)
if ($ExportHTML) {
    Write-Host "`nGenerating HTML report..." -ForegroundColor White
    
    $htmlReport = @"
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>RawrXD Extension Test Report</title>
    <style>
        body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; background: #1e1e1e; color: #d4d4d4; padding: 20px; }
        .container { max-width: 1200px; margin: 0 auto; }
        h1 { color: #4ec9b0; border-bottom: 2px solid #4ec9b0; padding-bottom: 10px; }
        h2 { color: #569cd6; margin-top: 30px; }
        .test-item { background: #252526; padding: 15px; margin: 10px 0; border-radius: 5px; border-left: 4px solid #4ec9b0; }
        .test-item.failed { border-left-color: #f48771; }
        .status { font-weight: bold; }
        .success { color: #4ec9b0; }
        .failed { color: #f48771; }
        .duration { color: #808080; float: right; }
        .summary { background: #2d2d30; padding: 20px; border-radius: 5px; margin: 20px 0; }
        .summary-stat { display: inline-block; margin-right: 30px; font-size: 1.2em; }
        pre { background: #1e1e1e; padding: 10px; border-radius: 3px; overflow-x: auto; }
    </style>
</head>
<body>
    <div class="container">
        <h1>🧪 RawrXD IDE Extension Test Report</h1>
        <p>Generated: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')</p>
        
        <div class="summary">
            <h2>Summary</h2>
            <div class="summary-stat"><strong>Total Tests:</strong> $($testResults.Count)</div>
            <div class="summary-stat"><strong class="success">Passed:</strong> $passed</div>
            <div class="summary-stat"><strong class="failed">Failed:</strong> $failed</div>
            <div class="summary-stat"><strong>Duration:</strong> $([math]::Round($totalDuration.TotalSeconds, 2))s</div>
        </div>
        
        <h2>Test Results</h2>
"@
    
    foreach ($test in $testResults.Keys) {
        $result = $testResults[$test]
        $statusClass = if ($result.Status -eq "Success") { "success" } else { "failed" }
        $failedClass = if ($result.Status -eq "Failed") { "failed" } else { "" }
        $duration = [math]::Round($result.Duration.TotalSeconds, 2)
        
        $htmlReport += @"
        <div class="test-item $failedClass">
            <span class="status $statusClass">$($result.Status)</span> — <strong>$test</strong>
            <span class="duration">${duration}s</span>
            $(if ($result.Status -eq "Failed") { "<pre>Error: $($result.Error)</pre>" } else { "" })
        </div>
"@
    }
    
    $htmlReport += @"
    </div>
</body>
</html>
"@
    
    $htmlPath = "d:\rawrxd\master_test_report_$(Get-Date -Format 'yyyyMMdd_HHmmss').html"
    $htmlReport | Out-File $htmlPath -Encoding UTF8
    Write-Host "HTML Report: $htmlPath" -ForegroundColor Gray
    
    # Open in browser
    Start-Process $htmlPath
}

# Detailed output files
if ($Detailed) {
    Write-Host "`n═══ Detailed Test Outputs ═══" -ForegroundColor Cyan
    
    # Show last smoke test report
    $latestReport = Get-ChildItem "d:\rawrxd\extension_smoketest_report_*.json" -ErrorAction SilentlyContinue | 
                    Sort-Object LastWriteTime -Descending | 
                    Select-Object -First 1
    
    if ($latestReport) {
        Write-Host "`nLatest Smoke Test Report:" -ForegroundColor White
        $report = Get-Content $latestReport.FullName -Raw | ConvertFrom-Json
        $report | Format-Table Test, Result, Details -AutoSize | Out-String | Write-Host
    }
}

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Gray
Write-Host "                    Test Suite Complete" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════`n" -ForegroundColor Gray

# Exit code
if ($failed -gt 0) {
    exit 1
} else {
    exit 0
}
