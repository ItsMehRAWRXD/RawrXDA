# 🔍 RawrXD Hidden & Non-Featured Discovery
# Discovers features that exist but aren't documented, promoted, or easily discoverable

Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "  RawrXD Hidden & Non-Featured Discovery v1.0" -ForegroundColor DarkCyan
Write-Host "  Finding UNDOCUMENTED, HIDDEN, and NON-PROMOTED features" -ForegroundColor DarkCyan
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host ""

$discoveryStartTime = Get-Date

# Initialize hidden/non-featured catalogs
$hiddenFeatures = @{
  UndocumentedFunctions   = @()
  HiddenUIElements        = @()
  SecretKeyboardShortcuts = @()
  DebugFeatures           = @()
  DeveloperTools          = @()
  EasterEggs              = @()
  UnusedCode              = @()
  CommentedFeatures       = @()
  ExperimentalFeatures    = @()
}

$totalHiddenFound = 0

function Find-HiddenFunctions {
  param([string]$Content)
    
  Write-Host "`n🔍 Searching for hidden/undocumented functions..." -ForegroundColor Yellow
    
  # Look for functions with suspicious names that might be hidden
  $hiddenPatterns = @(
    'function\s+.*[Dd]ebug.*',
    'function\s+.*[Tt]est.*',
    'function\s+.*[Hh]idden.*',
    'function\s+.*[Ss]ecret.*',
    'function\s+.*[Dd]ev.*',
    'function\s+.*[Ii]nternal.*',
    'function\s+.*[Pp]rivate.*',
    'function\s+.*[Tt]emp.*',
    'function\s+.*[Ee]xperimental.*',
    'function\s+.*[Ll]egacy.*'
  )
    
  foreach ($pattern in $hiddenPatterns) {
    $matches = [regex]::Matches($Content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    foreach ($match in $matches) {
      $functionName = $match.Value -replace 'function\s+', ''
      Write-Host "   🕵️ Found hidden function: $functionName" -ForegroundColor Cyan
      $script:totalHiddenFound++
      $hiddenFeatures.UndocumentedFunctions += @{
        Name    = $functionName
        Type    = "Hidden Function"
        Pattern = $pattern
        Context = $match.Value
      }
    }
  }
    
  # Look for functions with underscore prefixes (often private/hidden)
  $underscoreFunctions = [regex]::Matches($Content, 'function\s+_[a-zA-Z0-9_-]+')
  foreach ($match in $underscoreFunctions) {
    $functionName = $match.Value -replace 'function\s+', ''
    Write-Host "   🔒 Found private function: $functionName" -ForegroundColor DarkCyan
    $script:totalHiddenFound++
    $hiddenFeatures.UndocumentedFunctions += @{
      Name    = $functionName
      Type    = "Private Function"
      Pattern = "Underscore prefix"
      Context = $match.Value
    }
  }
}

function Find-HiddenUIElements {
  param([string]$Content)
    
  Write-Host "`n🔍 Searching for hidden UI elements..." -ForegroundColor Yellow
    
  $hiddenUIPatterns = @{
    "Hidden Panels"   = 'Visible\s*=\s*\$false|\.Hide\(\)'
    "Debug Windows"   = 'debug.*window|debug.*form|debug.*dialog'
    "Admin Menus"     = 'admin.*menu|admin.*item|admin.*strip'
    "Developer Tools" = 'dev.*tool|dev.*menu|developer.*'
    "Test Forms"      = 'test.*form|test.*window|test.*dialog'
    "Hidden Controls" = 'hidden.*control|\.Enabled\s*=\s*\$false'
    "Secret Dialogs"  = 'secret.*dialog|secret.*form|secret.*window'
    "Maintenance UI"  = 'maintenance.*|maint.*form|cleanup.*form'
  }
    
  foreach ($uiType in $hiddenUIPatterns.Keys) {
    $pattern = $hiddenUIPatterns[$uiType]
    $matches = [regex]::Matches($Content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        
    if ($matches.Count -gt 0) {
      Write-Host "   👁️ Found $uiType`: $($matches.Count) instances" -ForegroundColor Cyan
      $script:totalHiddenFound += $matches.Count
            
      foreach ($match in $matches) {
        $hiddenFeatures.HiddenUIElements += @{
          Type     = $uiType
          Pattern  = $pattern
          Context  = $match.Value
          Location = "Line with: $($match.Value.Substring(0, [Math]::Min(50, $match.Value.Length)))"
        }
      }
    }
  }
}

function Find-SecretKeyboardShortcuts {
  param([string]$Content)
    
  Write-Host "`n🔍 Searching for secret keyboard shortcuts..." -ForegroundColor Yellow
    
  # Look for key combinations that might be undocumented
  $keyPatterns = @(
    'Ctrl\+Shift\+[A-Z0-9]',
    'Alt\+Shift\+[A-Z0-9]',
    'Ctrl\+Alt\+[A-Z0-9]',
    'F[0-9]+',
    'Shift\+F[0-9]+',
    'Ctrl\+F[0-9]+',
    'Keys\.[A-Z][0-9]+',
    'KeyCode.*==.*Keys\.'
  )
    
  foreach ($pattern in $keyPatterns) {
    $matches = [regex]::Matches($Content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    foreach ($match in $matches) {
      # Try to find what the shortcut does by looking at surrounding code
      $context = ""
      $lines = $Content.Split("`n")
      for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i] -match [regex]::Escape($match.Value)) {
          $contextStart = [Math]::Max(0, $i - 2)
          $contextEnd = [Math]::Min($lines.Count - 1, $i + 2)
          $context = ($lines[$contextStart..$contextEnd] -join " ").Trim()
          break
        }
      }
            
      Write-Host "   ⌨️ Found secret shortcut: $($match.Value)" -ForegroundColor Cyan
      $script:totalHiddenFound++
      $hiddenFeatures.SecretKeyboardShortcuts += @{
        Shortcut = $match.Value
        Context  = $context.Substring(0, [Math]::Min(100, $context.Length))
        Pattern  = $pattern
      }
    }
  }
}

function Find-DebugFeatures {
  param([string]$Content)
    
  Write-Host "`n🔍 Searching for debug and developer features..." -ForegroundColor Yellow
    
  $debugPatterns = @{
    "Debug Logging"      = 'Write-Debug|debug.*log|log.*debug|\$debug'
    "Verbose Output"     = 'Write-Verbose|verbose.*log|\$verbose'
    "Console Output"     = 'Write-Host.*debug|console.*write|console.*log'
    "Trace Features"     = 'Write-Trace|trace.*log|\$trace'
    "Performance Timing" = 'Measure-Command|stopwatch|timer.*start|timer.*stop'
    "Memory Monitoring"  = 'Get-Process.*memory|\[GC\]|garbage.*collect'
    "Error Testing"      = 'throw.*test|test.*error|simulate.*error'
    "Feature Flags"      = '\$enable.*|\$disable.*|\$flag.*|\$test.*mode'
  }
    
  foreach ($debugType in $debugPatterns.Keys) {
    $pattern = $debugPatterns[$debugType]
    $matches = [regex]::Matches($Content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        
    if ($matches.Count -gt 0) {
      Write-Host "   🐛 Found $debugType`: $($matches.Count) instances" -ForegroundColor Cyan
      $script:totalHiddenFound += $matches.Count
            
      # Show first few examples
      $examples = $matches | Select-Object -First 3
      foreach ($example in $examples) {
        $hiddenFeatures.DebugFeatures += @{
          Type    = $debugType
          Pattern = $pattern
          Example = $example.Value
        }
      }
    }
  }
}

function Find-CommentedFeatures {
  param([string]$Content)
    
  Write-Host "`n🔍 Searching for commented-out features..." -ForegroundColor Yellow
    
  # Look for commented code that might be disabled features
  $commentPatterns = @(
    '#\s*function\s+[a-zA-Z0-9_-]+',
    '#\s*\$[a-zA-Z0-9_]+\s*=.*New-Object',
    '#\s*Add_[A-Za-z]+.*\{',
    '#\s*TODO.*feature|#\s*FEATURE.*TODO',
    '#\s*DISABLED|#\s*COMMENTED|#\s*REMOVED'
  )
    
  foreach ($pattern in $commentPatterns) {
    $matches = [regex]::Matches($Content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    foreach ($match in $matches) {
      Write-Host "   💬 Found commented feature: $($match.Value.Substring(0, [Math]::Min(50, $match.Value.Length)))" -ForegroundColor Yellow
      $script:totalHiddenFound++
      $hiddenFeatures.CommentedFeatures += @{
        Type    = "Commented Code"
        Content = $match.Value
        Pattern = $pattern
      }
    }
  }
}

function Find-EasterEggs {
  param([string]$Content)
    
  Write-Host "`n🔍 Searching for easter eggs and hidden messages..." -ForegroundColor Yellow
    
  $easterEggPatterns = @(
    'easter.*egg|egg.*easter',
    'secret.*message|hidden.*message',
    'konami|up.*up.*down.*down',
    'copyright.*[0-9]{4}|made.*by.*|created.*by.*',
    'version.*[0-9]+\.[0-9]+|build.*[0-9]+',
    'about.*box|about.*dialog|about.*form',
    '\$credits|\$author|\$version',
    'special.*thanks|thank.*you'
  )
    
  foreach ($pattern in $easterEggPatterns) {
    $matches = [regex]::Matches($Content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
    foreach ($match in $matches) {
      Write-Host "   🥚 Found potential easter egg: $($match.Value)" -ForegroundColor Magenta
      $script:totalHiddenFound++
      $hiddenFeatures.EasterEggs += @{
        Type    = "Easter Egg"
        Content = $match.Value
        Pattern = $pattern
      }
    }
  }
}

function Find-UnusedCode {
  param([string]$Content)
    
  Write-Host "`n🔍 Searching for unused/orphaned code..." -ForegroundColor Yellow
    
  # Find function definitions
  $functionMatches = [regex]::Matches($Content, 'function\s+([a-zA-Z0-9_-]+)')
  $definedFunctions = $functionMatches | ForEach-Object { $_.Groups[1].Value }
    
  # Check which functions are never called
  $unusedFunctions = @()
  foreach ($func in $definedFunctions) {
    # Look for calls to this function (excluding the definition line)
    $callPattern = "[^function\s]$func\s*\("
    $calls = [regex]::Matches($Content, $callPattern)
        
    if ($calls.Count -eq 0) {
      Write-Host "   🗑️ Found unused function: $func" -ForegroundColor DarkYellow
      $script:totalHiddenFound++
      $hiddenFeatures.UnusedCode += @{
        Type  = "Unused Function"
        Name  = $func
        Issue = "Function defined but never called"
      }
    }
  }
}

function Find-ExperimentalFeatures {
  param([string]$Content)
    
  Write-Host "`n🔍 Searching for experimental features..." -ForegroundColor Yellow
    
  $experimentalPatterns = @{
    "Beta Features"    = 'beta|Beta|BETA|\$beta'
    "Alpha Features"   = 'alpha|Alpha|ALPHA|\$alpha'
    "Experimental"     = 'experiment|Experiment|EXPERIMENT|\$experiment'
    "Work In Progress" = 'WIP|wip|work.*progress|in.*progress'
    "Future Features"  = 'future|Future|FUTURE|coming.*soon'
    "Proof of Concept" = 'POC|poc|proof.*concept|concept.*proof'
    "Prototype"        = 'prototype|Prototype|PROTOTYPE|\$prototype'
  }
    
  foreach ($expType in $experimentalPatterns.Keys) {
    $pattern = $experimentalPatterns[$expType]
    $matches = [regex]::Matches($Content, $pattern, [System.Text.RegularExpressions.RegexOptions]::IgnoreCase)
        
    if ($matches.Count -gt 0) {
      Write-Host "   🧪 Found $expType`: $($matches.Count) instances" -ForegroundColor Cyan
      $script:totalHiddenFound += $matches.Count
            
      foreach ($match in $matches) {
        $hiddenFeatures.ExperimentalFeatures += @{
          Type    = $expType
          Content = $match.Value
          Pattern = $pattern
        }
      }
    }
  }
}

# Main analysis
Write-Host "🔍 HIDDEN FEATURES DISCOVERY" -ForegroundColor DarkCyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan

if (Test-Path ".\RawrXD.ps1") {
  try {
    $rawrxdContent = Get-Content ".\RawrXD.ps1" -Raw
        
    Write-Host "`n📖 Analyzing RawrXD.ps1 for hidden features..." -ForegroundColor Cyan
        
    Find-HiddenFunctions -Content $rawrxdContent
    Find-HiddenUIElements -Content $rawrxdContent
    Find-SecretKeyboardShortcuts -Content $rawrxdContent
    Find-DebugFeatures -Content $rawrxdContent
    Find-CommentedFeatures -Content $rawrxdContent
    Find-EasterEggs -Content $rawrxdContent
    Find-UnusedCode -Content $rawrxdContent
    Find-ExperimentalFeatures -Content $rawrxdContent
        
  }
  catch {
    Write-Host "❌ Error reading RawrXD.ps1: $_" -ForegroundColor Red
  }
}
else {
  Write-Host "❌ RawrXD.ps1 not found!" -ForegroundColor Red
}

Write-Host "`n📊 HIDDEN FEATURES DISCOVERY RESULTS" -ForegroundColor DarkCyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan

Write-Host "`n🎯 Summary:" -ForegroundColor White
Write-Host "   Total Hidden Features Found: $script:totalHiddenFound" -ForegroundColor Cyan
Write-Host "   Undocumented Functions: $($hiddenFeatures.UndocumentedFunctions.Count)" -ForegroundColor Gray
Write-Host "   Hidden UI Elements: $($hiddenFeatures.HiddenUIElements.Count)" -ForegroundColor Gray
Write-Host "   Secret Shortcuts: $($hiddenFeatures.SecretKeyboardShortcuts.Count)" -ForegroundColor Gray
Write-Host "   Debug Features: $($hiddenFeatures.DebugFeatures.Count)" -ForegroundColor Gray
Write-Host "   Commented Features: $($hiddenFeatures.CommentedFeatures.Count)" -ForegroundColor Gray
Write-Host "   Easter Eggs: $($hiddenFeatures.EasterEggs.Count)" -ForegroundColor Gray
Write-Host "   Unused Code: $($hiddenFeatures.UnusedCode.Count)" -ForegroundColor Gray
Write-Host "   Experimental Features: $($hiddenFeatures.ExperimentalFeatures.Count)" -ForegroundColor Gray

# Detailed reports
if ($hiddenFeatures.UndocumentedFunctions.Count -gt 0) {
  Write-Host "`n🕵️ UNDOCUMENTED FUNCTIONS:" -ForegroundColor Yellow
  foreach ($func in $hiddenFeatures.UndocumentedFunctions) {
    Write-Host "   • $($func.Name) ($($func.Type))" -ForegroundColor Cyan
  }
}

if ($hiddenFeatures.HiddenUIElements.Count -gt 0) {
  Write-Host "`n👁️ HIDDEN UI ELEMENTS:" -ForegroundColor Yellow
  $groupedUI = $hiddenFeatures.HiddenUIElements | Group-Object Type
  foreach ($group in $groupedUI) {
    Write-Host "   • $($group.Name): $($group.Count) instances" -ForegroundColor Cyan
  }
}

if ($hiddenFeatures.SecretKeyboardShortcuts.Count -gt 0) {
  Write-Host "`n⌨️ SECRET KEYBOARD SHORTCUTS:" -ForegroundColor Yellow
  foreach ($shortcut in $hiddenFeatures.SecretKeyboardShortcuts) {
    Write-Host "   • $($shortcut.Shortcut)" -ForegroundColor Cyan
    if ($shortcut.Context.Length -gt 10) {
      Write-Host "     Context: $($shortcut.Context.Substring(0, 60))..." -ForegroundColor Gray
    }
  }
}

if ($hiddenFeatures.DebugFeatures.Count -gt 0) {
  Write-Host "`n🐛 DEBUG & DEVELOPER FEATURES:" -ForegroundColor Yellow
  $groupedDebug = $hiddenFeatures.DebugFeatures | Group-Object Type
  foreach ($group in $groupedDebug) {
    Write-Host "   • $($group.Name): $($group.Count) instances" -ForegroundColor Cyan
  }
}

if ($hiddenFeatures.CommentedFeatures.Count -gt 0) {
  Write-Host "`n💬 COMMENTED-OUT FEATURES:" -ForegroundColor Yellow
  foreach ($commented in $hiddenFeatures.CommentedFeatures) {
    $preview = $commented.Content.Substring(0, [Math]::Min(60, $commented.Content.Length))
    Write-Host "   • $preview..." -ForegroundColor DarkYellow
  }
}

if ($hiddenFeatures.EasterEggs.Count -gt 0) {
  Write-Host "`n🥚 EASTER EGGS & HIDDEN MESSAGES:" -ForegroundColor Magenta
  foreach ($egg in $hiddenFeatures.EasterEggs) {
    Write-Host "   • $($egg.Content)" -ForegroundColor Magenta
  }
}

if ($hiddenFeatures.UnusedCode.Count -gt 0) {
  Write-Host "`n🗑️ UNUSED/ORPHANED CODE:" -ForegroundColor DarkYellow
  foreach ($unused in $hiddenFeatures.UnusedCode) {
    Write-Host "   • $($unused.Name) ($($unused.Type))" -ForegroundColor DarkYellow
    Write-Host "     Issue: $($unused.Issue)" -ForegroundColor Gray
  }
}

if ($hiddenFeatures.ExperimentalFeatures.Count -gt 0) {
  Write-Host "`n🧪 EXPERIMENTAL FEATURES:" -ForegroundColor Yellow
  $groupedExp = $hiddenFeatures.ExperimentalFeatures | Group-Object Type
  foreach ($group in $groupedExp) {
    Write-Host "   • $($group.Name): $($group.Count) instances" -ForegroundColor Cyan
  }
}

$discoveryEndTime = Get-Date
$discoveryDuration = [math]::Round(($discoveryEndTime - $discoveryStartTime).TotalSeconds, 1)

Write-Host "`n📈 HIDDEN FEATURES DISCOVERY COMPLETE!" -ForegroundColor DarkCyan
Write-Host "════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "Duration: $discoveryDuration seconds"
Write-Host "Hidden Features Found: $script:totalHiddenFound"

if ($script:totalHiddenFound -eq 0) {
  Write-Host "`n🎯 TRANSPARENT: No hidden features found! RawrXD is completely transparent." -ForegroundColor Green
}
elseif ($script:totalHiddenFound -le 10) {
  Write-Host "`n🔍 MINIMAL: Few hidden features found, mostly debug/development tools." -ForegroundColor Yellow
}
elseif ($script:totalHiddenFound -le 25) {
  Write-Host "`n👁️ MODERATE: Some hidden features discovered, typical for development software." -ForegroundColor Yellow
}
else {
  Write-Host "`n🕵️ EXTENSIVE: Many hidden features found! Lots of undocumented functionality." -ForegroundColor Cyan
}

Write-Host "`n💡 INSIGHTS:" -ForegroundColor White
Write-Host "   • Hidden features often indicate active development" -ForegroundColor Gray
Write-Host "   • Commented code shows evolution and feature consideration" -ForegroundColor Gray
Write-Host "   • Debug features suggest thorough testing approach" -ForegroundColor Gray
Write-Host "   • Unused code may indicate refactoring opportunities" -ForegroundColor Gray

Write-Host "`n═══════════════════════════════════════════════════════════════" -ForegroundColor DarkCyan
Write-Host "Hidden features discovery complete! Review findings above." -ForegroundColor White
Write-Host "═══════════════════════════════════════════════════════════════" -ForegroundColor DarkCyan