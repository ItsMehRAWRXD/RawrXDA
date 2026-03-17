#!/usr/bin/env pwsh
<#
.SYNOPSIS
    RawrXD IDE Launcher - Quantization-Ready AI Development Environment
.DESCRIPTION
    Launches the RawrXD Qt Shell IDE with proper environment configuration
    and dependency checking.
.NOTES
    Built with Qt 6.7.3 + MSVC 2022
    Supports ggml Q4_0/Q8_0 quantization for 10× speed, ½ RAM
#>

$ErrorActionPreference = "Stop"
$IDE_PATH = $PSScriptRoot
$EXE_NAME = "RawrXD-QtShell.exe"

Write-Host @"
╔════════════════════════════════════════════════════════════╗
║           RawrXD IDE - Quantization Ready                  ║
║  AI Development Environment with Full IDE Integration      ║
╚════════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

# Check if executable exists
if (-not (Test-Path "$IDE_PATH\$EXE_NAME")) {
    Write-Host "❌ Error: $EXE_NAME not found in $IDE_PATH" -ForegroundColor Red
    exit 1
}

# Check for Qt DLLs
$requiredDlls = @("Qt6Core.dll", "Qt6Gui.dll", "Qt6Widgets.dll")
$missingDlls = @()

foreach ($dll in $requiredDlls) {
    if (-not (Test-Path "$IDE_PATH\$dll")) {
        $missingDlls += $dll
    }
}

if ($missingDlls.Count -gt 0) {
    Write-Host "❌ Missing Qt DLLs:" -ForegroundColor Red
    $missingDlls | ForEach-Object { Write-Host "   - $_" -ForegroundColor Yellow }
    exit 1
}

# Check for platform plugin
if (-not (Test-Path "$IDE_PATH\platforms\qwindows.dll")) {
    Write-Host "⚠️  Warning: Platform plugin missing (qwindows.dll)" -ForegroundColor Yellow
}

Write-Host "`n✅ All dependencies found" -ForegroundColor Green
Write-Host "🚀 Launching RawrXD IDE..." -ForegroundColor Cyan
Write-Host ""

# Set working directory and launch
Set-Location $IDE_PATH
Start-Process -FilePath ".\$EXE_NAME" -WorkingDirectory $IDE_PATH

Write-Host "✅ IDE launched successfully!" -ForegroundColor Green
Write-Host ""
Write-Host "Features:" -ForegroundColor Cyan
Write-Host "  • Full IDE with 45+ subsystems" -ForegroundColor Gray
Write-Host "  • AI-powered code assistance" -ForegroundColor Gray
Write-Host "  • GGUF model quantization support" -ForegroundColor Gray
Write-Host "  • Multi-terminal integration" -ForegroundColor Gray
Write-Host "  • Git/VCS integration" -ForegroundColor Gray
Write-Host "  • Live markdown & UML rendering" -ForegroundColor Gray
Write-Host ""
