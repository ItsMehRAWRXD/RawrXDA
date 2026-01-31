# Test-AIIntegrations.ps1
# Comprehensive test suite for all AI integration updates

$ErrorActionPreference = "Continue"
$TestResults = @()

function Write-TestResult {
    param(
        [string]$TestName,
        [bool]$Passed,
        [string]$Message = ""
    )
    
    $status = if ($Passed) { "✅ PASS" } else { "❌ FAIL" }
    $color = if ($Passed) { "Green" } else { "Red" }
    
    Write-Host "[$status] $TestName" -ForegroundColor $color
    if ($Message) {
        Write-Host "   $Message" -ForegroundColor Gray
    }
    
    $script:TestResults += [PSCustomObject]@{
        Test = $TestName
        Passed = $Passed
        Message = $Message
        Timestamp = Get-Date
    }
}

Write-Host "`n🧪 RawrXD AI Integration Test Suite" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan
Write-Host ""

# ============================================================
# TEST 1: Module File Existence
# ============================================================
Write-Host "📦 Testing Module Files..." -ForegroundColor Yellow

$modules = @(
    "Modules\RawrXD-AmazonQ.psm1",
    "Modules\RawrXD.GitHubCopilot.psm1",
    "Modules\RawrXD-AIAgentRouter.psm1"
)

foreach ($module in $modules) {
    $exists = Test-Path $module
    Write-TestResult -TestName "Module exists: $module" -Passed $exists -Message $(if ($exists) { "Found" } else { "Missing" })
}

# ============================================================
# TEST 2: Backend Server Script
# ============================================================
Write-Host "`n🔧 Testing Backend Server..." -ForegroundColor Yellow

$backendExists = Test-Path "AI-Backend-Server.ps1"
Write-TestResult -TestName "Backend server script exists" -Passed $backendExists

if ($backendExists) {
    $backendContent = Get-Content "AI-Backend-Server.ps1" -Raw
    $hasEndpoints = $backendContent -match "api/amazonq" -and $backendContent -match "api/copilot" -and $backendContent -match "api/openvsx"
    Write-TestResult -TestName "Backend server has all endpoints" -Passed $hasEndpoints
}

# ============================================================
# TEST 3: Documentation
# ============================================================
Write-Host "`n📚 Testing Documentation..." -ForegroundColor Yellow

$docExists = Test-Path "AI-INTEGRATION-SETUP-GUIDE.md"
Write-TestResult -TestName "Integration guide exists" -Passed $docExists

if ($docExists) {
    $docContent = Get-Content "AI-INTEGRATION-SETUP-GUIDE.md" -Raw
    $hasSections = $docContent -match "Amazon Q" -and $docContent -match "GitHub Copilot" -and $docContent -match "Open VSX"
    Write-TestResult -TestName "Documentation has all sections" -Passed $hasSections
}

# ============================================================
# TEST 4: Module Syntax Validation
# ============================================================
Write-Host "`n🔍 Testing Module Syntax..." -ForegroundColor Yellow

foreach ($module in $modules) {
    if (Test-Path $module) {
        try {
            $null = [System.Management.Automation.PSParser]::Tokenize((Get-Content $module -Raw), [ref]$null)
            Write-TestResult -TestName "Syntax valid: $module" -Passed $true
        }
        catch {
            Write-TestResult -TestName "Syntax valid: $module" -Passed $false -Message $_.Exception.Message
        }
    }
}

# ============================================================
# TEST 5: Amazon Q Module Functions
# ============================================================
Write-Host "`n🤖 Testing Amazon Q Module..." -ForegroundColor Yellow

if (Test-Path "Modules\RawrXD-AmazonQ.psm1") {
    try {
        $modulePath = (Resolve-Path "Modules\RawrXD-AmazonQ.psm1").Path
        Import-Module $modulePath -Force -ErrorAction Stop
        
        $functions = @(
            "Initialize-AmazonQ",
            "Connect-AmazonQWithProfile",
            "Connect-AmazonQWithCredentials",
            "Invoke-AmazonQChat",
            "Get-AmazonQCodeSuggestions",
            "Invoke-AmazonQCodeAnalysis"
        )
        
        foreach ($func in $functions) {
            $exists = Get-Command $func -ErrorAction SilentlyContinue
            Write-TestResult -TestName "Function exists: $func" -Passed ($null -ne $exists)
        }
        
        Remove-Module RawrXD-AmazonQ -ErrorAction SilentlyContinue
    }
    catch {
        Write-TestResult -TestName "Amazon Q module loads" -Passed $false -Message $_.Exception.Message
    }
}

# ============================================================
# TEST 6: GitHub Copilot Module Functions
# ============================================================
Write-Host "`n🐙 Testing GitHub Copilot Module..." -ForegroundColor Yellow

if (Test-Path "Modules\RawrXD.GitHubCopilot.psm1") {
    try {
        $modulePath = (Resolve-Path "Modules\RawrXD.GitHubCopilot.psm1").Path
        Import-Module $modulePath -Force -ErrorAction Stop
        
        $functions = @(
            "Initialize-GitHubCopilot",
            "Connect-GitHubCopilot",
            "Test-GitHubCopilotAuth",
            "Invoke-CopilotChat",
            "Get-CopilotCodeSuggestions"
        )
        
        foreach ($func in $functions) {
            $exists = Get-Command $func -ErrorAction SilentlyContinue
            Write-TestResult -TestName "Function exists: $func" -Passed ($null -ne $exists)
        }
        
        # Test OAuth parameter
        $help = Get-Help Connect-GitHubCopilot -ErrorAction SilentlyContinue
        $hasOAuth = $help -and ($help.Parameters.Parameter | Where-Object { $_.Name -eq "UseOAuth" })
        Write-TestResult -TestName "OAuth parameter exists" -Passed ($null -ne $hasOAuth)
        
        Remove-Module RawrXD.GitHubCopilot -ErrorAction SilentlyContinue
    }
    catch {
        Write-TestResult -TestName "GitHub Copilot module loads" -Passed $false -Message $_.Exception.Message
    }
}

# ============================================================
# TEST 7: AI Agent Router Module
# ============================================================
Write-Host "`n🎯 Testing AI Agent Router..." -ForegroundColor Yellow

if (Test-Path "Modules\RawrXD-AIAgentRouter.psm1") {
    try {
        $modulePath = (Resolve-Path "Modules\RawrXD-AIAgentRouter.psm1").Path
        Import-Module $modulePath -Force -ErrorAction Stop
        
        $functions = @(
            "Initialize-AIAgentRouter",
            "Invoke-AIChat",
            "Get-AICodeSuggestions",
            "Get-AIRouterStatus"
        )
        
        foreach ($func in $functions) {
            $exists = Get-Command $func -ErrorAction SilentlyContinue
            Write-TestResult -TestName "Function exists: $func" -Passed ($null -ne $exists)
        }
        
        Remove-Module RawrXD-AIAgentRouter -ErrorAction SilentlyContinue
    }
    catch {
        Write-TestResult -TestName "AI Router module loads" -Passed $false -Message $_.Exception.Message
    }
}

# ============================================================
# TEST 8: Open VSX Registry Integration
# ============================================================
Write-Host "`n📦 Testing Open VSX Registry Integration..." -ForegroundColor Yellow

if (Test-Path "RawrXD-Marketplace.psm1") {
    $marketplaceContent = Get-Content "RawrXD-Marketplace.psm1" -Raw
    
    $hasOpenVSX = $marketplaceContent -match "Get-OpenVSXRegistryExtensions"
    Write-TestResult -TestName "Open VSX function exists" -Passed $hasOpenVSX
    
    $hasIntegration = $marketplaceContent -match "openVSX" -and $marketplaceContent -match "Get-OpenVSXRegistryExtensions"
    Write-TestResult -TestName "Open VSX integrated in marketplace" -Passed $hasIntegration
}

# ============================================================
# TEST 9: WebView2 Login Settings
# ============================================================
Write-Host "`n🌐 Testing WebView2 Login Settings..." -ForegroundColor Yellow

if (Test-Path "RawrXD.ps1") {
    $rawrxdContent = Get-Content "RawrXD.ps1" -Raw
    
    $hasLoginSettings = $rawrxdContent -match "IsPasswordAutosaveEnabled" -and $rawrxdContent -match "IsGeneralAutofillEnabled"
    Write-TestResult -TestName "WebView2 login settings configured" -Passed $hasLoginSettings
    
    $hasLoginButtons = $rawrxdContent -match "rawrxd-login-buttons" -or $rawrxdContent -match "loginButtonsHtml"
    Write-TestResult -TestName "Quick login buttons added" -Passed $hasLoginButtons
}

# ============================================================
# TEST 10: PS51-Browser-Host Enhancements
# ============================================================
Write-Host "`n🎬 Testing PS51-Browser-Host Enhancements..." -ForegroundColor Yellow

if (Test-Path "PS51-Browser-Host.ps1") {
    $ps51Content = Get-Content "PS51-Browser-Host.ps1" -Raw
    
    $hasCookieSettings = $ps51Content -match "1A05" -and $ps51Content -match "third-party cookies"
    Write-TestResult -TestName "Cookie settings configured" -Passed $hasCookieSettings
    
    $hasAuthSettings = $ps51Content -match "1A02" -and $ps51Content -match "authentication"
    Write-TestResult -TestName "Authentication settings configured" -Passed $hasAuthSettings
    
    $hasLoginButtons = $ps51Content -match "Gmail|GitHub|ChatGPT" -or $ps51Content -match "quick.*login"
    Write-TestResult -TestName "Login buttons present" -Passed $hasLoginButtons
}

# ============================================================
# TEST 11: Marketplace Config
# ============================================================
Write-Host "`n🏪 Testing Marketplace Configuration..." -ForegroundColor Yellow

if (Test-Path "marketplace\marketplace-config.json") {
    try {
        $config = Get-Content "marketplace\marketplace-config.json" | ConvertFrom-Json
        
        $hasOpenVSX = $config.realMarketplaceSources.openVSX -ne $null
        Write-TestResult -TestName "Open VSX in marketplace config" -Passed $hasOpenVSX
        
        if ($hasOpenVSX) {
            $enabled = $config.realMarketplaceSources.openVSX.enabled
            Write-TestResult -TestName "Open VSX enabled in config" -Passed $enabled
        }
    }
    catch {
        Write-TestResult -TestName "Marketplace config valid JSON" -Passed $false -Message $_.Exception.Message
    }
}

# ============================================================
# TEST 12: Integration Test (Dry Run)
# ============================================================
Write-Host "`n🔄 Testing Integration (Dry Run)..." -ForegroundColor Yellow

try {
    # Test router initialization (without actually connecting)
    $modulePath = (Resolve-Path "Modules\RawrXD-AIAgentRouter.psm1").Path
    Import-Module $modulePath -Force -ErrorAction Stop
    
    # Check if router can be initialized (will fail gracefully without real credentials)
    $routerStatus = Get-AIRouterStatus -ErrorAction SilentlyContinue
    $routerWorks = $routerStatus -ne $null
    Write-TestResult -TestName "AI Router status function works" -Passed $routerWorks
    
    Remove-Module RawrXD-AIAgentRouter -ErrorAction SilentlyContinue
}
catch {
    Write-TestResult -TestName "Integration test" -Passed $false -Message $_.Exception.Message
}

# ============================================================
# SUMMARY
# ============================================================
Write-Host "`n" + "=" * 60 -ForegroundColor Cyan
Write-Host "📊 Test Summary" -ForegroundColor Cyan
Write-Host "=" * 60 -ForegroundColor Cyan

$total = $TestResults.Count
$passed = ($TestResults | Where-Object { $_.Passed }).Count
$failed = $total - $passed
$passRate = [math]::Round(($passed / $total) * 100, 1)

Write-Host "Total Tests: $total" -ForegroundColor White
Write-Host "Passed: $passed" -ForegroundColor Green
Write-Host "Failed: $failed" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Red" })
Write-Host "Pass Rate: $passRate%" -ForegroundColor $(if ($passRate -eq 100) { "Green" } else { "Yellow" })

if ($failed -gt 0) {
    Write-Host "`n❌ Failed Tests:" -ForegroundColor Red
    $TestResults | Where-Object { -not $_.Passed } | ForEach-Object {
        Write-Host "   - $($_.Test): $($_.Message)" -ForegroundColor Red
    }
}

Write-Host "`n✅ Test suite completed!" -ForegroundColor Green

# Export results
$TestResults | Export-Csv -Path "test-results.csv" -NoTypeInformation
Write-Host "`nResults exported to: test-results.csv" -ForegroundColor Gray

return $TestResults

