#!/usr/bin/env pwsh
<#
.SYNOPSIS
Agentic autonomous Win32 build-fix runner for RawrXD.

.DESCRIPTION
Reads a build log file, extracts errors, and processes them one-by-one.
Supports automatic fixes for common build blockers (missing CMake cache, Qt deploy, etc.).

.EXAMPLE
pwsh .\scripts\agentic_build_fix.ps1 -LogPath "C:\Users\HiH8e\OneDrive\Desktop\PS Dlazy init ide cmake --build bui.txt" -RepoRoot "D:\lazy init ide" -AutoFix
#>

param(
    [Parameter(Mandatory = $true)][string]$LogPath,
    [string]$RepoRoot = (Get-Location).Path,
    [string]$BuildDir = "build",
    [string]$Generator = "Visual Studio 17 2022",
    [string]$Config = "Release",
    [string]$Target = "RawrXD-Win32IDE",
    [int]$MaxErrors = 50,
    [switch]$AutoFix,
    [switch]$DryRun
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Write-Section($text) {
    Write-Host ""; Write-Host $text -ForegroundColor Cyan; Write-Host "";
}

function Try-ImportModule($name, $path) {
    if (Test-Path $path) {
        try { Import-Module $path -Force -ErrorAction Stop; return $true } catch { return $false }
    }
    return $false
}

function Normalize-ErrorLine([string]$Line) {
    $clean = $Line -replace '\s+\[.*$',''
    return $clean.Trim()
}

function Get-BuildErrors([string]$Content) {
    $lines = $Content -split "`r?`n"
    $errors = @()
    $seen = @{}

    foreach ($line in $lines) {
        $candidate = $null
        if ($line -match "CMake Error:") {
            $candidate = $line
        } elseif ($line -match "error C\d+" -or $line -match "fatal error" -or $line -match "LNK\d+") {
            $candidate = $line
        } elseif ($line -match "missing CMakeCache.txt" -or $line -match "not a CMake build directory") {
            $candidate = $line
        } elseif ($line -match "0xc0000007b" -or $line -match "Qt6.*dll.*missing" -or $line -match "Qt6.*dll was not found") {
            $candidate = $line
        }

        if ($candidate) {
            $normalized = Normalize-ErrorLine $candidate
            $key = $normalized
            if ($normalized -match '^(?<file>.*)\((?<line>\d+)(,\d+)?\):\s+error\s+(?<code>[A-Z]+\d+):\s+(?<msg>.*)$') {
                $key = "$($matches.file)|$($matches.code)|$($matches.msg)"
            }

            if (-not $seen.ContainsKey($key)) {
                $seen[$key] = $true
                $errors += $normalized
            }
        }
    }

    return $errors | Select-Object -First $MaxErrors
}

function Invoke-FixStep([string]$Command, [string]$Reason) {
    Write-Host "[ACTION] $Reason" -ForegroundColor Yellow
    Write-Host "  -> $Command" -ForegroundColor DarkGray

    if ($DryRun -or -not $AutoFix) { return }

    Push-Location $RepoRoot
    try {
        Invoke-Expression $Command
    } finally {
        Pop-Location
    }
}

if (-not (Test-Path $LogPath)) {
    throw "Log file not found: $LogPath"
}

$agentModule = Join-Path $RepoRoot "RawrXD.AutonomousAgent.psm1"
$autonomyModule = Join-Path $RepoRoot "RawrXD.Agentic.Autonomy.psm1"

Try-ImportModule -name "RawrXD.AutonomousAgent" -path $agentModule | Out-Null
Try-ImportModule -name "RawrXD.Agentic.Autonomy" -path $autonomyModule | Out-Null

Write-Section "RawrXD Agentic Build Fix Runner"
Write-Host "Log: $LogPath"
Write-Host "Repo: $RepoRoot"
Write-Host "BuildDir: $BuildDir"
Write-Host "Config: $Config"
Write-Host "AutoFix: $AutoFix  DryRun: $DryRun"

$content = Get-Content -Path $LogPath -Raw
$errors = Get-BuildErrors -Content $content

if (-not $errors -or $errors.Count -eq 0) {
    Write-Host "No build errors detected in log." -ForegroundColor Green
    exit 0
}

Write-Section "Detected Errors"
$index = 0
foreach ($err in $errors) {
    $index++
    Write-Host "[$index] $err" -ForegroundColor Red
}

Write-Section "Processing Fixes"
foreach ($err in $errors) {
    if ($err -match "missing CMakeCache.txt" -or $err -match "not a CMake build directory") {
        Invoke-FixStep -Reason "Configure CMake build directory" -Command "cmake -S `"$RepoRoot`" -B `"$BuildDir`" -G `"$Generator`" -A x64"
        continue
    }

    if ($err -match "Qt6.*dll" -or $err -match "0xc0000007b") {
        Invoke-FixStep -Reason "Build + deploy Qt runtime" -Command "cmake --build `"$BuildDir`" --config `"$Config`" --target RawrXD-Deploy"
        continue
    }

    if ($err -match "AutonomousAgent.h" -and $err -match "chrono|steady_clock|map") {
        Invoke-FixStep -Reason "Add missing C++ headers to AutonomousAgent.h" -Command "# Fix: add <chrono>/<map>/<utility> includes in src\\win32app\\AutonomousAgent.h"
        continue
    }

    if ($err -match "Win32IDE.cpp" -and $err -match "syntax error") {
        Invoke-FixStep -Reason "Inspect Win32IDE.cpp syntax error near reported line" -Command "# Fix: open src\\win32app\\Win32IDE.cpp around the error line and remove stray tokens"
        continue
    }

    if ($err -match "LNK\d+") {
        Invoke-FixStep -Reason "Rebuild with verbose linker output" -Command "cmake --build `"$BuildDir`" --config `"$Config`" --target $Target -- /v:diag"
        continue
    }

    if ($err -match "error C\d+") {
        Invoke-FixStep -Reason "Rebuild to reproduce compiler error" -Command "cmake --build `"$BuildDir`" --config `"$Config`" --target $Target"
        continue
    }

    Write-Host "[INFO] No automatic fix rule for: $err" -ForegroundColor DarkYellow
}

Write-Section "Next Steps"
Write-Host "- Re-run this script with -AutoFix to apply commands automatically." -ForegroundColor Gray
Write-Host "- Or run with -DryRun to preview commands." -ForegroundColor Gray
