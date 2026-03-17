#!/usr/bin/env pwsh
# NASM IDE Swarm Agent - Python Diagnostic and Fix
# This script will diagnose and attempt to fix Python issues

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "NASM IDE Python Diagnostic Tool" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# Check Python launcher
Write-Host "[1/5] Testing Python launcher..." -ForegroundColor Yellow
try {
  $pyVersion = py --version 2>&1
  Write-Host "[✓] Python launcher found: $pyVersion" -ForegroundColor Green
}
catch {
  Write-Host "[✗] Python launcher not found" -ForegroundColor Red
  exit 1
}

# Test core modules
Write-Host "[2/5] Testing core Python modules..." -ForegroundColor Yellow
try {
  $result = py -c "import sys, os, json; print('OK')" 2>&1
  if ($LASTEXITCODE -eq 0) {
    Write-Host "[✓] Basic modules working" -ForegroundColor Green
  }
  else {
    throw "Core modules failed"
  }
}
catch {
  Write-Host "[✗] Core modules broken - Python installation corrupted" -ForegroundColor Red
  Write-Host "Error output: $result" -ForegroundColor Red
    
  Write-Host ""
  Write-Host "RECOMMENDED FIXES:" -ForegroundColor Yellow
  Write-Host "1. Download and reinstall Python from https://python.org" -ForegroundColor White
  Write-Host "2. Make sure to check 'Add Python to PATH'" -ForegroundColor White
  Write-Host "3. Consider using Anaconda or Miniconda instead" -ForegroundColor White
  Write-Host ""
    
  # Try to run without dependencies
  Write-Host "[3/5] Attempting to run swarm components without pip..." -ForegroundColor Yellow
  Write-Host "This will likely fail but may give useful error info" -ForegroundColor Gray
    
  try {
    py swarm_controller.py --version 2>&1 | Out-Null
  }
  catch {
    Write-Host "[✗] Swarm controller cannot run" -ForegroundColor Red
  }
    
  Write-Host ""
  Write-Host "Python installation must be fixed before proceeding." -ForegroundColor Red
  Read-Host "Press Enter to exit"
  exit 1
}

# Test pip
Write-Host "[3/5] Testing pip..." -ForegroundColor Yellow
try {
  py -c "import pip; print('Pip OK')" 2>&1 | Out-Null
  if ($LASTEXITCODE -eq 0) {
    Write-Host "[✓] pip module accessible" -ForegroundColor Green
  }
  else {
    Write-Host "[!] pip module has issues, trying alternative..." -ForegroundColor Yellow
    py -m ensurepip --default-pip 2>&1 | Out-Null
  }
}
catch {
  Write-Host "[✗] pip not working properly" -ForegroundColor Red
}

# Install dependencies
Write-Host "[4/5] Installing dependencies..." -ForegroundColor Yellow
try {
  py -m pip install --user -r requirements.txt
  if ($LASTEXITCODE -eq 0) {
    Write-Host "[✓] Dependencies installed successfully" -ForegroundColor Green
  }
  else {
    throw "pip install failed"
  }
}
catch {
  Write-Host "[✗] Failed to install dependencies" -ForegroundColor Red
  Write-Host "Trying manual install of critical packages..." -ForegroundColor Yellow
    
  $packages = @("asyncio", "psutil", "aiofiles", "websockets")
  foreach ($pkg in $packages) {
    try {
      py -c "import $pkg; print('$pkg OK')" 2>&1 | Out-Null
      if ($LASTEXITCODE -eq 0) {
        Write-Host "[✓] $pkg already available" -ForegroundColor Green
      }
      else {
        Write-Host "[-] $pkg needs installation" -ForegroundColor Yellow
      }
    }
    catch {
      Write-Host "[✗] $pkg not available" -ForegroundColor Red
    }
  }
}

# Test swarm system
Write-Host "[5/5] Testing swarm components..." -ForegroundColor Yellow
if (Test-Path "swarm_controller.py") {
  Write-Host "[✓] Swarm controller found" -ForegroundColor Green
}
else {
  Write-Host "[✗] swarm_controller.py missing" -ForegroundColor Red
}

if (Test-Path "dashboard.py") {
  Write-Host "[✓] Dashboard found" -ForegroundColor Green
}
else {
  Write-Host "[✗] dashboard.py missing" -ForegroundColor Red
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Diagnostic Complete" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan

Read-Host "Press Enter to continue"