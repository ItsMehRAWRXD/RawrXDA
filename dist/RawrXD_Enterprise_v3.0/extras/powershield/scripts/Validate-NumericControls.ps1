#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Comprehensive NumericUpDown Validation Checker for RawrXD
.DESCRIPTION
    This script analyzes all NumericUpDown controls in RawrXD.ps1 to identify
    potential value validation issues and ensures proper min/max ranges.
.NOTES
    Author: PowerShield Team
    Version: 1.0
#>

param(
  [string]$SourceFile = ".\RawrXD.ps1",
  [switch]$ShowDetails,
  [switch]$FixIssues
)

Write-Host "🔧 NUMERICUPDOWN VALIDATION ANALYZER" -ForegroundColor Cyan
Write-Host "=" * 60

if (-not (Test-Path $SourceFile)) {
  Write-Error "Source file not found: $SourceFile"
  exit 1
}

$content = Get-Content $SourceFile
$numericControls = @()
$settingsValues = @()
$issues = @()

Write-Host "📄 Analyzing: $SourceFile" -ForegroundColor Green
Write-Host "📊 Total Lines: $($content.Length)" -ForegroundColor Yellow
Write-Host ""

# Step 1: Find all NumericUpDown controls
Write-Host "🔍 STEP 1: Identifying NumericUpDown Controls" -ForegroundColor White
Write-Host "-" * 50

for ($i = 0; $i -lt $content.Length; $i++) {
  $line = $content[$i]
  $lineNumber = $i + 1

  if ($line -match '\$(\w+) = New-Object System\.Windows\.Forms\.NumericUpDown') {
    $controlName = $Matches[1]

    # Scan next 10 lines for properties
    $control = @{
      Name        = $controlName
      LineNumber  = $lineNumber
      Minimum     = $null
      Maximum     = $null
      Value       = $null
      ValueSource = $null
      Context     = "Unknown"
    }

    for ($j = $i + 1; $j -lt [Math]::Min($i + 15, $content.Length); $j++) {
      $propLine = $content[$j]

      if ($propLine -match "\`$$controlName\.Minimum\s*=\s*(\d+)") {
        $control.Minimum = [int]$Matches[1]
      }
      elseif ($propLine -match "\`$$controlName\.Maximum\s*=\s*(\d+)") {
        $control.Maximum = [int]$Matches[1]
      }
      elseif ($propLine -match "\`$$controlName\.Value\s*=\s*(.+)$") {
        $control.ValueSource = $Matches[1].Trim()
      }

      # Determine context based on nearby code
      if ($propLine -match "security|Security" -or $line -match "security|Security") {
        $control.Context = "Security Settings"
      }
      elseif ($propLine -match "chat|Chat" -or $line -match "chat|Chat") {
        $control.Context = "Chat Settings"
      }
      elseif ($propLine -match "tab|Tab" -or $line -match "tab|Tab") {
        $control.Context = "Tab Settings"
      }
      elseif ($propLine -match "editor|Editor" -or $line -match "editor|Editor") {
        $control.Context = "Editor Settings"
      }
    }

    $numericControls += $control
    Write-Host "  ✅ Found: $($control.Name) (Line $lineNumber) - $($control.Context)" -ForegroundColor Green
  }
}

# Step 2: Find settings values that might conflict
Write-Host ""
Write-Host "🔍 STEP 2: Analyzing Settings Values" -ForegroundColor White
Write-Host "-" * 50

$settingsPatterns = @(
  'MaxTabs\s*=\s*(\d+)',
  'MaxChatTabs\s*=\s*(\d+)',
  'MaxLoginAttempts\s*=\s*(\d+)',
  'SessionTimeout\s*=\s*(\d+)',
  'AutoSaveInterval\s*=\s*(\d+)',
  'EditorFontSize\s*=\s*(\d+)',
  'TabSize\s*=\s*(\d+)',
  'MaxErrorsPerMinute\s*=\s*(\d+)'
)

foreach ($pattern in $settingsPatterns) {
  for ($i = 0; $i -lt $content.Length; $i++) {
    $line = $content[$i]
    if ($line -match $pattern) {
      $settingName = ($pattern -split '\\')[0]
      $settingValue = [int]$Matches[1]

      $settingsValues += @{
        Name       = $settingName
        Value      = $settingValue
        LineNumber = $i + 1
        Line       = $line.Trim()
      }

      Write-Host "  📊 $settingName = $settingValue (Line $($i + 1))" -ForegroundColor Gray
    }
  }
}

# Step 3: Cross-reference and find potential issues
Write-Host ""
Write-Host "🚨 STEP 3: Identifying Potential Issues" -ForegroundColor White
Write-Host "-" * 50

foreach ($control in $numericControls) {
  if ($control.ValueSource) {
    # Try to find matching settings
    $relatedSettings = $settingsValues | Where-Object {
      $control.ValueSource -like "*$($_.Name)*"
    }

    foreach ($setting in $relatedSettings) {
      $hasIssue = $false
      $issueType = ""

      if ($control.Minimum -ne $null -and $setting.Value -lt $control.Minimum) {
        $hasIssue = $true
        $issueType += "VALUE_BELOW_MINIMUM "
      }

      if ($control.Maximum -ne $null -and $setting.Value -gt $control.Maximum) {
        $hasIssue = $true
        $issueType += "VALUE_ABOVE_MAXIMUM "
      }

      if ($control.Minimum -eq $null) {
        $hasIssue = $true
        $issueType += "NO_MINIMUM_SET "
      }

      if ($control.Maximum -eq $null) {
        $hasIssue = $true
        $issueType += "NO_MAXIMUM_SET "
      }

      if ($hasIssue) {
        $issue = @{
          ControlName  = $control.Name
          Context      = $control.Context
          ControlLine  = $control.LineNumber
          SettingName  = $setting.Name
          SettingValue = $setting.Value
          SettingLine  = $setting.LineNumber
          ControlMin   = $control.Minimum
          ControlMax   = $control.Maximum
          IssueType    = $issueType.Trim()
          Severity     = if ($issueType -like "*ABOVE_MAXIMUM*" -or $issueType -like "*BELOW_MINIMUM*") { "HIGH" } else { "MEDIUM" }
        }

        $issues += $issue
      }
    }
  }
}

# Step 4: Report results
Write-Host ""
Write-Host "📋 VALIDATION RESULTS" -ForegroundColor Cyan
Write-Host "=" * 60

if ($issues.Count -eq 0) {
  Write-Host "✅ No validation issues found!" -ForegroundColor Green
}
else {
  Write-Host "⚠️ Found $($issues.Count) potential validation issues:" -ForegroundColor Yellow
  Write-Host ""

  $highIssues = $issues | Where-Object { $_.Severity -eq "HIGH" }
  $mediumIssues = $issues | Where-Object { $_.Severity -eq "MEDIUM" }

  if ($highIssues) {
    Write-Host "🔴 HIGH SEVERITY ISSUES:" -ForegroundColor Red
    foreach ($issue in $highIssues) {
      Write-Host "  ❌ $($issue.ControlName) ($($issue.Context))" -ForegroundColor Red
      Write-Host "     Setting: $($issue.SettingName) = $($issue.SettingValue)" -ForegroundColor Gray
      Write-Host "     Control Range: $($issue.ControlMin) - $($issue.ControlMax)" -ForegroundColor Gray
      Write-Host "     Issue: $($issue.IssueType)" -ForegroundColor Gray
      Write-Host "     Lines: Control=$($issue.ControlLine), Setting=$($issue.SettingLine)" -ForegroundColor Gray
      Write-Host ""
    }
  }

  if ($mediumIssues) {
    Write-Host "🟡 MEDIUM SEVERITY ISSUES:" -ForegroundColor Yellow
    foreach ($issue in $mediumIssues) {
      Write-Host "  ⚠️ $($issue.ControlName) ($($issue.Context))" -ForegroundColor Yellow
      Write-Host "     Issue: $($issue.IssueType)" -ForegroundColor Gray
      Write-Host ""
    }
  }
}

# Step 5: Show summary
Write-Host ""
Write-Host "📊 SUMMARY" -ForegroundColor Cyan
Write-Host "=" * 60
Write-Host "📝 NumericUpDown Controls Found: $($numericControls.Count)" -ForegroundColor White
Write-Host "⚙️ Settings Values Found: $($settingsValues.Count)" -ForegroundColor White
Write-Host "🚨 Validation Issues: $($issues.Count)" -ForegroundColor $(if ($issues.Count -eq 0) { "Green" } else { "Red" })

if ($ShowDetails) {
  Write-Host ""
  Write-Host "📋 DETAILED ANALYSIS" -ForegroundColor Cyan
  Write-Host "=" * 60

  Write-Host ""
  Write-Host "🎛️ All NumericUpDown Controls:" -ForegroundColor White
  foreach ($control in $numericControls) {
    Write-Host "  📍 $($control.Name) (Line $($control.LineNumber))" -ForegroundColor Gray
    Write-Host "     Context: $($control.Context)" -ForegroundColor DarkGray
    Write-Host "     Range: $($control.Minimum) - $($control.Maximum)" -ForegroundColor DarkGray
    Write-Host "     Value Source: $($control.ValueSource)" -ForegroundColor DarkGray
    Write-Host ""
  }

  Write-Host "⚙️ All Settings Values:" -ForegroundColor White
  foreach ($setting in $settingsValues) {
    Write-Host "  📊 $($setting.Name) = $($setting.Value) (Line $($setting.LineNumber))" -ForegroundColor Gray
  }
}

# Step 6: Recommendations
Write-Host ""
Write-Host "💡 RECOMMENDATIONS" -ForegroundColor Cyan
Write-Host "=" * 60

if ($issues.Count -gt 0) {
  Write-Host "1. 🔧 Fix high-severity validation range conflicts immediately" -ForegroundColor White
  Write-Host "2. 🎛️ Ensure all NumericUpDown controls have appropriate min/max values" -ForegroundColor White
  Write-Host "3. ⚙️ Review settings initialization to prevent conflicts" -ForegroundColor White
  Write-Host "4. 🧪 Test all settings dialogs with edge case values" -ForegroundColor White
  Write-Host "5. 📝 Add validation error handling for out-of-range values" -ForegroundColor White
}
else {
  Write-Host "✅ NumericUpDown validation looks good!" -ForegroundColor Green
  Write-Host "🧪 Consider testing edge cases in settings dialogs" -ForegroundColor White
  Write-Host "📝 Document valid ranges for each setting" -ForegroundColor White
}

Write-Host ""
Write-Host "✨ Analysis completed!" -ForegroundColor Green
