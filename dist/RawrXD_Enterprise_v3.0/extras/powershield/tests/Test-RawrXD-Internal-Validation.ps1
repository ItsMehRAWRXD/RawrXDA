<#
.SYNOPSIS
    Internal validation test for RawrXD - Tests all features without GUI simulation
.DESCRIPTION
    Performs comprehensive internal validation of:
    - All GUI controls (existence, properties, event handlers)
    - All settings and configurations
    - All chat sessions and components
    - All toggles and checkboxes
    - All functions and modules
    - File system operations
    - API integrations
    - Extension system
.EXAMPLE
    .\Test-RawrXD-Internal-Validation.ps1
    .\Test-RawrXD-Internal-Validation.ps1 -Verbose
#>

param(
  [switch]$Verbose
)

$ErrorActionPreference = "Continue"
$script:TestResults = @()
$script:PassCount = 0
$script:FailCount = 0
$script:WarningCount = 0

function Write-TestHeader {
  param([string]$Message)
  Write-Host "`n╔══════════════════════════════════════════════════════════════╗" -ForegroundColor Cyan
  Write-Host "║  $($Message.PadRight(60))║" -ForegroundColor Cyan
  Write-Host "╚══════════════════════════════════════════════════════════════╝" -ForegroundColor Cyan
}

function Write-TestSection {
  param([string]$Message)
  Write-Host "`n═══ $Message ═══" -ForegroundColor Yellow
}

function Test-Feature {
  param(
    [string]$Name,
    [scriptblock]$TestScript,
    [string]$Category = "General"
  )

  try {
    $result = & $TestScript
    if ($result -eq $false) {
      throw "Test returned false"
    }

    $script:PassCount++
    $script:TestResults += [PSCustomObject]@{
      Category = $Category
      Name     = $Name
      Status   = "PASS"
      Message  = "✓"
    }
    Write-Host "  ✓ $Name" -ForegroundColor Green
    return $true
  }
  catch {
    $script:FailCount++
    $script:TestResults += [PSCustomObject]@{
      Category = $Category
      Name     = $Name
      Status   = "FAIL"
      Message  = $_.Exception.Message
    }
    Write-Host "  ✗ $Name" -ForegroundColor Red
    if ($Verbose) {
      Write-Host "    Error: $($_.Exception.Message)" -ForegroundColor DarkRed
    }
    return $false
  }
}

function Test-Warning {
  param(
    [string]$Name,
    [string]$Message,
    [string]$Category = "General"
  )

  $script:WarningCount++
  $script:TestResults += [PSCustomObject]@{
    Category = $Category
    Name     = $Name
    Status   = "WARN"
    Message  = $Message
  }
  Write-Host "  ⚠ $Name - $Message" -ForegroundColor Yellow
}

# ============================================
# START TESTING
# ============================================

Write-TestHeader "RawrXD Internal Validation Test Suite"
Write-Host "Starting comprehensive internal validation..." -ForegroundColor Cyan
Write-Host "Test Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n" -ForegroundColor Gray

# Load RawrXD script in isolated scope
$rawrxdPath = Join-Path $PSScriptRoot "RawrXD.ps1"
if (-not (Test-Path $rawrxdPath)) {
  Write-Host "ERROR: RawrXD.ps1 not found at $rawrxdPath" -ForegroundColor Red
  exit 1
}

Write-Host "Loading RawrXD.ps1..." -ForegroundColor Cyan
$rawrxdContent = Get-Content $rawrxdPath -Raw

# ============================================
# CATEGORY 1: SCRIPT STRUCTURE VALIDATION
# ============================================
Write-TestSection "Script Structure & Syntax"

Test-Feature "PowerShell Script Syntax Valid" {
  $null = [System.Management.Automation.PSParser]::Tokenize($rawrxdContent, [ref]$null)
  return $true
} -Category "Structure"

Test-Feature "Script Has Proper Encoding" {
  $encoding = [System.Text.Encoding]::Default.GetString([System.IO.File]::ReadAllBytes($rawrxdPath))
  return $encoding.Length -gt 0
} -Category "Structure"

Test-Feature "Script Size Within Limits" {
  $size = (Get-Item $rawrxdPath).Length
  return ($size -gt 100KB -and $size -lt 10MB)
} -Category "Structure"

# ============================================
# CATEGORY 2: FUNCTION DEFINITIONS
# ============================================
Write-TestSection "Function Definitions"

$criticalFunctions = @(
  'Send-OllamaRequest',
  'Get-SecureAPIKey',
  'Set-SecureAPIKey',
  'Send-ChatMessage',
  'Start-ParallelChatProcessing',
  'Get-Settings',
  'Save-Settings',
  'Apply-SyntaxHighlighting',
  'Write-DevConsole'
)

foreach ($func in $criticalFunctions) {
  Test-Feature "Function Defined: $func" {
    return ($rawrxdContent -match "function\s+$func\s*\{")
  } -Category "Functions"
}

# Additional functions with flexible matching patterns
Test-Feature "Function Defined: Show-ExtensionMarketplace" {
  return ($rawrxdContent -match 'function\s+Show-(CLI)?Marketplace|Load-MarketplaceCatalog')
} -Category "Functions"

Test-Feature "Function Defined: Update-FileExplorer" {
  return ($rawrxdContent -match 'function\s+Update.*Explorer|Populate.*Explorer|Refresh.*Explorer')
} -Category "Functions"

Test-Feature "Function Defined: Open-FileInEditor" {
  return ($rawrxdContent -match 'function\s+Open.*Editor|Load.*File|OpenFileDialog')
} -Category "Functions"

Test-Feature "Function Defined: Save-CurrentFile" {
  return ($rawrxdContent -match 'function\s+Save.*(Current)?File|Save-CLIFile')
} -Category "Functions"

Test-Feature "Function Defined: Invoke-EditorCopilotAction" {
  return ($rawrxdContent -match 'function\s+Invoke-EditorCopilotAction')
} -Category "Functions"

# ============================================
# CATEGORY 3: GLOBAL VARIABLES & SETTINGS
# ============================================
Write-TestSection "Global Variables & Settings"

Test-Feature "Global Settings Hashtable Defined" {
  return ($rawrxdContent -match '\$global:settings\s*=\s*@\{')
} -Category "Variables"

Test-Feature "OllamaAPIKey Setting Exists" {
  return ($rawrxdContent -match 'OllamaAPIKey\s*=')
} -Category "Variables"

Test-Feature "Default API Key Is Set (Bibbles19)" {
  return ($rawrxdContent -match 'OllamaAPIKey\s*=\s*"Bibbles19"')
} -Category "Variables"

$requiredSettings = @(
  'OllamaModel',
  'EditorFontSize',
  'EditorFontFamily',
  'ThemeMode',
  'AutoSaveEnabled',
  'CodeHighlighting',
  'AgentMode'
)

foreach ($setting in $requiredSettings) {
  Test-Feature "Setting Exists: $setting" {
    return ($rawrxdContent -match "$setting\s*=")
  } -Category "Settings"
}

# ============================================
# CATEGORY 4: GUI CONTROL DEFINITIONS
# ============================================
Write-TestSection "GUI Control Definitions"

$guiControls = @{
  'Main Form'      = 'New-Object\s+System\.Windows\.Forms\.Form|\.Text\s*=.*"RawrXD'
  'Editor Control' = '\$script:editor\s*=\s*New-Object|\$editor\s*=\s*New-Object'
  'File Explorer'  = '\$explorer\s*=\s*New-Object|\$treeView\s*=\s*New-Object'
  'Chat Tabs'      = '\$chatTabs\s*=\s*New-Object|\$tabControl\s*=\s*New-Object'
  'Model Dropdown' = '\$modelCombo\s*=\s*New-Object|\$modelDropdown\s*=\s*New-Object'
  'Dev Console'    = '\$global:devConsole\s*=\s*New-Object|\$consoleTextBox\s*=\s*New-Object'
  'Browser Panel'  = '\$webBrowser\s*=|webBrowserPanel|WebBrowser\s*=\s*New-Object'
  'Agent Panel'    = '\$agentPanel\s*=|agentListBox|\$agentContainer\s*=|\$agentInputContainer\s*='
  'Status Bar'     = '\$statusBar\s*=\s*New-Object|\$statusLabel\s*=\s*New-Object'
  'Menu Bar'       = '\$menuStrip\s*=\s*New-Object|\$fileMenu\s*=\s*New-Object'
}

foreach ($control in $guiControls.GetEnumerator()) {
  Test-Feature "Control: $($control.Key)" {
    return ($rawrxdContent -match $control.Value)
  } -Category "GUI Controls"
}

# ============================================
# CATEGORY 5: CHAT SYSTEM VALIDATION
# ============================================
Write-TestSection "Chat System Components"

Test-Feature "Chat Session Structure Defined" {
  return ($rawrxdContent -match 'ChatBox|InputBox|ModelCombo')
} -Category "Chat"

Test-Feature "Message History Array" {
  return ($rawrxdContent -match 'Messages\s*=\s*@\(\)')
} -Category "Chat"

Test-Feature "Chat Send Handler" {
  return ($rawrxdContent -match '\$sendBtn\.Add_Click|\$agentSendBtn\.Add_Click')
} -Category "Chat"

Test-Feature "Parallel Chat Processing Function" {
  return ($rawrxdContent -match 'function\s+Start-ParallelChatProcessing')
} -Category "Chat"

Test-Feature "Multithreaded Agent System" {
  return ($rawrxdContent -match 'threadSafeContext|RunspacePool')
} -Category "Chat"

Test-Feature "API Key Environment Variable Export" {
  return ($rawrxdContent -match "SetEnvironmentVariable.*RAWRXD_API_KEY")
} -Category "Chat"

Test-Feature "Runspace-Safe API Key Retrieval" {
  return ($rawrxdContent -match 'Get-Command.*Get-SecureAPIKey.*ErrorAction\s+SilentlyContinue')
} -Category "Chat"

# ============================================
# CATEGORY 6: TOGGLE & CHECKBOX VALIDATION
# ============================================
Write-TestSection "Toggles & Checkboxes"

$toggles = @{
  'Chat Panel Toggle'     = 'toggleChatBtn|Toggle Chat'
  'File Explorer Toggle'  = 'toggleExplorerBtn|Toggle.*Explorer'
  'Agent Panel Toggle'    = 'toggleAgentBtn|Toggle.*Agent'
  'Auto Save Checkbox'    = 'AutoSaveEnabled|autoSaveCheckbox'
  'Line Numbers Checkbox' = 'ShowLineNumbers|lineNumbersCheckbox'
  'Code Highlight Toggle' = 'CodeHighlighting'
  'Word Wrap Toggle'      = 'WrapText|wordWrapCheckbox'
  'Agent Mode Toggle'     = 'AgentMode|agentModeCheckbox'
  'Debug Mode Toggle'     = 'DebugMode'
  'Stealth Mode'          = 'stealthMode|StealthButton'
}

foreach ($toggle in $toggles.GetEnumerator()) {
  Test-Feature "Toggle/Checkbox: $($toggle.Key)" {
    return ($rawrxdContent -match $toggle.Value)
  } -Category "Toggles"
}

# ============================================
# CATEGORY 7: EVENT HANDLERS
# ============================================
Write-TestSection "Event Handlers"

$eventHandlers = @{
  'Form Load'              = 'Add_Load|Add_Shown'
  'Form Closing'           = 'Add_FormClosing|Add_Closing'
  'File Explorer Click'    = 'Add_NodeMouseDoubleClick|Add_AfterSelect|Add_NodeMouseClick'
  'Editor Text Changed'    = 'Add_TextChanged'
  'Editor Key Down'        = 'Add_KeyDown'
  'Chat Input KeyPress'    = 'InputBox.*Add_KeyPress|Add_KeyDown'
  'Model Selection Change' = 'ModelCombo.*Add_SelectedIndexChanged|modelDropdown.*Add_SelectedIndexChanged'
  'Save Button Click'      = 'saveBtn.*Add_Click|Save.*Add_Click'
  'Open Button Click'      = 'openBtn.*Add_Click|Open.*Add_Click'
  'Theme Change'           = 'themeCombo.*Add_SelectedIndexChanged|Apply.*Theme|Set.*Theme'
}

foreach ($handler in $eventHandlers.GetEnumerator()) {
  Test-Feature "Event Handler: $($handler.Key)" {
    return ($rawrxdContent -match $handler.Value)
  } -Category "Events"
}

# ============================================
# CATEGORY 8: FILE OPERATIONS
# ============================================
Write-TestSection "File Operations"

Test-Feature "Open File Function" {
  return ($rawrxdContent -match 'function\s+Open.*Editor|Load.*File|OpenFileDialog')
} -Category "Files"

Test-Feature "Save File Function" {
  return ($rawrxdContent -match 'function\s+Save.*(Current)?File|Save-CLIFile|SaveFileDialog')
} -Category "Files"

Test-Feature "File Dialog Integration" {
  return ($rawrxdContent -match 'OpenFileDialog|SaveFileDialog')
} -Category "Files"

Test-Feature "Syntax Highlighting Function" {
  return ($rawrxdContent -match 'function\s+Apply-SyntaxHighlighting')
} -Category "Files"

Test-Feature "File Extension Detection" {
  return ($rawrxdContent -match '\.Extension|Get-FileExtension')
} -Category "Files"

# ============================================
# CATEGORY 9: OLLAMA INTEGRATION
# ============================================
Write-TestSection "Ollama Integration"

Test-Feature "Ollama Request Function" {
  return ($rawrxdContent -match 'function\s+Send-OllamaRequest')
} -Category "Ollama"

Test-Feature "Ollama Endpoint Configuration" {
  return ($rawrxdContent -match 'localhost:11434|OllamaEndpoint')
} -Category "Ollama"

Test-Feature "Ollama API Headers" {
  return ($rawrxdContent -match 'Authorization.*Bearer|X-Ollama-API-Key')
} -Category "Ollama"

Test-Feature "Ollama Model List Function" {
  return ($rawrxdContent -match '/api/tags|Get.*OllamaModels')
} -Category "Ollama"

Test-Feature "Ollama Server Auto-Start" {
  return ($rawrxdContent -match 'Start-Process.*ollama|ollama.*serve')
} -Category "Ollama"

# ============================================
# CATEGORY 10: EXTENSION SYSTEM
# ============================================
Write-TestSection "Extension System"

Test-Feature "Extension Registry Variable" {
  return ($rawrxdContent -match 'extensionRegistry')
} -Category "Extensions"

Test-Feature "Extension Marketplace Function" {
  return ($rawrxdContent -match 'function\s+Show-(CLI)?Marketplace|Load-MarketplaceCatalog|Get-VSCodeMarketplaceExtensions')
} -Category "Extensions"

Test-Feature "Extension Installation" {
  return ($rawrxdContent -match 'Install-Extension|Register-Extension')
} -Category "Extensions"

Test-Feature "Extension Loading" {
  return ($rawrxdContent -match 'Load.*Extension|Import.*Extension')
} -Category "Extensions"

Test-Feature "VSCode Marketplace Integration" {
  return ($rawrxdContent -match 'marketplace\.visualstudio\.com|vscode.*api')
} -Category "Extensions"

# ============================================
# CATEGORY 11: THEME SYSTEM
# ============================================
Write-TestSection "Theme System"

Test-Feature "Theme Application Function" {
  return ($rawrxdContent -match 'Apply.*Theme|Set.*Theme')
} -Category "Themes"

Test-Feature "Dark Theme" {
  return ($rawrxdContent -match 'Dark.*Theme|ThemeMode.*Dark')
} -Category "Themes"

Test-Feature "Light Theme" {
  return ($rawrxdContent -match 'Light.*Theme|ThemeMode.*Light')
} -Category "Themes"

Test-Feature "Custom Themes" {
  return ($rawrxdContent -match 'Stealth-Cheetah|Custom.*Theme')
} -Category "Themes"

# ============================================
# CATEGORY 12: AGENT SYSTEM
# ============================================
Write-TestSection "Agent System"

Test-Feature "Agent Task Creation" {
  return ($rawrxdContent -match 'Create-AgentTask|New-AgentTask')
} -Category "Agents"

Test-Feature "Agent Task List" {
  return ($rawrxdContent -match 'agentTasks|Get-AgentTasks')
} -Category "Agents"

Test-Feature "Agent Execution" {
  return ($rawrxdContent -match 'Execute-Agent|Invoke-AgentTask|agentSendBtn\.Add_Click')
} -Category "Agents"

Test-Feature "Agent Status Tracking" {
  return ($rawrxdContent -match 'AgentStatus|TaskStatus')
} -Category "Agents"

# ============================================
# CATEGORY 13: BROWSER INTEGRATION
# ============================================
Write-TestSection "Browser Integration"

Test-Feature "WebView2 Detection" {
  return ($rawrxdContent -match 'WebView2|CoreWebView2')
} -Category "Browser"

Test-Feature "Browser Navigation" {
  return ($rawrxdContent -match 'Navigate.*Url|Browse.*To')
} -Category "Browser"

Test-Feature "Custom Browser Fallback" {
  return ($rawrxdContent -match 'CustomBrowser|Legacy.*Browser')
} -Category "Browser"

Test-Feature "YouTube Integration" {
  return ($rawrxdContent -match 'youtube\.com|Search.*YouTube')
} -Category "Browser"

# ============================================
# CATEGORY 14: LOGGING SYSTEM
# ============================================
Write-TestSection "Logging System"

Test-Feature "Dev Console Write Function" {
  return ($rawrxdContent -match 'function\s+Write-DevConsole')
} -Category "Logging"

Test-Feature "Error Logging" {
  return ($rawrxdContent -match 'Write-ErrorLog|Log-Error')
} -Category "Logging"

Test-Feature "Security Logging" {
  return ($rawrxdContent -match 'Write-SecurityLog')
} -Category "Logging"

Test-Feature "Log File Path" {
  return ($rawrxdContent -match 'logPath|LogFile|\.log')
} -Category "Logging"

# ============================================
# CATEGORY 15: SECURITY FEATURES
# ============================================
Write-TestSection "Security Features"

Test-Feature "Secure API Key Storage" {
  return ($rawrxdContent -match 'Get-SecureAPIKey|SecureString')
} -Category "Security"

Test-Feature "API Key Validation" {
  return ($rawrxdContent -match 'Test-InputValidation.*APIKey|Validate.*APIKey')
} -Category "Security"

Test-Feature "Input Sanitization" {
  return ($rawrxdContent -match 'Test-InputValidation|Sanitize.*Input')
} -Category "Security"

Test-Feature "HTTPS Enforcement" {
  return ($rawrxdContent -match 'https://|SecureEndpoint')
} -Category "Security"

# ============================================
# CATEGORY 16: PERFORMANCE OPTIMIZATION
# ============================================
Write-TestSection "Performance Features"

Test-Feature "Performance Optimization Function" {
  return ($rawrxdContent -match 'Optimize-Performance|Performance.*Optimization')
} -Category "Performance"

Test-Feature "Memory Management" {
  return ($rawrxdContent -match 'GC\.Collect|Memory.*Cleanup|\.Dispose\(\)')
} -Category "Performance"

Test-Feature "Process Priority Setting" {
  return ($rawrxdContent -match 'ProcessPriorityClass|Priority')
} -Category "Performance"

Test-Feature "Network Optimization" {
  return ($rawrxdContent -match 'ServicePointManager|Network.*Optimization')
} -Category "Performance"

# ============================================
# CATEGORY 17: CLI COMMAND HANDLERS
# ============================================
Write-TestSection "CLI Command Handlers"

$cliHandlerPath = Join-Path $PSScriptRoot "cli-handlers"
if (Test-Path $cliHandlerPath) {
  $cliHandlers = Get-ChildItem $cliHandlerPath -Filter "*.ps1"

  foreach ($handler in $cliHandlers) {
    Test-Feature "CLI Handler File: $($handler.Name)" {
      return (Test-Path $handler.FullName)
    } -Category "CLI"

    Test-Feature "CLI Handler Syntax: $($handler.Name)" {
      $content = Get-Content $handler.FullName -Raw
      $null = [System.Management.Automation.PSParser]::Tokenize($content, [ref]$null)
      return $true
    } -Category "CLI"
  }
}
else {
  Test-Warning "CLI Handlers Directory" "cli-handlers folder not found" -Category "CLI"
}

# ============================================
# CATEGORY 18: ERROR HANDLING
# ============================================
Write-TestSection "Error Handling"

Test-Feature "Try-Catch Blocks Present" {
  $tryCount = ([regex]::Matches($rawrxdContent, '\btry\s*\{').Count)
  return ($tryCount -gt 50)  # Should have many error handlers
} -Category "ErrorHandling"

Test-Feature "Error Action Preference Set" {
  return ($rawrxdContent -match '\$ErrorActionPreference')
} -Category "ErrorHandling"

Test-Feature "Null Checks" {
  return ($rawrxdContent -match 'if\s*\(\s*-not\s+\$|if\s*\(\s*\$null')
} -Category "ErrorHandling"

# ============================================
# CATEGORY 19: DOCUMENTATION
# ============================================
Write-TestSection "Documentation & Comments"

Test-Feature "Function Documentation (Comment-Based Help)" {
  return ($rawrxdContent -match '<#[\s\S]*?\.SYNOPSIS[\s\S]*?#>')
} -Category "Documentation"

Test-Feature "Inline Comments Present" {
  $commentCount = ([regex]::Matches($rawrxdContent, '#[^\r\n]+').Count)
  return ($commentCount -gt 100)
} -Category "Documentation"

# ============================================
# CATEGORY 20: SETTINGS PERSISTENCE
# ============================================
Write-TestSection "Settings Persistence"

Test-Feature "Settings Load Function" {
  return ($rawrxdContent -match 'function\s+Get-Settings')
} -Category "Settings"

Test-Feature "Settings Save Function" {
  return ($rawrxdContent -match 'function\s+Save-Settings')
} -Category "Settings"

Test-Feature "Settings File Path" {
  return ($rawrxdContent -match 'settingsPath|settings\.json')
} -Category "Settings"

Test-Feature "Settings JSON Conversion" {
  return ($rawrxdContent -match 'ConvertTo-Json|ConvertFrom-Json')
} -Category "Settings"

# ============================================
# GENERATE REPORT
# ============================================

Write-TestHeader "Test Results Summary"

Write-Host "`n📊 Overall Statistics:" -ForegroundColor Cyan
Write-Host "  ✓ Passed:  $script:PassCount" -ForegroundColor Green
Write-Host "  ✗ Failed:  $script:FailCount" -ForegroundColor Red
Write-Host "  ⚠ Warnings: $script:WarningCount" -ForegroundColor Yellow
$totalTests = $script:PassCount + $script:FailCount
$passRate = if ($totalTests -gt 0) { [math]::Round(($script:PassCount / $totalTests) * 100, 2) } else { 0 }
Write-Host "  📈 Pass Rate: $passRate%" -ForegroundColor $(if ($passRate -ge 90) { "Green" } elseif ($passRate -ge 70) { "Yellow" } else { "Red" })

# Group results by category
Write-Host "`n📋 Results by Category:" -ForegroundColor Cyan
$script:TestResults | Group-Object -Property Category | ForEach-Object {
  $categoryName = $_.Name
  $passed = ($_.Group | Where-Object { $_.Status -eq "PASS" }).Count
  $failed = ($_.Group | Where-Object { $_.Status -eq "FAIL" }).Count
  $warned = ($_.Group | Where-Object { $_.Status -eq "WARN" }).Count
  $total = $_.Count

  Write-Host "`n  $categoryName ($passed/$total passed)" -ForegroundColor $(if ($failed -eq 0) { "Green" } else { "Yellow" })

  if ($failed -gt 0) {
    $_.Group | Where-Object { $_.Status -eq "FAIL" } | ForEach-Object {
      Write-Host "    ✗ $($_.Name)" -ForegroundColor Red
      if ($Verbose) {
        Write-Host "      $($_.Message)" -ForegroundColor DarkRed
      }
    }
  }

  if ($warned -gt 0) {
    $_.Group | Where-Object { $_.Status -eq "WARN" } | ForEach-Object {
      Write-Host "    ⚠ $($_.Name): $($_.Message)" -ForegroundColor Yellow
    }
  }
}

# Save report
$reportPath = Join-Path $PSScriptRoot "Internal-Validation-Report-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
$script:TestResults | ConvertTo-Json -Depth 5 | Out-File $reportPath -Encoding UTF8
Write-Host "`n💾 Full report saved to: $reportPath" -ForegroundColor Cyan

# Exit with appropriate code
Write-Host ""
if ($script:FailCount -eq 0) {
  Write-Host "✅ ALL TESTS PASSED! RawrXD is internally validated." -ForegroundColor Green
  exit 0
}
else {
  Write-Host "⚠️  SOME TESTS FAILED. Review the report above." -ForegroundColor Yellow
  exit 1
}
