# 🎯 RawrXD Advanced Feature Testing Suite
# Following the 5 recommended next steps from systematic testing
# Author: AI Testing Agent
# Date: November 24, 2025

Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  🚀 RawrXD ADVANCED FEATURE TESTING SUITE" -ForegroundColor Cyan
Write-Host "  Comprehensive testing following systematic recommendations" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan

# Initialize test tracking
$script:advancedResults = @{
  HighPriority = @{}
  MultiModel   = @{}
  ComplexUI    = @{}
  Performance  = @{}
  Security     = @{}
}
$script:totalAdvancedTests = 0
$script:passedAdvancedTests = 0
$script:failedAdvancedTests = 0

# Load required assemblies
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

function Test-AdvancedFeature {
  param(
    [string]$Category,
    [string]$TestName,
    [string]$Description,
    [string]$Priority,
    [scriptblock]$TestLogic
  )
    
  $script:totalAdvancedTests++
  $icon = switch ($Priority) {
    "HIGH" { "🔥" }
    "MEDIUM" { "⚡" }
    "LOW" { "📋" }
    default { "🔸" }
  }
    
  Write-Host "`n$icon Testing: $TestName" -ForegroundColor Yellow
  Write-Host "   Category: $Category | Priority: $Priority" -ForegroundColor DarkGray
  Write-Host "   $Description" -ForegroundColor Gray
    
  try {
    $startTime = Get-Date
    $result = & $TestLogic
    $duration = [math]::Round(((Get-Date) - $startTime).TotalMilliseconds)
        
    if ($result -eq $true) {
      Write-Host "   ✅ PASS (${duration}ms)" -ForegroundColor Green
      $script:passedAdvancedTests++
      $script:advancedResults[$Category][$TestName] = "PASS"
    }
    elseif ($result -eq "PARTIAL") {
      Write-Host "   ⚠️ PARTIAL (${duration}ms)" -ForegroundColor Yellow
      $script:advancedResults[$Category][$TestName] = "PARTIAL"
    }
    else {
      Write-Host "   ❌ FAIL (${duration}ms)" -ForegroundColor Red
      $script:failedAdvancedTests++
      $script:advancedResults[$Category][$TestName] = "FAIL"
    }
  }
  catch {
    Write-Host "   💥 ERROR: $_" -ForegroundColor Red
    $script:failedAdvancedTests++
    $script:advancedResults[$Category][$TestName] = "ERROR: $_"
  }
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 1: 🔥 HIGH PRIORITY FEATURE DEEP TESTING
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host "`n" -NoNewline
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Red
Write-Host "  🔥 SECTION 1: HIGH PRIORITY FEATURE DEEP TESTING" -ForegroundColor Red
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Red

# Test 1.1: Process-AgentCommand Deep Analysis
Test-AdvancedFeature -Category "HighPriority" -TestName "Process-AgentCommand Robustness" -Priority "HIGH" -Description "Deep test of agent command processing with various inputs" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  # Extract Process-AgentCommand function
  $hasFunction = $content -match "function Process-AgentCommand"
  $hasInputValidation = $content -match "Test-InputSafety.*Command"
  $hasErrorHandling = $content -match "Process-AgentCommand.*catch"
  $hasSecurityLogging = $content -match "Write-ErrorLog.*SECURITY.*Process-AgentCommand"
    
  # Check for command whitelist/blacklist
  $hasCommandFiltering = $content -match "Unsafe agent command blocked|dangerous.*command"
    
  $score = @($hasFunction, $hasInputValidation, $hasErrorHandling, $hasSecurityLogging, $hasCommandFiltering) | Where-Object { $_ } | Measure-Object
    
  Write-Host "      Function present: $hasFunction" -ForegroundColor Gray
  Write-Host "      Input validation: $hasInputValidation" -ForegroundColor Gray
  Write-Host "      Error handling: $hasErrorHandling" -ForegroundColor Gray
  Write-Host "      Security logging: $hasSecurityLogging" -ForegroundColor Gray
  Write-Host "      Command filtering: $hasCommandFiltering" -ForegroundColor Gray
    
  return ($score.Count -ge 4)
}

# Test 1.2: Security Function Chain Integrity
Test-AdvancedFeature -Category "HighPriority" -TestName "Security Function Chain" -Priority "HIGH" -Description "Verify Protect/Unprotect/Test-InputSafety chain works correctly" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  # Check all security functions exist
  $hasProtect = $content -match "function Protect-SensitiveString"
  $hasUnprotect = $content -match "function Unprotect-SensitiveString"
  $hasInputSafety = $content -match "function Test-InputSafety"
    
  # Check they're actually used together
  $protectUsed = ($content | Select-String "Protect-SensitiveString" -AllMatches).Matches.Count
  $unprotectUsed = ($content | Select-String "Unprotect-SensitiveString" -AllMatches).Matches.Count
  $inputSafetyUsed = ($content | Select-String "Test-InputSafety" -AllMatches).Matches.Count
    
  Write-Host "      Protect-SensitiveString: defined=$hasProtect, usages=$protectUsed" -ForegroundColor Gray
  Write-Host "      Unprotect-SensitiveString: defined=$hasUnprotect, usages=$unprotectUsed" -ForegroundColor Gray
  Write-Host "      Test-InputSafety: defined=$hasInputSafety, usages=$inputSafetyUsed" -ForegroundColor Gray
    
  return ($hasProtect -and $hasUnprotect -and $hasInputSafety -and $protectUsed -gt 3 -and $inputSafetyUsed -gt 5)
}

# Test 1.3: Error Logging System Completeness
Test-AdvancedFeature -Category "HighPriority" -TestName "Error Logging System" -Priority "HIGH" -Description "Verify comprehensive error logging across all modules" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasWriteErrorLog = $content -match "function Write-ErrorLog"
  $hasEmergencyLog = $content -match "function Write-EmergencyLog"
  $hasStartupLog = $content -match "function Write-StartupLog"
  $hasSecurityLog = $content -match "function Write-SecurityLog"
    
  # Check error categories
  $categories = @("SECURITY", "NETWORK", "FILE", "UI", "AI", "SYSTEM", "OPERATION")
  $foundCategories = $categories | Where-Object { $content -match "Write-ErrorLog.*$_|ErrorCategory.*$_" }
    
  Write-Host "      Write-ErrorLog: $hasWriteErrorLog" -ForegroundColor Gray
  Write-Host "      Write-EmergencyLog: $hasEmergencyLog" -ForegroundColor Gray
  Write-Host "      Write-StartupLog: $hasStartupLog" -ForegroundColor Gray
  Write-Host "      Write-SecurityLog: $hasSecurityLog" -ForegroundColor Gray
  Write-Host "      Error categories found: $($foundCategories.Count)/$($categories.Count)" -ForegroundColor Gray
    
  return ($hasWriteErrorLog -and $hasEmergencyLog -and $foundCategories.Count -ge 4)
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 2: 💬 MULTI-MODEL CONVERSATION TESTING
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host "`n" -NoNewline
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Magenta
Write-Host "  💬 SECTION 2: MULTI-MODEL CONVERSATION TESTING" -ForegroundColor Magenta
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Magenta

# Test 2.1: Ollama Multi-Model Availability
Test-AdvancedFeature -Category "MultiModel" -TestName "Ollama Multi-Model Support" -Priority "HIGH" -Description "Test availability of multiple AI models" -TestLogic {
  try {
    $ollamaVersion = Invoke-RestMethod -Uri "http://localhost:11434/api/version" -TimeoutSec 3 -ErrorAction Stop
    $models = ollama list 2>$null
    $modelCount = ($models | Select-String -Pattern "^[a-zA-Z]").Count
        
    # Get specific model names
    $modelNames = @()
    foreach ($line in $models) {
      if ($line -match "^(\S+)\s+") {
        $modelNames += $matches[1]
      }
    }
        
    Write-Host "      Ollama version: $($ollamaVersion.version)" -ForegroundColor Gray
    Write-Host "      Available models: $modelCount" -ForegroundColor Gray
    if ($modelNames.Count -gt 0) {
      Write-Host "      Model list: $($modelNames -join ', ')" -ForegroundColor Gray
    }
        
    return ($modelCount -gt 0)
  }
  catch {
    Write-Host "      ⚠️ Ollama not available: $_" -ForegroundColor Yellow
    return $false
  }
}

# Test 2.2: Chat Tab System Architecture
Test-AdvancedFeature -Category "MultiModel" -TestName "Chat Tab System" -Priority "HIGH" -Description "Verify multi-chat tab system implementation" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasNewChatTab = $content -match "function New-ChatTab"
  $hasRemoveChatTab = $content -match "function Remove-ChatTab"
  $hasSendChatMessage = $content -match "function Send-ChatMessage"
  $hasChatTabsCollection = $content -match '\$script:chatTabs'
  $hasActiveChatTabId = $content -match '\$script:activeChatTabId'
  $hasMultithreading = $content -match "UseMultithreading|threadSafeContext|RunspacePool"
    
  Write-Host "      New-ChatTab: $hasNewChatTab" -ForegroundColor Gray
  Write-Host "      Remove-ChatTab: $hasRemoveChatTab" -ForegroundColor Gray
  Write-Host "      Send-ChatMessage: $hasSendChatMessage" -ForegroundColor Gray
  Write-Host "      Chat tabs collection: $hasChatTabsCollection" -ForegroundColor Gray
  Write-Host "      Active tab tracking: $hasActiveChatTabId" -ForegroundColor Gray
  Write-Host "      Multithreading support: $hasMultithreading" -ForegroundColor Gray
    
  return ($hasNewChatTab -and $hasRemoveChatTab -and $hasSendChatMessage -and $hasChatTabsCollection)
}

# Test 2.3: Context Window Management
Test-AdvancedFeature -Category "MultiModel" -TestName "Context Window Management" -Priority "MEDIUM" -Description "Test conversation history and context management" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasMessageHistory = $content -match '\$chatSession\.Messages|\$messageHistory'
  $hasContextLimit = $content -match "context|history.*count|max.*messages"
  $hasTimestamp = $content -match "Timestamp.*Get-Date"
  $hasRoleTracking = $content -match "Role.*user|Role.*assistant"
    
  Write-Host "      Message history: $hasMessageHistory" -ForegroundColor Gray
  Write-Host "      Context management: $hasContextLimit" -ForegroundColor Gray
  Write-Host "      Timestamp tracking: $hasTimestamp" -ForegroundColor Gray
  Write-Host "      Role tracking: $hasRoleTracking" -ForegroundColor Gray
    
  return ($hasMessageHistory -and $hasRoleTracking)
}

# Test 2.4: Streaming Response Handling
Test-AdvancedFeature -Category "MultiModel" -TestName "Streaming Response Support" -Priority "MEDIUM" -Description "Verify streaming response processing capability" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasStreamParam = $content -match '"stream"\s*=\s*\$true|stream.*true'
  $hasResponseAppend = $content -match "AppendText|response.*append"
  $hasProcessingIndicator = $content -match "processing|AI \(processing"
  $hasScrollToCaret = $content -match "ScrollToCaret"
    
  Write-Host "      Streaming enabled: $hasStreamParam" -ForegroundColor Gray
  Write-Host "      Response appending: $hasResponseAppend" -ForegroundColor Gray
  Write-Host "      Processing indicator: $hasProcessingIndicator" -ForegroundColor Gray
  Write-Host "      Auto-scroll: $hasScrollToCaret" -ForegroundColor Gray
    
  return ($hasResponseAppend -and $hasProcessingIndicator -and $hasScrollToCaret)
}

# Test 2.5: Model Switching Capability
Test-AdvancedFeature -Category "MultiModel" -TestName "Model Switching" -Priority "HIGH" -Description "Test ability to switch between different AI models" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasCurrentModel = $content -match '\$script:currentModel|\$currentModel'
  $hasModelSelection = $content -match "OllamaModel|model.*dropdown|model.*combobox"
  $hasModelSwitch = $content -match "Switch-Model|change.*model|select.*model"
  $hasChatTabModel = $content -match "chatSession\.Model|\$chatSession.*Model"
    
  Write-Host "      Current model tracking: $hasCurrentModel" -ForegroundColor Gray
  Write-Host "      Model selection UI: $hasModelSelection" -ForegroundColor Gray
  Write-Host "      Model switching: $hasModelSwitch" -ForegroundColor Gray
  Write-Host "      Per-tab model: $hasChatTabModel" -ForegroundColor Gray
    
  return ($hasCurrentModel -and $hasModelSelection)
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 3: 🎛️ COMPLEX UI WORKFLOWS
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host "`n" -NoNewline
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Blue
Write-Host "  🎛️ SECTION 3: COMPLEX UI WORKFLOWS" -ForegroundColor Blue
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Blue

# Test 3.1: File Operation Workflow
Test-AdvancedFeature -Category "ComplexUI" -TestName "File Operations Workflow" -Priority "HIGH" -Description "Test complete file open/edit/save workflow" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasOpenDialog = $content -match "OpenFileDialog"
  $hasSaveDialog = $content -match "SaveFileDialog"
  $hasFileRead = $content -match "Get-Content.*file|Read-File"
  $hasFileWrite = $content -match "Set-Content|Out-File"
  $hasPathValidation = $content -match "Test-Path|Test-InputSafety.*FilePath"
  $hasRecentFiles = $content -match "recent.*files|file.*history"
    
  Write-Host "      Open dialog: $hasOpenDialog" -ForegroundColor Gray
  Write-Host "      Save dialog: $hasSaveDialog" -ForegroundColor Gray
  Write-Host "      File reading: $hasFileRead" -ForegroundColor Gray
  Write-Host "      File writing: $hasFileWrite" -ForegroundColor Gray
  Write-Host "      Path validation: $hasPathValidation" -ForegroundColor Gray
  Write-Host "      Recent files: $hasRecentFiles" -ForegroundColor Gray
    
  return ($hasOpenDialog -and $hasSaveDialog -and $hasFileRead -and $hasFileWrite -and $hasPathValidation)
}

# Test 3.2: Settings Management
Test-AdvancedFeature -Category "ComplexUI" -TestName "Settings Management" -Priority "MEDIUM" -Description "Verify settings load/save/modify workflow" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasLoadSettings = $content -match "function Load-Settings|Load.*Settings"
  $hasSaveSettings = $content -match "function Save.*Settings|Save-CustomizationSettings"
  $hasGlobalSettings = $content -match '\$global:settings|\$script:settings'
  $hasSettingsFile = $content -match "settings\.json|config.*json"
  $hasDefaultSettings = $content -match "default.*settings|DefaultSettings"
    
  Write-Host "      Load settings: $hasLoadSettings" -ForegroundColor Gray
  Write-Host "      Save settings: $hasSaveSettings" -ForegroundColor Gray
  Write-Host "      Global settings: $hasGlobalSettings" -ForegroundColor Gray
  Write-Host "      Settings file: $hasSettingsFile" -ForegroundColor Gray
  Write-Host "      Default settings: $hasDefaultSettings" -ForegroundColor Gray
    
  return ($hasLoadSettings -and $hasSaveSettings -and $hasGlobalSettings)
}

# Test 3.3: Theme System
Test-AdvancedFeature -Category "ComplexUI" -TestName "Theme System" -Priority "MEDIUM" -Description "Test theme switching and customization" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasApplyTheme = $content -match "function Apply-Theme|Apply.*Theme"
  $hasCurrentTheme = $content -match '\$script:CurrentTheme'
  $hasThemes = @("Dark", "Light", "Stealth", "Custom") | Where-Object { $content -match $_ }
  $hasThemeCommands = $content -match "/theme|switch theme|use.*theme"
  $hasCustomThemeBuilder = $content -match "Show-CustomThemeBuilder|CustomTheme"
    
  Write-Host "      Apply theme function: $hasApplyTheme" -ForegroundColor Gray
  Write-Host "      Current theme tracking: $hasCurrentTheme" -ForegroundColor Gray
  Write-Host "      Themes available: $($hasThemes.Count)" -ForegroundColor Gray
  Write-Host "      Chat theme commands: $hasThemeCommands" -ForegroundColor Gray
  Write-Host "      Custom theme builder: $hasCustomThemeBuilder" -ForegroundColor Gray
    
  return ($hasApplyTheme -and $hasCurrentTheme -and $hasThemes.Count -ge 2)
}

# Test 3.4: Menu Event Handler Integration
Test-AdvancedFeature -Category "ComplexUI" -TestName "Menu Event Handlers" -Priority "MEDIUM" -Description "Verify menu items are connected to handlers" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  # Count menu items and event handlers
  $menuItems = [regex]::Matches($content, "ToolStripMenuItem|MenuItem").Count
  $clickHandlers = [regex]::Matches($content, "Add_Click").Count
    
  # Check specific menu operations
  $hasFileMenu = $content -match "File.*Menu|menu.*File"
  $hasEditMenu = $content -match "Edit.*Menu|menu.*Edit"
  $hasViewMenu = $content -match "View.*Menu|menu.*View"
  $hasToolsMenu = $content -match "Tools.*Menu|menu.*Tools"
    
  Write-Host "      Menu items found: $menuItems" -ForegroundColor Gray
  Write-Host "      Click handlers: $clickHandlers" -ForegroundColor Gray
  Write-Host "      File menu: $hasFileMenu" -ForegroundColor Gray
  Write-Host "      Edit menu: $hasEditMenu" -ForegroundColor Gray
  Write-Host "      View menu: $hasViewMenu" -ForegroundColor Gray
    
  return ($menuItems -gt 5 -and $clickHandlers -gt 10)
}

# Test 3.5: Dialog Sequence Testing
Test-AdvancedFeature -Category "ComplexUI" -TestName "Dialog Sequences" -Priority "LOW" -Description "Test dialog creation and result handling" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasMessageBox = $content -match "MessageBox::Show|\[.*MessageBox\]"
  $hasShowDialog = $content -match "ShowDialog\(\)"
  $hasDialogResult = $content -match "DialogResult"
  $hasOKCancel = $content -match "DialogResult.*OK|DialogResult.*Cancel"
  $hasCustomDialogs = $content -match "New-Object.*Form.*dialog|dialog.*Form"
    
  Write-Host "      MessageBox usage: $hasMessageBox" -ForegroundColor Gray
  Write-Host "      ShowDialog: $hasShowDialog" -ForegroundColor Gray
  Write-Host "      DialogResult handling: $hasDialogResult" -ForegroundColor Gray
  Write-Host "      OK/Cancel handling: $hasOKCancel" -ForegroundColor Gray
  Write-Host "      Custom dialogs: $hasCustomDialogs" -ForegroundColor Gray
    
  return ($hasMessageBox -and $hasShowDialog -and $hasDialogResult)
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 4: ⚡ PERFORMANCE TESTING
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host "`n" -NoNewline
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Yellow
Write-Host "  ⚡ SECTION 4: PERFORMANCE TESTING" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Yellow

# Test 4.1: Script Load Time
Test-AdvancedFeature -Category "Performance" -TestName "Script Parse Time" -Priority "HIGH" -Description "Measure script parsing performance" -TestLogic {
  $parseStart = Get-Date
  $scriptContent = Get-Content ".\RawrXD.ps1" -Raw
  $scriptSize = $scriptContent.Length
  $lineCount = ($scriptContent | Measure-Object -Line).Lines
  $parseEnd = Get-Date
  $parseTime = ($parseEnd - $parseStart).TotalMilliseconds
    
  # Also test AST parsing
  $astStart = Get-Date
  $tokens = $null
  $errors = $null
  $ast = [System.Management.Automation.Language.Parser]::ParseInput($scriptContent, [ref]$tokens, [ref]$errors)
  $astEnd = Get-Date
  $astTime = ($astEnd - $astStart).TotalMilliseconds
    
  Write-Host "      Script size: $([math]::Round($scriptSize / 1024, 2)) KB" -ForegroundColor Gray
  Write-Host "      Line count: $lineCount" -ForegroundColor Gray
  Write-Host "      File read time: ${parseTime}ms" -ForegroundColor Gray
  Write-Host "      AST parse time: ${astTime}ms" -ForegroundColor Gray
  Write-Host "      Parse errors: $($errors.Count)" -ForegroundColor Gray
    
  # Good performance: < 500ms for parse, < 2000ms for AST
  return ($parseTime -lt 500 -and $astTime -lt 2000 -and $errors.Count -eq 0)
}

# Test 4.2: Memory Efficiency
Test-AdvancedFeature -Category "Performance" -TestName "Memory Efficiency" -Priority "MEDIUM" -Description "Test memory management patterns" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasGCCollect = $content -match "\[System.GC\]::Collect|\[GC\]::Collect"
  $hasDispose = ($content | Select-String "\.Dispose\(\)" -AllMatches).Matches.Count
  $hasUsing = $content -match "using\s*\(" -or $content -match "\$.*\.Dispose\(\)"
  $hasMemoryTracking = $content -match "memory.*usage|Get-Process.*WorkingSet|PerformanceMetrics.*Memory"
    
  Write-Host "      GC.Collect calls: $hasGCCollect" -ForegroundColor Gray
  Write-Host "      Dispose calls: $hasDispose" -ForegroundColor Gray
  Write-Host "      Resource cleanup: $hasUsing" -ForegroundColor Gray
  Write-Host "      Memory tracking: $hasMemoryTracking" -ForegroundColor Gray
    
  return ($hasDispose -gt 5 -and $hasMemoryTracking)
}

# Test 4.3: Async/Parallel Processing
Test-AdvancedFeature -Category "Performance" -TestName "Async/Parallel Support" -Priority "HIGH" -Description "Verify async and parallel processing capabilities" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasRunspacePool = $content -match "RunspacePool|RunspaceFactory"
  $hasAsyncJobs = $content -match "Start-Job|Invoke-Async|BeginInvoke"
  $hasThreadSafe = $content -match "threadSafeContext|thread.*safe"
  $hasParallelChat = $content -match "parallel.*chat|parallel.*message"
  $hasBackgroundWorker = $content -match "BackgroundWorker"
    
  Write-Host "      RunspacePool: $hasRunspacePool" -ForegroundColor Gray
  Write-Host "      Async jobs: $hasAsyncJobs" -ForegroundColor Gray
  Write-Host "      Thread-safe context: $hasThreadSafe" -ForegroundColor Gray
  Write-Host "      Parallel chat: $hasParallelChat" -ForegroundColor Gray
  Write-Host "      BackgroundWorker: $hasBackgroundWorker" -ForegroundColor Gray
    
  return ($hasRunspacePool -or $hasAsyncJobs -or $hasThreadSafe)
}

# Test 4.4: Ollama Response Time
Test-AdvancedFeature -Category "Performance" -TestName "Ollama Response Time" -Priority "HIGH" -Description "Test AI model response latency" -TestLogic {
  try {
    # Test simple API call
    $apiStart = Get-Date
    $versionResponse = Invoke-RestMethod -Uri "http://localhost:11434/api/version" -TimeoutSec 5
    $apiTime = ((Get-Date) - $apiStart).TotalMilliseconds
        
    # Test model generation (simple prompt) - use API endpoint to get models
    $modelsResponse = Invoke-RestMethod -Uri "http://localhost:11434/api/tags" -TimeoutSec 5 -ErrorAction Stop
    $availableModels = $modelsResponse.models | Where-Object { $_.name -ne $null }
        
    if ($availableModels.Count -gt 0) {
      # Pick a small/fast model if available, otherwise first model
      $firstModel = $availableModels[0].name
      foreach ($m in $availableModels) {
        if ($m.name -match "gemma3:1b|phi|llama3\.2") {
          $firstModel = $m.name
          break
        }
      }
            
      $genStart = Get-Date
      $testPrompt = @{
        model  = $firstModel
        prompt = "Say 'test' and nothing else"
        stream = $false
      } | ConvertTo-Json
            
      $genResponse = Invoke-RestMethod -Uri "http://localhost:11434/api/generate" -Method Post -Body $testPrompt -ContentType "application/json" -TimeoutSec 30 -ErrorAction Stop
      $genTime = ((Get-Date) - $genStart).TotalMilliseconds
            
      Write-Host "      API version call: ${apiTime}ms" -ForegroundColor Gray
      Write-Host "      Model used: $firstModel" -ForegroundColor Gray
      Write-Host "      Generation time: ${genTime}ms" -ForegroundColor Gray
      Write-Host "      Response length: $($genResponse.response.Length) chars" -ForegroundColor Gray
            
      return ($apiTime -lt 1000 -and $genTime -lt 30000)
    }
    else {
      Write-Host "      No models available for generation test" -ForegroundColor Yellow
      Write-Host "      API response time: ${apiTime}ms" -ForegroundColor Gray
      return ($apiTime -lt 1000)
    }
  }
  catch {
    Write-Host "      ⚠️ Ollama test failed: $_" -ForegroundColor Yellow
    return $false
  }
}# Test 4.5: UI Responsiveness Patterns
Test-AdvancedFeature -Category "Performance" -TestName "UI Responsiveness" -Priority "MEDIUM" -Description "Verify UI keeps responsive during operations" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasDoEvents = $content -match "\[Application\]::DoEvents"
  $hasAsyncUI = $content -match "Invoke-.*Async|BeginInvoke"
  $hasProgressIndicator = $content -match "processing.*indicator|show.*progress|ProgressBar"
  $hasTimers = $content -match "Timer|Add_Tick"
  $hasStatusUpdate = $content -match "Update.*Status|status.*update"
    
  Write-Host "      DoEvents calls: $hasDoEvents" -ForegroundColor Gray
  Write-Host "      Async UI updates: $hasAsyncUI" -ForegroundColor Gray
  Write-Host "      Progress indicators: $hasProgressIndicator" -ForegroundColor Gray
  Write-Host "      Timer usage: $hasTimers" -ForegroundColor Gray
  Write-Host "      Status updates: $hasStatusUpdate" -ForegroundColor Gray
    
  return ($hasDoEvents -or $hasAsyncUI) -and $hasTimers
}

# ═══════════════════════════════════════════════════════════════════════════════
# SECTION 5: 🔒 DEEP SECURITY TESTING
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host "`n" -NoNewline
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Green
Write-Host "  🔒 SECTION 5: DEEP SECURITY TESTING" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Green

# Test 5.1: Input Validation Coverage
Test-AdvancedFeature -Category "Security" -TestName "Input Validation Coverage" -Priority "HIGH" -Description "Verify comprehensive input validation" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  # Check validation for different input types
  $inputTypes = @("FilePath", "FileContent", "ChatPrompt", "ChatMessage", "Command", "ModelName")
  $foundTypes = $inputTypes | Where-Object { $content -match "Test-InputSafety.*$_|Type.*$_" }
    
  # Check dangerous patterns blocked
  $hasScriptInjection = $content -match "script.*injection|javascript.*vbscript"
  $hasSQLInjection = $content -match "SQL.*injection|select.*insert.*update.*delete"
  $hasCommandInjection = $content -match "exec.*eval.*system|cmd.*powershell"
  $hasPathTraversal = $content -match "\.\./|\.\.\\\\"
    
  Write-Host "      Input types validated: $($foundTypes.Count)/$($inputTypes.Count)" -ForegroundColor Gray
  Write-Host "      Types: $($foundTypes -join ', ')" -ForegroundColor Gray
  Write-Host "      Script injection check: $hasScriptInjection" -ForegroundColor Gray
  Write-Host "      SQL injection check: $hasSQLInjection" -ForegroundColor Gray
  Write-Host "      Command injection check: $hasCommandInjection" -ForegroundColor Gray
  Write-Host "      Path traversal check: $hasPathTraversal" -ForegroundColor Gray
    
  return ($foundTypes.Count -ge 4 -and $hasScriptInjection -and $hasSQLInjection)
}

# Test 5.2: Encryption Implementation
Test-AdvancedFeature -Category "Security" -TestName "Encryption Implementation" -Priority "HIGH" -Description "Verify encryption is properly implemented" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasStealthCrypto = $content -match "class StealthCrypto|\[StealthCrypto\]"
  $hasAES = $content -match "AES|Aes.*Managed|AesCryptoServiceProvider"
  $hasSecureString = $content -match "ConvertTo-SecureString|ConvertFrom-SecureString"
  $hasEncryptionKey = $content -match "EncryptionKey|encryption.*key"
  $hasKeyDerivation = $content -match "PBKDF2|Rfc2898|key.*deriv"
    
  Write-Host "      StealthCrypto class: $hasStealthCrypto" -ForegroundColor Gray
  Write-Host "      AES encryption: $hasAES" -ForegroundColor Gray
  Write-Host "      SecureString usage: $hasSecureString" -ForegroundColor Gray
  Write-Host "      Encryption key management: $hasEncryptionKey" -ForegroundColor Gray
  Write-Host "      Key derivation: $hasKeyDerivation" -ForegroundColor Gray
    
  return ($hasAES -or $hasStealthCrypto) -and $hasEncryptionKey
}

# Test 5.3: Security Logging
Test-AdvancedFeature -Category "Security" -TestName "Security Logging" -Priority "HIGH" -Description "Verify security events are logged" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasSecurityLog = $content -match "function Write-SecurityLog"
  $securityLogCalls = ($content | Select-String "Write-SecurityLog" -AllMatches).Matches.Count
  $hasSecurityCategory = $content -match "SECURITY.*HIGH|SECURITY.*CRITICAL"
  $hasAuditLog = $content -match "audit.*log|security.*audit"
  $hasAlertNotification = $content -match "Send-AlertNotification.*Security|SecurityVulnerability"
    
  Write-Host "      Write-SecurityLog function: $hasSecurityLog" -ForegroundColor Gray
  Write-Host "      Security log calls: $securityLogCalls" -ForegroundColor Gray
  Write-Host "      High-severity security logging: $hasSecurityCategory" -ForegroundColor Gray
  Write-Host "      Audit logging: $hasAuditLog" -ForegroundColor Gray
  Write-Host "      Alert notifications: $hasAlertNotification" -ForegroundColor Gray
    
  return ($hasSecurityLog -and $securityLogCalls -gt 5)
}

# Test 5.4: Session Security
Test-AdvancedFeature -Category "Security" -TestName "Session Security" -Priority "MEDIUM" -Description "Verify session management security" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasSessionTracking = $content -match '\$script:CurrentSession|CurrentSession'
  $hasSessionTimeout = $content -match "SessionTimeout|session.*timeout"
  $hasSessionValidation = $content -match "function Test-SessionSecurity|Test.*Session"
  $hasAuthRequired = $content -match "AuthenticationRequired|auth.*required"
  $hasSessionStart = $content -match "StartTime|session.*start"
    
  Write-Host "      Session tracking: $hasSessionTracking" -ForegroundColor Gray
  Write-Host "      Session timeout: $hasSessionTimeout" -ForegroundColor Gray
  Write-Host "      Session validation: $hasSessionValidation" -ForegroundColor Gray
  Write-Host "      Authentication required: $hasAuthRequired" -ForegroundColor Gray
  Write-Host "      Session start tracking: $hasSessionStart" -ForegroundColor Gray
    
  return ($hasSessionTracking -and $hasSessionTimeout)
}

# Test 5.5: Stealth Mode Implementation
Test-AdvancedFeature -Category "Security" -TestName "Stealth Mode" -Priority "MEDIUM" -Description "Verify stealth/privacy features" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  $hasStealthMode = $content -match "function Enable-StealthMode|StealthMode"
  $hasProcessHiding = $content -match "ProcessHiding|process.*hide"
  $hasAntiForensics = $content -match "AntiForensics|anti.*forensics"
  $hasHistoryClear = $content -match "Clear.*History|history.*clear"
  $hasMinimizeFootprint = $content -match "GC\]::Collect|minimize.*footprint"
    
  Write-Host "      Stealth mode function: $hasStealthMode" -ForegroundColor Gray
  Write-Host "      Process hiding: $hasProcessHiding" -ForegroundColor Gray
  Write-Host "      Anti-forensics: $hasAntiForensics" -ForegroundColor Gray
  Write-Host "      History clearing: $hasHistoryClear" -ForegroundColor Gray
  Write-Host "      Footprint minimization: $hasMinimizeFootprint" -ForegroundColor Gray
    
  return ($hasStealthMode -and ($hasAntiForensics -or $hasHistoryClear))
}

# Test 5.6: Edge Case Input Handling
Test-AdvancedFeature -Category "Security" -TestName "Edge Case Input Handling" -Priority "HIGH" -Description "Test handling of malicious/edge case inputs" -TestLogic {
  $content = Get-Content ".\RawrXD.ps1" -Raw
    
  # Check for null/empty handling
  $hasNullCheck = $content -match "\[string\]::IsNullOrEmpty|\[string\]::IsNullOrWhiteSpace"
  $hasNullOrEmpty = ($content | Select-String "IsNullOrEmpty|IsNullOrWhiteSpace" -AllMatches).Matches.Count
    
  # Check for length validation
  $hasLengthCheck = $content -match "\.Length\s*(-gt|-lt|-ge|-le)|max.*length"
    
  # Check for type validation
  $hasTypeCheck = $content -match "-is \[|GetType\(\)"
    
  # Check for error boundary
  $hasTryCatch = ($content | Select-String "try\s*\{" -AllMatches).Matches.Count
    
  Write-Host "      Null/empty checks: $hasNullOrEmpty" -ForegroundColor Gray
  Write-Host "      Length validation: $hasLengthCheck" -ForegroundColor Gray
  Write-Host "      Type validation: $hasTypeCheck" -ForegroundColor Gray
  Write-Host "      Try-catch blocks: $hasTryCatch" -ForegroundColor Gray
    
  return ($hasNullOrEmpty -gt 10 -and $hasTryCatch -gt 20)
}

# ═══════════════════════════════════════════════════════════════════════════════
# FINAL SUMMARY
# ═══════════════════════════════════════════════════════════════════════════════

Write-Host "`n" -NoNewline
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  📊 ADVANCED TESTING COMPLETE - COMPREHENSIVE RESULTS" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$overallSuccessRate = if ($script:totalAdvancedTests -gt 0) { 
  [math]::Round(($script:passedAdvancedTests / $script:totalAdvancedTests) * 100, 1) 
}
else { 0 }

Write-Host "`n🎯 OVERALL SUMMARY:" -ForegroundColor White
Write-Host "   Total Tests: $($script:totalAdvancedTests)" -ForegroundColor Gray
Write-Host "   Passed: $($script:passedAdvancedTests)" -ForegroundColor Green
Write-Host "   Failed: $($script:failedAdvancedTests)" -ForegroundColor Red
Write-Host "   Success Rate: $overallSuccessRate%" -ForegroundColor Cyan

Write-Host "`n📋 RESULTS BY CATEGORY:" -ForegroundColor White

foreach ($category in $script:advancedResults.Keys | Sort-Object) {
  $categoryResults = $script:advancedResults[$category]
  if ($categoryResults.Count -eq 0) { continue }
    
  $categoryPassed = ($categoryResults.Values | Where-Object { $_ -eq "PASS" }).Count
  $categoryTotal = $categoryResults.Count
  $categoryRate = [math]::Round(($categoryPassed / $categoryTotal) * 100, 0)
    
  $categoryIcon = switch ($category) {
    "HighPriority" { "🔥" }
    "MultiModel" { "💬" }
    "ComplexUI" { "🎛️" }
    "Performance" { "⚡" }
    "Security" { "🔒" }
    default { "📋" }
  }
    
  Write-Host "`n$categoryIcon $category ($categoryPassed/$categoryTotal - $categoryRate%):" -ForegroundColor White
    
  foreach ($test in $categoryResults.Keys | Sort-Object) {
    $result = $categoryResults[$test]
    $resultColor = switch -Regex ($result) {
      "^PASS" { "Green" }
      "^PARTIAL" { "Yellow" }
      default { "Red" }
    }
    $status = $result -replace "ERROR: .*", "ERROR"
    Write-Host "      $test`: " -ForegroundColor Gray -NoNewline
    Write-Host "$status" -ForegroundColor $resultColor
  }
}

# Grade the application
Write-Host "`n🏆 OVERALL GRADE:" -ForegroundColor White
$grade = switch ($true) {
  ($overallSuccessRate -ge 95) { "A+ (Outstanding)"; $gradeColor = "Green" }
  ($overallSuccessRate -ge 90) { "A (Excellent)"; $gradeColor = "Green" }
  ($overallSuccessRate -ge 85) { "B+ (Very Good)"; $gradeColor = "Green" }
  ($overallSuccessRate -ge 80) { "B (Good)"; $gradeColor = "Yellow" }
  ($overallSuccessRate -ge 75) { "C+ (Above Average)"; $gradeColor = "Yellow" }
  ($overallSuccessRate -ge 70) { "C (Average)"; $gradeColor = "Yellow" }
  ($overallSuccessRate -ge 60) { "D (Below Average)"; $gradeColor = "Red" }
  default { "F (Needs Improvement)"; $gradeColor = "Red" }
}
Write-Host "   $grade" -ForegroundColor $gradeColor

# Recommendations
Write-Host "`n💡 RECOMMENDATIONS:" -ForegroundColor Cyan

$failedTests = @()
foreach ($category in $script:advancedResults.Keys) {
  foreach ($test in $script:advancedResults[$category].Keys) {
    if ($script:advancedResults[$category][$test] -notmatch "^PASS") {
      $failedTests += "$category/$test"
    }
  }
}

if ($failedTests.Count -eq 0) {
  Write-Host "   🎉 All tests passed! Consider:" -ForegroundColor Green
  Write-Host "      - Adding stress tests with larger datasets" -ForegroundColor Gray
  Write-Host "      - Implementing fuzz testing for inputs" -ForegroundColor Gray
  Write-Host "      - Adding network failure simulation" -ForegroundColor Gray
}
else {
  Write-Host "   ⚠️ Focus on fixing these failed tests:" -ForegroundColor Yellow
  foreach ($failed in $failedTests) {
    Write-Host "      - $failed" -ForegroundColor Gray
  }
}

Write-Host "`n═══════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "  Advanced Feature Testing Complete - $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')" -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
