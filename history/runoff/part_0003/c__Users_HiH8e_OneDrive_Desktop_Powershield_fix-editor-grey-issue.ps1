# Fix for Grey/Black Editor Issue
# The problem: Apply-SyntaxHighlighting sets SelectionColor which makes text invisible
# Solution: Temporarily disable syntax highlighting or fix the color assignments

Write-Host "Fixing RawrXD.ps1 editor rendering issue..." -ForegroundColor Cyan

$filePath = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\RawrXD.ps1"
$content = Get-Content $filePath -Raw

# Option 1: Disable syntax highlighting timer (quick fix)
Write-Host "Disabling syntax highlighting auto-trigger..." -ForegroundColor Yellow

# Comment out the syntax highlighting timer initialization
$content = $content -replace '(\$timerRefLocal\.Interval = 500)', '# $1  # DISABLED - Causes text to disappear'
$content = $content -replace '(\$timerRefLocal\.Add_Tick)', '# $1  # DISABLED - Causes text to disappear'

# Save the fixed file
$content | Set-Content $filePath -Force

Write-Host "✓ Fixed! Restart RawrXD.ps1 to test." -ForegroundColor Green
Write-Host ""
Write-Host "What was fixed:" -ForegroundColor Cyan
Write-Host "- Disabled automatic syntax highlighting that was changing SelectionColor" -ForegroundColor White
Write-Host "- This prevents text from turning grey/black when focused" -ForegroundColor White
Write-Host ""
Write-Host "Trade-off:" -ForegroundColor Yellow
Write-Host "- Syntax highlighting won't auto-update as you type" -ForegroundColor White
Write-Host "- You can manually trigger it if needed later" -ForegroundColor White
