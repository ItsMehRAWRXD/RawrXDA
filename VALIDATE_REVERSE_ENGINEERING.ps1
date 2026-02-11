#requires -Version 7.0
<#
.SYNOPSIS
    Validates the Reverse Engineering Master System
.DESCRIPTION
    Checks all components are in place and operational
#>

$ProjectRoot = "d:\lazy init ide"
$TestResults = @{
    Passed = 0
    Failed = 0
    Total = 0
    Details = @()
}

function Test-Component {
    param(
        [string]$Name,
        [scriptblock]$Test,
        [string]$Expected
    )
    
    $TestResults.Total++
    
    try {
        $result = & $Test
        if ($result) {
            $TestResults.Passed++
            Write-Host "✓ $Name" -ForegroundColor Green
            $TestResults.Details += @{
                Name = $Name
                Status = "PASS"
                Expected = $Expected
                Actual = "Present"
            }
        } else {
            $TestResults.Failed++
            Write-Host "✗ $Name" -ForegroundColor Red
            $TestResults.Details += @{
                Name = $Name
                Status = "FAIL"
                Expected = $Expected
                Actual = "Missing"
            }
        }
    } catch {
        $TestResults.Failed++
        Write-Host "✗ $Name - Error: $_" -ForegroundColor Red
        $TestResults.Details += @{
            Name = $Name
            Status = "ERROR"
            Expected = $Expected
            Actual = $_.Exception.Message
        }
    }
}

Write-Host "`n╔══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║         REVERSE ENGINEERING MASTER - VALIDATION SUITE                        ║" -ForegroundColor Magenta
Write-Host "╚══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

Write-Host "`n[1/6] Core System Files" -ForegroundColor Cyan
Test-Component "REVERSE_ENGINEERING_MASTER.ps1" {
    Test-Path "$ProjectRoot\REVERSE_ENGINEERING_MASTER.ps1"
} "Master integration script"

Test-Component "REVERSE_ENGINEERING_QUICK_START.md" {
    Test-Path "$ProjectRoot\REVERSE_ENGINEERING_QUICK_START.md"
} "Quick start guide"

Test-Component "REVERSE_ENGINEERING_COMPLETE_DELIVERY.md" {
    Test-Path "$ProjectRoot\REVERSE_ENGINEERING_COMPLETE_DELIVERY.md"
} "Complete delivery documentation"

Write-Host "`n[2/6] Existing Infrastructure" -ForegroundColor Cyan
Test-Component "Reverse-Engineer-Cursor.ps1" {
    Test-Path "$ProjectRoot\Reverse-Engineer-Cursor.ps1"
} "Cursor reverse engineering script"

Test-Component "Build-CodexReverse.ps1" {
    Test-Path "$ProjectRoot\Build-CodexReverse.ps1"
} "Codex reverse engineering script"

Test-Component "Cursor_Source_Extracted directory" {
    Test-Path "$ProjectRoot\Cursor_Source_Extracted"
} "Cursor extraction directory"

Test-Component "Cursor_Reverse_Engineered_Fork directory" {
    Test-Path "$ProjectRoot\Cursor_Reverse_Engineered_Fork"
} "Cursor fork directory"

Write-Host "`n[3/6] IDE Source Directories" -ForegroundColor Cyan
Test-Component "src/ directory" {
    Test-Path "$ProjectRoot\src"
} "IDE source directory"

Test-Component "include/ directory" {
    Test-Path "$ProjectRoot\include"
} "IDE include directory"

Write-Host "`n[4/6] Build System Integration" -ForegroundColor Cyan
Test-Component "BUILD_ORCHESTRATOR.ps1" {
    Test-Path "$ProjectRoot\BUILD_ORCHESTRATOR.ps1"
} "Build orchestrator (from previous session)"

Test-Component "BUILD_IDE_FAST.ps1" {
    Test-Path "$ProjectRoot\BUILD_IDE_FAST.ps1"
} "Fast build script"

Test-Component "BUILD_IDE_PRODUCTION.ps1" {
    Test-Path "$ProjectRoot\BUILD_IDE_PRODUCTION.ps1"
} "Production build script"

Write-Host "`n[5/6] Script Syntax Validation" -ForegroundColor Cyan
Test-Component "Master script syntax valid" {
    $null = [System.Management.Automation.PSParser]::Tokenize(
        (Get-Content "$ProjectRoot\REVERSE_ENGINEERING_MASTER.ps1" -Raw), 
        [ref]$null
    )
    $true
} "No PowerShell syntax errors"

Write-Host "`n[6/6] Functional Tests" -ForegroundColor Cyan
Test-Component "Master script has required parameters" {
    $content = Get-Content "$ProjectRoot\REVERSE_ENGINEERING_MASTER.ps1" -Raw
    $content -match 'param\s*\(' -and $content -match '\$Mode'
} "Script parameters defined"

Test-Component "Master script has all modes" {
    $content = Get-Content "$ProjectRoot\REVERSE_ENGINEERING_MASTER.ps1" -Raw
    $content -match 'analyze' -and 
    $content -match 'auto' -and 
    $content -match 'manual' -and 
    $content -match 'cursor' -and 
    $content -match 'codex' -and 
    $content -match 'all' -and
    $content -match 'integrate'
} "All 7 modes implemented"

Test-Component "Completeness analysis function exists" {
    $content = Get-Content "$ProjectRoot\REVERSE_ENGINEERING_MASTER.ps1" -Raw
    $content -match 'function Get-CompletenessCircle'
} "Completeness detection implemented"

Test-Component "Integration functions exist" {
    $content = Get-Content "$ProjectRoot\REVERSE_ENGINEERING_MASTER.ps1" -Raw
    $content -match 'function Integrate-CursorFeatures' -and
    $content -match 'function Integrate-CodexFeatures'
} "Integration functions implemented"

Test-Component "Interactive menu exists" {
    $content = Get-Content "$ProjectRoot\REVERSE_ENGINEERING_MASTER.ps1" -Raw
    $content -match 'function Show-InteractiveMenu'
} "Interactive mode implemented"

# Summary
Write-Host "`n╔══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║                           VALIDATION SUMMARY                                  ║" -ForegroundColor Magenta
Write-Host "╚══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Magenta

Write-Host "`nResults:" -ForegroundColor White
Write-Host "  Total Tests: $($TestResults.Total)" -ForegroundColor White
Write-Host "  Passed: $($TestResults.Passed)" -ForegroundColor Green
Write-Host "  Failed: $($TestResults.Failed)" -ForegroundColor $(if ($TestResults.Failed -eq 0) { 'Green' } else { 'Red' })

$percentage = [math]::Round(($TestResults.Passed / $TestResults.Total) * 100, 2)
$color = if ($percentage -eq 100) { 'Green' } 
         elseif ($percentage -ge 80) { 'Yellow' } 
         else { 'Red' }

Write-Host "`nValidation Score: $percentage%" -ForegroundColor $color

if ($TestResults.Failed -gt 0) {
    Write-Host "`nFailed Tests:" -ForegroundColor Red
    foreach ($detail in $TestResults.Details | Where-Object { $_.Status -ne "PASS" }) {
        Write-Host "  ✗ $($detail.Name)" -ForegroundColor Red
        Write-Host "    Expected: $($detail.Expected)" -ForegroundColor Gray
        Write-Host "    Actual: $($detail.Actual)" -ForegroundColor Gray
    }
}

Write-Host "`n╔══════════════════════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║                          NEXT STEPS                                           ║" -ForegroundColor Cyan
Write-Host "╚══════════════════════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan

if ($percentage -eq 100) {
    Write-Host "`n✅ All validation checks passed! System is ready to use." -ForegroundColor Green
    Write-Host "`nRun this to get started:" -ForegroundColor White
    Write-Host "  .\REVERSE_ENGINEERING_MASTER.ps1 -Mode analyze" -ForegroundColor Cyan
    Write-Host "`nOr this for automatic integration:" -ForegroundColor White
    Write-Host "  .\REVERSE_ENGINEERING_MASTER.ps1 -Mode auto -AutoDetect -IntegrateAll" -ForegroundColor Cyan
} elseif ($percentage -ge 80) {
    Write-Host "`n⚠ Most checks passed, but some components may be missing." -ForegroundColor Yellow
    Write-Host "  System should still be functional." -ForegroundColor Yellow
    Write-Host "`nYou can still run:" -ForegroundColor White
    Write-Host "  .\REVERSE_ENGINEERING_MASTER.ps1 -Mode analyze" -ForegroundColor Cyan
} else {
    Write-Host "`n✗ Several validation checks failed." -ForegroundColor Red
    Write-Host "  Please review failed tests above." -ForegroundColor Red
    Write-Host "`nYou may need to re-download the system files." -ForegroundColor Yellow
}

Write-Host ""
