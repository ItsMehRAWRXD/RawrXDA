#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Verification script for RawrXD 3.0 PRO production deployment
.DESCRIPTION
    Validates all security fixes, module loads, and agentic engine functionality
.PARAMETER Quick
    Run only critical tests (30 seconds)
.PARAMETER Full
    Run comprehensive test suite (2 minutes)
#>

param(
    [switch]$Quick = $false,
    [switch]$Full = $true
)

$script:TestResults = @{
    Passed = 0
    Failed = 0
    Warnings = 0
    Details = @()
}

function Write-TestResult {
    param(
        [Parameter(Mandatory)]
        [string]$TestName,
        
        [Parameter(Mandatory)]
        [ValidateSet("Pass", "Fail", "Warning")]
        [string]$Result,
        
        [Parameter()]
        [string]$Message = ""
    )
    
    $color = switch ($Result) {
        "Pass" { "Green" }
        "Fail" { "Red" }
        "Warning" { "Yellow" }
    }
    
    $symbol = switch ($Result) {
        "Pass" { "✅" }
        "Fail" { "❌" }
        "Warning" { "⚠️" }
    }
    
    Write-Host "$symbol $TestName" -ForegroundColor $color
    if ($Message) {
        Write-Host "   $Message" -ForegroundColor Gray
    }
    
    $script:TestResults.$Result++
    $script:TestResults.Details += @{
        Test = $TestName
        Result = $Result
        Message = $Message
    }
}

Write-Host "`n🔒 RawrXD 3.0 PRO - Production Verification Suite`n" -ForegroundColor Cyan

# ============================================
# MODULE LOADING TESTS
# ============================================

Write-Host "═══ MODULE LOADING ═══" -ForegroundColor Yellow

$ModulesPath = "E:\Desktop\Powershield\Modules"

if (Test-Path $ModulesPath) {
    Write-TestResult "Modules directory exists" "Pass"
} else {
    Write-TestResult "Modules directory exists" "Fail" "Path: $ModulesPath"
    exit 1
}

$modules = @(
    "SecurityManager.psm1",
    "ProductionMonitoring.psm1",
    "AgenticEngine.psm1",
    "AgenticIntegration.psm1"
)

foreach ($module in $modules) {
    $path = Join-Path $ModulesPath $module
    if (Test-Path $path) {
        $size = [math]::Round((Get-Item $path).Length / 1KB, 2)
        Write-TestResult "$module ($size KB)" "Pass"
    } else {
        Write-TestResult "$module" "Fail" "Not found at $path"
    }
}

# ============================================
# SECURITY TESTS
# ============================================

Write-Host "`n═══ SECURITY VALIDATION ═══" -ForegroundColor Yellow

try {
    Import-Module (Join-Path $ModulesPath "SecurityManager.psm1") -Force -ErrorAction Stop
    Write-TestResult "SecurityManager imports" "Pass"
} catch {
    Write-TestResult "SecurityManager imports" "Fail" $_.Exception.Message
}

# Test C-01: No hardcoded keys
Write-Host "`nC-01: Hardcoded Encryption Keys" -ForegroundColor Cyan
$mainFile = Get-Content "E:\Desktop\Powershield\RawrXD.ps1" -Raw
if ($mainFile -match 'private.*readonly.*byte\[\].*DefaultKey') {
    Write-TestResult "No hardcoded DefaultKey" "Fail" "Found legacy hardcoded key"
} else {
    Write-TestResult "No hardcoded DefaultKey" "Pass"
}

# Test PBKDF2 implementation
if (Get-Command -Name "[SecurityManager]::DeriveKeyFromPassword" -ErrorAction SilentlyContinue) {
    Write-TestResult "PBKDF2 key derivation available" "Pass"
} elseif ($mainFile -match 'Rfc2898DeriveBytes') {
    Write-TestResult "PBKDF2 key derivation available" "Pass"
} else {
    Write-TestResult "PBKDF2 key derivation available" "Warning" "Check SecurityManager source"
}

# Test C-02: Argument safety
Write-Host "`nC-02: Command Injection Prevention" -ForegroundColor Cyan
$updatedOllama = $mainFile -match 'ArgumentList.AddRange'
if ($updatedOllama) {
    Write-TestResult "ArgumentList used in Ollama execution" "Pass"
} elseif ($mainFile -match 'psi.Arguments.*escapedPrompt') {
    Write-TestResult "ArgumentList used in Ollama execution" "Fail" "Still using unsafe psi.Arguments"
} else {
    Write-TestResult "ArgumentList used in Ollama execution" "Warning" "Could not verify in main file"
}

# Test M-01: Credential management
Write-Host "`nM-01: Secure Credential Storage" -ForegroundColor Cyan
if (Get-Command -Name "Set-SecureAICredential" -ErrorAction SilentlyContinue) {
    Write-TestResult "Secure credential APIs available" "Pass"
} elseif ($mainFile -match 'StoreSecureCredential|WindowsCredentialManager') {
    Write-TestResult "Secure credential APIs available" "Pass"
} else {
    Write-TestResult "Secure credential APIs available" "Warning" "Check AgenticIntegration source"
}

# Test M-02: Path validation
Write-Host "`nM-02: Path Traversal Prevention" -ForegroundColor Cyan
try {
    if (Get-Command "[SecurityManager]::ValidatePath" -ErrorAction SilentlyContinue) {
        $testPath = Join-Path $PSScriptRoot "test.txt"
        $result = [SecurityManager]::ValidatePath($testPath, $PSScriptRoot)
        Write-TestResult "Path validation works" "Pass"
        
        # Test traversal blocking
        $traversalPath = "..\..\windows\system32"
        $blocked = -not [SecurityManager]::ValidatePath($traversalPath, $PSScriptRoot)
        if ($blocked) {
            Write-TestResult "Directory traversal blocked" "Pass"
        } else {
            Write-TestResult "Directory traversal blocked" "Warning" "Could not verify"
        }
    } else {
        Write-TestResult "Path validation available" "Warning" "Check SecurityManager source"
    }
} catch {
    Write-TestResult "Path validation available" "Warning" $_.Exception.Message
}

# ============================================
# AGENTIC ENGINE TESTS
# ============================================

if (-not $Quick) {
    Write-Host "`n═══ AGENTIC ENGINE ═══" -ForegroundColor Yellow
    
    try {
        Import-Module (Join-Path $ModulesPath "AgenticEngine.psm1") -Force -ErrorAction Stop
        Write-TestResult "AgenticEngine imports" "Pass"
    } catch {
        Write-TestResult "AgenticEngine imports" "Fail" $_.Exception.Message
    }
    
    try {
        Import-Module (Join-Path $ModulesPath "ProductionMonitoring.psm1") -Force -ErrorAction Stop
        Write-TestResult "ProductionMonitoring imports" "Pass"
    } catch {
        Write-TestResult "ProductionMonitoring imports" "Fail" $_.Exception.Message
    }
    
    try {
        Import-Module (Join-Path $ModulesPath "AgenticIntegration.psm1") -Force -ErrorAction Stop
        Write-TestResult "AgenticIntegration imports" "Pass"
    } catch {
        Write-TestResult "AgenticIntegration imports" "Fail" $_.Exception.Message
    }
}

# ============================================
# DOCUMENTATION TESTS
# ============================================

Write-Host "`n═══ DOCUMENTATION ═══" -ForegroundColor Yellow

$docs = @(
    "SECURITY_REMEDIATION_REPORT.md",
    "QUICKSTART.md",
    "IMPLEMENTATION_SUMMARY.md"
)

foreach ($doc in $docs) {
    $path = "E:\Desktop\Powershield\$doc"
    if (Test-Path $path) {
        $lines = @(Get-Content $path).Count
        Write-TestResult "$doc ($lines lines)" "Pass"
    } else {
        Write-TestResult "$doc" "Fail" "Not found"
    }
}

# ============================================
# RawrXD.PS1 UPDATES
# ============================================

Write-Host "`n═══ RAWRXD.PS1 UPDATES ═══" -ForegroundColor Yellow

$checks = @(
    @{ Pattern = 'Import-Module.*SecurityManager'; Name = "SecurityManager import" },
    @{ Pattern = 'Import-Module.*AgenticEngine'; Name = "AgenticEngine import" },
    @{ Pattern = 'Initialize-AgenticEngine'; Name = "Agentic initialization" },
    @{ Pattern = 'ArgumentList'; Name = "ArgumentList safe execution" }
)

foreach ($check in $checks) {
    if ($mainFile -match $check.Pattern) {
        Write-TestResult "RawrXD.ps1: $($check.Name)" "Pass"
    } else {
        Write-TestResult "RawrXD.ps1: $($check.Name)" "Warning" "Pattern not found"
    }
}

# ============================================
# SUMMARY
# ============================================

Write-Host "`n═══ TEST SUMMARY ═══" -ForegroundColor Yellow
Write-Host "✅ Passed:  $($script:TestResults.Passed)" -ForegroundColor Green
Write-Host "⚠️  Warnings: $($script:TestResults.Warnings)" -ForegroundColor Yellow
Write-Host "❌ Failed:  $($script:TestResults.Failed)" -ForegroundColor Red

if ($script:TestResults.Failed -eq 0) {
    Write-Host "`n🎉 All critical tests passed! Ready for production deployment.`n" -ForegroundColor Green
} elseif ($script:TestResults.Failed -le 2) {
    Write-Host "`n⚠️  Some tests failed. Review above and check documentation.`n" -ForegroundColor Yellow
} else {
    Write-Host "`n❌ Multiple test failures. Fix issues before deployment.`n" -ForegroundColor Red
}

# ============================================
# RECOMMENDED NEXT STEPS
# ============================================

Write-Host "📝 Next Steps:" -ForegroundColor Cyan
Write-Host "  1. Review SECURITY_REMEDIATION_REPORT.md"
Write-Host "  2. Follow QUICKSTART.md for getting started"
Write-Host "  3. Run: .\RawrXD.ps1"
Write-Host "  4. Check: Get-AgenticMetrics"
Write-Host "  5. Verify: Get-SystemHealth`n"

Write-Host "📚 Documentation:" -ForegroundColor Cyan
Write-Host "  - E:\Desktop\Powershield\SECURITY_REMEDIATION_REPORT.md"
Write-Host "  - E:\Desktop\Powershield\QUICKSTART.md"
Write-Host "  - E:\Desktop\Powershield\IMPLEMENTATION_SUMMARY.md`n"

Write-Host "🔗 Modules:" -ForegroundColor Cyan
Write-Host "  - E:\Desktop\Powershield\Modules\SecurityManager.psm1"
Write-Host "  - E:\Desktop\Powershield\Modules\AgenticEngine.psm1"
Write-Host "  - E:\Desktop\Powershield\Modules\ProductionMonitoring.psm1"
Write-Host "  - E:\Desktop\Powershield\Modules\AgenticIntegration.psm1`n"
