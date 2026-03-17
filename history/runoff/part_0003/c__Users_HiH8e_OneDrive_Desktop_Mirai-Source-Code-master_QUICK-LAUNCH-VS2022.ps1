# ⚡ QUICK LAUNCH VS 2022 - BotBuilder Project
# Simple one-command launcher for Visual Studio 2022

param(
    [switch]$Repair = $false
)

Write-Host "🚀 Visual Studio 2022 - BotBuilder Launcher`n" -ForegroundColor Green

# Paths
$VS_PATH = "D:\Microsoft Visual Studio 2022\Common7\IDE\devenv.exe"
$SOLUTION_PATH = "$PSScriptRoot\Projects\BotBuilder\BotBuilder.sln"
$VS_CACHE = "$env:LOCALAPPDATA\Microsoft\VisualStudio"
$MEF_CACHE = "$env:TEMP\VisualStudioComponentCache"

# Verify paths exist
if (-not (Test-Path $VS_PATH)) {
    Write-Host "❌ Visual Studio 2022 not found at D: drive" -ForegroundColor Red
    Write-Host "Trying alternative paths..." -ForegroundColor Yellow
    
    # Try common locations
    $altPaths = @(
        "D:\Microsoft Visual Studio 2022\Community\Common7\IDE\devenv.exe",
        "D:\Microsoft Visual Studio 2022\Professional\Common7\IDE\devenv.exe",
        "C:\Program Files\Microsoft Visual Studio 2022\Community\Common7\IDE\devenv.exe",
        "C:\Program Files\Microsoft Visual Studio 2022\Professional\Common7\IDE\devenv.exe"
    )
    
    $found = $false
    foreach ($path in $altPaths) {
        if (Test-Path $path) {
            $VS_PATH = $path
            $found = $true
            Write-Host "✓ Found VS at: $path" -ForegroundColor Green
            break
        }
    }
    
    if (-not $found) {
        Write-Host "❌ Could not find Visual Studio 2022 anywhere" -ForegroundColor Red
        exit 1
    }
}

if (-not (Test-Path $SOLUTION_PATH)) {
    Write-Host "❌ BotBuilder.sln not found at: $SOLUTION_PATH" -ForegroundColor Red
    exit 1
}

# Step 1: Close any running VS instances
Write-Host "⏹️  Closing any running Visual Studio instances..." -ForegroundColor Cyan
$vsInstances = Get-Process devenv -ErrorAction SilentlyContinue
if ($vsInstances) {
    $vsInstances | Stop-Process -Force -ErrorAction SilentlyContinue
    Start-Sleep -Seconds 2
    Write-Host "✓ Visual Studio instances closed" -ForegroundColor Green
}

# Step 2: Clear cache
Write-Host "`n🧹 Clearing Visual Studio cache..." -ForegroundColor Cyan
try {
    # Component model cache
    Get-ChildItem "$VS_CACHE\17.0_*\ComponentModelCache" -ErrorAction SilentlyContinue | 
        Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
    
    # MEF cache
    if (Test-Path $MEF_CACHE) {
        Remove-Item $MEF_CACHE -Recurse -Force -ErrorAction SilentlyContinue
    }
    New-Item $MEF_CACHE -ItemType Directory -Force -ErrorAction SilentlyContinue | Out-Null
    
    Write-Host "✓ Cache cleared" -ForegroundColor Green
} catch {
    Write-Host "⚠️  Warning: Could not clear all caches (safe to continue)" -ForegroundColor Yellow
}

# Step 3: Optional repair
if ($Repair) {
    Write-Host "`n🔧 Running Visual Studio repair..." -ForegroundColor Cyan
    & $VS_PATH /repair /quiet
    Start-Sleep -Seconds 5
    Write-Host "✓ Repair completed" -ForegroundColor Green
}

# Step 4: Launch VS with solution
Write-Host "`n🎯 Launching Visual Studio 2022..." -ForegroundColor Cyan
Write-Host "   Opening: $SOLUTION_PATH`n" -ForegroundColor Gray

try {
    & $VS_PATH $SOLUTION_PATH
} catch {
    Write-Host "❌ Error launching Visual Studio: $_" -ForegroundColor Red
    exit 1
}

Write-Host "✓ Visual Studio launched successfully" -ForegroundColor Green
