# RawrZ Security Platform - Elevated PowerShell Script
# This script starts the application with Administrator privileges

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "RawrZ Security Platform - Elevated Mode" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check if running as administrator
$isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

if ($isAdmin) {
    Write-Host "[OK] Running with Administrator privileges" -ForegroundColor Green
    Write-Host ""
    Write-Host "Starting RawrZ Security Platform..." -ForegroundColor Yellow
    Write-Host ""
    
    # Change to the script directory
    $scriptPath = Split-Path -Parent $MyInvocation.MyCommand.Path
    Set-Location $scriptPath
    Write-Host "[INFO] Changed to directory: $scriptPath" -ForegroundColor Yellow
    
    # Kill any existing Node.js processes
    try {
        Get-Process -Name "node" -ErrorAction SilentlyContinue | Stop-Process -Force
        Write-Host "[INFO] Stopped existing Node.js processes" -ForegroundColor Yellow
    } catch {
        Write-Host "[INFO] No existing Node.js processes found" -ForegroundColor Yellow
    }
    
    # Start the server with elevated privileges
    Write-Host "[INFO] Starting server with full privileges..." -ForegroundColor Green
    node api-server.js
    
} else {
    Write-Host "[ERROR] This script requires Administrator privileges" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please run this script as Administrator:" -ForegroundColor Yellow
    Write-Host "1. Right-click on start-elevated.ps1" -ForegroundColor White
    Write-Host "2. Select 'Run with PowerShell'" -ForegroundColor White
    Write-Host "3. Click 'Yes' when prompted by UAC" -ForegroundColor White
    Write-Host ""
    Write-Host "Alternatively, you can:" -ForegroundColor Yellow
    Write-Host "1. Open PowerShell as Administrator" -ForegroundColor White
    Write-Host "2. Navigate to this directory" -ForegroundColor White
    Write-Host "3. Run: .\start-elevated.ps1" -ForegroundColor White
    Write-Host ""
    Write-Host "Or use the batch file: start-elevated.bat" -ForegroundColor Cyan
    Write-Host ""
    
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host ""
Write-Host "RawrZ Security Platform has stopped." -ForegroundColor Yellow
Read-Host "Press Enter to exit"
