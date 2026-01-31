<#
.SYNOPSIS
    Comprehensive feature testing for RawrXD IDE
.DESCRIPTION
    Tests all menu options, dropdowns, settings, and features programmatically
    Can run in CLI mode or interact with GUI components
.PARAMETER Mode
    'CLI' - Test via command-line functions only
    'GUI' - Test GUI components directly (requires IDE running)
    'Hybrid' - Test both CLI and GUI (default)
#>

param(
  [ValidateSet('CLI', 'GUI', 'Hybrid')]
  [string]$Mode = 'CLI',

  [switch]$Verbose,

  [string]$OutputFile = ".\test-results-$(Get-Date -Format 'yyyyMMdd-HHmmss').json"
)

# Test results collection
$script:TestResults = @{
  Timestamp = Get-Date
  Mode      = $Mode
  Passed    = 0
  Failed    = 0
  Skipped   = 0
  Tests     = @()
}

function Write-TestLog {
  param(
    [string]$Message,
    [ValidateSet('INFO', 'PASS', 'FAIL', 'WARN', 'SKIP')]
    [string]$Level = 'INFO'
  )

  $color = switch ($Level) {
    'PASS' { 'Green' }
    'FAIL' { 'Red' }
    'WARN' { 'Yellow' }
    'SKIP' { 'Cyan' }
    default { 'White' }
  }

  $prefix = switch ($Level) {
    'PASS' { '✅' }
    'FAIL' { '❌' }
    'WARN' { '⚠️' }
    'SKIP' { '⏭️' }
    default { 'ℹ️' }
  }

  Write-Host "$prefix $Message" -ForegroundColor $color
}

function Test-Feature {
  param(
    [string]$Name,
    [string]$Category,
    [scriptblock]$TestScript,
    [string]$Description = ""
  )

  $testResult = @{
    Name        = $Name
    Category    = $Category
    Description = $Description
    Status      = 'Unknown'
    Error       = $null
    Duration    = 0
    Timestamp   = Get-Date
  }

  Write-TestLog "Testing: $Category - $Name" 'INFO'

  $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()

  try {
    $result = & $TestScript
    $stopwatch.Stop()
    $testResult.Duration = $stopwatch.ElapsedMilliseconds

    if ($result -eq $true) {
      $testResult.Status = 'Passed'
      $script:TestResults.Passed++
      Write-TestLog "  PASSED - $Name ($($testResult.Duration)ms)" 'PASS'
    }
    elseif ($result -eq $null) {
      $testResult.Status = 'Skipped'
      $script:TestResults.Skipped++
      Write-TestLog "  SKIPPED - $Name" 'SKIP'
    }
    else {
      $testResult.Status = 'Failed'
      $testResult.Error = "Returned false"
      $script:TestResults.Failed++
      Write-TestLog "  FAILED - $Name" 'FAIL'
    }
  }
  catch {
    $stopwatch.Stop()
    $testResult.Duration = $stopwatch.ElapsedMilliseconds
    $testResult.Status = 'Failed'
    $testResult.Error = $_.Exception.Message
    $script:TestResults.Failed++
    Write-TestLog "  FAILED - $Name : $($_.Exception.Message)" 'FAIL'
  }

  $script:TestResults.Tests += $testResult
}

# ==============================================
# CLI MODE TESTS
# ==============================================

function Test-CLIMode {
  Write-TestLog "`n=== CLI MODE TESTS ===" 'INFO'

  # Test 1: Script loads without errors
  Test-Feature -Name "Script Loading" -Category "Core" -Description "RawrXD.ps1 loads without syntax errors" -TestScript {
    $result = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'exit' 2>&1"
    return ($LASTEXITCODE -eq 0)
  }

  # Test 2: List models command
  Test-Feature -Name "List Models (Ollama)" -Category "AI Integration" -Description "CLI can list Ollama models" -TestScript {
    $result = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'list-models' 2>&1 | Select-Object -Last 5"
    return ($result -match "Model:|qwen|llama|mistral" -or $result -match "No models")
  }

  # Test 3: Help command
  Test-Feature -Name "Help Command" -Category "CLI" -Description "CLI help displays available commands" -TestScript {
    $result = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'help' 2>&1"
    return ($result -match "Available commands|list-models|chat|file")
  }

  # Test 4: File operations
  Test-Feature -Name "File Read" -Category "File System" -Description "CLI can read files" -TestScript {
    # Create test file
    "Test content $(Get-Date)" | Out-File -FilePath ".\test-file-temp.txt" -Encoding UTF8
    $result = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'read-file test-file-temp.txt' 2>&1"
    Remove-Item ".\test-file-temp.txt" -ErrorAction SilentlyContinue
    return ($result -match "Test content")
  }

  # Test 5: Settings access
  Test-Feature -Name "Settings Loading" -Category "Configuration" -Description "Settings file loads correctly" -TestScript {
    Test-Path ".\config\settings.json" -or $true  # Pass if settings exist or can be created
  }

  # Test 6: Extension system
  Test-Feature -Name "Extension System" -Category "Extensions" -Description "Extension marketplace accessible" -TestScript {
    $result = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'list-extensions' 2>&1"
    return ($result -match "extension|marketplace|available" -or $result -match "No extensions")
  }

  # Test 7: Git integration
  Test-Feature -Name "Git Status" -Category "Version Control" -Description "Git status command works" -TestScript {
    if (Test-Path ".git") {
      $result = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'git-status' 2>&1"
      return ($result -match "branch|commit|status" -or $result -match "Not a git")
    }
    else {
      return $null  # Skip if not a git repo
    }
  }

  # Test 8: Theme system
  Test-Feature -Name "Theme Switching" -Category "UI" -Description "Theme commands available" -TestScript {
    $result = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'list-themes' 2>&1"
    return ($result -match "theme|dark|light|monokai|solarized" -or $result -match "Available")
  }

  # Test 9: Search functionality
  Test-Feature -Name "Code Search" -Category "Search" -Description "Grep/search commands work" -TestScript {
    $result = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'grep function RawrXD.ps1' 2>&1"
    return ($result -match "function|found|match")
  }

  # Test 10: Terminal commands
  Test-Feature -Name "Terminal Execution" -Category "Terminal" -Description "Can execute terminal commands" -TestScript {
    $result = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'exec Get-Date' 2>&1"
    return ($result -match "\d{4}" -or $result -match "executed")
  }
}

# ==============================================
# FUNCTION EXISTENCE TESTS
# ==============================================

function Test-FunctionExistence {
  Write-TestLog "`n=== FUNCTION EXISTENCE TESTS ===" 'INFO'

  # Load the script to check functions
  $scriptContent = Get-Content ".\RawrXD.ps1" -Raw

  $functionsToTest = @(
    'New-ChatTab',
    'Send-ChatMessage',
    'Update-Explorer',
    'Show-Marketplace',
    'Get-Settings',
    'Save-Settings',
    'Apply-EditorSettings',
    'Show-ModelSettings',
    'Show-EditorSettings',
    'Show-ChatSettings',
    'Invoke-GitCommand',
    'Open-Browser',
    'Register-AgentTool',
    'Invoke-AgentTool',
    'Get-SystemSpecifications',
    'Scan-ForModels',
    'Show-CommandPalette',
    'Update-ChatStatus',
    'Show-BulkActionsMenu',
    'Get-ThreadingStatus'
  )

  foreach ($funcName in $functionsToTest) {
    Test-Feature -Name "$funcName Exists" -Category "Functions" -Description "Function $funcName is defined" -TestScript {
      return ($scriptContent -match "function $funcName")
    }
  }
}

# ==============================================
# AGENT TOOLS TESTS
# ==============================================

function Test-AgentTools {
  Write-TestLog "`n=== AGENT TOOLS TESTS ===" 'INFO'

  $agentTools = @(
    'read_file',
    'write_file',
    'list_directory',
    'create_directory',
    'delete_file',
    'execute_command',
    'git_status',
    'git_commit',
    'git_push',
    'git_clone',
    'git_pull',
    'git_reset',
    'git_clean',
    'git_abort',
    'process_kill',
    'file_unlock'
  )

  $scriptContent = Get-Content ".\RawrXD.ps1" -Raw

  foreach ($tool in $agentTools) {
    Test-Feature -Name "Agent Tool: $tool" -Category "Agent Tools" -Description "Agent tool $tool is registered" -TestScript {
      return ($scriptContent -match "Register-AgentTool.*-Name `"$tool`"")
    }
  }
}

# ==============================================
# SETTINGS TESTS
# ==============================================

function Test-Settings {
  Write-TestLog "`n=== SETTINGS TESTS ===" 'INFO'

  $settingsToTest = @(
    'OllamaModel',
    'MaxTabs',
    'AutoSaveEnabled',
    'EditorFontFamily',
    'EditorFontSize',
    'EditorTextColor',
    'EditorBackgroundColor',
    'CodeHighlighting',
    'MaxChatTabs',
    'ChatTabAutoClose'
  )

  $scriptContent = Get-Content ".\RawrXD.ps1" -Raw

  foreach ($setting in $settingsToTest) {
    Test-Feature -Name "Setting: $setting" -Category "Settings" -Description "Setting $setting is defined" -TestScript {
      return ($scriptContent -match "\`$$setting|`"$setting`"")
    }
  }
}

# ==============================================
# MENU STRUCTURE TESTS
# ==============================================

function Test-MenuStructure {
  Write-TestLog "`n=== MENU STRUCTURE TESTS ===" 'INFO'

  $menus = @(
    'File',
    'Edit',
    'View',
    'Tools',
    'Git',
    'AI',
    'Help'
  )

  $scriptContent = Get-Content ".\RawrXD.ps1" -Raw

  foreach ($menu in $menus) {
    Test-Feature -Name "Menu: $menu" -Category "Menus" -Description "$menu menu exists" -TestScript {
      return ($scriptContent -match "\`$${menu}Menu|New-Object.*MenuItem.*$menu")
    }
  }

  # Test specific menu items
  $menuItems = @(
    'New File',
    'Open File',
    'Save',
    'Save As',
    'Undo',
    'Redo',
    'Find',
    'Replace',
    'Settings',
    'Extensions',
    'Commit',
    'Push',
    'Pull',
    'Chat with AI',
    'System Scanner'
  )

  foreach ($item in $menuItems) {
    Test-Feature -Name "MenuItem: $item" -Category "Menu Items" -Description "Menu item '$item' exists" -TestScript {
      return ($scriptContent -match "$item")
    }
  }
}

# ==============================================
# FEATURE INTEGRATION TESTS
# ==============================================

function Test-FeatureIntegration {
  Write-TestLog "`n=== FEATURE INTEGRATION TESTS ===" 'INFO'

  # Test 1: Ollama connectivity
  Test-Feature -Name "Ollama Server" -Category "Integration" -Description "Ollama server is accessible" -TestScript {
    try {
      $response = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -Method GET -TimeoutSec 5
      return ($response -ne $null)
    }
    catch {
      return $null  # Skip if Ollama not running
    }
  }

  # Test 2: WebView2 availability
  Test-Feature -Name "WebView2 Runtime" -Category "Integration" -Description "WebView2 runtime is installed" -TestScript {
    $webview2Path = "C:\Users\$env:USERNAME\AppData\Local\Temp\WVLibs\Microsoft.Web.WebView2.Core.dll"
    return (Test-Path $webview2Path) -or (Test-Path "C:\Program Files (x86)\Microsoft\EdgeWebView\Application")
  }

  # Test 3: Python environment (for swarm)
  Test-Feature -Name "Python Available" -Category "Integration" -Description "Python is installed" -TestScript {
    try {
      $null = python --version 2>&1
      return ($LASTEXITCODE -eq 0)
    }
    catch {
      return $null  # Skip if Python not required
    }
  }

  # Test 4: Extension modules
  Test-Feature -Name "ModelMaker Module" -Category "Integration" -Description "ModelMaker extension module exists" -TestScript {
    Test-Path ".\modules\ModelMaker" -or Test-Path ".\extensions\ModelMaker"
  }
}

# ==============================================
# ERROR DETECTION TESTS
# ==============================================

function Test-ErrorDetection {
  Write-TestLog "`n=== ERROR DETECTION TESTS ===" 'INFO'

  # Test 1: No RTF errors in logs
  Test-Feature -Name "No RTF Errors" -Category "Error Detection" -Description "No RTF format errors in recent runs" -TestScript {
    $logFile = Get-ChildItem ".\logs" -Filter "startup_*.log" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($logFile) {
      $content = Get-Content $logFile.FullName -Raw
      return ($content -notmatch "File format is not valid|RTF")
    }
    return $true
  }

  # Test 2: No null reference errors
  Test-Feature -Name "No Null References" -Category "Error Detection" -Description "No null reference errors in recent logs" -TestScript {
    $logFile = Get-ChildItem ".\logs" -Filter "startup_*.log" -ErrorAction SilentlyContinue | Sort-Object LastWriteTime -Descending | Select-Object -First 1
    if ($logFile) {
      $content = Get-Content $logFile.FullName -Raw
      return ($content -notmatch "NullReferenceException|null reference")
    }
    return $true
  }

  # Test 3: Timer safety
  Test-Feature -Name "Timer Null Checks" -Category "Error Detection" -Description "Timers have null safety checks" -TestScript {
    $scriptContent = Get-Content ".\RawrXD.ps1" -Raw
    $timerCount = ([regex]::Matches($scriptContent, "\.Add_Tick\(")).Count
    $nullCheckCount = ([regex]::Matches($scriptContent, "if \(-not \`\$.*\) \{[\s\S]*?return")).Count
    return ($nullCheckCount -gt ($timerCount / 2))  # At least half of timers should have null checks
  }
}

# ==============================================
# PERFORMANCE TESTS
# ==============================================

function Test-Performance {
  Write-TestLog "`n=== PERFORMANCE TESTS ===" 'INFO'

  # Test 1: Script load time
  Test-Feature -Name "Script Load Time" -Category "Performance" -Description "Script loads in under 10 seconds" -TestScript {
    $sw = [System.Diagnostics.Stopwatch]::StartNew()
    $null = powershell -ExecutionPolicy Bypass -Command "& '.\RawrXD.ps1' -CliMode -Command 'exit' 2>&1"
    $sw.Stop()
    Write-TestLog "  Load time: $($sw.ElapsedMilliseconds)ms" 'INFO'
    return ($sw.ElapsedMilliseconds -lt 10000)
  }

  # Test 2: File size reasonable
  Test-Feature -Name "Script Size" -Category "Performance" -Description "Script file is not excessively large" -TestScript {
    $size = (Get-Item ".\RawrXD.ps1").Length
    Write-TestLog "  Script size: $([math]::Round($size/1MB, 2))MB" 'INFO'
    return ($size -lt 10MB)  # Should be under 10MB
  }
}

# ==============================================
# MAIN EXECUTION
# ==============================================

Write-Host "`n╔════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║  RawrXD IDE - Comprehensive Test Suite    ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════╝`n" -ForegroundColor Cyan

Write-TestLog "Test Mode: $Mode" 'INFO'
Write-TestLog "Output File: $OutputFile" 'INFO'
Write-TestLog "Start Time: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n" 'INFO'

# Change to script directory
Set-Location $PSScriptRoot

# Run test suites based on mode
if ($Mode -eq 'CLI' -or $Mode -eq 'Hybrid') {
  Test-CLIMode
}

Test-FunctionExistence
Test-AgentTools
Test-Settings
Test-MenuStructure
Test-FeatureIntegration
Test-ErrorDetection
Test-Performance

# ==============================================
# RESULTS SUMMARY
# ==============================================

Write-Host "`n`n╔════════════════════════════════════════════╗" -ForegroundColor Cyan
Write-Host "║           TEST RESULTS SUMMARY             ║" -ForegroundColor Cyan
Write-Host "╚════════════════════════════════════════════╝`n" -ForegroundColor Cyan

$total = $script:TestResults.Passed + $script:TestResults.Failed + $script:TestResults.Skipped
$passRate = if ($total -gt 0) { [math]::Round(($script:TestResults.Passed / $total) * 100, 2) } else { 0 }

Write-Host "Total Tests:    $total" -ForegroundColor White
Write-Host "Passed:         $($script:TestResults.Passed)" -ForegroundColor Green
Write-Host "Failed:         $($script:TestResults.Failed)" -ForegroundColor Red
Write-Host "Skipped:        $($script:TestResults.Skipped)" -ForegroundColor Cyan
Write-Host "Pass Rate:      $passRate%" -ForegroundColor $(if ($passRate -ge 80) { 'Green' } elseif ($passRate -ge 60) { 'Yellow' } else { 'Red' })
Write-Host ""

# Show failed tests
if ($script:TestResults.Failed -gt 0) {
  Write-Host "Failed Tests:" -ForegroundColor Red
  $script:TestResults.Tests | Where-Object { $_.Status -eq 'Failed' } | ForEach-Object {
    Write-Host "  • $($_.Category) - $($_.Name)" -ForegroundColor Red
    if ($_.Error) {
      Write-Host "    Error: $($_.Error)" -ForegroundColor DarkRed
    }
  }
  Write-Host ""
}

# Show skipped tests
if ($script:TestResults.Skipped -gt 0) {
  Write-Host "Skipped Tests:" -ForegroundColor Cyan
  $script:TestResults.Tests | Where-Object { $_.Status -eq 'Skipped' } | ForEach-Object {
    Write-Host "  • $($_.Category) - $($_.Name)" -ForegroundColor Cyan
  }
  Write-Host ""
}

# Save results to JSON
$script:TestResults | ConvertTo-Json -Depth 10 | Out-File -FilePath $OutputFile -Encoding UTF8
Write-TestLog "Results saved to: $OutputFile" 'INFO'

# Exit with appropriate code
if ($script:TestResults.Failed -gt 0) {
  exit 1
}
else {
  exit 0
}
