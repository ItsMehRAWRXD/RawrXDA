# Direct Launch Visual Studio 2022 with BotBuilder Solution

Write-Host ""
Write-Host "🔷 Visual Studio 2022 - BotBuilder Launcher" -ForegroundColor Cyan
Write-Host "=========================================" -ForegroundColor Cyan
Write-Host ""

# Paths
$VS_PATH = "D:\Microsoft Visual Studio 2022\Enterprise"
$DEVENV_EXE = "$VS_PATH\Common7\IDE\devenv.exe"
$SOLUTION_PATH = "c:\Users\HiH8e\OneDrive\Desktop\Mirai-Source-Code-master\Projects\BotBuilder\BotBuilder.sln"

# Validate paths
Write-Host "✓ Checking Visual Studio..." -ForegroundColor Gray
if (-not (Test-Path $DEVENV_EXE)) {
    Write-Host "❌ Visual Studio not found at: $VS_PATH" -ForegroundColor Red
    exit 1
}
Write-Host "  ✅ Found at: $VS_PATH" -ForegroundColor Green

Write-Host "✓ Checking BotBuilder solution..." -ForegroundColor Gray
if (-not (Test-Path $SOLUTION_PATH)) {
    Write-Host "❌ Solution not found at: $SOLUTION_PATH" -ForegroundColor Red
    exit 1
}
Write-Host "  ✅ Found at: $SOLUTION_PATH" -ForegroundColor Green
Write-Host ""

# Clear cache quickly (optional)
Write-Host "🧹 Clearing cache..." -ForegroundColor Yellow
$cacheDir = "$env:LOCALAPPDATA\Microsoft\VisualStudio"
Get-ChildItem "$cacheDir" -Filter "*17.0*" -Directory -ErrorAction SilentlyContinue | ForEach-Object {
    $componentCache = Join-Path $_.FullName "ComponentModelCache"
    if (Test-Path $componentCache) {
        Remove-Item "$componentCache\*" -Force -ErrorAction SilentlyContinue
    }
}
Write-Host "✅ Cache cleared" -ForegroundColor Green
Write-Host ""

# Launch VS with solution
Write-Host "🚀 Launching Visual Studio..." -ForegroundColor Yellow
Write-Host "    Opening: BotBuilder.sln" -ForegroundColor Gray
Write-Host ""

& "$DEVENV_EXE" "$SOLUTION_PATH"

Write-Host ""
Write-Host "✅ Visual Studio closed" -ForegroundColor Green
