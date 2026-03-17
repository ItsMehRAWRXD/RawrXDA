# Visual Studio 2022 Repair & Launch Script
# For D:\Microsoft Visual Studio 2022\Enterprise

Write-Host ""
Write-Host "🔧 Visual Studio 2022 Repair & Launch Script" -ForegroundColor Cyan
Write-Host "==========================================" -ForegroundColor Cyan
Write-Host ""

# Set paths
$VS_PATH = "D:\Microsoft Visual Studio 2022\Enterprise"
$DEVENV_EXE = "$VS_PATH\Common7\IDE\devenv.exe"

# Check if VS exists
if (-not (Test-Path $DEVENV_EXE)) {
    Write-Host "❌ Visual Studio not found at: $VS_PATH" -ForegroundColor Red
    Write-Host "Please check that the path exists" -ForegroundColor Red
    pause
    exit 1
}

Write-Host "✅ Found Visual Studio at: $VS_PATH" -ForegroundColor Green
Write-Host ""

# Clear cache
Write-Host "🧹 Step 1: Clearing Visual Studio cache..." -ForegroundColor Yellow

# Clear component model cache
$cacheDir = "$env:LOCALAPPDATA\Microsoft\VisualStudio"
if (Test-Path $cacheDir) {
    Write-Host "  Clearing VS component cache..."
    Get-ChildItem "$cacheDir" -Filter "*17.0*" -Directory | ForEach-Object {
        $componentCache = Join-Path $_.FullName "ComponentModelCache"
        if (Test-Path $componentCache) {
            Remove-Item "$componentCache\*" -Force -ErrorAction SilentlyContinue
        }
    }
    Write-Host "  ✅ Component cache cleared" -ForegroundColor Green
}

# Clear MEF cache
$mefCache = "$env:TEMP\VisualStudioComponentCache"
if (Test-Path $mefCache) {
    Write-Host "  Clearing MEF cache..."
    Remove-Item $mefCache -Recurse -Force -ErrorAction SilentlyContinue
    New-Item $mefCache -ItemType Directory -Force | Out-Null
    Write-Host "  ✅ MEF cache cleared" -ForegroundColor Green
}

# Clear temporary files
Write-Host "  Clearing temporary files..."
Get-ChildItem "$env:TEMP" -Filter "*vs*" -Directory -ErrorAction SilentlyContinue | 
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "  ✅ Temp files cleared" -ForegroundColor Green
Write-Host ""

# Optional: Run repair
$runRepair = Read-Host "Run VS Repair? (y/n)"
if ($runRepair -eq 'y' -or $runRepair -eq 'Y') {
    Write-Host ""
    Write-Host "⏳ Step 2: Running Visual Studio Repair..." -ForegroundColor Yellow
    Write-Host "  (This may take 5-10 minutes)" -ForegroundColor Gray
    
    & "$DEVENV_EXE" /repair
    
    Write-Host "✅ Repair completed" -ForegroundColor Green
    Write-Host ""
}

# Launch VS
Write-Host "🚀 Step 3: Launching Visual Studio 2022..." -ForegroundColor Yellow
Write-Host ""

& "$DEVENV_EXE"

Write-Host ""
Write-Host "✅ Visual Studio closed" -ForegroundColor Green
