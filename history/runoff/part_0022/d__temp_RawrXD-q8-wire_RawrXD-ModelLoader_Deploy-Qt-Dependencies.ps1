#!/usr/bin/env pwsh
# Deploy Qt Dependencies for RawrXD-QtShell

$ErrorActionPreference = "Stop"

$qtPath = "C:\Qt\6.7.3\msvc2022_64"
$targetDir = "d:\temp\RawrXD-q8-wire\RawrXD-ModelLoader\build\bin\Release"

Write-Host "`n=== Qt Dependency Deployment ===" -ForegroundColor Cyan

# Required Qt DLLs
$requiredDlls = @(
    'Qt6Widgets.dll',
    'Qt6Gui.dll', 
    'Qt6Core.dll',
    'Qt6Network.dll',
    'Qt6Concurrent.dll',
    'Qt6WebSockets.dll'
)

Write-Host "`nCopying Qt DLLs..." -ForegroundColor Yellow
foreach ($dll in $requiredDlls) {
    $src = Join-Path $qtPath "bin\$dll"
    if (Test-Path $src) {
        try {
            Copy-Item $src $targetDir -Force -ErrorAction SilentlyContinue
            Write-Host "  ✓ $dll" -ForegroundColor Green
        } catch {
            Write-Host "  ⚠ $dll (in use, likely already deployed)" -ForegroundColor Yellow
        }
    } else {
        Write-Host "  ✗ $dll (not found)" -ForegroundColor Red
    }
}

# Platform plugin
Write-Host "`nCopying Qt plugins..." -ForegroundColor Yellow
$pluginsDir = Join-Path $targetDir "platforms"
if (-not (Test-Path $pluginsDir)) {
    New-Item -ItemType Directory -Path $pluginsDir -Force | Out-Null
}

$platformPlugin = Join-Path $qtPath "plugins\platforms\qwindows.dll"
if (Test-Path $platformPlugin) {
    Copy-Item $platformPlugin $pluginsDir -Force
    Write-Host "  ✓ platforms\qwindows.dll" -ForegroundColor Green
} else {
    Write-Host "  ✗ platforms\qwindows.dll (not found)" -ForegroundColor Red
}

# Check for MSVC runtime
Write-Host "`nChecking MSVC runtime..." -ForegroundColor Yellow
$vcRedist = @('vcruntime140.dll', 'msvcp140.dll', 'vcruntime140_1.dll')
$foundRuntime = $false
foreach ($dll in $vcRedist) {
    if (Test-Path (Join-Path $targetDir $dll)) {
        Write-Host "  ✓ $dll (found)" -ForegroundColor Green
        $foundRuntime = $true
    }
}

if (-not $foundRuntime) {
    Write-Host "  ⚠ MSVC runtime DLLs not found in target directory" -ForegroundColor Yellow
    Write-Host "    These are usually provided by Visual Studio or VC++ Redistributable" -ForegroundColor Gray
}

Write-Host "`n=== Deployment Summary ===" -ForegroundColor Cyan
$dllCount = (Get-ChildItem $targetDir -Filter "Qt6*.dll").Count
Write-Host "Qt DLLs deployed: $dllCount" -ForegroundColor White
Write-Host "Platform plugin: $(if (Test-Path (Join-Path $pluginsDir 'qwindows.dll')) { 'Yes' } else { 'No' })" -ForegroundColor White
Write-Host "Executable: RawrXD-QtShell.exe" -ForegroundColor White

Write-Host "`n✓ Deployment complete! RawrXD-QtShell should now launch successfully.`n" -ForegroundColor Green
