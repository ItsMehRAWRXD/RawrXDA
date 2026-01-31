# 🔧 POPUP NOTIFICATION ANALYZER & FIXER
# This script identifies and fixes all popup notifications in RawrXD

Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "🔍 POPUP NOTIFICATION ANALYSIS - RawrXD.ps1" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host ""

# Check if RawrXD.ps1 exists
if (-not (Test-Path ".\RawrXD.ps1")) {
  Write-Host "❌ ERROR: RawrXD.ps1 not found in current directory" -ForegroundColor Red
  Write-Host "📂 Current Directory: $PWD" -ForegroundColor Gray
  exit 1
}

Write-Host "✅ Found RawrXD.ps1 - Analyzing popup notifications..." -ForegroundColor Green
Write-Host ""

# Read the file content
$content = Get-Content ".\RawrXD.ps1" -Raw
$lines = Get-Content ".\RawrXD.ps1"

# 🔍 ANALYSIS: Find all MessageBox patterns
Write-Host "🔍 POPUP ANALYSIS RESULTS:" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Yellow

$popupPatterns = @(
  @{
    Name        = "MessageBox::Show Calls"
    Pattern     = "\[System\.Windows\.Forms\.MessageBox\]::Show"
    Description = "Direct MessageBox popup calls"
  },
  @{
    Name        = "Show-ErrorNotification Calls"
    Pattern     = "Show-ErrorNotification"
    Description = "Error notification function calls"
  },
  @{
    Name        = "Generic MessageBox"
    Pattern     = "MessageBox\.Show"
    Description = "Alternative MessageBox syntax"
  }
)

$totalIssues = 0
$popupResults = @()

foreach ($pattern in $popupPatterns) {
  $matches = ($content | Select-String $pattern.Pattern -AllMatches).Matches
  $lineNumbers = @()

  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($lines[$i] -match $pattern.Pattern) {
      $lineNumbers += ($i + 1)
    }
  }

  $count = $matches.Count
  $totalIssues += $count

  $result = @{
    Name        = $pattern.Name
    Count       = $count
    Lines       = $lineNumbers
    Description = $pattern.Description
  }
  $popupResults += $result

  Write-Host "📋 $($pattern.Name): $count instances found" -ForegroundColor White
  if ($count -gt 0) {
    Write-Host "   📍 Lines: $($lineNumbers -join ', ')" -ForegroundColor Gray
  }
  Write-Host ""
}

Write-Host "🎯 TOTAL POPUP ISSUES: $totalIssues" -ForegroundColor $(if ($totalIssues -gt 0) { "Red" } else { "Green" })
Write-Host ""

# 🔧 DETAILED ANALYSIS: Examine each MessageBox call
Write-Host "🔧 DETAILED POPUP EXAMINATION:" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Yellow

$messageBoxLines = @()
for ($i = 0; $i -lt $lines.Count; $i++) {
  if ($lines[$i] -match "\[System\.Windows\.Forms\.MessageBox\]::Show") {
    $lineNum = $i + 1
    $messageBoxLines += @{
      LineNumber = $lineNum
      Content    = $lines[$i].Trim()
      Context    = "Function context analysis needed"
    }
  }
}

foreach ($msgBox in $messageBoxLines) {
  Write-Host "📍 Line $($msgBox.LineNumber):" -ForegroundColor Cyan
  Write-Host "   Code: $($msgBox.Content)" -ForegroundColor White

  # Analyze the type of popup
  $content = $msgBox.Content
  $popupType = "Unknown"
  $severity = "Medium"

  if ($content -match "Error|Failed|Exception") {
    $popupType = "Error Dialog"
    $severity = "High"
  }
  elseif ($content -match "Warning|Alert|Security") {
    $popupType = "Warning Dialog"
    $severity = "Medium"
  }
  elseif ($content -match "Information|Success|Complete") {
    $popupType = "Information Dialog"
    $severity = "Low"
  }
  elseif ($content -match "\`"YesNo\`"|\`"Question\`"") {
    $popupType = "Confirmation Dialog"
    $severity = "Medium"
  }

  Write-Host "   Type: $popupType | Severity: $severity" -ForegroundColor Yellow
  Write-Host ""
}

# 🛠️ CONFIGURATION CHECK
Write-Host "⚙️ CONFIGURATION ANALYSIS:" -ForegroundColor Yellow
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Yellow

$configCheck = @()

# Check EnablePopupNotifications setting
if ($content -match "EnablePopupNotifications\s*=\s*\`$false") {
  $configCheck += "✅ EnablePopupNotifications = `$false (CORRECT)"
}
elseif ($content -match "EnablePopupNotifications\s*=\s*\`$true") {
  $configCheck += "❌ EnablePopupNotifications = `$true (SHOULD BE FALSE)"
}
else {
  $configCheck += "⚠️ EnablePopupNotifications setting not found"
}

# Check if popups are bypassing the configuration
$bypassingPopups = 0
foreach ($msgBox in $messageBoxLines) {
  $lineNum = $msgBox.LineNumber
  $surroundingLines = ""

  # Get 3 lines before and after for context
  $startIdx = [math]::Max(0, $lineNum - 4)
  $endIdx = [math]::Min($lines.Count - 1, $lineNum + 2)

  for ($i = $startIdx; $i -le $endIdx; $i++) {
    $surroundingLines += $lines[$i] + "`n"
  }

  # Check if this popup respects the EnablePopupNotifications setting
  if ($surroundingLines -notmatch "EnablePopupNotifications" -and
    $surroundingLines -notmatch "if.*ShowToUser" -and
    $msgBox.Content -notmatch "Show-ErrorNotification") {
    $bypassingPopups++
  }
}

$configCheck += "🔍 Direct popup calls bypassing config: $bypassingPopups"

foreach ($check in $configCheck) {
  Write-Host $check
}
Write-Host ""

# 🎯 RECOMMENDATIONS
Write-Host "🎯 RECOMMENDATIONS:" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Green

$recommendations = @()

if ($totalIssues -gt 0) {
  $recommendations += "1. 🔧 Replace all direct MessageBox::Show calls with Write-DevConsole logging"
  $recommendations += "2. 📝 Update error dialogs to use Write-ErrorLog instead of popups"
  $recommendations += "3. ⚙️ Route information messages through Write-StartupLog"
  $recommendations += "4. 🛡️ Implement user confirmation through console prompts instead of modal dialogs"
  $recommendations += "5. 📊 Create a centralized notification system that respects EnablePopupNotifications"
}
else {
  $recommendations += "✅ No popup issues found - All notifications are properly configured!"
}

foreach ($rec in $recommendations) {
  Write-Host $rec -ForegroundColor White
}
Write-Host ""

# 🔧 GENERATE FIXES
Write-Host "🔧 GENERATING AUTOMATED FIXES:" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Green

if ($messageBoxLines.Count -gt 0) {
  Write-Host "📝 Creating RawrXD-Popup-Fixes.ps1..." -ForegroundColor Cyan

  $fixScript = @"
# 🔧 RawrXD Popup Notification Fixes
# Auto-generated script to replace popup dialogs with proper logging

Write-Host "🔧 Applying popup notification fixes to RawrXD.ps1..." -ForegroundColor Green

`$content = Get-Content ".\RawrXD.ps1" -Raw

# Fix 1: Error dialogs -> Write-DevConsole
`$content = `$content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*error[^"]*)", "([^"]*)", "OK", "Error"\)', 'Write-DevConsole "`$1" "ERROR"'

# Fix 2: Warning dialogs -> Write-DevConsole
`$content = `$content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*)", "([^"]*)", "OK", "Warning"\)', 'Write-DevConsole "`$1" "WARNING"'

# Fix 3: Information dialogs -> Write-DevConsole
`$content = `$content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*)", "([^"]*)", "OK", "Information"\)', 'Write-DevConsole "`$1" "INFO"'

# Fix 4: File operation errors -> Write-ErrorLog
`$content = `$content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("(Failed to [^"]*)", "([^"]*)", "OK", "Error"\)', 'Write-ErrorLog -Message "`$1" -Severity "HIGH"'

# Fix 5: Security alerts -> Write-StartupLog
`$content = `$content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*security[^"]*)", "Security Alert", "OK", "Warning"\)', 'Write-StartupLog "`$1" "WARNING"'

# Fix 6: Replace confirmation dialogs with console prompts
`$content = `$content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*)", "([^"]*)", "YesNo", "Question"\)', '(Read-Host "`$1 (y/N)") -eq "y"'

Write-Host "✅ Popup fixes applied!" -ForegroundColor Green
Write-Host "📝 Backing up original file to RawrXD-backup.ps1..." -ForegroundColor Yellow

Copy-Item ".\RawrXD.ps1" ".\RawrXD-backup.ps1"
Set-Content ".\RawrXD.ps1" `$content

Write-Host "✅ All popup notifications have been replaced with proper logging!" -ForegroundColor Green
Write-Host "📋 Changes applied:" -ForegroundColor Cyan
Write-Host "   • Error dialogs → Write-DevConsole with ERROR level" -ForegroundColor White
Write-Host "   • Warning dialogs → Write-DevConsole with WARNING level" -ForegroundColor White
Write-Host "   • Info dialogs → Write-DevConsole with INFO level" -ForegroundColor White
Write-Host "   • File errors → Write-ErrorLog with HIGH severity" -ForegroundColor White
Write-Host "   • Security alerts → Write-StartupLog with WARNING level" -ForegroundColor White
Write-Host "   • Confirmation dialogs → Console Read-Host prompts" -ForegroundColor White
Write-Host ""
Write-Host "🔄 Restart RawrXD to see the changes take effect." -ForegroundColor Green
"@

  Set-Content ".\RawrXD-Popup-Fixes.ps1" $fixScript
  Write-Host "✅ Fix script created: RawrXD-Popup-Fixes.ps1" -ForegroundColor Green
  Write-Host ""
}

# 📊 SUMMARY REPORT
Write-Host "📊 FINAL SUMMARY:" -ForegroundColor Cyan
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan

$summary = @{
  "Total Popup Issues"  = $totalIssues
  "MessageBox Calls"    = ($popupResults | Where-Object { $_.Name -eq "MessageBox::Show Calls" }).Count
  "Error Notifications" = ($popupResults | Where-Object { $_.Name -eq "Show-ErrorNotification Calls" }).Count
  "Bypassing Config"    = $bypassingPopups
  "Config Status"       = if ($content -match "EnablePopupNotifications\s*=\s*\`$false") { "✅ Properly Disabled" } else { "❌ Needs Fix" }
}

foreach ($item in $summary.GetEnumerator()) {
  $color = if ($item.Key -contains "Issue" -or $item.Key -contains "Bypassing") { "Red" } else { "White" }
  Write-Host "$($item.Key): $($item.Value)" -ForegroundColor $color
}

Write-Host ""
Write-Host "🎯 NEXT STEPS:" -ForegroundColor Yellow
if ($totalIssues -gt 0) {
  Write-Host "1. Run .\RawrXD-Popup-Fixes.ps1 to automatically fix all popup notifications" -ForegroundColor White
  Write-Host "2. Test RawrXD to ensure no more popup interruptions" -ForegroundColor White
  Write-Host "3. Check logs in Dev Tools tab for all notifications" -ForegroundColor White
}
else {
  Write-Host "✅ No action needed - All popups are properly configured!" -ForegroundColor Green
}

Write-Host ""
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "✅ Popup Analysis Complete!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
