# 🔧 RawrXD Popup Notification Fixes
# Auto-generated script to replace popup dialogs with proper logging

Write-Host "🔧 Applying popup notification fixes to RawrXD.ps1..." -ForegroundColor Green

$content = Get-Content ".\RawrXD.ps1" -Raw

# Fix 1: Error dialogs -> Write-DevConsole
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*error[^"]*)", "([^"]*)", "OK", "Error"\)', 'Write-DevConsole "$1" "ERROR"'

# Fix 2: Warning dialogs -> Write-DevConsole
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*)", "([^"]*)", "OK", "Warning"\)', 'Write-DevConsole "$1" "WARNING"'

# Fix 3: Information dialogs -> Write-DevConsole
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*)", "([^"]*)", "OK", "Information"\)', 'Write-DevConsole "$1" "INFO"'

# Fix 4: File operation errors -> Write-ErrorLog
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("(Failed to [^"]*)", "([^"]*)", "OK", "Error"\)', 'Write-ErrorLog -Message "$1" -Severity "HIGH"'

# Fix 5: Security alerts -> Write-StartupLog
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*security[^"]*)", "Security Alert", "OK", "Warning"\)', 'Write-StartupLog "$1" "WARNING"'

# Fix 6: Replace confirmation dialogs with console prompts
$content = $content -replace '\[System\.Windows\.Forms\.MessageBox\]::Show\("([^"]*)", "([^"]*)", "YesNo", "Question"\)', '(Read-Host "$1 (y/N)") -eq "y"'

Write-Host "✅ Popup fixes applied!" -ForegroundColor Green
Write-Host "📝 Backing up original file to RawrXD-backup.ps1..." -ForegroundColor Yellow

Copy-Item ".\RawrXD.ps1" ".\RawrXD-backup.ps1"
Set-Content ".\RawrXD.ps1" $content

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
