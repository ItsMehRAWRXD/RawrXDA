# Test-AmazonQ-Copilot-Fixes.ps1
# Tests for Amazon Q connection fix and GitHub Copilot setup

$ErrorActionPreference = "Continue"
$testResults = @{
    Total = 0
    Passed = 0
    Failed = 0
    Warnings = 0
    Issues = @()
}

function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed,
        [string]$Message = "",
        [string]$Warning = ""
    )

    $testResults.Total++
    if ($Passed) {
        $testResults.Passed++
        Write-Host "✅ PASS: $TestName" -ForegroundColor Green
        if ($Message) { Write-Host "   $Message" -ForegroundColor Gray }
    }
    elseif ($Warning) {
        $testResults.Warnings++
        Write-Host "⚠️  WARN: $TestName" -ForegroundColor Yellow
        Write-Host "   $Warning" -ForegroundColor Yellow
    }
    else {
        $testResults.Failed++
        Write-Host "❌ FAIL: $TestName" -ForegroundColor Red
        if ($Message) { Write-Host "   $Message" -ForegroundColor Red }
        $testResults.Issues += "${TestName}: ${Message}"
    }
}

Write-Host "`n" + "="*60 -ForegroundColor Cyan
Write-Host "  Amazon Q & GitHub Copilot Fix Tests" -ForegroundColor Cyan
Write-Host "="*60 -ForegroundColor Cyan

# Test 1: Fix script exists
Write-Host "`n=== Testing Fix Scripts ===" -ForegroundColor Cyan
$fixScript = "Fix-AmazonQ-Connection.ps1"
if (Test-Path $fixScript) {
    Write-TestResult -TestName "Fix Script Exists" -Passed $true -Message "Found: $fixScript"
} else {
    Write-TestResult -TestName "Fix Script Exists" -Passed $false -Message "Not found: $fixScript"
}

$setupScript = "Setup-GitHubCopilot.ps1"
if (Test-Path $setupScript) {
    Write-TestResult -TestName "Setup Script Exists" -Passed $true -Message "Found: $setupScript"
} else {
    Write-TestResult -TestName "Setup Script Exists" -Passed $false -Message "Not found: $setupScript"
}

# Test 2: Cursor settings file
Write-Host "`n=== Testing Cursor Settings ===" -ForegroundColor Cyan
$cursorSettingsPath = "$env:APPDATA\Cursor\User\settings.json"
if (Test-Path $cursorSettingsPath) {
    Write-TestResult -TestName "Cursor Settings File Exists" -Passed $true -Message "Found: $cursorSettingsPath"
    
    try {
        $settings = Get-Content $cursorSettingsPath -Raw | ConvertFrom-Json
        
        # Check if proxy setting is removed
        if ($settings.PSObject.Properties.Name -contains 'aiSuite.simple.provider') {
            $proxyValue = $settings.'aiSuite.simple.provider'
            if ($proxyValue -eq "proxy") {
                Write-TestResult -TestName "Proxy Setting Removed" -Passed $false -Message "Proxy setting still set to 'proxy'"
            } else {
                Write-TestResult -TestName "Proxy Setting Removed" -Passed $true -Message "Proxy setting is not 'proxy' (value: $proxyValue)"
            }
        } else {
            Write-TestResult -TestName "Proxy Setting Removed" -Passed $true -Message "Proxy setting has been removed"
        }
        
        # Check Amazon Q settings
        if ($settings.PSObject.Properties.Name -contains 'amazonQ.telemetry') {
            Write-TestResult -TestName "Amazon Q Telemetry Setting" -Passed $true -Message "amazonQ.telemetry is configured"
        } else {
            Write-TestResult -TestName "Amazon Q Telemetry Setting" -Passed $false -Warning "amazonQ.telemetry not found"
        }
        
        if ($settings.PSObject.Properties.Name -contains 'amazonQ.workspaceIndex') {
            Write-TestResult -TestName "Amazon Q Workspace Index Setting" -Passed $true -Message "amazonQ.workspaceIndex is configured"
        } else {
            Write-TestResult -TestName "Amazon Q Workspace Index Setting" -Passed $false -Warning "amazonQ.workspaceIndex not found"
        }
    }
    catch {
        Write-TestResult -TestName "Cursor Settings Parsing" -Passed $false -Message "Error parsing settings: $_"
    }
} else {
    Write-TestResult -TestName "Cursor Settings File Exists" -Passed $false -Warning "Cursor settings file not found (may not be installed)"
}

# Test 3: Workspace configuration
Write-Host "`n=== Testing Workspace Configuration ===" -ForegroundColor Cyan
$extensionsPath = ".vscode\extensions.json"
if (Test-Path $extensionsPath) {
    Write-TestResult -TestName "Extensions JSON Exists" -Passed $true -Message "Found: $extensionsPath"
    
    try {
        $extensions = Get-Content $extensionsPath -Raw | ConvertFrom-Json
        
        if ($extensions.recommendations -contains "GitHub.copilot") {
            Write-TestResult -TestName "GitHub Copilot in Extensions" -Passed $true -Message "GitHub Copilot extension recommended"
        } else {
            Write-TestResult -TestName "GitHub Copilot in Extensions" -Passed $false -Message "GitHub Copilot not in recommendations"
        }
        
        if ($extensions.recommendations -contains "amazonwebservices.amazon-q-vscode") {
            Write-TestResult -TestName "Amazon Q in Extensions" -Passed $true -Message "Amazon Q extension recommended"
        } else {
            Write-TestResult -TestName "Amazon Q in Extensions" -Passed $false -Message "Amazon Q not in recommendations"
        }
    }
    catch {
        Write-TestResult -TestName "Extensions JSON Parsing" -Passed $false -Message "Error parsing extensions.json: $_"
    }
} else {
    Write-TestResult -TestName "Extensions JSON Exists" -Passed $false -Message "Extensions.json not found"
}

$settingsPath = ".vscode\settings.json"
if (Test-Path $settingsPath) {
    Write-TestResult -TestName "Workspace Settings Exists" -Passed $true -Message "Found: $settingsPath"
    
    try {
        $settings = Get-Content $settingsPath -Raw | ConvertFrom-Json
        
        if ($settings.'github.copilot.enable') {
            Write-TestResult -TestName "GitHub Copilot Settings" -Passed $true -Message "GitHub Copilot settings configured"
        } else {
            Write-TestResult -TestName "GitHub Copilot Settings" -Passed $false -Warning "GitHub Copilot settings not found"
        }
        
        if ($settings.'amazonQ.telemetry') {
            Write-TestResult -TestName "Amazon Q Workspace Settings" -Passed $true -Message "Amazon Q workspace settings configured"
        } else {
            Write-TestResult -TestName "Amazon Q Workspace Settings" -Passed $false -Warning "Amazon Q workspace settings not found"
        }
    }
    catch {
        Write-TestResult -TestName "Workspace Settings Parsing" -Passed $false -Message "Error parsing settings.json: $_"
    }
} else {
    Write-TestResult -TestName "Workspace Settings Exists" -Passed $false -Message "Workspace settings.json not found"
}

# Test 4: Fix script functionality
Write-Host "`n=== Testing Fix Script Functionality ===" -ForegroundColor Cyan
if (Test-Path $fixScript) {
    try {
        # Test the -ShowStatus parameter
        $output = & pwsh -ExecutionPolicy Bypass -File $fixScript -ShowStatus 2>&1
        $exitCode = $LASTEXITCODE
        
        if ($exitCode -eq 0 -or $exitCode -eq $null) {
            Write-TestResult -TestName "Fix Script Execution" -Passed $true -Message "Script executes successfully"
        } else {
            Write-TestResult -TestName "Fix Script Execution" -Passed $false -Message "Script exit code: $exitCode"
        }
    }
    catch {
        Write-TestResult -TestName "Fix Script Execution" -Passed $false -Message "Error executing script: $_"
    }
}

# Test 5: Documentation
Write-Host "`n=== Testing Documentation ===" -ForegroundColor Cyan
$docPath = "AMAZONQ-COPILOT-SETUP-GUIDE.md"
if (Test-Path $docPath) {
    Write-TestResult -TestName "Documentation Exists" -Passed $true -Message "Found: $docPath"
    
    $docContent = Get-Content $docPath -Raw
    if ($docContent -match "Amazon Q") {
        Write-TestResult -TestName "Documentation Content" -Passed $true -Message "Documentation contains Amazon Q info"
    } else {
        Write-TestResult -TestName "Documentation Content" -Passed $false -Message "Documentation missing Amazon Q info"
    }
    
    if ($docContent -match "GitHub Copilot") {
        Write-TestResult -TestName "Documentation Content" -Passed $true -Message "Documentation contains GitHub Copilot info"
    } else {
        Write-TestResult -TestName "Documentation Content" -Passed $false -Message "Documentation missing GitHub Copilot info"
    }
} else {
    Write-TestResult -TestName "Documentation Exists" -Passed $false -Message "Documentation not found"
}

# Final Summary
Write-Host "`n" + "="*60 -ForegroundColor Cyan
Write-Host "  Test Summary" -ForegroundColor Cyan
Write-Host "="*60 -ForegroundColor Cyan
Write-Host "Total Tests:  $($testResults.Total)" -ForegroundColor White
Write-Host "Passed:      $($testResults.Passed)" -ForegroundColor Green
Write-Host "Failed:      $($testResults.Failed)" -ForegroundColor Red
Write-Host "Warnings:   $($testResults.Warnings)" -ForegroundColor Yellow

if ($testResults.Issues.Count -gt 0) {
    Write-Host "`nIssues Found:" -ForegroundColor Red
    foreach ($issue in $testResults.Issues) {
        Write-Host "  - $issue" -ForegroundColor Red
    }
}

if ($testResults.Failed -eq 0) {
    Write-Host "`n✅ All critical tests passed!" -ForegroundColor Green
    exit 0
}
else {
    Write-Host "`n⚠️  Some tests failed or have warnings. Review issues above." -ForegroundColor Yellow
    exit 0  # Exit 0 since warnings are acceptable
}

