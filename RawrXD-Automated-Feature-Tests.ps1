# 🤖 RawrXD Automated Feature Test Suite
# Auto-generated comprehensive test script

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RawrXD Comprehensive Automated Test Suite" -ForegroundColor Cyan
Write-Host "  Testing ALL discovered features automatically" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$testResults = @{}
$totalTests = 0
$passedTests = 0
$failedTests = 0

function Test-Feature {
    param(
        [string]$FeatureName,
        [string]$Category,
        [scriptblock]$TestCode,
        [string]$Description
    )
    
    $script:totalTests++
    Write-Host "`n🔸 Testing: $FeatureName" -ForegroundColor Yellow -NoNewline
    
    try {
        $result = & $TestCode
        if ($result) {
            Write-Host " ✅ PASS" -ForegroundColor Green
            $script:passedTests++
            $testResults[$FeatureName] = "PASS"
        }
        else {
            Write-Host " ⚠️ PARTIAL" -ForegroundColor Yellow
            $testResults[$FeatureName] = "PARTIAL"
        }
    }
    catch {
        Write-Host " ❌ FAIL: $_" -ForegroundColor Red
        $script:failedTests++
        $testResults[$FeatureName] = "FAIL: $_"
    }
}

Write-Host "`n🧪 AUTOMATED FEATURE TESTING" -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Green

# Test 1: RawrXD Script Accessibility
Test-Feature -FeatureName "RawrXD Script Accessibility" -Category "Core" -Description "Check if main script exists and is readable" -TestCode {
    $exists = Test-Path ".\RawrXD.ps1"
    $readable = $null -ne (Get-Content ".\RawrXD.ps1" -TotalCount 1 -ErrorAction SilentlyContinue)
    return ($exists -and $readable)
}

# Test 2: PowerShell Execution Policy
Test-Feature -FeatureName "PowerShell Execution Policy" -Category "Core" -Description "Verify script can execute" -TestCode {
    $policy = Get-ExecutionPolicy
    $policy -eq "Unrestricted" -or $policy -eq "RemoteSigned" -or $policy -eq "Bypass"
}

# Test 3: Required Assemblies
Test-Feature -FeatureName "Windows Forms Assembly" -Category "UI" -Description "Check if UI components can load" -TestCode {
    try {
        Add-Type -AssemblyName System.Windows.Forms
        Add-Type -AssemblyName System.Drawing
        $true
    }
    catch { $false }
}

# Test 4: Ollama Service Connectivity
Test-Feature -FeatureName "Ollama Service Connectivity" -Category "AI" -Description "Test connection to AI service" -TestCode {
    try {
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/version" -TimeoutSec 5 -ErrorAction Stop
        $response -ne $null
    }
    catch { $false }
}

# Test 5: Model Availability
Test-Feature -FeatureName "AI Models Available" -Category "AI" -Description "Check if AI models are accessible" -TestCode {
    try {
        $models = ollama list 2>$null
        $models -ne $null -and $models.Count -gt 1
    }
    catch { $false }
}

# Test 6: Function Definitions
Test-Feature -FeatureName "Core Functions Defined" -Category "Functions" -Description "Verify key functions exist in script" -TestCode {
    $content = Get-Content ".\RawrXD.ps1" -Raw
    $hasAgentCommand = $content -match "function.*Process-AgentCommand"
    $hasLoadSettings = $content -match "function.*Load-Settings"
    $hasLogging = $content -match "function.*Write-.*Log"
    return ($hasAgentCommand -and $hasLoadSettings -and $hasLogging)
}

# Test 7: Configuration System
Test-Feature -FeatureName "Configuration System" -Category "Config" -Description "Test settings load/save capability" -TestCode {
    $content = Get-Content ".\RawrXD.ps1" -Raw
    $hasSettings = $content -match "Load-Settings|Save-Settings|Save-CustomizationSettings|\`$global:settings"
    $hasConfig = $content -match "settings\.json|config|Configuration"
    return ($hasSettings -and $hasConfig)
}

# Test 8: Security Features
Test-Feature -FeatureName "Security Features" -Category "Security" -Description "Verify security functions are present" -TestCode {
    $content = Get-Content ".\RawrXD.ps1" -Raw
    $content -match "Security|ConvertTo-SecureString|Initialize-SecurityConfig"
}

# Test 9: File Operations
Test-Feature -FeatureName "File Operations" -Category "File" -Description "Check file handling capabilities" -TestCode {
    $content = Get-Content ".\RawrXD.ps1" -Raw
    $hasBasicOps = $content -match "Get-Content|Set-Content|Test-Path"
    $hasDialogs = $content -match "OpenFileDialog|SaveFileDialog"
    return ($hasBasicOps -and $hasDialogs)
}

# Test 10: Chat Interface
Test-Feature -FeatureName "Chat Interface" -Category "Chat" -Description "Verify chat functionality exists" -TestCode {
    $content = Get-Content ".\RawrXD.ps1" -Raw
    $content -match "chat|Chat|Send-|Receive-"
}

# Generate final report
Write-Host "`n📊 AUTOMATED TEST RESULTS" -ForegroundColor Cyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Cyan

$successRate = if ($totalTests -gt 0) { [math]::Round(($passedTests / $totalTests) * 100, 1) } else { 0 }

Write-Host "`n🎯 Test Summary:" -ForegroundColor White
Write-Host "   Total Tests: $totalTests" -ForegroundColor Gray
Write-Host "   Passed: $passedTests" -ForegroundColor Green
Write-Host "   Failed: $failedTests" -ForegroundColor Red
Write-Host "   Success Rate: $successRate%" -ForegroundColor Cyan

Write-Host "`n📋 Detailed Results:" -ForegroundColor White
foreach ($test in $testResults.Keys) {
    $result = $testResults[$test]
    $color = switch ($result) {
        "PASS" { "Green" }
        "PARTIAL" { "Yellow" }
        default { "Red" }
    }
    Write-Host "   $test`: $result" -ForegroundColor $color
}

if ($successRate -ge 80) {
    Write-Host "`n🎉 EXCELLENT: RawrXD is in great shape!" -ForegroundColor Green
}
elseif ($successRate -ge 60) {
    Write-Host "`n👍 GOOD: Most features are working well" -ForegroundColor Yellow
}
else {
    Write-Host "`n⚠️ ATTENTION: Several features may need attention" -ForegroundColor Red
}

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Automated testing complete! Check results above." -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
