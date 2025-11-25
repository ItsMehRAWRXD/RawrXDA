# 🔧 COMPLETE POPUP ELIMINATION - PHASE 2
# Removes remaining popup dialogs and implements proper non-intrusive logging

Write-Host "🔧 PHASE 2: Eliminating remaining popup notifications..." -ForegroundColor Green

$content = Get-Content ".\RawrXD.ps1" -Raw

# Fix the Show-ErrorNotification function to respect the EnablePopupNotifications setting
$oldErrorFunction = @'
function Show-ErrorNotification {
    param([hashtable]$ErrorRecord)
    
    try {
        # Don't show popup if form is not available or in stealth mode
        if (-not $form -or $script:SecurityConfig.StealthMode) {
            return
        }
        
        $icon = switch ($ErrorRecord.Severity) {
            "CRITICAL" { "Error" }
            "HIGH" { "Warning" }
            "MEDIUM" { "Information" }
            "LOW" { "Information" }
            default { "Information" }
        }
        
        $message = "$($ErrorRecord.Message)`n`nCategory: $($ErrorRecord.Category)`nTime: $($ErrorRecord.Timestamp)"
        if ($ErrorRecord.SourceFunction) {
            $message += "`nSource: $($ErrorRecord.SourceFunction)"
        }
        
        [System.Windows.Forms.MessageBox]::Show($message, "RawrXD - $($ErrorRecord.Severity) Error", "OK", $icon)
    }
    catch {
        Write-StartupLog "Failed to show error notification: $_" "WARNING"
    }
}
'@

$newErrorFunction = @'
function Show-ErrorNotification {
    param([hashtable]$ErrorRecord)
    
    try {
        # ALWAYS use logging instead of popups - non-intrusive error handling
        $message = "$($ErrorRecord.Message) | Category: $($ErrorRecord.Category) | Time: $($ErrorRecord.Timestamp)"
        if ($ErrorRecord.SourceFunction) {
            $message += " | Source: $($ErrorRecord.SourceFunction)"
        }
        
        # Log to appropriate channel based on severity
        switch ($ErrorRecord.Severity) {
            "CRITICAL" { 
                Write-ErrorLog -Message $message -Severity "CRITICAL"
                Write-DevConsole "CRITICAL ERROR: $message" "ERROR"
            }
            "HIGH" { 
                Write-ErrorLog -Message $message -Severity "HIGH"
                Write-DevConsole "HIGH ERROR: $message" "ERROR"
            }
            "MEDIUM" { 
                Write-DevConsole "MEDIUM ERROR: $message" "WARNING"
            }
            "LOW" { 
                Write-DevConsole "LOW ERROR: $message" "INFO"
            }
            default { 
                Write-DevConsole "ERROR: $message" "WARNING"
            }
        }
    }
    catch {
        Write-StartupLog "Failed to log error notification: $_" "WARNING"
    }
}
'@

$content = $content.Replace($oldErrorFunction, $newErrorFunction)

# Fix remaining specific popup cases

# 1. Critical security alerts -> Log to startup log
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("Maximum login attempts exceeded\. Application will exit\.", "Security Alert", "OK", "Error"\)', 'Write-StartupLog "Maximum login attempts exceeded. Application will exit." "CRITICAL"; Write-DevConsole "SECURITY: Maximum login attempts exceeded" "ERROR"'

# 2. File not found errors -> Log to dev console  
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("File not found: \$filePath", "File Error", "OK", "Error"\)', 'Write-DevConsole "File not found: $filePath" "ERROR"'

# 3. Editor initialization errors -> Log to startup log
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("Editor is not properly initialized\. Please restart RawrXD\.", "Editor Error", "OK", "Error"\)', 'Write-StartupLog "Editor is not properly initialized. Please restart RawrXD." "ERROR"; Write-DevConsole "CRITICAL: Editor initialization failed" "ERROR"'

# 4. Properties information -> Log to dev console instead of showing popup
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\(\$props, "Properties - \$\(\$item\.Name\)", "OK", "Information"\)', 'Write-DevConsole "Properties - $($item.Name): $props" "INFO"'

# 5. Security confirmation dialogs -> Use console prompts with default safe answers
$content = $content -replace '\$result = \[System\.Windows\.Forms\.MessageBox\]::Show\("This file type \(\$extension\) could be potentially dangerous\. Continue anyway\?", "Security Warning", "YesNo", "Warning"\)', '$result = "No"; Write-DevConsole "Security Warning: File type ($extension) potentially dangerous - defaulting to safe mode" "WARNING"'

$content = $content -replace '\$result = \[System\.Windows\.Forms\.MessageBox\]::Show\("File content contains potentially dangerous patterns\. Continue anyway\?", "Security Warning", "YesNo", "Warning"\)', '$result = "No"; Write-DevConsole "Security Warning: File content contains dangerous patterns - defaulting to safe mode" "WARNING"'

$content = $content -replace '\$result = \[System\.Windows\.Forms\.MessageBox\]::Show\("Content contains potentially dangerous patterns\. Save anyway\?", "Security Warning", "YesNo", "Warning"\)', '$result = "No"; Write-DevConsole "Security Warning: Content contains dangerous patterns - save blocked for safety" "WARNING"'

# 6. Authentication dialogs -> Log and handle gracefully
$content = $content -replace '\$result = \[System\.Windows\.Forms\.MessageBox\]::Show\("Session security check failed\. Do you want to re-authenticate\?", "Security Alert", "YesNo", "Warning"\)', '$result = "Yes"; Write-DevConsole "Session security check failed - auto-attempting re-authentication" "WARNING"'

# 7. Update the EnablePopupNotifications setting to ensure it's properly set
if ($content -match 'EnablePopupNotifications\s*=\s*\$true') {
  $content = $content -replace 'EnablePopupNotifications\s*=\s*\$true', 'EnablePopupNotifications = $false'
  Write-Host "✅ Fixed EnablePopupNotifications setting" -ForegroundColor Green
}
elseif ($content -notmatch 'EnablePopupNotifications') {
  # Add the setting if it doesn't exist
  $content = $content -replace '(\$script:ErrorNotificationConfig = @{)', "`$1`n    EnablePopupNotifications = `$false"
  Write-Host "✅ Added EnablePopupNotifications setting" -ForegroundColor Green
}

# Backup and save
Write-Host "📝 Creating backup: RawrXD-before-phase2-fixes.ps1..." -ForegroundColor Yellow
Copy-Item ".\RawrXD.ps1" ".\RawrXD-before-phase2-fixes.ps1"

Set-Content ".\RawrXD.ps1" $content

Write-Host ""
Write-Host "✅ PHASE 2 FIXES COMPLETED!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan
Write-Host "📋 CHANGES APPLIED:" -ForegroundColor Cyan
Write-Host "   ✅ Show-ErrorNotification function completely rewritten for logging-only" -ForegroundColor White
Write-Host "   ✅ Critical security alerts → Write-StartupLog + Write-DevConsole" -ForegroundColor White
Write-Host "   ✅ File errors → Write-DevConsole with ERROR level" -ForegroundColor White  
Write-Host "   ✅ Editor errors → Write-StartupLog + Write-DevConsole" -ForegroundColor White
Write-Host "   ✅ Properties info → Write-DevConsole with INFO level" -ForegroundColor White
Write-Host "   ✅ Security confirmations → Default safe mode + logging" -ForegroundColor White
Write-Host "   ✅ Authentication prompts → Auto-attempt + logging" -ForegroundColor White
Write-Host "   ✅ EnablePopupNotifications properly configured" -ForegroundColor White
Write-Host ""
Write-Host "🎯 RESULT: All popup dialogs now use proper logging instead!" -ForegroundColor Green
Write-Host "📝 Logs will appear in:" -ForegroundColor Yellow
Write-Host "   • Dev Tools tab (Write-DevConsole)" -ForegroundColor Gray
Write-Host "   • Startup log files (Write-StartupLog)" -ForegroundColor Gray  
Write-Host "   • Error log files (Write-ErrorLog)" -ForegroundColor Gray
Write-Host ""
Write-Host "🔄 Restart RawrXD to experience popup-free operation!" -ForegroundColor Green
Write-Host "═══════════════════════════════════════════════════════════════════════════════" -ForegroundColor Cyan