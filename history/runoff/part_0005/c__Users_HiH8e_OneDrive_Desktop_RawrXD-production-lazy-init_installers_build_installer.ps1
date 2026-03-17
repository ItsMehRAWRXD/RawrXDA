#!/usr/bin/env pwsh
# Build NSIS installer for RawrXD Win32 IDE
# Usage:
#   pwsh installers/build_installer.ps1 [-SourceRoot <path>] [-Output <path>]

param(
    [string]$SourceRoot = (Resolve-Path "$PSScriptRoot/.."),
    [string]$Output = (Join-Path $PSScriptRoot "RawrXD-Win32-Setup-1.0.0.exe")
)

$ErrorActionPreference = "Stop"

Write-Host "SourceRoot: $SourceRoot" -ForegroundColor Cyan
Write-Host "Output:    $Output" -ForegroundColor Cyan

if (-not (Get-Command makensis -ErrorAction SilentlyContinue)) {
    Write-Host "[ERROR] makensis not found in PATH. Install NSIS and ensure makensis is available." -ForegroundColor Red
    exit 1
}

$scriptPath = Join-Path $PSScriptRoot "installer.nsi"
if (-not (Test-Path $scriptPath)) {
    Write-Host "[ERROR] installer.nsi not found at $scriptPath" -ForegroundColor Red
    exit 1
}

$env:SOURCE_ROOT = $SourceRoot

# NSIS uses OutFile inside the script; override by copying if needed
$cmd = "makensis /DSOURCE_ROOT=`"$SourceRoot`" /DOutFile=`"$Output`" `"$scriptPath`""
Write-Host "Running: $cmd" -ForegroundColor Yellow

$process = Start-Process -FilePath "makensis" -ArgumentList @("/DSOURCE_ROOT=$SourceRoot", "/XOutFile \"$Output\"", "$scriptPath") -Wait -PassThru

if ($process.ExitCode -ne 0) {
    Write-Host "[ERROR] makensis exited with code $($process.ExitCode)" -ForegroundColor Red
    exit $process.ExitCode
}

if (-not (Test-Path $Output)) {
    Write-Host "[ERROR] Output file not found: $Output" -ForegroundColor Red
    exit 1
}

Write-Host "[OK] Installer built: $Output" -ForegroundColor Green
exit 0
