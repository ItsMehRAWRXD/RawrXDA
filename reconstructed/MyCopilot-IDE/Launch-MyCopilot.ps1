# Quick Launcher for MyCopilot IDE
# This script provides easy access to launch the built application

param(
    [Parameter()]
    [ValidateSet('Portable', 'Unpacked', 'Installer')]
    [string]$Version = 'Portable'
)

$BuildDir = 'D:\MyCopilot-IDE\build-20251013-142008'

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  MyCopilot IDE - Quick Launcher" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

switch ($Version) {
    'Portable' {
        $ExePath = Join-Path $BuildDir 'MyCopilot-IDE-Portable-1.0.0.exe'
        Write-Host "Launching Portable Version..." -ForegroundColor Green
    }
    'Unpacked' {
        $ExePath = Join-Path $BuildDir 'win-unpacked\MyCopilot-IDE.exe'
        Write-Host "Launching Unpacked Version..." -ForegroundColor Green
    }
    'Installer' {
        $ExePath = Join-Path $BuildDir 'MyCopilot-IDE-Setup-1.0.0.exe'
        Write-Host "Launching Installer..." -ForegroundColor Yellow
        Write-Host "Note: This will install the application to your system." -ForegroundColor Yellow
    }
}

if (Test-Path $ExePath) {
    Write-Host "Path: $ExePath" -ForegroundColor Gray
    Write-Host ""
    Write-Host "Starting application..." -ForegroundColor Cyan
    
    try {
        Start-Process -FilePath $ExePath
        Write-Host "✓ Application launched successfully!" -ForegroundColor Green
        Write-Host ""
        Write-Host "Tips:" -ForegroundColor Cyan
        Write-Host "  - Press Ctrl+Shift+I to open DevTools and check logs" -ForegroundColor Gray
        Write-Host "  - The UI should show todo tracker and code editor" -ForegroundColor Gray
        Write-Host "  - Try submitting an AI request to test PowerShell integration" -ForegroundColor Gray
    }
    catch {
        Write-Host "✗ Failed to launch: $_" -ForegroundColor Red
        exit 1
    }
}
else {
    Write-Host "✗ Executable not found: $ExePath" -ForegroundColor Red
    Write-Host ""
    Write-Host "Available versions:" -ForegroundColor Yellow
    Write-Host "  .\Launch-MyCopilot.ps1 -Version Portable" -ForegroundColor Gray
    Write-Host "  .\Launch-MyCopilot.ps1 -Version Unpacked" -ForegroundColor Gray
    Write-Host "  .\Launch-MyCopilot.ps1 -Version Installer" -ForegroundColor Gray
    exit 1
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
