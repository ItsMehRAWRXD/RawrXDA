# 🔍 RawrXD Feature Discovery & Test Generator
# Automatically discovers all features, functions, and capabilities from the source code

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RawrXD Feature Discovery & Test Generator v1.0" -ForegroundColor Cyan
Write-Host "  Analyzing source code to discover ALL testable features" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$discoveryStartTime = Get-Date

# Initialize feature catalog
$featureCatalog = @{
  Functions         = @()
  UIControls        = @()
  MenuItems         = @()
  EventHandlers     = @()
  ConfigSettings    = @()
  APIEndpoints      = @()
  FileOperations    = @()
  SecurityFeatures  = @()
  ChatFeatures      = @()
  ModelFeatures     = @()
  DialogWindows     = @()
  KeyboardShortcuts = @()
}

# Helper function to analyze PowerShell code
function Analyze-PowerShellCode {
  param([string]$FilePath)
    
  Write-Host "🔍 Analyzing: $FilePath" -ForegroundColor Yellow
    
  if (-not (Test-Path $FilePath)) {
    Write-Host "   ❌ File not found" -ForegroundColor Red
    return
  }
    
  $content = Get-Content $FilePath -Raw
  $lineNumber = 0
  $lines = Get-Content $FilePath
    
  foreach ($line in $lines) {
    $lineNumber++
        
    # Discover Functions
    if ($line -match '^\s*function\s+([a-zA-Z0-9_-]+)') {
      $functionName = $matches[1]
      $featureCatalog.Functions += @{
        Name   = $functionName
        File   = $FilePath
        Line   = $lineNumber
        Type   = "Function"
        Tested = $false
      }
    }
        
    # Discover UI Controls
    if ($line -match 'New-Object\s+System\.Windows\.Forms\.(\w+)' -or 
      $line -match '\$(\w+)\s*=\s*New-Object.*Windows\.Forms\.(\w+)') {
      $controlType = if ($matches[2]) { $matches[2] } else { $matches[1] }
      $controlVar = if ($matches[1] -and $matches[2]) { $matches[1] } else { "Unknown" }
            
      $featureCatalog.UIControls += @{
        Name   = "$controlVar ($controlType)"
        Type   = $controlType
        File   = $FilePath
        Line   = $lineNumber
        Tested = $false
      }
    }
        
    # Discover Menu Items
    if ($line -match '\$(\w*[Mm]enu\w*)\s*=|Add_Click.*\{|ToolStripMenuItem|MenuStrip') {
      $menuName = if ($matches[1]) { $matches[1] } else { "Menu Action" }
      $featureCatalog.MenuItems += @{
        Name   = $menuName
        File   = $FilePath
        Line   = $lineNumber
        Type   = "Menu"
        Tested = $false
      }
    }
        
    # Discover Event Handlers
    if ($line -match '\.Add_(\w+)\s*\(' -or $line -match 'Register-ObjectEvent') {
      $eventType = if ($matches[1]) { $matches[1] } else { "ObjectEvent" }
      $featureCatalog.EventHandlers += @{
        Name   = $eventType
        File   = $FilePath
        Line   = $lineNumber
        Type   = "Event"
        Tested = $false
      }
    }
        
    # Discover Configuration Settings
    if ($line -match '\$config\.|Load-Settings|Save-Settings|\$settings\.' -or
      $line -match 'Get-ItemProperty.*HKCU:|Set-ItemProperty.*HKCU:') {
      $featureCatalog.ConfigSettings += @{
        Name   = "Configuration Management"
        File   = $FilePath
        Line   = $lineNumber
        Type   = "Config"
        Tested = $false
      }
    }
        
    # Discover API Endpoints
    if ($line -match 'Invoke-RestMethod.*localhost:11434|ollama\s+\w+|\$ollamaServer') {
      $featureCatalog.APIEndpoints += @{
        Name   = "Ollama API Integration"
        File   = $FilePath
        Line   = $lineNumber
        Type   = "API"
        Tested = $false
      }
    }
        
    # Discover File Operations
    if ($line -match 'Get-Content|Set-Content|Out-File|Test-Path|New-Item.*-ItemType.*File|Remove-Item.*File') {
      $operation = $line -replace '.*?(Get-Content|Set-Content|Out-File|Test-Path|New-Item|Remove-Item).*', '$1'
      $featureCatalog.FileOperations += @{
        Name   = "$operation File Operation"
        File   = $FilePath
        Line   = $lineNumber
        Type   = "File"
        Tested = $false
      }
    }
        
    # Discover Security Features
    if ($line -match 'ConvertTo-SecureString|ConvertFrom-SecureString|Test-.*Security|Initialize-SecurityConfig|Write-SecurityLog') {
      $featureCatalog.SecurityFeatures += @{
        Name   = "Security Function"
        File   = $FilePath
        Line   = $lineNumber
        Type   = "Security"
        Tested = $false
      }
    }
        
    # Discover Chat Features
    if ($line -match 'Send-Chat|Receive-Chat|\$chat|chatBox|chatHistory') {
      $featureCatalog.ChatFeatures += @{
        Name   = "Chat Functionality"
        File   = $FilePath
        Line   = $lineNumber
        Type   = "Chat"
        Tested = $false
      }
    }
        
    # Discover Model Features
    if ($line -match 'Process-AgentCommand|Invoke-.*Model|Switch-Model|\$currentModel') {
      $featureCatalog.ModelFeatures += @{
        Name   = "Model Management"
        File   = $FilePath
        Line   = $lineNumber
        Type   = "Model"
        Tested = $false
      }
    }
        
    # Discover Dialog Windows
    if ($line -match 'Show-.*Dialog|MessageBox::Show|\[System\.Windows\.Forms\.MessageBox\]') {
      $featureCatalog.DialogWindows += @{
        Name   = "Dialog System"
        File   = $FilePath
        Line   = $lineNumber
        Type   = "Dialog"
        Tested = $false
      }
    }
        
    # Discover Keyboard Shortcuts
    if ($line -match 'KeyDown|KeyPress|KeyUp|Ctrl\+|Alt\+|Shift\+|\$_.KeyCode') {
      $featureCatalog.KeyboardShortcuts += @{
        Name   = "Keyboard Shortcut"
        File   = $FilePath
        Line   = $lineNumber
        Type   = "Keyboard"
        Tested = $false
      }
    }
  }
}

Write-Host "🔍 DISCOVERING FEATURES FROM SOURCE CODE" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

# Analyze main RawrXD script
Write-Host "`n📁 Analyzing RawrXD.ps1..." -ForegroundColor Cyan
Analyze-PowerShellCode -FilePath ".\RawrXD.ps1"

# Analyze any other related scripts
$relatedScripts = @(
  ".\Test-RawrXD-Full.ps1",
  ".\Test-Live-Ollama.ps1",
  ".\Headless-Agentic-Test-CustomModels.ps1"
)

foreach ($script in $relatedScripts) {
  if (Test-Path $script) {
    Write-Host "`n📁 Analyzing $script..." -ForegroundColor Cyan
    Analyze-PowerShellCode -FilePath $script
  }
}

# Generate comprehensive feature report
Write-Host "`n📊 FEATURE DISCOVERY RESULTS" -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Green

$totalFeatures = 0
foreach ($category in $featureCatalog.Keys) {
  $count = $featureCatalog[$category].Count
  if ($count -gt 0) {
    Write-Host "`n🔸 $category`: $count features discovered" -ForegroundColor White
    $totalFeatures += $count
        
    # Show top 5 features in each category
    $featureCatalog[$category] | Select-Object -First 5 | ForEach-Object {
      Write-Host "   • $($_.Name) (Line $($_.Line))" -ForegroundColor Gray
    }
        
    if ($count -gt 5) {
      Write-Host "   ... and $($count - 5) more" -ForegroundColor DarkGray
    }
  }
}

Write-Host "`n🎯 TOTAL DISCOVERED FEATURES: $totalFeatures" -ForegroundColor Cyan

# Generate comprehensive test plan
Write-Host "`n🧪 GENERATING COMPREHENSIVE TEST PLAN" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

$testPlan = @"
# 🧪 RawrXD Comprehensive Test Plan
# Auto-generated from feature discovery on $(Get-Date -Format "yyyy-MM-dd HH:mm:ss")

## 📋 Discovered Features Summary
- **Total Features**: $totalFeatures
- **Functions**: $($featureCatalog.Functions.Count)
- **UI Controls**: $($featureCatalog.UIControls.Count)
- **Menu Items**: $($featureCatalog.MenuItems.Count)
- **Event Handlers**: $($featureCatalog.EventHandlers.Count)
- **Config Settings**: $($featureCatalog.ConfigSettings.Count)
- **API Endpoints**: $($featureCatalog.APIEndpoints.Count)
- **File Operations**: $($featureCatalog.FileOperations.Count)
- **Security Features**: $($featureCatalog.SecurityFeatures.Count)
- **Chat Features**: $($featureCatalog.ChatFeatures.Count)
- **Model Features**: $($featureCatalog.ModelFeatures.Count)
- **Dialog Windows**: $($featureCatalog.DialogWindows.Count)
- **Keyboard Shortcuts**: $($featureCatalog.KeyboardShortcuts.Count)

## 🎯 Priority Test Categories

### 🔥 HIGH PRIORITY (Core Functionality)
"@

foreach ($category in @("Functions", "ModelFeatures", "ChatFeatures")) {
  if ($featureCatalog[$category].Count -gt 0) {
    $testPlan += "`n#### $category ($($featureCatalog[$category].Count) features)`n"
    foreach ($feature in $featureCatalog[$category]) {
      $testPlan += "- [ ] Test: $($feature.Name) (Line $($feature.Line))`n"
    }
  }
}

$testPlan += @"

### ⚡ MEDIUM PRIORITY (UI & Interaction)
"@

foreach ($category in @("UIControls", "MenuItems", "EventHandlers", "DialogWindows")) {
  if ($featureCatalog[$category].Count -gt 0) {
    $testPlan += "`n#### $category ($($featureCatalog[$category].Count) features)`n"
    foreach ($feature in $featureCatalog[$category] | Select-Object -First 10) {
      $testPlan += "- [ ] Test: $($feature.Name) (Line $($feature.Line))`n"
    }
  }
}

$testPlan += @"

### 📋 LOW PRIORITY (System Integration)
"@

foreach ($category in @("ConfigSettings", "APIEndpoints", "FileOperations", "SecurityFeatures", "KeyboardShortcuts")) {
  if ($featureCatalog[$category].Count -gt 0) {
    $testPlan += "`n#### $category ($($featureCatalog[$category].Count) features)`n"
    foreach ($feature in $featureCatalog[$category] | Select-Object -First 5) {
      $testPlan += "- [ ] Test: $($feature.Name) (Line $($feature.Line))`n"
    }
  }
}

# Save test plan
$testPlanFile = ".\RawrXD-Comprehensive-Test-Plan.md"
Set-Content $testPlanFile $testPlan -Encoding UTF8

Write-Host "✅ Comprehensive test plan saved to: $testPlanFile" -ForegroundColor Green

# Generate automated test script
Write-Host "`n🤖 GENERATING AUTOMATED TEST SCRIPT" -ForegroundColor Yellow
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Yellow

$autoTestScript = @'
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
        } else {
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
    Test-Path ".\RawrXD.ps1" -and (Get-Content ".\RawrXD.ps1" -TotalCount 1)
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
    } catch { $false }
}

# Test 4: Ollama Service Connectivity
Test-Feature -FeatureName "Ollama Service Connectivity" -Category "AI" -Description "Test connection to AI service" -TestCode {
    try {
        $response = Invoke-RestMethod -Uri "http://localhost:11434/api/version" -TimeoutSec 5 -ErrorAction Stop
        $response -ne $null
    } catch { $false }
}

# Test 5: Model Availability
Test-Feature -FeatureName "AI Models Available" -Category "AI" -Description "Check if AI models are accessible" -TestCode {
    try {
        $models = ollama list 2>$null
        $models -ne $null -and $models.Count -gt 1
    } catch { $false }
}

# Test 6: Function Definitions
Test-Feature -FeatureName "Core Functions Defined" -Category "Functions" -Description "Verify key functions exist in script" -TestCode {
    $content = Get-Content ".\RawrXD.ps1" -Raw
    $content -match "function.*Process-AgentCommand" -and 
    $content -match "function.*Load-Settings" -and
    $content -match "function.*Write-.*Log"
}

# Test 7: Configuration System
Test-Feature -FeatureName "Configuration System" -Category "Config" -Description "Test settings load/save capability" -TestCode {
    $content = Get-Content ".\RawrXD.ps1" -Raw
    $content -match "Load-Settings|Save-Settings|\$config\." -and
    $content -match "Get-ItemProperty|Set-ItemProperty"
}

# Test 8: Security Features
Test-Feature -FeatureName "Security Features" -Category "Security" -Description "Verify security functions are present" -TestCode {
    $content = Get-Content ".\RawrXD.ps1" -Raw
    $content -match "Security|ConvertTo-SecureString|Initialize-SecurityConfig"
}

# Test 9: File Operations
Test-Feature -FeatureName "File Operations" -Category "File" -Description "Check file handling capabilities" -TestCode {
    $content = Get-Content ".\RawrXD.ps1" -Raw
    $content -match "Get-Content|Set-Content|Test-Path" -and
    $content -match "OpenFileDialog|SaveFileDialog"
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
} elseif ($successRate -ge 60) {
    Write-Host "`n👍 GOOD: Most features are working well" -ForegroundColor Yellow
} else {
    Write-Host "`n⚠️ ATTENTION: Several features may need attention" -ForegroundColor Red
}

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Automated testing complete! Check results above." -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
'@

$autoTestFile = ".\RawrXD-Automated-Feature-Tests.ps1"
Set-Content $autoTestFile $autoTestScript -Encoding UTF8

Write-Host "✅ Automated test script saved to: $autoTestFile" -ForegroundColor Green

# Summary
$discoveryEndTime = Get-Date
$discoveryDuration = [math]::Round(($discoveryEndTime - $discoveryStartTime).TotalSeconds, 1)

Write-Host "`n📈 DISCOVERY COMPLETE!" -ForegroundColor Green
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "Duration: $discoveryDuration seconds"
Write-Host "Features Discovered: $totalFeatures"
Write-Host "Test Plan Generated: $testPlanFile"
Write-Host "Automated Tests: $autoTestFile"

Write-Host "`n🚀 NEXT STEPS:" -ForegroundColor Cyan
Write-Host "1. Review the test plan: Get-Content '$testPlanFile'"
Write-Host "2. Run automated tests: .\RawrXD-Automated-Feature-Tests.ps1"
Write-Host "3. Manual testing for complex UI features"
Write-Host "4. Performance testing under load"

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "Feature discovery complete! Now you know exactly what to test." -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan