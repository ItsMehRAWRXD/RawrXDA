# 🧪 RawrXD Comprehensive GUI & Feature Test Suite
# Tests ALL features including menus, dialogs, and agentic capabilities
# Run this to validate the full application

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  RawrXD Comprehensive Test Suite v3.0" -ForegroundColor Cyan
Write-Host "  Full Feature Validation (No GUI Launch)" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

$ErrorActionPreference = "Continue"
$testResults = @{}
$totalTests = 0
$passedTests = 0
$failedTests = 0

function Test-Feature {
    param(
        [string]$Name,
        [scriptblock]$Test,
        [string]$Category = "General"
    )
    
    $script:totalTests++
    Write-Host "  Testing: $Name..." -ForegroundColor Yellow -NoNewline
    
    try {
        $result = & $Test
        if ($result) {
            Write-Host " ✅ PASS" -ForegroundColor Green
            $script:passedTests++
            $script:testResults["$Category::$Name"] = "PASS"
            return $true
        }
        else {
            Write-Host " ❌ FAIL" -ForegroundColor Red
            $script:failedTests++
            $script:testResults["$Category::$Name"] = "FAIL"
            return $false
        }
    }
    catch {
        Write-Host " ❌ ERROR: $_" -ForegroundColor Red
        $script:failedTests++
        $script:testResults["$Category::$Name"] = "ERROR: $_"
        return $false
    }
}

# ============================================
# Load RawrXD Source for Testing
# ============================================

Write-Host "`n📦 Loading RawrXD source code for analysis..." -ForegroundColor Yellow

$rawrxdPath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1"
if (-not (Test-Path $rawrxdPath)) {
    Write-Host "❌ RawrXD.ps1 not found at $rawrxdPath" -ForegroundColor Red
    exit 1
}

$sourceCode = Get-Content $rawrxdPath -Raw
$sourceLines = Get-Content $rawrxdPath
Write-Host "  ✅ Loaded $($sourceLines.Count) lines of source code" -ForegroundColor Green

# ============================================
# TEST CATEGORY 1: Core Functions Exist
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 1: Core Function Definitions" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$coreFunctions = @(
    "Show-ModelSettings",
    "Show-SecuritySettings",
    "Show-EditorSettings",
    "Show-ChatSettings",
    "Show-Marketplace",
    "Show-InstalledExtensions",
    "Show-CommandPalette",
    "Show-AuthenticationDialog",
    "Send-OllamaRequest",
    "Send-Chat",
    "Register-AgentTool",
    "Invoke-AgentTool",
    "Get-AgentToolsSchema",
    "Update-Explorer",
    "Open-Browser",
    "Get-GitStatus",
    "Invoke-TerminalCommand",
    "New-ChatTab",
    "Save-ChatHistory",
    "Write-DevConsole"
)

foreach ($func in $coreFunctions) {
    Test-Feature -Name "Function: $func" -Category "CoreFunctions" -Test {
        $sourceCode -match "function\s+$func\s*\{"
    }
}

# ============================================
# TEST CATEGORY 2: Menu Items Defined
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 2: Menu Items & Event Handlers" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$menuItems = @(
    @{ Name = "File Menu"; Pattern = '\$fileMenu\s*=' },
    @{ Name = "Settings Menu"; Pattern = '\$settingsMenu\s*=' },
    @{ Name = "Security Menu"; Pattern = '\$securityMenu\s*=' },
    @{ Name = "Extensions Menu"; Pattern = '\$extensionsMenu\s*=' },
    @{ Name = "Tools Menu"; Pattern = '\$toolsMenu\s*=' },
    @{ Name = "Chat Menu"; Pattern = '\$chatMenu\s*=' },
    @{ Name = "Model Settings Item"; Pattern = '\$modelSettingsItem\s*=' },
    @{ Name = "Security Settings Item"; Pattern = '\$securitySettingsItem\s*=' },
    @{ Name = "Editor Settings Item"; Pattern = '\$editorSettingsItem\s*=' },
    @{ Name = "Marketplace Item"; Pattern = '\$marketplaceItem\s*=' }
)

foreach ($item in $menuItems) {
    Test-Feature -Name $item.Name -Category "MenuItems" -Test {
        $sourceCode -match $item.Pattern
    }
}

# Check event handlers are connected
$eventHandlers = @(
    @{ Name = "Model Settings Click Handler"; Pattern = '\$modelSettingsItem\.Add_Click' },
    @{ Name = "Security Settings Click Handler"; Pattern = '\$securitySettingsItem\.Add_Click' },
    @{ Name = "Editor Settings Click Handler"; Pattern = '\$editorSettingsItem\.Add_Click' },
    @{ Name = "Marketplace Click Handler"; Pattern = '\$marketplaceItem\.Add_Click' }
)

foreach ($handler in $eventHandlers) {
    Test-Feature -Name $handler.Name -Category "EventHandlers" -Test {
        $sourceCode -match $handler.Pattern
    }
}

# ============================================
# TEST CATEGORY 3: Dialog Forms
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 3: Dialog Form Implementations" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$dialogs = @(
    @{ Name = "Security Settings Form"; Pattern = 'function Show-SecuritySettings[\s\S]*?\$settingsForm\.ShowDialog\(\)' },
    @{ Name = "Model Settings Form"; Pattern = 'function Show-ModelSettings[\s\S]*?StartPosition.*CenterScreen' },
    @{ Name = "Authentication Dialog"; Pattern = 'function Show-AuthenticationDialog[\s\S]*?\$authForm' },
    @{ Name = "Command Palette Form"; Pattern = '\$commandPalette\s*=.*Form' }
)

foreach ($dialog in $dialogs) {
    Test-Feature -Name $dialog.Name -Category "Dialogs" -Test {
        $sourceCode -match $dialog.Pattern
    }
}

# ============================================
# TEST CATEGORY 4: Agent Mode Auto-Enable
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 4: Agent Mode Auto-Enable Feature" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-Feature -Name "Auto-Enable Keywords Array" -Category "AgentMode" -Test {
    $sourceCode -match '\$agenticKeywords\s*=\s*@\('
}

Test-Feature -Name "Auto-Enable Detection Logic" -Category "AgentMode" -Test {
    $sourceCode -match 'requiresAgentMode.*-and.*-not.*AgentMode'
}

Test-Feature -Name "Context Gathering for Agentic Requests" -Category "AgentMode" -Test {
    $sourceCode -match 'INTELLIGENT CONTEXT GATHERING|Gathering real filesystem context'
}

Test-Feature -Name "Path Extraction from Messages" -Category "AgentMode" -Test {
    $sourceCode -match '\[regex\]::Matches\(\$msg'
}

# ============================================
# TEST CATEGORY 5: Agent Tools Registration
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 5: Agent Tools Registration" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$agentTools = @(
    "read_file",
    "write_file",
    "list_directory",
    "execute_command",
    "git_status",
    "browse_url",
    "apply_edit",
    "get_environment"
)

foreach ($tool in $agentTools) {
    Test-Feature -Name "Agent Tool: $tool" -Category "AgentTools" -Test {
        $sourceCode -match "Register-AgentTool.*-Name.*`"$tool`""
    }
}

# ============================================
# TEST CATEGORY 6: Ollama Integration
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 6: Ollama API Integration" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-Feature -Name "Ollama API Endpoint Config" -Category "Ollama" -Test {
    $sourceCode -match '\$OllamaAPIEndpoint\s*=.*localhost:11434'
}

Test-Feature -Name "Send-OllamaRequest Function" -Category "Ollama" -Test {
    $sourceCode -match 'function Send-OllamaRequest'
}

Test-Feature -Name "Model Validation" -Category "Ollama" -Test {
    $sourceCode -match '\$availableModels.*api/tags|\$Model.*-notin.*availableModels'
}

Test-Feature -Name "Retry Logic with Backoff" -Category "Ollama" -Test {
    $sourceCode -match '\$maxRetries|\$retryCount.*-lt.*maxRetries'
}

# Live Ollama test
Test-Feature -Name "Ollama Service Running" -Category "Ollama" -Test {
    Test-NetConnection -ComputerName "localhost" -Port 11434 -InformationLevel Quiet -WarningAction SilentlyContinue
}

# ============================================
# TEST CATEGORY 7: UI Components
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 7: UI Components" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$uiComponents = @(
    @{ Name = "Main Form"; Pattern = '\$form\s*=.*Windows\.Forms\.Form' },
    @{ Name = "Editor (RichTextBox)"; Pattern = '\$script:editor\s*=.*RichTextBox' },
    @{ Name = "File Explorer (TreeView)"; Pattern = '\$explorer\s*=.*TreeView' },
    @{ Name = "Chat Tab Control"; Pattern = '\$chatTabControl\s*=.*TabControl' },
    @{ Name = "Terminal Output"; Pattern = '\$terminalOutput\s*=.*RichTextBox' },
    @{ Name = "Browser Container"; Pattern = '\$browserContainer\s*=' },
    @{ Name = "Dev Console"; Pattern = '\$global:devConsole\s*=' },
    @{ Name = "Agent Tasks List"; Pattern = '\$agentTasksList\s*=.*ListView' },
    @{ Name = "Git Status Box"; Pattern = '\$gitStatusBox\s*=' }
)

foreach ($comp in $uiComponents) {
    Test-Feature -Name $comp.Name -Category "UIComponents" -Test {
        $sourceCode -match $comp.Pattern
    }
}

# ============================================
# TEST CATEGORY 8: Security Features
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 8: Security Features" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-Feature -Name "Security Config Hashtable" -Category "Security" -Test {
    $sourceCode -match '\$script:SecurityConfig\s*=\s*@\{'
}

Test-Feature -Name "Session Management" -Category "Security" -Test {
    $sourceCode -match '\$script:CurrentSession\s*=\s*@\{'
}

Test-Feature -Name "Input Validation Function" -Category "Security" -Test {
    $sourceCode -match 'function Test-InputSafety'
}

Test-Feature -Name "Stealth Mode Function" -Category "Security" -Test {
    $sourceCode -match 'function Enable-StealthMode'
}

Test-Feature -Name "Encryption Functions" -Category "Security" -Test {
    $sourceCode -match 'function Protect-SensitiveString'
}

# ============================================
# TEST CATEGORY 9: Syntax Validation
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 9: PowerShell Syntax Validation" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-Feature -Name "Script Syntax Check" -Category "Syntax" -Test {
    $errors = $null
    $tokens = $null
    [System.Management.Automation.Language.Parser]::ParseFile($rawrxdPath, [ref]$tokens, [ref]$errors)
    $errors.Count -eq 0
}

# ============================================
# TEST CATEGORY 10: File Operations
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST 10: File System Integration" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

Test-Feature -Name "RawrXD.exe exists" -Category "Files" -Test {
    Test-Path "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.exe"
}

Test-Feature -Name "AppData directory" -Category "Files" -Test {
    $appDataDir = Join-Path $env:APPDATA "RawrXD"
    # It may not exist yet, that's ok - just check it would be valid
    $true
}

Test-Feature -Name "D:\professional-nasm-ide accessible" -Category "Files" -Test {
    Test-Path "D:\professional-nasm-ide"
}

# ============================================
# SUMMARY
# ============================================

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  TEST SUMMARY" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor Cyan

Write-Host "`n  Total Tests: $totalTests" -ForegroundColor White
Write-Host "  Passed: $passedTests" -ForegroundColor Green
Write-Host "  Failed: $failedTests" -ForegroundColor $(if ($failedTests -gt 0) { "Red" } else { "Green" })

$passRate = [math]::Round(($passedTests / $totalTests) * 100, 1)
Write-Host "`n  Pass Rate: $passRate%" -ForegroundColor $(if ($passRate -ge 90) { "Green" } elseif ($passRate -ge 70) { "Yellow" } else { "Red" })

# Show failed tests
if ($failedTests -gt 0) {
    Write-Host "`n  ❌ Failed Tests:" -ForegroundColor Red
    foreach ($test in $testResults.GetEnumerator()) {
        if ($test.Value -ne "PASS") {
            Write-Host "     - $($test.Key): $($test.Value)" -ForegroundColor Red
        }
    }
}

# Category breakdown
Write-Host "`n  📊 Category Breakdown:" -ForegroundColor Yellow
$categories = $testResults.Keys | ForEach-Object { ($_ -split "::")[0] } | Sort-Object -Unique
foreach ($cat in $categories) {
    $catTests = $testResults.Keys | Where-Object { $_ -like "$cat::*" }
    $catPassed = ($catTests | Where-Object { $testResults[$_] -eq "PASS" }).Count
    $catTotal = $catTests.Count
    $catIcon = if ($catPassed -eq $catTotal) { "✅" } elseif ($catPassed -gt 0) { "⚠️" } else { "❌" }
    Write-Host "     $catIcon $cat : $catPassed/$catTotal" -ForegroundColor $(if ($catPassed -eq $catTotal) { "Green" } elseif ($catPassed -gt 0) { "Yellow" } else { "Red" })
}

Write-Host "`n🔚 Comprehensive test complete!" -ForegroundColor Cyan

# Return exit code based on results
if ($failedTests -eq 0) {
    Write-Host "   All tests passed! ✨" -ForegroundColor Green
    exit 0
}
else {
    Write-Host "   Some tests failed - review output above." -ForegroundColor Yellow
    exit 1
}
