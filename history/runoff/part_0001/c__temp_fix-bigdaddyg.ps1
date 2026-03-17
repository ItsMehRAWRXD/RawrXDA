# Stop Cursor
Write-Host "Stopping Cursor..." -ForegroundColor Yellow
Get-Process -Name "cursor" -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2

# Clear extension cache directories
$paths = @(
    "$env:APPDATA\..\Local\CursorProCache\vscode\extensions",
    "$env:USERPROFILE\.cursor\extensions"
)

foreach ($path in $paths) {
    if (Test-Path $path) {
        Write-Host "Clearing: $path" -ForegroundColor Cyan
        Get-ChildItem $path -Directory | Where-Object {$_.Name -match "bigdaddyg"} | Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    }
}

Write-Host "✓ Old extensions removed" -ForegroundColor Green

# Reinstall the new extension
Write-Host "`nInstalling new BigDaddyG extension..." -ForegroundColor Yellow
$cursor = "E:\Everything\cursor\Cursor.exe"
$ext = "E:\Everything\cursor\extensions\bigdaddyg-copilot-1.0.0"

& $cursor --install-extension $ext --force

Write-Host "`n✓ Complete! Restart Cursor now." -ForegroundColor Green
