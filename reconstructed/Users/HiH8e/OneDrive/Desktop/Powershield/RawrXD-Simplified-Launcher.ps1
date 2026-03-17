#!/usr/bin/env powershell
<#
.SYNOPSIS
    RawrXD Agentic IDE - Simplified Launcher
.DESCRIPTION
    Launches RawrXD with error handling and GUI isolation
#>

$ErrorActionPreference = 'Stop'

Write-Host "╔════════════════════════════════════════════════════════╗" -ForegroundColor Magenta
Write-Host "║  🚀 RawrXD AGENTIC IDE - SIMPLIFIED LAUNCHER 🚀       ║" -ForegroundColor Magenta
Write-Host "╚════════════════════════════════════════════════════════╝" -ForegroundColor Magenta
Write-Host ""

# Set script root
$script:RawrXDPath = if ($PSScriptRoot) { $PSScriptRoot } else { (Get-Location).Path }
$script:MainIDEPath = Join-Path $script:RawrXDPath 'RawrXD.ps1'

# Verify main script exists
if (-not (Test-Path $script:MainIDEPath)) {
    Write-Host "❌ Error: RawrXD.ps1 not found at $script:MainIDEPath" -ForegroundColor Red
    exit 1
}

Write-Host "📍 Loading: $script:MainIDEPath" -ForegroundColor Cyan
Write-Host ""

try {
    # Load and run the main script in isolated scope
    $scriptContent = Get-Content -Path $script:MainIDEPath -Raw
    
    # Invoke the script content in its own scope
    $null = Invoke-Command -ScriptBlock ([scriptblock]::Create($scriptContent)) -ErrorAction Continue
    
} catch {
    Write-Host "❌ Error: $_" -ForegroundColor Red
    Write-Host ""
    Write-Host "💡 Tip: Check the startup log at: $env:APPDATA\RawrXD\startup.log" -ForegroundColor Yellow
    exit 1
}
