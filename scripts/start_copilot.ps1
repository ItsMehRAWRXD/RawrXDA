<#
.SYNOPSIS
    Starts the HexMag Local Copilot Service.
    
.DESCRIPTION
    This script sets up the environment and launches the local HexMag engine.
    It provides a free, private, self-hosted coding assistant API on localhost:8000.
    No external cloud calls. No watermarking. 100% Local.

.EXAMPLE
    .\start_copilot.ps1
#>

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ServiceDir = Join-Path $ScriptDir "..\services"
$HexMagDir = Join-Path $ServiceDir "hexmag"

Write-Host "=== Starting RawrXD Local Copilot ===" -ForegroundColor Cyan

# 1. Check Python
try {
    $pyVersion = python --version 2>&1
    Write-Host "Detected: $pyVersion" -ForegroundColor Gray
} catch {
    Write-Error "Python not found! Please install Python 3.9 or newer."
}

# 2. Install Dependencies (if needed)
Write-Host "Checking dependencies..." -ForegroundColor Gray
try {
    Push-Location $ServiceDir
    pip install -r requirements.txt | Out-Null
    Pop-Location
} catch {
    Write-Warning "Could not install dependencies. Attempting to run anyway..."
}

# 3. Start the Engine
$Port = 8001
Write-Host "Launching HexMag Engine on port $Port..." -ForegroundColor Green
Write-Host "API Endpoint: http://localhost:$Port/ask" -ForegroundColor Gray
Write-Host "Press Ctrl+C to stop." -ForegroundColor Yellow

$Env:PYTHONPATH = "$ServiceDir;$Env:PYTHONPATH"

try {
    python "$HexMagDir\hexmag_engine.py" --port $Port
} catch {
    Write-Error "Failed to start the engine. Check the logs above."
}
