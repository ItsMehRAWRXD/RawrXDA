#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD Agentic IDE - GUI Launcher with Assembly Fix
    
.DESCRIPTION
    Launches RawrXD with agentic capabilities in GUI mode.
    Automatically handles .NET assembly loading for Windows.Forms support.
    
.PARAMETER Model
    Ollama model to use (default: bigdaddyg-fast:latest)
    
.PARAMETER Temperature
    Model temperature 0-1 (default: 0.7)
    
.EXAMPLE
    .\RawrXD-Agentic-GUI.ps1
#>

param(
    [string]$Model = 'bigdaddyg-fast:latest',
    [decimal]$Temperature = 0.7
)

$ErrorActionPreference = 'SilentlyContinue'

# Pre-load all required assemblies BEFORE any GUI code
Write-Host "🔧 Loading .NET assemblies..." -ForegroundColor Cyan

try {
    # Load required assemblies in correct order
    Add-Type -AssemblyName System.Windows.Forms | Out-Null
    Add-Type -AssemblyName System.Drawing | Out-Null
    Add-Type -AssemblyName System.Windows.Forms.DataVisualization | Out-Null
    Add-Type -AssemblyName PresentationCore | Out-Null
    Add-Type -AssemblyName PresentationFramework | Out-Null
    
    Write-Host "✅ Assemblies loaded" -ForegroundColor Green
} catch {
    Write-Host "⚠️  Assembly loading warning (non-critical): $($_.Exception.Message)" -ForegroundColor Yellow
}

# Get script directory
$script:RawrXDPath = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$script:ModulePath = Join-Path $script:RawrXDPath 'RawrXD-Agentic-Module.psm1'
$script:IDEPath = Join-Path $script:RawrXDPath 'RawrXD.ps1'

Write-Host "`n" -ForegroundColor White
Write-Host "╔════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║  🚀 RawrXD AGENTIC IDE - GUI LAUNCHER 🚀        ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════╝`n" -ForegroundColor Magenta

# Initialize agentic mode
Write-Host "⚙️  Initializing agentic mode..." -ForegroundColor Cyan

try {
    # Import agentic module
    Import-Module $script:ModulePath -Force -ErrorAction Stop
    Write-Host "✅ Module imported" -ForegroundColor Green
    
    # Enable agentic mode
    Enable-RawrXDAgentic -Model $Model -Temperature $Temperature -ErrorAction Stop
    Write-Host "✅ Agentic mode enabled" -ForegroundColor Green
    
    # Show status
    Write-Host "`n" -ForegroundColor White
    Get-RawrXDAgenticStatus
    
} catch {
    Write-Host "❌ Error initializing agentic mode: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "`nAttempting to launch IDE without agentic mode..." -ForegroundColor Yellow
}

# Launch IDE
Write-Host "`n" -ForegroundColor White
Write-Host "🎯 Launching RawrXD IDE..." -ForegroundColor Cyan
Write-Host "`n"

try {
    # Change to IDE directory for relative paths
    Push-Location $script:RawrXDPath
    
    # Execute IDE with full error handling
    & $script:IDEPath
    
    Pop-Location
} catch {
    Write-Host "❌ Error launching IDE: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "`nAlternative: Use terminal mode" -ForegroundColor Yellow
    Write-Host "  .\Launch-RawrXD-Agentic.ps1 -Terminal`n" -ForegroundColor Cyan
}
