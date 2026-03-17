#!/usr/bin/env powershell
# RawrXD-AgenticIDE PowerShell Launcher
# Configures and launches the Qt-based IDE with prerequisite checks

param(
    [switch]$SkipChecks = $false,
    [string]$Model = "mistral",
    [string]$Endpoint = "http://localhost:11434",
    [int]$Timeout = 5000
)

$ErrorActionPreference = "Continue"

# Console setup
$Host.UI.RawUI.WindowTitle = "RawrXD Agentic IDE Launcher"
Clear-Host

Write-Host ""
Write-Host "===================================================" -ForegroundColor Cyan
Write-Host " RawrXD Agentic IDE - AI-Powered Code Editor" -ForegroundColor Cyan
Write-Host "===================================================" -ForegroundColor Cyan
Write-Host ""

# Get script directory
$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$ideExe = Join-Path $scriptDir "RawrXD-AgenticIDE.exe"

# Check if IDE exists
if (-not (Test-Path $ideExe)) {
    Write-Host "ERROR: RawrXD-AgenticIDE.exe not found at:" -ForegroundColor Red
    Write-Host "  $ideExe" -ForegroundColor Red
    Write-Host ""
    Write-Host "Please ensure the executable is in the same directory as this launcher." -ForegroundColor Yellow
    Read-Host "Press Enter to exit"
    exit 1
}

if (-not $SkipChecks) {
    Write-Host "[*] Checking prerequisites..." -ForegroundColor Yellow
    Write-Host ""

    # Check Ollama
    $ollamaRunning = Get-Process -Name "ollama" -ErrorAction SilentlyContinue
    if ($ollamaRunning) {
        Write-Host "[✓] Ollama service detected - AI completions enabled" -ForegroundColor Green
    } else {
        Write-Host "[!] WARNING: Ollama not running" -ForegroundColor Yellow
        Write-Host "     AI code completions will be unavailable." -ForegroundColor Yellow
        Write-Host "     To enable: Run 'ollama serve' in another terminal" -ForegroundColor Yellow
        Write-Host ""
    }

    # Check Clang
    if (Get-Command clangd -ErrorAction SilentlyContinue) {
        Write-Host "[✓] Clang found - LSP diagnostics enabled" -ForegroundColor Green
    } else {
        Write-Host "[!] WARNING: Clang not installed" -ForegroundColor Yellow
        Write-Host "     Real-time diagnostics will be unavailable." -ForegroundColor Yellow
        Write-Host ""
    }

    Write-Host ""
}

Write-Host "[*] Configuration:" -ForegroundColor Cyan
Write-Host "    Model: $Model" -ForegroundColor White
Write-Host "    Endpoint: $Endpoint" -ForegroundColor White
Write-Host "    Timeout: ${Timeout}ms" -ForegroundColor White
Write-Host ""

Write-Host "[*] Launching RawrXD-AgenticIDE..." -ForegroundColor Cyan
Write-Host ""

# Launch IDE with environment configuration
$env:RAWRXD_MODEL = $Model
$env:RAWRXD_ENDPOINT = $Endpoint
$env:RAWRXD_TIMEOUT = $Timeout

try {
    & $ideExe
} catch {
    Write-Host "ERROR: Failed to launch IDE" -ForegroundColor Red
    Write-Host $_.Exception.Message -ForegroundColor Red
    Read-Host "Press Enter to exit"
    exit 1
}

Write-Host "[✓] IDE launched successfully" -ForegroundColor Green
Write-Host ""
Write-Host "Troubleshooting:" -ForegroundColor Yellow
Write-Host "- If the IDE doesn't start, check that Qt runtime DLLs are available" -ForegroundColor Gray
Write-Host "- For AI completions, ensure Ollama is running (ollama serve)" -ForegroundColor Gray
Write-Host "- See DEPLOYMENT_GUIDE.md for detailed configuration" -ForegroundColor Gray
Write-Host ""
