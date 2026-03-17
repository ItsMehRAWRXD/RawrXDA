#!/usr/bin/env pwsh
# NASM IDE Swarm Launch - Fixed for Experimental Python
# Handles experimental/incomplete Python builds gracefully

param(
  [switch]$Force,
  [switch]$Simple,
  [string]$Mode = "auto"
)

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "NASM IDE Swarm Agent System" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Get Python executable path
try {
  $pythonExe = py -c "import sys; print(sys.executable)" 2>&1
  Write-Host "Python executable: $pythonExe" -ForegroundColor Gray
}
catch {
  Write-Host "[✗] Python not found" -ForegroundColor Red
  exit 1
}

# Check if experimental build
$isExperimental = $pythonExe -like "*python3.13t.exe*"

if ($isExperimental) {
  Write-Host "[!] EXPERIMENTAL PYTHON DETECTED" -ForegroundColor Yellow
  Write-Host "    Free-threading build missing core modules" -ForegroundColor Yellow
  Write-Host ""
    
  if ($Simple -or $Mode -eq "simple") {
    Write-Host "[MODE] Forced simple mode" -ForegroundColor Green
    & $PSScriptRoot\run-simple-swarm.ps1
    return
  }
    
  Write-Host "Available options:" -ForegroundColor White
  Write-Host "  1. Run simplified swarm (basic functionality)" -ForegroundColor Green
  Write-Host "  2. Get Python installation help" -ForegroundColor Blue
  Write-Host "  3. Exit" -ForegroundColor Gray
  Write-Host ""
    
  do {
    $choice = Read-Host "Choose option (1-3)"
  } while ($choice -notin @("1", "2", "3"))
    
  switch ($choice) {
    "1" {
      Write-Host "[LAUNCH] Starting simplified mode..." -ForegroundColor Green
      & $PSScriptRoot\run-simple-swarm.ps1
    }
    "2" {
      Write-Host ""
      Write-Host "========================================" -ForegroundColor Yellow
      Write-Host "Python Installation Guide" -ForegroundColor Yellow
      Write-Host "========================================" -ForegroundColor Yellow
      Write-Host ""
      Write-Host "PROBLEM:" -ForegroundColor Red
      Write-Host "Your Python is an experimental free-threading build" -ForegroundColor White
      Write-Host "It's missing: asyncio, logging, typing, dataclasses" -ForegroundColor White
      Write-Host ""
      Write-Host "SOLUTION 1 - Standard Python:" -ForegroundColor Green
      Write-Host "1. Download from https://python.org/downloads/" -ForegroundColor White
      Write-Host "2. Choose Python 3.11.x or 3.12.x (stable)" -ForegroundColor White
      Write-Host "3. Check 'Add Python to PATH' during install" -ForegroundColor White
      Write-Host "4. Restart this script" -ForegroundColor White
      Write-Host ""
      Write-Host "SOLUTION 2 - Anaconda:" -ForegroundColor Green
      Write-Host "1. Install from https://anaconda.com" -ForegroundColor White
      Write-Host "2. Run: conda create -n nasm python=3.11" -ForegroundColor White
      Write-Host "3. Run: conda activate nasm" -ForegroundColor White
      Write-Host "4. Restart this script" -ForegroundColor White
      Write-Host ""
    }
    "3" {
      Write-Host "Exiting..." -ForegroundColor Gray
      return
    }
  }
}
else {
  Write-Host "[✓] Standard Python detected" -ForegroundColor Green
    
  # Test core modules
  Write-Host "[1/4] Testing core modules..." -ForegroundColor Yellow
  try {
    py -c "import asyncio, logging, typing, dataclasses" 2>$null
    Write-Host "[✓] Core modules available" -ForegroundColor Green
  }
  catch {
    Write-Host "[✗] Core modules missing - Python may be corrupted" -ForegroundColor Red
    Write-Host "Falling back to simple mode..." -ForegroundColor Yellow
    & $PSScriptRoot\run-simple-swarm.ps1
    return
  }
    
  # Install dependencies
  Write-Host "[2/4] Installing dependencies..." -ForegroundColor Yellow
  try {
    py -m pip install -q -r requirements.txt
    Write-Host "[✓] Dependencies installed" -ForegroundColor Green
  }
  catch {
    Write-Host "[!] Some dependencies failed - continuing anyway" -ForegroundColor Yellow
  }
    
  # Launch full system
  Write-Host "[3/4] Starting swarm controller..." -ForegroundColor Yellow
  Start-Job -ScriptBlock {
    Set-Location $using:PWD
    py swarm_controller.py
  } -Name "SwarmController" | Out-Null
    
  Start-Sleep -Seconds 3
    
  Write-Host "[4/4] Starting dashboard..." -ForegroundColor Yellow
  Start-Job -ScriptBlock {
    Set-Location $using:PWD
    py dashboard.py
  } -Name "Dashboard" | Out-Null
    
  Start-Sleep -Seconds 2
    
  # Open browser
  Start-Process "http://localhost:8080"
    
  Write-Host ""
  Write-Host "[✓] Full swarm system launched!" -ForegroundColor Green
  Write-Host "    Dashboard: http://localhost:8080" -ForegroundColor White
  Write-Host ""
}