#!/usr/bin/env pwsh
# RawrXD Win32 IDE Launcher
# Version 1.0.0

$ErrorActionPreference = "Stop"

Write-Host "====================================" -ForegroundColor Cyan
Write-Host " RawrXD Win32 IDE" -ForegroundColor Cyan
Write-Host " Version 1.0.0" -ForegroundColor Cyan
Write-Host "====================================" -ForegroundColor Cyan
Write-Host ""

# Set deployment root
$DeployRoot = Split-Path -Parent $PSCommandPath

# Check for binary
$BinaryPath = Join-Path $DeployRoot "bin\AgenticIDEWin.exe"
if (!(Test-Path $BinaryPath)) {
    Write-Host "[ERROR] AgenticIDEWin.exe not found in bin directory" -ForegroundColor Red
    exit 1
}

# Check for config
$ConfigDir = Join-Path $env:APPDATA "RawrXD"
$ConfigPath = Join-Path $ConfigDir "config.json"
if (!(Test-Path $ConfigPath)) {
    Write-Host "[INFO] First run detected - creating config directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Force -Path $ConfigDir | Out-Null
    
    $DefaultConfig = Join-Path $DeployRoot "config\config.json"
    if (Test-Path $DefaultConfig) {
        Copy-Item $DefaultConfig $ConfigPath
        Write-Host "[OK] Configuration file created" -ForegroundColor Green
    }
}

# Check for logs directory
$LogsDir = Join-Path $env:LOCALAPPDATA "RawrXD\logs"
if (!(Test-Path $LogsDir)) {
    Write-Host "[INFO] Creating logs directory..." -ForegroundColor Yellow
    New-Item -ItemType Directory -Force -Path $LogsDir | Out-Null
}

# Check for required environment variables
if (!$env:OPENAI_API_KEY) {
    Write-Host ""
    Write-Host "[WARNING] OPENAI_API_KEY environment variable not set" -ForegroundColor Yellow
    Write-Host "AI features will not be available" -ForegroundColor Yellow
    Write-Host "To enable AI features, set the environment variable:" -ForegroundColor Yellow
    Write-Host '  $env:OPENAI_API_KEY = "your-api-key-here"' -ForegroundColor White
    Write-Host '  [Environment]::SetEnvironmentVariable("OPENAI_API_KEY", "your-key", "User")' -ForegroundColor White
    Write-Host ""
}

# Check for Node.js
if (!(Get-Command node -ErrorAction SilentlyContinue)) {
    Write-Host "[WARNING] Node.js not found in PATH" -ForegroundColor Yellow
    Write-Host "AI orchestration features may not work" -ForegroundColor Yellow
    Write-Host "Please install Node.js from: https://nodejs.org/" -ForegroundColor Yellow
    Write-Host ""
}

# Launch IDE
Write-Host "[INFO] Launching RawrXD Win32 IDE..." -ForegroundColor Yellow
Write-Host ""

try {
    $Process = Start-Process -FilePath $BinaryPath -ArgumentList $args -PassThru
    
    # Wait a moment to check if it started
    Start-Sleep -Seconds 2
    
    if (!$Process.HasExited) {
        Write-Host "[OK] IDE launched successfully (PID: $($Process.Id))" -ForegroundColor Green
    } else {
        Write-Host "[ERROR] IDE exited immediately (Exit Code: $($Process.ExitCode))" -ForegroundColor Red
        Write-Host "Check logs at: $LogsDir" -ForegroundColor Yellow
        exit 1
    }
}
catch {
    Write-Host "[ERROR] Failed to launch IDE: $_" -ForegroundColor Red
    Write-Host "Check logs at: $LogsDir" -ForegroundColor Yellow
    exit 1
}

exit 0
