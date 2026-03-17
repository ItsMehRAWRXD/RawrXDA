#!/usr/bin/env pwsh
<#
.SYNOPSIS
    Quick Start - Launch RawrXD and Run Tests
.DESCRIPTION
    One-command setup to launch RawrXD IDE and testing suite
#>

param(
    [switch]$TestOnly = $false,
    [switch]$NoCleanup = $false
)

$PSDefaultParameterValues['Out-String:Width'] = 120

# Colors
$Colors = @{
    Green  = [System.ConsoleColor]::Green
    Red    = [System.ConsoleColor]::Red
    Yellow = [System.ConsoleColor]::Yellow
    Cyan   = [System.ConsoleColor]::Cyan
    White  = [System.ConsoleColor]::White
    Magenta = [System.ConsoleColor]::Magenta
}

function Write-Header {
    param([string]$Title)
    Write-Host "`n" -NoNewline
    Write-Host "╔════════════════════════════════════════════════════════════════╗" -ForegroundColor $Colors.Magenta
    Write-Host "║ $Title" -ForegroundColor $Colors.Magenta
    Write-Host "╚════════════════════════════════════════════════════════════════╝" -ForegroundColor $Colors.Magenta
}

function Write-Info {
    param([string]$Message, [string]$Icon = "ℹ️")
    Write-Host "$Icon $Message" -ForegroundColor $Colors.Cyan
}

function Write-Success {
    param([string]$Message)
    Write-Host "✅ $Message" -ForegroundColor $Colors.Green
}

function Write-Warning {
    param([string]$Message)
    Write-Host "⚠️  $Message" -ForegroundColor $Colors.Yellow
}

function Write-Error {
    param([string]$Message)
    Write-Host "❌ $Message" -ForegroundColor $Colors.Red
}

# ============================================
# MAIN MENU
# ============================================

Write-Header "RawrXD Built-In Tools - Quick Start"

Write-Host @"
Choose an option:

  1) Launch RawrXD IDE (normal mode)
  2) Launch RawrXD IDE (debug mode)
  3) Run Built-In Tools Test Suite (requires RawrXD running)
  4) Launch RawrXD + Run Tests (one-shot)
  5) View Test Results & Report
  6) Exit

"@ -ForegroundColor $Colors.White

$choice = Read-Host "Enter choice (1-6)"

$PSDefaultParameterValues['Out-String:Width'] = 120

switch ($choice) {
    "1" {
        Write-Header "Launching RawrXD IDE"
        Write-Info "Starting in normal mode..."
        try {
            Set-Location "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
            & ".\RawrXD.ps1"
        } catch {
            Write-Error "Failed to launch RawrXD: $_"
        }
    }
    
    "2" {
        Write-Header "Launching RawrXD IDE - Debug Mode"
        Write-Info "Starting with debug logging enabled..."
        try {
            Set-Location "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
            & ".\RawrXD.ps1" -Verbose
        } catch {
            Write-Error "Failed to launch RawrXD: $_"
        }
    }
    
    "3" {
        Write-Header "Running Built-In Tools Test Suite"
        
        # Check if RawrXD is running
        $rawrXDProcess = Get-Process -Name "pwsh" -ErrorAction SilentlyContinue | 
                         Where-Object { $_.MainWindowTitle -like "*RawrXD*" }
        
        if (-not $rawrXDProcess) {
            Write-Warning "RawrXD IDE doesn't appear to be running"
            Write-Info "Please launch RawrXD first (option 1 or 2), then run this option again"
            Write-Info "Run this in a NEW PowerShell terminal"
            pause
        } else {
            Write-Success "RawrXD IDE detected running"
            Write-Info "Starting test suite..."
            
            try {
                Set-Location "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
                & ".\Test-BuiltInTools-Real.ps1"
            } catch {
                Write-Error "Test suite failed: $_"
            }
        }
    }
    
    "4" {
        Write-Header "One-Shot: Launch RawrXD + Run Tests"
        Write-Warning "This will open RawrXD and run tests"
        Write-Info "Two new windows will appear"
        
        # Launch RawrXD in background job
        $job = Start-Job -ScriptBlock {
            Set-Location "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
            & ".\RawrXD.ps1"
        }
        
        Write-Info "RawrXD launching (Job ID: $($job.Id))..."
        Start-Sleep -Seconds 5
        
        Write-Info "Tests starting in 5 seconds..."
        Start-Sleep -Seconds 5
        
        try {
            Set-Location "C:\Users\HiH8e\OneDrive\Desktop\Powershield"
            & ".\Test-BuiltInTools-Real.ps1"
        } catch {
            Write-Error "Test suite failed: $_"
        }
        
        Write-Info "Waiting for RawrXD job to complete..."
        Get-Job -Id $job.Id | Wait-Job | Out-Null
    }
    
    "5" {
        Write-Header "Test Results & Report"
        
        $testDir = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\ToolTests"
        $testGuide = "C:\Users\HiH8e\OneDrive\Desktop\Powershield\TEST-GUIDE.md"
        
        Write-Info "Test Directory: $testDir"
        Write-Info "Test Guide: $testGuide"
        
        if (Test-Path $testGuide) {
            Write-Success "Test guide exists"
            Write-Info "Opening guide..."
            & notepad $testGuide
        } else {
            Write-Warning "Test guide not found"
        }
        
        if (Test-Path $testDir) {
            Write-Success "Test directory exists"
            Write-Host ""
            Get-ChildItem -Path $testDir -Recurse | ForEach-Object {
                Write-Host "  - $($_.FullName)" -ForegroundColor $Colors.Cyan
            }
        } else {
            Write-Warning "No test results yet (run tests first)"
        }
    }
    
    "6" {
        Write-Success "Exiting"
        exit 0
    }
    
    default {
        Write-Error "Invalid choice. Please enter 1-6"
        pause
    }
}

Write-Host "`n"
