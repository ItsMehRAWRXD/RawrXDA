# =============================================================================
# sync_command_registry.ps1 — COMMAND_TABLE Coverage Validation Wrapper
# =============================================================================
# Purpose: Runs audit_command_table.py to validate COMMAND_TABLE coverage
# Architecture: C++20, Win32, no Qt, no exceptions
# Usage:
#   .\scripts\sync_command_registry.ps1               # Full audit
#   .\scripts\sync_command_registry.ps1 -Validate     # Quick check (CI mode)
#   .\scripts\sync_command_registry.ps1 -Watch        # File watcher mode
#   .\scripts\sync_command_registry.ps1 -Generate     # Generate missing entries
# Rule: NO SOURCE FILE IS TO BE SIMPLIFIED.
# =============================================================================

param(
    [switch]$Validate,
    [switch]$Watch,
    [switch]$Generate,
    [switch]$Verbose,
    [string]$SrcRoot = ""
)

$ErrorActionPreference = "Stop"

# Single-root path resolver
. "$PSScriptRoot\\RawrXD_Root.ps1"

# Resolve project root
$ScriptDir = $PSScriptRoot
if (-not $ScriptDir -or $ScriptDir -eq "") {
    $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}
if (-not $ScriptDir -or $ScriptDir -eq "") { $ScriptDir = (Join-Path (Get-RawrXDRoot) "scripts") }
$ProjectRoot = Get-RawrXDRoot
if ($SrcRoot -eq "") { $SrcRoot = Join-Path $ProjectRoot "src" }

$AuditScript = Join-Path $ProjectRoot "scripts\audit_command_table.py"
$CommandTable = Join-Path $SrcRoot "core\command_registry.hpp"
$Win32IDE = Join-Path $SrcRoot "win32app\Win32IDE.h"

# =============================================================================
# VALIDATION MODE — Quick CI-friendly check
# =============================================================================
if ($Validate) {
    Write-Host "`n[Validate] Checking COMMAND_TABLE coverage..." -ForegroundColor Cyan
    $result = & python $AuditScript --src-root $SrcRoot --quiet 2>&1
    if ($LASTEXITCODE -eq 0) {
        Write-Host "[Validate] CLEAN — All IDM_* defines covered" -ForegroundColor Green
        exit 0
    } else {
        Write-Host "[Validate] GAPS FOUND — Run without -Validate for details" -ForegroundColor Yellow
        exit 1
    }
}

# =============================================================================
# GENERATE MODE — Produce X-macro lines for missing entries
# =============================================================================
if ($Generate) {
    Write-Host "`n[Generate] Producing COMMAND_TABLE entries for missing IDM_* defines..." -ForegroundColor Cyan
    & python $AuditScript --src-root $SrcRoot --generate
    exit $LASTEXITCODE
}

# =============================================================================
# WATCH MODE — Monitor source files and re-audit on changes
# =============================================================================
if ($Watch) {
    Write-Host "`n[Watch] Monitoring COMMAND_TABLE sources for changes..." -ForegroundColor Cyan
    Write-Host "  Watching: command_registry.hpp, Win32IDE.h, ide_constants.h" -ForegroundColor DarkGray
    Write-Host "  Press Ctrl+C to stop`n" -ForegroundColor DarkGray

    $watcher = New-Object System.IO.FileSystemWatcher
    $watcher.Path = $SrcRoot
    $watcher.IncludeSubdirectories = $true
    $watcher.Filter = "*.h"
    $watcher.EnableRaisingEvents = $false

    $lastRun = [DateTime]::MinValue

    while ($true) {
        $changed = $watcher.WaitForChanged([System.IO.WatcherChangeTypes]::Changed, 2000)
        if (-not $changed.TimedOut) {
            $now = Get-Date
            if (($now - $lastRun).TotalSeconds -gt 3) {
                Write-Host "[Watch] Change detected: $($changed.Name) — re-auditing..." -ForegroundColor Yellow
                & python $AuditScript --src-root $SrcRoot
                $lastRun = $now
            }
        }
    }
}

# =============================================================================
# DEFAULT — Full audit with report generation
# =============================================================================
Write-Host ""
Write-Host "=== COMMAND_TABLE Coverage Sync ===" -ForegroundColor Cyan

# Run the auditor
& python $AuditScript --src-root $SrcRoot

exit $LASTEXITCODE
