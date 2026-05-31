#Requires -Version 5.1
<#
.SYNOPSIS
    RawrXD IDE Uninstaller / De-provisioning Script

.DESCRIPTION
    Removes RawrXD IDE build artifacts, configuration, logs, and temporary files.
    Supports WhatIf dry-run mode and selective cleanup.

.PARAMETER WhatIf
    Dry-run: show what would be removed without deleting anything.

.PARAMETER ValidationProbe
    Minimal probe used by Validate-TurnkeyGapClosure.ps1 to verify the script
    loads and parses correctly. Exits 0 without performing any cleanup.

.PARAMETER Force
    Remove files without confirmation prompts.

.PARAMETER KeepLogs
    Preserve the logs directory during cleanup.

.PARAMETER KeepConfig
    Preserve settings/configuration files.
#>
param(
    [switch]$WhatIf,
    [switch]$ValidationProbe,
    [switch]$Force,
    [switch]$KeepLogs,
    [switch]$KeepConfig
)

$ErrorActionPreference = "Stop"
$repoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")

# ---------------------------------------------------------------------------
# Validation probe - fast exit used by the turnkey validator
# ---------------------------------------------------------------------------
if ($ValidationProbe) {
    Write-Host "[uninstall.ps1] ValidationProbe: script loaded successfully." -ForegroundColor Green
    exit 0
}

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
function Remove-Safely {
    param([string]$Path, [string]$Description)
    if (-not (Test-Path $Path)) { return }
    if ($WhatIf) {
        Write-Host "  [WhatIf] Would remove $Description : $Path" -ForegroundColor Yellow
        return
    }
    if (-not $Force) {
        $ans = Read-Host "  Remove $Description '$Path'? [y/N]"
        if ($ans -notmatch '^[yY]$') { Write-Host "  Skipped." -ForegroundColor Gray; return }
    }
    Remove-Item -Path $Path -Recurse -Force -ErrorAction SilentlyContinue
    Write-Host "  Removed: $Path" -ForegroundColor Green
}

# ---------------------------------------------------------------------------
# Display header
# ---------------------------------------------------------------------------
Write-Host "`n============================================" -ForegroundColor Cyan
Write-Host "  RawrXD IDE Uninstaller / De-provisioning" -ForegroundColor Cyan
if ($WhatIf) { Write-Host "  MODE: DRY-RUN (WhatIf)" -ForegroundColor Yellow }
Write-Host "============================================`n" -ForegroundColor Cyan

# ---------------------------------------------------------------------------
# Build artifacts
# ---------------------------------------------------------------------------
Write-Host "[1/4] Build artifacts" -ForegroundColor White
foreach ($dir in @("build", "build-win32", "build-ninja", "build-msvc", "_build")) {
    Remove-Safely (Join-Path $repoRoot $dir) "build directory"
}

# ---------------------------------------------------------------------------
# Temporary files
# ---------------------------------------------------------------------------
Write-Host "[2/4] Temporary files" -ForegroundColor White
foreach ($pattern in @("temp\*", "tmp\*", "*.tmp", "__pycache__")) {
    $items = Get-ChildItem -Path $repoRoot -Filter $pattern -Recurse -ErrorAction SilentlyContinue
    foreach ($item in $items) {
        Remove-Safely $item.FullName "temp file"
    }
}

# ---------------------------------------------------------------------------
# Log files (optional)
# ---------------------------------------------------------------------------
if (-not $KeepLogs) {
    Write-Host "[3/4] Log files" -ForegroundColor White
    $logItems = Get-ChildItem -Path (Join-Path $repoRoot "logs") -Filter "*.log" -ErrorAction SilentlyContinue
    foreach ($item in $logItems) {
        Remove-Safely $item.FullName "log file"
    }
} else {
    Write-Host "[3/4] Log files — skipped (KeepLogs)" -ForegroundColor Gray
}

# ---------------------------------------------------------------------------
# Configuration (optional)
# ---------------------------------------------------------------------------
if (-not $KeepConfig) {
    Write-Host "[4/4] Local configuration overrides" -ForegroundColor White
    foreach ($cfg in @("RawrXDSettings.local.json", "config\settings.local.json")) {
        Remove-Safely (Join-Path $repoRoot $cfg) "local config"
    }
} else {
    Write-Host "[4/4] Configuration — skipped (KeepConfig)" -ForegroundColor Gray
}

Write-Host "`n[Done] RawrXD de-provisioning complete." -ForegroundColor Green
exit 0
