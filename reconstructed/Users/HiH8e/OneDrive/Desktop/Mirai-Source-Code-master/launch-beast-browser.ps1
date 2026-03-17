# Quick Browser Launcher
# Simple PowerShell script to launch your Beast IDE in default browser

param(
    [string]$Url = "file:///C:/Users/HiH8e/OneDrive/Desktop/IDEre2.html"
)

Write-Host "🔥 BigDaddyG Beast IDE Browser Launcher" -ForegroundColor Green
Write-Host "Opening: $Url" -ForegroundColor Cyan

try {
    Start-Process $Url
    Write-Host "✅ Browser launched successfully!" -ForegroundColor Green
} catch {
    Write-Host "❌ Error launching browser: $($_.Exception.Message)" -ForegroundColor Red
}

# Keep window open briefly
Start-Sleep -Seconds 2